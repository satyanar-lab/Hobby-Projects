#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
EXECUTABLE_PATH="${BUILD_DIR}/app/bczl_rear_lighting_node_simulator"
CONFIG_PATH="${PROJECT_ROOT}/config/vsomeip/rear_lighting_node_simulator.json"

export VSOMEIP_CONFIGURATION="${CONFIG_PATH}"
export VSOMEIP_APPLICATION_NAME="rear_lighting_node_simulator"

"${EXECUTABLE_PATH}"