#!/usr/bin/env python3
"""BCL diagnostic client — UDS over DoIP (ISO 14229-1 + ISO 13400-2).

Connects to the body control rear lighting node simulator (or hardware)
over a raw TCP socket and sends UDS diagnostic requests.

DoIP logical addresses used:
    Tester  : 0x0E00
    Rear node: 0x0E01

UDS DIDs implemented:
    0xF190 — ECU identification string
    0xF101 — Lamp output states (5 bytes)
    0xF102 — Node health (3 bytes)
    0xF103 — Active fault codes

Usage examples:
    python bcl_diagnostic_client.py --host 127.0.0.1 --read-ecu-info
    python bcl_diagnostic_client.py --host 127.0.0.1 --read-lamp-status
    python bcl_diagnostic_client.py --host 127.0.0.1 --read-fault-codes
    python bcl_diagnostic_client.py --host 127.0.0.1 --read-node-health
    python bcl_diagnostic_client.py --host 127.0.0.1 --read-dtc
    python bcl_diagnostic_client.py --host 127.0.0.1 --clear-dtc
    python bcl_diagnostic_client.py --host 127.0.0.1 --inject-fault 1
    python bcl_diagnostic_client.py --host 127.0.0.1 --clear-fault 1
    python bcl_diagnostic_client.py --host 127.0.0.1 --all
"""

import argparse
import socket
import struct
import sys
from typing import List


# ─────────────────────────────────────────────────────────────────────────────
# DoIP constants (ISO 13400-2:2019)
# ─────────────────────────────────────────────────────────────────────────────

DOIP_PROTOCOL_VERSION   = 0xFD
DOIP_INVERSE_VERSION    = 0x02
DOIP_PORT               = 13400

DOIP_TYPE_ROUTING_REQ   = 0x0005
DOIP_TYPE_ROUTING_RESP  = 0x0006
DOIP_TYPE_DIAG_MSG      = 0x8001
DOIP_TYPE_DIAG_ACK      = 0x8002

TESTER_ADDR             = 0x0E00
ECU_ADDR                = 0x0E01

# UDS service IDs
SID_SESSION_CTRL        = 0x10
SID_CLEAR_DTC           = 0x14
SID_READ_DTC            = 0x19
SID_READ_DID            = 0x22
SID_ROUTINE_CTRL        = 0x31
SID_NEGATIVE_RESPONSE   = 0x7F
POSITIVE_RESPONSE_OFFSET = 0x40

# DIDs
DID_ECU_ID              = 0xF190
DID_LAMP_STATUS         = 0xF101
DID_NODE_HEALTH         = 0xF102
DID_ACTIVE_FAULTS       = 0xF103

# Lamp function names indexed 1–5 (index 0 unused)
LAMP_NAMES = ['', 'LEFT IND', 'RIGHT IND', 'HAZARD', 'PARK', 'HEAD']
LAMP_STATE = {0: 'UNKNOWN', 1: 'OFF', 2: 'ON'}

HEALTH_STATE = {0: 'UNKNOWN', 1: 'OPERATIONAL', 2: 'DEGRADED', 3: 'FAULTED'}

NRC_NAMES = {
    0x11: 'serviceNotSupported',
    0x12: 'subFunctionNotSupported',
    0x22: 'conditionsNotCorrect',
    0x31: 'requestOutOfRange',
}


# ─────────────────────────────────────────────────────────────────────────────
# DoIP framing
# ─────────────────────────────────────────────────────────────────────────────

def build_doip_frame(payload_type: int, payload: bytes) -> bytes:
    header = struct.pack('>BBHI',
                         DOIP_PROTOCOL_VERSION,
                         DOIP_INVERSE_VERSION,
                         payload_type,
                         len(payload))
    return header + payload


def recv_exact(sock: socket.socket, n: int) -> bytes:
    buf = b''
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise ConnectionError('Connection closed by server')
        buf += chunk
    return buf


def recv_doip_frame(sock: socket.socket):
    header = recv_exact(sock, 8)
    _, _, payload_type, length = struct.unpack('>BBHI', header)
    payload = recv_exact(sock, length) if length > 0 else b''
    return payload_type, payload


# ─────────────────────────────────────────────────────────────────────────────
# DoIP client
# ─────────────────────────────────────────────────────────────────────────────

class DoIPClient:
    def __init__(self, host: str, port: int = DOIP_PORT, timeout: float = 5.0):
        self.host = host
        self.port = port
        self.timeout = timeout
        self.sock = None

    def connect(self) -> None:
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(self.timeout)
        self.sock.connect((self.host, self.port))

    def close(self) -> None:
        if self.sock:
            self.sock.close()
            self.sock = None

    def activate_routing(self) -> None:
        payload = struct.pack('>HBI', TESTER_ADDR, 0x00, 0x00000000)
        self.sock.sendall(build_doip_frame(DOIP_TYPE_ROUTING_REQ, payload))

        ptype, data = recv_doip_frame(self.sock)
        if ptype != DOIP_TYPE_ROUTING_RESP:
            raise RuntimeError(f'Expected routing activation response, got 0x{ptype:04X}')
        if len(data) < 5:
            raise RuntimeError('Routing activation response too short')
        code = data[4]
        if code != 0x10:
            raise RuntimeError(f'Routing activation failed: code=0x{code:02X}')

    def send_uds(self, uds_bytes: bytes) -> bytes:
        payload = struct.pack('>HH', TESTER_ADDR, ECU_ADDR) + uds_bytes
        self.sock.sendall(build_doip_frame(DOIP_TYPE_DIAG_MSG, payload))

        # Consume frames until we get a diagnostic message (skip ACKs).
        while True:
            ptype, data = recv_doip_frame(self.sock)
            if ptype == DOIP_TYPE_DIAG_ACK:
                continue
            if ptype == DOIP_TYPE_DIAG_MSG:
                if len(data) < 4:
                    raise RuntimeError('Diagnostic response too short')
                return data[4:]  # strip src/target addresses
            raise RuntimeError(f'Unexpected DoIP frame type: 0x{ptype:04X}')

    def __enter__(self):
        self.connect()
        self.activate_routing()
        return self

    def __exit__(self, *_):
        self.close()


# ─────────────────────────────────────────────────────────────────────────────
# UDS operations
# ─────────────────────────────────────────────────────────────────────────────

def check_positive(resp: bytes, expected_sid: int) -> bytes:
    if not resp:
        raise RuntimeError('Empty UDS response')
    if resp[0] == SID_NEGATIVE_RESPONSE:
        nrc = resp[2] if len(resp) >= 3 else 0
        nrc_name = NRC_NAMES.get(nrc, f'0x{nrc:02X}')
        raise RuntimeError(f'Negative response for SID 0x{expected_sid:02X}: NRC {nrc_name}')
    expected = expected_sid + POSITIVE_RESPONSE_OFFSET
    if resp[0] != expected:
        raise RuntimeError(
            f'Unexpected response SID: got 0x{resp[0]:02X}, expected 0x{expected:02X}')
    return resp


def read_ecu_info(client: DoIPClient) -> str:
    resp = check_positive(
        client.send_uds(bytes([SID_READ_DID,
                               (DID_ECU_ID >> 8) & 0xFF,
                               DID_ECU_ID & 0xFF])),
        SID_READ_DID)
    return resp[3:].decode('ascii', errors='replace')


def read_lamp_status(client: DoIPClient) -> List[str]:
    resp = check_positive(
        client.send_uds(bytes([SID_READ_DID,
                               (DID_LAMP_STATUS >> 8) & 0xFF,
                               DID_LAMP_STATUS & 0xFF])),
        SID_READ_DID)
    states = resp[3:]
    lines = []
    for i, raw in enumerate(states):
        name = LAMP_NAMES[i + 1] if i + 1 < len(LAMP_NAMES) else f'LAMP{i + 1}'
        lines.append(f'  {name:<12} {LAMP_STATE.get(raw, f"0x{raw:02X}")}')
    return lines


def read_node_health(client: DoIPClient) -> List[str]:
    resp = check_positive(
        client.send_uds(bytes([SID_READ_DID,
                               (DID_NODE_HEALTH >> 8) & 0xFF,
                               DID_NODE_HEALTH & 0xFF])),
        SID_READ_DID)
    data = resp[3:]
    if len(data) < 3:
        return ['  (short response)']
    state = HEALTH_STATE.get(data[0], f'0x{data[0]:02X}')
    flags = data[1]
    fault_count = data[2]
    return [
        f'  state        {state}',
        f'  eth_link     {"UP" if flags & 0x01 else "DOWN"}',
        f'  svc_avail    {"YES" if flags & 0x02 else "NO"}',
        f'  fault_present{"YES" if flags & 0x04 else "NO"}',
        f'  fault_count  {fault_count}',
    ]


def read_fault_codes(client: DoIPClient) -> List[str]:
    resp = check_positive(
        client.send_uds(bytes([SID_READ_DID,
                               (DID_ACTIVE_FAULTS >> 8) & 0xFF,
                               DID_ACTIVE_FAULTS & 0xFF])),
        SID_READ_DID)
    data = resp[3:]
    if not data:
        return ['  (short response)']
    count = data[0]
    if count == 0:
        return ['  No active faults.']
    lines = [f'  Active fault count: {count}']
    for i in range(count):
        offset = 1 + i * 2
        if offset + 1 < len(data):
            code = (data[offset] << 8) | data[offset + 1]
            lines.append(f'  DTC 0x{code:04X}')
    return lines


def read_dtc_information(client: DoIPClient) -> List[str]:
    resp = check_positive(
        client.send_uds(bytes([SID_READ_DTC, 0x02, 0x09])),
        SID_READ_DTC)
    data = resp[3:]
    if not data:
        return ['  No DTC records.']
    lines = []
    offset = 0
    while offset + 3 < len(data):
        dtc = (data[offset] << 16) | (data[offset + 1] << 8) | data[offset + 2]
        status = data[offset + 3]
        lines.append(f'  DTC 0x{dtc:06X}  status=0x{status:02X}')
        offset += 4
    return lines if lines else ['  No active DTCs.']


def clear_dtc(client: DoIPClient) -> str:
    check_positive(
        client.send_uds(bytes([SID_CLEAR_DTC, 0xFF, 0xFF, 0xFF])),
        SID_CLEAR_DTC)
    return 'All DTCs cleared.'


def inject_fault(client: DoIPClient, lamp_id: int) -> str:
    routine_id = 0xB000 + lamp_id
    resp = check_positive(
        client.send_uds(bytes([SID_ROUTINE_CTRL, 0x01,
                               (routine_id >> 8) & 0xFF,
                               routine_id & 0xFF])),
        SID_ROUTINE_CTRL)
    name = LAMP_NAMES[lamp_id] if 1 <= lamp_id <= 5 else f'lamp{lamp_id}'
    return f'Fault injected for {name} (0x{routine_id:04X}). Response: {resp.hex()}'


def clear_fault(client: DoIPClient, lamp_id: int) -> str:
    routine_id = 0xB000 + lamp_id
    resp = check_positive(
        client.send_uds(bytes([SID_ROUTINE_CTRL, 0x02,
                               (routine_id >> 8) & 0xFF,
                               routine_id & 0xFF])),
        SID_ROUTINE_CTRL)
    name = LAMP_NAMES[lamp_id] if 1 <= lamp_id <= 5 else f'lamp{lamp_id}'
    return f'Fault cleared for {name} (0x{routine_id:04X}). Response: {resp.hex()}'


# ─────────────────────────────────────────────────────────────────────────────
# CLI
# ─────────────────────────────────────────────────────────────────────────────

def main() -> int:
    parser = argparse.ArgumentParser(
        description='BCL UDS diagnostic client (DoIP over TCP)')
    parser.add_argument('--host', default='127.0.0.1',
                        help='IP address of the rear lighting node (default: 127.0.0.1)')
    parser.add_argument('--port', type=int, default=DOIP_PORT,
                        help=f'DoIP TCP port (default: {DOIP_PORT})')
    parser.add_argument('--timeout', type=float, default=5.0,
                        help='Socket timeout in seconds (default: 5.0)')

    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--read-ecu-info',    action='store_true',
                       help='Read ECU identification (DID 0xF190)')
    group.add_argument('--read-lamp-status', action='store_true',
                       help='Read all lamp output states (DID 0xF101)')
    group.add_argument('--read-node-health', action='store_true',
                       help='Read node health status (DID 0xF102)')
    group.add_argument('--read-fault-codes', action='store_true',
                       help='Read active fault codes (DID 0xF103)')
    group.add_argument('--read-dtc',         action='store_true',
                       help='ReadDTCInformation 0x19/0x02 (active DTCs)')
    group.add_argument('--clear-dtc',        action='store_true',
                       help='Clear all DTCs (0x14 0xFFFFFF)')
    group.add_argument('--inject-fault',     type=int, metavar='LAMP_ID',
                       help='Inject fault for lamp 1-5 via RoutineControl')
    group.add_argument('--clear-fault',      type=int, metavar='LAMP_ID',
                       help='Clear fault for lamp 1-5 via RoutineControl')
    group.add_argument('--all',              action='store_true',
                       help='Run all read operations')

    args = parser.parse_args()

    try:
        with DoIPClient(args.host, args.port, args.timeout) as client:
            if args.read_ecu_info or args.all:
                print(f'ECU Info: {read_ecu_info(client)}')

            if args.read_lamp_status or args.all:
                print('Lamp Status:')
                for line in read_lamp_status(client):
                    print(line)

            if args.read_node_health or args.all:
                print('Node Health:')
                for line in read_node_health(client):
                    print(line)

            if args.read_fault_codes or args.all:
                print('Active Fault Codes (DID F103):')
                for line in read_fault_codes(client):
                    print(line)

            if args.read_dtc or args.all:
                print('DTC Information (0x19):')
                for line in read_dtc_information(client):
                    print(line)

            if args.clear_dtc:
                print(clear_dtc(client))

            if args.inject_fault is not None:
                if not 1 <= args.inject_fault <= 5:
                    print('Error: LAMP_ID must be 1–5', file=sys.stderr)
                    return 1
                print(inject_fault(client, args.inject_fault))

            if args.clear_fault is not None:
                if not 1 <= args.clear_fault <= 5:
                    print('Error: LAMP_ID must be 1–5', file=sys.stderr)
                    return 1
                print(clear_fault(client, args.clear_fault))

    except (ConnectionRefusedError, ConnectionError) as exc:
        print(f'Connection failed: {exc}', file=sys.stderr)
        print(f'Is rear_lighting_node_simulator running at {args.host}:{args.port}?',
              file=sys.stderr)
        return 1
    except Exception as exc:  # pylint: disable=broad-except
        print(f'Error: {exc}', file=sys.stderr)
        return 1

    return 0


if __name__ == '__main__':
    sys.exit(main())
