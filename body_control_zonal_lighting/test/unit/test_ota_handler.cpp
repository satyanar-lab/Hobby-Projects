#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "body_control/lighting/application/ota_session_manager.hpp"
#include "body_control/lighting/domain/uds_service_ids.hpp"

using body_control::lighting::application::OtaSessionManager;
using body_control::lighting::domain::uds::kSidNegativeResponse;
using body_control::lighting::domain::uds::kNrcConditionsNotCorrect;
using body_control::lighting::domain::uds::kNrcRequestOutOfRange;
using body_control::lighting::domain::uds::kNrcUploadDownloadNotAccepted;
using body_control::lighting::domain::uds::kNrcWrongBlockSequenceCounter;
using body_control::lighting::domain::uds::kNrcGeneralProgrammingFailure;

// ── Helpers ───────────────────────────────────────────────────────────────────

namespace
{

// Build a well-formed 0x34 request for the given firmware size.
std::vector<std::uint8_t> MakeRequestDownload(const std::uint32_t size)
{
    return {
        0x34U, 0x00U, 0x44U,
        0x00U, 0x00U, 0x00U, 0x00U,                                   // address
        static_cast<std::uint8_t>(size >> 24U),
        static_cast<std::uint8_t>((size >> 16U) & 0xFFU),
        static_cast<std::uint8_t>((size >>  8U) & 0xFFU),
        static_cast<std::uint8_t>(size & 0xFFU)
    };
}

// CRC-32/ISO-HDLC matching OtaSessionManager's implementation.
std::uint32_t Crc32(const std::vector<std::uint8_t>& data)
{
    std::uint32_t crc {0xFFFF'FFFFU};
    for (const std::uint8_t byte : data)
    {
        crc ^= static_cast<std::uint32_t>(byte);
        for (int bit = 0; bit < 8; ++bit)
        {
            crc = ((crc & 1U) != 0U)
                ? ((crc >> 1U) ^ 0xEDB8'8320U)
                : (crc >> 1U);
        }
    }
    return crc ^ 0xFFFF'FFFFU;
}

// Build a 0x37 request that includes a 4-byte CRC32 of the given data.
std::vector<std::uint8_t> MakeRequestTransferExitWithCrc(
    const std::vector<std::uint8_t>& firmware_data)
{
    const std::uint32_t crc = Crc32(firmware_data);
    return {
        0x37U,
        static_cast<std::uint8_t>(crc >> 24U),
        static_cast<std::uint8_t>((crc >> 16U) & 0xFFU),
        static_cast<std::uint8_t>((crc >>  8U) & 0xFFU),
        static_cast<std::uint8_t>(crc & 0xFFU)
    };
}

}  // namespace

// ── Fixture ───────────────────────────────────────────────────────────────────

class OtaHandlerTest : public ::testing::Test
{
protected:
    OtaSessionManager ota {};

    static void ExpectNegativeResponse(
        const std::vector<std::uint8_t>& resp,
        const std::uint8_t               request_sid,
        const std::uint8_t               nrc)
    {
        ASSERT_EQ(resp.size(), 3U);
        EXPECT_EQ(resp[0], kSidNegativeResponse);
        EXPECT_EQ(resp[1], request_sid);
        EXPECT_EQ(resp[2], nrc);
    }
};

// ── 0x34 RequestDownload ──────────────────────────────────────────────────────

TEST_F(OtaHandlerTest, RequestDownload_ValidRequest_ReturnsMaxBlockLength)
{
    const auto resp = ota.HandleRequestDownload(MakeRequestDownload(1024U));

    ASSERT_EQ(resp.size(), 4U);
    EXPECT_EQ(resp[0], 0x74U) << "positive response 0x34+0x40=0x74";
    EXPECT_EQ(resp[1], 0x20U) << "lengthFormatIdentifier: 2-byte field";
    // maxNumberOfBlockLength = 512 + 2 = 514 = 0x0202
    EXPECT_EQ(resp[2], 0x02U);
    EXPECT_EQ(resp[3], 0x02U);
}

TEST_F(OtaHandlerTest, RequestDownload_ShortRequest_ReturnsNrc)
{
    const std::vector<std::uint8_t> req {0x34U, 0x00U};
    ExpectNegativeResponse(
        ota.HandleRequestDownload(req), 0x34U, kNrcRequestOutOfRange);
}

TEST_F(OtaHandlerTest, RequestDownload_ZeroSize_ReturnsNrc)
{
    ExpectNegativeResponse(
        ota.HandleRequestDownload(MakeRequestDownload(0U)),
        0x34U, kNrcRequestOutOfRange);
}

TEST_F(OtaHandlerTest, RequestDownload_WhileActive_ReturnsUploadDownloadNotAccepted)
{
    static_cast<void>(ota.HandleRequestDownload(MakeRequestDownload(4U)));
    ASSERT_EQ(ota.GetState(), OtaSessionManager::OtaState::kActive);

    ExpectNegativeResponse(
        ota.HandleRequestDownload(MakeRequestDownload(4U)),
        0x34U, kNrcUploadDownloadNotAccepted);
}

// ── 0x36 TransferData ─────────────────────────────────────────────────────────

TEST_F(OtaHandlerTest, TransferData_OutsideSession_ReturnsNrc)
{
    const std::vector<std::uint8_t> req {0x36U, 0x01U, 0xAAU};
    ExpectNegativeResponse(
        ota.HandleTransferData(req), 0x36U, kNrcConditionsNotCorrect);
}

TEST_F(OtaHandlerTest, TransferData_FirstBlock_AcceptedAndEchoed)
{
    static_cast<void>(ota.HandleRequestDownload(MakeRequestDownload(4U)));

    const std::vector<std::uint8_t> req {0x36U, 0x01U, 0xAAU, 0xBBU, 0xCCU, 0xDDU};
    const auto resp = ota.HandleTransferData(req);

    ASSERT_EQ(resp.size(), 2U);
    EXPECT_EQ(resp[0], 0x76U) << "positive response 0x36+0x40=0x76";
    EXPECT_EQ(resp[1], 0x01U) << "blockSequenceCounter echoed";
}

TEST_F(OtaHandlerTest, TransferData_OutOfSequence_ReturnsNrc)
{
    static_cast<void>(ota.HandleRequestDownload(MakeRequestDownload(8U)));
    // Send block 0x01.
    static_cast<void>(ota.HandleTransferData(
        {0x36U, 0x01U, 0x00U, 0x01U, 0x02U, 0x03U}));
    // Skip to block 0x03 — should fail.
    ExpectNegativeResponse(
        ota.HandleTransferData({0x36U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U}),
        0x36U, kNrcWrongBlockSequenceCounter);
}

TEST_F(OtaHandlerTest, IsOtaModeActive_DuringTransfer_ReturnsTrue)
{
    static_cast<void>(ota.HandleRequestDownload(MakeRequestDownload(4U)));
    EXPECT_TRUE(ota.IsOtaModeActive());
}

// ── 0x37 RequestTransferExit ──────────────────────────────────────────────────

TEST_F(OtaHandlerTest, RequestTransferExit_OutsideSession_ReturnsNrc)
{
    const std::vector<std::uint8_t> req {0x37U};
    ExpectNegativeResponse(
        ota.HandleRequestTransferExit(req), 0x37U, kNrcConditionsNotCorrect);
}

TEST_F(OtaHandlerTest, RequestTransferExit_SizeMismatch_ReturnsNrc)
{
    static_cast<void>(ota.HandleRequestDownload(MakeRequestDownload(8U)));
    // Send only 4 bytes but announced 8.
    static_cast<void>(ota.HandleTransferData(
        {0x36U, 0x01U, 0xAAU, 0xBBU, 0xCCU, 0xDDU}));

    ExpectNegativeResponse(
        ota.HandleRequestTransferExit({0x37U}),
        0x37U, kNrcConditionsNotCorrect);
}

TEST_F(OtaHandlerTest, RequestTransferExit_CrcMismatch_ReturnsNrc)
{
    const std::vector<std::uint8_t> firmware {0xAAU, 0xBBU, 0xCCU, 0xDDU};
    static_cast<void>(ota.HandleRequestDownload(MakeRequestDownload(4U)));
    static_cast<void>(ota.HandleTransferData(
        {0x36U, 0x01U, 0xAAU, 0xBBU, 0xCCU, 0xDDU}));

    // Send an intentionally wrong CRC.
    ExpectNegativeResponse(
        ota.HandleRequestTransferExit({0x37U, 0x00U, 0x00U, 0x00U, 0x00U}),
        0x37U, kNrcGeneralProgrammingFailure);
}

TEST_F(OtaHandlerTest, FullTransfer_NoCrc_SuccessAndBecomesIdle)
{
    const std::vector<std::uint8_t> firmware {0x01U, 0x02U, 0x03U, 0x04U};
    static_cast<void>(ota.HandleRequestDownload(MakeRequestDownload(4U)));
    static_cast<void>(ota.HandleTransferData(
        {0x36U, 0x01U, 0x01U, 0x02U, 0x03U, 0x04U}));

    const auto resp = ota.HandleRequestTransferExit({0x37U});

    ASSERT_EQ(resp.size(), 1U);
    EXPECT_EQ(resp[0], 0x77U) << "positive response 0x37+0x40=0x77";
    EXPECT_EQ(ota.GetState(), OtaSessionManager::OtaState::kComplete);
    EXPECT_FALSE(ota.IsOtaModeActive());
}

TEST_F(OtaHandlerTest, FullTransfer_WithCrc_SuccessAndBecomesIdle)
{
    const std::vector<std::uint8_t> firmware {0xDEU, 0xADU, 0xBEU, 0xEFU};
    static_cast<void>(ota.HandleRequestDownload(MakeRequestDownload(4U)));
    static_cast<void>(ota.HandleTransferData(
        {0x36U, 0x01U, 0xDEU, 0xADU, 0xBEU, 0xEFU}));

    const auto resp = ota.HandleRequestTransferExit(
        MakeRequestTransferExitWithCrc(firmware));

    ASSERT_EQ(resp.size(), 1U);
    EXPECT_EQ(resp[0], 0x77U);
    EXPECT_FALSE(ota.IsOtaModeActive());
}

TEST_F(OtaHandlerTest, IsOtaModeActive_AfterComplete_ReturnsFalse)
{
    static_cast<void>(ota.HandleRequestDownload(MakeRequestDownload(1U)));
    static_cast<void>(ota.HandleTransferData({0x36U, 0x01U, 0xFFU}));
    static_cast<void>(ota.HandleRequestTransferExit({0x37U}));

    EXPECT_FALSE(ota.IsOtaModeActive());
    EXPECT_EQ(ota.GetState(), OtaSessionManager::OtaState::kComplete);
}

TEST_F(OtaHandlerTest, MultipleBlocks_SequenceWrapsCorrectly)
{
    // 3 blocks of 1 byte each.
    static_cast<void>(ota.HandleRequestDownload(MakeRequestDownload(3U)));
    EXPECT_EQ(ota.HandleTransferData({0x36U, 0x01U, 0xAAU})[1], 0x01U);
    EXPECT_EQ(ota.HandleTransferData({0x36U, 0x02U, 0xBBU})[1], 0x02U);
    EXPECT_EQ(ota.HandleTransferData({0x36U, 0x03U, 0xCCU})[1], 0x03U);

    const auto resp = ota.HandleRequestTransferExit({0x37U});
    EXPECT_EQ(resp[0], 0x77U);
}
