#include "scrollshift/input.hpp"

#include <cassert>
#include <fstream>
#include <iostream>
#include <linux/input.h>

#ifndef SCROLLSHIFT_SOURCE_DIR
#error "SCROLLSHIFT_SOURCE_DIR must be defined for fixture tests"
#endif

int main() {
  using namespace scrollshift;
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

  std::ifstream fixture(std::string(SCROLLSHIFT_SOURCE_DIR) + "/tests/fixtures/nick-2.4g-wireless-mouse-vertical.trace");
  assert(fixture);
  const auto events = load_recorded_trace(fixture);
  const auto summary = summarize_trace(events);
  assert(summary.events == 27);
  assert(summary.reports == 9);
  assert(summary.vertical_low_res == 9);
  assert(summary.vertical_hi_res == 9);
  assert(summary.vertical_low_res_total == 3);
  assert(summary.vertical_hi_res_total == 360);
  assert(summary.horizontal_low_res == 0);
  assert(summary.horizontal_hi_res == 0);

  // The captured mouse duplicated every vertical detent into a low-res +/-1
  // event and a high-res +/-120 event in the same timestamped packet.
  for (std::size_t i = 0; i < events.size(); i += 3) {
    assert(events[i].type == EV_REL && events[i].code == REL_WHEEL);
    assert(events[i + 1].type == EV_REL && events[i + 1].code == REL_WHEEL_HI_RES);
    assert(events[i + 2].type == EV_SYN && events[i + 2].code == SYN_REPORT);
    assert(events[i + 1].value == events[i].value * 120);
    assert(events[i + 1].sec == events[i].sec && events[i + 1].usec == events[i].usec);
  }

  std::cout << "input tests passed\n";
}
