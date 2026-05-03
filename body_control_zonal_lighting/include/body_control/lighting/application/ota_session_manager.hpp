#pragma once

#include <cstdint>
#include <vector>

// OTA firmware-transfer session state machine (UDS services 0x34/0x36/0x37).
//
// Implements the download side of ISO 14229-1 §14 (RequestDownload /
// TransferData / RequestTransferExit).  The Linux implementation streams
// received blocks to /tmp/ota_firmware_staging.bin and validates the CRC-32
// (ISO 3309 / CRC-32/ISO-HDLC) at RequestTransferExit time.
//
// A separate OtaSessionManager rather than embedding state in
// UdsRequestHandler keeps the UDS handler a thin dispatcher and lets OTA
// session state be tested in isolation.

namespace body_control
{
namespace lighting
{
namespace application
{

class OtaSessionManager
{
public:
    enum class OtaState : std::uint8_t
    {
        kIdle     = 0U,
        kActive   = 1U,
        kComplete = 2U,
        kFailed   = 3U,
    };

    OtaSessionManager() noexcept = default;
    ~OtaSessionManager() noexcept;

    OtaSessionManager(const OtaSessionManager&)            = delete;
    OtaSessionManager& operator=(const OtaSessionManager&) = delete;

    // ── UDS service handlers ───────────────────────────────────────────────────

    // 0x34 RequestDownload — client announces firmware size; server responds
    // with the maximum block payload length it can accept per 0x36 call.
    [[nodiscard]] std::vector<std::uint8_t> HandleRequestDownload(
        const std::vector<std::uint8_t>& req) noexcept;

    // 0x36 TransferData — receive one firmware chunk (max kMaxDataBytesPerBlock).
    [[nodiscard]] std::vector<std::uint8_t> HandleTransferData(
        const std::vector<std::uint8_t>& req) noexcept;

    // 0x37 RequestTransferExit — finalise; validate size and optional CRC-32.
    // On success the staging file is renamed to the validated path.
    [[nodiscard]] std::vector<std::uint8_t> HandleRequestTransferExit(
        const std::vector<std::uint8_t>& req) noexcept;

    // ── State accessors ────────────────────────────────────────────────────────

    [[nodiscard]] bool     IsOtaModeActive() const noexcept;
    [[nodiscard]] OtaState GetState()        const noexcept;

    // ── Protocol constants ─────────────────────────────────────────────────────

    // Maximum firmware-data bytes per 0x36 request (not counting the SID byte
    // or the blockSequenceCounter byte that precede the data).
    static constexpr std::uint16_t kMaxDataBytesPerBlock {512U};

    // maxNumberOfBlockLength = data + SID byte (0x36) + blockSequenceCounter.
    static constexpr std::uint16_t kMaxBlockLength {kMaxDataBytesPerBlock + 2U};

    // Maximum firmware image size accepted (10 MiB — well above any realistic
    // BCL image; guards against integer overflow in the size field).
    static constexpr std::uint32_t kMaxFirmwareSize {10U * 1024U * 1024U};

private:
    OtaState      state_          {OtaState::kIdle};
    std::uint32_t expected_size_  {0U};
    std::uint32_t received_size_  {0U};
    std::uint8_t  next_block_seq_ {0x01U};
    std::uint32_t running_crc_    {0xFFFF'FFFFU};
    int           staging_fd_     {-1};

    static constexpr const char* kStagingPath  {"/tmp/ota_firmware_staging.bin"};
    static constexpr const char* kValidatedPath{"/tmp/ota_firmware_validated.bin"};

    [[nodiscard]] static std::vector<std::uint8_t> NegativeResponse(
        std::uint8_t sid, std::uint8_t nrc) noexcept;

    // Incremental CRC-32/ISO-HDLC (poly 0xEDB88320, init/xorout 0xFFFFFFFF).
    [[nodiscard]] static std::uint32_t Crc32Update(
        std::uint32_t crc,
        const std::uint8_t* data,
        std::size_t len) noexcept;

    void CloseStaging() noexcept;
    void Reset() noexcept;
};

}  // namespace application
}  // namespace lighting
}  // namespace body_control
