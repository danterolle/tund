#!/bin/sh
set -eu

if ! command -v clangd >/dev/null 2>&1; then
    echo "clangd not found; install clangd to check C includes." >&2
    exit 1
fi

status=0
for file in $(find src tests tools -type f \( -name '*.c' -o -name '*.h' \) ! -path '*/windows/wintun.h' | sort); do
    output=$(clangd --check="$file" --compile-commands-dir=. 2>&1 || true)
    diagnostics=$(printf '%s\n' "$output" |
        grep -E 'unused-includes|Included header .*not used directly|unnecessary #include' || true)
    if [ -n "$diagnostics" ]; then
        printf '%s\n' "$diagnostics" | sed "s|^|$file: |"
        status=1
    fi
done
exit "$status"
