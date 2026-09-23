#pragma once

#include "avemotion/runtime/Diagnostics.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace avemotion::corpus_lab {

enum class SamplePhase { First, Warmup, Steady };

template <typename Visit>
[[nodiscard]] bool forEachSamplePhase(std::size_t totalFrames,
                                      std::size_t warmupSamples,
                                      std::size_t measuredSamples,
                                      Visit&& visit) {
    const auto frames = std::max<std::size_t>(1U, totalFrames);
    if (!visit(SamplePhase::First, 0U)) return false;
    for (std::size_t sample = 0U; sample < warmupSamples; ++sample) {
        if (!visit(SamplePhase::Warmup, sample % frames)) return false;
    }
    for (std::size_t sample = 0U; sample < measuredSamples; ++sample) {
        if (!visit(SamplePhase::Steady, sample % frames)) return false;
    }
    return true;
}

template <typename Duration>
[[nodiscard]] std::int64_t nanoseconds(Duration duration) noexcept {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}

[[nodiscard]] inline std::int64_t medianNanoseconds(
    std::vector<std::int64_t> values) {
    if (values.empty()) return 0;
    std::sort(values.begin(), values.end());
    const auto middle = values.size() / 2U;
    if ((values.size() & 1U) != 0U) return values[middle];
    return values[middle - 1U] / 2 + values[middle] / 2
        + ((values[middle - 1U] & 1) && (values[middle] & 1));
}

[[nodiscard]] inline std::int64_t nearestRankP95Nanoseconds(
    std::vector<std::int64_t> values) {
    if (values.empty()) return 0;
    std::sort(values.begin(), values.end());
    const auto rank = (95U * values.size() + 99U) / 100U;
    return values[std::min(rank, values.size()) - 1U];
}

struct PhaseCounts final {
    std::uint64_t metadataSessions = 0;
    std::uint64_t sceneSessions = 0;
    std::uint64_t modelSessions = 0;
    std::uint64_t cpuSessions = 0;
    std::uint64_t modelSamples = 0;
    std::uint64_t sceneSamples = 0;
};

[[nodiscard]] inline PhaseCounts phaseDelta(
    const runtime::DiagnosticsSnapshot& before,
    const runtime::DiagnosticsSnapshot& after) noexcept {
    return {
        after.referenceMetadataSessionsCreated - before.referenceMetadataSessionsCreated,
        after.referenceSceneSessionsCreated - before.referenceSceneSessionsCreated,
        after.referenceModelSessionsCreated - before.referenceModelSessionsCreated,
        after.referenceCpuSessionsCreated - before.referenceCpuSessionsCreated,
        after.referenceModelSamples - before.referenceModelSamples,
        after.referenceSceneSamples - before.referenceSceneSamples,
    };
}

} // namespace avemotion::corpus_lab
