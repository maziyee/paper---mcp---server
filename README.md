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

```
paper-mcp-server/
├── server.exe              # 主程序（静态链接，零 DLL 依赖）
├── config/
│   └── server_config.json  # 编辑此文件，填入视觉 API Key
├── tools/
│   └── vision_client.py    # 视觉分析脚本
├── papers/                 # 论文数据
└── logs/                   # 运行日志
```

**配置步骤：**

1. 编辑 `config/server_config.json`，将 `api_key` 改为你的 DashScope API Key
2. 在 VSCode 工作区根目录或用户目录创建 `.mcp.json`：

```json
{
  "mcpServers": {
    "paper-mcp": {
      "type": "stdio",
      "command": "C:\\path\\to\\paper-mcp-server\\server.exe",
      "args": ["--no-http"],
      "cwd": "C:\\path\\to\\paper-mcp-server"
    }
  }
}
```

> **前提**：系统需安装 Python 3（`python` 命令在 PATH 中可用），exe 本身无其他依赖。

3. 重启 Claude Code（`Ctrl+Shift+P` → `Reload Window`）

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

#### Linux / WSL

```bash
git clone https://github.com/maziyee/paper---mcp---server.git
cd paper---mcp---server
bash setup_linux.sh
./build/test_mcp
./build/server --no-http
```

---

## 配置说明

### 视觉 API

编辑 `config/server_config.json`：

```json
{
  "vision": {
    "base_url": "https://dashscope.aliyuncs.com/compatible-mode/v1",
    "model": "qwen3.7-plus",
    "api_key": "sk-your-api-key-here"
  }
}
```

| 字段 | 说明 |
|------|------|
| `base_url` | OpenAI 兼容 API 地址（DashScope / vLLM / Ollama 等） |
| `model` | 模型名（如 `qwen3.7-plus`、`gpt-4o`、`llava:13b`） |
| `api_key` | API Key（本地模型如 Ollama 可填 `not-needed`） |

> **注意**：API Key 仅从配置文件读取，不受环境变量覆盖。这是为了避免 MCP launcher 缓存残留旧 Key 导致 401 错误。

### 接入 Claude Code（Windows）

#### 方式一：项目级 `.mcp.json`（推荐）

在 VSCode 工作区根目录创建 `.mcp.json`：

```json
{
  "mcpServers": {
    "paper-mcp": {
      "type": "stdio",
      "command": "./utils/mcp_mt/build/server.exe",
      "args": ["--no-http"],
      "cwd": "./utils/mcp_mt"
    }
  }
}
```

> `cwd` 必须指向项目根目录 `mcp_mt/`，`server.exe` 会自动从 exe 位置推算 `vision_client.py` 路径。

#### 方式二：Claude Desktop

编辑 `%APPDATA%\Claude\claude_desktop_config.json`：

```json
{
  "mcpServers": {
    "paper-mcp": {
      "command": "J:\\path\\to\\mcp_mt\\build\\server.exe",
      "args": ["--no-http"],
      "cwd": "J:\\path\\to\\mcp_mt"
    }
  }
}
```

---

## 脚本路径自动寻路

`server.exe` 启动时自动查找 `vision_client.py`，依次尝试：

1. **源码布局**：`exe/../tools/scripts/vision_client.py`（`build/` → `tools/scripts/`）
2. **发布布局**：`exe/tools/vision_client.py`（同目录 `tools/`）
3. **回退**：`exe/vision_client.py`

无需手动配置脚本路径，两种布局开箱即用。

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
        "VISION_MODEL": "qwen3.7-plus"
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

---

## 故障排查

| 症状 | 原因 | 解决 |
|------|------|------|
| `Process exited with code 3221225781` | 缺少 DLL（`0xC0000135`） | 用 `x64-mingw-static` triplet 重新构建 |
| 视觉工具返回 `401 Incorrect API key` | API Key 未配置或错误 | 检查 `config/server_config.json` 中 `vision.api_key` |
| 视觉工具返回 `Connection refused` | 视觉 API 不可达 | 检查 `base_url`，确认服务已启动 |
| `python: can't open file` | 脚本路径推导失败 | 确认 `cwd` 设置正确，或检查 `FindScript()` 日志 |
| 工具调用后无反应 | Python 脚本执行异常 | 查看 `logs/rpc_*.log` 日志 |

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
│   ├── vision_tools.cpp       # 视觉工具注册（含 FindScript 自动寻路）
│   ├── vision_tools.h
│   └── scripts/               # Python 脚本
│       ├── vision_client.py   # 视觉 API 客户端（DashScope/Ollama 兼容）
│       ├── extract_data_points.py
│       ├── search_latest_data.py
│       ├── update_paper_data.py
│       └── cnki_search.py     # 知网搜索
├── test_tools/                # 测试
├── config/                    # 配置文件
├── logs/                      # 运行日志
├── papers/                    # 论文数据
├── release/                   # 发布包
│   └── paper-mcp-server/      # 可分发的独立包
│       ├── server.exe
│       ├── config/
│       ├── tools/
│       ├── papers/
│       └── logs/
├── scripts/                   # 辅助脚本（bridge 等）
├── setup_linux.sh             # Linux 一键构建
└── setup_windows.bat          # Windows 一键构建
```

## Windows 适配说明

| 适配项 | 说明 |
|------|------|
| `Quote()` — `""` 转义 | Windows `cmd.exe` 用 `""` 而非 `\"` 转义引号 |
| `--file` 临时文件传参 | 避免命令行 GBK 编码导致 JSON 解析失败 |
| `ensure_ascii=True` | Python 输出纯 ASCII，消除 GBK/UTF-8 编码冲突 |
| `2>&1` stderr 重定向 | 捕获 Python 错误信息到 MCP 响应 |
| `FindScript()` 自动寻路 | 运行时从 exe 位置推算脚本路径，支持源码/发布两种布局 |
| `_WIN32_WINNT=0x0A00` | 指定 Win10+ SDK 级别，cpp-httplib 需要 |
| `mcp_httplib_wrapper.h` | 绕过 MinGW 头文件缺少 `GetAddrInfoExCancel` |
| vcpkg triplet `x64-mingw-static` | 静态链接，产物零依赖 |

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

## 前置依赖

| 依赖 | 用途 | Windows 获取方式 |
|------|------|------------------|
| CMake >= 3.10 | 构建系统 | MSYS2: `pacman -S mingw-w64-x86_64-cmake` |
| C++17 编译器 (GCC) | 编译 | MSYS2: `pacman -S mingw-w64-x86_64-gcc` |
| Ninja | 构建生成器 | MSYS2: `pacman -S mingw-w64-x86_64-ninja` |
| vcpkg | C++ 包管理 | `git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg` |
| Python 3 | 工具脚本 | [python.org](https://www.python.org/downloads/) |

## License

MIT
