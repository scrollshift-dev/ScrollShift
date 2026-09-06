#include "scrollshift/daemon.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <sstream>
#include <thread>
#include <vector>

#include "scrollshift/config.hpp"
#include "scrollshift/input.hpp"
#include "scrollshift/relay.hpp"

namespace scrollshift {
namespace {
void interruptible_sleep(int milliseconds) {
  constexpr int quantum_ms = 50;
  int remaining = milliseconds;
  while (remaining > 0 && !relay_stop_requested()) {
    const int step = std::min(remaining, quantum_ms);
    std::this_thread::sleep_for(std::chrono::milliseconds(step));
    remaining -= step;
  }
}

const char* match_state_name(DeviceMatchState state) {
  switch (state) {
    case DeviceMatchState::Missing: return "missing";
    case DeviceMatchState::Unique: return "ready";
    case DeviceMatchState::Ambiguous: return "ambiguous";
  }
  return "unknown";
}

struct RelayWorker {
  std::filesystem::path path;
  std::string name;
  std::atomic<bool> finished{false};
  int result{0};
  std::ostringstream log;
  std::thread thread;
};

bool worker_for(const std::vector<std::unique_ptr<RelayWorker>>& workers,
                const std::filesystem::path& path) {
  return std::any_of(workers.begin(), workers.end(), [&](const auto& worker) {
    return worker->path == path && !worker->finished.load();
  });
}

void reap_workers(std::vector<std::unique_ptr<RelayWorker>>& workers, std::ostream& output,
                  bool join_all = false) {
  auto it = workers.begin();
  while (it != workers.end()) {
    auto& worker = *it;
    if (!join_all && !worker->finished.load()) { ++it; continue; }
    if (worker->thread.joinable()) worker->thread.join();
    const auto text = worker->log.str();
    if (!text.empty()) output << text;
    if (!join_all) output << "Input session for " << worker->name << " ended (code " << worker->result
                          << "); device will be rediscovered if still present.\n";
    output.flush();
    it = workers.erase(it);
  }
}
}

int run_doctor(const std::filesystem::path& config_path, std::ostream& output,
               const std::filesystem::path& input_root) {
  std::string error;
  const auto config = load_config(config_path, error);
  if (!config) {
    output << "scrollshift: configuration error: " << error << '\n';
    return 2;
  }

  const auto devices = discover_input_devices(input_root);
  output << "Config: " << config_path << '\n'
         << "Profile: " << config->profile << '\n';

  if (config->auto_discover) {
    const auto mice = automatic_mouse_candidates(devices);
    output << "Mode: automatic mouse discovery\n"
           << "Detected conventional wheel mice: " << mice.size() << '\n';
    for (const auto& mouse : mice) output << "  " << mouse.path << "  " << mouse.name << '\n';
    output << "Touchpads and touchscreens are excluded; hotplugged mice are discovered automatically.\n";
    if (mice.empty()) output << "No mouse is attached right now; the daemon can still start and wait for one.\n";
    return 0;
  }

  const auto diagnosis = diagnose_device_match(devices, config->device);
  output << "Mode: manual device override\n"
         << "Device: " << std::hex << config->device.vendor << ':' << config->device.product << std::dec;
  if (!config->device.name.empty()) output << "  " << config->device.name;
  output << '\n' << "Match state: " << match_state_name(diagnosis.state) << '\n';
  for (const auto& match : diagnosis.matches) output << "  " << match.path << "  " << match.name << '\n';

  if (diagnosis.state == DeviceMatchState::Unique) {
    output << "ScrollShift is ready to capture this device.\n";
    return 0;
  }
  if (diagnosis.state == DeviceMatchState::Missing) {
    output << "No currently readable capture candidate matches the configured identity.\n";
    return 1;
  }
  output << "Multiple capture candidates match; ScrollShift will refuse to grab any of them.\n";
  return 1;
}

int run_daemon(const std::filesystem::path& config_path, std::ostream& output,
               const std::filesystem::path& input_root,
               const std::filesystem::path& uinput_path) {
  std::string error;
  const auto config = load_config(config_path, error);
  if (!config) {
    output << "scrollshift: configuration error: " << error << '\n';
    return 2;
  }

  install_relay_signal_handlers();
  reset_relay_stop_request();
  output << "ScrollShift daemon starting with profile '" << config->profile << "'.\n";
  output.flush();

  if (config->auto_discover) {
    output << "Automatic mouse discovery enabled; touchpads and touchscreens are ignored.\n";
    output.flush();
    std::vector<std::unique_ptr<RelayWorker>> workers;
    bool announced_wait = false;
    while (!relay_stop_requested()) {
      reap_workers(workers, output);
      const auto mice = automatic_mouse_candidates(discover_input_devices(input_root));
      bool attached_any = false;
      for (const auto& mouse : mice) {
        if (worker_for(workers, mouse.path)) { attached_any = true; continue; }
        auto worker = std::make_unique<RelayWorker>();
        worker->path = mouse.path;
        worker->name = mouse.name.empty() ? mouse.path.string() : mouse.name;
        auto* raw = worker.get();
        output << "Attaching to " << mouse.path << " (" << worker->name << ")\n";
        output.flush();
        raw->thread = std::thread([raw, profile = config->profile, uinput_path]() {
          raw->result = run_accelerated_relay(raw->path, profile, 0, 0, raw->log, uinput_path, false);
          raw->finished.store(true);
        });
        workers.push_back(std::move(worker));
        attached_any = true;
      }
      if (!attached_any && workers.empty()) {
        if (!announced_wait) {
          output << "Waiting for a conventional wheel mouse...\n";
          output.flush();
          announced_wait = true;
        }
      } else {
        announced_wait = false;
      }
      interruptible_sleep(config->reconnect_ms);
    }
    reap_workers(workers, output, true);
    output << "ScrollShift daemon stopped.\n";
    output.flush();
    return 0;
  }

  DeviceMatchState last_state = DeviceMatchState::Unique;
  while (!relay_stop_requested()) {
    const auto diagnosis = diagnose_device_match(discover_input_devices(input_root), config->device);
    if (diagnosis.state == DeviceMatchState::Missing) {
      if (last_state != diagnosis.state) {
        output << "Waiting for configured mouse...\n";
        output.flush();
      }
      last_state = diagnosis.state;
      interruptible_sleep(config->reconnect_ms);
      continue;
    }
    if (diagnosis.state == DeviceMatchState::Ambiguous) {
      if (last_state != diagnosis.state) {
        output << "scrollshift: configured device identity is ambiguous (" << diagnosis.matches.size()
               << " matching event nodes); refusing to grab any of them.\n";
        output.flush();
      }
      last_state = diagnosis.state;
      interruptible_sleep(config->reconnect_ms);
      continue;
    }

    last_state = diagnosis.state;
    const auto& device = diagnosis.matches.front();
    output << "Attaching to " << device.path << " (" << device.name << ")\n";
    output.flush();
    const int result = run_accelerated_relay(device.path, config->profile, 0, 0, output, uinput_path, false);
    if (relay_stop_requested()) break;
    output << "Input session ended (code " << result << "); rediscovering device.\n";
    output.flush();
    interruptible_sleep(config->reconnect_ms);
  }

  output << "ScrollShift daemon stopped.\n";
  output.flush();
  return 0;
}
}  // namespace scrollshift
