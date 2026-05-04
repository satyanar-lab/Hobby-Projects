// OtaSessionManager — STM32 bare-metal stub.
//
// POSIX file I/O (<fcntl.h>, <unistd.h>) is not available on bare-metal.
// All three OTA transfer services return NRC 0x22 (conditionsNotCorrect).
// The protocol flow is fully functional on the Linux simulator; the production
// integration point for STM32 is documented in app/stm32_nucleo_h753zi/main.cpp.

#include "body_control/lighting/application/ota_session_manager.hpp"
#include "body_control/lighting/domain/uds_service_ids.hpp"

namespace body_control
{
namespace lighting
{
namespace application
{

OtaSessionManager::~OtaSessionManager() noexcept = default;

std::vector<std::uint8_t> OtaSessionManager::HandleRequestDownload(
    const std::vector<std::uint8_t>& /*req*/) noexcept
{
    return NegativeResponse(domain::uds::kSidRequestDownload,
                            domain::uds::kNrcConditionsNotCorrect);
}

std::vector<std::uint8_t> OtaSessionManager::HandleTransferData(
    const std::vector<std::uint8_t>& /*req*/) noexcept
{
    return NegativeResponse(domain::uds::kSidTransferData,
                            domain::uds::kNrcConditionsNotCorrect);
}

std::vector<std::uint8_t> OtaSessionManager::HandleRequestTransferExit(
    const std::vector<std::uint8_t>& /*req*/) noexcept
{
    return NegativeResponse(domain::uds::kSidRequestTransferExit,
                            domain::uds::kNrcConditionsNotCorrect);
}

bool OtaSessionManager::IsOtaModeActive() const noexcept
{
    return false;
}

OtaSessionManager::OtaState OtaSessionManager::GetState() const noexcept
{
    return OtaState::kIdle;
}

std::vector<std::uint8_t> OtaSessionManager::NegativeResponse(
    const std::uint8_t sid,
    const std::uint8_t nrc) noexcept
{
    return {domain::uds::kSidNegativeResponse, sid, nrc};
}

void OtaSessionManager::CloseStaging() noexcept {}

void OtaSessionManager::Reset() noexcept
{
    state_ = OtaState::kIdle;
}

std::uint32_t OtaSessionManager::Crc32Update(
    const std::uint32_t  crc,
    const std::uint8_t*  /*data*/,
    const std::size_t    /*len*/) noexcept
{
    return crc;
}

}  // namespace application
}  // namespace lighting
}  // namespace body_control
