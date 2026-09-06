#!/bin/sh
set -eu
repo=${SCROLLSHIFT_GITHUB_REPOSITORY:-scrollshift-dev/ScrollShift}
version=${SCROLLSHIFT_VERSION:-latest}
need(){ command -v "$1" >/dev/null 2>&1 || { echo "ScrollShift installer: required command not found: $1" >&2; exit 1; }; }
need curl; need tar; need mktemp; need uname
case "$(uname -s)/$(uname -m)" in Linux/x86_64|Linux/amd64) platform=linux-x86_64;; Linux/aarch64|Linux/arm64) platform=linux-aarch64;; *) echo "ScrollShift installer: unsupported platform (Linux x86_64/aarch64 only)" >&2; exit 1;; esac
if [ "$version" = latest ]; then
  latest=$(curl -fsSL -o /dev/null -w '%{url_effective}' "https://github.com/$repo/releases/latest")
  version=${latest##*/}; case "$version" in v[0-9A-Za-z._-]*) ;; *) echo "ScrollShift installer: could not resolve latest release" >&2; exit 1;; esac
else case "$version" in v*) ;; *) version="v$version";; esac; fi
v=${version#v}; root="scrollshift-$v-$platform"; archive="$root.tar.gz"; base=${SCROLLSHIFT_RELEASE_BASE:-"https://github.com/$repo/releases/download/$version"}
tmp=$(mktemp -d "${TMPDIR:-/tmp}/scrollshift-install.XXXXXX"); trap 'rm -rf "$tmp"' EXIT HUP INT TERM
curl -fsSL "$base/$archive" -o "$tmp/$archive"
curl -fsSL "$base/SHA256SUMS" -o "$tmp/SHA256SUMS" || { echo "ScrollShift installer: refusing an unverified release" >&2; exit 1; }
count=$(awk -v f="$archive" '$0 ~ /^[0-9a-f]{64}  / && substr($0,67)==f {n++} END{print n+0}' "$tmp/SHA256SUMS")
[ "$count" -eq 1 ] || { echo "ScrollShift installer: expected exactly one checksum entry for $archive" >&2; exit 1; }
expected=$(awk -v f="$archive" '$0 ~ /^[0-9a-f]{64}  / && substr($0,67)==f {print substr($0,1,64)}' "$tmp/SHA256SUMS")
if command -v sha256sum >/dev/null 2>&1; then actual=$(sha256sum "$tmp/$archive"|awk '{print $1}'); elif command -v shasum >/dev/null 2>&1; then actual=$(shasum -a 256 "$tmp/$archive"|awk '{print $1}'); else echo "ScrollShift installer: sha256sum or shasum is required" >&2; exit 1; fi
[ "$actual" = "$expected" ] || { echo "ScrollShift installer: checksum verification failed" >&2; exit 1; }
tar -xzf "$tmp/$archive" -C "$tmp"; binary="$tmp/$root/scrollshift"; [ -f "$binary" ] && [ ! -L "$binary" ] || { echo "ScrollShift installer: malformed release archive" >&2; exit 1; }; chmod 0755 "$binary"
if [ "$(id -u)" -eq 0 ]; then "$binary" service install; elif command -v sudo >/dev/null 2>&1; then sudo "$binary" service install; else echo "ScrollShift installer: sudo is required for the system input service" >&2; exit 1; fi
printf '\nInstalled ScrollShift %s. Next:\n  sudo scrollshift devices\n  sudo scrollshift configure /dev/input/eventX\n  sudo scrollshift service start\n' "$v"
