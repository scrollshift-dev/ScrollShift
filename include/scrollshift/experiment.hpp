#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace scrollshift {

enum class ScrollAxis { Vertical, Horizontal };

struct PlannedScrollPacket {
  std::int32_t hi_res_value{};
  std::int32_t low_res_value{};
  int delay_ms{};
};

struct ExperimentPreset {
  std::string name;
  std::string description;
  int steps{};
  int interval_ms{};
  bool emit_legacy_detent{};
};

std::vector<ExperimentPreset> experiment_presets();
const ExperimentPreset* find_experiment_preset(const std::string& name);
std::vector<PlannedScrollPacket> plan_scroll(const ExperimentPreset& preset, int direction);
std::string describe_plan(const ExperimentPreset& preset, ScrollAxis axis, int direction,
                          const std::vector<PlannedScrollPacket>& packets);

class VirtualWheel {
 public:
  explicit VirtualWheel(const std::filesystem::path& uinput_path = "/dev/uinput");
  ~VirtualWheel();
  VirtualWheel(const VirtualWheel&) = delete;
  VirtualWheel& operator=(const VirtualWheel&) = delete;
  VirtualWheel(VirtualWheel&&) = delete;
  VirtualWheel& operator=(VirtualWheel&&) = delete;

  void emit_packet(ScrollAxis axis, const PlannedScrollPacket& packet);

 private:
  int fd_{-1};
  bool created_{false};
  void emit(std::uint16_t type, std::uint16_t code, std::int32_t value);
};

int run_virtual_scroll_experiment(const ExperimentPreset& preset, ScrollAxis axis,
                                  int direction, int initial_delay_seconds,
                                  const std::filesystem::path& uinput_path = "/dev/uinput");

}  // namespace scrollshift
