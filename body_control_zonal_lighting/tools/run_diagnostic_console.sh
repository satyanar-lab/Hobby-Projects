#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
EXECUTABLE_PATH="${BUILD_DIR}/app/diagnostic_console"
CONFIG_PATH="${PROJECT_ROOT}/config/vsomeip/diagnostic_console.json"

export VSOMEIP_CONFIGURATION="${CONFIG_PATH}"
export VSOMEIP_APPLICATION_NAME="diagnostic_console"

"${EXECUTABLE_PATH}"
