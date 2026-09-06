#include "scrollshift/config.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <linux/input.h>
#include <sstream>

#include "scrollshift/transform.hpp"

namespace scrollshift {
namespace {
std::string trim(std::string value) {
  const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char c) { return std::isspace(c) != 0; });
  const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) { return std::isspace(c) != 0; }).base();
  if (first >= last) return {};
  return std::string(first, last);
}

bool parse_u16(std::string value, std::uint16_t& out) {
  value = trim(std::move(value));
  int base = 10;
  if (value.size() > 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
    value.erase(0, 2);
    base = 16;
  }
  if (value.empty()) return false;
  unsigned parsed = 0;
  const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), parsed, base);
  if (ec != std::errc{} || ptr != value.data() + value.size() || parsed > 0xffffU) return false;
  out = static_cast<std::uint16_t>(parsed);
  return true;
}

bool valid_profile(const std::string& profile) { return find_acceleration_profile(profile) != nullptr; }
}

std::optional<DaemonConfig> parse_config(std::istream& input, std::string& error) {
  DaemonConfig config;
  bool have_vendor = false;
  bool have_product = false;
  std::string line;
  int line_no = 0;
  while (std::getline(input, line)) {
    ++line_no;
    const auto hash = line.find('#');
    if (hash != std::string::npos) line.erase(hash);
    line = trim(std::move(line));
    if (line.empty()) continue;
    const auto eq = line.find('=');
    if (eq == std::string::npos) {
      error = "line " + std::to_string(line_no) + ": expected key = value";
      return std::nullopt;
    }
    const auto key = trim(line.substr(0, eq));
    const auto value = trim(line.substr(eq + 1));
    if (key == "mode") {
      if (value == "auto") config.auto_discover = true;
      else if (value == "device") config.auto_discover = false;
      else { error = "line " + std::to_string(line_no) + ": mode must be auto or device"; return std::nullopt; }
    } else if (key == "device_vendor") {
      config.auto_discover = false;
      if (!parse_u16(value, config.device.vendor)) { error = "line " + std::to_string(line_no) + ": invalid device_vendor"; return std::nullopt; }
      have_vendor = true;
    } else if (key == "device_product") {
      if (!parse_u16(value, config.device.product)) { error = "line " + std::to_string(line_no) + ": invalid device_product"; return std::nullopt; }
      have_product = true;
    } else if (key == "device_name") {
      if (value.empty()) { error = "line " + std::to_string(line_no) + ": device_name cannot be empty"; return std::nullopt; }
      config.device.name = value;
    } else if (key == "profile") {
      if (!valid_profile(value)) { error = "line " + std::to_string(line_no) + ": unknown profile '" + value + "'"; return std::nullopt; }
      config.profile = value;
    } else if (key == "reconnect_ms") {
      try {
        std::size_t used = 0;
        const int parsed = std::stoi(value, &used);
        if (used != value.size() || parsed < 100 || parsed > 30000) throw std::invalid_argument("range");
        config.reconnect_ms = parsed;
      } catch (...) {
        error = "line " + std::to_string(line_no) + ": reconnect_ms must be 100..30000";
        return std::nullopt;
      }
    } else {
      error = "line " + std::to_string(line_no) + ": unknown key '" + key + "'";
      return std::nullopt;
    }
  }
  if (!config.auto_discover && (!have_vendor || !have_product)) {
    error = "device_vendor and device_product are required in device mode";
    return std::nullopt;
  }
  return config;
}

std::optional<DaemonConfig> load_config(const std::filesystem::path& path, std::string& error) {
  std::ifstream input(path);
  if (!input) { error = "cannot open " + path.string(); return std::nullopt; }
  return parse_config(input, error);
}

std::string serialize_config(const DaemonConfig& config) {
  std::ostringstream out;
  out << "# ScrollShift configuration\n";
  if (config.auto_discover) {
    out << "# Automatically discovers conventional wheel mice and ignores touchpads/touchscreens.\n"
        << "mode = auto\n";
  } else {
    out << "# Manual device override; no /dev/input/eventN path is persisted.\n"
        << "mode = device\n"
        << "device_vendor = 0x" << std::hex << std::setw(4) << std::setfill('0') << config.device.vendor << '\n'
        << "device_product = 0x" << std::setw(4) << config.device.product << std::dec << '\n';
    if (!config.device.name.empty()) out << "device_name = " << config.device.name << '\n';
  }
  out << "profile = " << config.profile << '\n'
      << "reconnect_ms = " << config.reconnect_ms << '\n';
  return out.str();
}

DaemonConfig config_for_device(const DeviceInfo& device, const std::string& profile) {
  DaemonConfig config;
  config.auto_discover = false;
  config.device.vendor = device.vendor;
  config.device.product = device.product;
  config.device.name = device.name;
  config.profile = profile;
  return config;
}

bool matches_selector(const DeviceInfo& device, const DeviceSelector& selector) {
  return device.vendor == selector.vendor && device.product == selector.product &&
         (selector.name.empty() || device.name == selector.name);
}

bool is_capture_candidate(const DeviceInfo& device) {
  if (!device.relative_pointer) return false;
  if (!(device.wheel || device.hi_res_wheel || device.horizontal_wheel || device.hi_res_horizontal_wheel)) return false;
  if (device.vendor == 0x5357) return false;
  if (device.name.starts_with("ScrollShift ")) return false;
  return true;
}

bool is_automatic_mouse_candidate(const DeviceInfo& device) {
  if (!is_capture_candidate(device)) return false;
  // Known non-mouse input classes are never eligible, regardless of metadata.
  if (device.is_touchpad || device.is_touchscreen || device.is_joystick || device.is_tablet ||
      device.is_tablet_pad) return false;
  if (device.udev_classified) {
    // Trust explicit udev/libinput-style classification when present. Mouse-like
    // classes (mouse, pointing stick) are acceptable; everything else is refused
    // rather than falling through to the capability fallback. A touchpad/touchscreen
    // tagged alongside ID_INPUT_MOUSE is excluded above.
    return device.is_mouse || device.is_pointingstick;
  }
  // Capability-only fallback for systems where udev metadata is genuinely
  // unavailable or incomplete. Be conservative: prefer a false negative on
  // unusual hardware over grabbing a non-mouse device. Users retain the
  // explicit `configure DEVICE` override.
  if (device.bus == BUS_VIRTUAL) return false;          // unrelated virtual devices
  if (device.abs_axes) return false;                    // tablet/touchpad/joystick-style absolute motion
  return device.has_rel_x && device.has_rel_y &&        // full relative pointer signature
         (device.wheel || device.hi_res_wheel) &&       // vertical wheel
         device.mouse_button;                           // conventional mouse buttons
}

std::vector<DeviceInfo> automatic_mouse_candidates(const std::vector<DeviceInfo>& devices) {
  std::vector<DeviceInfo> matches;
  for (const auto& device : devices) if (is_automatic_mouse_candidate(device)) matches.push_back(device);
  return matches;
}

std::vector<DeviceInfo> matching_capture_devices(const std::vector<DeviceInfo>& devices,
                                                 const DeviceSelector& selector) {
  std::vector<DeviceInfo> matches;
  for (const auto& device : devices)
    if (is_capture_candidate(device) && matches_selector(device, selector)) matches.push_back(device);
  return matches;
}

DeviceMatchDiagnosis diagnose_device_match(const std::vector<DeviceInfo>& devices,
                                           const DeviceSelector& selector) {
  DeviceMatchDiagnosis diagnosis;
  diagnosis.matches = matching_capture_devices(devices, selector);
  if (diagnosis.matches.empty()) diagnosis.state = DeviceMatchState::Missing;
  else if (diagnosis.matches.size() == 1) diagnosis.state = DeviceMatchState::Unique;
  else diagnosis.state = DeviceMatchState::Ambiguous;
  return diagnosis;
}

int write_config_for_device(const std::filesystem::path& device_path,
                            const std::filesystem::path& config_path,
                            const std::string& profile, std::ostream& output) {
  if (!valid_profile(profile)) { output << "scrollshift: unknown acceleration profile: " << profile << '\n'; return 2; }
  const auto device = inspect_input_device(device_path);
  if (!device) { output << "scrollshift: cannot inspect " << device_path << '\n'; return 1; }
  if (!is_capture_candidate(*device)) { output << "scrollshift: " << device_path << " is not a supported wheel pointer\n"; return 1; }
  std::error_code ec;
  if (config_path.has_parent_path()) std::filesystem::create_directories(config_path.parent_path(), ec);
  if (ec) { output << "scrollshift: cannot create " << config_path.parent_path() << ": " << ec.message() << '\n'; return 1; }
  std::ofstream out(config_path, std::ios::trunc);
  if (!out) { output << "scrollshift: cannot write " << config_path << '\n'; return 1; }
  out << serialize_config(config_for_device(*device, profile));
  out.close();
  if (!out) { output << "scrollshift: failed while writing " << config_path << '\n'; return 1; }
  output << "Configured " << device->name << " (" << std::hex << std::setfill('0') << std::setw(4)
         << device->vendor << ':' << std::setw(4) << device->product << std::dec << ")\n"
         << "Profile: " << profile << "\nConfig: " << config_path << '\n';
  return 0;
}

}  // namespace scrollshift
