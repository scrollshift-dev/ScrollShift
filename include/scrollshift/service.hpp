#pragma once
#include <filesystem>
#include <functional>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace scrollshift {

enum class ServiceUnitStatus { NotInstalled, Current, Historical, Modified, Unrelated };

// Recognizes ScrollShift-generated unit bodies. A unit is trusted only when it
// byte-for-byte matches the exact current template or an exact known historical
// ScrollShift-generated template. A body that merely carries the managed marker
// is treated as modified/untrusted; a body without the marker is unrelated.
ServiceUnitStatus classify_service_unit(const std::string& body);
ServiceUnitStatus classify_service_unit_file(const std::filesystem::path& path, std::string* detail = nullptr);
std::vector<std::string> service_unit_formats();

// Transactionally installs `body` at `unit_path`, calling `reload` to validate
// the new unit with systemd. On reload failure the previous unit content is
// restored (or the newly installed unit removed when none existed before).
bool replace_service_unit(const std::filesystem::path& unit_path, const std::string& body,
                          const std::function<bool()>& reload, std::string& error);

int run_service_command(std::string_view action, bool follow_logs, std::ostream& out, std::ostream& err);
}
