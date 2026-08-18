# SmoothWheel roadmap

The first development phase is risk-first. The project should prove the Linux input pipeline, safety model and application compatibility before investing in a GUI, per-application profiles or broad packaging.

## Checkpoint 1 — Input reconnaissance ✅

**Goal:** understand exactly what real mice deliver before transforming anything.

- enumerate candidate `/dev/input/event*` devices and their capabilities;
- identify wheel-capable pointer devices without hard-coded event numbers;
- record `REL_WHEEL`, `REL_HWHEEL`, `REL_WHEEL_HI_RES` and `REL_HWHEEL_HI_RES` sequences;
- capture representative low-resolution and high-resolution mouse traces;
- make device/event inspection available through a non-invasive CLI mode;
- add fixtures so recorded traces can feed later tests without physical hardware.

**Exit evidence:** Complete. SmoothWheel identifies and inspects wheel-capable event nodes, records complete input streams read-only, and includes a permanent fixture derived from real hardware evidence.

## Checkpoint 2 — Virtual pointer feasibility spike ✅

**Goal:** prove that a uinput device can produce useful fine-grained wheel motion through the normal desktop stack.

- create a minimal virtual pointer using uinput/libevdev;
- emit vertical and horizontal wheel events;
- experiment with fractional `REL_*_HI_RES` sequences;
- inspect the resulting libinput `v120` events;
- test at least a browser, terminal/editor and native toolkit application;
- record whether applications actually render the stream smoothly.

**Exit evidence:** Complete enough for the architecture decision. A temporary uinput device successfully emitted conventional and fractional wheel gestures on the real desktop without destabilizing input. The subjective difference between fractional presets was small, which shifted the product target away from smoothing-for-its-own-sake toward intent-preserving velocity acceleration.

## Checkpoint 3 — Transparent pointer pass-through ✅

**Goal:** reproduce an input device faithfully before changing any wheel semantics.

- clone relevant physical device capabilities into the virtual device;
- forward relative motion, buttons and supported non-wheel events unchanged;
- preserve event packet boundaries (`SYN_REPORT` semantics);
- prevent the virtual device from being rediscovered as a physical source;
- measure added pointer latency and event loss;
- build deterministic fixture-driven pass-through tests.

**Exit evidence:** Complete on the current hardware gate. Real-device tests verified normal pointer movement, buttons, scrolling, no obvious duplication, timed expiry, and Ctrl-C recovery while the physical pointer was exclusively relayed through uinput.

## Checkpoint 4 — Safe exclusive capture 🚧

**Goal:** take ownership of a physical pointer without duplicate events or fragile recovery.

- use `EVIOCGRAB` through libevdev for a selected device;
- forward every required event through the virtual pointer;
- guarantee grab release on normal shutdown and handled signals;
- test daemon termination, exceptions and device disconnects;
- build a watchdog/fail-open strategy if needed;
- document safe development/recovery procedure.

**Current state:** normal expiry and Ctrl-C recovery have been exercised successfully on real hardware. The relay creates the virtual device before acquiring `EVIOCGRAB`, releases through RAII on controlled paths, and relies on descriptor teardown for process-death fail-open behavior. Forced process death and source-disconnect recovery remain to be exercised before CP4 is closed.

**Safety gate:** do not enable automatic startup until repeated crash/disconnect tests demonstrate reliable pointer recovery.

## Checkpoint 5 — Velocity model and motion engine v1 🚧

**Goal:** preserve user intent by making slow wheel input precise and fast wheel input accelerate strongly, implemented as a pure deterministic component independent of Linux I/O.

- estimate physical wheel cadence in normalized `v120` units;
- map cadence to an explicit acceleration multiplier;
- reset acceleration immediately on direction reversal;
- allow slow isolated detents to run below baseline for finer precision;
- maintain velocity/momentum state only where it improves responsiveness;
- emit fixed-step or time-based fractional output;
- implement decay and rapid-input accumulation;
- preserve total intended scroll distance within defined rounding bounds;
- handle reversal without long unwanted tails;
- unit-test timing, conservation, cancellation and determinism using a fake clock.

**Current state:** the pure cadence estimator and a packet-level wheel transformer are implemented and deterministic. The transformer keeps non-wheel events untouched, treats each `SYN_REPORT` packet as the transformation unit, scales paired legacy/high-resolution wheel representations coherently, and exposes deliberately distinct `precision`, `balanced`, `fast`, and `aggressive` profiles. Two real-hardware tuning gates established that the overall balanced shape is useful but needs more dynamic range in both directions. Balanced now targets roughly 0.45x at isolated/very slow input and 9x at hard-spin saturation, with a steeper curve that keeps the middle controlled. Because sub-1x motion cannot be represented honestly by independently rounding the legacy `REL_WHEEL` companion, the packet transformer now accumulates fractional legacy distance while emitting fractional `REL_WHEEL_HI_RES` immediately. Momentum/decay remains intentionally deferred until the cadence curve itself feels right.

**Exit evidence:** the complete motion model can be exhaustively tested from event fixtures without `/dev/input` or `/dev/uinput`.

## Checkpoint 6 — Complete wheel semantics

**Goal:** make the engine correct across the wheel behaviours Linux exposes.

- low-resolution detent wheels;
- high-resolution/free-spin wheels;
- horizontal wheels;
- simultaneous/mixed axes;
- rapid direction reversal;
- bursts spanning multiple detents;
- natural-scroll interaction (avoid implementing compositor policy twice);
- preserve physical high-resolution input rather than degrading it.

**Exit evidence:** a corpus of real and synthetic traces exercises every supported wheel mode.

## Checkpoint 7 — Daemon lifecycle, permissions and configuration

**Goal:** turn the prototype into an everyday background utility without making it a desktop application yet.

- stable `smoothwheel` CLI and foreground diagnostic mode;
- device selection by stable identity rather than event node number;
- minimal configuration file with validated values;
- clean hotplug/reconnect handling;
- decide and document udev/group/systemd-user permission model;
- optional systemd user/service integration where appropriate;
- clear diagnostics for missing permissions or unsupported devices.

**Exit evidence:** a user can install, configure, start, stop and diagnose SmoothWheel without running an opaque root daemon.

## Checkpoint 8 — Desktop/application compatibility matrix

**Goal:** establish the actual system-wide claim.

Test representative combinations of:

- Wayland compositors/desktops;
- XWayland applications;
- native X11 sessions where practical;
- Chromium/Chrome and Firefox;
- Electron;
- GTK and Qt applications;
- terminals/editors;
- PDF/document viewers;
- applications with their own smooth-scrolling behaviour.

Record behavioural differences rather than hiding them behind app-specific hacks. Add compatibility workarounds only when the abstraction is defensible.

**Exit evidence:** published matrix of tested environments and known limitations.

## Checkpoint 9 — Feel, latency and default-profile tuning

**Goal:** make SmoothWheel not merely functional but noticeably pleasant.

- timestamp physical input and virtual emission;
- measure scheduler jitter and end-to-end added latency where possible;
- compare several small smoothing models objectively and subjectively;
- test 60/100/120/144/165/240 Hz displays without tying behaviour to refresh rate;
- tune one strong default profile before exposing many knobs;
- add bounded speed/acceleration controls only where user value is demonstrated.

**Exit evidence:** repeatable timing benchmarks plus a stable default profile that works across the compatibility matrix.

## Checkpoint 10 — Hardening and first public release

**Goal:** make the first release boring to install and difficult to break.

- ASan/UBSan/LSan runs over deterministic components and event fixtures;
- fuzz config and event-trace parsers where worthwhile;
- repeated start/stop/grab/ungrab cycles;
- hotplug and suspend/resume stress;
- malformed/disappearing device tests;
- CPU/RSS idle and sustained-scroll measurements;
- packaging and clean-machine installation tests;
- documentation reconciliation;
- tag the first release only after safe-recovery tests pass.

## After the first ten checkpoints

Possible later work includes per-device profiles, per-application behaviour where desktop APIs make it reliable, a lightweight settings UI, additional distributions/package managers, richer telemetry-free diagnostics and experimental motion models. None of these should complicate the core input path until the base behaviour is proven.
