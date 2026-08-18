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
