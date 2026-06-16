#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>

#include "http_rpc.h"
#include "log_manager.h"
#include "mcp_server.h"
#include "mcp_spdlog_config.h"
#include "nlohmann/json.hpp"
#include "paper_tools.h"
#include "rpc_manager.h"
#include "std_handle_rpc.h"

namespace {

struct ServerConfig {
  std::string server_name = "paper-mcp";
  std::string server_version = "1.0.0";
  std::string http_host = "0.0.0.0";
  int http_port = 8080;
  bool http_enabled = true;
  bool stdio_enabled = true;
  std::string log_config_path = "config/log_config.json";
};

// 从 JSON 文件加载配置
ServerConfig LoadConfig(const std::string& path) {
  ServerConfig cfg;
  std::ifstream f(path);
  if (!f.is_open()) return cfg;

  auto j = nlohmann::json::parse(f, nullptr, false);
  if (j.is_discarded()) return cfg;

  if (j.contains("server")) {
    cfg.server_name = j["server"].value("name", cfg.server_name);
    cfg.server_version = j["server"].value("version", cfg.server_version);
  }
  if (j.contains("http")) {
    cfg.http_enabled = j["http"].value("enabled", cfg.http_enabled);
    cfg.http_host = j["http"].value("host", cfg.http_host);
    cfg.http_port = j["http"].value("port", cfg.http_port);
  }
  if (j.contains("stdio")) {
    cfg.stdio_enabled = j["stdio"].value("enabled", cfg.stdio_enabled);
  }
  return cfg;
}

// 命令行覆盖配置
void ApplyArgs(ServerConfig& cfg, int argc, char* argv[]) {
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--host" && i + 1 < argc) {
      cfg.http_host = argv[++i];
    } else if (arg == "--port" && i + 1 < argc) {
      cfg.http_port = std::stoi(argv[++i]);
    } else if (arg == "--no-http") {
      cfg.http_enabled = false;
    } else if (arg == "--no-stdio") {
      cfg.stdio_enabled = false;
    } else if (arg == "--config" && i + 1 < argc) {
      cfg = LoadConfig(argv[++i]);
    } else if (arg == "--log-config" && i + 1 < argc) {
      cfg.log_config_path = argv[++i];
    } else if (arg == "-h" || arg == "--help") {
      std::cout
          << "Usage: " << argv[0]
          << " [--config config/server_config.json]"
          << " [--host 0.0.0.0] [--port 8080] [--no-http] [--no-stdio]\n";
      exit(0);
    }
  }
}

// 信号处理
std::atomic<bool> g_running{true};
mcp::HttpRpcServer* g_http_server = nullptr;

void SignalHandler(int sig) {
  std::cerr << "\nSignal " << sig << " received, shutting down...\n";
  g_running = false;
  if (g_http_server) g_http_server->Stop();
}

}  // namespace

int main(int argc, char* argv[]) {
  // ======== 计算服务器基础目录（相对于可执行文件位置）========

  std::filesystem::path exe_path =
      std::filesystem::absolute(argv[0]).parent_path();
  // 开发环境：build/ 目录，配置在上层
  // 安装环境：bin/ 目录，配置在 /etc 或上层
  std::filesystem::path base_dir = exe_path.parent_path();  // mcp_mt/

  // ======== 加载配置 ========

  std::string config_path = (base_dir / "config/server_config.json").string();
  ServerConfig cfg = LoadConfig(config_path);
  ApplyArgs(cfg, argc, argv);

  if (!cfg.http_enabled && !cfg.stdio_enabled) {
    std::cerr << "Error: both transports disabled\n";
    return 1;
  }

  // ======== 初始化日志 ========

  std::string log_config_path = (base_dir / cfg.log_config_path).string();
  mcp::SpdlogConfig log_config;

  // 先加载日志配置文件
  if (!log_config.InitSpdlog(log_config_path)) {
    // 配置文件不存在，使用默认配置
    log_config.SetLevel("info");
    log_config.Setstdout(true);
    log_config.SetFileout(false);
  }

  // stdio 模式下 stdout 被 MCP 占用，日志绝对不能写 stdout
  if (cfg.stdio_enabled) {
    log_config.Setstdout(false);
    if (!log_config.GetIsFileout()) {
      log_config.SetFileout(true);
    }
  }

  // 日志文件路径使用绝对路径
  std::filesystem::path log_dir = base_dir / "logs";
  log_config.SetLogPath(log_dir.string());

  // 确保日志目录存在
  std::error_code ec;
  if (!std::filesystem::exists(log_dir)) {
    std::filesystem::create_directories(log_dir, ec);
    if (ec) {
      std::cerr << "Warning: cannot create log dir " << log_dir
                << ": " << ec.message() << "\n";
      // 回退到 /tmp
      log_dir = "/tmp/mcp_logs";
      std::filesystem::create_directories(log_dir, ec);
      log_config.SetLogPath(log_dir.string());
    }
  }

  mcp::Logger::GetInstance().Init(&log_config);

  // 双重保险：stdio 模式下强制确保不向 stdout 输出日志
  if (cfg.stdio_enabled) {
    auto logger = spdlog::get("rpc");
    if (logger) {
      auto& sinks = logger->sinks();
      sinks.erase(
          std::remove_if(sinks.begin(), sinks.end(),
                         [](const spdlog::sink_ptr& s) {
                           return std::dynamic_pointer_cast<
                                      spdlog::sinks::stdout_color_sink_mt>(s) !=
                                  nullptr;
                         }),
          sinks.end());
      // 如果所有 sink 都被移除了，关闭日志输出
      if (sinks.empty()) {
        logger->set_level(spdlog::level::off);
      }
    }
  }

  MCP_LOG_INFO("Server {} v{} starting (http={}, stdio={})",
               cfg.server_name, cfg.server_version,
               cfg.http_enabled, cfg.stdio_enabled);

  // ======== 组装服务 ========

  mcp::RpcManager manager;
  mcp::McpServer mcp_server(cfg.server_name, cfg.server_version);

  mcp_server.SetChangeCallback(
      [](mcp::ChangeType type, const std::string& name,
         const nlohmann::json&) {
        const char* t = "";
        switch (type) {
          case mcp::ChangeType::ToolAdded: t = "Tool"; break;
          case mcp::ChangeType::ResourceAdded: t = "Resource"; break;
          case mcp::ChangeType::PromptAdded: t = "Prompt"; break;
        }
        MCP_LOG_INFO("[{}] {} registered", t, name);
      });

  RegisterPaperTools(mcp_server);

  if (!mcp_server.InstallTo(manager)) {
    MCP_LOG_ERROR("InstallTo failed: [{}] {}",
                  mcp_server.GetLastErrorCode(), mcp_server.GetLastError());
    return 1;
  }

  // ======== 信号处理 ========

  std::signal(SIGINT, SignalHandler);
  std::signal(SIGTERM, SignalHandler);

  // ======== 提示信息（stderr，不污染 stdout MCP 通道）========

  std::cerr << "========================================\n";
  std::cerr << " " << cfg.server_name << " v" << cfg.server_version << "\n";
  std::cerr << "----------------------------------------\n";
  if (cfg.stdio_enabled) std::cerr << " stdio:  stdin/stdout (Content-Length)\n";
  if (cfg.http_enabled)
    std::cerr << " http:   http://" << cfg.http_host << ":" << cfg.http_port << "\n";
  std::cerr << "----------------------------------------\n";
  std::cerr << " Ctrl+C to stop\n";
  std::cerr << "========================================\n";
  std::cerr.flush();

  // ======== 启动 HTTP（后台线程）========

  std::thread http_thread;
  if (cfg.http_enabled) {
    http_thread = std::thread([&manager, &cfg]() {
      mcp::HttpRpcServer http_server(cfg.http_host, cfg.http_port, manager);
      g_http_server = &http_server;
      http_server.Start();
      MCP_LOG_INFO("HTTP transport stopped");
    });
  }

  // ======== 主线程运行 stdio ========

  if (cfg.stdio_enabled) {
    MCP_LOG_INFO("Stdio transport started (stdio)");
    mcp::StdRpcServer stdio_server(manager, std::cout, std::cin);
    stdio_server.Run();
    MCP_LOG_INFO("Stdio transport stopped");
  }

  // stdio 退出 → 通知 HTTP 停止
  if (cfg.http_enabled) {
    if (cfg.stdio_enabled && g_http_server) {
      g_http_server->Stop();  // 主线程 stdio 退出了，关 HTTP
    }
    if (http_thread.joinable()) http_thread.join();
  }

  MCP_LOG_INFO("Server exited");
  return 0;
}
