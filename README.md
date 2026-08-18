# SmoothWheel

**Intent-aware mouse-wheel scrolling for Linux.**

SmoothWheel is an early-stage C++ utility that sits at the Linux input layer, keeps ordinary pointer behaviour transparent, and transforms wheel motion according to how deliberately or rapidly the wheel is moved. The current `balanced` profile is tuned for slow precision at the low end and much faster traversal under a hard spin.

The working architecture is:

```text
physical mouse
    ↓
evdev + exclusive grab
    ↓
SmoothWheel
    ├── pointer movement/buttons → unchanged
    └── wheel packets → velocity estimator → acceleration transform
    ↓
uinput virtual pointer
    ↓
libinput → Wayland / XWayland / X11 → applications
```

SmoothWheel is still developmental. Real-hardware pass-through and acceleration have been validated on the initial Linux test machine, but crash/reconnect, suspend/resume, broader device diversity and application compatibility remain active hardening work.

## Build and test

```bash
make
make test
```

Equivalent CMake commands:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSMOOTHWHEEL_WARNINGS_AS_ERRORS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Requires Linux, a C++20 compiler and CMake 3.20+.

## Run an explicit development session

List input devices:

```bash
sudo ./build/smoothwheel devices
```

Run the current balanced acceleration path for 30 seconds:

```bash
sudo ./build/smoothwheel accelerate /dev/input/eventX --profile balanced --seconds 30
```

The time-bounded development commands remain useful for diagnostics, but ordinary use should move to the service flow below.

## Install as a background service

SmoothWheel installs a systemd system service because evdev capture and uinput creation require privileged input-device access in the current design.

First build and install:

```bash
make test
sudo make install
```

Then identify the real pointer event node and generate configuration from it:

```bash
sudo smoothwheel devices
sudo smoothwheel configure /dev/input/eventX
```

`configure` does **not** persist `/dev/input/eventX`. Event numbers are unstable across boots and reconnects. It stores the device vendor/product identity plus normalized kernel name in:

```text
/etc/smoothwheel/config.conf
```

The default generated profile is `balanced`. A different experimental profile can be selected during configuration:

```bash
sudo smoothwheel configure /dev/input/eventX --profile precision
```

Enable the service and start it immediately:

```bash
sudo smoothwheel service enable
```

The wrapper performs a systemd daemon reload before enabling the unit.

Before starting the service, a non-grabbing health check can validate the configuration and current device match:

```bash
sudo smoothwheel doctor
```

Day-to-day service controls are:

```bash
smoothwheel service status
smoothwheel service logs
sudo smoothwheel service restart
sudo smoothwheel service stop
sudo smoothwheel service start
sudo smoothwheel service disable
```

The daemon rediscovers the current event node from stable identity after startup or reconnect. If no matching mouse exists it waits. If the configured identity is ambiguous it refuses to grab any device instead of guessing. SmoothWheel-created virtual devices are excluded from capture candidates to prevent reinjection loops.

The systemd unit uses `Restart=on-failure`, but exit status 2 (invalid/missing configuration) is explicitly excluded from restart so a configuration mistake cannot create a restart storm. Ordinary mouse disappearance/reconnect is handled inside the daemon itself. Reconnect waits are signal-interruptible so stop/restart remains prompt even with a large `reconnect_ms`.

## Current configuration

Generated configuration is intentionally small:

```ini
device_vendor = 0x3151
device_product = 0x402d
device_name = Example Wireless Mouse
profile = balanced
reconnect_ms = 1000
```

Do not hand-edit event-node numbers into configuration. `device_vendor`, `device_product`, and `device_name` are the persistent selector.

## Diagnostics and reconnaissance

These commands are non-invasive and useful when diagnosing hardware:

```bash
smoothwheel inspect /dev/input/eventX
smoothwheel monitor /dev/input/eventX --record mouse.trace
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

See [`ROADMAP.md`](ROADMAP.md) for checkpoint scope and [`HANDOVER.md`](HANDOVER.md) for the current development state.

## Development-only virtual-wheel experiment

Checkpoint 2's uinput feasibility probes remain available:

```bash
./build/smoothwheel experiment --list
./build/smoothwheel experiment fine16 --dry-run
sudo ./build/smoothwheel experiment fine16
```

These are diagnostic tools, not the production motion model.

## Checkpoint archives

Use:

```bash
make checkpoint
```

when handing the repository to another environment. The archive excludes configured CMake build trees because CMake caches absolute source/build paths.
