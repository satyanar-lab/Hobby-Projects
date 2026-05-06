#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <string_view>

#include "body_control/lighting/domain/fault_types.hpp"
#include "body_control/lighting/domain/lamp_command_types.hpp"
#include "body_control/lighting/domain/lamp_status_types.hpp"
#include "body_control/lighting/domain/lighting_service_ids.hpp"
#include "body_control/lighting/hmi/hmi_display_strings.hpp"
#include "body_control/lighting/service/operator_service_consumer.hpp"
#include "body_control/lighting/service/operator_service_interface.hpp"
#include "body_control/lighting/transport/transport_adapter_interface.hpp"

#ifdef BCL_VSS_INTEGRATION_ENABLED
#include "vss_lamp_overlay.hpp"
#endif

namespace body_control
{
namespace lighting
{
namespace transport
{
namespace vsomeip
{

std::unique_ptr<TransportAdapterInterface>
CreateOperatorClientVsomeipClientAdapter();

}  // namespace vsomeip
}  // namespace transport
}  // namespace lighting
}  // namespace body_control

namespace
{

void PrintHelp()
{
    std::cout <<
        "Usage: diagnostic_console [--help] [--vss-snapshot]\n"
        "\n"
        "Options:\n"
        "  --help, -h      Show this help and exit.\n"
        "  --vss-snapshot  Connect to the running controller, read current lamp\n"
        "                  state and fault summary, and print a VSS-format JSON\n"
        "                  snapshot to stdout (single line, 11 keys), then exit.\n"
        "\n"
        "                  11 keys in the output (alphabetical order):\n"
        "                    5 bool  — Vehicle.Body.Lights.* IsSignaling / IsOn\n"
        "                    6 uint  — Vehicle.Private.BCL.Lighting.* ActiveFaultCode\n"
        "                              (5 per-lamp codes) + CommandSequenceCounter\n"
        "\n"
        "                  Note: per-function DTC codes require the UDS/DoIP path\n"
        "                  (services 0x19/0x22 over port 13400). Via the operator\n"
        "                  service used here, ActiveFaultCode fields are always\n"
        "                  0 — use the UDS python client for per-function DTCs.\n"
        "\n"
        "                  Exit codes:\n"
        "                    0  success, JSON on stdout\n"
        "                    2  VSS integration not built (rebuild with vss-tools)\n"
        "                    3  controller or exterior node unreachable\n"
        "\n"
        "Without options: starts the interactive diagnostic menu.\n";
}

void PrintCachedLampStatus(
    const body_control::lighting::service::OperatorServiceConsumer& consumer,
    const body_control::lighting::domain::LampFunction lamp_function)
{
    using body_control::lighting::hmi::LampFunctionToString;
    using body_control::lighting::hmi::LampOutputStateToString;

    body_control::lighting::domain::LampStatus lamp_status {};

    if (consumer.GetLampStatus(lamp_function, lamp_status))
    {
        std::cout << LampFunctionToString(lamp_status.function)
                  << " -> "
                  << LampOutputStateToString(lamp_status.output_state)
                  << ", applied="
                  << (lamp_status.command_applied ? "true" : "false")
                  << ", seq="
                  << lamp_status.last_sequence_counter
                  << '\n';
    }
}

body_control::lighting::domain::LampFunction SelectLampFunction()
{
    using body_control::lighting::domain::LampFunction;

    std::cout << "  1 -> Left indicator\n";
    std::cout << "  2 -> Right indicator\n";
    std::cout << "  3 -> Hazard lamp\n";
    std::cout << "  4 -> Park lamp\n";
    std::cout << "  5 -> Head lamp\n";
    std::cout << "  Selection: ";

    int sel {0};
    if (!(std::cin >> sel) && !std::cin.eof())
    {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    switch (sel)
    {
    case 1: return LampFunction::kLeftIndicator;
    case 2: return LampFunction::kRightIndicator;
    case 3: return LampFunction::kHazardLamp;
    case 4: return LampFunction::kParkLamp;
    case 5: return LampFunction::kHeadLamp;
    default: return LampFunction::kUnknown;
    }
}

// Prints events as they arrive from the operator service on the vsomeip
// dispatch thread, concurrent with the main input loop.
// Also provides a blocking WaitForController() used at startup so the menu
// is not shown until the controller is reachable.
class DiagnosticEventListener final
    : public body_control::lighting::service::OperatorServiceEventListenerInterface
{
public:
    // Blocks until OnControllerAvailabilityChanged(true) fires or timeout_s
    // elapses.  Returns true if the controller became available in time.
    bool WaitForController(const std::chrono::seconds timeout_s)
    {
        std::unique_lock<std::mutex> lock {mutex_};
        return cv_.wait_for(lock, timeout_s,
                            [this]() { return controller_available_.load(); });
    }

    void OnLampStatusUpdated(
        const body_control::lighting::domain::LampStatus& lamp_status) override
    {
        using body_control::lighting::hmi::LampFunctionToString;
        using body_control::lighting::hmi::LampOutputStateToString;
        std::cerr << "[event] "
                  << LampFunctionToString(lamp_status.function)
                  << " -> "
                  << LampOutputStateToString(lamp_status.output_state)
                  << '\n';
    }

    void OnNodeHealthUpdated(
        const body_control::lighting::domain::NodeHealthStatus&
            node_health_status) override
    {
        using body_control::lighting::hmi::NodeHealthStateToString;
        std::cerr << "[event] node health: "
                  << NodeHealthStateToString(node_health_status.health_state)
                  << ", svc="
                  << (node_health_status.service_available ? "up" : "down")
                  << '\n';
    }

    void OnControllerAvailabilityChanged(const bool is_available) override
    {
        std::cerr << "[event] controller "
                  << (is_available ? "available" : "unavailable")
                  << '\n';
        {
            const std::lock_guard<std::mutex> lock {mutex_};
            controller_available_.store(is_available);
        }
        if (is_available) { cv_.notify_all(); }
    }

private:
    std::mutex              mutex_ {};
    std::condition_variable cv_ {};
    std::atomic<bool>       controller_available_ {false};
};

// ─── VSS snapshot support ────────────────────────────────────────────────────

#ifdef BCL_VSS_INTEGRATION_ENABLED

// Event listener for the one-shot --vss-snapshot flow.
// Tracks controller availability, all-five-lamps-received, and health-received.
class VssSnapshotListener final
    : public body_control::lighting::service::OperatorServiceEventListenerInterface
{
public:
    bool WaitForController(std::chrono::seconds timeout)
    {
        std::unique_lock<std::mutex> lock {mutex_};
        return cv_.wait_for(lock, timeout, [this]{ return controller_available_; });
    }

    bool WaitForAllLamps(std::chrono::seconds timeout)
    {
        std::unique_lock<std::mutex> lock {mutex_};
        return cv_.wait_for(lock, timeout,
            [this]{ return lamp_received_count_ >= static_cast<int>(kFunctionCount); });
    }

    bool WaitForHealth(std::chrono::seconds timeout)
    {
        std::unique_lock<std::mutex> lock {mutex_};
        return cv_.wait_for(lock, timeout, [this]{ return health_received_; });
    }

    void OnLampStatusUpdated(
        const body_control::lighting::domain::LampStatus& s) override
    {
        const std::size_t idx = LampFunctionToIndex(s.function);
        if (idx < kFunctionCount)
        {
            {
                const std::lock_guard<std::mutex> lock {mutex_};
                if (!lamp_received_[idx])
                {
                    lamp_received_[idx] = true;
                    lamp_received_count_++;
                }
            }
            cv_.notify_all();
        }
    }

    void OnNodeHealthUpdated(
        const body_control::lighting::domain::NodeHealthStatus&) override
    {
        {
            const std::lock_guard<std::mutex> lock {mutex_};
            health_received_ = true;
        }
        cv_.notify_all();
    }

    void OnControllerAvailabilityChanged(bool is_available) override
    {
        if (is_available)
        {
            {
                const std::lock_guard<std::mutex> lock {mutex_};
                controller_available_ = true;
            }
            cv_.notify_all();
        }
    }

private:
    static constexpr std::size_t kFunctionCount {5U};

    // Maps LampFunction enum value (1–5) to a zero-based index (0–4).
    // Returns kFunctionCount as a sentinel for kUnknown or out-of-range values.
    [[nodiscard]] static std::size_t LampFunctionToIndex(
        body_control::lighting::domain::LampFunction f) noexcept
    {
        const auto val = static_cast<std::uint8_t>(f);
        if (val == 0U || val > static_cast<std::uint8_t>(kFunctionCount))
        {
            return kFunctionCount;
        }
        return static_cast<std::size_t>(val) - std::size_t{1};
    }

    std::mutex              mutex_ {};
    std::condition_variable cv_ {};
    bool                    controller_available_ {false};
    bool                    health_received_       {false};
    int                     lamp_received_count_   {0};
    std::array<bool, kFunctionCount> lamp_received_ {};
};

// One-shot VSS snapshot: connect → collect → emit → exit.
// Returns an exit code suitable for main().
[[nodiscard]] static int RunVssSnapshot()
{
    using namespace body_control::lighting;

    std::unique_ptr<transport::TransportAdapterInterface> transport_adapter =
        transport::vsomeip::CreateOperatorClientVsomeipClientAdapter();

    if (!transport_adapter)
    {
        std::cerr << "error: failed to create transport adapter.\n";
        return 3;
    }

    service::OperatorServiceConsumer consumer {
        *transport_adapter,
        domain::operator_service::kDiagnosticConsoleApplicationId};

    VssSnapshotListener listener {};
    consumer.SetEventListener(&listener);

    if (consumer.Initialize() != service::OperatorServiceStatus::kSuccess)
    {
        std::cerr << "error: failed to initialize operator service consumer.\n";
        return 3;
    }

    // Wait for the controller to become reachable.
    if (!listener.WaitForController(std::chrono::seconds{10}))
    {
        std::cerr << "error: controller not reachable after 10 s. "
                     "Is central_zone_controller_app running?\n";
        static_cast<void>(consumer.Shutdown());
        return 3;
    }

    // Request fresh state: all lamp statuses + current health.
    static_cast<void>(consumer.RequestGetAllLampStates());
    static_cast<void>(consumer.RequestNodeHealth());

    // Wait for all 5 lamp functions to arrive in the cache.
    if (!listener.WaitForAllLamps(std::chrono::seconds{5}))
    {
        std::cerr << "error: lamp state incomplete after 5 s. "
                     "Is the exterior_lighting_node running?\n";
        static_cast<void>(consumer.Shutdown());
        return 3;
    }

    // Wait for a health event (controller publishes periodically + on request).
    if (!listener.WaitForHealth(std::chrono::seconds{3}))
    {
        std::cerr << "error: health status not received after 3 s.\n";
        static_cast<void>(consumer.Shutdown());
        return 3;
    }

    // ── Collect lamp statuses from the consumer cache ────────────────────────

    constexpr std::array<domain::LampFunction, vss::VssLampOverlay::kLampFunctionCount>
        kAllFunctions {{
            domain::LampFunction::kLeftIndicator,
            domain::LampFunction::kRightIndicator,
            domain::LampFunction::kHazardLamp,
            domain::LampFunction::kParkLamp,
            domain::LampFunction::kHeadLamp,
        }};

    std::array<domain::LampStatus, vss::VssLampOverlay::kLampFunctionCount>
        lamp_statuses {};

    for (std::size_t i = 0U; i < kAllFunctions.size(); ++i)
    {
        // Pre-set function so the entry is valid even if GetLampStatus returns
        // false (no status received yet → output_state stays kUnknown → not on).
        lamp_statuses[i].function = kAllFunctions[i];
        static_cast<void>(consumer.GetLampStatus(kAllFunctions[i], lamp_statuses[i]));
    }

    // ── Build LampFaultStatus from NodeHealthStatus ──────────────────────────
    //
    // NodeHealthStatus exposes fault_present and active_fault_count but not
    // individual DTC codes. The per-function ActiveFaultCode signals (0xB001–
    // 0xB005) require the UDS/DoIP path (services 0x22/0x19 over port 13400).
    // Via the operator service, active_faults[] stays kNoFault, so all
    // ActiveFaultCode fields will be 0x0000 in the snapshot.

    domain::NodeHealthStatus health {};
    consumer.GetNodeHealthStatus(health);

    domain::LampFaultStatus fault_status {};
    fault_status.fault_present = health.lamp_driver_fault_present;
    // Clamp: NodeHealthStatus.active_fault_count is uint16; LampFaultStatus
    // uses uint8 with a maximum of 5 (one per lamp function).
    const std::uint16_t raw_count = health.active_fault_count;
    fault_status.active_fault_count =
        (raw_count > std::uint16_t{5U})
        ? std::uint8_t{5U}
        : static_cast<std::uint8_t>(raw_count);
    // active_faults[] intentionally left as kNoFault — see note above.

    // ── Produce and emit the snapshot ────────────────────────────────────────

    const vss::VssLampOverlay overlay {};
    const vss::VssSnapshot snapshot = overlay.Snapshot(lamp_statuses, fault_status);
    std::cout << vss::VssLampOverlay::ToJson(snapshot) << '\n';

    static_cast<void>(consumer.Shutdown());
    return 0;
}

#endif  // BCL_VSS_INTEGRATION_ENABLED

}  // namespace

int main(int argc, char** argv)
{
    // ── Parse CLI flags ───────────────────────────────────────────────────────
    bool vss_snapshot {false};
    bool show_help    {false};

    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg {argv[i]};  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        if (arg == "--vss-snapshot")         { vss_snapshot = true; }
        else if (arg == "--help" || arg == "-h") { show_help = true; }
    }

    if (show_help)
    {
        PrintHelp();
        return 0;
    }

    if (vss_snapshot)
    {
#ifdef BCL_VSS_INTEGRATION_ENABLED
        return RunVssSnapshot();
#else
        std::cerr << "VSS integration not built; rebuild with vss-tools installed "
                     "(see vss/requirements.txt)\n";
        return 2;
#endif
    }

    // ── Interactive diagnostic menu ───────────────────────────────────────────

    using body_control::lighting::domain::LampFunction;
    using body_control::lighting::domain::operator_service::
        kDiagnosticConsoleApplicationId;
    using body_control::lighting::service::OperatorServiceConsumer;
    using body_control::lighting::service::OperatorServiceStatus;
    using body_control::lighting::transport::TransportAdapterInterface;

    std::unique_ptr<TransportAdapterInterface> transport_adapter =
        body_control::lighting::transport::vsomeip::
            CreateOperatorClientVsomeipClientAdapter();

    if (transport_adapter == nullptr)
    {
        std::cerr << "Failed to create diagnostic transport adapter.\n";
        return 1;
    }

    OperatorServiceConsumer operator_service {
        *transport_adapter,
        kDiagnosticConsoleApplicationId};

    DiagnosticEventListener event_listener {};
    operator_service.SetEventListener(&event_listener);

    const OperatorServiceStatus init_status =
        operator_service.Initialize();

    if (init_status != OperatorServiceStatus::kSuccess)
    {
        std::cerr << "Failed to initialize diagnostic console.\n";
        return 1;
    }

    std::cout << "Waiting for controller (up to 10 s)...\n";
    if (!event_listener.WaitForController(std::chrono::seconds{10}))
    {
        std::cerr << "Controller not found. Is it running?\n";
        static_cast<void>(operator_service.Shutdown());
        return 1;
    }

    bool keep_running {true};

    while (keep_running)
    {
        std::cout << "\n=== Diagnostic Console ===\n";
        std::cout << "1  -> Activate left indicator\n";
        std::cout << "2  -> Deactivate left indicator\n";
        std::cout << "3  -> Activate right indicator\n";
        std::cout << "4  -> Deactivate right indicator\n";
        std::cout << "5  -> Activate hazard lamp\n";
        std::cout << "6  -> Deactivate hazard lamp\n";
        std::cout << "7  -> Activate park lamp\n";
        std::cout << "8  -> Deactivate park lamp\n";
        std::cout << "9  -> Activate head lamp\n";
        std::cout << "10 -> Deactivate head lamp\n";
        std::cout << "11 -> Request node health\n";
        std::cout << "12 -> Inject fault (select lamp)\n";
        std::cout << "13 -> Clear fault (select lamp)\n";
        std::cout << "14 -> Clear all faults\n";
        std::cout << "15 -> Get fault status\n";
        std::cout << "0  -> Exit\n";
        std::cout << "Selection: ";

        int selection {0};
        if (!(std::cin >> selection))
        {
            if (std::cin.eof()) { break; }
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        OperatorServiceStatus operator_status {OperatorServiceStatus::kSuccess};
        LampFunction status_function_to_print {LampFunction::kUnknown};
        bool should_print_lamp_status {false};

        switch (selection)
        {
        case 1:
            operator_status = operator_service.RequestLampActivate(
                LampFunction::kLeftIndicator);
            status_function_to_print = LampFunction::kLeftIndicator;
            should_print_lamp_status = true;
            break;

        case 2:
            operator_status = operator_service.RequestLampDeactivate(
                LampFunction::kLeftIndicator);
            status_function_to_print = LampFunction::kLeftIndicator;
            should_print_lamp_status = true;
            break;

        case 3:
            operator_status = operator_service.RequestLampActivate(
                LampFunction::kRightIndicator);
            status_function_to_print = LampFunction::kRightIndicator;
            should_print_lamp_status = true;
            break;

        case 4:
            operator_status = operator_service.RequestLampDeactivate(
                LampFunction::kRightIndicator);
            status_function_to_print = LampFunction::kRightIndicator;
            should_print_lamp_status = true;
            break;

        case 5:
            operator_status = operator_service.RequestLampActivate(
                LampFunction::kHazardLamp);
            status_function_to_print = LampFunction::kHazardLamp;
            should_print_lamp_status = true;
            break;

        case 6:
            operator_status = operator_service.RequestLampDeactivate(
                LampFunction::kHazardLamp);
            status_function_to_print = LampFunction::kHazardLamp;
            should_print_lamp_status = true;
            break;

        case 7:
            operator_status = operator_service.RequestLampActivate(
                LampFunction::kParkLamp);
            status_function_to_print = LampFunction::kParkLamp;
            should_print_lamp_status = true;
            break;

        case 8:
            operator_status = operator_service.RequestLampDeactivate(
                LampFunction::kParkLamp);
            status_function_to_print = LampFunction::kParkLamp;
            should_print_lamp_status = true;
            break;

        case 9:
            operator_status = operator_service.RequestLampActivate(
                LampFunction::kHeadLamp);
            status_function_to_print = LampFunction::kHeadLamp;
            should_print_lamp_status = true;
            break;

        case 10:
            operator_status = operator_service.RequestLampDeactivate(
                LampFunction::kHeadLamp);
            status_function_to_print = LampFunction::kHeadLamp;
            should_print_lamp_status = true;
            break;

        case 11:
        {
            operator_status = operator_service.RequestNodeHealth();

            body_control::lighting::domain::NodeHealthStatus node_health_status {};
            operator_service.GetNodeHealthStatus(node_health_status);

            using body_control::lighting::hmi::NodeHealthStateToString;
            std::cout << "Node health: "
                      << NodeHealthStateToString(node_health_status.health_state)
                      << ", eth="
                      << (node_health_status.ethernet_link_available ? "up" : "down")
                      << ", svc="
                      << (node_health_status.service_available ? "up" : "down")
                      << ", fault="
                      << (node_health_status.lamp_driver_fault_present ? "yes" : "no")
                      << ", fault_count="
                      << node_health_status.active_fault_count
                      << '\n';
            break;
        }

        case 12:
        {
            std::cout << "Select lamp to inject fault:\n";
            const LampFunction fn = SelectLampFunction();
            if (fn == LampFunction::kUnknown)
            {
                std::cout << "Invalid lamp.\n";
                continue;
            }
            operator_status = operator_service.RequestInjectFault(fn);
            break;
        }

        case 13:
        {
            std::cout << "Select lamp to clear fault:\n";
            const LampFunction fn = SelectLampFunction();
            if (fn == LampFunction::kUnknown)
            {
                std::cout << "Invalid lamp.\n";
                continue;
            }
            operator_status = operator_service.RequestClearFault(fn);
            break;
        }

        case 14:
        {
            const LampFunction kAllFunctions[] {
                LampFunction::kLeftIndicator,
                LampFunction::kRightIndicator,
                LampFunction::kHazardLamp,
                LampFunction::kParkLamp,
                LampFunction::kHeadLamp};
            for (const LampFunction f : kAllFunctions)
            {
                static_cast<void>(operator_service.RequestClearFault(f));
            }
            std::cout << "Clear-all-faults sent.\n";
            continue;
        }

        case 15:
        {
            operator_status = operator_service.RequestGetFaultStatus();

            body_control::lighting::domain::NodeHealthStatus nh {};
            operator_service.GetNodeHealthStatus(nh);

            std::cout << "Fault status: "
                      << (nh.lamp_driver_fault_present ? "FAULT PRESENT" : "no fault")
                      << ", count=" << nh.active_fault_count << '\n';
            break;
        }

        case 0:
            keep_running = false;
            continue;

        default:
            std::cout << "Invalid selection.\n";
            continue;
        }

        if (operator_status != OperatorServiceStatus::kSuccess)
        {
            std::cout << "Command failed.\n";
            continue;
        }

        if (should_print_lamp_status)
        {
            PrintCachedLampStatus(
                operator_service,
                status_function_to_print);
        }
    }

    const OperatorServiceStatus shutdown_status =
        operator_service.Shutdown();

    if (shutdown_status != OperatorServiceStatus::kSuccess)
    {
        std::cerr << "Diagnostic console shutdown completed with errors.\n";
        return 1;
    }

    return 0;
}
