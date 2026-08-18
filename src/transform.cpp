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
int scaled_hi_res(int value, double multiplier) {
  if (value == 0) return 0;
  const double scaled = static_cast<double>(value) * multiplier;
  const int rounded = static_cast<int>(std::lround(scaled));
  return rounded == 0 ? sign_of(value) : rounded;
}
int consume_legacy(double contribution, double& remainder) {
  if (contribution == 0.0) return 0;
  if (remainder != 0.0 && ((remainder < 0.0) != (contribution < 0.0))) remainder = 0.0;
  const double total = remainder + contribution;
  const int whole = total < 0.0 ? static_cast<int>(std::ceil(total)) : static_cast<int>(std::floor(total));
  remainder = total - static_cast<double>(whole);
  return whole;
}
}

std::vector<AccelerationProfile> acceleration_profiles() {
  return {
      {"precision", "Slow 0.70x precision with a gentle 2x ceiling", {420.0, 55.0, 0.70, 2.0, 0.30, 1.20}},
      {"balanced", "Long precision range with a strong 9x sustained hard-spin ceiling", {520.0, 35.0, 0.45, 9.0, 0.32, 3.00}},
      {"fast", "Precise low end with strong 12x rapid traversal", {500.0, 32.0, 0.45, 12.0, 0.47, 1.85}},
      {"aggressive", "Very wide 0.40x to 16x acceleration range", {520.0, 28.0, 0.40, 16.0, 0.55, 1.65}},
  };
}

const AccelerationProfile* find_acceleration_profile(const std::string& name) {
  static const auto profiles = acceleration_profiles();
  const auto it = std::find_if(profiles.begin(), profiles.end(), [&](const auto& p) { return p.name == name; });
  return it == profiles.end() ? nullptr : &*it;
}

WheelPacketTransformer::WheelPacketTransformer(VelocityConfig config)
    : vertical_(config), horizontal_(config) {}

void WheelPacketTransformer::reset() {
  vertical_.reset();
  horizontal_.reset();
  vertical_legacy_remainder_ = 0.0;
  horizontal_legacy_remainder_ = 0.0;
}

std::vector<input_event> WheelPacketTransformer::transform(const std::vector<input_event>& packet) {
  auto out = packet;
  const input_event* vhi = nullptr;
  const input_event* vlo = nullptr;
  const input_event* hhi = nullptr;
  const input_event* hlo = nullptr;
  for (const auto& e : packet) {
    if (e.type != EV_REL || e.value == 0) continue;
    if (e.code == REL_WHEEL_HI_RES) vhi = &e;
    else if (e.code == REL_WHEEL) vlo = &e;
    else if (e.code == REL_HWHEEL_HI_RES) hhi = &e;
    else if (e.code == REL_HWHEEL) hlo = &e;
  }

  auto apply_axis = [&](const input_event* hi, const input_event* lo, VelocityEstimator& estimator,
                        double& legacy_remainder, std::uint16_t hi_code, std::uint16_t lo_code) {
    const input_event* source = hi ? hi : lo;
    if (!source) return;
    const auto sample = estimator.observe(event_time(*source), sign_of(source->value));

    int transformed_hi = 0;
    if (hi) transformed_hi = scaled_hi_res(hi->value, sample.multiplier);
    const double legacy_contribution = hi ? static_cast<double>(transformed_hi) / 120.0
                                          : static_cast<double>(lo->value) * sample.multiplier;
    const int transformed_lo = lo ? consume_legacy(legacy_contribution, legacy_remainder) : 0;

    for (auto& e : out) {
      if (e.type != EV_REL) continue;
      if (hi && e.code == hi_code && e.value != 0) e.value = transformed_hi;
      if (lo && e.code == lo_code && e.value != 0) e.value = transformed_lo;
    }
  };

  apply_axis(vhi, vlo, vertical_, vertical_legacy_remainder_, REL_WHEEL_HI_RES, REL_WHEEL);
  apply_axis(hhi, hlo, horizontal_, horizontal_legacy_remainder_, REL_HWHEEL_HI_RES, REL_HWHEEL);
  return out;
}

}  // namespace smoothwheel
