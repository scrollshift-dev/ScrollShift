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

int backoff_checks();
int auto_mode_lifecycle();

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
  const fs::path auto_config = root / "auto.conf";
  {
    std::ofstream out(auto_config);
    out << "mode = auto\nprofile = balanced\nreconnect_ms = 1000\n";
  }
  {
    std::ostringstream doctor_output;
    if (scrollshift::run_doctor(auto_config, doctor_output, input_root) != 0 ||
        doctor_output.str().find("No mouse is attached right now") == std::string::npos) {
      std::fprintf(stderr, "[daemon-test] auto doctor did not accept empty hotplug-ready state\n");
      return 1;
    }
  }
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

  if (const int rc = backoff_checks(); rc != 0) return rc;
  if (const int rc = auto_mode_lifecycle(); rc != 0) return rc;

  fs::remove_all(root);
  return 0;
}

// Bounded exponential backoff: grows from the base interval and is capped.
int backoff_checks() {
  using scrollshift::relay_backoff_ms;
  const int base = 1000;
  if (relay_backoff_ms(0, base) != 1000) return 1;
  if (relay_backoff_ms(1, base) != 2000) return 1;
  if (relay_backoff_ms(2, base) != 4000) return 1;
  if (relay_backoff_ms(3, base) != 8000) return 1;
  if (relay_backoff_ms(4, base) != 16000) return 1;
  if (relay_backoff_ms(5, base) != 30000) return 1;   // capped
  if (relay_backoff_ms(30, base) != 30000) return 1;  // capped, no overflow
  if (relay_backoff_ms(0, 5000) != 5000) return 1;
  if (relay_backoff_ms(1, 5000) != 10000) return 1;
  if (relay_backoff_ms(2, 5000) != 20000) return 1;
  if (relay_backoff_ms(3, 5000) != 30000) return 1;
  if (relay_backoff_ms(-1, base) != 1000) return 1;   // invalid input stays at base
  if (relay_backoff_ms(0, 40000, 30000) != 40000) return 1;  // base above cap clamps cap to base
  std::fprintf(stderr, "[daemon-test] backoff policy checks passed\n");
  return 0;
}

// Auto mode with zero attached mice must wait quietly and stop promptly on
// SIGTERM even at the maximum reconnect interval.
int auto_mode_lifecycle() {
  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / ("scrollshift-auto-test-" + std::to_string(::getpid()));
  const fs::path input_root = root / "input";
  const fs::path udev_root = root / "udev";
  fs::create_directories(input_root);
  fs::create_directories(udev_root);
  const fs::path config = root / "auto.conf";
  {
    std::ofstream out(config);
    out << "mode = auto\nprofile = balanced\nreconnect_ms = 30000\n";
  }

  const pid_t child = ::fork();
  if (child < 0) return 1;
  if (child == 0) {
    std::ostringstream output;
    const int rc = scrollshift::run_daemon(config, output, input_root, root / "no-uinput", udev_root);
    std::fprintf(stderr, "[daemon-test] auto child: run_daemon returned %d\n", rc);
    _exit(rc);
  }

  ::usleep(150000);
  const auto started = std::chrono::steady_clock::now();
  if (::kill(child, SIGTERM) != 0) {
    std::fprintf(stderr, "[daemon-test] auto parent: kill(SIGTERM) failed\n");
    return 1;
  }
  const auto deadline = started + std::chrono::seconds(5);
  int status = 0;
  for (;;) {
    const pid_t done = ::waitpid(child, &status, WNOHANG);
    if (done == child) break;
    if (done == -1) return 1;
    if (std::chrono::steady_clock::now() > deadline) {
      std::fprintf(stderr, "[daemon-test] auto parent: child did not exit within 5s of SIGTERM\n");
      ::kill(child, SIGKILL);
      ::waitpid(child, &status, 0);
      return 1;
    }
    ::usleep(10000);
  }
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - started);
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    std::fprintf(stderr, "[daemon-test] auto parent: child did not exit cleanly (status %d)\n", status);
    return 1;
  }
  if (elapsed >= std::chrono::milliseconds(1000)) {
    std::fprintf(stderr, "[daemon-test] auto parent: auto daemon shutdown took too long (%lld ms)\n",
                 static_cast<long long>(elapsed.count()));
    return 1;
  }

  fs::remove_all(root);
  std::fprintf(stderr, "[daemon-test] auto-mode lifecycle passed\n");
  return 0;
}
