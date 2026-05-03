#!/bin/bash
cd "$(dirname "$0")/.."
export VSOMEIP_CONFIGURATION=config/vsomeip/diagnostic_console.json
export VSOMEIP_APPLICATION_NAME=diagnostic_console

exec ./build/app/diagnostic_console 2>/tmp/diagnostic_console_stderr.log
