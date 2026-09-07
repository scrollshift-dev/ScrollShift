#include "scrollshift/config.hpp"
#include <cassert>
#include <sstream>
#include <iostream>

int main() {
  using namespace scrollshift;
  std::string error;
  std::istringstream good("device_vendor = 0x3151\ndevice_product = 0x402d\ndevice_name = 2.4G Wireless Mouse\nprofile = balanced\nreconnect_ms = 750\n");
  auto config = parse_config(good, error);
  assert(config);
  assert(config->device.vendor == 0x3151);
  assert(config->device.product == 0x402d);
  assert(config->device.name == "2.4G Wireless Mouse");
  assert(config->profile == "balanced");
  assert(config->reconnect_ms == 750);

  DeviceInfo real;
  real.vendor=0x3151; real.product=0x402d; real.name="2.4G Wireless Mouse";
  real.relative_pointer=true; real.has_rel_x=true; real.has_rel_y=true;
  real.wheel=true; real.hi_res_wheel=true; real.mouse_button=true;
  assert(matches_selector(real, config->device));
  assert(is_capture_candidate(real));
  assert(is_automatic_mouse_candidate(real));

  DeviceInfo classified_mouse = real;
  classified_mouse.udev_classified = true; classified_mouse.is_mouse = true;
  assert(is_automatic_mouse_candidate(classified_mouse));
  DeviceInfo touchpad = real;
  touchpad.udev_classified = true; touchpad.is_touchpad = true;
  assert(!is_automatic_mouse_candidate(touchpad));
  DeviceInfo touchscreen = real;
  touchscreen.udev_classified = true; touchscreen.is_touchscreen = true;
  assert(!is_automatic_mouse_candidate(touchscreen));
  DeviceInfo classified_other = real;
  classified_other.udev_classified = true;
  assert(!is_automatic_mouse_candidate(classified_other));
  const auto auto_mice = automatic_mouse_candidates({touchpad, touchscreen, classified_other, classified_mouse});
  assert(auto_mice.size() == 1 && auto_mice.front().is_mouse);

  DeviceInfo virtual_device = real;
  virtual_device.vendor=0x5357; virtual_device.product=0x0003; virtual_device.name="ScrollShift Accelerated";
  assert(!is_capture_candidate(virtual_device));

  DeviceInfo qemu_tablet = real;
  qemu_tablet.name="QEMU QEMU USB Tablet"; qemu_tablet.relative_pointer=false;
  qemu_tablet.has_rel_x=false; qemu_tablet.has_rel_y=false; qemu_tablet.abs_axes=true;
  assert(!is_capture_candidate(qemu_tablet));
  assert(is_capture_candidate(qemu_tablet, true));
  DeviceInfo no_wheel = qemu_tablet; no_wheel.wheel=false; no_wheel.hi_res_wheel=false;
  assert(!is_capture_candidate(no_wheel, true));
  assert(!is_capture_candidate(virtual_device, true));

  DeviceInfo wrong = real; wrong.name="Consumer Control";
  assert(!matches_selector(wrong, config->device));

  auto matches = matching_capture_devices({virtual_device, wrong, real}, config->device);
  assert(matches.size() == 1 && matches.front().name == real.name);

  const auto ready = diagnose_device_match({virtual_device, wrong, real}, config->device);
  assert(ready.state == DeviceMatchState::Unique && ready.matches.size() == 1);
  const auto missing = diagnose_device_match({virtual_device, wrong}, config->device);
  assert(missing.state == DeviceMatchState::Missing && missing.matches.empty());
  DeviceInfo duplicate = real; duplicate.path = "/dev/input/event99";
  const auto ambiguous = diagnose_device_match({real, duplicate}, config->device);
  assert(ambiguous.state == DeviceMatchState::Ambiguous && ambiguous.matches.size() == 2);

  DeviceSelector qemu_selector{qemu_tablet.vendor, qemu_tablet.product, qemu_tablet.name};
  assert(diagnose_device_match({qemu_tablet}, qemu_selector).state == DeviceMatchState::Missing);
  assert(diagnose_device_match({qemu_tablet}, qemu_selector, true).state == DeviceMatchState::Unique);

  std::istringstream automatic("mode = auto\nprofile = balanced\nreconnect_ms = 500\n");
  auto auto_config = parse_config(automatic, error);
  assert(auto_config && auto_config->auto_discover);
  assert(serialize_config(*auto_config).find("mode = auto") != std::string::npos);

  std::istringstream forced("mode = device\ndevice_vendor = 0x0627\ndevice_product = 0x0001\ndevice_name = QEMU QEMU USB Tablet\nallow_non_pointer_wheel = true\nprofile = balanced\n");
  auto forced_config = parse_config(forced, error);
  assert(forced_config && forced_config->allow_non_pointer_wheel);
  assert(serialize_config(*forced_config).find("allow_non_pointer_wheel = true") != std::string::npos);

  const auto roundtrip_text = serialize_config(*config);
  std::istringstream roundtrip(roundtrip_text);
  auto again = parse_config(roundtrip, error);
  assert(again && again->device.vendor == config->device.vendor && again->profile == config->profile);

  std::istringstream bad("device_vendor = wat\ndevice_product = 1\n");
  assert(!parse_config(bad, error));
  assert(!error.empty());

  std::cout << "config tests passed\n";
}
