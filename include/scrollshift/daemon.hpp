#pragma once
#include <filesystem>
#include <iosfwd>
#include <string>

namespace scrollshift {
int run_doctor(const std::filesystem::path& config_path, std::ostream& output,
               const std::filesystem::path& input_root = "/dev/input",
               const std::filesystem::path& udev_data_root = "/run/udev/data");
int run_daemon(const std::filesystem::path& config_path, std::ostream& output,
               const std::filesystem::path& input_root = "/dev/input",
               const std::filesystem::path& uinput_path = "/dev/uinput",
               const std::filesystem::path& udev_data_root = "/run/udev/data");
int relay_backoff_ms(int consecutive_failures, int base_ms, int max_ms = 30000);
}
