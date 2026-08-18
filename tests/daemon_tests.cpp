#include "scrollshift/daemon.hpp"

#include <cassert>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

int main() {
  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / ("scrollshift-daemon-test-" + std::to_string(::getpid()));
  const fs::path input_root = root / "input";
  const fs::path config = root / "config.conf";
  fs::create_directories(input_root);
  {
    std::ofstream out(config);
    out << "device_vendor = 0x3151\n"
        << "device_product = 0x402d\n"
        << "device_name = 2.4G Wireless Mouse\n"
        << "profile = balanced\n"
        << "reconnect_ms = 30000\n";
  }

  // Even with the maximum reconnect sleep, SIGTERM should stop a waiting daemon promptly.
  const pid_t child = ::fork();
  assert(child >= 0);
  if (child == 0) {
    std::ostringstream output;
    const int rc = scrollshift::run_daemon(config, output, input_root, root / "no-uinput");
    _exit(rc);
  }

  ::usleep(150000);
  const auto started = std::chrono::steady_clock::now();
  assert(::kill(child, SIGTERM) == 0);
  int status = 0;
  assert(::waitpid(child, &status, 0) == child);
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - started);
  assert(WIFEXITED(status));
  assert(WEXITSTATUS(status) == 0);
  assert(elapsed < std::chrono::milliseconds(1000));

  fs::remove_all(root);
  return 0;
}
