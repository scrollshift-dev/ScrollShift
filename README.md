# SmoothWheel

**Native smooth mouse-wheel scrolling for Linux.**

SmoothWheel is an early-stage C++ utility intended to transform coarse mouse-wheel detents into fluid, high-resolution scrolling at the Linux input layer. The current repository is a development scaffold; it does **not** intercept input yet.

The working architecture is:

```text
physical mouse
    ↓
evdev
    ↓
SmoothWheel
    ├── pointer movement/buttons → unchanged
    └── wheel events → smoothing model
    ↓
uinput virtual pointer
    ↓
libinput → Wayland / XWayland / X11 → applications
```

This design is deliberately provisional. The first checkpoints exist to prove that the virtual high-resolution event path feels correct across real applications and that exclusive input capture can fail safely.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/smoothwheel --help
```

Requires a C++20 compiler and CMake 3.20+. Input dependencies such as libevdev will be introduced only when the corresponding checkpoint needs them.

## Project principles

- **Fail open whenever possible.** A daemon crash must not strand the user without pointer control.
- **Preserve non-wheel behaviour.** Motion, buttons and unrelated capabilities should remain transparent.
- **Prefer compositor-independent primitives.** Use the Linux input stack before reaching for desktop-specific hooks.
- **Measure instead of guessing.** Record event streams and latency; do not tune scroll feel entirely by intuition.
- **Keep the core small.** First make one wheel detent feel excellent everywhere. Add configuration and UI only when the underlying semantics are stable.

See [`ROADMAP.md`](ROADMAP.md) for the first ten checkpoints and [`HANDOVER.md`](HANDOVER.md) for the current development state.

## Read-only input reconnaissance

Checkpoint 1 adds non-invasive diagnostics. These commands **do not grab devices or inject input**.

```bash
./build/smoothwheel devices
./build/smoothwheel inspect /dev/input/eventX
./build/smoothwheel monitor /dev/input/eventX --record mouse.trace
```

`monitor` prints wheel events and packet boundaries while `--record` stores the complete raw event stream for deterministic fixture-driven development. Access to `/dev/input/event*` is commonly restricted; during development, run the diagnostic with sufficient read permission rather than changing device permissions globally.

## Development build

The shortest development path is:

```bash
make
make test
```

This is a thin convenience wrapper around CMake. The equivalent explicit commands are:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSMOOTHWHEEL_WARNINGS_AS_ERRORS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

### Checkpoint 2 virtual-wheel experiment

Checkpoint 2 creates a **temporary virtual uinput pointer only**. It does not grab, disable, or modify the physical mouse. List the available diagnostic gestures with:

```bash
./build/smoothwheel experiment --list
```

Preview exactly what a preset would emit without touching `/dev/uinput`:

```bash
./build/smoothwheel experiment fine16 --dry-run
```

Running a real experiment usually requires permission to open `/dev/uinput`, so during development it may be run with `sudo`. The command waits three seconds before emitting a single detent-equivalent gesture so the pointer can be moved over the target application:

```bash
sudo ./build/smoothwheel experiment fine16
```

These experiments are intentionally narrow feasibility probes. They are not yet the SmoothWheel smoothing algorithm.

## Experimental acceleration gate

After CP3 transparent pass-through was validated on real hardware, SmoothWheel added an experimental velocity-sensitive relay:

```bash
./build/smoothwheel accelerate --profiles
sudo ./build/smoothwheel accelerate /dev/input/eventX --profile balanced --seconds 20
```

This is intentionally time-bounded and developmental. It grabs the selected physical pointer only after creating a virtual replacement. Slow/isolated wheel input can deliberately fall below native detent speed while rapid same-direction cadence accelerates sharply. Balanced currently spans roughly 0.45x to 9x; the legacy wheel companion is accumulated fractionally so sub-detent high-resolution output does not falsely emit a full legacy detent each packet.
