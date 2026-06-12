#pragma once

#include <string>

#include "mcp_server.h"

namespace mcp {

// 论文工具集：注册到 McpServer
//
// 1. extract_data_points  — 从 TXT 提取数据点（位置标注）
// 2. search_latest_data   — 搜索单个指标的最新值
// 3. update_paper_data    — 写入最新数据位置
//
void RegisterPaperTools(McpServer& mcp);

}  // namespace mcp
