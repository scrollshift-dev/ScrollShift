#include "scrollshift/service.hpp"
#include "scrollshift/daemon.hpp"
#include "scrollshift/version.hpp"

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace scrollshift {
namespace {
namespace fs = std::filesystem;
constexpr const char* kUnitName = "scrollshift.service";
constexpr const char* kUnitPath = "/etc/systemd/system/scrollshift.service";
constexpr const char* kBinaryPath = "/usr/local/bin/scrollshift";
constexpr const char* kConfigDir = "/etc/scrollshift";
constexpr const char* kConfigPath = "/etc/scrollshift/config.conf";
constexpr const char* kMarker = "# Managed by scrollshift. Do not edit manually.";

int run(const std::vector<std::string>& args) {
  if (args.empty()) return 127;
  pid_t pid = ::fork();
  if (pid < 0) return 127;
  if (pid == 0) {
    std::vector<char*> av;
    av.reserve(args.size() + 1);
    for (const auto& s : args) av.push_back(const_cast<char*>(s.c_str()));
    av.push_back(nullptr);
    ::execvp(av[0], av.data());
    _exit(errno == ENOENT ? 127 : 126);
  }
  int status = 0;
  while (::waitpid(pid, &status, 0) < 0) if (errno != EINTR) return 127;
  if (WIFEXITED(status)) return WEXITSTATUS(status);
  return 128;
}

bool command_exists(const char* name) {
  const char* path = std::getenv("PATH");
  if (!path) return false;
  std::string p(path);
  std::size_t pos = 0;
  while (pos <= p.size()) {
    const auto end = p.find(':', pos);
    const auto dir = p.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    fs::path candidate = (dir.empty() ? fs::path(".") : fs::path(dir)) / name;
    if (::access(candidate.c_str(), X_OK) == 0) return true;
    if (end == std::string::npos) break;
    pos = end + 1;
  }
  return false;
}

bool is_root() { return ::geteuid() == 0; }

std::string unit_body() {
  return std::string(kMarker) + R"(
[Unit]
Description=ScrollShift responsive mouse-wheel scrolling
Documentation=https://scrollshift.dev/
After=systemd-udevd.service

[Service]
Type=simple
ExecStart=/usr/local/bin/scrollshift daemon --config /etc/scrollshift/config.conf
Restart=on-failure
RestartPreventExitStatus=2
RestartSec=2s
TimeoutStopSec=2s
NoNewPrivileges=yes
PrivateTmp=yes
ProtectSystem=strict
ProtectHome=yes
ProtectKernelTunables=yes
ProtectKernelModules=yes
ProtectControlGroups=yes
RestrictSUIDSGID=yes
LockPersonality=yes
RestrictAddressFamilies=AF_UNIX
ReadOnlyPaths=/etc/scrollshift

[Install]
WantedBy=multi-user.target
)";
}

bool managed_unit(std::string* why = nullptr) {
  std::ifstream in(kUnitPath);
  if (!in) { if (why) *why = "unit is not installed"; return false; }
  std::string body((std::istreambuf_iterator<char>(in)), {});
  if (body.rfind(kMarker, 0) != 0) { if (why) *why = "existing unit is not managed by ScrollShift"; return false; }
  if (body != unit_body()) {
    if (why) *why = "managed unit has been modified or is from an incompatible service format";
    return false;
  }
  return true;
}

int require_systemd(std::ostream& err) {
  if (!command_exists("systemctl")) { err << "scrollshift: systemctl not found; service management requires systemd\n"; return 1; }
  return 0;
}
int require_root(std::string_view action, std::ostream& err) {
  if (!is_root()) { err << "scrollshift: service " << action << " requires root (run `sudo scrollshift service " << action << "`)\n"; return 1; }
  return 0;
}

bool atomic_copy(const fs::path& src, const fs::path& dst, mode_t mode, std::ostream& err) {
  std::error_code ec;
  fs::create_directories(dst.parent_path(), ec);
  if (ec) { err << "scrollshift: cannot create " << dst.parent_path() << ": " << ec.message() << '\n'; return false; }
  const fs::path tmp = dst.string() + ".new";
  fs::copy_file(src, tmp, fs::copy_options::overwrite_existing, ec);
  if (ec) { err << "scrollshift: cannot copy executable: " << ec.message() << '\n'; return false; }
  if (::chmod(tmp.c_str(), mode) != 0) { err << "scrollshift: chmod failed: " << std::strerror(errno) << '\n'; fs::remove(tmp, ec); return false; }
  fs::rename(tmp, dst, ec);
  if (ec) { err << "scrollshift: cannot install " << dst << ": " << ec.message() << '\n'; fs::remove(tmp, ec); return false; }
  return true;
}

fs::path self_executable() {
  std::error_code ec;
  auto p = fs::read_symlink("/proc/self/exe", ec);
  return ec ? fs::path{} : fs::canonical(p, ec);
}

int install_service(std::ostream& out, std::ostream& err) {
  if (require_root("install", err) || require_systemd(err)) return 1;
  if (fs::exists(kUnitPath)) {
    std::string why;
    if (!managed_unit(&why)) { err << "scrollshift: refusing to replace " << kUnitPath << ": " << why << '\n'; return 1; }
  }
  const auto self = self_executable();
  if (self.empty()) { err << "scrollshift: cannot resolve current executable path\n"; return 1; }
  std::error_code ec;
  fs::create_directories(kConfigDir, ec);
  if (ec) { err << "scrollshift: cannot create " << kConfigDir << ": " << ec.message() << '\n'; return 1; }
  if (!atomic_copy(self, kBinaryPath, 0755, err)) return 1;
  const fs::path tmp = std::string(kUnitPath) + ".new";
  { std::ofstream f(tmp, std::ios::trunc); if (!f) { err << "scrollshift: cannot write temporary systemd unit\n"; return 1; } f << unit_body(); f.flush(); if (!f) return 1; }
  if (::chmod(tmp.c_str(), 0644) != 0) { err << "scrollshift: chmod unit failed: " << std::strerror(errno) << '\n'; fs::remove(tmp, ec); return 1; }
  fs::rename(tmp, kUnitPath, ec);
  if (ec) { err << "scrollshift: cannot install unit: " << ec.message() << '\n'; fs::remove(tmp, ec); return 1; }
  if (run({"systemctl", "daemon-reload"}) != 0 || run({"systemctl", "enable", kUnitName}) != 0) {
    err << "scrollshift: systemd reload/enable failed; service files were left installed for inspection\n"; return 1;
  }
  out << "Installed ScrollShift " << kVersion << " service.\n";
  if (fs::exists(kConfigPath)) {
    if (run({kBinaryPath, "doctor", "--config", kConfigPath}) == 0) {
      if (run({"systemctl", "restart", kUnitName}) != 0) { err << "scrollshift: service installed but failed to start\n"; return 1; }
      out << "Service enabled and running.\n";
      return 0;
    }
    run({"systemctl", "stop", kUnitName});
    out << "Service enabled but not started because the current configuration did not pass `scrollshift doctor`.\n";
    return 0;
  }
  run({"systemctl", "stop", kUnitName});
  out << "Service enabled but not started. Configure a mouse first, then run `sudo scrollshift service start`.\n";
  return 0;
}

int uninstall_service(std::ostream& out, std::ostream& err) {
  if (require_root("uninstall", err) || require_systemd(err)) return 1;
  if (!fs::exists(kUnitPath)) { out << "ScrollShift service is not installed.\n"; return 0; }
  std::string why;
  if (!managed_unit(&why)) { err << "scrollshift: refusing to uninstall: " << why << '\n'; return 1; }
  if (run({"systemctl", "stop", kUnitName}) != 0) { err << "scrollshift: refusing to remove the unit because systemctl stop failed\n"; return 1; }
  if (run({"systemctl", "disable", kUnitName}) != 0) { err << "scrollshift: refusing to remove the unit because systemctl disable failed\n"; return 1; }
  std::error_code ec;
  fs::remove(kUnitPath, ec);
  if (ec) { err << "scrollshift: cannot remove unit: " << ec.message() << '\n'; return 1; }
  if (run({"systemctl", "daemon-reload"}) != 0) { err << "scrollshift: unit removed but systemd daemon-reload failed\n"; return 1; }
  out << "Uninstalled the ScrollShift service registration. Configuration in /etc/scrollshift was kept.\n";
  return 0;
}

int managed_action(std::string_view action, std::ostream& err) {
  if (require_systemd(err)) return 1;
  if ((action == "start" || action == "stop" || action == "restart" || action == "enable" || action == "disable") && require_root(action, err)) return 1;
  std::string why;
  if (!managed_unit(&why)) { err << "scrollshift: refusing to " << action << ": " << why << " (run `sudo scrollshift service install`)\n"; return 1; }
  if ((action == "start" || action == "restart") && run({kBinaryPath, "doctor", "--config", kConfigPath}) != 0) {
    err << "scrollshift: refusing to " << action << ": configuration/device preflight failed\n"; return 1;
  }
  return run({"systemctl", std::string(action), kUnitName});
}
}

int run_service_command(std::string_view action, bool follow_logs, std::ostream& out, std::ostream& err) {
  if (action == "install") return install_service(out, err);
  if (action == "uninstall") return uninstall_service(out, err);
  if (action == "status") {
    if (require_systemd(err)) return 1;
    std::string why; out << "ScrollShift " << kVersion << '\n';
    out << "unit: " << (managed_unit(&why) ? "installed (managed)" : why) << '\n';
    if (!fs::exists(kUnitPath)) return 0;
    const int rc = run({"systemctl", "status", kUnitName, "--no-pager"});
    return (rc == 126 || rc == 127 || rc == 128) ? 1 : 0;
  }
  if (action == "logs") {
    if (require_systemd(err)) return 1;
    std::string why; if (!managed_unit(&why)) { err << "scrollshift: " << why << '\n'; return 1; }
    std::vector<std::string> args{"journalctl", "-u", kUnitName, "-n", "80"};
    if (follow_logs) args.push_back("-f"); else args.push_back("--no-pager");
    return run(args);
  }
  if (action == "start" || action == "stop" || action == "restart" || action == "enable" || action == "disable") return managed_action(action, err);
  err << "scrollshift: service action must be install, uninstall, start, stop, restart, status, enable, disable, or logs\n";
  return 2;
}
}
