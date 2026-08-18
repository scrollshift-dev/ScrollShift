#pragma once
#include <filesystem>
#include <iosfwd>
#include <string>

namespace smoothwheel {
void install_relay_signal_handlers();
void reset_relay_stop_request();
bool relay_stop_requested();

int run_pointer_relay(const std::filesystem::path& device, int seconds, int delay_seconds,
                      std::ostream& output,
                      const std::filesystem::path& uinput_path = "/dev/uinput",
                      bool manage_signals = true);
int run_accelerated_relay(const std::filesystem::path& device, const std::string& profile,
                          int seconds, int delay_seconds, std::ostream& output,
                          const std::filesystem::path& uinput_path = "/dev/uinput",
                          bool manage_signals = true);
}
