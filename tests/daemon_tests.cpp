#include "scrollshift/daemon.hpp"

#include <chrono>
#include <csignal>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

// The daemon under test is single-threaded and uses a volatile sig_atomic_t
// stop flag polled in 50 ms quanta, so SIGTERM must stop it within a bounded
// window even at the maximum reconnect interval.
//
// NOTE: this test deliberately uses no assert() on its critical path. In
// Release builds NDEBUG compiles assert(...) expressions out entirely, which
// would skip the kill/waitpid that make the test meaningful and leave the
// daemon child orphaned holding ctest's output pipe open (a 1500 s hang).
// All checks are explicit and return non-zero.
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
  if (child < 0) {
    std::fprintf(stderr, "[daemon-test] fork failed\n");
    return 1;
  }
  if (child == 0) {
    std::ostringstream output;
    std::fprintf(stderr, "[daemon-test] child: entering run_daemon\n");
    const int rc = scrollshift::run_daemon(config, output, input_root, root / "no-uinput");
    std::fprintf(stderr, "[daemon-test] child: run_daemon returned %d\n", rc);
    _exit(rc);
  }

  std::fprintf(stderr, "[daemon-test] parent: forked child %d\n", child);
  ::usleep(150000);
  const auto started = std::chrono::steady_clock::now();
  std::fprintf(stderr, "[daemon-test] parent: sending SIGTERM\n");
  if (::kill(child, SIGTERM) != 0) {
    std::fprintf(stderr, "[daemon-test] parent: kill(SIGTERM) failed\n");
    return 1;
  }

  // Bound the wait well below CTest's default timeout so a regression or a
  // broken environment fails fast with the child's progress markers.
  const auto deadline = started + std::chrono::seconds(5);
  int status = 0;
  for (;;) {
    const pid_t done = ::waitpid(child, &status, WNOHANG);
    if (done == child) break;
    if (done == -1) {
      std::fprintf(stderr, "[daemon-test] parent: waitpid error\n");
      return 1;
    }
    if (std::chrono::steady_clock::now() > deadline) {
      std::fprintf(stderr, "[daemon-test] parent: child did not exit within 5s of SIGTERM\n");
      ::kill(child, SIGKILL);
      ::waitpid(child, &status, 0);
      return 1;
    }
    ::usleep(10000);
  }

  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - started);
  std::fprintf(stderr, "[daemon-test] parent: child exited in %lld ms\n",
               static_cast<long long>(elapsed.count()));
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    std::fprintf(stderr, "[daemon-test] parent: child did not exit cleanly (status %d)\n", status);
    return 1;
  }
  if (elapsed >= std::chrono::milliseconds(1000)) {
    std::fprintf(stderr, "[daemon-test] parent: daemon shutdown took too long\n");
    return 1;
  }

  fs::remove_all(root);
  return 0;
}
