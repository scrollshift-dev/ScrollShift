#include "smoothwheel/velocity.hpp"
#include <cassert>
#include <chrono>
#include <iostream>

int main() {
  using namespace smoothwheel;
  using namespace std::chrono_literals;
  VelocityEstimator v;
  auto s = v.observe(0us, 1); assert(s.multiplier == 1.0);
  s = v.observe(500ms, 1); assert(s.multiplier == 1.0);
  v.reset(); v.observe(0us, 1);
  double previous = 1.0;
  for (int i=1;i<=8;++i) { s=v.observe(std::chrono::milliseconds(i*35),1); assert(s.multiplier >= previous); previous=s.multiplier; }
  assert(s.multiplier > 4.0 && s.multiplier <= 5.0);
  auto reversed=v.observe(300ms,-1); assert(reversed.multiplier == 1.0);
  auto after_reverse=v.observe(335ms,-1); assert(after_reverse.multiplier > 1.0);
  auto nonmonotonic=v.observe(100ms,-1); assert(nonmonotonic.multiplier == 1.0);
  std::cout << "velocity tests passed\n";
}
