#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include "my_rpc.h"

namespace mcp {

class RpcManager {
 public:
  // handler: payload -> result
  using Handler = std::function<std::string(const std::string& payload)>;

  // 注册方法
  bool RegisterMethod(const std::string& service_name,
                      const std::string& method_name, Handler handler);

  // 处理请求，返回序列化后的响应
  std::string HandleRequest(const RpcRequest& req);

 private:
  static std::string MakeKey(const std::string& service,
                             const std::string& method);

  std::unordered_map<std::string, Handler> handlers_;
};

}  // namespace mcp
