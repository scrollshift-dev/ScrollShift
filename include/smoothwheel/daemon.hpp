#pragma once
#include <filesystem>
#include <iosfwd>

namespace smoothwheel {
int run_daemon(const std::filesystem::path& config_path, std::ostream& output,
               const std::filesystem::path& input_root = "/dev/input",
               const std::filesystem::path& uinput_path = "/dev/uinput");
}
