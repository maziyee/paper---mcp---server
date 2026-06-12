#include "rpc_manager.h"

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

std::string RpcManager::HandleRequest(const RpcRequest& req) {
  if (!req.IsValid()) {
    RpcError err;
    err.SetSequenceId(req.GetSequenceId());
    err.SetErrorCode(errc::InvalidRequest);
    err.SetErrorMessage("missing service_name or method_name");
    std::string out;
    err.Serializer(out);
    return out;
  }

  std::string key = MakeKey(req.GetServiceName(), req.GetMethodName());
  auto it = handlers_.find(key);
  if (it == handlers_.end()) {
    RpcError err;
    err.SetSequenceId(req.GetSequenceId());
    err.SetErrorCode(errc::MethodNotFound);
    err.SetErrorMessage("method not found: " + key);
    std::string out;
    err.Serializer(out);
    return out;
  }

  RpcResponse resp;
  resp.SetSequenceId(req.GetSequenceId());
  try {
    std::string result = it->second(req.GetPayload());
    resp.SetResultData(std::move(result));
  } catch (const std::exception& e) {
    RpcError err;
    err.SetSequenceId(req.GetSequenceId());
    err.SetErrorCode(errc::InternalError);
    err.SetErrorMessage(std::string("handler exception: ") + e.what());
    std::string out;
    err.Serializer(out);
    return out;
  }

  std::string out;
  resp.Serializer(out);
  return out;
}

}  // namespace mcp
