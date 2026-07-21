#!/bin/sh
set -eu

if [ "$#" -ne 6 ]; then
    echo "Usage: $0 <peerforge> <peerforge-server> <port> <server-key> <client-key> <log>" >&2
    exit 2
fi

peerforge=$1
peerforge_server=$2
port=$3
server_key=$4
client_key=$5
log=$6
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

"$peerforge_server" -p "$port" -k "$server_key" -t 8 >"$log" 2>&1 &
server_pid=$!
sleep 1

set +e
"$peerforge" -s 127.0.0.1 -p "$port" -k "$client_key" -n 2 -t 800 -K 1 -d 1
status=$?
set -e

if [ "$status" -eq 0 ]; then
    echo "wrong-key Peerforge check unexpectedly succeeded" >&2
    exit 1
fi

echo "wrong-key Peerforge check passed"
