#include "smoothwheel/input.hpp"
#include <cassert>
#include <iostream>
#include <linux/input.h>

int main() {
  using namespace smoothwheel;
  RecordedEvent e{123, 456789, EV_REL, REL_WHEEL_HI_RES, -15};
  const auto line = serialize_event(e);
  const auto parsed = parse_recorded_event(line);
  assert(parsed);
  assert(parsed->sec == e.sec && parsed->usec == e.usec && parsed->type == e.type && parsed->code == e.code && parsed->value == e.value);
  assert(event_code_name(EV_REL, REL_WHEEL) == "REL_WHEEL");
  assert(event_code_name(EV_REL, REL_HWHEEL_HI_RES) == "REL_HWHEEL_HI_RES");
  assert(format_event(e).find("REL_WHEEL_HI_RES") != std::string::npos);
  assert(!parse_recorded_event("garbage"));
  assert(!parse_recorded_event("1 2 999999 4 5"));
  assert(!parse_recorded_event("1 2 3 4 5 trailing"));
  std::cout << "input tests passed\n";
}
