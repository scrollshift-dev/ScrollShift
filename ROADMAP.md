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

## Checkpoint 2 — Virtual pointer feasibility spike 🚧

**Goal:** prove that a uinput device can produce useful fine-grained wheel motion through the normal desktop stack.

- create a minimal virtual pointer using uinput/libevdev;
- emit vertical and horizontal wheel events;
- experiment with fractional `REL_*_HI_RES` sequences;
- inspect the resulting libinput `v120` events;
- test at least a browser, terminal/editor and native toolkit application;
- record whether applications actually render the stream smoothly.

**Current state:** the temporary uinput pointer and deterministic experiment presets are implemented and unit-tested. Real desktop/application behaviour is the remaining decision gate.

**Decision gate:** if fine-grained uinput wheel output is coalesced or quantized badly by the desktop/application stack, stop and reassess the architecture before proceeding.

## Checkpoint 3 — Transparent pointer pass-through

**Goal:** reproduce an input device faithfully before changing any wheel semantics.

- clone relevant physical device capabilities into the virtual device;
- forward relative motion, buttons and supported non-wheel events unchanged;
- preserve event packet boundaries (`SYN_REPORT` semantics);
- prevent the virtual device from being rediscovered as a physical source;
- measure added pointer latency and event loss;
- build deterministic fixture-driven pass-through tests.

**Exit evidence:** with transformation disabled, recorded input produces equivalent virtual output.

## Checkpoint 4 — Safe exclusive capture

**Goal:** take ownership of a physical pointer without duplicate events or fragile recovery.

- use `EVIOCGRAB` through libevdev for a selected device;
- forward every required event through the virtual pointer;
- guarantee grab release on normal shutdown and handled signals;
- test daemon termination, exceptions and device disconnects;
- build a watchdog/fail-open strategy if needed;
- document safe development/recovery procedure.

**Safety gate:** do not enable automatic startup until repeated crash/disconnect tests demonstrate reliable pointer recovery.

## Checkpoint 5 — Smoothing engine v1

**Goal:** implement the transformation as a pure, deterministic component independent of Linux I/O.

- model wheel input as impulses in normalized `v120` units;
- maintain velocity/momentum state;
- emit fixed-step or time-based fractional output;
- implement decay and rapid-input accumulation;
- preserve total intended scroll distance within defined rounding bounds;
- handle reversal without long unwanted tails;
- unit-test timing, conservation, cancellation and determinism using a fake clock.

**Exit evidence:** the smoothing model can be exhaustively tested from event fixtures without `/dev/input` or `/dev/uinput`.

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
