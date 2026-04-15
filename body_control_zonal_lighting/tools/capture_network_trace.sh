#!/usr/bin/env bash

set -euo pipefail

INTERFACE="${1:-lo}"
OUTPUT_FILE="${2:-vsomeip_trace.pcap}"

echo "Capturing SOME/IP traffic on interface: ${INTERFACE}"
echo "Output file: ${OUTPUT_FILE}"

sudo tcpdump -i "${INTERFACE}" udp port 30490 or udp port 30501 or udp port 30502 or udp port 30511 or udp port 30512 or udp port 30521 or udp port 30522 -w "${OUTPUT_FILE}"