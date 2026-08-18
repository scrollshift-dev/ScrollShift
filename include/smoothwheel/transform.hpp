#pragma once
#include <chrono>
#include <linux/input.h>
#include <string>
#include <vector>

#include "smoothwheel/velocity.hpp"

namespace smoothwheel {

struct AccelerationProfile {
  std::string name;
  std::string description;
  VelocityConfig velocity;
};

std::vector<AccelerationProfile> acceleration_profiles();
const AccelerationProfile* find_acceleration_profile(const std::string& name);

class WheelPacketTransformer {
 public:
  explicit WheelPacketTransformer(VelocityConfig config = {});
  std::vector<input_event> transform(const std::vector<input_event>& packet);
  void reset();
 private:
  VelocityEstimator vertical_;
  VelocityEstimator horizontal_;
  double vertical_legacy_remainder_{0.0};
  double horizontal_legacy_remainder_{0.0};
};

}  // namespace smoothwheel
