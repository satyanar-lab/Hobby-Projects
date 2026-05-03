#include "body_control/lighting/application/ota_session_manager.hpp"

#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>

#include "body_control/lighting/domain/uds_service_ids.hpp"

namespace body_control
{
namespace lighting
{
namespace application
{

// ─────────────────────────────────────────────────────────────────────────────

OtaSessionManager::~OtaSessionManager() noexcept
{
    CloseStaging();
    // Remove any incomplete staging artifact on destruction.
    if (state_ == OtaState::kActive)
    {
        static_cast<void>(::unlink(kStagingPath));
    }
}

// ── 0x34 RequestDownload ──────────────────────────────────────────────────────

std::vector<std::uint8_t> OtaSessionManager::HandleRequestDownload(
    const std::vector<std::uint8_t>& req) noexcept
{
    // Only accept a new download when idle (or after a previous complete/failed
    // session has been acknowledged).
    if (state_ == OtaState::kActive)
    {
        return NegativeResponse(domain::uds::kSidRequestDownload,
                                domain::uds::kNrcUploadDownloadNotAccepted);
    }

    // Minimum: SID + DFI + ALFID + 4-byte address + 4-byte size = 11 bytes.
    // We require ALFID = 0x44 (4-byte address, 4-byte size).
    if (req.size() < 11U || req[2] != 0x44U)
    {
        return NegativeResponse(domain::uds::kSidRequestDownload,
                                domain::uds::kNrcRequestOutOfRange);
    }

    // Parse the 4-byte memory size (big-endian, bytes 7–10).
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

    // Open (or truncate) the staging file.
    const int fd = ::open(kStagingPath,
                          O_CREAT | O_WRONLY | O_TRUNC,
                          0600);
    if (fd < 0)
    {
        return NegativeResponse(domain::uds::kSidRequestDownload,
                                domain::uds::kNrcUploadDownloadNotAccepted);
    }

    staging_fd_     = fd;
    expected_size_  = size;
    received_size_  = 0U;
    next_block_seq_ = 0x01U;
    running_crc_    = 0xFFFF'FFFFU;
    state_          = OtaState::kActive;

    std::cout << "[OTA] RequestDownload accepted: " << size << " bytes expected\n";

    // Positive response: 0x74 + lengthFormatIdentifier (0x20 = 2-byte field)
    // + maxNumberOfBlockLength (includes 0x36 SID + blockSeqCtr = +2 bytes).
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

    // Minimum: SID + blockSequenceCounter + at least 1 data byte.
    if (req.size() < 3U)
    {
        return NegativeResponse(domain::uds::kSidTransferData,
                                domain::uds::kNrcRequestOutOfRange);
    }

    // Maximum data bytes per block (payload size = req.size() - 2).
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

    const std::uint8_t* data_ptr = req.data() + 2U;

    // Append block to staging file.
    const ::ssize_t written = ::write(staging_fd_, data_ptr, data_len);
    if (written < 0 ||
        static_cast<std::size_t>(written) != data_len)
    {
        state_ = OtaState::kFailed;
        CloseStaging();
        static_cast<void>(::unlink(kStagingPath));
        return NegativeResponse(domain::uds::kSidTransferData,
                                domain::uds::kNrcGeneralProgrammingFailure);
    }

    // Update running CRC and byte counter.
    running_crc_    = Crc32Update(running_crc_, data_ptr, data_len);
    received_size_ += static_cast<std::uint32_t>(data_len);

    // Advance block sequence counter (wraps 0xFF → 0x00).
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

    // Validate received byte count against what was announced in 0x34.
    if (received_size_ != expected_size_)
    {
        state_ = OtaState::kFailed;
        CloseStaging();
        static_cast<void>(::unlink(kStagingPath));
        Reset();
        return NegativeResponse(domain::uds::kSidRequestTransferExit,
                                domain::uds::kNrcConditionsNotCorrect);
    }

    // If the request contains 4 CRC bytes (req size == 5), validate them.
    if (req.size() == 5U)
    {
        const std::uint32_t expected_crc =
            (static_cast<std::uint32_t>(req[1]) << 24U) |
            (static_cast<std::uint32_t>(req[2]) << 16U) |
            (static_cast<std::uint32_t>(req[3]) <<  8U) |
             static_cast<std::uint32_t>(req[4]);

        const std::uint32_t actual_crc = running_crc_ ^ 0xFFFF'FFFFU;

        if (actual_crc != expected_crc)
        {
            state_ = OtaState::kFailed;
            CloseStaging();
            static_cast<void>(::unlink(kStagingPath));
            Reset();
            return NegativeResponse(domain::uds::kSidRequestTransferExit,
                                    domain::uds::kNrcGeneralProgrammingFailure);
        }
    }

    // Sync and close the staging file, then promote it to the validated path.
    ::fsync(staging_fd_);
    CloseStaging();

    if (::rename(kStagingPath, kValidatedPath) != 0)
    {
        state_ = OtaState::kFailed;
        Reset();
        return NegativeResponse(domain::uds::kSidRequestTransferExit,
                                domain::uds::kNrcGeneralProgrammingFailure);
    }

    std::cout << "[OTA] Transfer complete: received " << received_size_
              << " bytes, CRC OK. Image saved to " << kValidatedPath << '\n';

    state_ = OtaState::kComplete;
    Reset();

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
    if (staging_fd_ >= 0)
    {
        ::close(staging_fd_);
        staging_fd_ = -1;
    }
}

void OtaSessionManager::Reset() noexcept
{
    expected_size_  = 0U;
    received_size_  = 0U;
    next_block_seq_ = 0x01U;
    running_crc_    = 0xFFFF'FFFFU;
    // state_ is left as-is (kComplete or kFailed) so callers can inspect it;
    // a new 0x34 resets state to kActive.
}

}  // namespace application
}  // namespace lighting
}  // namespace body_control
