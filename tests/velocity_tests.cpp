#include "smoothwheel/velocity.hpp"
#include <cassert>
#include <chrono>
#include <iostream>

int main() {
  using namespace smoothwheel;
  using namespace std::chrono_literals;

  VelocityConfig cfg{520.0, 35.0, 0.45, 9.0, 0.32, 3.00};
  VelocityEstimator v(cfg);
  auto s = v.observe(0us, 1);
  assert(s.multiplier == 0.45);
  s = v.observe(600ms, 1);
  assert(s.multiplier == 0.45);

  v.reset();
  v.observe(0us, 1);
  double previous = 0.45;
  for (int i = 1; i <= 16; ++i) {
    s = v.observe(std::chrono::milliseconds(i * 35), 1);
    assert(s.multiplier >= previous);
    previous = s.multiplier;
  }
  assert(s.multiplier > 8.5 && s.multiplier <= 9.0);

  auto reversed = v.observe(400ms, -1);
  assert(reversed.multiplier == 0.45);
  auto after_reverse = v.observe(435ms, -1);
  assert(after_reverse.multiplier > 0.45);
  auto nonmonotonic = v.observe(100ms, -1);
  assert(nonmonotonic.multiplier == 0.45);

  VelocityEstimator shaped(cfg);
  shaped.observe(0us, 1);
  auto slow = shaped.observe(450ms, 1);
  assert(slow.multiplier > 0.45 && slow.multiplier < 0.55);
  auto moderate = shaped.observe(700ms, 1); // 250 ms after prior sample
  assert(moderate.multiplier > 0.5 && moderate.multiplier < 1.0);
  VelocitySample hard{};
  for (int i = 1; i <= 18; ++i) hard = shaped.observe(700ms + std::chrono::milliseconds(i * 35), 1);
  assert(hard.multiplier > 8.5 && hard.multiplier <= 9.0);


  // High-resolution samples are normalized by their v120 magnitude. Thirty
  // units every 10 ms represents the same physical rate as 120 every 40 ms.
  VelocityEstimator hires(cfg);
  hires.observe(0us, 1, 30);
  VelocitySample hires_fast{};
  for (int i = 1; i <= 16; ++i)
    hires_fast = hires.observe(std::chrono::milliseconds(i * 10), 1, 30);
  assert(hires_fast.interval_ms == 40.0);
  assert(hires_fast.multiplier > 8.0);

  VelocityEstimator hires_slow(cfg);
  hires_slow.observe(0us, 1, 30);
  auto hires_precision = hires_slow.observe(100ms, 1, 30);
  assert(hires_precision.interval_ms == 400.0);
  assert(hires_precision.multiplier < 0.7);

  std::cout << "velocity tests passed\n";
}
