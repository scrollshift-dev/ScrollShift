#pragma once

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

namespace scrollshift {

struct DeviceInfo {
  std::filesystem::path path;
  std::string name;
  std::string phys;
  std::uint16_t bus{};
  std::uint16_t vendor{};
  std::uint16_t product{};
  std::uint16_t version{};
  bool readable{};
  bool relative_pointer{};
  bool wheel{};
  bool horizontal_wheel{};
  bool hi_res_wheel{};
  bool hi_res_horizontal_wheel{};
  bool udev_classified{};
  bool is_mouse{};
  bool is_touchpad{};
  bool is_touchscreen{};
};

struct RecordedEvent {
  std::int64_t sec{};
  std::int64_t usec{};
  std::uint16_t type{};
  std::uint16_t code{};
  std::int32_t value{};
};

struct TraceSummary {
  std::size_t events{};
  std::size_t reports{};
  std::size_t vertical_low_res{};
  std::size_t vertical_hi_res{};
  std::size_t horizontal_low_res{};
  std::size_t horizontal_hi_res{};
  std::int64_t vertical_low_res_total{};
  std::int64_t vertical_hi_res_total{};
  std::int64_t horizontal_low_res_total{};
  std::int64_t horizontal_hi_res_total{};
};

std::optional<DeviceInfo> inspect_input_device(const std::filesystem::path& path);
std::vector<DeviceInfo> discover_input_devices(const std::filesystem::path& root = "/dev/input");
std::string describe_device(const DeviceInfo& device);
std::string event_type_name(std::uint16_t type);
std::string event_code_name(std::uint16_t type, std::uint16_t code);
std::string format_event(const RecordedEvent& event);
std::string serialize_event(const RecordedEvent& event);
std::optional<RecordedEvent> parse_recorded_event(const std::string& line);
std::vector<RecordedEvent> load_recorded_trace(std::istream& input);
TraceSummary summarize_trace(const std::vector<RecordedEvent>& events);
int monitor_input_device(const std::filesystem::path& path, std::ostream& output,
                         std::ostream* record_stream, bool wheel_only);

}  // namespace scrollshift
