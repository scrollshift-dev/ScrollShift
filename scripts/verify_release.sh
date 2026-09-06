#!/bin/sh
set -eu

# verify_release.sh VERSION DIR
# verify_release.sh --published VERSION TAG ASSET-NAME-FILE
#
# DIR mode: validates that DIR contains exactly the public release asset set
# the verified installer can consume: the two architecture archives and a
# structurally valid SHA256SUMS manifest with exactly one entry per archive,
# where every archive checksum matches.
#
# --published mode: validates the complete published GitHub asset-name set. Each
# non-empty line of ASSET-NAME-FILE is checked against the expected public set
# (the two architecture archives plus SHA256SUMS). GitHub auto-attaches source
# archives (<TAG>.tar.gz and <TAG>.zip) to every release, which are tolerated as
# benign; any other name is an unexpected published asset and is rejected.
#
# Both modes exit non-zero with a diagnostic on any inconsistency and never
# modify anything; the caller decides whether to create or accept a release.

if [ "${1:-}" = "--published" ]; then
  [ "$#" -eq 4 ] || { echo "usage: verify_release.sh --published VERSION TAG ASSET-NAME-FILE" >&2; exit 1; }
  version=$2
  tag=$3
  names=$4
  x86="scrollshift-$version-linux-x86_64.tar.gz"
  arm="scrollshift-$version-linux-aarch64.tar.gz"

  [ -f "$names" ] || { echo "published asset-name file not found: $names" >&2; exit 1; }

  missing=0
  for asset in "$x86" "$arm" "SHA256SUMS"; do
    grep -qx "$asset" "$names" || { echo "published release is missing $asset" >&2; missing=1; }
  done

  # Isolate genuine unexpected assets: drop empty lines, the three expected
  # names, and the two GitHub auto-attached source archives.
  extra=$(grep -v '^$' "$names" \
    | grep -vx "$x86" \
    | grep -vx "$arm" \
    | grep -vx "SHA256SUMS" \
    | grep -vx "$tag.tar.gz" \
    | grep -vx "$tag.zip" || true)

  if [ "$missing" -eq 1 ] || [ -n "$extra" ]; then
    [ -z "$extra" ] || { echo "unexpected published assets:" >&2; printf '%s\n' "$extra" >&2; }
    echo "published release asset set is not exactly the expected public set" >&2
    exit 1
  fi

  echo "verified published asset-name set matches the expected public set"
  exit 0
fi

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