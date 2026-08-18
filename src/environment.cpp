#include "scrollshift/environment.hpp"

#include <cstdlib>
#include <ostream>
#include <sys/utsname.h>

namespace scrollshift {
namespace {
const char* env_or_unknown(const char* name) {
  const char* value = std::getenv(name);
  return value && *value ? value : "(unset)";
}
}

int print_environment(std::ostream& output) {
  utsname info{};
  if (::uname(&info) == 0)
    output << "Kernel: " << info.sysname << ' ' << info.release << " (" << info.machine << ")\n";
  else
    output << "Kernel: (uname failed)\n";

  output << "Session type: " << env_or_unknown("XDG_SESSION_TYPE") << '\n'
         << "Desktop: " << env_or_unknown("XDG_CURRENT_DESKTOP") << '\n'
         << "Wayland display: " << env_or_unknown("WAYLAND_DISPLAY") << '\n'
         << "X display: " << env_or_unknown("DISPLAY") << '\n';
  return 0;
}
}  // namespace scrollshift
