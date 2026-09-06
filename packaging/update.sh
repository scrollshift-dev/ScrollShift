#!/bin/sh
set -eu
tmp=$(mktemp "${TMPDIR:-/tmp}/scrollshift-update.XXXXXX")
trap 'rm -f "$tmp"' EXIT HUP INT TERM
curl -fsSL https://scrollshift.dev/install.sh -o "$tmp"
sh -n "$tmp"
sh "$tmp"
