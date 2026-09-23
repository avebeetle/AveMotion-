#pragma once

#include "avemotion/model/AssetModel.hpp"

#include <cstdint>

namespace avemotion::render::detail {

struct RepeaterTransformSample final {
    float copies = 0.0F;
    float offset = 0.0F;
    model::MotionVec2Value position;
    model::MotionVec2Value scale{100.0F, 100.0F};
    float rotationDegrees = 0.0F;
    model::MotionVec2Value anchor;
    float startOpacity = 100.0F;
    float endOpacity = 100.0F;
};

struct RepeaterCopyState final {
    model::MotionMatrix3x2Value localMatrix;
    float opacityMultiplier = 1.0F;
    std::int32_t visibleCopies = 0;
    bool visible = false;
    bool valid = false;
};

// Reproduces the pinned Telegram rlottie LOTRepeaterTransform and
// LOTRepeaterItem arithmetic. The returned matrix is local to the repeater's
// parent; callers append parent/composition/presentation transforms.
[[nodiscard]] RepeaterCopyState evaluateRepeaterCopy(
    const RepeaterTransformSample& sample,
    std::uint32_t copyIndex) noexcept;

} // namespace avemotion::render::detail
