#include <iostream>
#include <string>

#include "nlohmann/json.hpp"
#include "rpc_manager.h"
#include "std_handle_rpc.h"

int main() {
  mcp::RpcManager manager;

  manager.RegisterMethod("Echo", "Echo",
                         [](const std::string& payload) { return payload; });

  manager.RegisterMethod("Math", "Add", [](const std::string& payload) {
    auto j = nlohmann::json::parse(payload);
    return std::to_string(j["a"].get<int>() + j["b"].get<int>());
  });

  std::cerr << "Registered: Echo/Echo, Math/Add" << std::endl;
  std::cerr << "Ctrl+D to quit" << std::endl;

  mcp::StdRpcServer server(manager, std::cout, std::cin);
  server.Run();

  return 0;
}
