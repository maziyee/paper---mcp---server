#pragma once

#include <istream>
#include <ostream>
#include <string>

#include "my_rpc.h"
#include "rpc_manager.h"

namespace mcp {

// 基于 stdin/stdout 的 RPC 服务端，Content-Length 协议分帧，JSON 消息体
//
// 输入帧:
//   Content-Length: <N>\r\n\r\n{"service_name":"...","method_name":"...","payload":"...","id":1}
//
// 输出帧:
//   Content-Length: <M>\r\n\r\n{"id":1,"result":"..."}  或  {"id":1,"error":{"code":-32601,"message":"..."}}
//
class StdRpcServer {
 public:
  StdRpcServer(RpcManager& rpc_manager, std::ostream& out, std::istream& in);

  // 循环处理直到 EOF
  void Run();

 private:
  // 从 in_ 读取一帧，返回原始负载
  bool ReadMessage(std::string& out_body);
  // 向 out_ 写入 RPC 响应/错误，use_jsonrpc=true 时输出 JSON-RPC 2.0 格式
  bool WriteMessage(const RpcResponse& resp, bool use_jsonrpc = false);
  bool WriteMessage(const RpcError& err, bool use_jsonrpc = false);

  RpcManager& rpc_manager_;
  std::ostream& out_;
  std::istream& in_;
};

}  // namespace mcp
