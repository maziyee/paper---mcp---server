#pragma once

#include <stdint.h>

#include <string>

#include "nlohmann/json.hpp"

namespace mcp {

struct RpcHeader {
  uint32_t body_size_;
  uint32_t sequence_id_;
};

class RpcBase {
 public:
  virtual ~RpcBase() = default;
  virtual bool Serializer(std::string& out) = 0;
  virtual bool Deserializer(const std::string& in) = 0;

  uint32_t GetSequenceId() const { return sequence_id_; }
  void SetSequenceId(uint32_t sequence_id) { sequence_id_ = sequence_id; }

 private:
  uint32_t sequence_id_;
};

class RpcRequest : public RpcBase {
 public:
  std::string GetServiceName() const { return service_name_; }
  void SetServiceName(const std::string& service_name) {
    service_name_ = service_name;
  }

  std::string GetMethodName() const { return method_name_; }
  void SetMethodName(const std::string& method_name) {
    method_name_ = method_name;
  }

  std::string GetPayload() const { return payload_; }
  void SetPayload(const std::string& payload) { payload_ = payload; }

  bool Serializer(std::string& out) override;
  bool Deserializer(const std::string& in) override;

  bool IsValid() const {
    return !service_name_.empty() && !method_name_.empty();
  }

  // JSON → RpcRequest（自动识别自定义格式和 JSON-RPC 2.0 格式）
  //
  // 自定义格式： {"service_name":"...","method_name":"...","payload":...,"id":1}
  // JSON-RPC 2.0：{"jsonrpc":"2.0","method":"service/method","params":...,"id":1}
  static RpcRequest FromJson(const nlohmann::json& j);

 private:
  std::string service_name_;
  std::string method_name_;
  std::string payload_;
};

class RpcResponse : public RpcBase {
 public:
  std::string GetResultData() const { return result_data_; }
  void SetResultData(const std::string& result_data) {
    result_data_ = result_data;
  }

  bool Serializer(std::string& out) override;
  bool Deserializer(const std::string& in) override;

  // RpcResponse → JSON（自定义格式）
  nlohmann::json ToJson() const;

  // RpcResponse → JSON（JSON-RPC 2.0 格式）
  nlohmann::json ToJsonRpc() const;

 private:
  std::string result_data_;
};

namespace errc {
// JSON-RPC 标准错误（-32700 ~ -32600）
constexpr int ParseError = -32700;        // 数据包解析失败
constexpr int InvalidRequest = -32600;    // 请求格式无效
constexpr int ServiceNotFound = -32601;   // 服务不存在
constexpr int MethodNotFound = -32602;    // 方法不存在
constexpr int InvalidParams = -32603;     // 参数无效
constexpr int InternalError = -32604;     // 内部错误

// -32000 ~ -32099 预留给业务自定义错误
constexpr int BusinessError = -32000;

// McpServer 注册错误（-32100 ~ -32199）
constexpr int kInvalidSchema = -32100;     // ToolInputSchema 校验失败
constexpr int kEmptyName = -32101;         // 名称为空
constexpr int kDuplicateName = -32102;     // 名称重复
constexpr int kHandlerEmpty = -32103;      // handler 为空
constexpr int kRegisterFailed = -32104;    // 注册到 RpcManager 失败
constexpr int kInstallIncomplete = -32105; // 未完全安装
}  // namespace errc

class RpcError : public RpcBase {
 public:
  int GetErrorCode() const { return error_code_; }
  void SetErrorCode(int error_code) { error_code_ = error_code; }

  std::string GetErrorMessage() const { return error_message_; }
  void SetErrorMessage(const std::string& error_message) {
    error_message_ = error_message;
  }

  bool Serializer(std::string& out) override;
  bool Deserializer(const std::string& in) override;

  // RpcError → JSON（自定义格式）
  nlohmann::json ToJson() const;

  // RpcError → JSON（JSON-RPC 2.0 格式）
  nlohmann::json ToJsonRpc() const;

 private:
  int error_code_ = 0;
  std::string error_message_;
};

}  // namespace mcp
