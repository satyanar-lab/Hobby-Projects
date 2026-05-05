// SOME/IP-SD multicast listener — Linux POSIX implementation.
//
// Joins multicast group 224.244.224.245 on port 30490, then receives SD frames
// and fires a callback each time an OfferService arrives for the configured
// service/instance pair.
//
// A FindService is sent once at Start() so any provider already in its main
// phase answers immediately instead of waiting for its next interval.
//
// The receive socket has a 1-second SO_RCVTIMEO so the listen loop can check
// the running_ flag without blocking indefinitely.

#include "body_control/lighting/transport/some_ip_sd_listener.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <array>
#include <cstring>

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
constexpr std::size_t   kRxBufSize   {1500U};
}  // namespace

SomeIpSdListener::~SomeIpSdListener() noexcept
{
    static_cast<void>(Stop());
    if (recv_fd_ >= 0) { ::close(recv_fd_); recv_fd_ = -1; }
    if (send_fd_ >= 0) { ::close(send_fd_); send_fd_ = -1; }
}

SdStatus SomeIpSdListener::Init(
    const std::uint16_t service_id,
    const std::uint16_t instance_id,
    OfferCallback        callback) noexcept
{
    wanted_service_id_  = service_id;
    wanted_instance_id_ = instance_id;
    callback_           = std::move(callback);

    // Receive socket: multicast, bound to 0.0.0.0:30490.
    recv_fd_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (recv_fd_ < 0) { return SdStatus::kSocketError; }

    const int reuse = 1;
    static_cast<void>(::setsockopt(recv_fd_, SOL_SOCKET,
                                   SO_REUSEADDR, &reuse, sizeof(reuse)));

    sockaddr_in bind_addr {};
    bind_addr.sin_family      = AF_INET;
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind_addr.sin_port        = htons(kSdPort);
    if (::bind(recv_fd_,
               reinterpret_cast<const sockaddr*>(&bind_addr),
               static_cast<socklen_t>(sizeof(bind_addr))) < 0)
    {
        ::close(recv_fd_);
        recv_fd_ = -1;
        return SdStatus::kSocketError;
    }

    // Join the SOME/IP-SD multicast group.
    ip_mreq mreq {};
    static_cast<void>(::inet_pton(AF_INET, kSdMcastAddr, &mreq.imr_multiaddr));
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    static_cast<void>(::setsockopt(recv_fd_, IPPROTO_IP,
                                   IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)));

    // 1-second receive timeout so the loop checks running_ every second.
    timeval tv {};
    tv.tv_sec  = 1;
    tv.tv_usec = 0;
    static_cast<void>(::setsockopt(recv_fd_, SOL_SOCKET,
                                   SO_RCVTIMEO, &tv, sizeof(tv)));

    // Send socket for FindService requests (share the multicast loopback setup).
    send_fd_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (send_fd_ >= 0)
    {
        const int ttl  = 1;
        const int loop = 1;
        static_cast<void>(::setsockopt(send_fd_, IPPROTO_IP,
                                       IP_MULTICAST_TTL,  &ttl,  sizeof(ttl)));
        static_cast<void>(::setsockopt(send_fd_, IPPROTO_IP,
                                       IP_MULTICAST_LOOP, &loop, sizeof(loop)));
    }

    initialized_ = true;
    return SdStatus::kSuccess;
}

SdStatus SomeIpSdListener::Start() noexcept
{
    if (!initialized_) { return SdStatus::kNotInitialized; }
    running_.store(true);
    SendFindService();
    thread_ = std::thread(&SomeIpSdListener::ListenLoop, this);
    return SdStatus::kSuccess;
}

SdStatus SomeIpSdListener::Stop() noexcept
{
    running_.store(false);
    if (thread_.joinable()) { thread_.join(); }
    return SdStatus::kSuccess;
}

void SomeIpSdListener::SendFindService() noexcept
{
    if (send_fd_ < 0) { return; }

    const std::vector<std::uint8_t> frame =
        SomeIpSdCodec::EncodeFind(
            wanted_service_id_, wanted_instance_id_, 0x0001U);

    sockaddr_in mcast {};
    mcast.sin_family = AF_INET;
    mcast.sin_port   = htons(kSdPort);
    static_cast<void>(::inet_pton(AF_INET, kSdMcastAddr, &mcast.sin_addr));

    static_cast<void>(::sendto(
        send_fd_,
        frame.data(),
        frame.size(),
        0,
        reinterpret_cast<const sockaddr*>(&mcast),
        static_cast<socklen_t>(sizeof(mcast))));
}

void SomeIpSdListener::ListenLoop() noexcept
{
    std::array<std::uint8_t, kRxBufSize> buf {};

    while (running_.load())
    {
        const ssize_t received =
            ::recv(recv_fd_, buf.data(), buf.size(), 0);

        if (received <= 0)
        {
            // recv() returns -1 on timeout (EAGAIN) — check running_ and continue.
            continue;
        }

        DiscoveredService svc {};
        if (SomeIpSdCodec::DecodeOffer(
                buf.data(),
                static_cast<std::size_t>(received),
                wanted_service_id_,
                wanted_instance_id_,
                svc))
        {
            if (callback_) { callback_(svc); }
        }
    }
}

}  // namespace transport
}  // namespace lighting
}  // namespace body_control
