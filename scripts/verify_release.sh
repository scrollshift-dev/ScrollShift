#!/bin/sh
set -eu

# verify_release.sh VERSION DIR
#
# Validates that DIR contains exactly the public release asset set the verified
# installer can consume: the two architecture archives and a structurally valid
# SHA256SUMS manifest with exactly one entry per archive, where every archive
# checksum matches. Exits non-zero with a diagnostic on any inconsistency and
# never modifies anything; the caller decides whether to create or accept a
# release.

version=${1:?usage: verify_release.sh VERSION DIR}
dir=${2:?usage: verify_release.sh VERSION DIR}
x86="scrollshift-$version-linux-x86_64.tar.gz"
arm="scrollshift-$version-linux-aarch64.tar.gz"
manifest="SHA256SUMS"

fail() { echo "release verification failed: $*" >&2; exit 1; }

[ -f "$dir/$x86" ] || fail "missing $x86"
[ -f "$dir/$arm" ] || fail "missing $arm"
[ -f "$dir/$manifest" ] || fail "missing $manifest"

# The asset set must contain exactly the expected files.
for entry in "$dir"/*; do
  base=$(basename "$entry")
  [ "$base" = "$x86" ] || [ "$base" = "$arm" ] || [ "$base" = "$manifest" ] || fail "unexpected asset: $base"
done

# SHA256SUMS must be well formed and contain exactly one entry per archive.
awk -v x="$x86" -v a="$arm" '
  {
    if (NF != 2 || length($1) != 64 || $1 !~ /^[0-9a-f]+$/) { bad = 1; next }
    if ($2 == x) cx++
    else if ($2 == a) ca++
    else unexpected = unexpected " " $2
  }
  END {
    if (bad || cx != 1 || ca != 1 || unexpected != "") {
      printf "invalid SHA256SUMS: bad=%d x86_entries=%d aarch64_entries=%d unexpected=%s\n",
             bad, cx, ca, unexpected > "/dev/stderr"
      exit 1
    }
  }
' "$dir/$manifest" || fail "SHA256SUMS is malformed"

for archive in "$x86" "$arm"; do
  expected=$(awk -v f="$archive" '$2==f {print $1}' "$dir/$manifest")
  actual=$(sha256sum "$dir/$archive" | awk '{print $1}')
  [ "$actual" = "$expected" ] || fail "checksum mismatch for $archive"
done

echo "verified release asset set: $x86 $arm $manifest"