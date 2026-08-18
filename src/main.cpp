#include <iostream>
#include <string_view>

#include "smoothwheel/version.hpp"

namespace {
void print_help() {
  std::cout
      << "SmoothWheel " << smoothwheel::kVersion << '\n'
      << "Native smooth mouse-wheel scrolling for Linux.\n\n"
      << "Usage:\n"
      << "  smoothwheel --help\n"
      << "  smoothwheel --version\n\n"
      << "Smooth scrolling is not implemented yet. The first checkpoints are\n"
      << "focused on Linux input discovery, virtual-device output, and safe\n"
      << "event pass-through before exclusive capture is enabled.\n";
}
}  // namespace

int main(int argc, char** argv) {
  if (argc == 1) {
    print_help();
    return 0;
  }

  const std::string_view arg{argv[1]};
  if (arg == "--help" || arg == "-h") {
    print_help();
    return 0;
  }
  if (arg == "--version" || arg == "-V") {
    std::cout << "smoothwheel " << smoothwheel::kVersion << '\n';
    return 0;
  }

  std::cerr << "smoothwheel: unknown argument: " << arg << '\n';
  std::cerr << "Try 'smoothwheel --help'.\n";
  return 2;
}
