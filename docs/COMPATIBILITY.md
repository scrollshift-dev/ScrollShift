# Compatibility testing

ScrollShift sits below the compositor, but applications and toolkits can still interpret wheel events differently. “System-wide” therefore needs evidence across representative desktop/application classes rather than being inferred from one browser.

## Capture the environment

Run as the logged-in desktop user (not through `sudo`):

```bash
scrollshift environment
```

Also record:

```bash
scrollshift --version
sudo scrollshift doctor
```

The environment command reports the kernel, session type, desktop, and Wayland/X display variables without touching input devices.

## Primary application matrix

For each application, test ordinary slow detents, normal repeated scrolling, a hard wheel spin, and a rapid direction reversal. The frozen `balanced` profile is the baseline.

| Class | Representative application | Slow precision | Fast acceleration | Reversal | Notes |
|---|---|---:|---:|---:|---|
| Browser | Firefox | pending | pending | pending | |
| Browser | Chromium/Chrome | pending | pending | pending | |
| Electron | VS Code or another Electron app | pending | pending | pending | |
| Terminal | desktop terminal | pending | pending | pending | |
| GTK | file manager / native GTK app | pending | pending | pending | |
| Qt | native Qt app | pending | pending | pending | |
| Document | PDF/document viewer | pending | pending | pending | |
| XWayland | representative XWayland app | pending | pending | pending | |

Record differences; do not introduce per-application hacks merely to make the table green. A workaround belongs in the core abstraction only when there is a defensible input-level rule behind it.

## What counts as a pass

A compatibility pass does not require every application to animate identically. It requires:

- slow wheel input remains controllable and does not jump unexpectedly;
- rapid wheel input produces clearly greater traversal than slow input;
- reversal responds immediately without stale opposing motion;
- buttons and pointer movement remain unaffected;
- the application remains usable after service restart/reconnect;
- no duplicated wheel input is observed.

If an application deliberately applies its own smoothing, note the interaction rather than assuming ScrollShift should override it.
