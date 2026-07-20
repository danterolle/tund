#!/bin/sh
set -u

script_dir=$(cd "$(dirname "$0")" && pwd -P)
app_path="$script_dir/tund-gui.app"
gui_bin="$app_path/Contents/MacOS/tund-gui"
gui_pid=

terminate_tree() {
    pid=$1
    ps -axo pid=,ppid= | awk -v parent="$pid" '$2 == parent { print $1 }' |
    while read -r child_pid; do
        terminate_tree "$child_pid"
    done
    kill -TERM "$pid" >/dev/null 2>&1 || true
}

cleanup() {
    if [ -n "$gui_pid" ]; then
        terminate_tree "$gui_pid"
        wait "$gui_pid" >/dev/null 2>&1 || true
    fi
}

trap cleanup EXIT INT TERM

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

sudo "$gui_bin" "$@" &
gui_pid=$!
wait "$gui_pid"
status=$?
gui_pid=

if [ "$status" -ne 0 ]; then
    echo
    echo "TunD exited with status $status."
    printf "Press Return to close this window. "
    read _ || true
fi

exit "$status"
