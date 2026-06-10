#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>

namespace mcp {

class RpcManager;

class HttpRpcServer {
 public:
  // ======== SSE 回调类型 ========
  // SSE 数据发送器：向客户端推送一条事件，返回 false 表示连接已断开
  using SseSender = std::function<bool(const std::string& event_data)>;

  // SSE 端点处理器：客户端连接时调用一次，通过 SseSender 持续推送
  // 当 handler 返回时表示该连接结束
  using SseHandler = std::function<void(SseSender sender)>;

  // ======== 构造函数 ========

  // 从配置文件构造（读取 host、port 等配置项）
  explicit HttpRpcServer(const std::string& config_path,
                         RpcManager& rpc_manager);

  // 手动指定参数构造
  HttpRpcServer(const std::string& host, int port, RpcManager& rpc_manager);

  // ======== 虚析构函数 ========
  //
  // 为什么用 virtual：
  // 1. Pimpl 需要：Impl 在 .cpp 中定义，析构时必须有完整类型才能 delete
  //    unique_ptr<Impl> 的析构在 ~HttpRpcServer() 中展开，若定义在 .cpp
  //    则没问题； 但如果有子类继承 HttpRpcServer，通过基类指针 delete
  //    派生对象时， 基类析构函数不是 virtual
  //    会导致未定义行为（只析构基类部分，内存泄漏）
  // 2. 设计意图：HttpRpcServer 作为一个服务基类/接口，天然需要继承扩展
  //    （如支持 HTTPS、自定义中间件等），虚析构是 C++ 多态的"最后一道保险"
  virtual ~HttpRpcServer();

  // 禁止拷贝，允许移动
  HttpRpcServer(const HttpRpcServer&) = delete;
  HttpRpcServer& operator=(const HttpRpcServer&) = delete;
  HttpRpcServer(HttpRpcServer&&) noexcept;
  HttpRpcServer& operator=(HttpRpcServer&&) noexcept;

  // ======== 生命周期 ========

  // 启动 HTTP 服务器（阻塞当前线程，直到 Stop() 被调用或发生错误）
  // 返回 true 表示正常停止，false 表示异常退出
  bool Start();

  // 停止服务器（可在其他线程调用，让 Start() 返回）
  void Stop();

  // ======== SSE 端点注册 ========

  // 注册一个 SSE（Server-Sent Events）端点
  // path:    URL 路径，如 "/events"
  // handler: 客户端连接时调用的处理函数
  // 返回 false 表示路径已存在
  bool RegisterSseEndpoint(const std::string& path, SseHandler handler);

 private:
  // 连接配置
  std::string host_;
  int port_;

  // RPC 方法管理器（外部注入，不持有所有权）
  RpcManager& rpc_manager_;

  // 运行状态（多线程安全）
  std::atomic<bool> is_running_{false};

  // 启动时间（用于 /health 端点）
  std::chrono::steady_clock::time_point start_time_;

  class Impl;
  std::unique_ptr<Impl> pimpl_;
};

}  // namespace mcp
