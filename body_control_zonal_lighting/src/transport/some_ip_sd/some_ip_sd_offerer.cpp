// SOME/IP-SD OfferService sender — Linux POSIX implementation.
//
// Opens a UDP socket and periodically sends OfferService frames to the
// SOME/IP-SD well-known multicast address 224.244.224.245 port 30490.
//
// IP_MULTICAST_LOOP is enabled so the sender and a collocated listener (e.g.
// running CZC and simulator on the same host) both receive the offer.

#include "body_control/lighting/transport/some_ip_sd_offerer.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <thread>

#include "body_control/lighting/transport/some_ip_sd_codec.hpp"

namespace body_control
{
namespace lighting
{
namespace transport
{

namespace
{
constexpr const char*   kSdMcastAddr {"224.244.224.245"};
constexpr std::uint16_t kSdPort      {30490U};
}  // namespace

SomeIpSdOfferer::~SomeIpSdOfferer() noexcept
{
    static_cast<void>(Stop());
    if (socket_fd_ >= 0)
    {
        ::close(socket_fd_);
        socket_fd_ = -1;
    }
}

SdStatus SomeIpSdOfferer::Init(const ServiceOffer& offer) noexcept
{
    offer_ = offer;

    socket_fd_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_fd_ < 0) { return SdStatus::kSocketError; }

    // TTL = 1: stays on the local subnet; sufficient for in-vehicle ECU comms.
    const int ttl = 1;
    static_cast<void>(::setsockopt(socket_fd_, IPPROTO_IP,
                                   IP_MULTICAST_TTL, &ttl, sizeof(ttl)));

    // Enable loopback so a collocated listener on the same host sees the offer.
    const int loop = 1;
    static_cast<void>(::setsockopt(socket_fd_, IPPROTO_IP,
                                   IP_MULTICAST_LOOP, &loop, sizeof(loop)));

    initialized_ = true;
    return SdStatus::kSuccess;
}

SdStatus SomeIpSdOfferer::Start() noexcept
{
    if (!initialized_) { return SdStatus::kNotInitialized; }
    running_.store(true);
    thread_ = std::thread(&SomeIpSdOfferer::OfferLoop, this);
    return SdStatus::kSuccess;
}

SdStatus SomeIpSdOfferer::Stop() noexcept
{
    running_.store(false);
    if (thread_.joinable()) { thread_.join(); }
    return SdStatus::kSuccess;
}

void SomeIpSdOfferer::OfferLoop() noexcept
{
    std::uint16_t session_id {1U};

    sockaddr_in mcast {};
    mcast.sin_family = AF_INET;
    mcast.sin_port   = htons(kSdPort);
    static_cast<void>(::inet_pton(AF_INET, kSdMcastAddr, &mcast.sin_addr));

    const std::chrono::milliseconds period {
        static_cast<long>(offer_.period_ms)};

    while (running_.load())
    {
        const std::vector<std::uint8_t> frame =
            SomeIpSdCodec::EncodeOffer(offer_, session_id++);

        static_cast<void>(::sendto(
            socket_fd_,
            frame.data(),
            frame.size(),
            0,
            reinterpret_cast<const sockaddr*>(&mcast),
            static_cast<socklen_t>(sizeof(mcast))));

        std::this_thread::sleep_for(period);
    }
}

}  // namespace transport
}  // namespace lighting
}  // namespace body_control
