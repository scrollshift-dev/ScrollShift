#!/bin/sh
set -eu

repo="${SCROLLSHIFT_GITHUB_REPOSITORY:-scrollshift-dev/ScrollShift}"
custom_install_dir="${SCROLLSHIFT_INSTALL_DIR:-}"
install_dir="${custom_install_dir:-$HOME/.local/bin}"
version="${SCROLLSHIFT_VERSION:-}"

need() {
    command -v "$1" >/dev/null 2>&1 || { echo "ScrollShift installer: required command not found: $1" >&2; exit 1; }
}
need curl
need tar
need uname
need mktemp

if [ -z "$version" ]; then
    latest_url="$(curl -fsSL -o /dev/null -w '%{url_effective}' "https://github.com/$repo/releases/latest")"
    version="${latest_url##*/v}"
    case "$version" in
        ''|*[!0-9A-Za-z._-]*) echo "ScrollShift installer: could not determine latest release version" >&2; exit 1 ;;
    esac
fi
version="${version#v}"

os="$(uname -s)"
arch="$(uname -m)"
case "$os/$arch" in
    Linux/x86_64|Linux/amd64) platform="linux-x86_64" ;;
    Linux/aarch64|Linux/arm64) platform="linux-aarch64" ;;
    *) echo "ScrollShift installer: unsupported platform: $os/$arch (ScrollShift is Linux-only)" >&2; exit 1 ;;
esac

root="scrollshift-$version-$platform"
archive="$root.tar.gz"
base="${SCROLLSHIFT_RELEASE_BASE:-https://github.com/$repo/releases/download/v$version}"
tmp="$(mktemp -d "${TMPDIR:-/tmp}/scrollshift-install.XXXXXX")"
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

curl -fsSL "$base/$archive" -o "$tmp/$archive"
curl -fsSL "$base/SHA256SUMS" -o "$tmp/SHA256SUMS"
expected="$(awk -v file="$archive" '$2 == file { print $1; exit }' "$tmp/SHA256SUMS")"
[ -n "$expected" ] || { echo "ScrollShift installer: checksum for $archive not found" >&2; exit 1; }

if command -v sha256sum >/dev/null 2>&1; then
    actual="$(sha256sum "$tmp/$archive" | awk '{print $1}')"
elif command -v shasum >/dev/null 2>&1; then
    actual="$(shasum -a 256 "$tmp/$archive" | awk '{print $1}')"
else
    echo "ScrollShift installer: sha256sum or shasum is required to verify the release" >&2
    exit 1
fi
[ "$actual" = "$expected" ] || { echo "ScrollShift installer: checksum verification failed for $archive" >&2; exit 1; }

tar -xzf "$tmp/$archive" -C "$tmp"
[ -f "$tmp/$root/scrollshift" ] || { echo "ScrollShift installer: release archive did not contain scrollshift" >&2; exit 1; }
mkdir -p "$install_dir"
cp "$tmp/$root/scrollshift" "$install_dir/scrollshift"
chmod 0755 "$install_dir/scrollshift"

printf 'Installed ScrollShift %s to %s/scrollshift\n' "$version" "$install_dir"
case ":${PATH:-}:" in
    *":$install_dir:"*) ;;
    *) printf 'Add %s to PATH to run scrollshift from any directory.\n' "$install_dir" ;;
esac

printf '\nScrollShift is a Linux input daemon. The binary needs the systemd unit and\n'
printf 'configuration to run: see https://scrollshift-dev.github.io/docs/configuration\n'
printf 'for the (root) setup steps.\n'
