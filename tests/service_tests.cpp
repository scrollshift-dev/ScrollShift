#include "scrollshift/service.hpp"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace {
using namespace scrollshift;
namespace fs = std::filesystem;

std::string read_file(const fs::path& path) {
  std::ifstream in(path);
  return std::string(std::istreambuf_iterator<char>(in), {});
}

fs::path temp_dir() {
  const auto root = fs::temp_directory_path() / ("scrollshift-service-test-" + std::to_string(::getpid()));
  fs::remove_all(root);
  fs::create_directories(root);
  return root;
}

const char* name(ServiceUnitStatus status) {
  switch (status) {
    case ServiceUnitStatus::NotInstalled: return "NotInstalled";
    case ServiceUnitStatus::Current: return "Current";
    case ServiceUnitStatus::Historical: return "Historical";
    case ServiceUnitStatus::Modified: return "Modified";
    case ServiceUnitStatus::Unrelated: return "Unrelated";
  }
  return "?";
}

void expect(ServiceUnitStatus actual, ServiceUnitStatus expected, const char* label) {
  if (actual != expected) {
    std::cerr << "[service] FAIL: " << label << " expected " << name(expected) << " got " << name(actual) << '\n';
    std::exit(1);
  }
}

void write(const fs::path& path, const std::string& body) {
  std::ofstream out(path);
  out << body;
}
}  // namespace

int main() {
  using namespace scrollshift;

  const auto formats = service_unit_formats();
  const auto current = formats.back();
  // Two known historical formats are recognized: the pre-release
  // packaging/scrollshift.service.in output and the immediate-prior CLI unit
  // (managed marker without the format-marker line).
  assert(formats.size() == 3);
  const auto service_in_historical = formats[0];
  const auto prior_cli_historical = formats[1];
  const std::string marker = "# Managed by scrollshift. Do not edit manually.";

  // Body classification matrix.
  expect(classify_service_unit(current), ServiceUnitStatus::Current, "exact current format");
  expect(classify_service_unit(service_in_historical), ServiceUnitStatus::Historical, "service.in historical format");
  expect(classify_service_unit(prior_cli_historical), ServiceUnitStatus::Historical, "prior CLI historical format");
  // The prior CLI unit carries the managed marker but is a known exact format,
  // so it must be Historical, never Modified.
  assert(prior_cli_historical.rfind(marker, 0) == 0);

  {
    std::string modified = marker + "\n[Unit]\nDescription=Something edited\n";
    expect(classify_service_unit(modified), ServiceUnitStatus::Modified, "marker-bearing modified unit");
  }
  {
    // A historical body that has been edited is not an exact known format and
    // carries no marker, so it is unrelated (refused), never migrated.
    std::string tweaked = service_in_historical;
    tweaked.replace(tweaked.find("WantedBy=multi-user.target"), 4, "Wanted");
    expect(classify_service_unit(tweaked), ServiceUnitStatus::Unrelated, "edited historical unit");
  }
  {
    expect(classify_service_unit("[Unit]\nDescription=some other service\n"),
           ServiceUnitStatus::Unrelated, "unrelated unit");
  }

  // File-level classification.
  {
    const auto root = temp_dir();
    const fs::path unit = root / "scrollshift.service";
    expect(classify_service_unit_file(unit), ServiceUnitStatus::NotInstalled, "missing unit file");

    write(unit, current);
    std::string detail;
    expect(classify_service_unit_file(unit, &detail), ServiceUnitStatus::Current, "current file");
    assert(detail == "installed (managed)");

    write(unit, prior_cli_historical);
    expect(classify_service_unit_file(unit, &detail), ServiceUnitStatus::Historical, "historical file");
    assert(detail.find("known historical") != std::string::npos);

    write(unit, marker + "\n[Service]\nType=simple\n");
    expect(classify_service_unit_file(unit, &detail), ServiceUnitStatus::Modified, "modified file");

    write(unit, "[Unit]\nDescription=unrelated\n");
    expect(classify_service_unit_file(unit, &detail), ServiceUnitStatus::Unrelated, "unrelated file");

    fs::remove_all(root);
  }

  // Successful migration: prior CLI historical -> current, reload accepted.
  {
    const auto root = temp_dir();
    const fs::path unit = root / "scrollshift.service";
    write(unit, prior_cli_historical);
    std::string error;
    bool ok = replace_service_unit(unit, current, [] { return true; }, error);
    assert(ok);
    expect(classify_service_unit(read_file(unit)), ServiceUnitStatus::Current, "migrated unit");
    fs::remove_all(root);
  }

  // Failed migration: reload refused leaves the original unit byte-for-byte.
  {
    const auto root = temp_dir();
    const fs::path unit = root / "scrollshift.service";
    write(unit, prior_cli_historical);
    const auto before = read_file(unit);
    std::string error;
    bool ok = replace_service_unit(unit, current, [] { return false; }, error);
    assert(!ok);
    assert(!error.empty());
    expect(classify_service_unit(read_file(unit)), ServiceUnitStatus::Historical, "rollback preserves historical");
    assert(read_file(unit) == before);
    fs::remove_all(root);
  }

  // Failed fresh install: reload refused removes the newly installed unit.
  {
    const auto root = temp_dir();
    const fs::path unit = root / "scrollshift.service";
    std::string error;
    bool ok = replace_service_unit(unit, current, [] { return false; }, error);
    assert(!ok);
    assert(!fs::exists(unit));
    fs::remove_all(root);
  }

  // Non-root mutation refusal (only meaningful when the test runs unprivileged).
  if (::geteuid() != 0) {
    std::ostringstream out, err;
    for (const auto* action : {"install", "uninstall", "start", "stop", "restart", "enable", "disable"}) {
      out.str(""); err.str("");
      const int rc = run_service_command(action, false, out, err);
      if (rc == 0) {
        std::cerr << "[service] FAIL: non-root " << action << " unexpectedly returned 0\n";
        std::exit(1);
      }
      if (err.str().empty()) {
        std::cerr << "[service] FAIL: non-root " << action << " produced no diagnostic\n";
        std::exit(1);
      }
    }
  }

  std::cout << "service tests passed\n";
  return 0;
}