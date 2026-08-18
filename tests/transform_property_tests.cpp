#include "scrollshift/transform.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>

namespace {
input_event make_event(std::int64_t usec, unsigned short type, unsigned short code, int value) {
  input_event e{};
  e.time.tv_sec = static_cast<decltype(e.time.tv_sec)>(usec / 1000000);
  e.time.tv_usec = static_cast<decltype(e.time.tv_usec)>(usec % 1000000);
  e.type = type;
  e.code = code;
  e.value = value;
  return e;
}

bool same_event_shape(const input_event& a, const input_event& b) {
  return a.time.tv_sec == b.time.tv_sec && a.time.tv_usec == b.time.tv_usec && a.type == b.type &&
         a.code == b.code;
}

bool is_wheel_code(unsigned short code) {
  return code == REL_WHEEL || code == REL_WHEEL_HI_RES || code == REL_HWHEEL ||
         code == REL_HWHEEL_HI_RES;
}
}

int main() {
  using namespace scrollshift;
  const auto* profile = find_acceleration_profile("balanced");
  assert(profile != nullptr);

  WheelPacketTransformer left(profile->velocity);
  WheelPacketTransformer right(profile->velocity);
  std::mt19937 rng(0x534d4f4fU);  // "SMOO"; deterministic by design.
  std::uniform_int_distribution<int> interval_us(1000, 650000);
  std::uniform_int_distribution<int> mode(0, 7);
  std::uniform_int_distribution<int> direction(0, 1);
  std::uniform_int_distribution<int> motion(-25, 25);
  std::uniform_int_distribution<int> fractional(1, 4);

  std::int64_t now = 1'000'000;
  constexpr int kPackets = 100000;
  for (int i = 0; i < kPackets; ++i) {
    now += interval_us(rng);
    const int sign = direction(rng) == 0 ? -1 : 1;
    std::vector<input_event> packet;

    // Always include ordinary pointer traffic so transparency is continuously checked.
    packet.push_back(make_event(now, EV_REL, REL_X, motion(rng)));
    packet.push_back(make_event(now, EV_REL, REL_Y, motion(rng)));

    switch (mode(rng)) {
      case 0:  // paired vertical detent
        packet.push_back(make_event(now, EV_REL, REL_WHEEL, sign));
        packet.push_back(make_event(now, EV_REL, REL_WHEEL_HI_RES, sign * 120));
        break;
      case 1:  // fractional vertical hi-res
        packet.push_back(make_event(now, EV_REL, REL_WHEEL_HI_RES, sign * 30 * fractional(rng)));
        break;
      case 2:  // low-resolution-only burst
        packet.push_back(make_event(now, EV_REL, REL_WHEEL, sign * fractional(rng)));
        break;
      case 3:  // paired horizontal
        packet.push_back(make_event(now, EV_REL, REL_HWHEEL, sign));
        packet.push_back(make_event(now, EV_REL, REL_HWHEEL_HI_RES, sign * 120));
        break;
      case 4:  // mixed axes
        packet.push_back(make_event(now, EV_REL, REL_WHEEL_HI_RES, sign * 120));
        packet.push_back(make_event(now, EV_REL, REL_WHEEL, sign));
        packet.push_back(make_event(now, EV_REL, REL_HWHEEL_HI_RES, -sign * 60));
        break;
      case 5:  // duplicate same-axis events in one report
        packet.push_back(make_event(now, EV_REL, REL_WHEEL_HI_RES, sign * 60));
        packet.push_back(make_event(now, EV_REL, REL_WHEEL_HI_RES, sign * 60));
        packet.push_back(make_event(now, EV_REL, REL_WHEEL, sign));
        break;
      case 6:  // zero-net wheel packet
        packet.push_back(make_event(now, EV_REL, REL_WHEEL_HI_RES, sign * 60));
        packet.push_back(make_event(now, EV_REL, REL_WHEEL_HI_RES, -sign * 60));
        break;
      case 7:  // no wheel at all
        break;
    }
    packet.push_back(make_event(now, EV_SYN, SYN_REPORT, 0));

    const auto a = left.transform(packet);
    const auto b = right.transform(packet);
    assert(a.size() == packet.size());
    assert(b.size() == packet.size());
    assert(a.size() == b.size());

    for (std::size_t n = 0; n < packet.size(); ++n) {
      assert(same_event_shape(packet[n], a[n]));
      assert(same_event_shape(a[n], b[n]));
      assert(a[n].value == b[n].value);  // deterministic for identical state/input.
      if (packet[n].type != EV_REL || !is_wheel_code(packet[n].code))
        assert(a[n].value == packet[n].value);  // non-wheel traffic is transparent.
    }
  }

  std::cout << "transform property tests passed: " << kPackets << " deterministic packets\n";
}
