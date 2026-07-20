#!/bin/sh
set -eu

if [ "$#" -ne 3 ]; then
    echo "Usage: $0 <url> <sha256> <output-dll>" >&2
    exit 2
fi

url=$1
expected_sha=$2
output=$3
tmp_zip=$(mktemp "${TMPDIR:-/tmp}/wintun.XXXXXX")
tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/wintun.XXXXXX")

cleanup() {
    rm -f "$tmp_zip"
    rm -rf "$tmp_dir"
}

trap cleanup EXIT INT TERM

echo "  ↓ Downloading wintun.dll from wintun.net..."
curl -fsSL "$url" -o "$tmp_zip"

if command -v shasum >/dev/null 2>&1; then
    printf '%s  %s\n' "$expected_sha" "$tmp_zip" | shasum -a 256 -c -
elif command -v sha256sum >/dev/null 2>&1; then
    printf '%s  %s\n' "$expected_sha" "$tmp_zip" | sha256sum -c -
else
    echo "No SHA-256 checker found (need shasum or sha256sum)." >&2
    exit 1
fi

unzip -o "$tmp_zip" wintun/bin/amd64/wintun.dll -d "$tmp_dir" >/dev/null
mkdir -p "$(dirname "$output")"
cp "$tmp_dir/wintun/bin/amd64/wintun.dll" "$output"
echo "  ✓ wintun.dll downloaded"
