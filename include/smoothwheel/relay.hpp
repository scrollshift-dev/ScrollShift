#pragma once
#include <filesystem>
#include <iosfwd>

namespace smoothwheel {
int run_pointer_relay(const std::filesystem::path& device, int seconds, int delay_seconds,
                      std::ostream& output,
                      const std::filesystem::path& uinput_path = "/dev/uinput");
}
