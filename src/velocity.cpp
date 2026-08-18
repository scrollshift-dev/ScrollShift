#include "smoothwheel/velocity.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace smoothwheel {
VelocityEstimator::VelocityEstimator(VelocityConfig config) : config_(config) {
  if (!(config_.fast_interval_ms > 0.0 && config_.slow_interval_ms > config_.fast_interval_ms &&
        config_.max_multiplier >= 1.0 && config_.smoothing > 0.0 && config_.smoothing <= 1.0 &&
        config_.curve_power > 0.0))
    throw std::invalid_argument("invalid velocity configuration");
}

VelocitySample VelocityEstimator::observe(std::chrono::microseconds timestamp, int direction) {
  if (direction != -1 && direction != 1) throw std::invalid_argument("direction must be -1 or 1");
  VelocitySample out;
  if (!have_previous_ || direction != previous_direction_ || timestamp <= previous_) {
    filtered_ = 0.0;
    out.multiplier = 1.0;
  } else {
    out.interval_ms = std::chrono::duration<double, std::milli>(timestamp - previous_).count();
    const double span = config_.slow_interval_ms - config_.fast_interval_ms;
    out.instantaneous = std::clamp((config_.slow_interval_ms - out.interval_ms) / span, 0.0, 1.0);
    filtered_ += config_.smoothing * (out.instantaneous - filtered_);
    out.filtered = filtered_;
    const double shaped = std::pow(filtered_, config_.curve_power);
    out.multiplier = 1.0 + shaped * (config_.max_multiplier - 1.0);
  }
  previous_ = timestamp;
  previous_direction_ = direction;
  have_previous_ = true;
  out.filtered = filtered_;
  return out;
}

void VelocityEstimator::reset() { have_previous_ = false; previous_ = {}; previous_direction_ = 0; filtered_ = 0.0; }
}  // namespace smoothwheel
