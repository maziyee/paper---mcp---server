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

### 直接下载使用（Windows，无需编译）

从 [GitHub Releases](../../releases) 下载 `paper-mcp-server-windows-x64.zip`，解压到任意目录：

```powershell
# 1. 解压
unzip paper-mcp-server-windows-x64.zip -d C:\paper-mcp-server

# 2. 接入 Claude Code（全局）
claude mcp add -s user paper-mcp "C:\paper-mcp-server\server.exe"
```

然后编辑 `C:\Users\<用户名>\.claude.json`，补上 `args` / `cwd` / 视觉 API 环境变量（见下方 [接入 Claude](#接入-claudewindows) 章节）。

> **前提**：系统需安装 Python 3（`python` 命令可用），exe 本身无其他依赖。

### 从源码构建

#### Windows

**前置依赖**

| 依赖 | 用途 | 安装方式 |
|------|------|----------|
| MSYS2 (MinGW-w64) | C++17 编译器 | [msys2.org](https://www.msys2.org/) → `pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja` |
| vcpkg | C++ 包管理 | `git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg && C:\vcpkg\bootstrap-vcpkg.bat` |
| Python 3 | 工具脚本 | [python.org](https://www.python.org/downloads/) 安装并添加到 PATH |

**一键构建**

```powershell
git clone https://github.com/maziyee/paper---mcp---server.git
cd paper---mcp---server
setup_windows.bat
```

构建产物 `build\server.exe`（约 6.5 MB）**静态链接，零 DLL 依赖**，可直接拷贝运行。

> `server.exe` 内部通过 `popen("python ...")` 调用 Python 脚本，所以 Python 3 仍需在 PATH 中。

**手动构建**

```powershell
C:\vcpkg\vcpkg install spdlog nlohmann-json cpp-httplib --triplet x64-mingw-static
mkdir build && cd build
cmake .. -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_TOOLCHAIN_FILE="C:\vcpkg\scripts\buildsystems\vcpkg.cmake" ^
  -DVCPKG_TARGET_TRIPLET=x64-mingw-static ^
  -DCMAKE_EXE_LINKER_FLAGS="-static-libstdc++ -static-libgcc -static"
cmake --build .
```

**Windows 适配说明**

| 适配项 | 说明 |
|------|------|
| `python3` → `python` | Windows 上 Python 命令为 `python`，非 `python3` |
| `_WIN32_WINNT=0x0A00` | 指定 Win10+ SDK 级别，cpp-httplib 需要 |
| `mcp_httplib_wrapper.h` | 绕过 MinGW 头文件缺少 `GetAddrInfoExCancel` |
| vcpkg triplet | `x64-mingw-static` 静态链接，产物零依赖 |
| `CMAKE_BUILD_TYPE=Release` | 避免链接 debug 版 DLL |

#### Linux / WSL

```bash
git clone https://github.com/maziyee/paper---mcp---server.git
cd paper---mcp---server
bash setup_linux.sh
./build/test_mcp
./build/server --no-http
```

---

## 接入 Claude（Windows）

### 方式一：Claude Code CLI（推荐，全局生效）

```powershell
claude mcp add -s user paper-mcp "J:\path\to\mcp_mt\build\server.exe"
```

然后编辑 `C:\Users\<用户名>\.claude.json`，找到 `mcpServers.paper-mcp`，补全为：

```json
{
  "type": "stdio",
  "command": "J:\\path\\to\\mcp_mt\\build\\server.exe",
  "args": ["--no-http"],
  "cwd": "J:\\path\\to\\mcp_mt",
  "env": {
    "VISION_BASE_URL": "https://dashscope.aliyuncs.com/compatible-mode/v1",
    "VISION_MODEL": "qwen3.7-plus",
    "VISION_API_KEY": "<你的 DashScope API Key>"
  }
}
```

重新加载 VSCode（`Ctrl+Shift+P` → `Developer: Reload Window`）。

> `cwd` 必须设为项目根目录，server.exe 用相对路径调用 `tools/scripts/*.py`。

### 方式二：项目级 .mcp.json

在 VSCode 工作区根目录创建 `.mcp.json`：

```json
{
  "mcpServers": {
    "paper-mcp": {
      "type": "stdio",
      "command": "./utils/mcp_mt/build/server.exe",
      "args": ["--no-http"],
      "cwd": "j:/projects/paper-mcp-server/utils/mcp_mt",
      "env": {
        "VISION_BASE_URL": "https://dashscope.aliyuncs.com/compatible-mode/v1",
        "VISION_MODEL": "qwen3.7-plus",
        "VISION_API_KEY": "<你的 DashScope API Key>"
      }
    }
  }
}
```

### 方式三：Claude Desktop

编辑 `%APPDATA%\Claude\claude_desktop_config.json`：

```json
{
  "mcpServers": {
    "paper-mcp": {
      "command": "J:\\path\\to\\mcp_mt\\build\\server.exe",
      "args": ["--no-http"],
      "env": {
        "VISION_BASE_URL": "https://dashscope.aliyuncs.com/compatible-mode/v1",
        "VISION_MODEL": "qwen3.7-plus",
        "VISION_API_KEY": "<你的 DashScope API Key>"
      }
    }
  }
}
```

重启 Claude Desktop。

---

## 接入 Claude（WSL / Linux）

### Claude Code VSCode 扩展（WSL）

WSL 下 VSCode 扩展的 PATH 不包含 nvm 安装的 Node.js，需用 `mcp-node` + `mcp_bridge.js` 桥接：

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

**Bridge 架构（WSL 专用）**

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
Python 脚本 / 视觉大模型 API
```

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

---

## 故障排查

| 症状 | 原因 | 解决 |
|------|------|------|
| `Process exited with code 3221225781` | 缺少 DLL（`0xC0000135`） | 用 `x64-mingw-static` triplet 重新构建 |
| 视觉工具返回空 | `python3` 找不到 | 确认源码中已改为 `python`（Windows 适配） |
| 视觉工具返回 "Connection refused" | 环境变量未传递 | 检查 `env` 字段是否正确；确认 `cwd` 已设置 |
| 工具调用后无反应 | Python 脚本路径错误 | 确认 `cwd` 指向项目根目录 |

## 传输协议兼容

C++ 服务器支持两种 MCP 传输格式：

| 格式 | 请求 | 响应 | 使用者 |
|------|------|------|--------|
| Content-Length 帧 | `Content-Length: N\r\n\r\n{json}` | 同格式 | 标准 MCP 客户端、Ollama |
| 原始 JSON 行 | `{"method":"...",...}\n` | 同格式 | Claude Code VSCode 扩展 |

服务器自动检测请求格式并用相同格式回复。

## 用 Ollama 本地模型测试

```bash
ollama serve
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
├── include/                   # 头文件
│   ├── mcp_types.h            # 类型定义
│   ├── mcp_server.h           # MCP 协议层
│   ├── rpc_manager.h          # RPC 路由
│   ├── my_rpc.h / http_rpc.h / std_handle_rpc.h
│   └── mcp_httplib_wrapper.h  # MinGW 兼容 wrapper
├── mpc_src/
│   ├── common/                # 公共实现
│   └── server/                # 服务端入口
├── tools/
│   ├── paper_tools.cpp        # 论文工具注册
│   ├── vision_tools.cpp       # 视觉工具注册
│   ├── vision_tools.h
│   └── scripts/               # Python 脚本
│       ├── vision_client.py   # 视觉 API 客户端（DashScope）
│       ├── extract_data_points.py
│       ├── search_latest_data.py
│       ├── update_paper_data.py
│       └── cnki_search.py     # 知网搜索
├── test_tools/                # 测试
├── config/                    # 配置文件
├── logs/                      # 运行日志
├── papers/                    # 论文数据
├── scripts/                   # 辅助脚本（bridge 等）
├── setup_linux.sh             # Linux 一键构建
└── setup_windows.bat          # Windows 一键构建
```

## 前置依赖

| 依赖 | 用途 | Windows 获取方式 |
|------|------|------------------|
| CMake >= 3.10 | 构建系统 | MSYS2: `pacman -S mingw-w64-x86_64-cmake` |
| C++17 编译器 (GCC) | 编译 | MSYS2: `pacman -S mingw-w64-x86_64-gcc` |
| Ninja | 构建生成器 | MSYS2: `pacman -S mingw-w64-x86_64-ninja` |
| vcpkg | C++ 包管理 | `git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg` |
| Python 3 | 工具脚本 | [python.org](https://www.python.org/downloads/) |

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
