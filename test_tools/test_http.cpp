#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include "http_rpc.h"
#include "nlohmann/json.hpp"
#include "rpc_manager.h"

// 信号处理用的全局句柄（C 信号无 userdata，只能用全局变量）
static mcp::HttpRpcServer* g_server = nullptr;

static void SignalHandler(int sig) {
  std::cerr << "\nSignal " << sig << " received, shutting down..." << std::endl;
  if (g_server) g_server->Stop();
}

int main() {
  mcp::RpcManager manager;

  // ======== 注册测试方法 ========

  manager.RegisterMethod("Echo", "Echo",
                         [](const std::string& payload) { return payload; });

  manager.RegisterMethod("Math", "Add", [](const std::string& payload) {
    auto j = nlohmann::json::parse(payload);
    return std::to_string(j["a"].get<int>() + j["b"].get<int>());
  });

  manager.RegisterMethod("Math", "Mul", [](const std::string& payload) {
    auto j = nlohmann::json::parse(payload);
    return std::to_string(j["a"].get<int>() * j["b"].get<int>());
  });

  // ======== 创建并启动服务器 ========

  mcp::HttpRpcServer server("0.0.0.0", 8080, manager);
  g_server = &server;

  // 注册 SSE 端点：每秒推送一次递增计数
  server.RegisterSseEndpoint(
      "/events", [](auto sender) {
        int count = 0;
        while (sender(std::to_string(++count))) {
          std::this_thread::sleep_for(std::chrono::seconds(1));
        }
      });

  std::signal(SIGINT, SignalHandler);
  std::signal(SIGTERM, SignalHandler);

  std::cerr << "========================================" << std::endl;
  std::cerr << " HTTP RPC Server   http://0.0.0.0:8080" << std::endl;
  std::cerr << "----------------------------------------" << std::endl;
  std::cerr << " Endpoints:" << std::endl;
  std::cerr << "   GET  /health    Health check" << std::endl;
  std::cerr << "   GET  /events    SSE event stream" << std::endl;
  std::cerr << "   POST /rpc       RPC call (JSON)" << std::endl;
  std::cerr << "----------------------------------------" << std::endl;
  std::cerr << " Registered methods:" << std::endl;
  std::cerr << "   Echo/Echo       echo payload back" << std::endl;
  std::cerr << "   Math/Add        a + b" << std::endl;
  std::cerr << "   Math/Mul        a * b" << std::endl;
  std::cerr << "----------------------------------------" << std::endl;
  std::cerr << " Ctrl+C to stop" << std::endl;
  std::cerr << "========================================" << std::endl;

  return server.Start() ? 0 : 1;
}
