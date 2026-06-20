#include "vision_tools.h"

#include <array>
#include <cstdio>
#include <memory>
#include <string>

#include "log_manager.h"
#include "nlohmann/json.hpp"

namespace {

// ======== 工具函数 (与 paper_tools.cpp 一致的 popen + Quote 模式) ========

std::string Exec(const std::string& cmd) {
  std::string result;
  std::array<char, 4096> buf{};
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"),
                                                pclose);
  if (!pipe) return R"({"error":"popen failed"})";
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

// 获取脚本路径 (相对于可执行文件)
const char* kScriptPath = "tools/scripts/vision_client.py";

}  // namespace

namespace mcp {

void RegisterVisionTools(McpServer& mcp) {
  // ================================================================
  // 1. analyze_image — 单张图片分析
  // ================================================================
  {
    ToolInputSchema schema;
    schema.AddProperty("image_source", "string",
                       "图片来源: 本地文件路径或 HTTP(S) URL", true);
    schema.AddProperty("prompt", "string",
                       "分析提示词。默认为论文图表分析专用 prompt");
    schema.AddProperty("max_tokens", "integer",
                       "最大返回 token 数，默认 2000");
    schema.AddProperty("detail", "string",
                       "图片细节级别: low / high / auto，默认 auto");

    mcp.RegisterTool(
        "analyze_image",
        "使用视觉大模型分析单张图片。支持论文图表、实验图像、"
        "显微照片等。接受本地文件路径或 HTTP URL。",
        schema,
        [](const nlohmann::json& args) -> ToolResult {
          std::string source =
              args.value("image_source", "");
          std::string prompt = args.value("prompt", "");
          int max_tokens = args.value("max_tokens", 2000);
          std::string detail = args.value("detail", "auto");

          if (source.empty()) {
            return MakeErrorResult("image_source is required");
          }

          nlohmann::json payload;
          payload["image_source"] = source;
          if (!prompt.empty()) payload["prompt"] = prompt;
          payload["max_tokens"] = max_tokens;
          payload["detail"] = detail;

          std::string cmd = std::string("python ") + kScriptPath +
                            " analyze_image " + Quote(payload.dump());

          MCP_LOG_INFO("analyze_image: source={}", source);
          std::string out = Exec(cmd);
          return MakeTextResult(out);
        });
  }

  // ================================================================
  // 2. ocr_image — 图片文字提取
  // ================================================================
  {
    ToolInputSchema schema;
    schema.AddProperty("image_source", "string",
                       "图片来源: 本地文件路径或 HTTP(S) URL", true);
    schema.AddProperty("format", "string",
                       "输出格式: plain / markdown / json，默认 plain");
    schema.AddProperty("language", "string",
                       "目标语言提示，如 'zh,en'");
    schema.AddProperty("max_tokens", "integer",
                       "最大返回 token 数，默认 4000");

    mcp.RegisterTool(
        "ocr_image",
        "从图片中提取文字（OCR）。支持纯文本、Markdown、JSON 输出格式。"
        "适用于论文表格截图、图表标注、扫描页等场景。",
        schema,
        [](const nlohmann::json& args) -> ToolResult {
          std::string source = args.value("image_source", "");
          std::string format = args.value("format", "plain");
          std::string language = args.value("language", "");
          int max_tokens = args.value("max_tokens", 4000);

          if (source.empty()) {
            return MakeErrorResult("image_source is required");
          }

          nlohmann::json payload;
          payload["image_source"] = source;
          payload["format"] = format;
          if (!language.empty()) payload["language"] = language;
          payload["max_tokens"] = max_tokens;

          std::string cmd = std::string("python ") + kScriptPath +
                            " ocr_image " + Quote(payload.dump());

          MCP_LOG_INFO("ocr_image: source={} format={}", source, format);
          std::string out = Exec(cmd);
          return MakeTextResult(out);
        });
  }

  // ================================================================
  // 3. compare_images — 多图片对比
  // ================================================================
  {
    ToolInputSchema schema;
    schema.AddProperty("image_sources", "string",
                       "图片来源列表: 逗号分隔的 2-4 个文件路径或 URL", true);
    schema.AddProperty("prompt", "string",
                       "对比提示词。默认为通用图片对比 prompt");

    mcp.RegisterTool(
        "compare_images",
        "对比 2-4 张图片的差异和相似之处。适用于对比论文中不同实验条件的"
        "结果图、不同方法的输出对比、同一数据的不同可视化等场景。",
        schema,
        [](const nlohmann::json& args) -> ToolResult {
          std::string sources_str = args.value("image_sources", "");
          std::string prompt = args.value("prompt", "");

          if (sources_str.empty()) {
            return MakeErrorResult("image_sources is required");
          }

          // 解析逗号分隔的图片列表
          nlohmann::json sources_arr = nlohmann::json::array();
          size_t start = 0;
          while (start < sources_str.size()) {
            size_t end = sources_str.find(',', start);
            if (end == std::string::npos) end = sources_str.size();
            std::string s = sources_str.substr(start, end - start);
            // trim whitespace
            size_t a = 0, b = s.size();
            while (a < b && s[a] == ' ') ++a;
            while (b > a && s[b - 1] == ' ') --b;
            if (a < b) sources_arr.push_back(s.substr(a, b - a));
            start = end + 1;
          }

          if (sources_arr.size() < 2) {
            return MakeErrorResult("至少需要 2 张图片进行对比");
          }
          if (sources_arr.size() > 4) {
            return MakeErrorResult("最多支持 4 张图片");
          }

          nlohmann::json payload;
          payload["image_sources"] = sources_arr;
          if (!prompt.empty()) payload["prompt"] = prompt;
          payload["max_tokens"] = 4000;

          std::string cmd = std::string("python ") + kScriptPath +
                            " compare_images " + Quote(payload.dump());

          MCP_LOG_INFO("compare_images: count={}", sources_arr.size());
          std::string out = Exec(cmd);
          return MakeTextResult(out);
        });
  }

  // ================================================================
  // 4. analyze_video — 视频分析
  // ================================================================
  {
    ToolInputSchema schema;
    schema.AddProperty("video_source", "string",
                       "视频来源: 本地文件路径或 HTTP(S) URL", true);
    schema.AddProperty("prompt", "string",
                       "分析提示词。默认为视频内容描述 prompt");

    mcp.RegisterTool(
        "analyze_video",
        "分析视频内容。当前返回关键帧提取方案指引，"
        "建议使用 ffmpeg 提取帧后配合 analyze_image 使用。",
        schema,
        [](const nlohmann::json& args) -> ToolResult {
          std::string source = args.value("video_source", "");
          std::string prompt = args.value("prompt", "");

          if (source.empty()) {
            return MakeErrorResult("video_source is required");
          }

          nlohmann::json payload;
          payload["video_source"] = source;
          if (!prompt.empty()) payload["prompt"] = prompt;

          std::string cmd = std::string("python ") + kScriptPath +
                            " analyze_video " + Quote(payload.dump());

          MCP_LOG_INFO("analyze_video: source={}", source);
          std::string out = Exec(cmd);
          return MakeTextResult(out);
        });
  }
}

}  // namespace mcp
