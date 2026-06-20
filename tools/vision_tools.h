#pragma once

#include "mcp_server.h"

namespace mcp {

// Vision 工具集：注册到 McpServer
//
// 通过 OpenAI 兼容接口调用多模态视觉大模型。
// 配置: VISION_BASE_URL / VISION_MODEL / VISION_API_KEY 环境变量
//       或 config/server_config.json 中的 "vision" 段
//
// 1. analyze_image   — 分析单张图片（图表、实验图等）
// 2. ocr_image       — 提取图片中文字（支持 plain/markdown/json 格式）
// 3. compare_images  — 对比 2-4 张图片
// 4. analyze_video   — 视频分析引导说明
//
void RegisterVisionTools(McpServer& mcp);

}  // namespace mcp
