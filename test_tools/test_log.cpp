#include <iostream>
#include <string>

#include "log_manager.h"
#include "mcp_spdlog_config.h"

int main() {
  mpc::SpdlogConfig spdlog_config;
  spdlog_config.InitSpdlog("../config/log_config.json");
  mpc::Logger::GetInstance().Init(&spdlog_config);
  MCP_LOG_INFO("test log");
  return 0;
}