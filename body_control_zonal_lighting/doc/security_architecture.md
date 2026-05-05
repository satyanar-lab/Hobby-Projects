# Security Architecture

## 1. Scope and disclaimers

This document describes the security posture of the Body Control Zonal Lighting
project as of the current release. It is portfolio/learning code, NOT production.
Where production-grade controls are absent, that gap is explicitly called out.

All claims in this document have been verified against the actual source code.
Where the implementation differs from what a production system would require,
the gap is documented in Section 5.

---

## 2. Threat model (informal)

The project is a body-control lighting node demonstrator connected to a controller
host and HMI over a local Ethernet segment.

| Asset | Attacker scenario | Severity if exploited |
|---|---|---|
| Lamp control state | Adversary on local network sends spoofed SOME/IP-shaped UDP commands to toggle lamps | Low — limited to lighting |
| Firmware integrity | Adversary delivers modified firmware via OTA | High — full ECU compromise |
| Diagnostic access | Adversary opens diagnostic session and reads/clears DTCs or injects faults | Medium — affects fleet diagnostics |
| Firmware confidentiality | Adversary extracts firmware from board to reverse-engineer or copy | Medium — IP exposure |

In a real vehicle context, attackers also include malicious diagnostic dongles
connected via OBD-II, compromised infotainment systems on the same Ethernet
segment, and supply-chain attackers with access to the manufacturing flash
programming step.

---

## 3. Implemented controls

### 3.1 Firmware image signing (Zephyr / MCUboot)

- **Algorithm**: ECDSA-P256
- **Key**: MCUboot tree test key at `bootloader/mcuboot/root-ec-p256.pem` (see gap 5.4)
- **Verification point**: MCUboot bootloader at every boot, before chainloading the application
- **Effect**: Firmware delivered via OTA cannot be modified or substituted without the private key
- **Boot verification cost**: ~50 ms on Cortex-M7

Verified in `app/zephyr_nucleo_h753zi/sysbuild/mcuboot.conf`:
```
CONFIG_BOOT_SIGNATURE_TYPE_ECDSA_P256=y
```
And in `app/zephyr_nucleo_h753zi/sysbuild.conf`:
```
SB_CONFIG_BOOT_SIGNATURE_TYPE_ECDSA_P256=y
```

### 3.2 OTA transfer integrity

- **Transport**: UDS over DoIP TCP (ISO 14229-1 / ISO 13400-2)
- **Block-level sequencing**: each TransferData block (UDS 0x36) carries a block sequence counter;
  out-of-order or duplicate blocks return NRC `kNrcWrongBlockSequenceCounter`
- **Size validation**: `HandleRequestTransferExit` (0x37) rejects the image if `received_size_ != expected_size_`
- **Optional CRC-32 validation**: if the tester appends 4 CRC bytes to the 0x37 request,
  the board validates CRC-32 against the running checksum accumulated over all 0x36 blocks;
  mismatch returns `kNrcGeneralProgrammingFailure` and aborts the session

  > **Note**: CRC validation is conditional on the client providing the CRC. If the tester sends
  > a bare 0x37 (1 byte), size validation passes and the image proceeds to MCUboot signature check.
  > MCUboot's ECDSA-P256 signature is the authoritative integrity gate.

- **Image-level integrity**: MCUboot validates the ECDSA-P256 signature on the staged image before swap

Verified in `src/application/ota_session_manager_zephyr.cpp`:
```cpp
// If the request carries 4 CRC bytes (req size == 5), validate them.
if (req.size() == 5U) { ... CRC check ... }
```

### 3.3 Anti-rollback (protection, not prevention)

- **Confirmation flag**: MCUboot is invoked with `BOOT_UPGRADE_TEST`; the new image must call
  `boot_write_img_confirmed()` before the next reboot
- **Effect**: An image that crashes, fails to confirm, or is rolled back never becomes permanent;
  MCUboot reverts to the previous image on the following boot
- **Gap**: this is rollback *protection* (a bad image rolls back automatically) but not rollback
  *prevention* (an attacker who possesses the signing key can still deliver an older signed
  image). See Section 5.1.

Verified in `src/application/ota_session_manager_zephyr.cpp`:
```cpp
rc = boot_request_upgrade(BOOT_UPGRADE_TEST);
```

### 3.4 Memory protection (Zephyr)

- **Stack overflow detection**: `CONFIG_HW_STACK_PROTECTION=y` enables MPU-based stack guards
- **Effect**: a stack overflow in any Zephyr thread triggers a fault immediately at the offending
  frame rather than silent corruption

Verified in `app/zephyr_nucleo_h753zi/prj.conf`:
```
# MPU stack guard pages catch stack overflows immediately at the offending frame.
CONFIG_HW_STACK_PROTECTION=y
```

---

## 4. Wire protocol exposure

| Protocol | Encryption | Authentication | Integrity | Where used |
|---|---|---|---|---|
| SOME/IP-shaped UDP (41000/41001) | None | None | None | Controller ↔ rear node |
| DoIP TCP (13400) | None (plain TCP) | None | TCP checksum only | Tester ↔ rear node |
| UDS over DoIP | None | None at protocol level | Optional CRC-32 on 0x37 payload | Diagnostic + OTA |
| vsomeip Unix sockets | N/A (local) | OS user-level | N/A (local) | Controller ↔ HMI |

All wire traffic is in cleartext. An attacker on the same Ethernet segment can:

- Read all lamp commands and status events
- Inject commands (no message authentication code)
- Replay previously captured commands
- Initiate an OTA session (but the image must still carry a valid ECDSA-P256 signature)

---

## 5. Identified gaps and production fix paths

### 5.1 No firmware version anti-rollback counter

**Risk**: An attacker who obtains a previously signed older firmware image (which had a known
vulnerability) can flash it via OTA. The signature is valid; MCUboot accepts it.

**Fix**: Embed a monotonic security version number in firmware image metadata. Store the
last-accepted version in a dedicated non-volatile counter (e.g., Zephyr NVS or OTP fuses).
The bootloader rejects images whose security version is lower than the stored value.
Update the stored version after a new image is confirmed.

### 5.2 No transport-layer security on automotive Ethernet

**Risk**: Local network attackers can read, inject, or replay SOME/IP-shaped frames or UDS messages.

**Fix**: For SOME/IP-shaped UDP, apply MACsec at L2 (IEEE 802.1AE) or DTLS at L4. For UDS
over DoIP, use TLS-encrypted DoIP per ISO 13400-2:2019 routing activation type 0xE0 (secured).

### 5.3 No ECU authentication or mutual authentication for diagnostic access

**Risk**: Any host that reaches TCP port 13400 can open a diagnostic session and invoke UDS
services without any challenge or credential.

**Fix**: Implement UDS Service 0x27 (Security Access) as a minimum gate before OTA-related
services (0x34/0x36/0x37) and fault-injection routines (0x31). For a production gateway,
replace with UDS Service 0x29 (Authentication, ISO 14229-1:2020) using PKI-based ECU
certificates and mutual authentication.

### 5.4 Test key in bootloader

**Risk**: This project uses the publicly distributed MCUboot test key
(`bootloader/mcuboot/root-ec-p256.pem`). Any image signed with that key is accepted.
The private key is freely available on GitHub.

**Fix**: Generate project-specific keys in an HSM:
```
imgtool keygen -k production_signing_key.pem -t ecdsa-p256
```
Embed only the public key in MCUboot. The private key never leaves the HSM. Sign release
images through a signing service that authenticates the requester before issuing a signature.

### 5.5 No secure boot at SoC level

**Risk**: MCUboot itself resides in addressable flash. An attacker with physical flash access
(e.g., via the exposed ST-LINK header) can overwrite or replace MCUboot, bypassing all
image signature checks.

**Fix**: Enable STM32H753ZI Read-Out Protection (RDP Level 1 or 2) to block JTAG/SWD
readout and direct flash access. Use STM32 RSS/SBSFU to verify MCUboot at the SoC trust
anchor level before executing it. Disable the debug interface in production units.

### 5.6 Diagnostic services unrestricted

**Risk**: UDS 0x14 (Clear DTC) and 0x31 (Routine Control / fault injection) are accessible
to any host without a security gate.

**Fix**: Require a successful UDS 0x27 Security Access seed-key exchange before allowing
destructive operations. In a multi-tier access model, fault injection requires a higher-level
security seed than DTC readout.

### 5.7 No firmware confidentiality

**Risk**: Firmware binary can be read out of flash via a debugger and reverse-engineered
to discover application logic or proprietary algorithms.

**Fix**: MCUboot supports encrypted images at rest (EC-P256 + AES). Keys are provisioned
per-device at manufacture. Combined with RDP Level 2, this makes offline extraction
impractical without physical decapping of the SoC.

### 5.8 Optional rather than mandatory CRC on OTA transfer exit

**Risk**: A tester that omits CRC bytes from the 0x37 request bypasses the transport-layer
integrity check; the image proceeds directly to MCUboot signature validation. A truncated
or partially corrupted image can reach MCUboot if the sequence counter accepts it.

**Fix**: Make CRC mandatory: if `req.size() != 5U` in `HandleRequestTransferExit`, return
`kNrcConditionsNotCorrect` instead of proceeding. This makes the CRC an enforced protocol
gate rather than an optional one, while MCUboot signature remains the authoritative
integrity check.

---

## 6. Compliance mapping

This project demonstrates concepts relevant to the following automotive security standards.
The project itself does not claim compliance — these are reference points for the
engineering work.

| Standard | Relevance | What this project demonstrates |
|---|---|---|
| ISO/SAE 21434 | Cybersecurity engineering for road vehicles | Threat model and gap analysis (this document) |
| UN R155 | Cybersecurity Management System for vehicle types | OTA traceability via UDS sessions and signed images |
| UN R156 | Software update process | Signed OTA, dual-bank rollback, version tracking |
| ISO 13400-2 | Diagnostic communication over IP | DoIP TCP server implementation |
| ISO 14229-1 | UDS services | 0x10 / 0x14 / 0x19 / 0x22 / 0x31 / 0x34 / 0x36 / 0x37; 0x27 gap documented in 5.6 |

---

## 7. Summary

The project implements **firmware integrity** end-to-end: ECDSA-P256 signing, dual-bank
MCUboot, automatic rollback on confirmation failure. It does NOT implement **transport
security**, **ECU authentication**, **anti-rollback version prevention**, or **firmware
confidentiality**. These gaps are documented above with production fix paths.

For a portfolio/learning project, this scope reflects a deliberate trade-off: depth on one
complete security chain (image integrity through the OTA path) over breadth across all chains.
The architectural patterns and code structure support adding the missing controls without
redesign of the core layers.
