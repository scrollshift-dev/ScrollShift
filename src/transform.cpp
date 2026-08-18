#include "smoothwheel/transform.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>

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
struct AxisPacket {
  int hi_sum{0};
  int lo_sum{0};
  std::size_t hi_first{static_cast<std::size_t>(-1)};
  std::size_t lo_first{static_cast<std::size_t>(-1)};
  std::chrono::microseconds timestamp{};
  bool have_timestamp{false};
};
void collect_axis(const std::vector<input_event>& packet, std::uint16_t hi_code, std::uint16_t lo_code,
                  AxisPacket& axis) {
  for (std::size_t i = 0; i < packet.size(); ++i) {
    const auto& e = packet[i];
    if (e.type != EV_REL || e.value == 0) continue;
    if (e.code == hi_code) {
      axis.hi_sum += e.value;
      if (axis.hi_first == static_cast<std::size_t>(-1)) axis.hi_first = i;
      axis.timestamp = event_time(e);
      axis.have_timestamp = true;
    } else if (e.code == lo_code) {
      axis.lo_sum += e.value;
      if (axis.lo_first == static_cast<std::size_t>(-1)) axis.lo_first = i;
      if (!axis.have_timestamp) {
        axis.timestamp = event_time(e);
        axis.have_timestamp = true;
      }
    }
  }
}
}

std::vector<AccelerationProfile> acceleration_profiles() {
  return {
      {"precision", "Slow 0.70x precision with a gentle 2x ceiling", {420.0, 55.0, 0.70, 2.0, 0.30, 1.20}},
      {"balanced", "Clear slow/fast separation without extreme travel", {420.0, 45.0, 1.0, 4.0, 0.38, 1.0}},
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
  AxisPacket vertical;
  AxisPacket horizontal;
  collect_axis(packet, REL_WHEEL_HI_RES, REL_WHEEL, vertical);
  collect_axis(packet, REL_HWHEEL_HI_RES, REL_HWHEEL, horizontal);

  auto apply_axis = [&](const AxisPacket& axis, VelocityEstimator& estimator, double& legacy_remainder,
                        std::uint16_t hi_code, std::uint16_t lo_code) {
    const int source_value = axis.hi_sum != 0 ? axis.hi_sum : axis.lo_sum;
    if (!axis.have_timestamp || source_value == 0) return;
    const int normalized_v120 = axis.hi_sum != 0 ? std::abs(axis.hi_sum) : std::abs(axis.lo_sum) * 120;
    const auto sample = estimator.observe(axis.timestamp, sign_of(source_value), normalized_v120);

    const int transformed_hi = axis.hi_sum != 0 ? scaled_hi_res(axis.hi_sum, sample.multiplier) : 0;
    const double legacy_contribution = axis.hi_sum != 0
                                           ? static_cast<double>(transformed_hi) / 120.0
                                           : static_cast<double>(axis.lo_sum) * sample.multiplier;
    const int transformed_lo = axis.lo_sum != 0 ? consume_legacy(legacy_contribution, legacy_remainder) : 0;

    bool wrote_hi = false;
    bool wrote_lo = false;
    for (auto& e : out) {
      if (e.type != EV_REL) continue;
      if (e.code == hi_code && e.value != 0) {
        e.value = wrote_hi ? 0 : transformed_hi;
        wrote_hi = true;
      }
      if (e.code == lo_code && e.value != 0) {
        e.value = wrote_lo ? 0 : transformed_lo;
        wrote_lo = true;
      }
    }
  };

  apply_axis(vertical, vertical_, vertical_legacy_remainder_, REL_WHEEL_HI_RES, REL_WHEEL);
  apply_axis(horizontal, horizontal_, horizontal_legacy_remainder_, REL_HWHEEL_HI_RES, REL_HWHEEL);
  return out;
}

}  // namespace smoothwheel
