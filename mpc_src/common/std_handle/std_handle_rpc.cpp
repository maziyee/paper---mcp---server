#include "std_handle_rpc.h"

#include <cstdio>
#include <ctime>

#include "log_manager.h"
#include "nlohmann/json.hpp"

namespace mcp {

StdRpcServer::StdRpcServer(RpcManager& rpc_manager, std::ostream& out,
                           std::istream& in)
    : rpc_manager_(rpc_manager), out_(out), in_(in) {}

bool StdRpcServer::ReadMessage(std::string& out_body) {
  std::string line;
  int content_length = -1;
  const std::string kHeader = "content-length:";

  // 读第一行
  if (!std::getline(in_, line)) {
    MCP_LOG_INFO("ReadMessage: EOF on first read");
    return false;
  }
  if (!line.empty() && line.back() == '\r') {
    line.pop_back();
  }

  // 判断格式：以 { 开头 = 纯 JSON 行（无 Content-Length header）
  if (!line.empty() && line[0] == '{') {
    MCP_LOG_INFO("ReadMessage: raw JSON mode, len={}", line.size());
    out_body = std::move(line);
    return true;
  }

  MCP_LOG_INFO("ReadMessage: header mode, first line='{}'",
               line.substr(0, 80));

  // Content-Length 帧格式：读 header 行
  while (!line.empty()) {
    std::string lower = line;
    for (char& c : lower) c = static_cast<char>(std::tolower(c));
    if (lower.compare(0, kHeader.size(), kHeader) == 0) {
      auto colon = lower.find(':');
      if (colon != std::string::npos) {
        size_t start = colon + 1;
        while (start < line.size() && line[start] == ' ') ++start;
        content_length = std::stoi(line.substr(start));
      }
    }
    if (!std::getline(in_, line)) break;
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
  }

  if (content_length <= 0) return false;

  std::string body(content_length, '\0');
  size_t total = 0;
  while (total < static_cast<size_t>(content_length)) {
    in_.read(body.data() + total, content_length - total);
    std::streamsize n = in_.gcount();
    if (n <= 0) return false;
    total += static_cast<size_t>(n);
  }
  if (total != static_cast<size_t>(content_length)) {
    MCP_LOG_ERROR("ReadMessage: total != content_length");
    return false;
  }

  out_body = std::move(body);
  return true;
}

static void WriteFrame(std::ostream& out, const std::string& body) {
  out << "Content-Length: " << body.size() << "\r\n\r\n";
  out.write(body.data(), body.size());
  out.flush();
}

bool StdRpcServer::WriteMessage(const RpcResponse& resp, bool use_jsonrpc) {
  if (use_jsonrpc) {
    WriteFrame(out_, resp.ToJsonRpc().dump());
  } else {
    WriteFrame(out_, resp.ToJson().dump());
  }
  return out_.good();
}

bool StdRpcServer::WriteMessage(const RpcError& err, bool use_jsonrpc) {
  if (use_jsonrpc) {
    WriteFrame(out_, err.ToJsonRpc().dump());
  } else {
    WriteFrame(out_, err.ToJson().dump());
  }
  return out_.good();
}

void StdRpcServer::Run() {
  std::string body;
  while (ReadMessage(body)) {
    try {
      auto request = nlohmann::json::parse(body);

      // 检测请求格式：JSON-RPC 2.0 vs 自定义
      bool is_jsonrpc = request.contains("jsonrpc");

      // 检测传输格式：请求体不包含 \r\n = 原始 JSON 行模式
      bool is_raw_mode = (body.find("\r\n") == std::string::npos &&
                          body.find('\n') == std::string::npos);

      // JSON-RPC 2.0 通知：没有 "id" 字段，不返回响应
      bool is_notification = is_jsonrpc && !request.contains("id");

      RpcRequest req = RpcRequest::FromJson(request);

      if (!req.IsValid()) {
        if (is_notification) continue;
        RpcError err;
        err.SetSequenceId(req.GetSequenceId());
        err.SetErrorCode(errc::InvalidRequest);
        err.SetErrorMessage("missing service_name or method_name");
        WriteMessage(err, is_jsonrpc);
        continue;
      }

      // 通知不返回响应
      if (is_notification) {
        rpc_manager_.HandleRequest(req);
        continue;
      }

      nlohmann::json json_resp = rpc_manager_.HandleRequest(req);
      std::string resp_body = json_resp.dump();

      if (is_raw_mode) {
        // 原始 JSON 行模式：直接输出 JSON + 换行
        out_ << resp_body << "\n";
        out_.flush();
      } else {
        // Content-Length 帧模式
        out_ << "Content-Length: " << resp_body.size() << "\r\n\r\n";
        out_.write(resp_body.data(), resp_body.size());
        out_.flush();
      }
    } catch (const std::exception& e) {
      RpcError err;
      err.SetErrorCode(errc::ParseError);
      err.SetErrorMessage(std::string("json parse error: ") + e.what());
      WriteMessage(err, false);
    }
  }
}

}  // namespace mcp
