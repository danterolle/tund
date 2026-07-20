#!/bin/sh
set -eu

if [ "$#" -ne 5 ]; then
    echo "Usage: $0 <peerforge> <peerforge-server> <port> <key> <log>" >&2
    exit 2
fi

peerforge=$1
peerforge_server=$2
port=$3
key=$4
log=$5
server_pid=

cleanup() {
    status=$?
    if [ -n "$server_pid" ]; then
        kill "$server_pid" >/dev/null 2>&1 || true
        wait "$server_pid" >/dev/null 2>&1 || true
    fi
    if [ "$status" -ne 0 ] && [ -f "$log" ]; then
        cat "$log" >&2
    fi
    exit "$status"
}

trap cleanup EXIT INT TERM

"$peerforge_server" -p "$port" -k "$key" -t 20 >"$log" 2>&1 &
server_pid=$!
sleep 1

"$peerforge" -s 127.0.0.1 -p "$port" -k "$key" -n 32 -t 3000 -K 2 -d 32
