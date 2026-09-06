#!/bin/sh
set -eu

# Deterministic tests for scripts/verify_release.sh covering the partial-release
# cases the existing-release verification must reject, in both the directory
# mode and the GitHub-layer --published asset-name mode. No network or
# privileges are required; the archives are small fixture files.

SCRIPT_DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
VERIFY="$SCRIPT_DIR/../scripts/verify_release.sh"

root=$(mktemp -d "${TMPDIR:-/tmp}/scrollshift-release-verify.XXXXXX")
trap 'rm -rf "$root"' EXIT HUP INT TERM

version="0.1.0-test"
tag="v$version"
x86="scrollshift-$version-linux-x86_64.tar.gz"
arm="scrollshift-$version-linux-aarch64.tar.gz"

printf 'x86-fixture' > "$root/$x86"
printf 'arm-fixture' > "$root/$arm"
x86sum=$(sha256sum "$root/$x86" | awk '{print $1}')
armsum=$(sha256sum "$root/$arm" | awk '{print $1}')

fail() { echo "FAIL: $1" >&2; exit 1; }

expect_fail() {
  if "$VERIFY" "$version" "$root" >/dev/null 2>&1; then fail "expected verification failure: $1"; fi
}
expect_ok() {
  if ! "$VERIFY" "$version" "$root" >/dev/null 2>&1; then fail "expected verification success: $1"; fi
}

write_manifest() { printf '%b' "$1" > "$root/SHA256SUMS"; }
full_manifest="$x86sum  $x86\n$armsum  $arm\n"

# Complete, matching set succeeds.
write_manifest "$full_manifest"
expect_ok "complete matching release"

# One archive missing.
rm -f "$root/$arm"
expect_fail "one archive missing"
printf 'arm-fixture' > "$root/$arm"

# Both archives present but SHA256SUMS missing.
rm -f "$root/SHA256SUMS"
expect_fail "SHA256SUMS missing"
write_manifest "$full_manifest"

# SHA256SUMS present but malformed.
write_manifest "this is not a checksum manifest\n"
expect_fail "malformed SHA256SUMS"
write_manifest "$full_manifest"

# Manifest missing one archive.
write_manifest "$x86sum  $x86\n"
expect_fail "manifest missing one archive"
write_manifest "$full_manifest"

# Duplicate archive entry.
write_manifest "$x86sum  $x86\n$x86sum  $x86\n$armsum  $arm\n"
expect_fail "duplicate archive entry"
write_manifest "$full_manifest"

# Published archive checksum differs from the published manifest.
write_manifest "$(printf '%064d' 0)  $x86\n$armsum  $arm\n"
expect_fail "published archive checksum differs"
write_manifest "$full_manifest"

# Unexpected extra asset in the directory set is rejected.
printf 'stray' > "$root/unexpected.txt"
expect_fail "unexpected asset present in directory"
rm -f "$root/unexpected.txt"

# Complete matching set succeeds again (idempotent rerun).
expect_ok "complete matching release after failure cases"

# ---- GitHub-layer --published asset-name mode ----
names="$root/published-names"
write_names() { printf '%b' "$1" > "$names"; }
# A normal GitHub release carries the expected assets plus the two auto source archives.
normal_set="$x86\n$arm\nSHA256SUMS\n$tag.tar.gz\n$tag.zip\n"

expect_published_fail() {
  if "$VERIFY" --published "$version" "$tag" "$names" >/dev/null 2>&1; then fail "expected published-set failure: $1"; fi
}
expect_published_ok() {
  if ! "$VERIFY" --published "$version" "$tag" "$names" >/dev/null 2>&1; then fail "expected published-set success: $1"; fi
}

# Complete expected set (with benign auto source archives) succeeds.
write_names "$normal_set"
expect_published_ok "complete published set with source archives"

# Missing one archive is rejected.
write_names "$arm\nSHA256SUMS\n$tag.tar.gz\n$tag.zip\n"
expect_published_fail "published set missing one archive"

# Missing SHA256SUMS is rejected.
write_names "$x86\n$arm\n$tag.tar.gz\n$tag.zip\n"
expect_published_fail "published set missing SHA256SUMS"

# An unexpected/stale extra asset is rejected even though all required assets are present.
write_names "$normal_set"
printf 'stale-0.1.0-previous.tar.gz\n' >> "$names"
expect_published_fail "unexpected extra published asset"

# A source archive name that does not match the tag is treated as unexpected.
write_names "$normal_set"
printf 'v0.0.1.tar.gz\n' >> "$names"
expect_published_fail "stale source archive name is unexpected"

# Complete expected set succeeds again.
write_names "$normal_set"
expect_published_ok "complete published set after failure cases"

echo "release verification tests passed"