#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

#include "smoothwheel/input.hpp"
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
      << "  smoothwheel --help\n"
      << "  smoothwheel --version\n\n"
      << "Checkpoint 1 commands are read-only: they never grab a device and\n"
      << "never inject input. 'monitor' prints wheel events by default; --all\n"
      << "shows every event while --record saves the complete raw event stream.\n";
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
