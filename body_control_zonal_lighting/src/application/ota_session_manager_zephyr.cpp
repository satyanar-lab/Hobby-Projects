// OtaSessionManager — Zephyr RTOS implementation.
//
// Implements UDS 0x34/0x36/0x37 OTA transfer with real flash write via
// Zephyr's stream_flash API.  Received firmware blocks are written
// progressively to slot1_partition (the MCUboot secondary slot) as they
// arrive in 0x36 TransferData requests.  On successful 0x37
// RequestTransferExit the slot is committed with
// boot_request_upgrade(BOOT_UPGRADE_TEST), then a 100 ms delayed reboot
// is scheduled so the DoIP TCP stack can flush the positive response
// before the system resets.
//
// MCUboot swap-using-move mode: on the next boot MCUboot copies slot1 to
// slot0, launches the new image, and waits for boot_write_img_confirmed()
// (called from HealthThread after the first successful health event TX).
// If confirmation never arrives, MCUboot reverts to the previous image on
// the following boot.
//
// Thread safety: exactly one OtaSessionManager instance exists on the
// device and is accessed only from the DoIP/UDS thread.  No locking needed.
//
// Key constraint: stream_flash scratch buffer must be a multiple of the
// flash write-block-size.  STM32H753ZI internal flash requires 32-byte
// aligned writes, so s_stream_buf is sized at 1024 bytes (32 × 32).

#ifdef CONFIG_BOOTLOADER_MCUBOOT

#include <zephyr/dfu/mcuboot.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/storage/stream_flash.h>
#include <zephyr/sys/reboot.h>

#include "body_control/lighting/application/ota_session_manager.hpp"
#include "body_control/lighting/domain/uds_service_ids.hpp"

LOG_MODULE_REGISTER(ota_session_mgr, LOG_LEVEL_INF);

// ── File-scope flash state ────────────────────────────────────────────────────
//
// One OtaSessionManager instance per device; file-scope statics are safe.
// The stream_flash context and flash_area pointer are invalidated after
// CloseStaging() and re-initialised on the next HandleRequestDownload call.

static const struct flash_area* s_flash_area {nullptr};
static struct stream_flash_ctx  s_stream_ctx {};

// Write-block-size for STM32H753ZI internal flash is 32 bytes.
// Buffer must be a multiple of write-block-size; 1024 = 32 × 32.
alignas(4) static std::uint8_t s_stream_buf[1024U];

// ── Delayed reboot ────────────────────────────────────────────────────────────
//
// Scheduled 100 ms after successful RequestTransferExit so the DoIP thread
// can transmit the positive response before sys_reboot() fires.

static void RebootWorkHandler(struct k_work* /*work*/) noexcept
{
    LOG_INF("[OTA] Rebooting into new image");
    sys_reboot(SYS_REBOOT_COLD);
}

static K_WORK_DELAYABLE_DEFINE(s_reboot_work, RebootWorkHandler);

// ─────────────────────────────────────────────────────────────────────────────

namespace body_control
{
namespace lighting
{
namespace application
{

OtaSessionManager::~OtaSessionManager() noexcept
{
    CloseStaging();
}

// ── 0x34 RequestDownload ──────────────────────────────────────────────────────

std::vector<std::uint8_t> OtaSessionManager::HandleRequestDownload(
    const std::vector<std::uint8_t>& req) noexcept
{
    if (state_ == OtaState::kActive)
    {
        return NegativeResponse(domain::uds::kSidRequestDownload,
                                domain::uds::kNrcUploadDownloadNotAccepted);
    }

    // Minimum: SID(1) + DFI(1) + ALFID(1) + addr(4) + size(4) = 11 bytes.
    // ALFID 0x44 = 4-byte address length + 4-byte memory size length.
    if (req.size() < 11U || req[2] != 0x44U)
    {
        return NegativeResponse(domain::uds::kSidRequestDownload,
                                domain::uds::kNrcRequestOutOfRange);
    }

    const std::uint32_t size =
        (static_cast<std::uint32_t>(req[7])  << 24U) |
        (static_cast<std::uint32_t>(req[8])  << 16U) |
        (static_cast<std::uint32_t>(req[9])  <<  8U) |
         static_cast<std::uint32_t>(req[10]);

    if (size == 0U || size > kMaxFirmwareSize)
    {
        return NegativeResponse(domain::uds::kSidRequestDownload,
                                domain::uds::kNrcRequestOutOfRange);
    }

    // Open slot1_partition (MCUboot secondary slot) for streaming writes.
    // FIXED_PARTITION_ID resolves the DTS label at compile time.
    int rc = flash_area_open(FIXED_PARTITION_ID(slot1_partition), &s_flash_area);
    if (rc != 0)
    {
        LOG_ERR("[OTA] flash_area_open slot1 failed: %d", rc);
        s_flash_area = nullptr;
        return NegativeResponse(domain::uds::kSidRequestDownload,
                                domain::uds::kNrcUploadDownloadNotAccepted);
    }

    // Initialise the streaming flash context.  fa_dev is the underlying flash
    // device; fa_off and fa_size bound the write window to slot1 only.
    rc = stream_flash_init(
        &s_stream_ctx,
        s_flash_area->fa_dev,
        s_stream_buf,
        sizeof(s_stream_buf),
        s_flash_area->fa_off,
        s_flash_area->fa_size,
        nullptr);

    if (rc != 0)
    {
        LOG_ERR("[OTA] stream_flash_init failed: %d", rc);
        flash_area_close(s_flash_area);
        s_flash_area = nullptr;
        return NegativeResponse(domain::uds::kSidRequestDownload,
                                domain::uds::kNrcUploadDownloadNotAccepted);
    }

    expected_size_  = size;
    received_size_  = 0U;
    next_block_seq_ = 0x01U;
    running_crc_    = 0xFFFF'FFFFU;
    state_          = OtaState::kActive;

    LOG_INF("[OTA] RequestDownload accepted: %u bytes expected", size);

    // Positive response: 0x74 + lengthFormatIdentifier + maxBlockLength(2).
    const std::uint8_t mbl_hi =
        static_cast<std::uint8_t>(kMaxBlockLength >> 8U);
    const std::uint8_t mbl_lo =
        static_cast<std::uint8_t>(kMaxBlockLength & 0xFFU);

    return {
        static_cast<std::uint8_t>(domain::uds::kSidRequestDownload +
                                  domain::uds::kPositiveResponseOffset),
        0x20U,
        mbl_hi,
        mbl_lo
    };
}

// ── 0x36 TransferData ─────────────────────────────────────────────────────────

std::vector<std::uint8_t> OtaSessionManager::HandleTransferData(
    const std::vector<std::uint8_t>& req) noexcept
{
    if (state_ != OtaState::kActive)
    {
        return NegativeResponse(domain::uds::kSidTransferData,
                                domain::uds::kNrcConditionsNotCorrect);
    }

    // Minimum: SID(1) + blockSeqCounter(1) + at least 1 data byte.
    if (req.size() < 3U)
    {
        return NegativeResponse(domain::uds::kSidTransferData,
                                domain::uds::kNrcRequestOutOfRange);
    }

    const std::size_t data_len = req.size() - 2U;
    if (data_len > kMaxDataBytesPerBlock)
    {
        return NegativeResponse(domain::uds::kSidTransferData,
                                domain::uds::kNrcRequestOutOfRange);
    }

    const std::uint8_t block_seq = req[1];
    if (block_seq != next_block_seq_)
    {
        return NegativeResponse(domain::uds::kSidTransferData,
                                domain::uds::kNrcWrongBlockSequenceCounter);
    }

    const std::uint8_t* const data_ptr = req.data() + 2U;

    // flush=false: data is buffered in s_stream_buf and written to flash only
    // when the buffer fills or flush=true is passed.  CONFIG_IMG_ERASE_PROGRESSIVELY
    // erases each 128 KB sector lazily at the first write crossing its boundary.
    const int rc = stream_flash_buffered_write(
        &s_stream_ctx, data_ptr, data_len, false);

    if (rc != 0)
    {
        LOG_ERR("[OTA] stream_flash_buffered_write block %u failed: %d",
                static_cast<unsigned>(block_seq), rc);
        state_ = OtaState::kFailed;
        CloseStaging();
        return NegativeResponse(domain::uds::kSidTransferData,
                                domain::uds::kNrcGeneralProgrammingFailure);
    }

    running_crc_    = Crc32Update(running_crc_, data_ptr, data_len);
    received_size_ += static_cast<std::uint32_t>(data_len);
    next_block_seq_ = static_cast<std::uint8_t>(
        (static_cast<std::uint16_t>(block_seq) + 1U) & 0xFFU);

    return {
        static_cast<std::uint8_t>(domain::uds::kSidTransferData +
                                  domain::uds::kPositiveResponseOffset),
        block_seq
    };
}

// ── 0x37 RequestTransferExit ──────────────────────────────────────────────────

std::vector<std::uint8_t> OtaSessionManager::HandleRequestTransferExit(
    const std::vector<std::uint8_t>& req) noexcept
{
    if (state_ != OtaState::kActive)
    {
        return NegativeResponse(domain::uds::kSidRequestTransferExit,
                                domain::uds::kNrcConditionsNotCorrect);
    }

    if (received_size_ != expected_size_)
    {
        LOG_ERR("[OTA] size mismatch: got %u, expected %u",
                received_size_, expected_size_);
        state_ = OtaState::kFailed;
        CloseStaging();
        Reset();
        return NegativeResponse(domain::uds::kSidRequestTransferExit,
                                domain::uds::kNrcConditionsNotCorrect);
    }

    // Require exactly SID(1) + CRC32(4) = 5 bytes. ISO 14229-1 makes the
    // parameterRecord optional, but we enforce it as defence-in-depth so
    // a bare 0x37 cannot bypass CRC validation.
    if (req.size() != 5U)
    {
        LOG_ERR("[OTA] 0x37 rejected: expected 5 bytes, got %zu", req.size());
        state_ = OtaState::kFailed;
        CloseStaging();
        Reset();
        return NegativeResponse(domain::uds::kSidRequestTransferExit,
                                domain::uds::kNrcIncorrectMessageLengthOrInvalidFormat);
    }

    const std::uint32_t expected_crc =
        (static_cast<std::uint32_t>(req[1]) << 24U) |
        (static_cast<std::uint32_t>(req[2]) << 16U) |
        (static_cast<std::uint32_t>(req[3]) <<  8U) |
         static_cast<std::uint32_t>(req[4]);

    const std::uint32_t actual_crc = running_crc_ ^ 0xFFFF'FFFFU;

    if (actual_crc != expected_crc)
    {
        LOG_ERR("[OTA] CRC mismatch: got 0x%08X, expected 0x%08X",
                actual_crc, expected_crc);
        state_ = OtaState::kFailed;
        CloseStaging();
        Reset();
        return NegativeResponse(domain::uds::kSidRequestTransferExit,
                                domain::uds::kNrcGeneralProgrammingFailure);
    }

    // Flush the remaining bytes in s_stream_buf to flash (flush=true).
    int rc = stream_flash_buffered_write(&s_stream_ctx, nullptr, 0U, true);
    if (rc != 0)
    {
        LOG_ERR("[OTA] stream flush failed: %d", rc);
        state_ = OtaState::kFailed;
        CloseStaging();
        Reset();
        return NegativeResponse(domain::uds::kSidRequestTransferExit,
                                domain::uds::kNrcGeneralProgrammingFailure);
    }

    CloseStaging();

    // Mark slot1 as pending test upgrade.  MCUboot swaps slot1→slot0 on the
    // next boot and waits for boot_write_img_confirmed() from the new image.
    // BOOT_UPGRADE_TEST (not _PERMANENT) enables automatic rollback if the
    // new image fails to confirm itself within one boot cycle.
    rc = boot_request_upgrade(BOOT_UPGRADE_TEST);
    if (rc != 0)
    {
        LOG_ERR("[OTA] boot_request_upgrade failed: %d", rc);
        state_ = OtaState::kFailed;
        Reset();
        return NegativeResponse(domain::uds::kSidRequestTransferExit,
                                domain::uds::kNrcGeneralProgrammingFailure);
    }

    LOG_INF("[OTA] Transfer complete: %u bytes written, CRC OK — reboot in 100 ms",
            received_size_);

    state_ = OtaState::kComplete;
    Reset();

    // 100 ms delay lets the DoIP TCP stack transmit the positive response
    // before sys_reboot() fires.  The reboot work runs on the system work queue.
    k_work_schedule(&s_reboot_work, K_MSEC(100));

    return {static_cast<std::uint8_t>(domain::uds::kSidRequestTransferExit +
                                      domain::uds::kPositiveResponseOffset)};
}

// ── State accessors ───────────────────────────────────────────────────────────

bool OtaSessionManager::IsOtaModeActive() const noexcept
{
    return state_ == OtaState::kActive;
}

OtaSessionManager::OtaState OtaSessionManager::GetState() const noexcept
{
    return state_;
}

// ── Private helpers ───────────────────────────────────────────────────────────

std::vector<std::uint8_t> OtaSessionManager::NegativeResponse(
    const std::uint8_t sid, const std::uint8_t nrc) noexcept
{
    return {domain::uds::kSidNegativeResponse, sid, nrc};
}

std::uint32_t OtaSessionManager::Crc32Update(
    std::uint32_t crc,
    const std::uint8_t* data,
    const std::size_t len) noexcept
{
    for (std::size_t i = 0U; i < len; ++i)
    {
        crc ^= static_cast<std::uint32_t>(data[i]);
        for (int bit = 0; bit < 8; ++bit)
        {
            crc = ((crc & 1U) != 0U)
                ? ((crc >> 1U) ^ 0xEDB8'8320U)
                : (crc >> 1U);
        }
    }
    return crc;
}

void OtaSessionManager::CloseStaging() noexcept
{
    if (s_flash_area != nullptr)
    {
        flash_area_close(s_flash_area);
        s_flash_area = nullptr;
    }
}

void OtaSessionManager::Reset() noexcept
{
    expected_size_  = 0U;
    received_size_  = 0U;
    next_block_seq_ = 0x01U;
    running_crc_    = 0xFFFF'FFFFU;
}

}  // namespace application
}  // namespace lighting
}  // namespace body_control

#endif  // CONFIG_BOOTLOADER_MCUBOOT
