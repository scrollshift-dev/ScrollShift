# ScrollShift

**Intent-aware mouse-wheel scrolling for Linux.**

ScrollShift is an early-stage C++ utility that sits at the Linux input layer, keeps ordinary pointer behaviour transparent, and transforms wheel motion according to how deliberately or rapidly the wheel is moved. The current `balanced` profile is tuned for slow precision at the low end and much faster traversal under a hard spin.

The working architecture is:

```text
physical mouse
    ↓
evdev + exclusive grab
    ↓
ScrollShift
    ├── pointer movement/buttons → unchanged
    └── wheel packets → velocity estimator → acceleration transform
    ↓
uinput virtual pointer
    ↓
libinput → Wayland / XWayland / X11 → applications
```

ScrollShift is still developmental. Real-hardware pass-through, acceleration, forced-crash recovery, receiver reconnect, and suspend/resume have been validated on the primary Linux test machine. Broader device diversity, application compatibility, and long-running resource/latency evidence remain active hardening work.

## Build and test

```bash
make
make test
```

Equivalent CMake commands:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSCROLLSHIFT_WARNINGS_AS_ERRORS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Requires Linux, a C++20 compiler and CMake 3.20+.

## Run an explicit development session

List input devices:

```bash
sudo ./build/scrollshift devices
```

Run the current balanced acceleration path for 30 seconds:

```bash
sudo ./build/scrollshift accelerate /dev/input/eventX --profile balanced --seconds 30
```

The time-bounded development commands remain useful for diagnostics, but ordinary use should move to the service flow below.

## Install as a background service

The recommended release install is:

```bash
curl -fsSL https://scrollshift.dev/install.sh | sh
```

The installer verifies the selected GitHub release against `SHA256SUMS`, then invokes ScrollShift's own privileged service installer. The CLI copies the running executable atomically to `/usr/local/bin/scrollshift`, writes a managed `/etc/systemd/system/scrollshift.service`, reloads systemd, and enables the service. On a fresh install it creates `/etc/scrollshift/config.conf` in automatic-discovery mode and starts immediately. ScrollShift classifies input devices using udev/libinput metadata when available, ignores touchpads, touchscreens, joysticks, tablets and keyboard auxiliary nodes, and automatically attaches conventional wheel mice as they appear. When classification metadata is missing it uses a deliberately conservative evdev capability check so a non-mouse relative device is not mistaken for a mouse. No `/dev/input/eventN` configuration is required.

The normal first-run path is therefore just:

```bash
curl -fsSL https://scrollshift.dev/install.sh | sh
scrollshift service status
```

For unusual hardware, `sudo scrollshift devices` shows classification/capabilities and `sudo scrollshift configure /dev/input/eventX` switches to a persistent specific-device override. If a VM routes wheel events through an absolute tablet-style device, `sudo scrollshift configure /dev/input/eventX --force` explicitly permits that selected wheel-capable device while leaving automatic discovery conservative.

Service commands follow the same lifecycle API used by the Gantry Go services:

```bash
sudo scrollshift service install
sudo scrollshift service uninstall
sudo scrollshift service start
sudo scrollshift service stop
sudo scrollshift service restart
scrollshift service status
scrollshift service logs
scrollshift service logs --follow
sudo scrollshift service enable
sudo scrollshift service disable
```

Mutating commands require root. ScrollShift refuses to start/restart when the configuration/device preflight fails, refuses to replace or remove a systemd unit it does not recognize as ScrollShift-managed, and keeps `/etc/scrollshift` on ordinary uninstall. `uninstall.sh --purge` is the explicit destructive configuration-removal path.

To download a verified binary without installing or touching systemd:

```bash
curl -fsSL https://scrollshift.dev/download.sh | sh
```

The current service remains a root system service because evdev capture and uinput creation require privileged input-device access in the current design.

## Current configuration

Generated configuration is intentionally small:

```ini
mode = auto
profile = balanced
reconnect_ms = 1000
```

`mode = auto` is the default: ScrollShift discovers conventional wheel mice automatically using udev input classification when available, and only falls back to conservative evdev capability checks when classification metadata is missing. Touchpads, touchscreens, joysticks, tablets and keyboard auxiliary nodes are never selected. Do not hand-edit event-node numbers into configuration. For unusual hardware, `sudo scrollshift configure /dev/input/eventX` writes a `mode = device` override that persists stable vendor/product/name identity, never an event-node path. `--force` additionally permits a manually selected wheel-capable non-relative device (for example, some QEMU/SPICE tablet devices); it never broadens automatic discovery.

## Diagnostics and reconnaissance

These commands are non-invasive and useful when diagnosing hardware or compatibility:

```bash
scrollshift environment
scrollshift inspect /dev/input/eventX
scrollshift monitor /dev/input/eventX --record mouse.trace
```

`monitor` never grabs or injects input. It can record complete raw evdev traces for fixture-driven regression testing.

## Project principles

- **Fail open whenever possible.** A daemon crash must not strand the user without pointer control.
- **Preserve non-wheel behaviour.** Motion, buttons and unrelated capabilities should remain transparent.
- **Preserve intent, not animation.** Slow wheel motion should stay slow and precise; hard spins should become dramatically faster.
- **Prefer compositor-independent primitives.** Use the Linux input stack before desktop-specific hooks.
- **Measure instead of guessing.** Capture real event streams, latency and failure behaviour.
- **Keep configuration small.** Expose user concepts, not every internal tuning constant.
- **Never guess which device to grab.** Ambiguity is a failure state, not a precedence rule.

See [`ROADMAP.md`](ROADMAP.md) for checkpoint scope, [`HANDOVER.md`](HANDOVER.md) for the current development state, and [`docs/COMPATIBILITY.md`](docs/COMPATIBILITY.md) for the application test matrix.

## Development-only virtual-wheel experiment

Checkpoint 2's uinput feasibility probes remain available:

```bash
./build/scrollshift experiment --list
./build/scrollshift experiment fine16 --dry-run
sudo ./build/scrollshift experiment fine16
```

These are diagnostic tools, not the production motion model.

## Checkpoint archives

Use:

```bash
make checkpoint
```

when handing the repository to another environment. The archive excludes configured CMake build trees because CMake caches absolute source/build paths.
