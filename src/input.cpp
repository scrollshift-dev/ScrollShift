#include "smoothwheel/input.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <cctype>
#include <fcntl.h>
#include <iomanip>
#include <linux/input.h>
#include <sstream>
#include <sys/ioctl.h>
#include <unistd.h>

namespace smoothwheel {
namespace {
constexpr std::size_t kBitsPerWord = sizeof(unsigned long) * 8;

template <std::size_t N>
bool bit_set(const std::array<unsigned long, N>& bits, unsigned bit) {
  const auto word = bit / kBitsPerWord;
  return word < N && (bits[word] & (1UL << (bit % kBitsPerWord))) != 0;
}

std::string ioctl_string(int fd, unsigned long request, std::size_t size) {
  std::vector<char> buffer(size, '\0');
  if (::ioctl(fd, request, buffer.data()) < 0) return {};
  return std::string(buffer.data());
}

std::string trim_device_name(std::string value) {
  const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char c) { return std::isspace(c) != 0; });
  const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) { return std::isspace(c) != 0; }).base();
  if (first >= last) return {};
  return std::string(first, last);
}

bool wheel_code(std::uint16_t code) {
  return code == REL_WHEEL || code == REL_HWHEEL || code == REL_WHEEL_HI_RES ||
         code == REL_HWHEEL_HI_RES;
}
}  // namespace

std::optional<DeviceInfo> inspect_input_device(const std::filesystem::path& path) {
  const int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0) return std::nullopt;

  DeviceInfo d;
  d.path = path;
  d.readable = true;
  d.name = trim_device_name(ioctl_string(fd, EVIOCGNAME(256), 256));
  d.phys = ioctl_string(fd, EVIOCGPHYS(256), 256);

  input_id id{};
  if (::ioctl(fd, EVIOCGID, &id) == 0) {
    d.bus = id.bustype; d.vendor = id.vendor; d.product = id.product; d.version = id.version;
  }

  std::array<unsigned long, (EV_MAX / kBitsPerWord) + 2> ev_bits{};
  std::array<unsigned long, (REL_MAX / kBitsPerWord) + 2> rel_bits{};
  if (::ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits.data()) >= 0 && bit_set(ev_bits, EV_REL)) {
    if (::ioctl(fd, EVIOCGBIT(EV_REL, sizeof(rel_bits)), rel_bits.data()) >= 0) {
      d.relative_pointer = bit_set(rel_bits, REL_X) || bit_set(rel_bits, REL_Y);
      d.wheel = bit_set(rel_bits, REL_WHEEL);
      d.horizontal_wheel = bit_set(rel_bits, REL_HWHEEL);
      d.hi_res_wheel = bit_set(rel_bits, REL_WHEEL_HI_RES);
      d.hi_res_horizontal_wheel = bit_set(rel_bits, REL_HWHEEL_HI_RES);
    }
  }
  ::close(fd);
  return d;
}

std::vector<DeviceInfo> discover_input_devices(const std::filesystem::path& root) {
  std::vector<DeviceInfo> devices;
  std::error_code ec;
  if (!std::filesystem::exists(root, ec)) return devices;
  for (const auto& entry : std::filesystem::directory_iterator(root, ec)) {
    if (ec) break;
    const auto filename = entry.path().filename().string();
    if (!filename.starts_with("event")) continue;
    if (auto info = inspect_input_device(entry.path())) devices.push_back(std::move(*info));
  }
  std::sort(devices.begin(), devices.end(), [](const auto& a, const auto& b) { return a.path < b.path; });
  return devices;
}

std::string event_type_name(std::uint16_t type) {
  switch (type) { case EV_SYN: return "EV_SYN"; case EV_KEY: return "EV_KEY"; case EV_REL: return "EV_REL"; case EV_ABS: return "EV_ABS"; default: return "EV_" + std::to_string(type); }
}

std::string event_code_name(std::uint16_t type, std::uint16_t code) {
  if (type == EV_SYN && code == SYN_REPORT) return "SYN_REPORT";
  if (type == EV_REL) {
    switch (code) { case REL_X: return "REL_X"; case REL_Y: return "REL_Y"; case REL_WHEEL: return "REL_WHEEL"; case REL_HWHEEL: return "REL_HWHEEL"; case REL_WHEEL_HI_RES: return "REL_WHEEL_HI_RES"; case REL_HWHEEL_HI_RES: return "REL_HWHEEL_HI_RES"; default: break; }
  }
  return std::to_string(code);
}

std::string describe_device(const DeviceInfo& d) {
  std::ostringstream s;
  s << d.path << "  " << (d.name.empty() ? "(unnamed)" : d.name) << '\n'
    << "  id " << std::hex << std::setfill('0') << std::setw(4) << d.vendor << ':' << std::setw(4) << d.product << std::dec
    << "  relative=" << (d.relative_pointer ? "yes" : "no")
    << "  wheel=" << (d.wheel ? "yes" : "no")
    << "  hi-res=" << (d.hi_res_wheel ? "yes" : "no")
    << "  h-wheel=" << (d.horizontal_wheel ? "yes" : "no")
    << "  h-hi-res=" << (d.hi_res_horizontal_wheel ? "yes" : "no");
  if (!d.phys.empty()) s << "\n  phys " << d.phys;
  return s.str();
}

std::string format_event(const RecordedEvent& e) {
  std::ostringstream s;
  s << e.sec << '.' << std::setw(6) << std::setfill('0') << e.usec << "  "
    << event_type_name(e.type) << ' ' << event_code_name(e.type, e.code) << "  " << e.value;
  return s.str();
}

std::string serialize_event(const RecordedEvent& e) {
  std::ostringstream s; s << e.sec << ' ' << e.usec << ' ' << e.type << ' ' << e.code << ' ' << e.value; return s.str();
}

std::optional<RecordedEvent> parse_recorded_event(const std::string& line) {
  if (line.empty() || line[0] == '#') return std::nullopt;
  RecordedEvent e; std::istringstream s(line);
  long long sec, usec; unsigned type, code; long long value;
  if (!(s >> sec >> usec >> type >> code >> value)) return std::nullopt;
  std::string trailing; if (s >> trailing) return std::nullopt;
  if (type > 0xffff || code > 0xffff || value < INT32_MIN || value > INT32_MAX) return std::nullopt;
  e.sec = sec; e.usec = usec; e.type = static_cast<std::uint16_t>(type); e.code = static_cast<std::uint16_t>(code); e.value = static_cast<std::int32_t>(value); return e;
}

std::vector<RecordedEvent> load_recorded_trace(std::istream& input) {
  std::vector<RecordedEvent> events;
  std::string line;
  while (std::getline(input, line)) {
    if (auto event = parse_recorded_event(line)) events.push_back(*event);
  }
  return events;
}

TraceSummary summarize_trace(const std::vector<RecordedEvent>& events) {
  TraceSummary summary;
  summary.events = events.size();
  for (const auto& event : events) {
    if (event.type == EV_SYN && event.code == SYN_REPORT) ++summary.reports;
    if (event.type != EV_REL) continue;
    switch (event.code) {
      case REL_WHEEL:
        ++summary.vertical_low_res;
        summary.vertical_low_res_total += event.value;
        break;
      case REL_WHEEL_HI_RES:
        ++summary.vertical_hi_res;
        summary.vertical_hi_res_total += event.value;
        break;
      case REL_HWHEEL:
        ++summary.horizontal_low_res;
        summary.horizontal_low_res_total += event.value;
        break;
      case REL_HWHEEL_HI_RES:
        ++summary.horizontal_hi_res;
        summary.horizontal_hi_res_total += event.value;
        break;
      default:
        break;
    }
  }
  return summary;
}

int monitor_input_device(const std::filesystem::path& path, std::ostream& out, std::ostream* record, bool wheel_only) {
  const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0) { out << "smoothwheel: cannot open " << path << ": " << std::strerror(errno) << '\n'; return 1; }
  if (record) *record << "# smoothwheel-event-trace v1\n# sec usec type code value\n";
  out << "Reading " << path << " read-only. Press Ctrl-C to stop.\n";
  input_event raw{};
  while (true) {
    const auto n = ::read(fd, &raw, sizeof(raw));
    if (n == static_cast<ssize_t>(sizeof(raw))) {
      RecordedEvent e{raw.time.tv_sec, raw.time.tv_usec, raw.type, raw.code, raw.value};
      const bool visible = !wheel_only || (e.type == EV_REL && wheel_code(e.code)) || (e.type == EV_SYN && e.code == SYN_REPORT);
      if (visible) out << format_event(e) << '\n';
      if (record) *record << serialize_event(e) << '\n';
      continue;
    }
    if (n < 0 && errno == EINTR) continue;
    if (n < 0) out << "smoothwheel: read failed: " << std::strerror(errno) << '\n';
    else out << "smoothwheel: input device closed or returned a partial event\n";
    ::close(fd); return 1;
  }
}

}  // namespace smoothwheel
