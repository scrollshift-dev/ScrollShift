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
  DeviceSelector device;
  std::string profile{"balanced"};
  int reconnect_ms{1000};
};

std::optional<DaemonConfig> parse_config(std::istream& input, std::string& error);
std::optional<DaemonConfig> load_config(const std::filesystem::path& path, std::string& error);
std::string serialize_config(const DaemonConfig& config);
DaemonConfig config_for_device(const DeviceInfo& device, const std::string& profile = "balanced");
bool matches_selector(const DeviceInfo& device, const DeviceSelector& selector);
bool is_capture_candidate(const DeviceInfo& device);
std::vector<DeviceInfo> matching_capture_devices(const std::vector<DeviceInfo>& devices,
                                                 const DeviceSelector& selector);

enum class DeviceMatchState { Missing, Unique, Ambiguous };
struct DeviceMatchDiagnosis {
  DeviceMatchState state{DeviceMatchState::Missing};
  std::vector<DeviceInfo> matches;
};
DeviceMatchDiagnosis diagnose_device_match(const std::vector<DeviceInfo>& devices,
                                           const DeviceSelector& selector);
int write_config_for_device(const std::filesystem::path& device,
                            const std::filesystem::path& config_path,
                            const std::string& profile, std::ostream& output);

}  // namespace scrollshift
