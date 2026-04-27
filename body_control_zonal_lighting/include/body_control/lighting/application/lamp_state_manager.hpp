#ifndef BODY_CONTROL_LIGHTING_APPLICATION_LAMP_STATE_MANAGER_HPP_
#define BODY_CONTROL_LIGHTING_APPLICATION_LAMP_STATE_MANAGER_HPP_

#include <array>
#include <cstddef>
#include <cstdint>

#include "body_control/lighting/application/command_arbitrator.hpp"
#include "body_control/lighting/domain/lamp_status_types.hpp"

namespace body_control::lighting::application
{

/**
 * Maintains the latest known output state for every lamp function.
 *
 * The controller writes into this manager each time it receives a LampStatus
 * event from the rear node; the arbitrator and HMI read from it via
 * GetArbitrationContext() and GetLampStatus() respectively.  All operations
 * are O(1) — the backing store is a fixed-size array indexed by lamp function.
 *
 * Not thread-safe; the controller serialises access with its cache_mutex_.
 */
class LampStateManager
{
public:
    LampStateManager() noexcept;
    ~LampStateManager() = default;

    LampStateManager(const LampStateManager&) = delete;
    LampStateManager& operator=(const LampStateManager&) = delete;
    LampStateManager(LampStateManager&&) = delete;
    LampStateManager& operator=(LampStateManager&&) = delete;

    /** Clears all cached statuses to their zero-initialised defaults. */
    void Reset() noexcept;

    /**
     * Writes a received LampStatus into the cache, replacing the previous
     * entry for that lamp function.
     *
     * @param lamp_status  Status report from the rear node.
     * @return false if lamp_status.function is kUnknown or out of range.
     */
    [[nodiscard]] bool UpdateLampStatus(
        const domain::LampStatus& lamp_status) noexcept;

    /**
     * Reads the cached LampStatus for a lamp function.
     *
     * @param function    Which lamp to query.
     * @param lamp_status Populated with the cached value on true.
     * @return false if no status has been stored for this function yet.
     */
    [[nodiscard]] bool GetLampStatus(
        domain::LampFunction function,
        domain::LampStatus& lamp_status) const noexcept;

    /**
     * Returns true if the given lamp function has output state kOn.
     *
     * Convenience wrapper used by the arbitrator and health checks; avoids
     * callers writing output_state == LampOutputState::kOn everywhere.
     *
     * @param function  Lamp function to check.
     * @return false if no status has been received or state is kOff/kUnknown.
     */
    [[nodiscard]] bool IsFunctionActive(
        domain::LampFunction function) const noexcept;

    /**
     * Builds an ArbitrationContext from the current cached states.
     *
     * The controller calls this immediately before Arbitrate() to give the
     * arbitrator a consistent snapshot of all five lamp functions at once.
     */
    [[nodiscard]] ArbitrationContext GetArbitrationContext() const noexcept;

private:
    /// Number of distinct lamp functions tracked (matches domain::LampFunction count).
    static constexpr std::size_t kManagedLampCount {5U};

    /**
     * Maps a LampFunction to a zero-based array index.
     *
     * @param function  Must be in [kLeftIndicator, kHeadLamp].
     * @param index     Set to the corresponding array position on true.
     * @return false if function is kUnknown or out of the valid range.
     */
    [[nodiscard]] static bool TryGetIndex(
        domain::LampFunction function,
        std::size_t& index) noexcept;

    /** Returns true when output_state is kOn; false for kOff or kUnknown. */
    [[nodiscard]] static bool IsOutputStateActive(
        domain::LampOutputState output_state) noexcept;

    /// Indexed by LampFunctionToIndex(); zero-initialised until first update.
    std::array<domain::LampStatus, kManagedLampCount> lamp_status_cache_ {};
};

}  // namespace body_control::lighting::application

#endif  // BODY_CONTROL_LIGHTING_APPLICATION_LAMP_STATE_MANAGER_HPP_
