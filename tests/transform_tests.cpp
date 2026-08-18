#include "scrollshift/transform.hpp"

#include <cassert>
#include <iostream>
#include <vector>

namespace {
input_event ev(long sec, long usec, unsigned short type, unsigned short code, int value) {
  input_event e{};
  e.time.tv_sec = sec;
  e.time.tv_usec = usec;
  e.type = type;
  e.code = code;
  e.value = value;
  return e;
}
int value_sum(const std::vector<input_event>& p, unsigned short code) {
  int total = 0;
  for (const auto& e : p)
    if (e.type == EV_REL && e.code == code) total += e.value;
  return total;
}
}

int main() {
  using namespace scrollshift;
  auto* profile = find_acceleration_profile("balanced");
  assert(profile);
  assert(profile->velocity.min_multiplier == 1.0);
  assert(profile->velocity.max_multiplier == 4.0);
  assert(profile->velocity.curve_power == 1.0);

  WheelPacketTransformer t(profile->velocity);
  auto p = t.transform({ev(1, 0, EV_REL, REL_WHEEL, 1), ev(1, 0, EV_REL, REL_WHEEL_HI_RES, 120),
                        ev(1, 0, EV_SYN, SYN_REPORT, 0)});
  assert(value_sum(p, REL_WHEEL) == 1);
  assert(value_sum(p, REL_WHEEL_HI_RES) == 120);

  // Slow/isolated detents remain native-speed in the frozen balanced profile.
  p = t.transform({ev(1, 600000, EV_REL, REL_WHEEL, 1), ev(1, 600000, EV_REL, REL_WHEEL_HI_RES, 120),
                   ev(1, 600000, EV_SYN, SYN_REPORT, 0)});
  assert(value_sum(p, REL_WHEEL) == 1);
  assert(value_sum(p, REL_WHEEL_HI_RES) == 120);

  // Sustained rapid same-direction detents accelerate, but remain bounded by 4x.
  t.reset();
  p = t.transform({ev(3, 0, EV_REL, REL_WHEEL, 1), ev(3, 0, EV_REL, REL_WHEEL_HI_RES, 120),
                   ev(3, 0, EV_SYN, SYN_REPORT, 0)});
  for (int i = 1; i <= 18; ++i)
    p = t.transform({ev(3, i * 35000, EV_REL, REL_WHEEL, 1),
                     ev(3, i * 35000, EV_REL, REL_WHEEL_HI_RES, 120),
                     ev(3, i * 35000, EV_SYN, SYN_REPORT, 0)});
  assert(value_sum(p, REL_WHEEL) >= 3 && value_sum(p, REL_WHEEL) <= 4);
  assert(value_sum(p, REL_WHEEL_HI_RES) >= 360 && value_sum(p, REL_WHEEL_HI_RES) <= 480);

  // Reversal must immediately drop back to native speed and clear opposing legacy carry.
  p = t.transform({ev(4, 0, EV_REL, REL_WHEEL, -1), ev(4, 0, EV_REL, REL_WHEEL_HI_RES, -120),
                   ev(4, 0, EV_SYN, SYN_REPORT, 0)});
  assert(value_sum(p, REL_WHEEL) == -1);
  assert(value_sum(p, REL_WHEEL_HI_RES) == -120);

  // Non-wheel pointer traffic is transparent.
  t.reset();
  p = t.transform({ev(5, 0, EV_REL, REL_X, 7), ev(5, 0, EV_REL, REL_Y, -3),
                   ev(5, 0, EV_SYN, SYN_REPORT, 0)});
  assert(value_sum(p, REL_X) == 7);
  assert(value_sum(p, REL_Y) == -3);

  // Low-resolution-only wheels are supported without inventing HI_RES events.
  t.reset();
  p = t.transform({ev(6, 0, EV_REL, REL_WHEEL, 1), ev(6, 0, EV_SYN, SYN_REPORT, 0)});
  assert(value_sum(p, REL_WHEEL) == 1);
  assert(value_sum(p, REL_WHEEL_HI_RES) == 0);

  // High-resolution-only/free-spin input is preserved at slow speed.
  t.reset();
  p = t.transform({ev(7, 0, EV_REL, REL_WHEEL_HI_RES, 30), ev(7, 0, EV_SYN, SYN_REPORT, 0)});
  assert(value_sum(p, REL_WHEEL_HI_RES) == 30);
  assert(value_sum(p, REL_WHEEL) == 0);

  // Fractional HI_RES cadence is normalized by v120 magnitude: 30 every 10 ms
  // is equivalent to 120 every 40 ms and must eventually accelerate.
  for (int i = 1; i <= 18; ++i)
    p = t.transform({ev(7, i * 10000, EV_REL, REL_WHEEL_HI_RES, 30),
                     ev(7, i * 10000, EV_SYN, SYN_REPORT, 0)});
  assert(value_sum(p, REL_WHEEL_HI_RES) > 90);
  assert(value_sum(p, REL_WHEEL_HI_RES) <= 120);

  // Multiple events for the same axis in one SYN_REPORT are aggregated once,
  // rather than each receiving the full transformed packet value.
  t.reset();
  p = t.transform({ev(8, 0, EV_REL, REL_WHEEL_HI_RES, 60),
                   ev(8, 0, EV_REL, REL_WHEEL_HI_RES, 60),
                   ev(8, 0, EV_REL, REL_WHEEL, 1),
                   ev(8, 0, EV_SYN, SYN_REPORT, 0)});
  assert(value_sum(p, REL_WHEEL_HI_RES) == 120);
  assert(value_sum(p, REL_WHEEL) == 1);

  // A multi-detent low-resolution burst is treated as one larger physical sample.
  t.reset();
  p = t.transform({ev(9, 0, EV_REL, REL_WHEEL, 3), ev(9, 0, EV_SYN, SYN_REPORT, 0)});
  assert(value_sum(p, REL_WHEEL) == 3);

  // Horizontal and vertical estimators are independent, including mixed-axis packets.
  t.reset();
  p = t.transform({ev(10, 0, EV_REL, REL_WHEEL_HI_RES, 120),
                   ev(10, 0, EV_REL, REL_WHEEL, 1),
                   ev(10, 0, EV_REL, REL_HWHEEL_HI_RES, -120),
                   ev(10, 0, EV_REL, REL_HWHEEL, -1),
                   ev(10, 0, EV_SYN, SYN_REPORT, 0)});
  assert(value_sum(p, REL_WHEEL_HI_RES) == 120);
  assert(value_sum(p, REL_HWHEEL_HI_RES) == -120);
  assert(value_sum(p, REL_WHEEL) == 1);
  assert(value_sum(p, REL_HWHEEL) == -1);

  // A zero-net packet does not create synthetic movement.
  t.reset();
  p = t.transform({ev(11, 0, EV_REL, REL_WHEEL_HI_RES, 60),
                   ev(11, 0, EV_REL, REL_WHEEL_HI_RES, -60),
                   ev(11, 0, EV_SYN, SYN_REPORT, 0)});
  assert(value_sum(p, REL_WHEEL_HI_RES) == 0);

  std::cout << "transform tests passed\n";
}
