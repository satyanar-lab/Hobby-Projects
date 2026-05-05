#ifndef BODY_CONTROL_LIGHTING_TRANSPORT_SOME_IP_SD_LISTENER_HPP
#define BODY_CONTROL_LIGHTING_TRANSPORT_SOME_IP_SD_LISTENER_HPP

#include <atomic>
#include <functional>
#include <thread>

#include "body_control/lighting/transport/some_ip_sd_types.hpp"

namespace body_control
{
namespace lighting
{
namespace transport
{

// Listens for SOME/IP-SD OfferService frames on multicast 224.244.224.245:30490.
// Calls the registered callback when an offer arrives for the configured service.
//
// Also sends an initial FindService at startup so providers answer immediately
// rather than waiting for their next main-phase interval.
//
// Threading: ListenLoop() runs on its own std::thread (separate from any data
// transport thread).  The callback is invoked on the listener thread — callers
// must not block in the callback.
//
// Platform: Linux only.
class SomeIpSdListener
{
public:
    using OfferCallback = std::function<void(const DiscoveredService&)>;

    SomeIpSdListener() noexcept = default;
    ~SomeIpSdListener() noexcept;

    [[nodiscard]] SdStatus Init(
        std::uint16_t  service_id,
        std::uint16_t  instance_id,
        OfferCallback  callback) noexcept;

    [[nodiscard]] SdStatus Start() noexcept;
    [[nodiscard]] SdStatus Stop() noexcept;

private:
    void ListenLoop() noexcept;
    void SendFindService() noexcept;

    std::uint16_t     wanted_service_id_  {0U};
    std::uint16_t     wanted_instance_id_ {0U};
    OfferCallback     callback_           {};
    int               recv_fd_            {-1};
    int               send_fd_            {-1};
    std::atomic<bool> running_            {false};
    std::thread       thread_             {};
    bool              initialized_        {false};
};

}  // namespace transport
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_TRANSPORT_SOME_IP_SD_LISTENER_HPP
