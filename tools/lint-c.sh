#!/bin/sh
set -eu

if ! command -v clang >/dev/null 2>&1; then
    echo "clang not found; install clang to lint C sources." >&2
    exit 1
fi
if ! command -v clangd >/dev/null 2>&1; then
    echo "clangd not found; install clangd to check C includes." >&2
    exit 1
fi

flags=
while [ "$#" -gt 0 ]; do
    if [ "$1" = "--" ]; then
        shift
        break
    fi
    flags="$flags $1"
    shift
done

if [ "$#" -eq 0 ]; then
    echo "Usage: $0 <clang flags...> -- <source files...>" >&2
    exit 2
fi

status=0
syntax_checked=0
include_checked=0

for file in "$@"; do
    if ! clang -fsyntax-only $flags "$file"; then
        status=1
    fi
    syntax_checked=$((syntax_checked + 1))
done

for file in $(find src tests tools -type f \( -name '*.c' -o -name '*.h' \) ! -path '*/windows/wintun.h' | sort); do
    output=$(clangd --check="$file" --compile-commands-dir=. 2>&1 || true)
    diagnostics=$(printf '%s\n' "$output" |
        grep -E 'unused-includes|Included header .*not used directly|unnecessary #include' || true)
    if [ -n "$diagnostics" ]; then
        printf '%s\n' "$diagnostics" | sed "s|^|$file: |"
        status=1
    fi
    include_checked=$((include_checked + 1))
done

if [ "$status" -eq 0 ]; then
    echo "C syntax lint passed: $syntax_checked source files"
    echo "C include lint passed: $include_checked C/header files"
fi

exit "$status"
