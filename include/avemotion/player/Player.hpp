#pragma once

#include "avemotion/runtime/Handles.hpp"
#include "avemotion/runtime/Playback.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace avemotion::runtime {
class Instance;
}

namespace avemotion::player {

inline constexpr std::uint32_t kInvalidPlayerHandleIndex =
    runtime::kInvalidHandleIndex;

struct PlayerHandle final {
    std::uint32_t index = kInvalidPlayerHandleIndex;
    std::uint32_t generation = 0;

    [[nodiscard]] constexpr bool valid() const noexcept {
        return index != kInvalidPlayerHandleIndex && generation != 0U;
    }

    [[nodiscard]] constexpr std::uint64_t packed() const noexcept {
        return (static_cast<std::uint64_t>(generation) << 32U)
            | static_cast<std::uint64_t>(index);
    }

    [[nodiscard]] friend constexpr bool operator==(
        const PlayerHandle&,
        const PlayerHandle&) noexcept = default;
};

enum class HiddenTimePolicy : std::uint8_t {
    // Logical playback continues while no frames are requested. The current
    // state is sampled immediately when the entry becomes visible again.
    KeepUp,

    // A playing entry is paused when hidden and resumed from the same logical
    // position when shown. User-paused entries are never resumed implicitly.
    Freeze,
};

enum class FrameSelection : std::uint8_t {
    // Return only entries whose timeline, lifecycle or explicit invalidation
    // requires a new host frame.
    DueOnly,

    // Also return every currently visible entry. This is intended for host
    // repaint/expose paths where pixels must be redrawn even if animation
    // state did not change.
    AllVisible,
};

enum class FrameReason : std::uint32_t {
    None = 0U,
    FirstFrame = 1U << 0U,
    TimelineAdvanced = 1U << 1U,
    PlaybackChanged = 1U << 2U,
    VisibilityChanged = 1U << 3U,
    ExplicitInvalidation = 1U << 4U,
    Completion = 1U << 5U,
    HostRepaint = 1U << 6U,
};

[[nodiscard]] constexpr FrameReason operator|(
    FrameReason left,
    FrameReason right) noexcept {
    return static_cast<FrameReason>(
        static_cast<std::uint32_t>(left)
        | static_cast<std::uint32_t>(right));
}

constexpr FrameReason& operator|=(
    FrameReason& left,
    FrameReason right) noexcept {
    left = left | right;
    return left;
}

[[nodiscard]] constexpr bool hasReason(
    FrameReason value,
    FrameReason reason) noexcept {
    return (static_cast<std::uint32_t>(value)
        & static_cast<std::uint32_t>(reason)) != 0U;
}

struct PlayerEntryOptions final {
    // Upper bound for presentation wakeups generated for this entry. The
    // effective cadence is min(assetFrameRate * playbackRate, this value).
    double maximumPresentationRate = 60.0;
    bool visible = true;
    HiddenTimePolicy hiddenTimePolicy = HiddenTimePolicy::KeepUp;
};

// The player never creates a thread or a platform timer. These optional
// callbacks let a host adapter map the portable schedule onto its own event
// loop. Callbacks are dispatched on the same control thread that called the
// Player method and must not re-enter the same Player object.
struct PlayerHostCallbacks final {
    void* userData = nullptr;
    void (*requestFrame)(void* userData) noexcept = nullptr;
    void (*scheduleWakeup)(
        void* userData,
        runtime::MotionTime deadline) noexcept = nullptr;
    void (*cancelWakeup)(void* userData) noexcept = nullptr;
};

enum class PlayerErrorCode : std::uint8_t {
    None,
    InvalidArgument,
    InvalidHandle,
    DuplicateInstance,
};

struct PlayerError final {
    PlayerErrorCode code = PlayerErrorCode::None;
    std::string message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return code != PlayerErrorCode::None;
    }
};

struct PlayerAddResult final {
    PlayerHandle handle;
    PlayerError error;

    [[nodiscard]] explicit operator bool() const noexcept {
        return handle.valid() && !error;
    }
};

struct ScheduledFrame final {
    PlayerHandle handle;
    runtime::InstanceHandle instanceHandle;
    runtime::Instance* instance = nullptr;
    runtime::PlaybackSnapshot playback;
    FrameReason reasons = FrameReason::None;
    bool visible = false;
};

struct PlayerTickView final {
    // The span remains valid until the next non-const Player call.
    std::span<const ScheduledFrame> frames;
    std::optional<runtime::MotionTime> nextDeadline;
    std::uint64_t sequence = 0U;
    std::uint64_t skippedDeadlines = 0U;
};

struct PlayerDiagnostics final {
    std::uint64_t registrations = 0U;
    std::uint64_t removals = 0U;
    std::uint64_t ticks = 0U;
    std::uint64_t framesReturned = 0U;
    std::uint64_t frameRequestsPosted = 0U;
    std::uint64_t frameRequestsCoalesced = 0U;
    std::uint64_t wakeupsScheduled = 0U;
    std::uint64_t wakeupsCancelled = 0U;
    std::uint64_t skippedDeadlines = 0U;
    std::uint64_t visibilityChanges = 0U;
    std::uint64_t automaticPauses = 0U;
    std::uint64_t automaticResumes = 0U;
    std::uint64_t storageGeneration = 0U;
    std::size_t registeredInstances = 0U;
    std::size_t visibleInstances = 0U;
    std::size_t playingInstances = 0U;
};

namespace detail {
struct PlayerState;
}

// Application-independent centralized player/scheduler.
//
// Threading contract:
// - all methods are control-thread-only;
// - no hidden worker or timer is created;
// - Instance evaluation and rendering remain host responsibilities;
// - one Player can coordinate many runtime::Instance objects;
// - registrations keep instances alive until explicit removal or clear().
class Player final {
public:
    explicit Player(PlayerHostCallbacks callbacks = {});
    Player(Player&&) noexcept;
    Player& operator=(Player&&) noexcept;
    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;
    ~Player();

    void setHostCallbacks(
        PlayerHostCallbacks callbacks,
        runtime::MotionTime now) noexcept;

    [[nodiscard]] PlayerAddResult addInstance(
        std::shared_ptr<runtime::Instance> instance,
        runtime::MotionTime now,
        PlayerEntryOptions options = {});

    [[nodiscard]] bool removeInstance(
        PlayerHandle handle,
        runtime::MotionTime now) noexcept;

    void clear(runtime::MotionTime now) noexcept;

    [[nodiscard]] std::shared_ptr<runtime::Instance> instance(
        PlayerHandle handle) const noexcept;

    [[nodiscard]] bool setVisible(
        PlayerHandle handle,
        bool visible,
        runtime::MotionTime now) noexcept;

    [[nodiscard]] bool setMaximumPresentationRate(
        PlayerHandle handle,
        double framesPerSecond,
        runtime::MotionTime now) noexcept;

    [[nodiscard]] bool invalidate(
        PlayerHandle handle,
        FrameReason reason = FrameReason::ExplicitInvalidation) noexcept;

    [[nodiscard]] bool play(
        PlayerHandle handle,
        runtime::MotionTime now) noexcept;
    [[nodiscard]] bool pause(
        PlayerHandle handle,
        runtime::MotionTime now) noexcept;
    [[nodiscard]] bool resume(
        PlayerHandle handle,
        runtime::MotionTime now) noexcept;
    [[nodiscard]] bool stop(
        PlayerHandle handle,
        runtime::MotionTime now) noexcept;
    [[nodiscard]] bool seekNormalized(
        PlayerHandle handle,
        double normalizedPosition,
        runtime::MotionTime now) noexcept;
    [[nodiscard]] bool setControlledProgress(
        PlayerHandle handle,
        double normalizedPosition,
        runtime::MotionTime now) noexcept;
    [[nodiscard]] bool setDirection(
        PlayerHandle handle,
        runtime::PlaybackDirection direction,
        runtime::MotionTime now) noexcept;
    [[nodiscard]] bool setPlaybackRate(
        PlayerHandle handle,
        double playbackRate,
        runtime::MotionTime now) noexcept;
    [[nodiscard]] bool setLoopMode(
        PlayerHandle handle,
        runtime::PlaybackLoopMode loopMode,
        runtime::MotionTime now) noexcept;

    [[nodiscard]] PlayerTickView tick(
        runtime::MotionTime now,
        FrameSelection selection = FrameSelection::DueOnly) noexcept;

    [[nodiscard]] std::optional<runtime::MotionTime> nextDeadline() const noexcept;
    [[nodiscard]] PlayerDiagnostics diagnostics() const noexcept;
    void resetDiagnostics() noexcept;

private:
    std::unique_ptr<detail::PlayerState> state_;
};

} // namespace avemotion::player
