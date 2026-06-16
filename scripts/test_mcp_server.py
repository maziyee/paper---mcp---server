#!/usr/bin/env python3
"""Minimal MCP stdio server for testing Claude Code connectivity"""
import sys, json

def read_message():
    """Read a Content-Length framed message from stdin"""
    content_length = None
    for line in sys.stdin:
        line = line.rstrip('\r\n')
        if line == '':
            break
        if line.lower().startswith('content-length:'):
            content_length = int(line.split(':')[1].strip())
    if content_length is None:
        return None
    body = sys.stdin.read(content_length)
    return json.loads(body)

def write_message(data):
    """Write a Content-Length framed message to stdout"""
    body = json.dumps(data)
    header = f"Content-Length: {len(body)}\r\n\r\n"
    sys.stdout.write(header + body)
    sys.stdout.flush()

def main():
    # Log to stderr only
    sys.stderr.write("[test-mcp] Ready, waiting for initialize...\n")
    sys.stderr.flush()

    while True:
        msg = read_message()
        if msg is None:
            sys.stderr.write("[test-mcp] EOF, exiting\n")
            break

        method = msg.get('method', '')
        msg_id = msg.get('id')

        if method == 'initialize':
            sys.stderr.write(f"[test-mcp] Got initialize (id={msg_id})\n")
            sys.stderr.flush()
            write_message({
                "jsonrpc": "2.0",
                "id": msg_id,
                "result": {
                    "protocolVersion": msg.get('params', {}).get('protocolVersion', '2024-11-05'),
                    "capabilities": {"tools": {}},
                    "serverInfo": {"name": "test-mcp", "version": "1.0.0"}
                }
            })

        elif method == 'notifications/initialized':
            sys.stderr.write("[test-mcp] Got initialized notification\n")
            sys.stderr.flush()

        elif method == 'tools/list':
            sys.stderr.write(f"[test-mcp] Got tools/list (id={msg_id})\n")
            sys.stderr.flush()
            write_message({
                "jsonrpc": "2.0",
                "id": msg_id,
                "result": {"tools": []}
            })

        elif method == 'tools/call':
            sys.stderr.write(f"[test-mcp] Got tools/call (id={msg_id})\n")
            sys.stderr.flush()
            write_message({
                "jsonrpc": "2.0",
                "id": msg_id,
                "result": {
                    "content": [{"type": "text", "text": "Hello from test-mcp!"}],
                    "isError": False
                }
            })

        elif msg.get('id') is None:
            # Notification without method field handled above
            sys.stderr.write(f"[test-mcp] Unknown notification: {method}\n")
            sys.stderr.flush()

        else:
            sys.stderr.write(f"[test-mcp] Unknown method: {method}\n")
            sys.stderr.flush()
            write_message({
                "jsonrpc": "2.0",
                "id": msg_id,
                "error": {"code": -32601, "message": f"Method not found: {method}"}
            })

if __name__ == '__main__':
    main()
