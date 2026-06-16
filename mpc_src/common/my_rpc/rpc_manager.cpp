#include "rpc_manager.h"

#include "nlohmann/json.hpp"

namespace mcp {

// ======== 私有 ========

std::string RpcManager::MakeKey(const std::string& service,
                                const std::string& method) {
  return service + "/" + method;
}

// ======== 注册方法 ========

bool RpcManager::RegisterMethod(const std::string& service_name,
                                const std::string& method_name,
                                Handler handler) {
  if (service_name.empty() || method_name.empty() || !handler) {
    return false;
  }
  std::string key = MakeKey(service_name, method_name);
  handlers_[key] = std::move(handler);
  return true;
}

// ======== 处理请求 ========

nlohmann::json RpcManager::HandleRequest(const RpcRequest& req) {
  if (!req.IsValid()) {
    RpcError err;
    err.SetSequenceId(req.GetSequenceId());
    err.SetErrorCode(errc::InvalidRequest);
    err.SetErrorMessage("missing service_name or method_name");
    return err.ToJsonRpc();
  }

  std::string key = MakeKey(req.GetServiceName(), req.GetMethodName());
  auto it = handlers_.find(key);
  if (it == handlers_.end()) {
    RpcError err;
    err.SetSequenceId(req.GetSequenceId());
    err.SetErrorCode(errc::MethodNotFound);
    err.SetErrorMessage("method not found: " + key);
    return err.ToJsonRpc();
  }

  try {
    std::string result = it->second(req.GetPayload());
    RpcResponse resp;
    resp.SetSequenceId(req.GetSequenceId());
    resp.SetResultData(std::move(result));
    return resp.ToJsonRpc();
  } catch (const std::exception& e) {
    RpcError err;
    err.SetSequenceId(req.GetSequenceId());
    err.SetErrorCode(errc::InternalError);
    err.SetErrorMessage(std::string("handler exception: ") + e.what());
    return err.ToJsonRpc();
  }
}

}  // namespace mcp
