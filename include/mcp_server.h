#pragma once

#include <string>
#include <unordered_map>

#include "mcp_types.h"

namespace mcp {

class RpcManager;

// MCP 协议层，提供 tools / resources / prompts 注册和调度
//
// 架构：
//   - tool_defs_      存储工具元数据（名称、描述、参数 schema），用于 tools/list
//   - tool_handlers_   存储工具执行函数，用于 tools/call
//   - resource_defs_ / resource_handlers_ 同理
//   - prompt_defs_   / prompt_handlers_   同理
//
// 用法：
//   RpcManager manager;
//   McpServer mcp("my_server", "1.0");
//   mcp.RegisterTool("echo", "...", {...}, handler);
//   mcp.InstallTo(manager);
//   StdRpcServer server(manager, std::cout, std::cin);
//   server.Run();
//
class McpServer {
 public:
  McpServer(const std::string& server_name, const std::string& server_version);

  // 注册
  void RegisterTool(const std::string& name, const std::string& description,
                    const ToolInputSchema& inputSchema, ToolHandler handler);

  void RegisterResource(const std::string& uri, const std::string& name,
                        const std::string& description,
                        const std::string& mimeType, ResourceHandler handler);

  void RegisterPrompt(const std::string& name, const std::string& description,
                      const nlohmann::json& arguments,
                      PromptHandler handler);

  // 安装 MCP 协议方法到 RpcManager
  void InstallTo(RpcManager& manager);

 private:
  void RegisterInitialize(RpcManager& manager);
  void RegisterToolsList(RpcManager& manager);
  void RegisterToolsCall(RpcManager& manager);
  void RegisterResourcesList(RpcManager& manager);
  void RegisterResourcesRead(RpcManager& manager);
  void RegisterPromptsList(RpcManager& manager);
  void RegisterPromptsGet(RpcManager& manager);

  static std::string ErrorResult(const std::string& msg);

  std::string server_name_;
  std::string server_version_;

  // 工具：定义 + 处理器分离
  std::unordered_map<std::string, ToolDef> tool_defs_;
  std::unordered_map<std::string, ToolHandler> tool_handlers_;

  // 资源：定义 + 处理器分离
  std::unordered_map<std::string, ResourceDef> resource_defs_;
  std::unordered_map<std::string, ResourceHandler> resource_handlers_;

  // 提示：定义 + 处理器分离
  std::unordered_map<std::string, PromptDef> prompt_defs_;
  std::unordered_map<std::string, PromptHandler> prompt_handlers_;
};

}  // namespace mcp
