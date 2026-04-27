#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <utility>
#include <vector>

#include <vsomeip/vsomeip.hpp>

#include "body_control/lighting/domain/lighting_service_ids.hpp"
#include "body_control/lighting/transport/transport_adapter_interface.hpp"
#include "body_control/lighting/transport/transport_status.hpp"

namespace body_control
{
namespace lighting
{
namespace transport
{
namespace vsomeip
{

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace
{

// Maps a vsomeip message to the transport-layer neutral TransportMessage struct.
// MT_NOTIFICATION is the vsomeip message type for pushed events; all other
// message types (requests, responses) are treated as non-event messages.
// is_reliable is always true because vsomeip routes over TCP by default for
// this service configuration.
TransportMessage BuildTransportMessage(
    const std::shared_ptr<::vsomeip::message>& msg)
{
    TransportMessage tm{};
    tm.service_id         = msg->get_service();
    tm.instance_id        = msg->get_instance();
    tm.method_or_event_id = msg->get_method();
    tm.client_id          = msg->get_client();
    tm.session_id         = msg->get_session();
    tm.is_event =
        (msg->get_message_type() == ::vsomeip::message_type_e::MT_NOTIFICATION);
    tm.is_reliable = true;

    const auto pl = msg->get_payload();
    if (pl != nullptr)
    {
        const auto* data   = pl->get_data();
        const auto  length = pl->get_length();
        tm.payload.assign(data, data + length);
    }
    return tm;
}

} // namespace

// ---------------------------------------------------------------------------
// VsomeipServerAdapterImpl
//
// Offers one SOME/IP service, handles incoming method calls, and sends
// responses and event notifications.
// ---------------------------------------------------------------------------

class VsomeipServerAdapterImpl final : public TransportAdapterInterface
{
public:
    struct EventConfig
    {
        ::vsomeip::event_t      event_id;
        ::vsomeip::eventgroup_t eventgroup_id;
    };

    VsomeipServerAdapterImpl(
        std::string                      app_name,
        ::vsomeip::service_t             service_id,
        ::vsomeip::instance_t            instance_id,
        std::vector<::vsomeip::method_t> method_ids,
        std::vector<EventConfig>         events)
        : app_name_(std::move(app_name))
        , service_id_(service_id)
        , instance_id_(instance_id)
        , method_ids_(std::move(method_ids))
        , events_(std::move(events))
    {}

    ~VsomeipServerAdapterImpl() override = default;

    VsomeipServerAdapterImpl(const VsomeipServerAdapterImpl&)            = delete;
    VsomeipServerAdapterImpl& operator=(const VsomeipServerAdapterImpl&) = delete;
    VsomeipServerAdapterImpl(VsomeipServerAdapterImpl&&)                 = delete;
    VsomeipServerAdapterImpl& operator=(VsomeipServerAdapterImpl&&)      = delete;

    [[nodiscard]] TransportStatus Initialize() override
    {
        runtime_ = ::vsomeip::runtime::get();
        app_     = runtime_->create_application(app_name_);
        if (!app_->init())
        {
            return TransportStatus::kNotInitialized;
        }

        for (const auto method_id : method_ids_)
        {
            app_->register_message_handler(
                service_id_, instance_id_, method_id,
                [this](const std::shared_ptr<::vsomeip::message>& msg)
                {
                    OnRequestReceived(msg);
                });
        }

        for (const auto& ev : events_)
        {
            app_->offer_event(
                service_id_, instance_id_, ev.event_id,
                std::set<::vsomeip::eventgroup_t>{ev.eventgroup_id},
                ::vsomeip::event_type_e::ET_EVENT);
        }

        app_->offer_service(service_id_, instance_id_);

        // app_->start() blocks indefinitely running the vsomeip event loop.
        // It must run on a dedicated thread so Initialize() can return to the caller.
        dispatch_thread_ = std::thread([this]() { app_->start(); });
        return TransportStatus::kSuccess;
    }

    [[nodiscard]] TransportStatus Shutdown() override
    {
        if (app_ != nullptr)
        {
            app_->stop_offer_service(service_id_, instance_id_);
            app_->stop();
        }
        if (dispatch_thread_.joinable())
        {
            dispatch_thread_.join();
        }
        return TransportStatus::kSuccess;
    }

    // A server adapter never initiates requests; only clients send them.
    [[nodiscard]] TransportStatus SendRequest(
        const TransportMessage& /*transport_message*/) override
    {
        return TransportStatus::kInvalidArgument;
    }

    [[nodiscard]] TransportStatus SendResponse(
        const TransportMessage& transport_message) override
    {
        if (app_ == nullptr)
        {
            return TransportStatus::kNotInitialized;
        }

        // The original request is keyed by (client_id, session_id) so vsomeip
        // can route the response back to the correct client process and correlate
        // it to the originating request on the client side.
        std::shared_ptr<::vsomeip::message> original_request;
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            const auto key = std::make_pair(
                transport_message.client_id, transport_message.session_id);
            auto it = pending_requests_.find(key);
            if (it == pending_requests_.end())
            {
                return TransportStatus::kInvalidArgument;
            }
            original_request = it->second;
            pending_requests_.erase(it);
        }

        auto response = runtime_->create_response(original_request);
        auto payload  = runtime_->create_payload();
        payload->set_data(transport_message.payload);
        response->set_payload(payload);
        app_->send(response);
        return TransportStatus::kSuccess;
    }

    [[nodiscard]] TransportStatus SendEvent(
        const TransportMessage& transport_message) override
    {
        if (app_ == nullptr)
        {
            return TransportStatus::kNotInitialized;
        }
        auto payload = runtime_->create_payload();
        payload->set_data(transport_message.payload);
        app_->notify(
            service_id_, instance_id_,
            transport_message.method_or_event_id, payload);
        return TransportStatus::kSuccess;
    }

    void SetMessageHandler(
        TransportMessageHandlerInterface* message_handler) noexcept override
    {
        message_handler_.store(message_handler);
    }

private:
    void OnRequestReceived(const std::shared_ptr<::vsomeip::message>& msg)
    {
        // Store the original vsomeip message so SendResponse() can call
        // runtime_->create_response() on it later.  The map entry is erased
        // in SendResponse() once the response is sent.
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_requests_[{msg->get_client(), msg->get_session()}] = msg;
        }
        auto* handler = message_handler_.load();
        if (handler != nullptr)
        {
            handler->OnTransportMessageReceived(BuildTransportMessage(msg));
        }
    }

    std::string                      app_name_;
    ::vsomeip::service_t             service_id_;
    ::vsomeip::instance_t            instance_id_;
    std::vector<::vsomeip::method_t> method_ids_;
    std::vector<EventConfig>         events_;

    std::shared_ptr<::vsomeip::runtime>     runtime_{};
    std::shared_ptr<::vsomeip::application> app_{};
    std::thread                             dispatch_thread_{};

    std::atomic<TransportMessageHandlerInterface*> message_handler_{nullptr};

    std::mutex pending_mutex_{};
    std::map<std::pair<::vsomeip::client_t, ::vsomeip::session_t>,
             std::shared_ptr<::vsomeip::message>>
        pending_requests_{};
};

// ---------------------------------------------------------------------------
// VsomeipClientAdapterImpl
//
// Consumes one SOME/IP service, sends method requests, and receives
// responses and event notifications.
// ---------------------------------------------------------------------------

class VsomeipClientAdapterImpl final : public TransportAdapterInterface
{
public:
    struct EventConfig
    {
        ::vsomeip::event_t      event_id;
        ::vsomeip::eventgroup_t eventgroup_id;
    };

    VsomeipClientAdapterImpl(
        std::string              app_name,
        ::vsomeip::service_t     service_id,
        ::vsomeip::instance_t    instance_id,
        std::vector<EventConfig> events_to_subscribe)
        : app_name_(std::move(app_name))
        , service_id_(service_id)
        , instance_id_(instance_id)
        , events_(std::move(events_to_subscribe))
    {}

    ~VsomeipClientAdapterImpl() override = default;

    VsomeipClientAdapterImpl(const VsomeipClientAdapterImpl&)            = delete;
    VsomeipClientAdapterImpl& operator=(const VsomeipClientAdapterImpl&) = delete;
    VsomeipClientAdapterImpl(VsomeipClientAdapterImpl&&)                 = delete;
    VsomeipClientAdapterImpl& operator=(VsomeipClientAdapterImpl&&)      = delete;

    [[nodiscard]] TransportStatus Initialize() override
    {
        runtime_ = ::vsomeip::runtime::get();
        app_     = runtime_->create_application(app_name_);
        if (!app_->init())
        {
            return TransportStatus::kNotInitialized;
        }

        app_->register_availability_handler(
            service_id_, instance_id_,
            [this](::vsomeip::service_t  /*svc*/,
                   ::vsomeip::instance_t /*inst*/,
                   bool                   available)
            {
                auto* handler = message_handler_.load();
                if (handler != nullptr)
                {
                    handler->OnTransportAvailabilityChanged(available);
                }
            });

        for (const auto& ev : events_)
        {
            app_->request_event(
                service_id_, instance_id_, ev.event_id,
                std::set<::vsomeip::eventgroup_t>{ev.eventgroup_id},
                ::vsomeip::event_type_e::ET_EVENT);
        }

        app_->register_message_handler(
            service_id_, instance_id_, ::vsomeip::ANY_METHOD,
            [this](const std::shared_ptr<::vsomeip::message>& msg)
            {
                auto* handler = message_handler_.load();
                if (handler != nullptr)
                {
                    handler->OnTransportMessageReceived(
                        BuildTransportMessage(msg));
                }
            });

        app_->request_service(service_id_, instance_id_);

        for (const auto& ev : events_)
        {
            app_->subscribe(service_id_, instance_id_, ev.eventgroup_id);
        }

        // Same blocking-start pattern as VsomeipServerAdapterImpl: the event
        // loop must run on a separate thread so Initialize() returns promptly.
        dispatch_thread_ = std::thread([this]() { app_->start(); });
        return TransportStatus::kSuccess;
    }

    [[nodiscard]] TransportStatus Shutdown() override
    {
        if (app_ != nullptr)
        {
            app_->release_service(service_id_, instance_id_);
            app_->stop();
        }
        if (dispatch_thread_.joinable())
        {
            dispatch_thread_.join();
        }
        return TransportStatus::kSuccess;
    }

    [[nodiscard]] TransportStatus SendRequest(
        const TransportMessage& transport_message) override
    {
        if (app_ == nullptr)
        {
            return TransportStatus::kNotInitialized;
        }
        auto request = runtime_->create_request(transport_message.is_reliable);
        request->set_service(service_id_);
        request->set_instance(instance_id_);
        request->set_method(transport_message.method_or_event_id);
        auto payload = runtime_->create_payload();
        payload->set_data(transport_message.payload);
        request->set_payload(payload);
        app_->send(request);
        return TransportStatus::kSuccess;
    }

    // A client adapter never sends responses; only servers reply to requests.
    [[nodiscard]] TransportStatus SendResponse(
        const TransportMessage& /*transport_message*/) override
    {
        return TransportStatus::kInvalidArgument;
    }

    // A client adapter never sends events; only servers publish notifications.
    [[nodiscard]] TransportStatus SendEvent(
        const TransportMessage& /*transport_message*/) override
    {
        return TransportStatus::kInvalidArgument;
    }

    void SetMessageHandler(
        TransportMessageHandlerInterface* message_handler) noexcept override
    {
        message_handler_.store(message_handler);
    }

private:
    std::string              app_name_;
    ::vsomeip::service_t     service_id_;
    ::vsomeip::instance_t    instance_id_;
    std::vector<EventConfig> events_;

    std::shared_ptr<::vsomeip::runtime>     runtime_{};
    std::shared_ptr<::vsomeip::application> app_{};
    std::thread                             dispatch_thread_{};

    std::atomic<TransportMessageHandlerInterface*> message_handler_{nullptr};
};

// ---------------------------------------------------------------------------
// Factory functions
// ---------------------------------------------------------------------------

std::unique_ptr<TransportAdapterInterface>
CreateCentralZoneControllerRuntimeAdapter()
{
    namespace rls = domain::rear_lighting_service;
    using EC      = VsomeipClientAdapterImpl::EventConfig;
    return std::make_unique<VsomeipClientAdapterImpl>(
        "central_zone_controller",
        rls::kServiceId,
        rls::kInstanceId,
        std::vector<EC>{
            {rls::kLampStatusEventId, rls::kLampStatusEventGroupId},
            {rls::kNodeHealthEventId, rls::kNodeHealthEventGroupId}});
}

std::unique_ptr<TransportAdapterInterface>
CreateRearLightingNodeRuntimeAdapter()
{
    namespace rls = domain::rear_lighting_service;
    using EC      = VsomeipServerAdapterImpl::EventConfig;
    return std::make_unique<VsomeipServerAdapterImpl>(
        "rear_lighting_node_simulator",
        rls::kServiceId,
        rls::kInstanceId,
        std::vector<::vsomeip::method_t>{
            rls::kSetLampCommandMethodId,
            rls::kGetLampStatusMethodId,
            rls::kGetNodeHealthMethodId},
        std::vector<EC>{
            {rls::kLampStatusEventId, rls::kLampStatusEventGroupId},
            {rls::kNodeHealthEventId, rls::kNodeHealthEventGroupId}});
}

std::unique_ptr<TransportAdapterInterface>
CreateControllerOperatorRuntimeAdapter()
{
    namespace ops = domain::operator_service;
    using EC      = VsomeipServerAdapterImpl::EventConfig;
    return std::make_unique<VsomeipServerAdapterImpl>(
        "controller_operator",
        ops::kServiceId,
        ops::kInstanceId,
        std::vector<::vsomeip::method_t>{
            ops::kRequestLampToggleMethodId,
            ops::kRequestLampActivateMethodId,
            ops::kRequestLampDeactivateMethodId,
            ops::kRequestNodeHealthMethodId},
        std::vector<EC>{
            {ops::kLampStatusEventId, static_cast<::vsomeip::eventgroup_t>(0x0001U)},
            {ops::kNodeHealthEventId, static_cast<::vsomeip::eventgroup_t>(0x0002U)}});
}

std::unique_ptr<TransportAdapterInterface>
CreateOperatorClientRuntimeAdapter()
{
    namespace ops = domain::operator_service;
    using EC      = VsomeipClientAdapterImpl::EventConfig;
    return std::make_unique<VsomeipClientAdapterImpl>(
        "hmi_control_panel",
        ops::kServiceId,
        ops::kInstanceId,
        std::vector<EC>{
            {ops::kLampStatusEventId, static_cast<::vsomeip::eventgroup_t>(0x0001U)},
            {ops::kNodeHealthEventId, static_cast<::vsomeip::eventgroup_t>(0x0002U)}});
}

}  // namespace vsomeip
}  // namespace transport
}  // namespace lighting
}  // namespace body_control
