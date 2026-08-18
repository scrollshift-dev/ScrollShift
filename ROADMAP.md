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

## Checkpoint 4 — Safe exclusive capture ✅

**Goal:** take ownership of a physical pointer without duplicate events or fragile recovery.

- use `EVIOCGRAB` for a selected device;
- forward every required event through the virtual pointer;
- guarantee grab release on normal shutdown and handled signals;
- test daemon termination and device disconnects;
- establish fail-open behaviour for abnormal process death;
- document safe development/recovery procedure.

**Exit evidence:** Complete on the primary Linux test machine. Transparent relay behavior passed normal expiry and Ctrl-C tests; the persistent service then passed normal restart, forced `SIGKILL`, receiver unplug/replug, and suspend/resume. Forced death immediately returned the physical mouse because kernel descriptor teardown released the grab, and systemd subsequently restarted SmoothWheel. Device loss/reappearance also recovered without relying on a persistent `/dev/input/eventN`.

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

**Current state:** the pure cadence estimator and a packet-level wheel transformer are implemented and deterministic. The transformer keeps non-wheel events untouched, treats each `SYN_REPORT` packet as the transformation unit, scales paired legacy/high-resolution wheel representations coherently, and exposes deliberately distinct `precision`, `balanced`, `fast`, and `aggressive` profiles. Two real-hardware tuning gates established that the overall balanced shape is useful but needs more dynamic range in both directions. After wider-range experiments, real-hardware feedback preferred the original balanced response. Balanced is therefore restored to its original 1x baseline, 4x ceiling, 420 ms slow interval, 45 ms fast interval, 0.38 smoothing and linear curve. The fractional legacy accumulator remains available for profiles that use sub-1x output. Momentum/decay remains intentionally deferred.

**Exit evidence:** the complete motion model can be exhaustively tested from event fixtures without `/dev/input` or `/dev/uinput`.

## Checkpoint 6 — Complete wheel semantics ✅

**Goal:** make the engine correct across the wheel behaviours Linux exposes.

- low-resolution detent wheels;
- high-resolution/free-spin wheels;
- horizontal wheels;
- simultaneous/mixed axes;
- rapid direction reversal;
- bursts spanning multiple detents;
- multiple same-axis events inside one `SYN_REPORT`;
- preserve physical high-resolution input rather than degrading it.

**Exit evidence:** Complete at the engine/fixture level. The packet transformer now aggregates arbitrary same-axis wheel events once per `SYN_REPORT`, handles low-resolution-only and high-resolution-only streams, maintains independent horizontal/vertical velocity state, and normalizes cadence by actual v120 magnitude. This means fractional high-resolution samples are interpreted by equivalent physical rate rather than event frequency alone. Real primary-hardware evidence covers the paired `REL_WHEEL`/`REL_WHEEL_HI_RES` path; synthetic regression cases cover the remaining supported semantics until more physical devices are available.

## Checkpoint 7 — Daemon lifecycle, permissions and configuration ✅

**Goal:** turn the prototype into an everyday background utility without making it a desktop application yet.

- stable `smoothwheel` CLI and foreground diagnostic mode;
- device selection by stable identity rather than event node number;
- minimal configuration file with validated values;
- clean hotplug/reconnect handling;
- decide and document udev/group/systemd permission model;
- systemd service integration;
- clear diagnostics for missing permissions or unsupported devices.

**Current state:** the first long-running daemon and systemd service are implemented. `smoothwheel configure DEVICE` persists vendor/product identity plus normalized kernel device name rather than `/dev/input/eventN`; the daemon rediscovers the current event node, waits through absence, retries after device-session failure, refuses ambiguous matches, and excludes SmoothWheel virtual devices from capture. `smoothwheel service enable|status|restart|start|stop|disable` wraps systemd operations, and the unit uses `Restart=on-failure`. The current permission model is deliberately a root system service; a less-privileged udev/group model can be evaluated later rather than weakening input permissions prematurely.

**Current hardening:** device matching now has explicit missing/unique/ambiguous states with regression coverage. Reconnect waits are interruptible so SIGTERM remains prompt even at the maximum 30-second reconnect interval. Invalid/missing configuration exits with status 2 and the systemd unit uses `RestartPreventExitStatus=2` to avoid configuration-error restart storms. `smoothwheel doctor` validates configuration/device matching without grabbing input.

**Hardware evidence:** the root system service has now passed start/restart, forced `SIGKILL` recovery, receiver unplug/replug with rediscovery, and suspend/resume on the primary Linux test machine. The tuned `balanced` profile behaves the same in the daemon as in the time-bounded experiment.

**Exit evidence:** Complete for the initial service model. A user can install, configure, start, stop, restart and diagnose SmoothWheel predictably; the daemon finds the configured physical device by stable identity and background recovery has real-hardware evidence. A less-privileged permission model remains a possible post-1.0 hardening improvement, not a blocker for proving the runtime architecture.

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
