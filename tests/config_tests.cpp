#include "smoothwheel/config.hpp"
#include <cassert>
#include <sstream>
#include <iostream>

int main() {
  using namespace smoothwheel;
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
  real.relative_pointer=true; real.wheel=true; real.hi_res_wheel=true;
  assert(matches_selector(real, config->device));
  assert(is_capture_candidate(real));

  DeviceInfo virtual_device = real;
  virtual_device.vendor=0x5357; virtual_device.product=0x0003; virtual_device.name="SmoothWheel Accelerated";
  assert(!is_capture_candidate(virtual_device));

  DeviceInfo wrong = real; wrong.name="Consumer Control";
  assert(!matches_selector(wrong, config->device));

  auto matches = matching_capture_devices({virtual_device, wrong, real}, config->device);
  assert(matches.size() == 1 && matches.front().name == real.name);

  const auto roundtrip_text = serialize_config(*config);
  std::istringstream roundtrip(roundtrip_text);
  auto again = parse_config(roundtrip, error);
  assert(again && again->device.vendor == config->device.vendor && again->profile == config->profile);

  std::istringstream bad("device_vendor = wat\ndevice_product = 1\n");
  assert(!parse_config(bad, error));
  assert(!error.empty());

  std::cout << "config tests passed\n";
}
