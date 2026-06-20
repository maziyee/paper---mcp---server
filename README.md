# Paper MCP Server

基于 C++ 的 MCP（Model Context Protocol）服务端，提供视觉分析（图片/视频）、论文数据提取、OCR、最新数据搜索等功能。

## 架构

```
Claude Desktop / MCP 客户端
    │
    │  stdin/stdout 或 HTTP
    ▼
┌─────────────────────────────┐
│  McpServer (协议层)          │
│  ├─ tools/list              │  ← 自动发现工具
│  ├─ tools/call              │  ← 执行工具
│  ├─ resources/list          │  ← 列出资源
│  └─ resources/read          │  ← 读取资源
├─────────────────────────────┤
│  RpcManager (路由层)         │
├─────────────────────────────┤
│  HttpRpcServer / StdRpcServer│  ← 双传输
└─────────────────────────────┘
    │
    ▼
  Python 脚本 ────────────► 论文数据处理
  视觉大模型 API (DashScope) ► 图片/视频分析
```

## 已有工具

### 视觉分析

| 工具 | 功能 | 输入 |
|------|------|------|
| `analyze_image` | 使用视觉大模型分析图片内容（论文图表、实验图像等） | 本地路径 / URL |
| `ocr_image` | 从图片中提取文字，支持 plain/markdown/json 格式 | 本地路径 / URL |
| `compare_images` | 对比 2-4 张图片的差异和相似之处 | 多个路径 / URL |
| `analyze_video` | 分析视频内容，返回关键帧提取方案 | 本地路径 / URL |

### 论文数据处理

| 工具 | 功能 |
|------|------|
| `extract_data_points` | 从论文 TXT 提取时序数据点（数字+单位+位置） |
| `search_latest_data` | 对单个数据点搜索全网最新值 |
| `update_paper_data` | 将最新数据写入论文元数据 |

## 快速开始

### Linux / WSL

```bash
# 1. 克隆
git clone http://git.cpptrain.top/MT/mcp_mt.git
cd mcp_mt

# 2. 安装依赖 + 编译
bash setup_linux.sh

# 3. 测试
./build/test_mcp

# 4. 启动服务
./build/server --no-http
```

### Windows

```powershell
git clone http://git.cpptrain.top/MT/mcp_mt.git
cd mcp_mt
setup_windows.bat
```

## 部署到 Claude

### Claude Desktop（Windows）

编译完成后，编辑 Claude Desktop 配置文件（`%APPDATA%\Claude\claude_desktop_config.json`）：

```json
{
  "mcpServers": {
    "paper-mcp": {
      "command": "D:/projects/mcp_mt/build/Release/server.exe",
      "args": ["--no-http"],
      "env": {
        "VISION_BASE_URL": "https://dashscope.aliyuncs.com/compatible-mode/v1",
        "VISION_MODEL": "qwen3.7-plus",
        "VISION_API_KEY": "<your-api-key>"
      }
    }
  }
}
```

重启 Claude Desktop 即可使用。

### Claude Desktop（macOS / Linux）

编辑 `~/Library/Application Support/Claude/claude_desktop_config.json`：

```json
{
  "mcpServers": {
    "paper-mcp": {
      "command": "/path/to/mcp_mt/build/server",
      "args": ["--no-http"],
      "env": {
        "VISION_BASE_URL": "https://dashscope.aliyuncs.com/compatible-mode/v1",
        "VISION_MODEL": "qwen3.7-plus",
        "VISION_API_KEY": "<your-api-key>"
      }
    }
  }
}
```

### Claude Code VSCode 扩展（Windows 原生）

Windows 下 VSCode 的 PATH 环境是完整的，可直接使用 Node.js：

```json
{
  "mcpServers": {
    "paper-mcp": {
      "type": "stdio",
      "command": "node",
      "args": ["D:/projects/mcp_mt/scripts/mcp_bridge.js"],
      "cwd": "D:/projects/mcp_mt",
      "env": {
        "VISION_BASE_URL": "https://dashscope.aliyuncs.com/compatible-mode/v1",
        "VISION_MODEL": "qwen3.7-plus",
        "VISION_API_KEY": "<your-api-key>"
      }
    }
  }
}
```

> **Windows vs WSL 区别**：Windows 原生环境不需要 `mcp-node` 桥接脚本，可直接用 `node` 调用 `mcp_bridge.js`。`mcp-node` 仅为解决 WSL 下 PATH 不完整的问题。

## 前置依赖

| 依赖 | 用途 |
|------|------|
| CMake >= 3.10 | 构建系统 |
| C++17 编译器 (GCC/MSVC) | 编译 |
| vcpkg | C++ 包管理 |
| Python 3 | 工具脚本 |
| spdlog, nlohmann-json, httplib | 通过 vcpkg 安装 |

## 配置 Claude Code（VSCode 扩展 / WSL）

在项目根目录创建 `.mcp.json`：

```json
{
  "mcpServers": {
    "paper-mcp": {
      "type": "stdio",
      "command": "/home/you_dian/.local/bin/mcp-node",
      "args": ["/home/you_dian/MCP/mcp_mt/scripts/mcp_bridge.js"],
      "cwd": "/home/you_dian/MCP/mcp_mt",
      "env": {
        "VISION_BASE_URL": "https://dashscope.aliyuncs.com/compatible-mode/v1",
        "VISION_MODEL": "qwen3.7-plus",
        "VISION_API_KEY": "<your-api-key>"
      }
    }
  }
}
```

> **WSL 用户注意**：VSCode 扩展宿主机的 PATH 不包含 nvm 安装的 Node.js，会导致 `spawn ENOENT`。需要用 VSCode 自带的 Node.js 启动。`mcp-node` 脚本会自动发现 VSCode 的 Node.js 路径。

### 所需文件

| 文件 | 作用 |
|------|------|
| `.mcp.json` | MCP 服务器配置（放项目根目录） |
| `mcp_bridge.js` | Node.js 中继，启动 C++ server 并桥接管道 |
| `mcp-node` | 自动发现 VSCode Node.js 的 wrapper 脚本 |

### Bridge 架构（WSL 专用）

```
Claude Code (VSCode 扩展)
    │  spawn mcp-node → 自动找到 VSCode Node.js
    ▼
mcp_bridge.js (Node.js 中继)
    │  spawn server --no-http
    ▼
paper-mcp C++ Server (stdio 模式)
    │
    ▼
Python 脚本 ────────────► 论文数据处理
视觉大模型 API (DashScope) ► 图片/视频分析
```

VSCode 扩展的 MCP Gateway 在 WSL 下 spawn 进程时 PATH 不完整，直接用 C++ 二进制或系统 Python 都会报 `ENOENT`。通过 `mcp-node`（指向 VSCode 自带的 Node.js）可以绕过此限制。

### 传输协议兼容

C++ 服务器支持两种 MCP 传输格式：

| 格式 | 请求 | 响应 | 使用者 |
|------|------|------|--------|
| Content-Length 帧 | `Content-Length: N\r\n\r\n{json}` | 同格式 | 标准 MCP 客户端、Ollama |
| 原始 JSON 行 | `{"method":"...",...}\n` | 同格式 | Claude Code VSCode 扩展 |

服务器自动检测请求格式并用相同格式回复。

## 用 Ollama 本地模型测试

```bash
# 1. 启动 Ollama
ollama serve

# 2. 运行 demo（使用 qwen2.5:7b）
python3 scripts/ollama_mcp_demo.py
```

交互示例：
```
👤 你: 论文 test_001 中有哪些时序数据？
🤖 AI 思考中... (6.3s)
🔧 调用工具: extract_data_points
   参数: {"paper_id": "test_001"}
   ✅ 提取 34 个数据点
💬 回复: 2023年集成电路产量4200亿片，2024年4510亿片...
```

## 项目结构

```
mcp_mt/
├── include/           # 头文件
│   ├── mcp_types.h    # 类型定义（ContentItem, ToolResult, ToolInputSchema）
│   ├── mcp_server.h   # MCP 协议层
│   ├── rpc_manager.h  # RPC 路由
│   ├── my_rpc.h       # RPC 基类
│   ├── http_rpc.h     # HTTP 传输
│   └── std_handle_rpc.h # stdio 传输
├── mpc_src/
│   ├── common/        # 公共实现
│   └── server/        # 服务端入口
├── tools/
│   ├── paper_tools.cpp   # 论文工具注册
│   ├── vision_tools.cpp  # 视觉工具注册（图片分析/OCR/视频）
│   ├── vision_tools.h    # 视觉工具头文件
│   └── scripts/          # Python 脚本
├── test_tools/        # 测试
├── config/            # 配置文件
└── papers/            # 论文数据
```

## 命令行参数

```bash
./build/server [选项]
  --config <path>     服务器配置文件 (默认 config/server_config.json)
  --log-config <path> 日志配置文件 (默认 config/log_config.json)
  --host <ip>         HTTP 监听地址 (默认 0.0.0.0)
  --port <port>       HTTP 监听端口 (默认 8080)
  --no-http           禁用 HTTP 传输
  --no-stdio          禁用 stdio 传输
```

## License

MIT
