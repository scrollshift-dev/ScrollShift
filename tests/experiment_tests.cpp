#include "scrollshift/experiment.hpp"

#include <cassert>
#include <iostream>
#include <numeric>

int main() {
  using namespace scrollshift;

  for (const auto& preset : experiment_presets()) {
    for (const int direction : {-1, 1}) {
      const auto packets = plan_scroll(preset, direction);
      assert(static_cast<int>(packets.size()) == preset.steps);
      int hi_res_total = 0;
      int low_res_total = 0;
      for (const auto& packet : packets) {
        hi_res_total += packet.hi_res_value;
        low_res_total += packet.low_res_value;
      }
      assert(hi_res_total == direction * 120);
      assert(low_res_total == (preset.emit_legacy_detent ? direction : 0));
      if (preset.emit_legacy_detent) {
        for (std::size_t i = 0; i + 1 < packets.size(); ++i) assert(packets[i].low_res_value == 0);
        assert(packets.back().low_res_value == direction);
      }
    }
  }

  const auto* fine16 = find_experiment_preset("fine16");
  assert(fine16);
  const auto fine16_packets = plan_scroll(*fine16, 1);
  assert(fine16_packets.size() == 16);
  assert(fine16_packets.front().hi_res_value == 8);
  assert(fine16_packets.back().hi_res_value == 7);
  assert(describe_plan(*fine16, ScrollAxis::Vertical, 1, fine16_packets).find("hi-res=120") != std::string::npos);
  assert(find_experiment_preset("not-a-preset") == nullptr);

  std::cout << "experiment tests passed\n";
}
