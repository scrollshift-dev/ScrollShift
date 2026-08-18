# SmoothWheel development handover

## Canonical state

This repository was initialized on 2026-08-18 as the canonical SmoothWheel program repository. Checkpoint 1 read-only input reconnaissance is complete. The CLI can enumerate readable evdev devices, inspect wheel capabilities, monitor events, and record complete event traces. Real hardware evidence from a 2.4G Wireless Mouse (3151:402d) is preserved in `tests/fixtures/nick-2.4g-wireless-mouse-vertical.trace`. **No exclusive input grab or smoothing is implemented yet. Checkpoint 2 is the current frontier: a non-grabbing uinput feasibility experiment.**

The paired website repository is `SmoothWheel/SmoothWheel.github.io`. It is built with Nift and intentionally presents SmoothWheel as early development until the feasibility and safety checkpoints are complete.

## Product goal

SmoothWheel should bring SmoothScroll-style fluid wheel behaviour to Linux at a system-wide level rather than through browser extensions or application plugins.

The current preferred architecture is:

```text
physical mouse
    ↓
Linux evdev device
    ↓
SmoothWheel input/capture layer
    ├── motion/buttons/other supported events → transparent pass-through
    └── wheel events → normalized smoothing engine
    ↓
virtual uinput pointer
    ↓
libinput / compositor
    ↓
Wayland, XWayland, X11 applications
```

This is a hypothesis to prove, not an architecture to defend at all costs.

## Why this architecture is plausible

Linux's evdev interface is the generic userspace input-event interface. The kernel uinput module lets a userspace process create a virtual input device and inject events through it. Kernel documentation recommends libevdev as the less error-prone wrapper for new uinput software.

Modern Linux wheel semantics are also suitable for the experiment: `REL_WHEEL_HI_RES` and `REL_HWHEEL_HI_RES` represent high-resolution wheel movement where an accumulated value of 120 corresponds to one detent. libinput exposes this as normalized `v120` scroll values and explicitly allows fractions of 120 for high-resolution scrolling.

Relevant primary references:

- Linux input event codes: https://docs.kernel.org/input/event-codes.html
- Linux uinput documentation: https://docs.kernel.org/input/uinput.html
- libevdev API: https://www.freedesktop.org/software/libevdev/doc/latest/
- libinput wheel API: https://wayland.freedesktop.org/libinput/doc/latest/wheel-api.html
- Wayland architecture: https://wayland.freedesktop.org/architecture.html

## Checkpoint 1 hardware evidence

On 2026-08-18 a real 2.4G Wireless Mouse (USB ID `3151:402d`) was inspected on Linux. Its pointer event node exposed vertical and horizontal low-resolution and high-resolution wheel capabilities. Observed vertical wheel packets consistently emitted `REL_WHEEL +/-1` and `REL_WHEEL_HI_RES +/-120` together, followed by `SYN_REPORT`; fast scrolling altered event timing rather than the per-detent magnitude. A representative subset is committed as a permanent fixture.

The same USB receiver also exposes a separate Consumer Control event node with horizontal-wheel capabilities but no relative pointer axes. This is evidence that capability presence alone is insufficient for future automatic device selection: physical-device grouping and pointer classification must be considered before exclusive capture.

## Current frontier: Checkpoint 2

A non-grabbing uinput feasibility spike is implemented. `smoothwheel experiment` creates a temporary virtual pointer with vertical/horizontal low-resolution and high-resolution wheel capabilities, emits one controlled gesture, then removes the device. The physical mouse is never grabbed. The gesture planner is deterministic and tested without requiring `/dev/uinput`.

Available presets intentionally compare a conventional detent against 8/16/24 fractional high-resolution reports and a diagnostic high-resolution-only variant. Each fractional paired preset conserves exactly 120 v120 units and emits one matching legacy detent at the accumulated boundary. This follows the kernel wheel model while allowing us to test what the real desktop actually consumes.

The runtime uinput path cannot be meaningfully validated in the development container because it does not expose `/dev/uinput`; real-desktop validation is therefore the current decision gate.

## Most important unresolved question

Do fine-grained high-resolution wheel events emitted through a virtual uinput pointer produce consistently smooth motion across the real desktop/application stack?

That is why Checkpoint 2 is an explicit decision gate. Do not spend weeks building configuration, packaging or UI before answering it on real Wayland/XWayland/X11 applications.

## Safety invariant

Exclusive device capture is dangerous if done casually. `EVIOCGRAB` prevents other clients from receiving events from the grabbed device. Once SmoothWheel starts grabbing a physical mouse, it is responsible for faithfully reproducing the events the desktop still needs.

The project's safety target is:

> SmoothWheel must never knowingly trade smooth scrolling for fragile pointer ownership.

In practice:

- do not grab devices during Checkpoint 1;
- prove virtual output before exclusive capture;
- prove complete pass-through before altering wheel events;
- keep a recoverable input/session path during early grab experiments;
- release grabs on every controlled shutdown path;
- deliberately kill/crash the process during Checkpoint 4 and verify recovery;
- do not enable automatic startup until fail-open behaviour is demonstrated.

## Architectural boundaries

### Input backend

Responsible for device discovery, opening evdev devices, capability inspection, event capture and hotplug. It should not know smoothing policy.

### Virtual output backend

Responsible for creating/configuring the uinput device and emitting correctly packetized Linux input events. It should not know how velocity curves work.

### Smoothing engine

A pure deterministic library driven by normalized wheel impulses and monotonic timestamps/fake-clock ticks. It should not depend on file descriptors, libevdev, uinput, Wayland or a desktop environment.

This separation is important because the smoothing engine should become heavily unit/property tested without privileged input access.

### Runtime/daemon

Owns lifecycle, scheduling, configuration, device reconnects and diagnostics. It coordinates the two I/O backends and the pure smoothing engine.

## Event semantics to preserve

Do not reduce the mouse to `REL_X`, `REL_Y` and three buttons. Real pointer devices may expose additional buttons, horizontal wheels and high-resolution wheel events. Device capability cloning/passthrough should be explicit and tested.

`EV_SYN` packet boundaries matter. A physical hardware action may consist of several input events terminated by `SYN_REPORT`; do not casually stream individual copied events with different grouping.

High-resolution wheel support must preserve information rather than quantizing it back to coarse detents. The kernel defines 120 high-resolution wheel units as one detent; libinput's preferred wheel API represents the same logical unit as `v120`.

## Reinjection-loop prevention

A virtual SmoothWheel device will itself appear in the Linux input subsystem. Device discovery must reliably identify and exclude SmoothWheel-created devices, otherwise the daemon can consume its own emitted events and create a feedback loop.

Design a deterministic identity strategy early (name/vendor/product/phys/uniq properties as appropriate) and regression-test the exclusion rule.

## Configuration philosophy

Do not freeze configuration before the scroll model is understood.

Likely user concepts are:

- selected device(s);
- overall distance/speed;
- impulse/acceleration strength;
- decay or duration;
- horizontal-wheel enablement.

Avoid exposing internal scheduler constants simply because they exist. Defaults should be good enough that most users do not need a tuning ritual.

## Per-application settings

Treat per-application profiles as later work, not part of the initial architecture. On Wayland the compositor owns input routing and generic clients do not have the same global window-control model as X11. A robust system-wide input path is more important than prematurely coupling the daemon to compositor-specific active-window APIs.

## Performance targets

SmoothWheel is latency-sensitive but not throughput-heavy. Optimize for:

- negligible extra pointer-motion latency;
- consistent scheduler timing;
- low idle CPU;
- no busy-wait loop;
- stable RSS during long sessions;
- predictable behaviour independent of display refresh rate.

Do not confuse a very high emission rate with smoothness. Measure whether applications actually consume the additional granularity usefully.

## Testing strategy

Build tests in layers:

1. recorded raw input fixtures;
2. capability/discovery tests where practical;
3. pure smoothing-engine unit/property tests with fake time;
4. virtual-output event-sequence tests;
5. pass-through equivalence tests;
6. process lifecycle/crash/recovery tests;
7. hardware-in-the-loop compatibility tests.

Every bug found using a real mouse should be reduced to a reproducible fixture/test where possible.

Useful future properties include:

- transformation disabled ⇒ output event stream is semantically equivalent to input;
- one detent's normalized input produces the configured total normalized output within a documented rounding tolerance;
- equal event/timestamp sequences produce equal output sequences;
- opposite-direction input cancels/reverses according to explicit rules rather than leaving stale momentum;
- virtual devices are never accepted as capture sources;
- daemon exit releases exclusive ownership.

## Current build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The binary now implements `devices`, `inspect DEVICE`, and read-only `monitor DEVICE [--all] [--record FILE]` in addition to `--help` and `--version`. The event-trace parser/serializer is unit tested and the build passes with warnings treated as errors.

## Development version

Current development version: **0.0.1-dev**.

Do not call an early experimental input grab `1.0`. Versioning should remain explicitly developmental until the safety and compatibility model is established.

## First ten checkpoints

`ROADMAP.md` is canonical for checkpoint scope. In short:

1. input reconnaissance;
2. virtual pointer feasibility;
3. transparent pass-through;
4. safe exclusive capture;
5. smoothing engine v1;
6. complete wheel semantics;
7. daemon/permissions/configuration;
8. desktop/application compatibility;
9. feel/latency/default tuning;
10. hardening and first release.

## Definition of done for each checkpoint

A checkpoint should not be declared complete merely because the happy-path demo works. Before moving on:

- tests/evidence for the checkpoint's explicit guarantee must exist;
- new failure modes discovered during development must be documented or regression-tested;
- README/website wording must not claim capabilities that have not landed;
- ROADMAP/HANDOVER should be reconciled if architecture or scope changed materially;
- disposable build artifacts should not be committed.

## Immediate next action

Finish **Checkpoint 1 hardware evidence** before starting Checkpoint 2.

Run `smoothwheel devices` on a real Linux desktop, select the wheel-capable physical mouse, then run `smoothwheel monitor /dev/input/eventX --record mouse.trace` and exercise several slow detents, rapid detents, direction reversals, and horizontal/free-spin behaviour if the device supports it. Preserve the resulting trace as a regression fixture after reviewing it for device-specific metadata/privacy. Do not grab devices and do not inject events yet.

## Product-direction update after CP2

The primary UX target is now **intent-preserving velocity-sensitive scrolling**, not smoothing for its own sake. Slow physical wheel movement must remain slow, immediate and precise; rapid wheel movement should accelerate strongly for traversal; reversal must respond immediately rather than fighting stale momentum. Fine-grained output remains a mechanism available to the engine, not the product goal.

A pure `VelocityEstimator` was introduced ahead of hardware capture work so this central policy can be developed deterministically. It currently maps detent cadence to a bounded multiplier, leaves slow/isolated input at 1x, accumulates acceleration under rapid same-direction input, and resets on reversal/non-monotonic timestamps. Do not couple this estimator to evdev/uinput I/O.

## CP3/CP4 hardware gate

A time-bounded `smoothwheel relay DEVICE --seconds N` experiment now clones EV_REL/EV_KEY capabilities into a temporary uinput pointer, creates that device before taking `EVIOCGRAB`, and then mirrors complete raw `input_event` packets unchanged. SIGINT/SIGTERM request a clean stop; RAII releases the grab and destroys the virtual device. This is intentionally not a daemon and has no autostart path.

This code is **not considered CP3/CP4 complete until tested on a real physical mouse**. The next hardware test must verify movement, left/right/middle/extra buttons, vertical/horizontal wheel behaviour, no duplicate input, Ctrl-C recovery, timed-expiry recovery, and preferably forced-process-death recovery. If capability cloning misses an event family on the real device, fix the generic cloning model rather than hard-coding the user's mouse.
