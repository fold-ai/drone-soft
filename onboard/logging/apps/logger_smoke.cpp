#include "logging/blackbox.hpp"
#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
  std::string out = "/tmp/actprove_blackbox";
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--out" && i + 1 < argc) out = argv[++i];
  }
  actprove::logging::BlackboxWriter w({out, "", 1u << 20});
  if (!w.open_run()) {
    std::cerr << "open_run failed\n";
    return 1;
  }
  w.write_jsonl("mission_state", "{\"state\":\"BOOT\"}");
  std::cout << "run_dir=" << w.run_dir() << "\n";
  return 0;
}
