#include "my_rpc.h"

#include <cstring>

#include "log_manager.h"

namespace mcp {

// ==================== RpcRequest ====================

bool RpcRequest::Serializer(std::string& out) {
  try {
    uint32_t svc_len = service_name_.size();
    uint32_t mtd_len = method_name_.size();
    uint32_t pld_len = payload_.size();

    uint32_t body_size = sizeof(uint32_t) * 3 + svc_len + mtd_len + pld_len;
    RpcHeader header{body_size, GetSequenceId()};

    out.resize(sizeof(RpcHeader) + body_size);
    size_t pos = 0;

    // 写 Header
    std::memcpy(&out[pos], &header, sizeof(RpcHeader));
    pos += sizeof(RpcHeader);

    // 写 service_name
    std::memcpy(&out[pos], &svc_len, sizeof(uint32_t));
    pos += sizeof(uint32_t);
    std::memcpy(&out[pos], service_name_.data(), svc_len);
    pos += svc_len;

    // 写 method_name
    std::memcpy(&out[pos], &mtd_len, sizeof(uint32_t));
    pos += sizeof(uint32_t);
    std::memcpy(&out[pos], method_name_.data(), mtd_len);
    pos += mtd_len;

    // 写 payload
    std::memcpy(&out[pos], &pld_len, sizeof(uint32_t));
    pos += sizeof(uint32_t);
    std::memcpy(&out[pos], payload_.data(), pld_len);

    return true;
  } catch (std::exception& e) {
    MCP_LOG_ERROR("RpcRequest::Serializer error: {}", e.what());
    return false;
  }
}

bool RpcRequest::Deserializer(const std::string& in) {
  try {
    if (in.size() < sizeof(RpcHeader)) {
      MCP_LOG_ERROR("RpcRequest::Deserializer: input too small for header");
      return false;
    }

    RpcHeader header;
    std::memcpy(&header, in.data(), sizeof(RpcHeader));
    SetSequenceId(header.sequence_id_);

    size_t pos = sizeof(RpcHeader);
    size_t end = pos + header.body_size_;
    if (in.size() < end) {
      MCP_LOG_ERROR("RpcRequest::Deserializer: input too small for body");
      return false;
    }

    auto read_str = [&](std::string& dst) -> bool {
      if (pos + sizeof(uint32_t) > end) return false;
      uint32_t len;
      std::memcpy(&len, &in[pos], sizeof(uint32_t));
      pos += sizeof(uint32_t);
      if (pos + len > end) return false;
      dst.assign(&in[pos], len);
      pos += len;
      return true;
    };

    if (!read_str(service_name_)) {
      MCP_LOG_ERROR("RpcRequest::Deserializer: failed to read service_name");
      return false;
    }
    if (!read_str(method_name_)) {
      MCP_LOG_ERROR("RpcRequest::Deserializer: failed to read method_name");
      return false;
    }
    if (!read_str(payload_)) {
      MCP_LOG_ERROR("RpcRequest::Deserializer: failed to read payload");
      return false;
    }

    return true;
  } catch (std::exception& e) {
    MCP_LOG_ERROR("RpcRequest::Deserializer error: {}", e.what());
    return false;
  }
}

// ==================== RpcResponse ====================

bool RpcResponse::Serializer(std::string& out) {
  try {
    uint32_t data_len = result_data_.size();
    uint32_t body_size = sizeof(uint32_t) + data_len;

    RpcHeader header{body_size, GetSequenceId()};
    out.resize(sizeof(RpcHeader) + body_size);
    size_t pos = 0;

    std::memcpy(&out[pos], &header, sizeof(RpcHeader));
    pos += sizeof(RpcHeader);

    std::memcpy(&out[pos], &data_len, sizeof(uint32_t));
    pos += sizeof(uint32_t);
    std::memcpy(&out[pos], result_data_.data(), data_len);

    return true;
  } catch (std::exception& e) {
    MCP_LOG_ERROR("RpcResponse::Serializer error: {}", e.what());
    return false;
  }
}

bool RpcResponse::Deserializer(const std::string& in) {
  try {
    if (in.size() < sizeof(RpcHeader)) {
      MCP_LOG_ERROR("RpcResponse::Deserializer: input too small for header");
      return false;
    }

    RpcHeader header;
    std::memcpy(&header, in.data(), sizeof(RpcHeader));
    SetSequenceId(header.sequence_id_);

    size_t pos = sizeof(RpcHeader);
    size_t end = pos + header.body_size_;
    if (in.size() < end) {
      MCP_LOG_ERROR("RpcResponse::Deserializer: input too small for body");
      return false;
    }

    if (pos + sizeof(uint32_t) > end) {
      MCP_LOG_ERROR("RpcResponse::Deserializer: failed to read result_data size");
      return false;
    }
    uint32_t data_len;
    std::memcpy(&data_len, &in[pos], sizeof(uint32_t));
    pos += sizeof(uint32_t);

    if (pos + data_len > end) {
      MCP_LOG_ERROR("RpcResponse::Deserializer: failed to read result_data");
      return false;
    }
    result_data_.assign(&in[pos], data_len);

    return true;
  } catch (std::exception& e) {
    MCP_LOG_ERROR("RpcResponse::Deserializer error: {}", e.what());
    return false;
  }
}

// ==================== RpcError ====================

bool RpcError::Serializer(std::string& out) {
  try {
    uint32_t msg_len = error_message_.size();
    uint32_t body_size = sizeof(int) + sizeof(uint32_t) + msg_len;

    RpcHeader header{body_size, GetSequenceId()};
    out.resize(sizeof(RpcHeader) + body_size);
    size_t pos = 0;

    std::memcpy(&out[pos], &header, sizeof(RpcHeader));
    pos += sizeof(RpcHeader);

    std::memcpy(&out[pos], &error_code_, sizeof(int));
    pos += sizeof(int);

    std::memcpy(&out[pos], &msg_len, sizeof(uint32_t));
    pos += sizeof(uint32_t);
    std::memcpy(&out[pos], error_message_.data(), msg_len);

    return true;
  } catch (std::exception& e) {
    MCP_LOG_ERROR("RpcError::Serializer error: {}", e.what());
    return false;
  }
}

bool RpcError::Deserializer(const std::string& in) {
  try {
    if (in.size() < sizeof(RpcHeader)) {
      MCP_LOG_ERROR("RpcError::Deserializer: input too small for header");
      return false;
    }

    RpcHeader header;
    std::memcpy(&header, in.data(), sizeof(RpcHeader));
    SetSequenceId(header.sequence_id_);

    size_t pos = sizeof(RpcHeader);
    size_t end = pos + header.body_size_;
    if (in.size() < end) {
      MCP_LOG_ERROR("RpcError::Deserializer: input too small for body");
      return false;
    }

    if (pos + sizeof(int) > end) {
      MCP_LOG_ERROR("RpcError::Deserializer: failed to read error_code");
      return false;
    }
    std::memcpy(&error_code_, &in[pos], sizeof(int));
    pos += sizeof(int);

    if (pos + sizeof(uint32_t) > end) {
      MCP_LOG_ERROR("RpcError::Deserializer: failed to read error_message size");
      return false;
    }
    uint32_t msg_len;
    std::memcpy(&msg_len, &in[pos], sizeof(uint32_t));
    pos += sizeof(uint32_t);

    if (pos + msg_len > end) {
      MCP_LOG_ERROR("RpcError::Deserializer: failed to read error_message");
      return false;
    }
    error_message_.assign(&in[pos], msg_len);

    return true;
  } catch (std::exception& e) {
    MCP_LOG_ERROR("RpcError::Deserializer error: {}", e.what());
    return false;
  }
}

// ==================== JSON 序列化 ====================

// static
RpcRequest RpcRequest::FromJson(const nlohmann::json& j) {
  RpcRequest req;
  req.SetSequenceId(j.value("id", 0));

  if (j.contains("method") && !j.contains("service_name")) {
    // === JSON-RPC 2.0 格式 ===
    // MCP 协议方法: "initialize", "tools/list", "tools/call" 等
    // 统一映射：service_name="mcp", method_name=完整方法名
    std::string full = j.value("method", "");
    req.SetServiceName("mcp");
    req.SetMethodName(full);
    // "params" → payload
    if (j.contains("params")) {
      if (j["params"].is_string()) {
        req.SetPayload(j["params"].get<std::string>());
      } else {
        req.SetPayload(j["params"].dump());
      }
    }
  } else {
    // === 自定义格式 ===
    req.SetServiceName(j.value("service_name", ""));
    req.SetMethodName(j.value("method_name", ""));
    if (j.contains("payload")) {
      if (j["payload"].is_string()) {
        req.SetPayload(j["payload"].get<std::string>());
      } else {
        req.SetPayload(j["payload"].dump());
      }
    }
  }
  return req;
}

nlohmann::json RpcResponse::ToJson() const {
  nlohmann::json j;
  j["id"] = GetSequenceId();
  j["result"] = result_data_;
  return j;
}

nlohmann::json RpcResponse::ToJsonRpc() const {
  nlohmann::json j;
  j["jsonrpc"] = "2.0";
  j["id"] = GetSequenceId();
  // MCP 协议要求 result 是 JSON 对象，尝试解析
  try {
    j["result"] = nlohmann::json::parse(result_data_);
  } catch (...) {
    j["result"] = result_data_;
  }
  return j;
}

nlohmann::json RpcError::ToJson() const {
  nlohmann::json j;
  j["id"] = GetSequenceId();
  j["error"]["code"] = error_code_;
  j["error"]["message"] = error_message_;
  return j;
}

nlohmann::json RpcError::ToJsonRpc() const {
  nlohmann::json j;
  j["jsonrpc"] = "2.0";
  j["id"] = GetSequenceId();
  j["error"]["code"] = error_code_;
  j["error"]["message"] = error_message_;
  return j;
}

}  // namespace mcp
