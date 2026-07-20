#!/bin/sh
set -eu

mode=format
if [ "${1:-}" = "--check" ]; then
    mode=check
    shift
fi

if [ "$#" -ne 0 ]; then
    echo "Usage: $0 [--check]" >&2
    exit 2
fi

if ! command -v clang-format >/dev/null 2>&1; then
    echo "clang-format not found; install clang-format to format C sources." >&2
    exit 1
fi

files=$(find src tests tools -type f \( -name '*.c' -o -name '*.h' \) ! -path '*/windows/wintun.h' | sort)
file_count=$(printf '%s\n' "$files" | sed '/^$/d' | wc -l | tr -d ' ')

if [ "$mode" = "check" ]; then
    clang-format --dry-run --Werror $files
    echo "C format check passed: $file_count C/header files"
else
    clang-format -i $files
    echo "C format applied: $file_count C/header files"
fi
