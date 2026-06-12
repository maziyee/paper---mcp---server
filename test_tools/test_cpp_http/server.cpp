#include <httplib.h>

#include <string>

int main() {
  httplib::Server svr;
  svr.Get("/hello", [](const httplib::Request& req, httplib::Response& res) {
    res.set_content("Hello World!", "text/plain");
  });

  svr.Get("/greet", [](const httplib::Request& req, httplib::Response& res) {
    std::string name = req.get_param_value("name");
    if (name.empty()) {
      res.set_content("Hello World!", "text/plain");
    } else {
      res.set_content("Hello " + name + "!", "text/plain");
    }
  });

  std::cout << "Server running on http://0.0.0.0:8080" << std::endl;
  svr.listen("0.0.0.0", 8080);

  return 0;
}