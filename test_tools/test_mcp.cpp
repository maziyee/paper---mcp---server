#include <cassert>
#include <iostream>
#include <string>

#include "mcp_server.h"
#include "rpc_manager.h"

using namespace mcp;

namespace {

// 发送 RPC 请求并返回解析后的 JSON 结果
struct RpcResult {
  bool is_error = false;
  nlohmann::json data;
};

RpcResult CallRpc(RpcManager& manager, const std::string& service,
                  const std::string& method, const nlohmann::json& payload) {
  // 1. 构造 JSON-RPC 请求
  nlohmann::json req_json;
  req_json["service_name"] = service;
  req_json["method_name"] = method;
  req_json["payload"] = payload.dump();
  req_json["id"] = 1;

  RpcRequest req = RpcRequest::FromJson(req_json);
  assert(req.IsValid());

  // 2. 执行
  std::string raw = manager.HandleRequest(req);

  // 3. 解析响应
  RpcResponse resp;
  if (resp.Deserializer(raw)) {
    return {false, nlohmann::json::parse(resp.GetResultData())};
  }

  RpcError err;
  if (err.Deserializer(raw)) {
    return {true,
            {{"code", err.GetErrorCode()}, {"message", err.GetErrorMessage()}}};
  }

  return {true, {{"message", "failed to parse response"}}};
}

// ======== 测试 1：tools/list ========
void TestToolsList(RpcManager& manager) {
  std::cout << "=== Test tools/list ===" << std::endl;

  auto result = CallRpc(manager, "mcp", "tools/list", {});
  assert(!result.is_error);

  assert(result.data.contains("tools"));
  auto& tools = result.data["tools"];
  assert(tools.is_array());
  assert(tools.size() == 3);

  std::cout << "  Tools count: " << tools.size() << std::endl;
  for (const auto& t : tools) {
    std::cout << "    - " << t["name"] << ": " << t["description"] << std::endl;
  }
  std::cout << "  PASSED" << std::endl;
}

// ======== 测试 2：tools/call echo ========
void TestEcho(RpcManager& manager) {
  std::cout << "=== Test tools/call echo ===" << std::endl;

  nlohmann::json args;
  args["message"] = "Hello MCP!";

  nlohmann::json params;
  params["name"] = "echo";
  params["arguments"] = args;

  auto result = CallRpc(manager, "mcp", "tools/call", params);
  assert(!result.is_error);

  std::cout << "  Result: " << result.data.dump() << std::endl;
  assert(result.data.contains("content"));
  auto& content = result.data["content"];
  assert(content.is_array() && content.size() == 1);
  assert(content[0]["type"] == "text");
  assert(content[0]["text"] == "Echo: Hello MCP!");

  std::cout << "  PASSED" << std::endl;
}

// ======== 测试 3：tools/call add ========
void TestAdd(RpcManager& manager) {
  std::cout << "=== Test tools/call add ===" << std::endl;

  nlohmann::json args;
  args["a"] = 123;
  args["b"] = 456;

  nlohmann::json params;
  params["name"] = "add";
  params["arguments"] = args;

  auto result = CallRpc(manager, "mcp", "tools/call", params);
  assert(!result.is_error);

  std::cout << "  Result: " << result.data.dump() << std::endl;
  auto& content = result.data["content"];
  assert(content[0]["text"] == "579");

  // 再测一组
  args["a"] = -10;
  args["b"] = 100;
  params["arguments"] = args;
  result = CallRpc(manager, "mcp", "tools/call", params);
  std::cout << "  Result2: " << result.data.dump() << std::endl;
  assert(result.data["content"][0]["text"] == "90");

  std::cout << "  PASSED" << std::endl;
}

// ======== 测试 4：调用不存在的工具 ========
void TestUnknownTool(RpcManager& manager) {
  std::cout << "=== Test unknown tool ===" << std::endl;

  nlohmann::json params;
  params["name"] = "nonexistent";
  params["arguments"] = nlohmann::json::object();

  auto result = CallRpc(manager, "mcp", "tools/call", params);

  // 应该返回 isError
  std::cout << "  Result: " << result.data.dump() << std::endl;
  assert(result.data.contains("isError"));
  assert(result.data["isError"] == true);

  std::cout << "  PASSED" << std::endl;
}

// ======== 测试 5：initialize ========
void TestInitialize(RpcManager& manager) {
  std::cout << "=== Test initialize ===" << std::endl;

  nlohmann::json params;
  params["protocolVersion"] = "0.1.0";
  params["clientInfo"]["name"] = "test_client";
  params["clientInfo"]["version"] = "1.0";

  auto result = CallRpc(manager, "mcp", "initialize", params);
  assert(!result.is_error);

  std::cout << "  Result: " << result.data.dump() << std::endl;
  assert(result.data["serverInfo"]["name"] == "test_server");
  assert(result.data["serverInfo"]["version"] == "1.0");
  assert(result.data["capabilities"].contains("tools"));

  std::cout << "  PASSED" << std::endl;
}

// ======== 测试 6：ToolInputSchema 序列化 ========
void TestSchemaSerialization() {
  std::cout << "=== Test ToolInputSchema ===" << std::endl;

  ToolInputSchema schema;
  schema.AddProperty("name", "string", "用户姓名", true);
  schema.AddProperty("age", "integer");
  schema.AddProperty("tags", "array", "标签列表").SetItems("string");
  schema.SetEnum({"red", "green", "blue"});

  // ToJson
  nlohmann::json j = schema.ToJson();
  std::cout << "  Schema: " << j.dump() << std::endl;
  assert(j["type"] == "object");
  assert(j["properties"]["name"]["type"] == "string");
  assert(j["properties"]["name"]["description"] == "用户姓名");
  assert(j["properties"]["age"]["type"] == "integer");
  assert(j["properties"]["tags"]["items"]["type"] == "string");
  assert(j["properties"]["tags"]["enum"].size() == 3);
  assert(j["required"][0] == "name");

  // FromJson 往返
  auto restored = ToolInputSchema::FromJson(j);
  assert(restored.GetType() == "object");
  assert(restored.GetRequired().size() == 1);
  assert(restored.GetRequired()[0] == "name");

  nlohmann::json j2 = restored.ToJson();
  assert(j2.dump() == j.dump());

  std::cout << "  PASSED (roundtrip OK)" << std::endl;
}

}  // namespace

int main() {
  std::cout << "========================================" << std::endl;
  std::cout << " MCP Server Test Suite" << std::endl;
  std::cout << "========================================" << std::endl;

  // ======== 组装服务端 ========

  RpcManager manager;
  McpServer mcp("test_server", "1.0");

  // 注册 ech 工具
  {
    ToolInputSchema schema;
    schema.AddProperty("message", "string", "要回显的消息内容", true);
    mcp.RegisterTool(
        "echo", "回显输入的消息", schema,
        [](const nlohmann::json& args) -> ToolResult {
          return MakeTextResult("Echo: " + args["message"].get<std::string>());
        });
  }

  // 注册 add 工具
  {
    ToolInputSchema schema;
    schema.AddProperty("a", "integer", "第一个加数", true);
    schema.AddProperty("b", "integer", "第二个加数", true);
    mcp.RegisterTool("add", "返回 a + b 的结果", schema,
                     [](const nlohmann::json& args) -> ToolResult {
                       int a = args["a"].get<int>();
                       int b = args["b"].get<int>();
                       return MakeTextResult(std::to_string(a + b));
                     });
  }

  // 注册 greet 工具（无参数，仅测试无 required 场景）
  {
    ToolInputSchema schema;
    schema.AddProperty("lang", "string", "语言：zh/en", false)
        .SetDefault("zh")
        .SetEnum({"zh", "en"});
    mcp.RegisterTool("greet", "问候", schema,
                     [](const nlohmann::json& args) -> ToolResult {
                       std::string lang = args.value("lang", "zh");
                       if (lang == "en") return MakeTextResult("Hello!");
                       return MakeTextResult("你好！");
                     });
  }

  mcp.InstallTo(manager);

  // ======== 运行测试 ========

  std::cout << std::endl;

  TestSchemaSerialization();
  std::cout << std::endl;

  TestInitialize(manager);
  std::cout << std::endl;

  TestToolsList(manager);
  std::cout << std::endl;

  TestEcho(manager);
  std::cout << std::endl;

  TestAdd(manager);
  std::cout << std::endl;

  TestUnknownTool(manager);
  std::cout << std::endl;

  std::cout << "========================================" << std::endl;
  std::cout << " All tests passed!" << std::endl;
  std::cout << "========================================" << std::endl;
  return 0;
}
