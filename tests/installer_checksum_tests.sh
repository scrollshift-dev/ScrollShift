#!/bin/sh
set -eu

# Deterministic checksum-handling tests for packaging/download.sh. Uses
# file:// URLs so no network access or privileges are required.

SCRIPT_DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
DOWNLOAD_SH="$SCRIPT_DIR/../packaging/download.sh"

root=$(mktemp -d "${TMPDIR:-/tmp}/scrollshift-installer-test.XXXXXX")
trap 'rm -rf "$root"' EXIT HUP INT TERM

version="0.1.0-test"
platform=linux-x86_64
archive="scrollshift-$version-$platform.tar.gz"
binary_dir="scrollshift-$version-$platform"
outdir="$root/out"
mkdir -p "$root/src/$binary_dir" "$outdir"

printf '#!/bin/sh\necho fixture\n' > "$root/src/$binary_dir/scrollshift"
(cd "$root/src" && tar -czf "$root/$archive" "$binary_dir")
expected=$(sha256sum "$root/$archive" | awk '{print $1}')

base="file://$root"
run_download() {
  (cd "$outdir" && SCROLLSHIFT_VERSION="$version" SCROLLSHIFT_RELEASE_BASE="$base" \
    sh "$DOWNLOAD_SH" --output scrollshift --force)
}

fail() { echo "FAIL: $1" >&2; exit 1; }

# Happy path: exactly one correct entry.
printf '%s  %s\n' "$expected" "$archive" > "$root/SHA256SUMS"
run_download || fail "happy path download failed"
[ -f "$outdir/scrollshift" ] || fail "happy path did not produce output"

# Missing entry.
printf '%s  other.tar.gz\n' "$expected" > "$root/SHA256SUMS"
if run_download 2>/dev/null; then fail "missing entry was not rejected"; fi
rm -f "$outdir/scrollshift"

# Duplicate entry.
printf '%s  %s\n%s  %s\n' "$expected" "$archive" "$expected" "$archive" > "$root/SHA256SUMS"
if run_download 2>/dev/null; then fail "duplicate entry was not rejected"; fi

# Malformed manifest.
printf 'this is not a checksum manifest\n' > "$root/SHA256SUMS"
if run_download 2>/dev/null; then fail "malformed manifest was not rejected"; fi

# Wrong checksum.
printf '%064d  %s\n' 0 "$archive" > "$root/SHA256SUMS"
if run_download 2>/dev/null; then fail "wrong checksum was not rejected"; fi

# Missing manifest entirely.
rm -f "$root/SHA256SUMS"
if run_download 2>/dev/null; then fail "missing manifest was not rejected"; fi

echo "installer checksum tests passed"