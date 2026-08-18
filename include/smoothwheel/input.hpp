#pragma once

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

namespace smoothwheel {

struct DeviceInfo {
  std::filesystem::path path;
  std::string name;
  std::string phys;
  std::uint16_t bus = 0;
  std::uint16_t vendor = 0;
  std::uint16_t product = 0;
  std::uint16_t version = 0;
  bool readable = false;
  bool relative_pointer = false;
  bool wheel = false;
  bool horizontal_wheel = false;
  bool hi_res_wheel = false;
  bool hi_res_horizontal_wheel = false;
};

struct RecordedEvent {
  std::int64_t sec = 0;
  std::int64_t usec = 0;
  std::uint16_t type = 0;
  std::uint16_t code = 0;
  std::int32_t value = 0;
};

std::vector<DeviceInfo> discover_input_devices(const std::filesystem::path& root = "/dev/input");
std::optional<DeviceInfo> inspect_input_device(const std::filesystem::path& path);
std::string event_type_name(std::uint16_t type);
std::string event_code_name(std::uint16_t type, std::uint16_t code);
std::string describe_device(const DeviceInfo& device);
std::string format_event(const RecordedEvent& event);
std::string serialize_event(const RecordedEvent& event);
std::optional<RecordedEvent> parse_recorded_event(const std::string& line);
int monitor_input_device(const std::filesystem::path& path, std::ostream& out,
                         std::ostream* record, bool wheel_only);

}  // namespace smoothwheel
