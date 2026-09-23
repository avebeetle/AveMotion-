#pragma once

#include "avemotion/render/RenderPlan.hpp"

#include <string>

namespace avemotion::render {

struct HeadlessPlanRecord final {
    RenderPlanFingerprints fingerprints;
    RenderPlanStatistics statistics;
    runtime::RectF presentedBounds;
    runtime::RectF dirtyRegion;
};

class HeadlessPlanBackend final {
public:
    [[nodiscard]] HeadlessPlanRecord record(
        const MotionRenderPlan& plan) const noexcept;
    [[nodiscard]] std::string describe(const MotionRenderPlan& plan) const;
};

[[nodiscard]] RenderPlanFingerprints computeRenderPlanFingerprints(
    const MotionRenderPlan& plan) noexcept;

} // namespace avemotion::render
