#!/bin/sh
set -eu
repo=${SCROLLSHIFT_GITHUB_REPOSITORY:-scrollshift-dev/ScrollShift}; version=${SCROLLSHIFT_VERSION:-latest}; output=scrollshift; force=0
while [ $# -gt 0 ]; do case "$1" in --version) shift; version=${1:?};; --output) shift; output=${1:?};; --force) force=1;; --help) echo 'usage: download.sh [--version VERSION] [--output PATH] [--force]'; exit 0;; *) echo "unknown option: $1" >&2; exit 2;; esac; shift; done
[ ! -e "$output" ] || [ "$force" -eq 1 ] || { echo "refusing to overwrite $output (use --force)" >&2; exit 1; }
case "$(uname -s)/$(uname -m)" in Linux/x86_64|Linux/amd64) platform=linux-x86_64;; Linux/aarch64|Linux/arm64) platform=linux-aarch64;; *) echo 'unsupported platform' >&2; exit 1;; esac
if [ "$version" = latest ]; then latest=$(curl -fsSL -o /dev/null -w '%{url_effective}' "https://github.com/$repo/releases/latest"); version=${latest##*/}; else case "$version" in v*) ;; *) version="v$version";; esac; fi
v=${version#v}; root="scrollshift-$v-$platform"; archive="$root.tar.gz"; base=${SCROLLSHIFT_RELEASE_BASE:-"https://github.com/$repo/releases/download/$version"}; tmp=$(mktemp -d "${TMPDIR:-/tmp}/scrollshift-download.XXXXXX"); trap 'rm -rf "$tmp"' EXIT HUP INT TERM
curl -fsSL "$base/$archive" -o "$tmp/$archive"; curl -fsSL "$base/SHA256SUMS" -o "$tmp/SHA256SUMS" || { echo 'refusing an unverified release' >&2; exit 1; }
count=$(awk -v f="$archive" '$0 ~ /^[0-9a-f]{64}  / && substr($0,67)==f {n++} END{print n+0}' "$tmp/SHA256SUMS"); [ "$count" -eq 1 ] || { echo 'invalid checksum manifest' >&2; exit 1; }; expected=$(awk -v f="$archive" '$0 ~ /^[0-9a-f]{64}  / && substr($0,67)==f {print substr($0,1,64)}' "$tmp/SHA256SUMS")
if command -v sha256sum >/dev/null 2>&1; then actual=$(sha256sum "$tmp/$archive"|awk '{print $1}'); else actual=$(shasum -a 256 "$tmp/$archive"|awk '{print $1}'); fi; [ "$actual" = "$expected" ] || { echo 'checksum verification failed' >&2; exit 1; }
tar -xzf "$tmp/$archive" -C "$tmp"; binary="$tmp/$root/scrollshift"; if [ ! -f "$binary" ] || [ -L "$binary" ]; then echo 'malformed archive' >&2; exit 1; fi; chmod 0755 "$binary"; mv -f "$binary" "$output"; echo "Downloaded ScrollShift $v to $output"
