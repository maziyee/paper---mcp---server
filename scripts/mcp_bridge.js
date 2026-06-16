#!/usr/bin/env node
/**
 * MCP Bridge — Node.js wrapper that spawns the C++ MCP server
 * and relays stdin/stdout with protocol format matching.
 */
const { spawn } = require('child_process');

const SERVER = '/home/you_dian/MCP/mcp_mt/build/server';
const CWD = '/home/you_dian/MCP/mcp_mt';

// Log to file, not stderr
const fs = require('fs');
const log = (msg) => fs.appendFileSync('/home/you_dian/MCP/logs/mcp_bridge.log', `[${new Date().toISOString()}] ${msg}\n`);

log(`Bridge starting: ${SERVER}`);

const proc = spawn(SERVER, ['--no-http'], { cwd: CWD, stdio: ['pipe', 'pipe', 'pipe'] });

proc.on('error', (err) => {
    log(`Spawn error: ${err.message}`);
    // Report error to Claude Code via stdout (MCP error format)
    process.stdout.write(JSON.stringify({
        jsonrpc: "2.0", id: 0,
        error: { code: -32000, message: `Spawn failed: ${err.message}` }
    }) + '\n');
    process.exit(1);
});

proc.on('spawn', () => {
    log(`Server spawned, PID=${proc.pid}`);
});

// Relay stdin → server
process.stdin.on('data', (data) => {
    log(`CLIENT->SERVER (${data.length} bytes)`);
    proc.stdin.write(data);
});

process.stdin.on('end', () => {
    log('Client stdin closed');
    proc.stdin.end();
});

// Relay server stdout → client
proc.stdout.on('data', (data) => {
    log(`SERVER->CLIENT (${data.length} bytes)`);
    process.stdout.write(data);
});

// Relay server stderr → file
proc.stderr.on('data', (data) => {
    fs.appendFileSync('/home/you_dian/MCP/logs/mcp_bridge_stderr.log', data);
});

proc.on('close', (code) => {
    log(`Server exited with code ${code}`);
    process.exit(code || 0);
});

process.on('SIGTERM', () => { proc.kill(); process.exit(); });
process.on('SIGINT', () => { proc.kill(); process.exit(); });
