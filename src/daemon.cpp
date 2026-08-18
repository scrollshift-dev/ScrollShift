#include "smoothwheel/daemon.hpp"

#include <chrono>
#include <thread>

#include "smoothwheel/config.hpp"
#include "smoothwheel/input.hpp"
#include "smoothwheel/relay.hpp"

namespace smoothwheel {
int run_daemon(const std::filesystem::path& config_path, std::ostream& output,
               const std::filesystem::path& input_root,
               const std::filesystem::path& uinput_path) {
  std::string error;
  const auto config = load_config(config_path, error);
  if (!config) { output << "smoothwheel: configuration error: " << error << '\n'; return 2; }

  install_relay_signal_handlers();
  reset_relay_stop_request();
  output << "SmoothWheel daemon starting with profile '" << config->profile << "'.\n";
  output.flush();

  while (!relay_stop_requested()) {
    const auto matches = matching_capture_devices(discover_input_devices(input_root), config->device);
    if (matches.empty()) {
      output << "Waiting for configured mouse...\n";
      output.flush();
      std::this_thread::sleep_for(std::chrono::milliseconds(config->reconnect_ms));
      continue;
    }
    if (matches.size() > 1) {
      output << "smoothwheel: configured device identity is ambiguous (" << matches.size()
             << " matching event nodes); refusing to grab any of them.\n";
      output.flush();
      std::this_thread::sleep_for(std::chrono::milliseconds(config->reconnect_ms));
      continue;
    }

    output << "Attaching to " << matches.front().path << " (" << matches.front().name << ")\n";
    output.flush();
    const int result = run_accelerated_relay(matches.front().path, config->profile, 0, 0, output, uinput_path,
                                             false);
    if (relay_stop_requested()) break;
    output << "Input session ended (code " << result << "); rediscovering device.\n";
    output.flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(config->reconnect_ms));
  }

  output << "SmoothWheel daemon stopped.\n";
  return 0;
}
}  // namespace smoothwheel
