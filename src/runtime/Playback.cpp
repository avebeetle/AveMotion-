#include "avemotion/runtime/Playback.hpp"

#include <cmath>
#include <limits>

namespace avemotion::runtime {

MotionTime MotionTime::fromSeconds(double seconds) noexcept {
    if (!std::isfinite(seconds)) {
        return {};
    }
    constexpr long double scale = 1'000'000'000.0L;
    const auto value = static_cast<long double>(seconds) * scale;
    if (value >= static_cast<long double>(std::numeric_limits<std::int64_t>::max())) {
        return {std::numeric_limits<std::int64_t>::max()};
    }
    if (value <= static_cast<long double>(std::numeric_limits<std::int64_t>::min())) {
        return {std::numeric_limits<std::int64_t>::min()};
    }
    return {static_cast<std::int64_t>(std::llround(value))};
}

} // namespace avemotion::runtime
