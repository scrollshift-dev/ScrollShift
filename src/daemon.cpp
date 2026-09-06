#include "scrollshift/daemon.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <thread>
#include <vector>

#include "scrollshift/config.hpp"
#include "scrollshift/input.hpp"
#include "scrollshift/relay.hpp"

namespace scrollshift {
namespace {
constexpr int kMaxBackoffMs = 30000;
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

struct BackoffState {
  std::map<std::filesystem::path, int> failures;
  std::map<std::filesystem::path, std::chrono::steady_clock::time_point> next_attempt;
  std::set<std::filesystem::path> previously_present;
};

void reap_workers(std::vector<std::unique_ptr<RelayWorker>>& workers, std::ostream& output,
                  BackoffState* backoff, int base_ms, bool join_all = false) {
  auto it = workers.begin();
  while (it != workers.end()) {
    auto& worker = *it;
    if (!join_all && !worker->finished.load()) { ++it; continue; }
    if (worker->thread.joinable()) worker->thread.join();
    bool announce = join_all;
    if (!join_all && backoff) {
      if (worker->result != 0) {
        ++backoff->failures[worker->path];
        // Only surface the first failure of a burst; repeated identical errors
        // are suppressed in favour of the bounded backoff cadence.
        announce = backoff->failures[worker->path] == 1;
        backoff->next_attempt[worker->path] = std::chrono::steady_clock::now() +
            std::chrono::milliseconds(relay_backoff_ms(backoff->failures[worker->path], base_ms));
      } else {
        backoff->failures[worker->path] = 0;
        backoff->next_attempt.erase(worker->path);
      }
    }
    const auto text = worker->log.str();
    if (!text.empty() && announce) output << text;
    if (!join_all && announce) output << "Input session for " << worker->name << " ended (code "
                                      << worker->result
                                      << "); retrying with bounded backoff while the device remains present.\n";
    output.flush();
    it = workers.erase(it);
  }
}
}  // namespace

int relay_backoff_ms(int consecutive_failures, int base_ms, int max_ms) {
  if (base_ms < 1 || consecutive_failures < 0) return base_ms;
  if (max_ms < base_ms) max_ms = base_ms;
  long delay = base_ms;
  for (int i = 0; i < consecutive_failures && delay < max_ms; ++i) {
    delay *= 2;
    if (delay > max_ms) delay = max_ms;
  }
  return static_cast<int>(delay);
}

int run_doctor(const std::filesystem::path& config_path, std::ostream& output,
               const std::filesystem::path& input_root, const std::filesystem::path& udev_data_root) {
  std::string error;
  const auto config = load_config(config_path, error);
  if (!config) {
    output << "scrollshift: configuration error: " << error << '\n';
    return 2;
  }

  const auto devices = discover_input_devices(input_root, udev_data_root);
  output << "Config: " << config_path << '\n'
         << "Profile: " << config->profile << '\n';

  if (config->auto_discover) {
    const auto mice = automatic_mouse_candidates(devices);
    output << "Mode: automatic mouse discovery\n"
           << "Detected conventional wheel mice: " << mice.size() << '\n';
    for (const auto& mouse : mice) output << "  " << mouse.path << "  " << mouse.name << '\n';
    output << "Touchpads, touchscreens, joysticks, tablets and other non-mouse classes are excluded; hotplugged mice are discovered automatically.\n";
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
               const std::filesystem::path& input_root, const std::filesystem::path& uinput_path,
               const std::filesystem::path& udev_data_root) {
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
    output << "Automatic mouse discovery enabled; non-mouse input classes are ignored.\n";
    output.flush();
    std::vector<std::unique_ptr<RelayWorker>> workers;
    BackoffState backoff;
    bool announced_wait = false;
    while (!relay_stop_requested()) {
      reap_workers(workers, output, &backoff, config->reconnect_ms);
      const auto mice = automatic_mouse_candidates(discover_input_devices(input_root, udev_data_root));

      std::set<std::filesystem::path> present;
      for (const auto& mouse : mice) present.insert(mouse.path);
      // A device that was absent and reappears starts with a clean slate, so a
      // replug resets any prior contention backoff for that device.
      for (const auto& path : present) {
        if (!backoff.previously_present.count(path)) { backoff.failures[path] = 0; backoff.next_attempt.erase(path); }
      }
      backoff.previously_present = present;
      for (auto it = backoff.next_attempt.begin(); it != backoff.next_attempt.end();) {
        if (!present.count(it->first)) it = backoff.next_attempt.erase(it); else ++it;
      }
      for (auto it = backoff.failures.begin(); it != backoff.failures.end();) {
        if (!present.count(it->first)) it = backoff.failures.erase(it); else ++it;
      }

      const auto now = std::chrono::steady_clock::now();
      std::chrono::milliseconds min_backoff_remaining{kMaxBackoffMs};
      bool any_pending_backoff = false;
      bool any_eligible_now = false;
      for (const auto& mouse : mice) {
        if (worker_for(workers, mouse.path)) {
          backoff.failures[mouse.path] = 0;
          backoff.next_attempt.erase(mouse.path);
          continue;
        }
        const auto pending = backoff.next_attempt.find(mouse.path);
        if (pending != backoff.next_attempt.end() && pending->second > now) {
          const auto remaining =
              std::chrono::duration_cast<std::chrono::milliseconds>(pending->second - now);
          min_backoff_remaining = std::min(min_backoff_remaining, remaining);
          any_pending_backoff = true;
          continue;
        }
        any_eligible_now = true;
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
        backoff.failures[mouse.path] = 0;
        backoff.next_attempt.erase(mouse.path);
      }

      if (mice.empty() && workers.empty()) {
        if (!announced_wait) {
          output << "Waiting for a conventional wheel mouse...\n";
          output.flush();
          announced_wait = true;
        }
      } else {
        announced_wait = false;
      }

      int sleep_ms = config->reconnect_ms;
      if (!any_eligible_now && any_pending_backoff) {
        const auto remaining = min_backoff_remaining.count();
        if (remaining > 0) sleep_ms = std::max(sleep_ms, static_cast<int>(std::min<long>(remaining, kMaxBackoffMs)));
      }
      interruptible_sleep(sleep_ms);
    }
    reap_workers(workers, output, &backoff, config->reconnect_ms, true);
    output << "ScrollShift daemon stopped.\n";
    output.flush();
    return 0;
  }

  DeviceMatchState last_state = DeviceMatchState::Unique;
  while (!relay_stop_requested()) {
    const auto diagnosis = diagnose_device_match(discover_input_devices(input_root, udev_data_root), config->device);
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
