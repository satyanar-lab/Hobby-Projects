#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
EXECUTABLE_PATH="${BUILD_DIR}/app/hmi_control_panel_qt"
CONFIG_PATH="${PROJECT_ROOT}/config/vsomeip/central_zone_controller.json"

export VSOMEIP_CONFIGURATION="${CONFIG_PATH}"
export VSOMEIP_APPLICATION_NAME="hmi_control_panel"

# WSL2 does not expose a hardware GPU to Linux processes.  Force Qt to use the
# software rasteriser so the QML scene graph does not try to initialise EGL/Zink
# (which crashes with "failed to choose pdev" in a WSL2 environment).
if grep -qEi "microsoft|wsl" /proc/version 2>/dev/null; then
    export LIBGL_ALWAYS_SOFTWARE=1
    export QT_QUICK_BACKEND=software
fi

"${EXECUTABLE_PATH}"
