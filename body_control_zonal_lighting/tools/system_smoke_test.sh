#!/usr/bin/env bash
# system_smoke_test.sh
#
# Smoke-tests the Linux simulation layer end-to-end:
#   1. Spawns rear_lighting_node_simulator in the background.
#   2. Waits for it to be ready, then runs diagnostic_console with a
#      pre-canned input that activates the park lamp and exits.
#   3. Verifies the diagnostic console output contains "ParkLamp -> On".
#   4. Kills the simulator cleanly.
#
# Note: central_zone_controller_app and diagnostic_console both bind to
# UDP port 41000 and cannot run simultaneously on the same host.  This
# script exercises the simulator + diagnostic-console path.  For a
# controller health-poll test, start the controller manually in a
# separate terminal.
#
# Exit 0 on success, non-zero on failure.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"

SIM_BIN="${BUILD_DIR}/app/rear_lighting_node_simulator"
DIAG_BIN="${BUILD_DIR}/app/diagnostic_console"

SIM_CONFIG="${PROJECT_ROOT}/config/vsomeip/rear_lighting_node_simulator.json"
CLIENT_CONFIG="${PROJECT_ROOT}/config/vsomeip/central_zone_controller.json"

LOG_DIR="$(mktemp -d)"
SIM_LOG="${LOG_DIR}/simulator.log"
DIAG_LOG="${LOG_DIR}/diagnostic_console.log"

SIM_PID=""

cleanup()
{
    if [ -n "${SIM_PID}" ] && kill -0 "${SIM_PID}" 2>/dev/null; then
        kill "${SIM_PID}" 2>/dev/null || true
        wait "${SIM_PID}" 2>/dev/null || true
    fi
    rm -rf "${LOG_DIR}"
}

trap cleanup EXIT

# ── verify binaries exist ─────────────────────────────────────────────────────
for bin in "${SIM_BIN}" "${DIAG_BIN}"; do
    if [ ! -x "${bin}" ]; then
        echo "ERROR: binary not found or not executable: ${bin}" >&2
        echo "       Run cmake --build build first." >&2
        exit 1
    fi
done

# ── start simulator ───────────────────────────────────────────────────────────
echo "[smoke] Starting rear_lighting_node_simulator..."
VSOMEIP_CONFIGURATION="${SIM_CONFIG}" \
VSOMEIP_APPLICATION_NAME="rear_lighting_node_simulator" \
    "${SIM_BIN}" >"${SIM_LOG}" 2>&1 &
SIM_PID=$!

# Wait up to 3 seconds for the simulator to report ready.
READY=0
for _ in 1 2 3 4 5 6; do
    sleep 0.5
    if grep -q "running" "${SIM_LOG}" 2>/dev/null; then
        READY=1
        break
    fi
done

if [ "${READY}" -eq 0 ]; then
    echo "ERROR: simulator did not start within 3 seconds." >&2
    cat "${SIM_LOG}" >&2
    exit 1
fi

echo "[smoke] Simulator is ready."

# ── send park-lamp activate via diagnostic console ────────────────────────────
# Input: 7 = Activate park lamp, 0 = Exit
echo "[smoke] Sending park lamp activate command via diagnostic_console..."
printf '7\n0\n' | \
    VSOMEIP_CONFIGURATION="${CLIENT_CONFIG}" \
    VSOMEIP_APPLICATION_NAME="diagnostic_console" \
        "${DIAG_BIN}" >"${DIAG_LOG}" 2>&1

DIAG_EXIT=$?

if [ "${DIAG_EXIT}" -ne 0 ]; then
    echo "ERROR: diagnostic_console exited with code ${DIAG_EXIT}." >&2
    cat "${DIAG_LOG}" >&2
    exit 1
fi

echo "[smoke] diagnostic_console output:"
cat "${DIAG_LOG}"

# ── verify round-trip ─────────────────────────────────────────────────────────
if grep -q "ParkLamp.*On" "${DIAG_LOG}"; then
    echo "[smoke] PASS: LampStatusEvent confirmed ParkLamp -> On."
    exit 0
else
    echo "ERROR: expected 'ParkLamp.*On' not found in diagnostic_console output." >&2
    exit 1
fi
