#include "smoothwheel/experiment.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/ioctl.h>
#include <thread>
#include <unistd.h>

namespace smoothwheel {
namespace {

void checked_ioctl(int fd, unsigned long request, int value, const char* what) {
  if (::ioctl(fd, request, value) < 0) {
    throw std::runtime_error(std::string("uinput ") + what + ": " + std::strerror(errno));
  }
}

void checked_ioctl_ptr(int fd, unsigned long request, void* value, const char* what) {
  if (::ioctl(fd, request, value) < 0) {
    throw std::runtime_error(std::string("uinput ") + what + ": " + std::strerror(errno));
  }
}

std::vector<std::int32_t> split_exactly(int total, int steps) {
  std::vector<std::int32_t> values;
  values.reserve(static_cast<std::size_t>(steps));
  const int sign = total < 0 ? -1 : 1;
  const int magnitude = std::abs(total);
  const int base = magnitude / steps;
  const int remainder = magnitude % steps;
  for (int i = 0; i < steps; ++i) {
    values.push_back(sign * (base + (i < remainder ? 1 : 0)));
  }
  return values;
}

}  // namespace

std::vector<ExperimentPreset> experiment_presets() {
  return {
      {"detent", "one conventional 120-v120 detent in a single report", 1, 0, true},
      {"fine8", "one detent divided into 8 high-resolution reports", 8, 12, true},
      {"fine16", "one detent divided into 16 high-resolution reports", 16, 8, true},
      {"fine24", "one detent divided into 24 high-resolution reports", 24, 6, true},
      {"hires-only16", "16 high-resolution reports without a legacy REL_WHEEL detent", 16, 8, false},
  };
}

const ExperimentPreset* find_experiment_preset(const std::string& name) {
  static const auto presets = experiment_presets();
  const auto it = std::find_if(presets.begin(), presets.end(), [&](const auto& preset) {
    return preset.name == name;
  });
  return it == presets.end() ? nullptr : &*it;
}

std::vector<PlannedScrollPacket> plan_scroll(const ExperimentPreset& preset, int direction) {
  if (direction != -1 && direction != 1) throw std::invalid_argument("direction must be -1 or 1");
  if (preset.steps <= 0) throw std::invalid_argument("preset steps must be positive");
  const auto hi_res_values = split_exactly(direction * 120, preset.steps);
  std::vector<PlannedScrollPacket> packets;
  packets.reserve(hi_res_values.size());
  for (std::size_t i = 0; i < hi_res_values.size(); ++i) {
    const bool last = i + 1 == hi_res_values.size();
    packets.push_back({hi_res_values[i], (preset.emit_legacy_detent && last) ? direction : 0,
                       last ? 0 : preset.interval_ms});
  }
  return packets;
}

std::string describe_plan(const ExperimentPreset& preset, ScrollAxis axis, int direction,
                          const std::vector<PlannedScrollPacket>& packets) {
  std::ostringstream out;
  out << "preset=" << preset.name << " axis=" << (axis == ScrollAxis::Vertical ? "vertical" : "horizontal")
      << " direction=" << (direction > 0 ? "+1" : "-1") << " packets=" << packets.size() << '\n';
  std::int64_t hi_res_total = 0;
  std::int64_t low_res_total = 0;
  for (std::size_t i = 0; i < packets.size(); ++i) {
    const auto& packet = packets[i];
    hi_res_total += packet.hi_res_value;
    low_res_total += packet.low_res_value;
    out << std::setw(2) << (i + 1) << ": hi-res=" << std::setw(4) << packet.hi_res_value
        << " low-res=" << std::setw(2) << packet.low_res_value
        << " next-delay-ms=" << packet.delay_ms << '\n';
  }
  out << "totals: hi-res=" << hi_res_total << " low-res=" << low_res_total << '\n';
  return out.str();
}

VirtualWheel::VirtualWheel(const std::filesystem::path& uinput_path) {
  fd_ = ::open(uinput_path.c_str(), O_WRONLY | O_NONBLOCK | O_CLOEXEC);
  if (fd_ < 0) {
    throw std::runtime_error("cannot open " + uinput_path.string() + ": " + std::strerror(errno));
  }

  try {
    checked_ioctl(fd_, UI_SET_EVBIT, EV_KEY, "enable EV_KEY");
    checked_ioctl(fd_, UI_SET_KEYBIT, BTN_LEFT, "enable BTN_LEFT");
    checked_ioctl(fd_, UI_SET_EVBIT, EV_REL, "enable EV_REL");
    checked_ioctl(fd_, UI_SET_RELBIT, REL_X, "enable REL_X");
    checked_ioctl(fd_, UI_SET_RELBIT, REL_Y, "enable REL_Y");
    checked_ioctl(fd_, UI_SET_RELBIT, REL_WHEEL, "enable REL_WHEEL");
    checked_ioctl(fd_, UI_SET_RELBIT, REL_WHEEL_HI_RES, "enable REL_WHEEL_HI_RES");
    checked_ioctl(fd_, UI_SET_RELBIT, REL_HWHEEL, "enable REL_HWHEEL");
    checked_ioctl(fd_, UI_SET_RELBIT, REL_HWHEEL_HI_RES, "enable REL_HWHEEL_HI_RES");

    uinput_setup setup{};
    setup.id.bustype = BUS_VIRTUAL;
    setup.id.vendor = 0x5357;   // 'SW' - development-only virtual identity.
    setup.id.product = 0x0001;
    setup.id.version = 1;
    std::strncpy(setup.name, "SmoothWheel CP2 Virtual Wheel", UINPUT_MAX_NAME_SIZE - 1);
    checked_ioctl_ptr(fd_, UI_DEV_SETUP, &setup, "UI_DEV_SETUP");
    checked_ioctl(fd_, UI_DEV_CREATE, 0, "UI_DEV_CREATE");
    created_ = true;
  } catch (...) {
    ::close(fd_);
    fd_ = -1;
    throw;
  }
}

VirtualWheel::~VirtualWheel() {
  if (fd_ >= 0) {
    if (created_) ::ioctl(fd_, UI_DEV_DESTROY);
    ::close(fd_);
  }
}

void VirtualWheel::emit(std::uint16_t type, std::uint16_t code, std::int32_t value) {
  input_event event{};
  event.type = type;
  event.code = code;
  event.value = value;
  const auto written = ::write(fd_, &event, sizeof(event));
  if (written != static_cast<ssize_t>(sizeof(event))) {
    throw std::runtime_error(std::string("uinput write failed: ") + std::strerror(errno));
  }
}

void VirtualWheel::emit_packet(ScrollAxis axis, const PlannedScrollPacket& packet) {
  const auto hi_res_code = axis == ScrollAxis::Vertical ? REL_WHEEL_HI_RES : REL_HWHEEL_HI_RES;
  const auto low_res_code = axis == ScrollAxis::Vertical ? REL_WHEEL : REL_HWHEEL;
  // Match the ordering observed on the captured physical mouse when a
  // legacy detent is present: low-resolution event first, then hi-res.
  if (packet.low_res_value != 0) emit(EV_REL, low_res_code, packet.low_res_value);
  if (packet.hi_res_value != 0) emit(EV_REL, hi_res_code, packet.hi_res_value);
  emit(EV_SYN, SYN_REPORT, 0);
}

int run_virtual_scroll_experiment(const ExperimentPreset& preset, ScrollAxis axis,
                                  int direction, int initial_delay_seconds,
                                  const std::filesystem::path& uinput_path) {
  try {
    const auto packets = plan_scroll(preset, direction);
    VirtualWheel wheel(uinput_path);
    std::cout << "Created temporary SmoothWheel virtual pointer.\n"
              << "No physical device is grabbed; your real mouse remains untouched.\n"
              << "Move the pointer over the application you want to test.\n";
    for (int remaining = initial_delay_seconds; remaining > 0; --remaining) {
      std::cout << "Emitting in " << remaining << "...\n" << std::flush;
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    for (const auto& packet : packets) {
      wheel.emit_packet(axis, packet);
      if (packet.delay_ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(packet.delay_ms));
    }
    std::cout << "Experiment complete. Virtual device will now be removed.\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "smoothwheel: experiment failed: " << error.what() << '\n';
    return 1;
  }
}

}  // namespace smoothwheel
