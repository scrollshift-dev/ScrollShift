#pragma once

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "scrollshift/input.hpp"

namespace scrollshift {

struct DeviceSelector {
  std::uint16_t vendor{};
  std::uint16_t product{};
  std::string name;
};

struct DaemonConfig {
  bool auto_discover{true};
  bool allow_non_pointer_wheel{false};
  DeviceSelector device;
  std::string profile{"balanced"};
  int reconnect_ms{1000};
};

std::optional<DaemonConfig> parse_config(std::istream& input, std::string& error);
std::optional<DaemonConfig> load_config(const std::filesystem::path& path, std::string& error);
std::string serialize_config(const DaemonConfig& config);
DaemonConfig config_for_device(const DeviceInfo& device, const std::string& profile = "balanced");
bool matches_selector(const DeviceInfo& device, const DeviceSelector& selector);
bool is_capture_candidate(const DeviceInfo& device, bool allow_non_pointer_wheel = false);
bool is_automatic_mouse_candidate(const DeviceInfo& device);
std::vector<DeviceInfo> automatic_mouse_candidates(const std::vector<DeviceInfo>& devices);
std::vector<DeviceInfo> matching_capture_devices(const std::vector<DeviceInfo>& devices,
                                                 const DeviceSelector& selector,
                                                 bool allow_non_pointer_wheel = false);

enum class DeviceMatchState { Missing, Unique, Ambiguous };
struct DeviceMatchDiagnosis {
  DeviceMatchState state{DeviceMatchState::Missing};
  std::vector<DeviceInfo> matches;
};
DeviceMatchDiagnosis diagnose_device_match(const std::vector<DeviceInfo>& devices,
                                           const DeviceSelector& selector,
                                           bool allow_non_pointer_wheel = false);
int write_config_for_device(const std::filesystem::path& device,
                            const std::filesystem::path& config_path,
                            const std::string& profile, std::ostream& output,
                            bool allow_non_pointer_wheel = false);

}  // namespace scrollshift
