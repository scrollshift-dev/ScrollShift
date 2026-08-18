# SmoothWheel development handover

## Canonical state

SmoothWheel has completed input reconnaissance (CP1), the initial virtual-output feasibility gate (CP2), and transparent physical-pointer pass-through on real hardware (CP3). Normal timed expiry and Ctrl-C recovery of exclusive capture are also proven. CP4 remains open only for abnormal-death/device-loss recovery evidence. CP5 is now active: a deterministic velocity estimator and packet-level acceleration transformer are implemented, with the first real-hardware acceleration tuning gate next.

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

## Completed virtual-output feasibility (Checkpoint 2)

A non-grabbing uinput feasibility spike is implemented. `smoothwheel experiment` creates a temporary virtual pointer with vertical/horizontal low-resolution and high-resolution wheel capabilities, emits one controlled gesture, then removes the device. The physical mouse is never grabbed. The gesture planner is deterministic and tested without requiring `/dev/uinput`.

Available presets intentionally compare a conventional detent against 8/16/24 fractional high-resolution reports and a diagnostic high-resolution-only variant. Each fractional paired preset conserves exactly 120 v120 units and emits one matching legacy detent at the accumulated boundary. This follows the kernel wheel model while allowing us to test what the real desktop actually consumes.

The runtime uinput path was exercised on the real desktop. Fractional virtual wheel gestures were accepted, but the subjective difference among simple fixed-subdivision presets was not compelling enough to define the product around visual smoothing alone.

## Most important unresolved question

Can SmoothWheel infer the user's scrolling intent from wheel cadence strongly enough that slow movement remains precise while rapid wheel spins become unmistakably faster, without adding floatiness or surprising reversals?

That is the current CP5 hardware gate. Smoothing/interpolation is secondary to intent-preserving acceleration.

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
- introduce the service explicitly for lifecycle testing first; leave boot-time autostart enabled only after fail-open forced-death/reconnect behaviour is demonstrated;
- invalid configuration must fail closed without a systemd restart storm;
- service stop/restart must remain prompt even while the daemon is waiting for a missing mouse.

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

The binary now implements read-only discovery/trace tooling, temporary relay/acceleration diagnostics, persistent configuration generation, a long-running daemon, and systemd service controls. The current automated suite includes CLI, input-fixture, virtual-output planner, velocity, packet-transform and configuration/selector tests, built with warnings treated as errors.

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

CP4 and the initial CP7 service lifecycle are now closed with real-hardware evidence: normal restart, forced `SIGKILL`, receiver unplug/replug, and suspend/resume all recovered correctly on the primary Linux test machine. CP6 is also closed at the engine/fixture level with low-resolution, high-resolution/free-spin, burst, duplicate-event, mixed-axis and reversal regression coverage.

The next frontier is CP8/CP9/CP10: build a representative desktop/application compatibility matrix, measure long-running resource/latency behavior, and continue failure/sanitizer/property hardening. Keep the restored original `balanced` profile frozen unless sustained dogfooding reveals a concrete defect; do not resume speculative curve tuning simply because more parameters are available.

## Product-direction update after CP2

The primary UX target is now **intent-preserving velocity-sensitive scrolling**, not smoothing for its own sake. Slow physical wheel movement must remain slow, immediate and precise; rapid wheel movement should accelerate strongly for traversal; reversal must respond immediately rather than fighting stale momentum. Fine-grained output remains a mechanism available to the engine, not the product goal.

A pure `VelocityEstimator` was introduced ahead of hardware capture work so this central policy can be developed deterministically. It maps detent cadence to a bounded profile-dependent multiplier, supports sub-1x slow precision in the tuned profiles, accumulates acceleration under rapid same-direction input, and resets on reversal/non-monotonic timestamps. Do not couple this estimator to evdev/uinput I/O.

## CP3/CP4 hardware gate

A time-bounded `smoothwheel relay DEVICE --seconds N` experiment clones EV_REL/EV_KEY capabilities into a temporary uinput pointer, creates that device before taking `EVIOCGRAB`, and mirrors complete raw `input_event` packets unchanged. SIGINT/SIGTERM request a clean stop; RAII releases the grab and destroys the virtual device. That experiment remains useful for diagnostics, while a separate long-running daemon/service path now exists for dogfooding.

CP3 has now passed its real-hardware gate: movement, ordinary buttons/scrolling, no obvious duplicate input, timed expiry, and Ctrl-C recovery all behaved normally. CP4 still requires deliberate forced-process-death and device-loss recovery evidence. If capability cloning later misses an event family, fix the generic model rather than hard-coding one mouse.

## CP5 packet-level acceleration prototype

`smoothwheel accelerate DEVICE --profile NAME --seconds N` now runs the same safe, time-bounded exclusive relay but buffers one evdev packet through `SYN_REPORT` before transforming wheel events. This matters because the kernel can emit `REL_WHEEL` and `REL_WHEEL_HI_RES` together; the transformer derives one cadence multiplier per axis and applies it coherently to both representations while preserving pointer motion/buttons/other events.

Profiles are intentionally exaggerated enough to distinguish the product direction:

- `precision`: maximum 2x;
- `balanced`: original preferred response, 1x to 4x with a linear cadence curve;
- `fast`: approximately 0.45x to 12x;
- `aggressive`: approximately 0.40x to 16x.

These are experimental tuning presets, not frozen user configuration. Real-hardware experiments tried substantially wider balanced ranges, but the original balanced response was preferred overall. Balanced is restored to a 1x floor, 4x ceiling, 420 ms slow interval, 45 ms fast interval, 0.38 smoothing and a linear curve. The packet transformer also carries a fractional legacy-wheel remainder, allowing sub-detent high-resolution output without spuriously emitting a full `REL_WHEEL` detent every packet. Same-direction rapid input increases the multiplier; reversal resets immediately and clears opposing fractional legacy carry. Momentum and post-input decay are not implemented yet.

## Persistent daemon/service state

A first production-shaped runtime now exists:

- `smoothwheel configure DEVICE [--profile NAME]` inspects a real device and writes `/etc/smoothwheel/config.conf`;
- configuration persists vendor/product + normalized kernel name, never `/dev/input/eventN`;
- matching requires a relative wheel pointer and explicitly excludes SmoothWheel virtual devices;
- zero matches cause the daemon to wait, while multiple matches cause it to refuse capture rather than guess;
- `smoothwheel daemon` attaches with the configured acceleration profile and rediscovers after source-session failure;
- `smoothwheel service enable|status|restart|start|stop|disable` provides simple systemd control;
- the systemd unit uses `Restart=on-failure` and is installed with the binary;
- the current permission model is a root system service, intentionally avoiding broad `/dev/input` permission changes during early development.

The user's receiver exposes a pointer node and a consumer-control node sharing vendor/product identity, so retaining the normalized kernel name is a real requirement, not decorative metadata. Surrounding whitespace from kernel-reported names is normalized before serialization/matching.

This milestone has deterministic parser/selector tests and a staged install-tree check, but no claim should yet be made that hotplug, boot startup, forced process death or suspend/resume are proven. Those are the next hardware gate.

## Checkpoint packaging

When handing the repository to another environment, do not include the local CMake `build/` tree. CMake caches absolute source/build paths, so copying a configured build directory can make a clean checkout fail before compilation. Use `make checkpoint` to create `../SmoothWheel-checkpoint.zip`; it preserves Git metadata while excluding build/cache artifacts.

## 2026-08-18 lifecycle hardening update

The daemon/service layer now has deterministic regression coverage for device-match states (missing, unique, ambiguous) and prompt SIGTERM shutdown while waiting for a device even when `reconnect_ms` is configured to 30000 ms. Reconnect sleeps are interruptible in short quanta rather than one long uninterruptible sleep. The systemd unit uses `RestartPreventExitStatus=2`, so missing/invalid configuration exits do not loop forever under `Restart=on-failure`.

`smoothwheel doctor` is a read-only/non-grabbing diagnostic that validates config and reports whether the configured stable identity is currently missing, uniquely ready, or ambiguous. `smoothwheel service logs` provides a compact journal view for dogfooding diagnostics.

Those lifecycle changes were subsequently exercised on real hardware; see the hardware lifecycle closure below.


## 2026-08-18 hardware lifecycle closure

The primary test machine successfully exercised the installed system service through normal start/restart, forced `SIGKILL`, physical receiver unplug/replug, and suspend/resume. All paths recovered without manual mouse repair, and stable identity rediscovery survived device disappearance. Treat this as the evidence closing CP4 and the initial CP7 lifecycle/hotplug gate.

## 2026-08-18 complete wheel semantics update

The packet transformer no longer assumes one wheel event of each code per `SYN_REPORT`. It aggregates same-axis events once per report, supports low-resolution-only and high-resolution-only streams, handles horizontal and vertical axes independently, and uses normalized v120 magnitude when estimating cadence. For example, 30 v120 every 10 ms is treated as the same physical rate as 120 v120 every 40 ms rather than as an artificially faster four-times-higher event frequency. This is important for genuine high-resolution/free-spin hardware.
