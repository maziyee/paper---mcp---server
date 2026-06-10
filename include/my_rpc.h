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

  // JSON → RpcRequest
  static RpcRequest FromJson(const nlohmann::json& j) {
    RpcRequest req;
    req.SetSequenceId(j.value("id", 0));
    req.SetServiceName(j.value("service_name", ""));
    req.SetMethodName(j.value("method_name", ""));
    if (j.contains("payload")) {
      if (j["payload"].is_string()) {
        req.SetPayload(j["payload"].get<std::string>());
      } else {
        req.SetPayload(j["payload"].dump());
      }
    }
    return req;
  }

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

  // RpcResponse → JSON
  nlohmann::json ToJson() const {
    nlohmann::json j;
    j["id"] = GetSequenceId();
    j["result"] = result_data_;
    return j;
  }

 private:
  std::string result_data_;
};

namespace errc {
// 协议解析错误
constexpr int ParseError = -32700;        // 数据包解析失败
constexpr int InvalidRequest = -32600;    // 请求格式无效
constexpr int ServiceNotFound = -32601;   // 服务不存在
constexpr int MethodNotFound = -32602;    // 方法不存在
constexpr int InvalidParams = -32603;     // 参数无效
constexpr int InternalError = -32604;     // 内部错误

// -32000 ~ -32099 预留给业务自定义错误
constexpr int BusinessError = -32000;
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

  // RpcError → JSON
  nlohmann::json ToJson() const {
    nlohmann::json j;
    j["id"] = GetSequenceId();
    j["error"]["code"] = error_code_;
    j["error"]["message"] = error_message_;
    return j;
  }

 private:
  int error_code_ = 0;
  std::string error_message_;
};

}  // namespace mcp
