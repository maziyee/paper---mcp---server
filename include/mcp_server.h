#pragma once

#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include "mcp_types.h"

namespace mcp {

class RpcManager;

// MCP 协议层，提供 tools / resources / prompts 注册和调度
//
// 线程安全：
//   每个类别独立加锁，互不阻塞
//   - 注册（Register*）：独占锁
//   - 查询/执行（list/call/read/get）：共享锁，允许多个请求并发读
//   - tools 操作不阻塞 resources/prompts，反之亦然
//
// 用法：
//   RpcManager manager;
//   McpServer mcp("my_server", "1.0");
//   if (!mcp.RegisterTool("echo", "...", schema, handler)) { ... }
//   if (!mcp.InstallTo(manager)) { ... }
//
class McpServer {
 public:
  McpServer(const std::string& server_name, const std::string& server_version);

  // ======== 注册（独占对应锁，返回 false 表示参数无效）========

  bool RegisterTool(const std::string& name, const std::string& description,
                    const ToolInputSchema& inputSchema, ToolHandler handler);

  bool RegisterResource(const std::string& uri, const std::string& name,
                        const std::string& description,
                        const std::string& mimeType, ResourceHandler handler);

  bool RegisterPrompt(const std::string& name, const std::string& description,
                      const nlohmann::json& arguments,
                      PromptHandler handler);

  // ======== 安装 MCP 协议方法到 RpcManager ========

  bool InstallTo(RpcManager& manager);

  // ======== 变更回调 ========
  // 注册回调后，每次 RegisterTool/Resource/Prompt 成功时触发
  // 回调在锁外执行，可以安全地做耗时操作

  void SetChangeCallback(ChangeCallback cb);

  // ======== 错误信息（线程安全）========

  std::string GetLastError() const;
  int GetLastErrorCode() const;

 private:
  void FireChange(ChangeType type, const std::string& name,
                  const nlohmann::json& def);

  // ======== 私有方法 ========
  bool RegisterInitialize(RpcManager& manager);
  bool RegisterInitialized(RpcManager& manager);
  bool RegisterToolsList(RpcManager& manager);
  bool RegisterToolsCall(RpcManager& manager);
  bool RegisterResourcesList(RpcManager& manager);
  bool RegisterResourcesRead(RpcManager& manager);
  bool RegisterPromptsList(RpcManager& manager);
  bool RegisterPromptsGet(RpcManager& manager);

  void SetError(int code, const std::string& msg);
  static std::string ErrorResult(const std::string& msg);

  // ======== 数据 + 锁（每类独立）========

  // 工具
  mutable std::shared_mutex tools_mutex_;
  std::unordered_map<std::string, ToolDef> tool_defs_;
  std::unordered_map<std::string, ToolHandler> tool_handlers_;

  // 资源
  mutable std::shared_mutex resources_mutex_;
  std::unordered_map<std::string, ResourceDef> resource_defs_;
  std::unordered_map<std::string, ResourceHandler> resource_handlers_;

  // 提示
  mutable std::shared_mutex prompts_mutex_;
  std::unordered_map<std::string, PromptDef> prompt_defs_;
  std::unordered_map<std::string, PromptHandler> prompt_handlers_;

  // 变更回调
  mutable std::mutex callback_mutex_;
  ChangeCallback change_callback_;

  // 错误
  mutable std::mutex error_mutex_;
  std::string last_error_;
  int last_error_code_ = 0;

  std::string server_name_;
  std::string server_version_;
};

}  // namespace mcp
