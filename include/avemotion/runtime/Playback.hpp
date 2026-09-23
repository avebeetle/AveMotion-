#pragma once

#include "avemotion/model/Ids.hpp"

#include <cstddef>
#include <cstdint>

namespace avemotion::runtime {

struct MotionTime final {
    std::int64_t nanoseconds = 0;

    [[nodiscard]] static constexpr MotionTime fromNanoseconds(
        std::int64_t value) noexcept {
        return {value};
    }

    [[nodiscard]] static MotionTime fromSeconds(double seconds) noexcept;

    [[nodiscard]] friend constexpr bool operator==(
        const MotionTime&,
        const MotionTime&) noexcept = default;
};

enum class PlaybackStatus : std::uint8_t {
    Stopped,
    Playing,
    Paused,
    Controlled,
    Holding,
};

enum class PlaybackDirection : std::int8_t {
    Reverse = -1,
    Forward = 1,
};

enum class PlaybackLoopMode : std::uint8_t {
    Once,
    Loop,
};

struct PlaybackSnapshot final {
    PlaybackStatus status = PlaybackStatus::Stopped;
    PlaybackDirection direction = PlaybackDirection::Forward;
    PlaybackLoopMode loopMode = PlaybackLoopMode::Loop;
    model::ClipId clip{0U};
    double playbackRate = 1.0;
    double normalizedPosition = 0.0;
    std::size_t frameIndex = 0;
    std::uint64_t revision = 0;
    bool completed = false;
};

} // namespace avemotion::runtime
