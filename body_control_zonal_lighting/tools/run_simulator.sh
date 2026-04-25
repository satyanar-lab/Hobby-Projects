#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
EXECUTABLE_PATH="${BUILD_DIR}/app/rear_lighting_node_simulator"
CONFIG_PATH="${PROJECT_ROOT}/config/vsomeip/rear_lighting_node_simulator.json"

export VSOMEIP_CONFIGURATION="${CONFIG_PATH}"
export VSOMEIP_APPLICATION_NAME="rear_lighting_node_simulator"

_IFACE=$(ip -4 route show 192.168.0.0/24 2>/dev/null | awk '/dev/{for(i=1;i<=NF;i++) if($i=="dev") print $(i+1); exit}')
if [[ -n "${_IFACE}" ]]; then
    sudo ip route replace 224.224.224.245/32 dev "${_IFACE}" 2>/dev/null || true
else
    echo "WARNING: no interface on 192.168.0.0/24 — board unreachable. Did you run 'wsl --shutdown' after enabling mirrored networking?" >&2
fi

"${EXECUTABLE_PATH}"