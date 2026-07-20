#!/bin/sh
set -u

script_dir=$(cd "$(dirname "$0")" && pwd -P)
app_path="$script_dir/tund-gui.app"
gui_bin="$app_path/Contents/MacOS/tund-gui"

if [ ! -x "$gui_bin" ]; then
    echo "Cannot find tund-gui inside: $app_path" >&2
    echo "Keep this launcher next to tund-gui.app." >&2
    echo
    printf "Press Return to close this window. "
    read _ || true
    exit 1
fi

echo "TunD needs administrator privileges to create the TUN interface."
echo "macOS may ask for your password."
echo

sudo "$gui_bin" "$@"
status=$?

if [ "$status" -ne 0 ]; then
    echo
    echo "TunD exited with status $status."
    printf "Press Return to close this window. "
    read _ || true
fi

exit "$status"
