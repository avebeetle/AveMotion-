#include "BenchmarkMetrics.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
} // namespace

int main() {
    using avemotion::corpus_lab::medianNanoseconds;
    using avemotion::corpus_lab::nanoseconds;
    using avemotion::corpus_lab::nearestRankP95Nanoseconds;
    using avemotion::corpus_lab::phaseDelta;

    require(nanoseconds(std::chrono::microseconds{3}) == 3000,
            "duration conversion must retain nanosecond units");
    require(medianNanoseconds({}) == 0, "empty median must be zero");
    require(medianNanoseconds({9, 1, 5}) == 5, "odd median is wrong");
    require(medianNanoseconds({9, 1, 5, 3}) == 4, "even median is wrong");
    require(nearestRankP95Nanoseconds({}) == 0, "empty p95 must be zero");
    require(nearestRankP95Nanoseconds({8}) == 8, "single-value p95 is wrong");
    std::vector<std::int64_t> ordered;
    for (std::int64_t value = 1; value <= 100; ++value) ordered.push_back(value);
    require(nearestRankP95Nanoseconds(ordered) == 95,
            "p95 must use the nearest rank rather than interpolation");

    avemotion::runtime::DiagnosticsSnapshot before;
    before.referenceMetadataSessionsCreated = 2;
    before.referenceSceneSessionsCreated = 3;
    before.referenceModelSessionsCreated = 4;
    before.referenceCpuSessionsCreated = 5;
    before.referenceModelSamples = 6;
    before.referenceSceneSamples = 7;
    auto after = before;
    after.referenceMetadataSessionsCreated += 1;
    after.referenceSceneSessionsCreated += 2;
    after.referenceModelSessionsCreated += 3;
    after.referenceCpuSessionsCreated += 4;
    after.referenceModelSamples += 5;
    after.referenceSceneSamples += 6;
    const auto delta = phaseDelta(before, after);
    require(delta.metadataSessions == 1 && delta.sceneSessions == 2
                && delta.modelSessions == 3 && delta.cpuSessions == 4
                && delta.modelSamples == 5 && delta.sceneSamples == 6,
            "phase delta must subtract all session and sample counters");
    std::cout << "corpus benchmark metric tests passed\n";
}
