#include "smoothwheel/velocity.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace smoothwheel {
VelocityEstimator::VelocityEstimator(VelocityConfig config) : config_(config) {
  if (!(config_.fast_interval_ms > 0.0 && config_.slow_interval_ms > config_.fast_interval_ms &&
        config_.min_multiplier > 0.0 && config_.max_multiplier >= config_.min_multiplier &&
        config_.smoothing > 0.0 && config_.smoothing <= 1.0 && config_.curve_power > 0.0))
    throw std::invalid_argument("invalid velocity configuration");
}

VelocitySample VelocityEstimator::observe(std::chrono::microseconds timestamp, int direction, int normalized_v120) {
  if (direction != -1 && direction != 1) throw std::invalid_argument("direction must be -1 or 1");
  if (normalized_v120 <= 0) throw std::invalid_argument("normalized_v120 must be positive");
  VelocitySample out;
  if (!have_previous_ || direction != previous_direction_ || timestamp <= previous_) {
    filtered_ = 0.0;
    out.multiplier = config_.min_multiplier;
  } else {
    const double actual_interval_ms = std::chrono::duration<double, std::milli>(timestamp - previous_).count();
    // Profiles are calibrated in milliseconds per conventional 120-v120 detent.
    // Scale fractional/burst input to that equivalent cadence so a 30-v120
    // high-resolution sample every 10 ms is treated like 120 v120 every 40 ms.
    out.interval_ms = actual_interval_ms * 120.0 / static_cast<double>(normalized_v120);
    const double span = config_.slow_interval_ms - config_.fast_interval_ms;
    out.instantaneous = std::clamp((config_.slow_interval_ms - out.interval_ms) / span, 0.0, 1.0);
    filtered_ += config_.smoothing * (out.instantaneous - filtered_);
    out.filtered = filtered_;
    const double shaped = std::pow(filtered_, config_.curve_power);
    out.multiplier = config_.min_multiplier + shaped * (config_.max_multiplier - config_.min_multiplier);
  }
  previous_ = timestamp;
  previous_direction_ = direction;
  have_previous_ = true;
  out.filtered = filtered_;
  return out;
}

void VelocityEstimator::reset() { have_previous_ = false; previous_ = {}; previous_direction_ = 0; filtered_ = 0.0; }
}  // namespace smoothwheel
