#!/usr/bin/env python3
"""BCL OTA client — firmware transfer via UDS 0x34/0x36/0x37 over DoIP.

Implements the download side of ISO 14229-1 §14:
  0x34  RequestDownload   — announce firmware size
  0x36  TransferData      — send 512-byte chunks
  0x37  RequestTransferExit — finalise with CRC-32 validation

The rear node saves the received image to /tmp/ota_firmware_validated.bin.
During transfer the F102 NodeHealth DID returns health_state=0x04 (UPDATING).

Usage examples:
    python ota_client.py --host 127.0.0.1 --firmware /path/to/image.bin
    python ota_client.py --host 127.0.0.1 --dry-run
    python ota_client.py --host 127.0.0.1 --dry-run --size 4096

DoIP logical addresses:
    Tester   : 0x0E00
    Rear node: 0x0E01
"""

import argparse
import os
import socket
import struct
import sys
import zlib
from typing import List, Tuple


# ─────────────────────────────────────────────────────────────────────────────
# DoIP constants (ISO 13400-2:2019)
# ─────────────────────────────────────────────────────────────────────────────

DOIP_PROTOCOL_VERSION  = 0xFD
DOIP_INVERSE_VERSION   = 0x02
DOIP_PORT              = 13400

DOIP_TYPE_ROUTING_REQ  = 0x0005
DOIP_TYPE_ROUTING_RESP = 0x0006
DOIP_TYPE_DIAG_MSG     = 0x8001
DOIP_TYPE_DIAG_ACK     = 0x8002

TESTER_ADDR            = 0x0E00
ECU_ADDR               = 0x0E01

# UDS OTA service IDs
SID_SESSION_CTRL       = 0x10
SID_REQUEST_DOWNLOAD   = 0x34
SID_TRANSFER_DATA      = 0x36
SID_TRANSFER_EXIT      = 0x37
SID_NEGATIVE_RESPONSE  = 0x7F
POSITIVE_RESP_OFFSET   = 0x40

# OTA protocol constants
BLOCK_SIZE             = 512   # max data bytes per 0x36 request
ALFID                  = 0x44  # 4-byte address, 4-byte size

NRC_NAMES = {
    0x11: 'serviceNotSupported',
    0x12: 'subFunctionNotSupported',
    0x22: 'conditionsNotCorrect',
    0x31: 'requestOutOfRange',
    0x70: 'uploadDownloadNotAccepted',
    0x71: 'transferDataSuspended',
    0x72: 'generalProgrammingFailure',
    0x73: 'wrongBlockSequenceCounter',
}


# ─────────────────────────────────────────────────────────────────────────────
# DoIP framing (shared with bcl_diagnostic_client)
# ─────────────────────────────────────────────────────────────────────────────

def _build_doip_frame(payload_type: int, payload: bytes) -> bytes:
    header = struct.pack('>BBHI',
                         DOIP_PROTOCOL_VERSION,
                         DOIP_INVERSE_VERSION,
                         payload_type,
                         len(payload))
    return header + payload


def _recv_exact(sock: socket.socket, n: int) -> bytes:
    buf = b''
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise ConnectionError('Connection closed by peer')
        buf += chunk
    return buf


def _recv_doip_frame(sock: socket.socket) -> Tuple[int, bytes]:
    header = _recv_exact(sock, 8)
    _, _, payload_type, length = struct.unpack('>BBHI', header)
    payload = _recv_exact(sock, length) if length > 0 else b''
    return payload_type, payload


# ─────────────────────────────────────────────────────────────────────────────
# DoIP client
# ─────────────────────────────────────────────────────────────────────────────

class DoIPClient:
    def __init__(self, host: str, port: int = DOIP_PORT, timeout: float = 10.0):
        self.host    = host
        self.port    = port
        self.timeout = timeout
        self.sock    = None

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
        self.sock.sendall(_build_doip_frame(DOIP_TYPE_ROUTING_REQ, payload))
        ptype, data = _recv_doip_frame(self.sock)
        if ptype != DOIP_TYPE_ROUTING_RESP:
            raise RuntimeError(
                f'Expected routing activation response, got 0x{ptype:04X}')
        if len(data) < 5 or data[4] != 0x10:
            code = data[4] if len(data) >= 5 else 0
            raise RuntimeError(
                f'Routing activation failed: code=0x{code:02X}')

    def send_uds(self, uds_bytes: bytes) -> bytes:
        payload = struct.pack('>HH', TESTER_ADDR, ECU_ADDR) + uds_bytes
        self.sock.sendall(_build_doip_frame(DOIP_TYPE_DIAG_MSG, payload))
        while True:
            ptype, data = _recv_doip_frame(self.sock)
            if ptype == DOIP_TYPE_DIAG_ACK:
                continue
            if ptype == DOIP_TYPE_DIAG_MSG:
                if len(data) < 4:
                    raise RuntimeError('Diagnostic response too short')
                return data[4:]
            raise RuntimeError(f'Unexpected DoIP frame type: 0x{ptype:04X}')

    def __enter__(self):
        self.connect()
        self.activate_routing()
        return self

    def __exit__(self, *_):
        self.close()


# ─────────────────────────────────────────────────────────────────────────────
# UDS helpers
# ─────────────────────────────────────────────────────────────────────────────

def _check_positive(resp: bytes, expected_sid: int) -> bytes:
    if not resp:
        raise RuntimeError('Empty UDS response')
    if resp[0] == SID_NEGATIVE_RESPONSE:
        nrc = resp[2] if len(resp) >= 3 else 0
        nrc_name = NRC_NAMES.get(nrc, f'0x{nrc:02X}')
        raise RuntimeError(
            f'Negative response for SID 0x{expected_sid:02X}: NRC {nrc_name}')
    expected = expected_sid + POSITIVE_RESP_OFFSET
    if resp[0] != expected:
        raise RuntimeError(
            f'Unexpected SID: got 0x{resp[0]:02X}, expected 0x{expected:02X}')
    return resp


def _enter_extended_session(client: DoIPClient) -> None:
    _check_positive(
        client.send_uds(bytes([SID_SESSION_CTRL, 0x03])),
        SID_SESSION_CTRL)


# ─────────────────────────────────────────────────────────────────────────────
# OTA transfer
# ─────────────────────────────────────────────────────────────────────────────

def _split_blocks(data: bytes, block_size: int) -> List[bytes]:
    return [data[i:i + block_size] for i in range(0, len(data), block_size)]


def transfer_firmware(client: DoIPClient,
                      firmware: bytes,
                      verbose: bool = True) -> None:
    size       = len(firmware)
    crc32_val  = zlib.crc32(firmware) & 0xFFFFFFFF
    blocks     = _split_blocks(firmware, BLOCK_SIZE)
    num_blocks = len(blocks)

    if verbose:
        print(f'Firmware size : {size} bytes')
        print(f'Block count  : {num_blocks} × {BLOCK_SIZE}-byte blocks')
        print(f'CRC-32       : 0x{crc32_val:08X}')

    # Enter extended diagnostic session.
    _enter_extended_session(client)
    if verbose:
        print('Extended diagnostic session: OK')

    # 0x34 RequestDownload — ALFID=0x44, address=0, size=firmware_size
    req_dl = bytes([SID_REQUEST_DOWNLOAD, 0x00, ALFID,
                    0x00, 0x00, 0x00, 0x00,                       # memory address
                    (size >> 24) & 0xFF, (size >> 16) & 0xFF,
                    (size >>  8) & 0xFF,  size & 0xFF])
    resp = _check_positive(client.send_uds(req_dl), SID_REQUEST_DOWNLOAD)
    if verbose:
        max_bl_hi = resp[2] if len(resp) >= 3 else 0
        max_bl_lo = resp[3] if len(resp) >= 4 else 0
        max_bl    = (max_bl_hi << 8) | max_bl_lo
        print(f'RequestDownload: accepted, maxBlockLength={max_bl}')

    # 0x36 TransferData — send each block.
    block_seq = 0x01
    for idx, block in enumerate(blocks):
        req_td = bytes([SID_TRANSFER_DATA, block_seq]) + block
        _check_positive(client.send_uds(req_td), SID_TRANSFER_DATA)

        pct = int(100 * (idx + 1) / num_blocks)
        if verbose:
            print(f'  Transferring: {pct:3d}% ({idx + 1}/{num_blocks} blocks)',
                  end='\r', flush=True)

        block_seq = (block_seq + 1) & 0xFF

    if verbose:
        print()  # newline after progress
        print('TransferData: all blocks sent')

    # 0x37 RequestTransferExit — include CRC-32 (big-endian, 4 bytes).
    req_exit = bytes([SID_TRANSFER_EXIT,
                      (crc32_val >> 24) & 0xFF,
                      (crc32_val >> 16) & 0xFF,
                      (crc32_val >>  8) & 0xFF,
                       crc32_val        & 0xFF])
    _check_positive(client.send_uds(req_exit), SID_TRANSFER_EXIT)
    if verbose:
        print('RequestTransferExit: CRC validated, transfer complete.')
        print('Image saved to /tmp/ota_firmware_validated.bin on rear node.')


# ─────────────────────────────────────────────────────────────────────────────
# CLI
# ─────────────────────────────────────────────────────────────────────────────

def main() -> int:
    parser = argparse.ArgumentParser(
        description='BCL OTA client — UDS firmware transfer over DoIP (TCP)')
    parser.add_argument('--host', default='127.0.0.1',
                        help='IP address of the rear lighting node')
    parser.add_argument('--port', type=int, default=DOIP_PORT,
                        help=f'DoIP TCP port (default: {DOIP_PORT})')
    parser.add_argument('--timeout', type=float, default=10.0,
                        help='Socket timeout in seconds (default: 10.0)')
    parser.add_argument('--firmware', metavar='FILE',
                        help='Path to firmware binary (.bin or .hex)')
    parser.add_argument('--dry-run', action='store_true',
                        help='Generate synthetic firmware instead of reading a file')
    parser.add_argument('--size', type=int, default=2048,
                        help='Synthetic firmware size for --dry-run (default: 2048)')
    parser.add_argument('--quiet', action='store_true',
                        help='Suppress progress output')

    args = parser.parse_args()

    if not args.dry_run and not args.firmware:
        parser.error('Either --firmware FILE or --dry-run is required')

    if args.dry_run:
        firmware = bytes(
            (i * 0x37 + 0xA5) & 0xFF for i in range(args.size))
        print(f'[dry-run] Using synthetic firmware: {args.size} bytes')
    else:
        if not os.path.isfile(args.firmware):
            print(f'Error: firmware file not found: {args.firmware}',
                  file=sys.stderr)
            return 1
        with open(args.firmware, 'rb') as fh:
            firmware = fh.read()

    try:
        with DoIPClient(args.host, args.port, args.timeout) as client:
            transfer_firmware(client, firmware, verbose=not args.quiet)
    except (ConnectionRefusedError, ConnectionError) as exc:
        print(f'Connection failed: {exc}', file=sys.stderr)
        print(f'Is rear_lighting_node_simulator running at '
              f'{args.host}:{args.port}?', file=sys.stderr)
        return 1
    except Exception as exc:  # pylint: disable=broad-except
        print(f'OTA failed: {exc}', file=sys.stderr)
        return 1

    return 0


if __name__ == '__main__':
    sys.exit(main())
