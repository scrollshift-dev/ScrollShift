#!/bin/sh
set -eu
purge=0
[ "${1:-}" != --purge ] || purge=1
bin=/usr/local/bin/scrollshift
if [ -x "$bin" ]; then
  if [ "$(id -u)" -eq 0 ]; then "$bin" service uninstall; elif command -v sudo >/dev/null 2>&1; then sudo "$bin" service uninstall; else echo 'sudo is required to uninstall the system service' >&2; exit 1; fi
fi
if [ "$(id -u)" -eq 0 ]; then rm -f "$bin"; [ "$purge" -eq 0 ] || rm -rf /etc/scrollshift; else sudo rm -f "$bin"; [ "$purge" -eq 0 ] || sudo rm -rf /etc/scrollshift; fi
echo 'Removed ScrollShift binary and service registration.'
[ "$purge" -eq 0 ] && echo 'Kept /etc/scrollshift configuration (pass --purge to remove it).'
