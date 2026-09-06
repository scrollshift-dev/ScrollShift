#include "scrollshift/config.hpp"
#include "scrollshift/input.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <linux/input.h>
#include <unistd.h>

namespace {
using namespace scrollshift;

DeviceInfo mouse_shape() {
  DeviceInfo d;
  d.path = "/dev/input/event1";
  d.name = "Test Mouse";
  d.vendor = 0x1234;
  d.product = 0x5678;
  d.bus = BUS_USB;
  d.relative_pointer = true;
  d.has_rel_x = true;
  d.has_rel_y = true;
  d.wheel = true;
  d.hi_res_wheel = true;
  d.mouse_button = true;
  return d;
}

void check(const char* label, bool actual, bool expected) {
  if (actual != expected) {
    std::cerr << "[classifier] FAIL: " << label << " expected " << expected << " got " << actual << '\n';
    std::exit(1);
  }
}

std::filesystem::path write_udev_data(const std::filesystem::path& root, unsigned major, unsigned minor,
                                      const std::string& properties) {
  std::filesystem::create_directories(root);
  const auto path = root / ("c" + std::to_string(major) + ":" + std::to_string(minor));
  std::ofstream out(path);
  out << properties;
  return path;
}
}  // namespace

int main() {
  using namespace scrollshift;

  // udev property parsing is exercised through an injectable data root.
  {
    const auto root = std::filesystem::temp_directory_path() /
        ("scrollshift-classifier-" + std::to_string(::getpid()));
    DeviceInfo mouse;
    write_udev_data(root, 13, 100, "E:ID_INPUT=1\nE:ID_INPUT_MOUSE=1\n");
    apply_udev_classification(13, 100, root, mouse);
    check("udev mouse tag", mouse.is_mouse, true);
    check("udev classified", mouse.udev_classified, true);

    DeviceInfo joystick;
    write_udev_data(root, 13, 101, "E:ID_INPUT=1\nE:ID_INPUT_JOYSTICK=1\n");
    apply_udev_classification(13, 101, root, joystick);
    check("udev joystick tag", joystick.is_joystick, true);
    check("udev joystick classified", joystick.udev_classified, true);

    DeviceInfo conflict;
    write_udev_data(root, 13, 102, "E:ID_INPUT_MOUSE=1\nE:ID_INPUT_TOUCHPAD=1\n");
    apply_udev_classification(13, 102, root, conflict);
    check("udev conflict both set", conflict.is_mouse && conflict.is_touchpad, true);

    DeviceInfo key_only;
    write_udev_data(root, 13, 103, "E:ID_INPUT=1\nE:ID_INPUT_KEY=1\n");
    apply_udev_classification(13, 103, root, key_only);
    check("udev key-only classified", key_only.udev_classified, true);
    check("udev key-only not mouse", key_only.is_mouse, false);

    DeviceInfo tablet_pad;
    write_udev_data(root, 13, 104, "E:ID_INPUT_TABLET_PAD=1\n");
    apply_udev_classification(13, 104, root, tablet_pad);
    check("udev tablet-pad tag", tablet_pad.is_tablet_pad, true);

    DeviceInfo absent;
    apply_udev_classification(13, 105, root, absent);
    check("udev missing file unclassified", absent.udev_classified, false);

    std::filesystem::remove_all(root);
  }

  // Explicit udev mouse/trackball classifications are trusted.
  {
    auto d = mouse_shape();
    d.udev_classified = true; d.is_mouse = true;
    check("conventional mouse (udev)", is_automatic_mouse_candidate(d), true);
  }
  {
    auto d = mouse_shape();
    d.wheel = false; d.hi_res_wheel = true;
    d.udev_classified = true; d.is_mouse = true;
    check("high-resolution-only mouse (udev)", is_automatic_mouse_candidate(d), true);
  }
  {
    auto d = mouse_shape();
    d.udev_classified = true; d.is_mouse = true;
    d.name = "Trackball";
    check("trackball (udev mouse)", is_automatic_mouse_candidate(d), true);
  }
  {
    // A pointing stick is a mouse-like class; without a wheel it still fails the
    // capture gate, and with a wheel it is acceptable.
    auto stick = mouse_shape();
    stick.wheel = false; stick.hi_res_wheel = false;
    stick.udev_classified = true; stick.is_pointingstick = true;
    check("pointing stick without wheel", is_automatic_mouse_candidate(stick), false);
    auto stick_wheel = mouse_shape();
    stick_wheel.udev_classified = true; stick_wheel.is_pointingstick = true;
    check("pointing stick with wheel", is_automatic_mouse_candidate(stick_wheel), true);
  }

  // Known non-mouse classes are rejected even when mouse-shaped.
  {
    auto d = mouse_shape(); d.udev_classified = true; d.is_touchpad = true;
    check("touchpad", is_automatic_mouse_candidate(d), false);
  }
  {
    auto d = mouse_shape(); d.udev_classified = true; d.is_touchscreen = true;
    check("touchscreen", is_automatic_mouse_candidate(d), false);
  }
  {
    auto d = mouse_shape(); d.udev_classified = true; d.is_joystick = true;
    check("udev-classified joystick", is_automatic_mouse_candidate(d), false);
  }
  {
    auto d = mouse_shape(); d.udev_classified = true; d.is_tablet = true;
    check("udev-classified tablet", is_automatic_mouse_candidate(d), false);
  }
  {
    auto d = mouse_shape(); d.udev_classified = true; d.is_tablet_pad = true;
    check("udev-classified tablet pad", is_automatic_mouse_candidate(d), false);
  }
  {
    // Conflicting metadata: udev says both mouse and touchpad; touchpad wins.
    auto d = mouse_shape(); d.udev_classified = true; d.is_mouse = true; d.is_touchpad = true;
    check("conflicting mouse+touchpad", is_automatic_mouse_candidate(d), false);
  }
  {
    // Classified as a key-only device: no fallback may reclassify it as a mouse.
    auto d = mouse_shape(); d.udev_classified = true; d.is_keyboard = true;
    check("udev keyboard classification", is_automatic_mouse_candidate(d), false);
  }
  {
    auto d = mouse_shape(); d.udev_classified = true;  // recognized non-mouse class, no mouse flag
    check("recognized non-mouse class blocks fallback", is_automatic_mouse_candidate(d), false);
  }

  // Keyboard consumer-control and composite auxiliary nodes have no relative
  // pointer signature even when they expose wheel codes.
  {
    auto cc = mouse_shape();
    cc.name = "Consumer Control"; cc.relative_pointer = false; cc.has_rel_x = false; cc.has_rel_y = false;
    cc.udev_classified = true; cc.is_keyboard = true;
    check("keyboard consumer-control node", is_automatic_mouse_candidate(cc), false);
  }
  {
    auto aux = mouse_shape();
    aux.name = "Auxiliary"; aux.relative_pointer = false; aux.has_rel_x = false; aux.has_rel_y = false;
    aux.wheel = false; aux.horizontal_wheel = true;
    check("composite auxiliary node", is_automatic_mouse_candidate(aux), false);
  }

  // Capability fallback (no udev metadata) is conservative.
  {
    auto d = mouse_shape();  // no udev_classified, no is_mouse
    check("fallback real-mouse shape", is_automatic_mouse_candidate(d), true);
  }
  {
    auto d = mouse_shape(); d.udev_classified = false;
    d.is_joystick = false;
    d.bus = BUS_VIRTUAL;
    check("fallback unrelated virtual pointer", is_automatic_mouse_candidate(d), false);
  }
  {
    auto d = mouse_shape(); d.abs_axes = true;
    check("fallback rejects absolute axes", is_automatic_mouse_candidate(d), false);
  }
  {
    // Harmless absolute capability (e.g. ABS_WHEEL, no ABS_X/ABS_Y) must not be
    // treated like tablet-style motion.
    auto d = mouse_shape(); d.abs_axes = false; d.udev_classified = false;
    d.has_rel_x = true; d.has_rel_y = true; d.wheel = true; d.mouse_button = true;
    check("fallback permits harmless abs", is_automatic_mouse_candidate(d), true);
  }
  {
    auto d = mouse_shape(); d.has_rel_x = true; d.has_rel_y = false;
    check("fallback requires REL_X and REL_Y", is_automatic_mouse_candidate(d), false);
  }
  {
    auto d = mouse_shape(); d.mouse_button = false;
    check("fallback requires mouse button", is_automatic_mouse_candidate(d), false);
  }
  {
    auto d = mouse_shape(); d.wheel = false; d.hi_res_wheel = false; d.horizontal_wheel = true;
    check("fallback requires vertical wheel", is_automatic_mouse_candidate(d), false);
  }

  // ScrollShift's own virtual device is always excluded.
  {
    auto d = mouse_shape();
    d.vendor = 0x5357; d.product = 0x0003; d.name = "ScrollShift Accelerated";
    d.udev_classified = false;
    check("ScrollShift virtual device", is_automatic_mouse_candidate(d), false);
  }

  std::cout << "classifier tests passed\n";
  return 0;
}