#!/usr/bin/env python3
"""MCP Bridge — relays stdin/stdout to C++ MCP server via pipe."""
import subprocess, sys, os, threading

SERVER = '/home/you_dian/MCP/mcp_mt/build/server'
CWD = '/home/you_dian/MCP/mcp_mt'

proc = subprocess.Popen(
    [SERVER, '--no-http'],
    stdin=subprocess.PIPE, stdout=subprocess.PIPE,
    stderr=subprocess.PIPE, cwd=CWD
)

# Relay stderr to file
def relay_stderr():
    with open('/home/you_dian/MCP/logs/mcp_bridge_stderr.log', 'ab') as f:
        for data in iter(proc.stderr.read, b''):
            f.write(data)

threading.Thread(target=relay_stderr, daemon=True).start()

# Relay stdin → server (in background)
def relay_stdin():
    try:
        while True:
            data = sys.stdin.buffer.read(65536)
            if not data:
                proc.stdin.close()
                break
            proc.stdin.write(data)
            proc.stdin.flush()
    except Exception:
        pass

threading.Thread(target=relay_stdin, daemon=True).start()

# Relay server stdout → client (main thread)
try:
    while True:
        data = proc.stdout.read(65536)
        if not data:
            break
        sys.stdout.buffer.write(data)
        sys.stdout.buffer.flush()
except Exception:
    pass
finally:
    proc.terminate()
    proc.wait()
