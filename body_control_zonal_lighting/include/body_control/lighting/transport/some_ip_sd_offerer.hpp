#ifndef BODY_CONTROL_LIGHTING_TRANSPORT_SOME_IP_SD_OFFERER_HPP
#define BODY_CONTROL_LIGHTING_TRANSPORT_SOME_IP_SD_OFFERER_HPP

#include <atomic>
#include <thread>

#include "body_control/lighting/transport/some_ip_sd_types.hpp"

namespace body_control
{
namespace lighting
{
namespace transport
{

// Periodically broadcasts SOME/IP-SD OfferService frames to the well-known
// multicast group 224.244.224.245:30490 at the interval set in ServiceOffer::period_ms.
//
// Threading: OfferLoop() runs on its own std::thread; Init/Start/Stop are
// called from the application thread.  The socket is owned by this class.
//
// Platform: Linux only.  Zephyr uses an inline k_work_delayable in main.cpp.
class SomeIpSdOfferer
{
public:
    SomeIpSdOfferer() noexcept = default;
    ~SomeIpSdOfferer() noexcept;

    [[nodiscard]] SdStatus Init(const ServiceOffer& offer) noexcept;
    [[nodiscard]] SdStatus Start() noexcept;
    [[nodiscard]] SdStatus Stop() noexcept;

private:
    void OfferLoop() noexcept;

    ServiceOffer      offer_        {};
    int               socket_fd_    {-1};
    std::atomic<bool> running_      {false};
    std::thread       thread_       {};
    bool              initialized_  {false};
};

}  // namespace transport
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_TRANSPORT_SOME_IP_SD_OFFERER_HPP
