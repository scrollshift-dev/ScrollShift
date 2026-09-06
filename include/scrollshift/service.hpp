#pragma once
#include <iosfwd>
#include <string_view>

namespace scrollshift {
int run_service_command(std::string_view action, bool follow_logs, std::ostream& out, std::ostream& err);
}
