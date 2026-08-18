#pragma once
#include <chrono>
#include <cstdint>

namespace smoothwheel {

struct VelocityConfig {
  double slow_interval_ms{420.0};
  double fast_interval_ms{45.0};
  double max_multiplier{5.0};
  double smoothing{0.35};
  double curve_power{1.0};
};

struct VelocitySample {
  double interval_ms{};
  double instantaneous{};
  double filtered{};
  double multiplier{1.0};
};

class VelocityEstimator {
 public:
  explicit VelocityEstimator(VelocityConfig config = {});
  VelocitySample observe(std::chrono::microseconds timestamp, int direction);
  void reset();
 private:
  VelocityConfig config_;
  bool have_previous_{false};
  std::chrono::microseconds previous_{};
  int previous_direction_{};
  double filtered_{0.0};
};

}  // namespace smoothwheel
