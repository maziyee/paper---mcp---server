#include "std_handle_rpc.h"

#include "log_manager.h"

namespace mcp {

StdRpcServer::StdRpcServer(RpcManager& rpc_manager, std::ostream& out,
                           std::istream& in)
    : rpc_manager_(rpc_manager), out_(out), in_(in) {}

bool StdRpcServer::ReadMessage(std::string& out_body) {
  // 读 Content-Length header，直到空行（大小写不敏感）
  std::string line;
  int content_length = -1;
  const std::string kHeader = "content-length:";
  while (std::getline(in_, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty()) break;

    // 转小写后比较
    std::string lower = line;
    for (char& c : lower) c = static_cast<char>(std::tolower(c));
    if (lower.compare(0, kHeader.size(), kHeader) == 0) {
      // 找 ":" 位置，跳过空白
      auto colon = lower.find(':');
      if (colon == std::string::npos) continue;
      size_t start = colon + 1;
      while (start < line.size() && line[start] == ' ') ++start;
      content_length = std::stoi(line.substr(start));
    }
  }

  if (content_length <= 0) return false;

  // 循环读 body，避免一次 read 读不全
  std::string body(content_length, '\0');
  size_t total = 0;
  while (total < static_cast<size_t>(content_length)) {
    in_.read(body.data() + total, content_length - total);
    std::streamsize n = in_.gcount();
    if (n <= 0) return false;
    total += static_cast<size_t>(n);
  }
  if (total != static_cast<size_t>(content_length)) {
    MCP_LOG_ERROR("ReadMessage: total != content_length");
    return false;
  }

  out_body = std::move(body);
  return true;
}

static void WriteFrame(std::ostream& out, const std::string& body) {
  out << "Content-Length: " << body.size() << "\r\n\r\n";
  out.write(body.data(), body.size());
  out.flush();
}

bool StdRpcServer::WriteMessage(const RpcResponse& resp) {
  WriteFrame(out_, resp.ToJson().dump());
  return out_.good();
}

bool StdRpcServer::WriteMessage(const RpcError& err) {
  WriteFrame(out_, err.ToJson().dump());
  return out_.good();
}

void StdRpcServer::Run() {
  std::string body;
  while (ReadMessage(body)) {
    try {
      auto request = nlohmann::json::parse(body);
      RpcRequest req = RpcRequest::FromJson(request);

      if (!req.IsValid()) {
        RpcError err;
        err.SetSequenceId(req.GetSequenceId());
        err.SetErrorCode(errc::InvalidRequest);
        err.SetErrorMessage("missing service_name or method_name");
        WriteMessage(err);
        continue;
      }

      std::string raw_resp = rpc_manager_.HandleRequest(req);

      RpcResponse resp;
      if (resp.Deserializer(raw_resp)) {
        WriteMessage(resp);
        continue;
      }

      RpcError err;
      if (err.Deserializer(raw_resp)) {
        WriteMessage(err);
        continue;
      }

      RpcError fallback;
      fallback.SetErrorCode(errc::InternalError);
      fallback.SetErrorMessage("failed to parse binary response");
      WriteMessage(fallback);
    } catch (const std::exception& e) {
      RpcError err;
      err.SetErrorCode(errc::ParseError);
      err.SetErrorMessage(std::string("json parse error: ") + e.what());
      WriteMessage(err);
    }
  }
}

}  // namespace mcp
