#include "http_rpc.h"

#include <fstream>
#include <sstream>

#include "httplib.h"
#include "log_manager.h"
#include "my_rpc.h"
#include "nlohmann/json.hpp"
#include "rpc_manager.h"

namespace mcp {

// ======== Pimpl 实现类 ========
//
// 头文件中 HttpRpcServer 只有 unique_ptr<Impl>，不暴露 httplib::Server。
// 换 HTTP 库时只需改这个类和 CMakeLists，所有 include 了 http_rpc.h 的
// 文件不需要重新编译。

class HttpRpcServer::Impl {
 public:
  explicit Impl(RpcManager& rpc_manager) : rpc_manager_(rpc_manager) {
    // httplib 内部错误回调（如连接被重置等）
    server_.set_logger(
        [](const httplib::Request& req, const httplib::Response& res) {
          MCP_LOG_INFO("HTTP {} {} -> {}", req.method, req.path, res.status);
        });

    // 自定义错误响应：HTTP 错误也返回 JSON 格式
    server_.set_error_handler(
        [](const httplib::Request& req, httplib::Response& res) {
          RpcError err;
          err.SetSequenceId(0);
          err.SetErrorCode(errc::InternalError);
          switch (res.status) {
            case 404:
              err.SetErrorMessage("endpoint not found: " + req.path);
              break;
            case 405:
              err.SetErrorMessage("method not allowed: " + req.method);
              break;
            default:
              err.SetErrorMessage("http error: " + std::to_string(res.status));
              break;
          }
          res.set_content(err.ToJson().dump(), "application/json");
        });
  }

  httplib::Server server_;
  RpcManager& rpc_manager_;
};

// ======== 配置文件构造 ========

HttpRpcServer::HttpRpcServer(const std::string& config_path,
                             RpcManager& rpc_manager)
    : host_("0.0.0.0"),
      port_(8080),
      rpc_manager_(rpc_manager),
      is_running_(false),
      pimpl_(std::make_unique<Impl>(rpc_manager)) {
  std::ifstream ifs(config_path);
  if (!ifs.is_open()) {
    MCP_LOG_ERROR("HttpRpcServer: cannot open config file: {}", config_path);
    return;
  }

  try {
    auto j = nlohmann::json::parse(ifs);
    if (j.contains("host") && j["host"].is_string()) {
      host_ = j["host"].get<std::string>();
    }
    if (j.contains("port") && j["port"].is_number()) {
      port_ = j["port"].get<int>();
    }
  } catch (const std::exception& e) {
    MCP_LOG_ERROR("HttpRpcServer: config parse error: {}", e.what());
  }
}

// ======== 手动参数构造 ========

HttpRpcServer::HttpRpcServer(const std::string& host, int port,
                             RpcManager& rpc_manager)
    : host_(host),
      port_(port),
      rpc_manager_(rpc_manager),
      is_running_(false),
      pimpl_(std::make_unique<Impl>(rpc_manager)) {}

// ======== 虚析构函数 ========

HttpRpcServer::~HttpRpcServer() {
  if (is_running_) {
    Stop();
  }
}

// ======== 移动语义 ========

HttpRpcServer::HttpRpcServer(HttpRpcServer&& other) noexcept
    : host_(std::move(other.host_)),
      port_(other.port_),
      rpc_manager_(other.rpc_manager_),
      is_running_(other.is_running_.load()),
      pimpl_(std::move(other.pimpl_)) {
  other.is_running_ = false;
}

HttpRpcServer& HttpRpcServer::operator=(HttpRpcServer&& other) noexcept {
  if (this != &other) {
    if (is_running_) {
      Stop();
    }
    host_ = std::move(other.host_);
    port_ = other.port_;
    // rpc_manager_ is a reference, can't be reassigned — but source and target
    // must point to the same manager (they always do in practice)
    is_running_ = other.is_running_.load();
    pimpl_ = std::move(other.pimpl_);
    other.is_running_ = false;
  }
  return *this;
}

// ======== 启动 ========

bool HttpRpcServer::Start() {
  if (is_running_) {
    MCP_LOG_ERROR("HttpRpcServer: already running");
    return false;
  }

  is_running_ = true;

  // 处理单个 RPC 请求，返回 JSON 响应对象
  auto handle_one =
      [this](const nlohmann::json& request_json) -> nlohmann::json {
    RpcRequest rpc_req = RpcRequest::FromJson(request_json);

    if (!rpc_req.IsValid()) {
      RpcError err;
      err.SetSequenceId(rpc_req.GetSequenceId());
      err.SetErrorCode(errc::InvalidRequest);
      err.SetErrorMessage("missing service_name or method_name");
      return err.ToJson();
    }

    std::string raw_resp = rpc_manager_.HandleRequest(rpc_req);

    RpcResponse resp;
    if (resp.Deserializer(raw_resp)) {
      return resp.ToJson();
    }

    RpcError err;
    if (err.Deserializer(raw_resp)) {
      return err.ToJson();
    }

    RpcError fallback;
    fallback.SetSequenceId(rpc_req.GetSequenceId());
    fallback.SetErrorCode(errc::InternalError);
    fallback.SetErrorMessage("failed to parse binary response");
    return fallback.ToJson();
  };

  // 生成会话 ID
  auto generate_session_id = []() -> std::string {
    std::ostringstream oss;
    oss << std::hex
        << std::chrono::steady_clock::now().time_since_epoch().count();
    return oss.str();
  };

  // 注册健康检查端点：GET /health
  pimpl_->server_.Get("/health", [this](const httplib::Request& req,
                                        httplib::Response& res) {
    nlohmann::json status;
    status["status"] = "ok";
    status["host"] = host_;
    status["port"] = port_;
    status["uptime_seconds"] =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time_)
            .count();
    res.set_content(status.dump(), "application/json");
  });

  // ======== Streamable HTTP：POST /rpc ========
  //
  // 同一个端点，根据 Accept 头选择响应模式：
  //   Accept: application/json   → 普通 JSON 响应（短连接）
  //   Accept: text/event-stream  → SSE 流式响应（长连接，每个结果一个事件）
  //   未指定 Accept              → 默认为 application/json
  //
  pimpl_->server_.Post(
      "/rpc",
      [this, handle_one, generate_session_id](const httplib::Request& req,
                                              httplib::Response& res) {
        // Content-Type 校验
        if (!req.has_header("Content-Type") ||
            req.get_header_value("Content-Type").find("application/json") ==
                std::string::npos) {
          RpcError err;
          err.SetErrorCode(errc::ParseError);
          err.SetErrorMessage("Content-Type must be application/json");
          res.status = 400;
          res.set_content(err.ToJson().dump(), "application/json");
          return;
        }

        // 判断客户端期望的响应模式
        bool wants_stream =
            req.has_header("Accept") &&
            req.get_header_value("Accept").find("text/event-stream") !=
                std::string::npos;

        try {
          auto json = nlohmann::json::parse(req.body);

          // 统一为数组处理
          bool is_batch = json.is_array();
          nlohmann::json requests =
              is_batch ? json : nlohmann::json::array({json});

          // ======== 流式模式（SSE）========
          if (wants_stream) {
            std::string session_id = generate_session_id();
            res.set_header("Content-Type", "text/event-stream");
            res.set_header("Cache-Control", "no-cache");
            res.set_header("Connection", "keep-alive");
            res.set_header("Mcp-Session-Id", session_id);

            res.set_chunked_content_provider(
                "text/event-stream",
                [requests, handle_one](size_t offset,
                                       httplib::DataSink& sink) -> bool {
                  for (const auto& item : requests) {
                    if (!sink.is_writable()) return false;
                    nlohmann::json result;
                    try {
                      result = handle_one(item);
                    } catch (const std::exception& e) {
                      RpcError err;
                      err.SetErrorCode(errc::ParseError);
                      err.SetErrorMessage(std::string("error: ") + e.what());
                      result = err.ToJson();
                    }
                    std::string frame = "data: " + result.dump() + "\n\n";
                    if (!sink.write(frame.data(), frame.size())) {
                      return false;
                    }
                  }
                  sink.done();
                  return true;
                });
            return;
          }

          // ======== 普通模式（JSON 一次性返回）========
          if (is_batch) {
            nlohmann::json batch_result = nlohmann::json::array();
            for (const auto& item : requests) {
              try {
                batch_result.push_back(handle_one(item));
              } catch (const std::exception& e) {
                RpcError err;
                err.SetErrorCode(errc::ParseError);
                err.SetErrorMessage(std::string("batch item error: ") +
                                    e.what());
                batch_result.push_back(err.ToJson());
              }
            }
            res.set_content(batch_result.dump(), "application/json");
            return;
          }

          // 单请求
          auto result = handle_one(requests[0]);
          if (result.contains("error")) {
            res.status =
                result["error"]["code"].get<int>() == errc::InternalError
                    ? 500
                    : 400;
          }
          res.set_content(result.dump(), "application/json");
        } catch (const std::exception& e) {
          RpcError err;
          err.SetErrorCode(errc::ParseError);
          err.SetErrorMessage(std::string("json parse error: ") + e.what());
          res.status = 400;
          res.set_content(err.ToJson().dump(), "application/json");
        }
      });

  MCP_LOG_INFO("HttpRpcServer starting on {}:{}", host_, port_);

  start_time_ = std::chrono::steady_clock::now();

  // listen 会阻塞当前线程，直到 server_.stop() 被调用
  bool ok = pimpl_->server_.listen(host_.c_str(), port_);

  is_running_ = false;
  return ok;
}

// ======== 停止 ========

void HttpRpcServer::Stop() {
  if (!is_running_) return;
  pimpl_->server_.stop();
  is_running_ = false;
}

// ======== SSE 端点注册 ========

bool HttpRpcServer::RegisterSseEndpoint(const std::string& path,
                                        SseHandler handler) {
  if (path.empty() || !handler) {
    MCP_LOG_ERROR("HttpRpcServer: invalid SSE path or handler");
    return false;
  }

  // 用 lambda 包一层：httplib 的 chunked_content_provider → 我们的 SseHandler
  pimpl_->server_.Get(path, [this, handler = std::move(handler)](
                                const httplib::Request& req,
                                httplib::Response& res) {
    res.set_header("Content-Type", "text/event-stream");
    res.set_header("Cache-Control", "no-cache");
    res.set_header("Connection", "keep-alive");
    res.set_header("Access-Control-Allow-Origin", "*");

    res.set_chunked_content_provider(
        "text/event-stream",
        [this, handler, path = req.path](size_t offset,
                                         httplib::DataSink& sink) -> bool {
          // SseSender：将数据封装为 SSE 帧写入 sink
          // 注意：sender 只能在 chunked_content_provider 回调内使用
          //       因为 sink 的引用在回调返回后失效
          auto sender = [&sink](const std::string& data) -> bool {
            std::string frame;
            // 支持多行数据：逐行加 "data:" 前缀
            size_t start = 0;
            while (start < data.size()) {
              size_t end = data.find('\n', start);
              if (end == std::string::npos) end = data.size();
              frame += "data: ";
              frame.append(data, start, end - start);
              frame += "\n";
              start = end + 1;
            }
            frame += "\n";  // 空行表示事件结束
            return sink.write(frame.data(), frame.size());
          };

          try {
            handler(sender);
          } catch (const std::exception& e) {
            MCP_LOG_ERROR("SSE handler exception on {}: {}", path, e.what());
            // 尝试推送错误事件给客户端
            try {
              std::string err_frame =
                  "event: error\ndata: " + std::string(e.what()) + "\n\n";
              sink.write(err_frame.data(), err_frame.size());
            } catch (...) {
              // 客户端可能已断开，忽略写入失败
            }
          } catch (...) {
            MCP_LOG_ERROR("SSE handler unknown exception on {}", path);
          }
          sink.done();
          return true;
        });
  });

  return true;
}

}  // namespace mcp
