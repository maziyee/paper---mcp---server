#!/usr/bin/env python3
"""
使用本地 Ollama + paper-mcp (stdio) 的完整示例
"""
import subprocess, json, sys, requests, time, threading, os

SERVER_PATH = "/home/you_dian/MCP/mcp_mt/build/server"
PAPERS_DIR = "/home/you_dian/MCP/mcp_mt/papers"


class StdioMcpClient:
    """通过 stdio 子进程与 MCP 服务器通信"""

    def __init__(self, command, args=None, cwd=None):
        self.proc = subprocess.Popen(
            [command] + (args or []),
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            cwd=cwd,
        )
        self._id = 0
        self._lock = threading.Lock()
        self._init()

    def _init(self):
        """MCP 握手: initialize → initialized"""
        self._send({
            "jsonrpc": "2.0", "id": self._next_id(), "method": "initialize",
            "params": {"protocolVersion": "2024-11-05", "capabilities": {},
                       "clientInfo": {"name": "ollama-demo", "version": "1.0"}}
        })
        # initialized 是通知，不需要响应
        self._send_raw({
            "jsonrpc": "2.0", "method": "notifications/initialized", "params": {}
        })

    def _next_id(self):
        self._id += 1
        return self._id

    def _read_message(self):
        """从 stdout 读一个 Content-Length 帧"""
        header = b""
        while not header.endswith(b"\r\n\r\n"):
            ch = self.proc.stdout.read(1)
            if not ch:
                raise EOFError("Server stdout closed")
            header += ch
        cl = None
        for line in header.decode("latin-1").split("\r\n"):
            if line.lower().startswith("content-length:"):
                cl = int(line.split(":")[1].strip())
        if cl is None:
            raise ValueError(f"Bad header: {header}")
        body = self.proc.stdout.read(cl)
        return json.loads(body.decode("utf-8"))

    def _send_raw(self, msg):
        body = json.dumps(msg)
        frame = f"Content-Length: {len(body)}\r\n\r\n{body}"
        self.proc.stdin.write(frame.encode())
        self.proc.stdin.flush()

    def _send(self, msg):
        """发送请求并读取响应"""
        with self._lock:
            self._send_raw(msg)
            if "id" not in msg:
                return None  # notification, no response
            return self._read_message()

    def list_tools(self):
        resp = self._send({
            "jsonrpc": "2.0", "id": self._next_id(),
            "method": "tools/list", "params": {}
        })
        return resp.get("result", {}).get("tools", [])

    def call_tool(self, name, arguments):
        resp = self._send({
            "jsonrpc": "2.0", "id": self._next_id(),
            "method": "tools/call",
            "params": {"name": name, "arguments": arguments}
        })
        result = resp.get("result", {})
        content = result.get("content", [])
        if content:
            return content[0].get("text", "")
        return json.dumps(result)

    def close(self):
        try:
            self.proc.stdin.close()
        except Exception:
            pass
        self.proc.terminate()
        self.proc.wait(timeout=3)


def mcp_tools_to_ollama(tools):
    """将 MCP 工具格式转为 Ollama function calling 格式"""
    result = []
    for tool in tools:
        schema = tool.get("inputSchema", {})
        result.append({
            "type": "function",
            "function": {
                "name": tool["name"],
                "description": tool.get("description", ""),
                "parameters": {
                    "type": "object",
                    "properties": schema.get("properties", {}),
                    "required": schema.get("required", [])
                }
            }
        })
    return result


def main():
    # 1. 启动 MCP 服务器
    print("=" * 60)
    print("启动 paper-mcp 服务器 (stdio 模式)...")
    mcp = StdioMcpClient(SERVER_PATH, ["--no-http"], cwd=PAPERS_DIR.replace("/papers", ""))
    print("✅ MCP 服务器已连接")
    print()

    # 2. 获取工具列表
    tools = mcp.list_tools()
    print(f"📋 获取到 {len(tools)} 个工具:")
    for t in tools:
        props = list(t.get("inputSchema", {}).get("properties", {}).keys())
        print(f"   - {t['name']}: {t['description'][:60]}")
        print(f"     参数: {props}")
    print()

    # 3. 初始化 Ollama
    MODEL = "qwen2.5:7b"
    OLLAMA_URL = "http://localhost:11434"

    print(f"🤖 Ollama 模型: {MODEL}")
    ollama_tools = mcp_tools_to_ollama(tools)
    messages = []
    print("=" * 60)
    print("开始对话 (输入 'quit' 退出, 'tools' 查看工具)")
    print()

    while True:
        try:
            user_input = input("👤 你: ").strip()
        except (EOFError, KeyboardInterrupt):
            break

        if not user_input:
            continue
        if user_input.lower() in ("quit", "exit", "q"):
            break
        if user_input.lower() == "tools":
            for t in tools:
                print(f"   - {t['name']}: {t['description']}")
            print()
            continue

        messages.append({"role": "user", "content": user_input})

        # 调用 Ollama
        print("⏳ AI 思考中...", end=" ", flush=True)
        t0 = time.time()
        resp = requests.post(f"{OLLAMA_URL}/api/chat", json={
            "model": MODEL,
            "messages": messages,
            "tools": ollama_tools,
            "stream": False,
        })
        msg = resp.json().get("message", {})
        print(f"({time.time()-t0:.1f}s)")

        tool_calls = msg.get("tool_calls")
        if tool_calls:
            # Ollama 请求调用工具
            tc = tool_calls[0]
            fn_name = tc["function"]["name"]
            fn_args = tc["function"]["arguments"]
            print(f"🔧 调用工具: {fn_name}")
            print(f"   参数: {json.dumps(fn_args, ensure_ascii=False)[:200]}")

            try:
                result_text = mcp.call_tool(fn_name, fn_args)
                print(f"   ✅ 结果: {result_text[:300]}...")
            except Exception as e:
                result_text = f"工具调用失败: {e}"
                print(f"   ❌ {result_text}")

            # 将工具结果反馈给 AI
            messages.append(msg)  # assistant 的 tool_call
            messages.append({"role": "tool", "content": result_text})

            # 获取最终回复
            print("⏳ 生成回复...", end=" ", flush=True)
            t1 = time.time()
            final = requests.post(f"{OLLAMA_URL}/api/chat", json={
                "model": MODEL,
                "messages": messages,
                "stream": False,
            })
            final_content = final.json().get("message", {}).get("content", "")
            print(f"({time.time()-t1:.1f}s)")
            messages.append({"role": "assistant", "content": final_content})
            print(f"💬 回复: {final_content}\n")
        else:
            content = msg.get("content", "")
            messages.append({"role": "assistant", "content": content})
            print(f"💬 回复: {content}\n")

    mcp.close()
    print("\n👋 再见!")


if __name__ == "__main__":
    main()
