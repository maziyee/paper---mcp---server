#include "mcp_server.h"

#include "rpc_manager.h"

namespace mcp {

// ======== 构造 ========

McpServer::McpServer(const std::string& server_name,
                     const std::string& server_version)
    : server_name_(server_name), server_version_(server_version) {}

// ======== 注册 ========

void McpServer::RegisterTool(const std::string& name,
                             const std::string& description,
                             const ToolInputSchema& inputSchema,
                             ToolHandler handler) {
  tool_defs_[name] = ToolDef(name, description, inputSchema);
  tool_handlers_[name] = std::move(handler);
}

void McpServer::RegisterResource(const std::string& uri,
                                 const std::string& name,
                                 const std::string& description,
                                 const std::string& mimeType,
                                 ResourceHandler handler) {
  resource_defs_[uri] = ResourceDef(uri, name, description, mimeType);
  resource_handlers_[uri] = std::move(handler);
}

void McpServer::RegisterPrompt(const std::string& name,
                               const std::string& description,
                               const nlohmann::json& arguments,
                               PromptHandler handler) {
  prompt_defs_[name] = PromptDef(name, description, arguments);
  prompt_handlers_[name] = std::move(handler);
}

// ======== 安装 ========

void McpServer::InstallTo(RpcManager& manager) {
  RegisterInitialize(manager);
  RegisterToolsList(manager);
  RegisterToolsCall(manager);
  RegisterResourcesList(manager);
  RegisterResourcesRead(manager);
  RegisterPromptsList(manager);
  RegisterPromptsGet(manager);
}

// ======== 错误结果 ========

std::string McpServer::ErrorResult(const std::string& msg) {
  return MakeErrorResult(msg).ToResultJson().dump();
}

// ======== initialize ========

void McpServer::RegisterInitialize(RpcManager& manager) {
  manager.RegisterMethod(
      "mcp", "initialize",
      [this](const std::string& payload) -> std::string {
        auto params = nlohmann::json::parse(payload);
        nlohmann::json capabilities;
        capabilities["tools"] = nlohmann::json::object();
        capabilities["resources"] = nlohmann::json::object();
        capabilities["prompts"] = nlohmann::json::object();
        if (tool_defs_.empty()) capabilities.erase("tools");
        if (resource_defs_.empty()) capabilities.erase("resources");
        if (prompt_defs_.empty()) capabilities.erase("prompts");

        nlohmann::json result;
        result["protocolVersion"] = params.value("protocolVersion", "0.0.0");
        result["capabilities"] = capabilities;
        result["serverInfo"]["name"] = server_name_;
        result["serverInfo"]["version"] = server_version_;
        return result.dump();
      });
}

// ======== tools/list ========

void McpServer::RegisterToolsList(RpcManager& manager) {
  manager.RegisterMethod(
      "mcp", "tools/list",
      [this](const std::string& /*payload*/) -> std::string {
        nlohmann::json tools = nlohmann::json::array();
        for (const auto& [name, def] : tool_defs_) {
          tools.push_back(def.ToJson());
        }
        nlohmann::json result;
        result["tools"] = tools;
        return result.dump();
      });
}

// ======== tools/call ========

void McpServer::RegisterToolsCall(RpcManager& manager) {
  manager.RegisterMethod(
      "mcp", "tools/call",
      [this](const std::string& payload) -> std::string {
        auto params = nlohmann::json::parse(payload);
        std::string name = params.value("name", "");
        nlohmann::json args = params.contains("arguments")
                                  ? params["arguments"]
                                  : nlohmann::json::object();

        auto it = tool_handlers_.find(name);
        if (it == tool_handlers_.end()) {
          return MakeErrorResult("tool not found: " + name)
              .ToResultJson()
              .dump();
        }

        try {
          ToolResult tool_result = it->second(args);
          return tool_result.ToResultJson().dump();
        } catch (const std::exception& e) {
          return MakeErrorResult(e.what()).ToResultJson().dump();
        }
      });
}

// ======== resources/list ========

void McpServer::RegisterResourcesList(RpcManager& manager) {
  manager.RegisterMethod(
      "mcp", "resources/list",
      [this](const std::string& /*payload*/) -> std::string {
        nlohmann::json resources = nlohmann::json::array();
        for (const auto& [uri, def] : resource_defs_) {
          resources.push_back(def.ToJson());
        }
        nlohmann::json result;
        result["resources"] = resources;
        return result.dump();
      });
}

// ======== resources/read ========

void McpServer::RegisterResourcesRead(RpcManager& manager) {
  manager.RegisterMethod(
      "mcp", "resources/read",
      [this](const std::string& payload) -> std::string {
        auto params = nlohmann::json::parse(payload);
        std::string uri = params.value("uri", "");

        auto def_it = resource_defs_.find(uri);
        if (def_it == resource_defs_.end()) {
          return ErrorResult("resource not found: " + uri);
        }

        auto hdl_it = resource_handlers_.find(uri);
        if (hdl_it == resource_handlers_.end()) {
          return ErrorResult("resource handler not found: " + uri);
        }

        try {
          ResourceResult data = hdl_it->second();

          nlohmann::json content_item;
          content_item["uri"] = uri;
          content_item["mimeType"] = data.GetMimeType();
          if (data.IsBinary()) {
            content_item["blob"] = data.GetContent();
          } else {
            content_item["text"] = data.GetContent();
          }

          nlohmann::json result;
          result["contents"] = nlohmann::json::array({content_item});
          return result.dump();
        } catch (const std::exception& e) {
          return ErrorResult(e.what());
        }
      });
}

// ======== prompts/list ========

void McpServer::RegisterPromptsList(RpcManager& manager) {
  manager.RegisterMethod(
      "mcp", "prompts/list",
      [this](const std::string& /*payload*/) -> std::string {
        nlohmann::json prompts = nlohmann::json::array();
        for (const auto& [name, def] : prompt_defs_) {
          prompts.push_back(def.ToJson());
        }
        nlohmann::json result;
        result["prompts"] = prompts;
        return result.dump();
      });
}

// ======== prompts/get ========

void McpServer::RegisterPromptsGet(RpcManager& manager) {
  manager.RegisterMethod(
      "mcp", "prompts/get",
      [this](const std::string& payload) -> std::string {
        auto params = nlohmann::json::parse(payload);
        std::string name = params.value("name", "");
        nlohmann::json args = params.contains("arguments")
                                  ? params["arguments"]
                                  : nlohmann::json::object();

        auto hdl_it = prompt_handlers_.find(name);
        if (hdl_it == prompt_handlers_.end()) {
          return ErrorResult("prompt not found: " + name);
        }

        try {
          auto messages = hdl_it->second(args);
          nlohmann::json result;
          result["messages"] = messages;
          return result.dump();
        } catch (const std::exception& e) {
          return ErrorResult(e.what());
        }
      });
}

}  // namespace mcp
