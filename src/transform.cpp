#include "smoothwheel/transform.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace smoothwheel {
namespace {
std::chrono::microseconds event_time(const input_event& e) {
  return std::chrono::seconds(e.time.tv_sec) + std::chrono::microseconds(e.time.tv_usec);
}
int sign_of(int value) { return value < 0 ? -1 : 1; }
int scaled(int value, double multiplier) {
  if (value == 0) return 0;
  const double mag = std::abs(static_cast<double>(value)) * multiplier;
  return sign_of(value) * std::max(1, static_cast<int>(std::lround(mag)));
}
}

std::vector<AccelerationProfile> acceleration_profiles() {
  return {
      {"precision", "Gentle acceleration with a low ceiling", {420.0, 55.0, 2.0, 0.30}},
      {"balanced", "Clear slow/fast separation without extreme travel", {420.0, 45.0, 4.0, 0.38}},
      {"fast", "Strong acceleration for rapid traversal", {460.0, 40.0, 7.0, 0.45}},
      {"aggressive", "Very strong acceleration for hard wheel spins", {500.0, 35.0, 10.0, 0.55}},
  };
}

const AccelerationProfile* find_acceleration_profile(const std::string& name) {
  static const auto profiles = acceleration_profiles();
  const auto it = std::find_if(profiles.begin(), profiles.end(), [&](const auto& p) { return p.name == name; });
  return it == profiles.end() ? nullptr : &*it;
}

WheelPacketTransformer::WheelPacketTransformer(VelocityConfig config)
    : vertical_(config), horizontal_(config) {}

void WheelPacketTransformer::reset() { vertical_.reset(); horizontal_.reset(); }

std::vector<input_event> WheelPacketTransformer::transform(const std::vector<input_event>& packet) {
  auto out = packet;
  const input_event* vhi = nullptr;
  const input_event* hlo = nullptr;
  const input_event* hhi = nullptr;
  const input_event* hlow = nullptr;
  for (const auto& e : packet) {
    if (e.type != EV_REL || e.value == 0) continue;
    if (e.code == REL_WHEEL_HI_RES) vhi = &e;
    else if (e.code == REL_WHEEL) hlo = &e;
    else if (e.code == REL_HWHEEL_HI_RES) hhi = &e;
    else if (e.code == REL_HWHEEL) hlow = &e;
  }

  auto apply_axis = [&](const input_event* hi, const input_event* lo, VelocityEstimator& estimator,
                        std::uint16_t hi_code, std::uint16_t lo_code) {
    const input_event* source = hi ? hi : lo;
    if (!source) return;
    const auto sample = estimator.observe(event_time(*source), sign_of(source->value));
    for (auto& e : out) {
      if (e.type != EV_REL) continue;
      if (e.code == hi_code && e.value != 0) e.value = scaled(e.value, sample.multiplier);
      if (e.code == lo_code && e.value != 0) e.value = scaled(e.value, sample.multiplier);
    }
  };

  apply_axis(vhi, hlo, vertical_, REL_WHEEL_HI_RES, REL_WHEEL);
  apply_axis(hhi, hlow, horizontal_, REL_HWHEEL_HI_RES, REL_HWHEEL);
  return out;
}

}  // namespace smoothwheel
