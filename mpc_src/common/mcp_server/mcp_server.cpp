#include "mcp_server.h"

#include "log_manager.h"
#include "rpc_manager.h"

namespace mcp {

// ======== 构造 ========

McpServer::McpServer(const std::string& server_name,
                     const std::string& server_version)
    : server_name_(server_name), server_version_(server_version) {
  MCP_LOG_INFO("McpServer created: {} v{}", server_name_, server_version_);
}

// ======== 错误（线程安全）========

void McpServer::SetError(int code, const std::string& msg) {
  MCP_LOG_WARN("McpServer error [{}]: {}", code, msg);
  std::lock_guard<std::mutex> lock(error_mutex_);
  last_error_code_ = code;
  last_error_ = msg;
}

std::string McpServer::GetLastError() const {
  std::lock_guard<std::mutex> lock(error_mutex_);
  return last_error_;
}

int McpServer::GetLastErrorCode() const {
  std::lock_guard<std::mutex> lock(error_mutex_);
  return last_error_code_;
}

std::string McpServer::ErrorResult(const std::string& msg) {
  return MakeErrorResult(msg).ToResultJson().dump();
}

// ======== 变更回调 ========

void McpServer::SetChangeCallback(ChangeCallback cb) {
  std::lock_guard<std::mutex> lock(callback_mutex_);
  change_callback_ = std::move(cb);
}

void McpServer::FireChange(ChangeType type, const std::string& name,
                           const nlohmann::json& def) {
  ChangeCallback cb;
  {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    cb = change_callback_;
  }
  if (cb) {
    MCP_LOG_DEBUG("FireChange: type={} name={}", static_cast<int>(type), name);
    cb(type, name, def);
  }
}

// ======== 注册 ========

bool McpServer::RegisterTool(const std::string& name,
                             const std::string& description,
                             const ToolInputSchema& inputSchema,
                             ToolHandler handler) {
  if (name.empty()) {
    SetError(errc::kEmptyName, "tool name is empty");
    return false;
  }
  if (!handler) {
    SetError(errc::kHandlerEmpty, "tool handler is empty: " + name);
    return false;
  }
  if (inputSchema.IsEmpty()) {
    SetError(errc::kInvalidSchema, "tool schema is empty: " + name);
    return false;
  }
  std::string err = inputSchema.Validate();
  if (!err.empty()) {
    SetError(errc::kInvalidSchema,
             "tool schema invalid: " + name + " - " + err);
    return false;
  }

  nlohmann::json def_json;
  {
    std::unique_lock<std::shared_mutex> lock(tools_mutex_);
    if (tool_defs_.count(name)) {
      SetError(errc::kDuplicateName, "tool already registered: " + name);
      return false;
    }
    auto def = ToolDef(name, description, inputSchema);
    def_json = def.ToJson();
    tool_defs_[name] = std::move(def);
    tool_handlers_[name] = std::move(handler);
  }
  MCP_LOG_INFO("Tool registered: {} ({} props)", name, inputSchema.GetProperties().size());
  FireChange(ChangeType::ToolAdded, name, def_json);
  return true;
}

bool McpServer::RegisterResource(const std::string& uri,
                                 const std::string& name,
                                 const std::string& description,
                                 const std::string& mimeType,
                                 ResourceHandler handler) {
  if (uri.empty()) {
    SetError(errc::kEmptyName, "resource uri is empty");
    return false;
  }
  if (!handler) {
    SetError(errc::kHandlerEmpty, "resource handler is empty: " + uri);
    return false;
  }

  nlohmann::json def_json;
  {
    std::unique_lock<std::shared_mutex> lock(resources_mutex_);
    if (resource_defs_.count(uri)) {
      SetError(errc::kDuplicateName, "resource already registered: " + uri);
      return false;
    }
    auto def = ResourceDef(uri, name, description, mimeType);
    def_json = def.ToJson();
    resource_defs_[uri] = std::move(def);
    resource_handlers_[uri] = std::move(handler);
  }
  MCP_LOG_INFO("Resource registered: {} (mime={})", uri, mimeType);
  FireChange(ChangeType::ResourceAdded, uri, def_json);
  return true;
}

bool McpServer::RegisterPrompt(const std::string& name,
                               const std::string& description,
                               const nlohmann::json& arguments,
                               PromptHandler handler) {
  if (name.empty()) {
    SetError(errc::kEmptyName, "prompt name is empty");
    return false;
  }
  if (!handler) {
    SetError(errc::kHandlerEmpty, "prompt handler is empty: " + name);
    return false;
  }

  nlohmann::json def_json;
  {
    std::unique_lock<std::shared_mutex> lock(prompts_mutex_);
    if (prompt_defs_.count(name)) {
      SetError(errc::kDuplicateName, "prompt already registered: " + name);
      return false;
    }
    auto def = PromptDef(name, description, arguments);
    def_json = def.ToJson();
    prompt_defs_[name] = std::move(def);
    prompt_handlers_[name] = std::move(handler);
  }
  MCP_LOG_INFO("Prompt registered: {}", name);
  FireChange(ChangeType::PromptAdded, name, def_json);
  return true;
}

// ======== 安装 ========

bool McpServer::InstallTo(RpcManager& manager) {
  MCP_LOG_INFO("Installing MCP methods (tools={}, resources={}, prompts={})...",
               tool_defs_.size(), resource_defs_.size(), prompt_defs_.size());
  if (!RegisterInitialize(manager)) return false;
  if (!RegisterToolsList(manager)) return false;
  if (!RegisterToolsCall(manager)) return false;
  if (!RegisterResourcesList(manager)) return false;
  if (!RegisterResourcesRead(manager)) return false;
  if (!RegisterPromptsList(manager)) return false;
  if (!RegisterPromptsGet(manager)) return false;
  MCP_LOG_INFO("MCP methods installed successfully");
  return true;
}

// ======== initialize ========

bool McpServer::RegisterInitialize(RpcManager& manager) {
  return manager.RegisterMethod(
      "mcp", "initialize",
      [this](const std::string& payload) -> std::string {
        auto params = nlohmann::json::parse(payload);

        nlohmann::json caps;
        {
          std::shared_lock<std::shared_mutex> lock(tools_mutex_);
          if (!tool_defs_.empty()) caps["tools"] = nlohmann::json::object();
        }
        {
          std::shared_lock<std::shared_mutex> lock(resources_mutex_);
          if (!resource_defs_.empty()) caps["resources"] = nlohmann::json::object();
        }
        {
          std::shared_lock<std::shared_mutex> lock(prompts_mutex_);
          if (!prompt_defs_.empty()) caps["prompts"] = nlohmann::json::object();
        }

        nlohmann::json result;
        result["protocolVersion"] = params.value("protocolVersion", "0.0.0");
        result["capabilities"] = caps;
        result["serverInfo"]["name"] = server_name_;
        result["serverInfo"]["version"] = server_version_;
        return result.dump();
      });
}

// ======== tools/list ========

bool McpServer::RegisterToolsList(RpcManager& manager) {
  return manager.RegisterMethod(
      "mcp", "tools/list",
      [this](const std::string&) -> std::string {
        nlohmann::json tools = nlohmann::json::array();
        std::shared_lock<std::shared_mutex> lock(tools_mutex_);
        for (const auto& [name, def] : tool_defs_) {
          tools.push_back(def.ToJson());
        }
        return nlohmann::json({{"tools", tools}}).dump();
      });
}

// ======== tools/call ========

bool McpServer::RegisterToolsCall(RpcManager& manager) {
  return manager.RegisterMethod(
      "mcp", "tools/call",
      [this](const std::string& payload) -> std::string {
        auto params = nlohmann::json::parse(payload);
        std::string name = params.value("name", "");

        ToolHandler handler;
        {
          std::shared_lock<std::shared_mutex> lock(tools_mutex_);
          auto it = tool_handlers_.find(name);
          if (it == tool_handlers_.end()) {
            MCP_LOG_WARN("tools/call: not found: {}", name);
            return ErrorResult("tool not found: " + name);
          }
          handler = it->second;
        }
        // 解锁后执行，不阻塞其他请求

        MCP_LOG_DEBUG("tools/call: {}", name);
        try {
          nlohmann::json args = params.contains("arguments")
                                    ? params["arguments"]
                                    : nlohmann::json::object();
          return handler(args).ToResultJson().dump();
        } catch (const std::exception& e) {
          MCP_LOG_ERROR("tools/call exception: {} - {}", name, e.what());
          return ErrorResult(e.what());
        }
      });
}

// ======== resources/list ========

bool McpServer::RegisterResourcesList(RpcManager& manager) {
  return manager.RegisterMethod(
      "mcp", "resources/list",
      [this](const std::string&) -> std::string {
        nlohmann::json resources = nlohmann::json::array();
        std::shared_lock<std::shared_mutex> lock(resources_mutex_);
        for (const auto& [uri, def] : resource_defs_) {
          resources.push_back(def.ToJson());
        }
        return nlohmann::json({{"resources", resources}}).dump();
      });
}

// ======== resources/read ========

bool McpServer::RegisterResourcesRead(RpcManager& manager) {
  return manager.RegisterMethod(
      "mcp", "resources/read",
      [this](const std::string& payload) -> std::string {
        auto params = nlohmann::json::parse(payload);
        std::string uri = params.value("uri", "");

        ResourceHandler handler;
        std::string mime_type;
        {
          std::shared_lock<std::shared_mutex> lock(resources_mutex_);
          auto def_it = resource_defs_.find(uri);
          if (def_it == resource_defs_.end()) {
            MCP_LOG_WARN("resources/read: not found: {}", uri);
            return ErrorResult("resource not found: " + uri);
          }
          auto hdl_it = resource_handlers_.find(uri);
          if (hdl_it == resource_handlers_.end()) {
            MCP_LOG_WARN("resources/read: handler not found: {}", uri);
            return ErrorResult("resource handler not found: " + uri);
          }
          mime_type = def_it->second.GetMimeType();
          handler = hdl_it->second;
        }

        MCP_LOG_DEBUG("resources/read: {} (mime={})", uri, mime_type);
        try {
          ResourceResult data = handler();
          nlohmann::json content_item;
          content_item["uri"] = uri;
          content_item["mimeType"] = data.GetMimeType();
          if (data.IsBinary()) {
            content_item["blob"] = data.GetContent();
          } else {
            content_item["text"] = data.GetContent();
          }
          return nlohmann::json({{"contents", nlohmann::json::array({content_item})}})
              .dump();
        } catch (const std::exception& e) {
          MCP_LOG_ERROR("resources/read exception: {} - {}", uri, e.what());
          return ErrorResult(e.what());
        }
      });
}

// ======== prompts/list ========

bool McpServer::RegisterPromptsList(RpcManager& manager) {
  return manager.RegisterMethod(
      "mcp", "prompts/list",
      [this](const std::string&) -> std::string {
        nlohmann::json prompts = nlohmann::json::array();
        std::shared_lock<std::shared_mutex> lock(prompts_mutex_);
        for (const auto& [name, def] : prompt_defs_) {
          prompts.push_back(def.ToJson());
        }
        return nlohmann::json({{"prompts", prompts}}).dump();
      });
}

// ======== prompts/get ========

bool McpServer::RegisterPromptsGet(RpcManager& manager) {
  return manager.RegisterMethod(
      "mcp", "prompts/get",
      [this](const std::string& payload) -> std::string {
        auto params = nlohmann::json::parse(payload);
        std::string name = params.value("name", "");

        PromptHandler handler;
        {
          std::shared_lock<std::shared_mutex> lock(prompts_mutex_);
          auto it = prompt_handlers_.find(name);
          if (it == prompt_handlers_.end()) {
            MCP_LOG_WARN("prompts/get: not found: {}", name);
            return ErrorResult("prompt not found: " + name);
          }
          handler = it->second;
        }

        MCP_LOG_DEBUG("prompts/get: {}", name);
        try {
          nlohmann::json args = params.contains("arguments")
                                    ? params["arguments"]
                                    : nlohmann::json::object();
          nlohmann::json result;
          result["messages"] = handler(args);
          return result.dump();
        } catch (const std::exception& e) {
          MCP_LOG_ERROR("prompts/get exception: {} - {}", name, e.what());
          return ErrorResult(e.what());
        }
      });
}

}  // namespace mcp
