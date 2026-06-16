# Paper MCP Server

基于 C++ 的 MCP（Model Context Protocol）论文数据处理服务端，提供论文数据提取、最新数据搜索、数据更新等功能。

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
  Python 脚本 → 论文处理
```

## 已有工具

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

## 前置依赖

| 依赖 | 用途 |
|------|------|
| CMake >= 3.10 | 构建系统 |
| C++17 编译器 (GCC/MSVC) | 编译 |
| vcpkg | C++ 包管理 |
| Python 3 | 工具脚本 |
| spdlog, nlohmann-json, httplib | 通过 vcpkg 安装 |

## 配置 Claude Desktop

编辑 Claude Desktop 的 `claude_desktop_config.json`：

```json
{
  "mcpServers": {
    "paper-mcp": {
      "command": "/path/to/mcp_mt/build/server",
      "args": ["--no-http"]
    }
  }
}
```

重启 Claude Desktop，然后对话：

> 帮我分析 papers/test_001.txt 这篇论文，提取所有数据点

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
│   ├── paper_tools.cpp # 工具注册
│   └── scripts/       # Python 脚本
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
