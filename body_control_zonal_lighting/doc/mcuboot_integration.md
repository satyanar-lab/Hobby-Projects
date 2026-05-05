# MCUboot Integration — Zephyr OTA (Phase 13)

Real MCUboot-based dual-slot firmware update for the Zephyr RTOS target
(NUCLEO-H753ZI). Received firmware images are written to flash via the
stream_flash API; MCUboot performs the slot swap on the next boot.

---

## Flash partition layout

STM32H753ZI: 2 MB internal flash, dual-bank, 8 sectors × 128 KB per bank.

| Partition          | Offset       | Size   | Role                         |
|--------------------|--------------|--------|------------------------------|
| `boot_partition`   | 0x00000000   | 128 KB | MCUboot bootloader           |
| `storage_partition`| 0x00020000   | 128 KB | NVS / settings (reserved)    |
| `slot0_partition`  | 0x00040000   | 768 KB | Primary application image    |
| `slot1_partition`  | 0x00100000   | 768 KB | OTA staging image            |
| `scratch_partition`| 0x001C0000   | 256 KB | Swap workspace (present but unused with move mode) |

CPU base address: 0x08000000. Add to offsets for absolute addresses.

The stock Zephyr DTS for nucleo_h753zi defines undersized 256 KB slots.
The overlay (`boards/nucleo_h753zi.overlay`) deletes those nodes with
`/delete-node/` and redefines them at 768 KB.

---

## MCUboot configuration

- **Mode**: swap-using-move (supports rollback, no scratch partition needed)
- **Signature**: ECDSA-P256 (~50 ms boot verification on Cortex-M7 vs ~1500 ms for RSA-2048)
- **Key**: MCUboot test key `bootloader/mcuboot/root-ec-p256.pem`

> **DEMO KEY — never use in production.** Generate a project key with:
> ```bash
> imgtool keygen -k project_signing_key.pem -t ecdsa-p256
> ```

---

## Build commands

```bash
cd ~/zephyr-workspace

# Build MCUboot + signed application in one command
west build -b nucleo_h753zi --sysbuild \
  ~/workspace/Hobby-Projects/body_control_zonal_lighting/app/zephyr_nucleo_h753zi

# Artifacts:
#   build/mcuboot/zephyr/zephyr.bin          (~31 KB)  MCUboot bootloader
#   build/zephyr_nucleo_h753zi/zephyr/zephyr.signed.bin (~115 KB) signed app
```

### First-time flash (full chip)

Flash MCUboot to boot_partition and the initial application to slot0:

```bash
# Flash MCUboot bootloader
west flash --domain mcuboot

# Flash signed application to slot0
west flash --domain zephyr_nucleo_h753zi
```

Or use STM32CubeProgrammer to flash the merged hex:
```bash
# Generate merged hex (MCUboot + app)
west build -b nucleo_h753zi --sysbuild ... --build-type debug
# The merged hex is at build/zephyr_nucleo_h753zi/zephyr/zephyr.hex
```

### OTA update (after initial flash)

```bash
# Transfer signed image via UDS 0x34/0x36/0x37 over DoIP
python3 tools/ota_client/ota_client.py \
  --host 192.168.0.20 \
  --firmware ~/zephyr-workspace/build/zephyr_nucleo_h753zi/zephyr/zephyr.signed.bin
```

The OTA client sends the **signed** image (not the raw `zephyr.bin`).
MCUboot's ECDSA-P256 signature verification rejects unsigned images.

---

## OTA flow

```
Python OTA client                  NUCLEO-H753ZI (Zephyr)
─────────────────                  ──────────────────────
DoIP TCP connect (port 13400)  →
                               ←   routing activation response
0x34 RequestDownload           →
                               ←   0x74 positive response (maxBlockLen=514)
0x36 TransferData (block 1)   →   stream_flash_buffered_write()
0x36 TransferData (block 2)   →   stream_flash_buffered_write()
...                                (128 KB sector erased lazily at boundary)
0x37 RequestTransferExit +CRC →   flush stream, CRC validate
                               ←   0x77 positive response
                               ←   [100 ms delay — TCP stack flushes]
                                   boot_request_upgrade(BOOT_UPGRADE_TEST)
                                   sys_reboot(SYS_REBOOT_COLD)
                                   MCUboot swaps slot1 → slot0
                                   New image boots
                                   HealthThread: boot_write_img_confirmed()
```

### Rollback

If `boot_write_img_confirmed()` is never called (e.g. new image crashes
before Ethernet comes up), MCUboot reverts to the previous slot0 image on
the next boot. This is the BOOT_UPGRADE_TEST semantic.

---

## Image confirmation

`boot_write_img_confirmed()` is called in `HealthThread` after the first
successful NodeHealthStatus event transmission. This gate proves:
- GPIO driver initialised
- UDP transport and Ethernet link are operational
- Thread scheduler and dispatch loop are running

Calling it too early (e.g. immediately after `Initialize()`) would confirm
a partially-booted image before liveness is established.

---

## Recovery

If OTA produces a non-booting image and rollback also fails:

1. Connect ST-Link USB
2. Open STM32CubeProgrammer
3. Full chip erase
4. Re-flash MCUboot: `build/mcuboot/zephyr/zephyr.bin` at 0x08000000
5. Re-flash application: `build/zephyr_nucleo_h753zi/zephyr/zephyr.signed.bin` at 0x08040000
