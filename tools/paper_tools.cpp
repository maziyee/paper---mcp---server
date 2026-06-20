#include "paper_tools.h"

#include <cstdio>
#include <memory>
#include <sstream>

#include "log_manager.h"

namespace mcp {

namespace {

std::string Exec(const std::string& cmd) {
  std::string result;
  std::array<char, 4096> buf{};
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"),
                                                pclose);
  if (!pipe) {
    return R"({"error":"popen failed"})";
  }
  while (fgets(buf.data(), buf.size(), pipe.get()) != nullptr) {
    result += buf.data();
  }
  return result;
}

std::string Quote(const std::string& s) {
  std::string out = "\"";
  for (char c : s) {
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  out += '"';
  return out;
}

}  // namespace

void RegisterPaperTools(McpServer& mcp) {
  // ======== 工具 1：提取数据点 ========
  {
    ToolInputSchema schema;
    schema.AddProperty("paper_id", "string",
                       "论文 ID/文件名（不含扩展名）", true);
    mcp.RegisterTool(
        "extract_data_points",
        "从论文 TXT 文件中提取所有时序数据点（数字+单位），返回位置标注",
        schema,
        [](const nlohmann::json& args) -> ToolResult {
          std::string paper_id = args["paper_id"];
          std::string cmd = "python tools/scripts/extract_data_points.py "
                          + Quote("papers/" + paper_id + ".txt");
          MCP_LOG_INFO("extract_data_points: paper_id={}", paper_id);
          std::string out = Exec(cmd);
          return MakeTextResult(out);
        });
  }

  // ======== 工具 2：搜索最新数据 ========
  {
    ToolInputSchema schema;
    schema.AddProperty("metric", "string", "指标名称或原始文本", true);
    schema.AddProperty("unit", "string", "单位，如 万片、亿美元、%");
    schema.AddProperty("year", "integer", "原数据年份，如 2023");
    schema.AddProperty("context", "string", "上下文文本（前后各80字符）");
    mcp.RegisterTool(
        "search_latest_data",
        "对提取出的单个数据点搜索全网最新值",
        schema,
        [](const nlohmann::json& args) -> ToolResult {
          std::string metric = args.value("metric", "");
          std::string unit = args.value("unit", "");
          int year = args.value("year", 2020);
          std::string context = args.value("context", "");

          nlohmann::json input;
          input["metric"] = metric;
          input["unit"] = unit;
          input["year"] = year;
          input["context"] = context;

          std::string cmd =
              "python tools/scripts/search_latest_data.py " + Quote(input.dump());
          MCP_LOG_INFO("search_latest_data: metric={}", metric);
          std::string out = Exec(cmd);
          return MakeTextResult(out);
        });
  }

  // ======== 工具 3：写入更新位置 ========
  {
    ToolInputSchema schema;
    schema.AddProperty("paper_id", "string", "论文 ID/文件名", true);

    // 复杂嵌套类型：array of object — 用原始 JSON schema
    nlohmann::json updates_prop;
    updates_prop["type"] = "array";
    updates_prop["description"] =
        "更新列表 [{\"point_id\":1,\"latest_value\":\"4200\","
        "\"latest_year\":2026,\"source\":\"...\"}]";
    nlohmann::json& items = updates_prop["items"];
    items["type"] = "object";
    items["properties"]["point_id"] = {{"type", "integer"}};
    items["properties"]["latest_value"] = {{"type", "string"}};
    items["properties"]["latest_year"] = {{"type", "integer"}};
    items["properties"]["source"] = {{"type", "string"}};
    items["properties"]["source_url"] = {{"type", "string"}};
    schema.AddRawProperty("updates", updates_prop, true);

    mcp.RegisterTool(
        "update_paper_data",
        "将最新数据写入论文元数据，标注更改位置（第几段第几行）",
        schema,
        [](const nlohmann::json& args) -> ToolResult {
          std::string paper_id = args["paper_id"];
          std::string updates_str = args["updates"].dump();

          std::string cmd = "python tools/scripts/update_paper_data.py "
                          + Quote(paper_id) + " " + Quote(updates_str);
          MCP_LOG_INFO("update_paper_data: paper_id={}, updates={} items",
                       paper_id, args["updates"].size());
          std::string out = Exec(cmd);
          return MakeTextResult(out);
        });
  }
}

}  // namespace mcp
