#include "scrollshift/daemon.hpp"

#include <algorithm>
#include <chrono>
#include <thread>

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
}

int run_doctor(const std::filesystem::path& config_path, std::ostream& output,
               const std::filesystem::path& input_root) {
  std::string error;
  const auto config = load_config(config_path, error);
  if (!config) {
    output << "scrollshift: configuration error: " << error << '\n';
    return 2;
  }

  const auto diagnosis = diagnose_device_match(discover_input_devices(input_root), config->device);
  output << "Config: " << config_path << '\n'
         << "Profile: " << config->profile << '\n'
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

  DeviceMatchState last_state = DeviceMatchState::Unique;  // force first state message
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
