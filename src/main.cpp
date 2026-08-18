#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

#include "smoothwheel/experiment.hpp"
#include "smoothwheel/input.hpp"
#include "smoothwheel/relay.hpp"
#include "smoothwheel/transform.hpp"
#include "smoothwheel/version.hpp"

namespace {
void print_help() {
  std::cout
      << "SmoothWheel " << smoothwheel::kVersion << '\n'
      << "Native smooth mouse-wheel scrolling for Linux.\n\n"
      << "Usage:\n"
      << "  smoothwheel devices\n"
      << "  smoothwheel inspect DEVICE\n"
      << "  smoothwheel monitor DEVICE [--all] [--record FILE]\n"
      << "  smoothwheel relay DEVICE [--seconds N]\n"
      << "  smoothwheel accelerate DEVICE [--profile NAME] [--seconds N]\n"
      << "  smoothwheel accelerate --profiles\n"
      << "  smoothwheel experiment --list\n"
      << "  smoothwheel experiment PRESET [--axis vertical|horizontal] [--direction +/-1] [--delay SECONDS] [--dry-run]\n"
      << "  smoothwheel --help\n"
      << "  smoothwheel --version\n\n"
      << "Checkpoint 1 commands are read-only: they never grab a device and\n"
      << "never inject input. 'monitor' prints wheel events by default; --all\n"
      << "shows every event while --record saves the complete raw event stream.\n\n"
      << "Checkpoint 2 experiment commands create only a temporary uinput device.\n"
      << "They do not grab or modify a physical input device.\n\n"
      << "The relay command is an experimental Checkpoint 3/4 safety test. It\n"
      << "temporarily grabs the selected pointer and mirrors it through uinput.\n";
}

int devices() {
  const auto found = smoothwheel::discover_input_devices();
  if (found.empty()) {
    std::cout << "No readable /dev/input/event* devices found.\n"
              << "Your account may not have permission to read input devices.\n";
    return 0;
  }
  for (const auto& d : found) {
    if (d.relative_pointer || d.wheel || d.hi_res_wheel || d.horizontal_wheel || d.hi_res_horizontal_wheel)
      std::cout << smoothwheel::describe_device(d) << "\n\n";
  }
  return 0;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc == 1) { print_help(); return 0; }
  const std::string_view arg{argv[1]};
  if (arg == "--help" || arg == "-h") { print_help(); return 0; }
  if (arg == "--version" || arg == "-V") { std::cout << "smoothwheel " << smoothwheel::kVersion << '\n'; return 0; }
  if (arg == "devices" && argc == 2) return devices();
  if (arg == "inspect" && argc == 3) {
    auto d = smoothwheel::inspect_input_device(argv[2]);
    if (!d) { std::cerr << "smoothwheel: cannot inspect " << argv[2] << "\n"; return 1; }
    std::cout << smoothwheel::describe_device(*d) << '\n'; return 0;
  }

  if (arg == "experiment" && argc >= 3) {
    const std::string_view sub{argv[2]};
    if (sub == "--list" && argc == 3) {
      for (const auto& preset : smoothwheel::experiment_presets())
        std::cout << preset.name << "  " << preset.description << '\n';
      return 0;
    }
    const auto* preset = smoothwheel::find_experiment_preset(std::string(sub));
    if (!preset) { std::cerr << "smoothwheel: unknown experiment preset: " << sub << '\n'; return 2; }
    smoothwheel::ScrollAxis axis = smoothwheel::ScrollAxis::Vertical;
    int direction = -1;
    int delay = 3;
    bool dry_run = false;
    for (int i = 3; i < argc; ++i) {
      const std::string_view opt{argv[i]};
      if (opt == "--dry-run") dry_run = true;
      else if (opt == "--axis" && i + 1 < argc) {
        const std::string_view value{argv[++i]};
        if (value == "vertical") axis = smoothwheel::ScrollAxis::Vertical;
        else if (value == "horizontal") axis = smoothwheel::ScrollAxis::Horizontal;
        else { std::cerr << "smoothwheel: axis must be vertical or horizontal\n"; return 2; }
      } else if (opt == "--direction" && i + 1 < argc) {
        const std::string_view value{argv[++i]};
        if (value == "+1" || value == "1") direction = 1;
        else if (value == "-1") direction = -1;
        else { std::cerr << "smoothwheel: direction must be +1 or -1\n"; return 2; }
      } else if (opt == "--delay" && i + 1 < argc) {
        try { delay = std::stoi(argv[++i]); } catch (...) { std::cerr << "smoothwheel: invalid delay\n"; return 2; }
        if (delay < 0 || delay > 30) { std::cerr << "smoothwheel: delay must be between 0 and 30 seconds\n"; return 2; }
      } else { std::cerr << "smoothwheel: invalid experiment option: " << opt << '\n'; return 2; }
    }
    const auto packets = smoothwheel::plan_scroll(*preset, direction);
    if (dry_run) { std::cout << smoothwheel::describe_plan(*preset, axis, direction, packets); return 0; }
    return smoothwheel::run_virtual_scroll_experiment(*preset, axis, direction, delay);
  }
  if (arg == "accelerate" && argc == 3 && std::string_view(argv[2]) == "--profiles") {
    for (const auto& profile : smoothwheel::acceleration_profiles())
      std::cout << profile.name << "  " << profile.description << '\n';
    return 0;
  }
  if (arg == "accelerate" && argc >= 3) {
    int seconds = 20; std::string profile = "balanced";
    for (int i = 3; i < argc; ++i) {
      std::string_view opt{argv[i]};
      if (opt == "--seconds" && i + 1 < argc) { try { seconds = std::stoi(argv[++i]); } catch (...) { std::cerr << "smoothwheel: invalid seconds\n"; return 2; } }
      else if (opt == "--profile" && i + 1 < argc) profile = argv[++i];
      else { std::cerr << "smoothwheel: invalid accelerate option: " << opt << '\n'; return 2; }
    }
    return smoothwheel::run_accelerated_relay(argv[2], profile, seconds, 3, std::cout);
  }
  if (arg == "relay" && argc >= 3) {
    int seconds = 10;
    for (int i = 3; i < argc; ++i) {
      std::string_view opt{argv[i]};
      if (opt == "--seconds" && i + 1 < argc) { try { seconds = std::stoi(argv[++i]); } catch (...) { std::cerr << "smoothwheel: invalid seconds\n"; return 2; } }
      else { std::cerr << "smoothwheel: invalid relay option: " << opt << '\n'; return 2; }
    }
    return smoothwheel::run_pointer_relay(argv[2], seconds, 3, std::cout);
  }
  if (arg == "monitor" && argc >= 3) {
    bool wheel_only = true; std::string record_path;
    for (int i = 3; i < argc; ++i) {
      std::string_view opt{argv[i]};
      if (opt == "--all") wheel_only = false;
      else if (opt == "--record" && i + 1 < argc) record_path = argv[++i];
      else { std::cerr << "smoothwheel: invalid monitor option: " << opt << '\n'; return 2; }
    }
    std::ofstream record;
    if (!record_path.empty()) { record.open(record_path); if (!record) { std::cerr << "smoothwheel: cannot create " << record_path << '\n'; return 1; } }
    return smoothwheel::monitor_input_device(argv[2], std::cout, record_path.empty() ? nullptr : &record, wheel_only);
  }
  std::cerr << "smoothwheel: invalid arguments\nTry 'smoothwheel --help'.\n"; return 2;
}
