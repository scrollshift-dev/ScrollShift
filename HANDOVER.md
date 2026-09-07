# ScrollShift development handover

## Canonical state

ScrollShift has completed input reconnaissance (CP1), the initial virtual-output feasibility gate (CP2), and transparent physical-pointer pass-through on real hardware (CP3). Normal timed expiry and Ctrl-C recovery of exclusive capture are also proven. CP4 remains open only for abnormal-death/device-loss recovery evidence. CP5 is now active: a deterministic velocity estimator and packet-level acceleration transformer are implemented, with the first real-hardware acceleration tuning gate next.

The paired website repository is `ScrollShift/ScrollShift.github.io`. It is built with Nift and intentionally presents ScrollShift as early development until the feasibility and safety checkpoints are complete.

## Product goal

ScrollShift should bring SmoothScroll-style fluid wheel behaviour to Linux at a system-wide level rather than through browser extensions or application plugins.

The current preferred architecture is:

```text
physical mouse
    ↓
Linux evdev device
    ↓
ScrollShift input/capture layer
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

A non-grabbing uinput feasibility spike is implemented. `scrollshift experiment` creates a temporary virtual pointer with vertical/horizontal low-resolution and high-resolution wheel capabilities, emits one controlled gesture, then removes the device. The physical mouse is never grabbed. The gesture planner is deterministic and tested without requiring `/dev/uinput`.

Available presets intentionally compare a conventional detent against 8/16/24 fractional high-resolution reports and a diagnostic high-resolution-only variant. Each fractional paired preset conserves exactly 120 v120 units and emits one matching legacy detent at the accumulated boundary. This follows the kernel wheel model while allowing us to test what the real desktop actually consumes.

The runtime uinput path was exercised on the real desktop. Fractional virtual wheel gestures were accepted, but the subjective difference among simple fixed-subdivision presets was not compelling enough to define the product around visual smoothing alone.

## Most important unresolved question

Can ScrollShift infer the user's scrolling intent from wheel cadence strongly enough that slow movement remains precise while rapid wheel spins become unmistakably faster, without adding floatiness or surprising reversals?

That is the current CP5 hardware gate. Smoothing/interpolation is secondary to intent-preserving acceleration.

## Safety invariant

Exclusive device capture is dangerous if done casually. `EVIOCGRAB` prevents other clients from receiving events from the grabbed device. Once ScrollShift starts grabbing a physical mouse, it is responsible for faithfully reproducing the events the desktop still needs.

The project's safety target is:

> ScrollShift must never knowingly trade smooth scrolling for fragile pointer ownership.

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

A virtual ScrollShift device will itself appear in the Linux input subsystem. Device discovery must reliably identify and exclude ScrollShift-created devices, otherwise the daemon can consume its own emitted events and create a feedback loop.

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

ScrollShift is latency-sensitive but not throughput-heavy. Optimize for:

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

## Release state

- **v0.1.0** — historical initial release, immutable, at commit `21ce000343bd435b189e4cc0ec4518e5441ebd53` (published 2026-08-23). Pre-dates the rename and the pre-release hardening series.
- **v0.1.1** — first hardened/publicity-ready release, at commit `ac973dd925cf62a76c85c363665a4be8e7533ff6` (tag object `bf701f74d977ca74776cd07041b5c2b5dadb2c13`, published 2026-09-07). Automatic conventional-mouse discovery, conservative non-mouse exclusion, hotplug/backoff, Gantry-style service lifecycle with safe historical-unit migration, verified installers, and strengthened release verification.
- **Current development version: 0.1.2** (`scrollshift --version`; also the CMake project version).

Versioning should remain explicitly developmental until the safety and compatibility model is fully established; do not call an early experimental input grab `1.0`.

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

A time-bounded `scrollshift relay DEVICE --seconds N` experiment clones EV_REL/EV_KEY capabilities into a temporary uinput pointer, creates that device before taking `EVIOCGRAB`, and mirrors complete raw `input_event` packets unchanged. SIGINT/SIGTERM request a clean stop; RAII releases the grab and destroys the virtual device. That experiment remains useful for diagnostics, while a separate long-running daemon/service path now exists for dogfooding.

CP3 has now passed its real-hardware gate: movement, ordinary buttons/scrolling, no obvious duplicate input, timed expiry, and Ctrl-C recovery all behaved normally. CP4 still requires deliberate forced-process-death and device-loss recovery evidence. If capability cloning later misses an event family, fix the generic model rather than hard-coding one mouse.

## CP5 packet-level acceleration prototype

`scrollshift accelerate DEVICE --profile NAME --seconds N` now runs the same safe, time-bounded exclusive relay but buffers one evdev packet through `SYN_REPORT` before transforming wheel events. This matters because the kernel can emit `REL_WHEEL` and `REL_WHEEL_HI_RES` together; the transformer derives one cadence multiplier per axis and applies it coherently to both representations while preserving pointer motion/buttons/other events.

Profiles are intentionally exaggerated enough to distinguish the product direction:

- `precision`: maximum 2x;
- `balanced`: original preferred response, 1x to 4x with a linear cadence curve;
- `fast`: approximately 0.45x to 12x;
- `aggressive`: approximately 0.40x to 16x.

These are experimental tuning presets, not frozen user configuration. Real-hardware experiments tried substantially wider balanced ranges, but the original balanced response was preferred overall. Balanced is restored to a 1x floor, 4x ceiling, 420 ms slow interval, 45 ms fast interval, 0.38 smoothing and a linear curve. The packet transformer also carries a fractional legacy-wheel remainder, allowing sub-detent high-resolution output without spuriously emitting a full `REL_WHEEL` detent every packet. Same-direction rapid input increases the multiplier; reversal resets immediately and clears opposing fractional legacy carry. Momentum and post-input decay are not implemented yet.

## Persistent daemon/service state

A first production-shaped runtime now exists:

- `scrollshift configure DEVICE [--profile NAME]` inspects a real device and writes `/etc/scrollshift/config.conf`;
- configuration persists vendor/product + normalized kernel name, never `/dev/input/eventN`;
- matching requires a relative wheel pointer and explicitly excludes ScrollShift virtual devices;
- zero matches cause the daemon to wait, while multiple matches cause it to refuse capture rather than guess;
- `scrollshift daemon` attaches with the configured acceleration profile and rediscovers after source-session failure;
- `scrollshift service install|uninstall|start|stop|restart|status|enable|disable|logs` provides a hardened, consistent systemd lifecycle API;
- the systemd unit uses `Restart=on-failure` and is installed with the binary;
- the current permission model is a root system service, intentionally avoiding broad `/dev/input` permission changes during early development.

The user's receiver exposes a pointer node and a consumer-control node sharing vendor/product identity, so retaining the normalized kernel name is a real requirement, not decorative metadata. Surrounding whitespace from kernel-reported names is normalized before serialization/matching.

This milestone has deterministic parser/selector tests and a staged install-tree check, but no claim should yet be made that hotplug, boot startup, forced process death or suspend/resume are proven. Those are the next hardware gate.

## Checkpoint packaging

When handing the repository to another environment, do not include the local CMake `build/` tree. CMake caches absolute source/build paths, so copying a configured build directory can make a clean checkout fail before compilation. Use `make checkpoint` to create `../ScrollShift-checkpoint.zip`; it preserves Git metadata while excluding build/cache artifacts.

## 2026-08-18 lifecycle hardening update

The daemon/service layer now has deterministic regression coverage for device-match states (missing, unique, ambiguous) and prompt SIGTERM shutdown while waiting for a device even when `reconnect_ms` is configured to 30000 ms. Reconnect sleeps are interruptible in short quanta rather than one long uninterruptible sleep. The systemd unit uses `RestartPreventExitStatus=2`, so missing/invalid configuration exits do not loop forever under `Restart=on-failure`.

`scrollshift doctor` is a read-only/non-grabbing diagnostic that validates config and reports whether the configured stable identity is currently missing, uniquely ready, or ambiguous. `scrollshift service logs` provides a compact journal view for dogfooding diagnostics.

Those lifecycle changes were subsequently exercised on real hardware; see the hardware lifecycle closure below.


## 2026-09-06 automatic mouse discovery update

The first-release default no longer requires users to identify or configure `/dev/input/eventN`. `mode = auto` is now the default daemon configuration. Device inspection reads udev database properties for `ID_INPUT_MOUSE`, `ID_INPUT_TOUCHPAD` and `ID_INPUT_TOUCHSCREEN`; explicit touchpads/touchscreens are always excluded. When udev classification is unavailable, ScrollShift falls back conservatively to evdev relative-pointer + wheel capabilities while still excluding its own virtual devices.

In auto mode the daemon may start with zero mice attached, waits without failing, attaches qualifying wheel mice as they appear, supports multiple simultaneous mice with independent relay workers, and re-enumerates after unplug/replug or event-node renumbering. `/dev/input/eventN` is never persisted. Existing explicit device configurations remain backward-compatible, and `scrollshift configure DEVICE` now serves as a manual override for unusual hardware rather than a mandatory setup step.

`service install` creates an automatic config only when `/etc/scrollshift/config.conf` does not already exist, so upgrades preserve user choices. A fresh install can therefore install, enable and start in one operation. `doctor` in auto mode reports currently detected mouse candidates but succeeds when none are attached, because waiting for future hotplug is a valid ready state.

This materially improves suitability for native desktop integration such as Omarchy: the platform can enable the service without per-machine event-node setup, while touchpad behaviour remains outside ScrollShift. Before first release, this still needs real-hardware testing on at least a laptop with a touchpad + USB/Bluetooth mouse and ideally two simultaneous mice, including hotplug and suspend/resume.

## 2026-08-18 hardware lifecycle closure

The primary test machine successfully exercised the installed system service through normal start/restart, forced `SIGKILL`, physical receiver unplug/replug, and suspend/resume. All paths recovered without manual mouse repair, and stable identity rediscovery survived device disappearance. Treat this as the evidence closing CP4 and the initial CP7 lifecycle/hotplug gate.

## 2026-08-18 complete wheel semantics update

The packet transformer no longer assumes one wheel event of each code per `SYN_REPORT`. It aggregates same-axis events once per report, supports low-resolution-only and high-resolution-only streams, handles horizontal and vertical axes independently, and uses normalized v120 magnitude when estimating cadence. For example, 30 v120 every 10 ms is treated as the same physical rate as 120 v120 every 40 ms rather than as an artificially faster four-times-higher event frequency. This is important for genuine high-resolution/free-spin hardware.


## 2026-08-18 property/sanitizer hardening update

A fixed-seed randomized transformer test now executes 100,000 mixed packet shapes and verifies deterministic equality between independent transformer instances while asserting that all non-wheel event values, event codes/types, timestamps and packet sizes remain unchanged. The generated corpus includes low-resolution-only, high-resolution-only, paired legacy/high-resolution, horizontal, mixed-axis, burst, duplicate-event and zero-net cases.

The full 12-test suite passes under AddressSanitizer + UndefinedBehaviorSanitizer with leak detection enabled. In the ordinary warnings-as-errors debug build, the 100,000-packet property test completes in roughly 0.1 seconds in the current container, which is ample throughput headroom for human input. Do not turn that number into a public performance claim without a reproducible benchmark protocol; its purpose here is to detect pathological overhead.


## Service CLI hardening for first release

The first-release service surface is intentionally owned by the C++ CLI rather than shell installer logic. `service install` resolves `/proc/self/exe`, atomically installs the exact executable to `/usr/local/bin/scrollshift`, creates `/etc/scrollshift`, writes a default `mode = auto` config only when no config exists, writes the managed system unit via a temporary file + rename, performs `daemon-reload`, enables the unit, and starts it after the non-grabbing `doctor` preflight. Existing config is never overwritten.

Safety invariants learned from the Gantry Go service work and retained here:

- mutation requires root; read-only status/logs do not;
- service process spawning uses `fork`/`execvp` argument vectors, not shell-interpolated command strings;
- the unit and executable use canonical absolute paths;
- install/uninstall/start/stop/restart/enable/disable refuse an unmanaged or malformed unit rather than modifying somebody else's systemd configuration;
- binary and unit replacement are staged through same-directory temporary files then renamed;
- start/restart fail closed when `doctor` cannot validate configuration; auto mode treats zero currently attached mice as a valid waiting state, while manual device mode retains strict identity validation;
- uninstall removes service registration but preserves `/etc/scrollshift`; explicit website `uninstall.sh --purge` removes configuration;
- the unit keeps the previous fail-open lifecycle policy (`Restart=on-failure`, `RestartPreventExitStatus=2`, bounded stop timeout) and adds systemd sandboxing compatible with evdev/uinput access.

The website scripts are thin distribution entry points. `install.sh` and `download.sh` fail closed unless exactly one structurally valid checksum entry matches the selected release archive and the archive bytes verify. The shell installer does not hand-author systemd state; it delegates that to `scrollshift service install`.

## 2026-09-06 pre-release correction pass

An adversarial pre-release review (the "ScrollShift pre-release review" findings) drove a focused hardening pass ahead of v0.1.0. The review accepted the capture/transform core, installer verification, config parsing and current systemd hardening as sound, and the corrections below address the substantive findings without weakening the service ownership invariant.

**Automatic classifier (release blocker addressed).** The capability fallback could previously mistake an unclassified non-mouse relative device (joystick, drawing tablet in relative mode, another tool's virtual pointer, keyboard auxiliary node) for a mouse. Classification now:

- recognizes the full relevant `ID_INPUT_*` set (mouse, touchpad, touchscreen, joystick, tablet, tablet-pad, pointing-stick, keyboard, plus key/switch/accelerometer tags) and treats any recognized classification as authoritative — a device udev says is a joystick/tablet/etc. can no longer be reclassified by the fallback;
- trusts explicit mouse/pointing-stick classification when present;
- requires the capability fallback to show a full pointer signature: both `REL_X` and `REL_Y`, a vertical wheel (`REL_WHEEL`/`REL_WHEEL_HI_RES`), a `BTN_MOUSE`-family button, no `ABS_X`/`ABS_Y` (tablet/touchpad/joystick-style absolute motion), and not `BUS_VIRTUAL` (unrelated virtual devices). Harmless absolute capabilities such as `ABS_WHEEL` do not trip the exclusion;
- is deterministic-testable through an injectable udev-data root plus a comprehensive pure classifier matrix.

A false negative on unusual hardware is preferred over grabbing a non-mouse device; users retain `scrollshift configure DEVICE` as the explicit override.

**Managed service-unit upgrade handling (issue accepted, proposed weakening rejected).** Ownership remains exact-template based: a unit is managed only when it byte-for-byte matches the exact current template or one of the exact known historical ScrollShift-generated templates (the pre-release `packaging/scrollshift.service.in` output and the immediate-prior CLI unit with the managed marker but without the format-marker line). Marker presence alone is still insufficient — a marker-bearing but modified body is refused for install/uninstall/start/stop. Known historical units migrate transactionally to the current format, and a failed `daemon-reload` rolls the previous unit back. No `--force` escape was added.

**Release publication integrity.** Release builds use `SCROLLSHIFT_WARNINGS_AS_ERRORS=ON`; the workflow checks out the repository in `publish` and verifies the candidate asset set (via `scripts/verify_release.sh`) before creating a release; website installer parity is checked before publication. An existing release is accepted on rerun only under a two-layer invariant: (1) the complete published GitHub asset-name set is exactly the expected public set — the two architecture archives plus `SHA256SUMS` — with any name outside the expected three rejected as an unexpected asset (`verify_release.sh --published`; GitHub source-code downloads are not release assets and never appear in the asset list); and (2) the three expected assets' contents satisfy `scripts/verify_release.sh` and the published manifest is byte-identical to the candidate manifest. A partial release (missing `SHA256SUMS`, missing archive, malformed or differing manifest, checksum mismatch, or unexpected extra asset) fails loudly and is never auto-repaired. Partial cases have deterministic coverage in `tests/release_verify_tests.sh` for both directory and `--published` modes. `scripts/check_workflow_checkout.py` (standard-library only, with ordering enforcement and deterministic tests in `tests/check_workflow_checkout_tests.py`) statically rejects any workflow job that invokes repository scripts before or without an `actions/checkout` step. The post-publication live installer smoke test remains.

**EVIOCGRAB contention backoff.** The auto-mode daemon now applies bounded exponential backoff (base `reconnect_ms`, capped at 30 s) per still-present device after repeated grab/session failures, resets after a successful session and on device reappearance, keeps devices independent (a permanently busy device does not penalize another mouse), suppresses repeated identical failure logging, and remains prompt on SIGTERM.

**Tests added.** Pure classifier matrix and injectable udev-data parsing (`classifier_unit`), service unit ownership/migration matrix plus non-root mutation refusal (`service_unit`), auto-mode zero-mouse lifecycle and backoff policy checks (`daemon_lifecycle_unit`), installer checksum failure cases (missing/duplicate/malformed/wrong/missing-manifest), and release verification partial cases (missing archive, missing `SHA256SUMS`, malformed manifest, manifest missing an archive, duplicate entry, checksum mismatch, unexpected asset) plus shellcheck in CI.

**Outstanding hardware coverage (post v0.1.1):** automatic discovery selecting the user's intended physical mouse and ordinary wheel scrolling were exercised by the user on real hardware before release, and most ordinary service/mouse lifecycle behaviour was exercised during development. Not yet covered on real hardware and explicitly non-blocking for v0.1.x: two simultaneous physical mice, touchscreen isolation, unusual composite receivers, and broader unusual Linux pointing-device combinations. These remain RELEASE.md hardware gates for later coverage; do not claim them passed until exercised.
