#include <cassert>
#include <iostream>
#include <string>

#include "my_rpc.h"

void TestRpcRequest() {
  std::cout << "=== Test RpcRequest ===" << std::endl;

  // 构造请求
  mcp::RpcRequest req;
  req.SetSequenceId(42);
  req.SetServiceName("TestService");
  req.SetMethodName("TestMethod");
  req.SetPayload(R"({"key":"value"})");

  // 序列化
  std::string data;
  assert(req.Serializer(data) && "RpcRequest::Serializer failed");
  std::cout << "  Serialize: " << data.size() << " bytes" << std::endl;

  // 反序列化到新对象
  mcp::RpcRequest req2;
  assert(req2.Deserializer(data) && "RpcRequest::Deserializer failed");

  // 校验
  assert(req2.GetSequenceId() == 42);
  assert(req2.GetServiceName() == "TestService");
  assert(req2.GetMethodName() == "TestMethod");
  assert(req2.GetPayload() == R"({"key":"value"})");
  assert(req2.IsValid());

  std::cout << "  sequence_id: " << req2.GetSequenceId() << std::endl;
  std::cout << "  service_name: " << req2.GetServiceName() << std::endl;
  std::cout << "  method_name: " << req2.GetMethodName() << std::endl;
  std::cout << "  payload: " << req2.GetPayload() << std::endl;
  std::cout << "  PASSED" << std::endl;
}

void TestRpcResponse() {
  std::cout << "=== Test RpcResponse ===" << std::endl;

  // 构造响应
  mcp::RpcResponse resp;
  resp.SetSequenceId(100);
  resp.SetResultData(R"({"status":"ok","data":[1,2,3]})");

  // 序列化
  std::string data;
  assert(resp.Serializer(data) && "RpcResponse::Serializer failed");
  std::cout << "  Serialize: " << data.size() << " bytes" << std::endl;

  // 反序列化
  mcp::RpcResponse resp2;
  assert(resp2.Deserializer(data) && "RpcResponse::Deserializer failed");

  // 校验
  assert(resp2.GetSequenceId() == 100);
  assert(resp2.GetResultData() == R"({"status":"ok","data":[1,2,3]})");

  std::cout << "  sequence_id: " << resp2.GetSequenceId() << std::endl;
  std::cout << "  result_data: " << resp2.GetResultData() << std::endl;
  std::cout << "  PASSED" << std::endl;
}

void TestRpcError() {
  std::cout << "=== Test RpcError ===" << std::endl;

  // 构造错误
  mcp::RpcError err;
  err.SetSequenceId(7);
  err.SetErrorCode(mcp::errc::MethodNotFound);
  err.SetErrorMessage("Method 'foo' not found in service 'bar'");

  // 序列化
  std::string data;
  assert(err.Serializer(data) && "RpcError::Serializer failed");
  std::cout << "  Serialize: " << data.size() << " bytes" << std::endl;

  // 反序列化
  mcp::RpcError err2;
  assert(err2.Deserializer(data) && "RpcError::Deserializer failed");

  // 校验
  assert(err2.GetSequenceId() == 7);
  assert(err2.GetErrorCode() == mcp::errc::MethodNotFound);
  assert(err2.GetErrorMessage() == "Method 'foo' not found in service 'bar'");

  std::cout << "  sequence_id: " << err2.GetSequenceId() << std::endl;
  std::cout << "  error_code: " << err2.GetErrorCode() << std::endl;
  std::cout << "  error_message: " << err2.GetErrorMessage() << std::endl;
  std::cout << "  PASSED" << std::endl;
}

void TestEmptyPayload() {
  std::cout << "=== Test Empty Fields ===" << std::endl;

  mcp::RpcRequest req;
  req.SetSequenceId(0);
  std::string data;
  assert(req.Serializer(data));
  mcp::RpcRequest req2;
  assert(req2.Deserializer(data));
  assert(req2.GetServiceName().empty());
  assert(req2.GetPayload().empty());
  std::cout << "  Empty request: PASSED" << std::endl;

  mcp::RpcError err;
  err.SetErrorCode(0);
  std::string err_data;
  assert(err.Serializer(err_data));
  mcp::RpcError err2;
  assert(err2.Deserializer(err_data));
  assert(err2.GetErrorCode() == 0);
  assert(err2.GetErrorMessage().empty());
  std::cout << "  Empty error: PASSED" << std::endl;
}

int main() {
  TestRpcRequest();
  TestRpcResponse();
  TestRpcError();
  TestEmptyPayload();
  std::cout << "\nAll tests passed!" << std::endl;
  return 0;
}
