#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <unistd.h>

#include "scrollshift/config.hpp"
#include "scrollshift/daemon.hpp"
#include "scrollshift/environment.hpp"
#include "scrollshift/experiment.hpp"
#include "scrollshift/input.hpp"
#include "scrollshift/relay.hpp"
#include "scrollshift/service.hpp"
#include "scrollshift/transform.hpp"
#include "scrollshift/version.hpp"

namespace {
void print_help() {
  std::cout
      << "ScrollShift " << scrollshift::kVersion << '\n'
      << "Native smooth mouse-wheel scrolling for Linux.\n\n"
      << "Usage:\n"
      << "  scrollshift devices\n"
      << "  scrollshift environment\n"
      << "  scrollshift configure DEVICE [--profile NAME] [--config FILE]\n"
      << "  scrollshift daemon [--config FILE]\n"
      << "  scrollshift doctor [--config FILE]\n"
      << "  scrollshift service install|uninstall|start|stop|restart|status|enable|disable|logs [--follow]\n"
      << "  scrollshift inspect DEVICE\n"
      << "  scrollshift monitor DEVICE [--all] [--record FILE]\n"
      << "  scrollshift relay DEVICE [--seconds N]\n"
      << "  scrollshift accelerate DEVICE [--profile NAME] [--seconds N]\n"
      << "  scrollshift accelerate --profiles\n"
      << "  scrollshift experiment --list\n"
      << "  scrollshift experiment PRESET [--axis vertical|horizontal] [--direction +/-1] [--delay SECONDS] [--dry-run]\n"
      << "  scrollshift --help\n"
      << "  scrollshift --version\n\n"
      << "Checkpoint 1 commands are read-only: they never grab a device and\n"
      << "never inject input. 'monitor' prints wheel events by default; --all\n"
      << "shows every event while --record saves the complete raw event stream.\n\n"
      << "Checkpoint 2 experiment commands create only a temporary uinput device.\n"
      << "They do not grab or modify a physical input device.\n\n"
      << "The installed daemon discovers conventional wheel mice automatically, ignores\n"
      << "touchpads/touchscreens, and handles hotplug without persisting event-node paths.\n";
}

int devices() {
  const auto found = scrollshift::discover_input_devices();
  if (found.empty()) {
    std::cout << "No readable /dev/input/event* devices found.\n"
              << "Your account may not have permission to read input devices.\n";
    return 0;
  }
  for (const auto& d : found) {
    if (d.relative_pointer || d.wheel || d.hi_res_wheel || d.horizontal_wheel || d.hi_res_horizontal_wheel)
      std::cout << scrollshift::describe_device(d) << "\n\n";
  }
  return 0;
}


}  // namespace

int main(int argc, char** argv) {
  if (argc == 1) { print_help(); return 0; }
  const std::string_view arg{argv[1]};
  if (arg == "--help" || arg == "-h") { print_help(); return 0; }
  if (arg == "--version" || arg == "-V") { std::cout << "scrollshift " << scrollshift::kVersion << '\n'; return 0; }
  if (arg == "devices" && argc == 2) return devices();
  if (arg == "environment" && argc == 2) return scrollshift::print_environment(std::cout);
  if (arg == "configure" && argc >= 3) {
    std::string profile = "balanced";
    std::filesystem::path config_path = "/etc/scrollshift/config.conf";
    for (int i = 3; i < argc; ++i) {
      const std::string_view opt{argv[i]};
      if (opt == "--profile" && i + 1 < argc) profile = argv[++i];
      else if (opt == "--config" && i + 1 < argc) config_path = argv[++i];
      else { std::cerr << "scrollshift: invalid configure option: " << opt << '\n'; return 2; }
    }
    return scrollshift::write_config_for_device(argv[2], config_path, profile, std::cout);
  }
  if (arg == "daemon") {
    std::filesystem::path config_path = "/etc/scrollshift/config.conf";
    for (int i = 2; i < argc; ++i) {
      const std::string_view opt{argv[i]};
      if (opt == "--config" && i + 1 < argc) config_path = argv[++i];
      else { std::cerr << "scrollshift: invalid daemon option: " << opt << '\n'; return 2; }
    }
    return scrollshift::run_daemon(config_path, std::cout);
  }
  if (arg == "doctor") {
    std::filesystem::path config_path = "/etc/scrollshift/config.conf";
    for (int i = 2; i < argc; ++i) {
      const std::string_view opt{argv[i]};
      if (opt == "--config" && i + 1 < argc) config_path = argv[++i];
      else { std::cerr << "scrollshift: invalid doctor option: " << opt << '\n'; return 2; }
    }
    return scrollshift::run_doctor(config_path, std::cout);
  }
  if (arg == "service" && argc >= 3) {
    bool follow = false;
    if (argc == 4 && std::string_view(argv[3]) == "--follow" && std::string_view(argv[2]) == "logs") follow = true;
    else if (argc != 3) { std::cerr << "scrollshift: invalid service options\n"; return 2; }
    return scrollshift::run_service_command(argv[2], follow, std::cout, std::cerr);
  }
  if (arg == "inspect" && argc == 3) {
    auto d = scrollshift::inspect_input_device(argv[2]);
    if (!d) { std::cerr << "scrollshift: cannot inspect " << argv[2] << "\n"; return 1; }
    std::cout << scrollshift::describe_device(*d) << '\n'; return 0;
  }

  if (arg == "experiment" && argc >= 3) {
    const std::string_view sub{argv[2]};
    if (sub == "--list" && argc == 3) {
      for (const auto& preset : scrollshift::experiment_presets())
        std::cout << preset.name << "  " << preset.description << '\n';
      return 0;
    }
    const auto* preset = scrollshift::find_experiment_preset(std::string(sub));
    if (!preset) { std::cerr << "scrollshift: unknown experiment preset: " << sub << '\n'; return 2; }
    scrollshift::ScrollAxis axis = scrollshift::ScrollAxis::Vertical;
    int direction = -1;
    int delay = 3;
    bool dry_run = false;
    for (int i = 3; i < argc; ++i) {
      const std::string_view opt{argv[i]};
      if (opt == "--dry-run") dry_run = true;
      else if (opt == "--axis" && i + 1 < argc) {
        const std::string_view value{argv[++i]};
        if (value == "vertical") axis = scrollshift::ScrollAxis::Vertical;
        else if (value == "horizontal") axis = scrollshift::ScrollAxis::Horizontal;
        else { std::cerr << "scrollshift: axis must be vertical or horizontal\n"; return 2; }
      } else if (opt == "--direction" && i + 1 < argc) {
        const std::string_view value{argv[++i]};
        if (value == "+1" || value == "1") direction = 1;
        else if (value == "-1") direction = -1;
        else { std::cerr << "scrollshift: direction must be +1 or -1\n"; return 2; }
      } else if (opt == "--delay" && i + 1 < argc) {
        try { delay = std::stoi(argv[++i]); } catch (...) { std::cerr << "scrollshift: invalid delay\n"; return 2; }
        if (delay < 0 || delay > 30) { std::cerr << "scrollshift: delay must be between 0 and 30 seconds\n"; return 2; }
      } else { std::cerr << "scrollshift: invalid experiment option: " << opt << '\n'; return 2; }
    }
    const auto packets = scrollshift::plan_scroll(*preset, direction);
    if (dry_run) { std::cout << scrollshift::describe_plan(*preset, axis, direction, packets); return 0; }
    return scrollshift::run_virtual_scroll_experiment(*preset, axis, direction, delay);
  }
  if (arg == "accelerate" && argc == 3 && std::string_view(argv[2]) == "--profiles") {
    for (const auto& profile : scrollshift::acceleration_profiles())
      std::cout << profile.name << "  " << profile.description << '\n';
    return 0;
  }
  if (arg == "accelerate" && argc >= 3) {
    int seconds = 20; std::string profile = "balanced";
    for (int i = 3; i < argc; ++i) {
      std::string_view opt{argv[i]};
      if (opt == "--seconds" && i + 1 < argc) { try { seconds = std::stoi(argv[++i]); } catch (...) { std::cerr << "scrollshift: invalid seconds\n"; return 2; } }
      else if (opt == "--profile" && i + 1 < argc) profile = argv[++i];
      else { std::cerr << "scrollshift: invalid accelerate option: " << opt << '\n'; return 2; }
    }
    return scrollshift::run_accelerated_relay(argv[2], profile, seconds, 3, std::cout);
  }
  if (arg == "relay" && argc >= 3) {
    int seconds = 10;
    for (int i = 3; i < argc; ++i) {
      std::string_view opt{argv[i]};
      if (opt == "--seconds" && i + 1 < argc) { try { seconds = std::stoi(argv[++i]); } catch (...) { std::cerr << "scrollshift: invalid seconds\n"; return 2; } }
      else { std::cerr << "scrollshift: invalid relay option: " << opt << '\n'; return 2; }
    }
    return scrollshift::run_pointer_relay(argv[2], seconds, 3, std::cout);
  }
  if (arg == "monitor" && argc >= 3) {
    bool wheel_only = true; std::string record_path;
    for (int i = 3; i < argc; ++i) {
      std::string_view opt{argv[i]};
      if (opt == "--all") wheel_only = false;
      else if (opt == "--record" && i + 1 < argc) record_path = argv[++i];
      else { std::cerr << "scrollshift: invalid monitor option: " << opt << '\n'; return 2; }
    }
    std::ofstream record;
    if (!record_path.empty()) { record.open(record_path); if (!record) { std::cerr << "scrollshift: cannot create " << record_path << '\n'; return 1; } }
    return scrollshift::monitor_input_device(argv[2], std::cout, record_path.empty() ? nullptr : &record, wheel_only);
  }
  std::cerr << "scrollshift: invalid arguments\nTry 'scrollshift --help'.\n"; return 2;
}
