/*
 * Telegram-compatible Repeater transform/opacity evaluation.
 *
 * This AveMotion-owned implementation intentionally reproduces observable
 * numerical behaviour from LGPL-covered Telegram rlottie components
 * LOTRepeaterTransform, LOTRepeaterItem and VMatrix. The original sources and
 * license texts remain vendored; see NOTICE.md and docs/LICENSE_AND_SECURITY.md.
 */

#include "RepeaterTransform.hpp"

#include <cmath>
#include <limits>

namespace avemotion::render::detail {
namespace {

constexpr float kDegreesToRadians = 0.017453292519943295769F;

[[nodiscard]] bool finite(float value) noexcept {
    return std::isfinite(value);
}

[[nodiscard]] bool finite(model::MotionVec2Value value) noexcept {
    return finite(value.x) && finite(value.y);
}

[[nodiscard]] bool finite(const model::MotionMatrix3x2Value& value) noexcept {
    return finite(value.m11) && finite(value.m12)
        && finite(value.m21) && finite(value.m22)
        && finite(value.dx) && finite(value.dy);
}

void telegramSinCos(float angleDegrees, float& sine, float& cosine) noexcept {
    sine = 0.0F;
    cosine = 0.0F;
    if (angleDegrees == 90.0F || angleDegrees == -270.0F) {
        sine = 1.0F;
    } else if (angleDegrees == 270.0F || angleDegrees == -90.0F) {
        sine = -1.0F;
    } else if (angleDegrees == 180.0F) {
        cosine = -1.0F;
    } else {
        const float radians = kDegreesToRadians * angleDegrees;
        sine = std::sin(radians);
        cosine = std::cos(radians);
    }
}

[[nodiscard]] model::MotionMatrix3x2Value telegramRepeaterMatrix(
    const RepeaterTransformSample& sample,
    float multiplier) noexcept {
    // This is deliberately written in the same operation order as
    // LOTRepeaterTransform::matrix():
    // translate(position * n), translate(anchor), scale(pow(scale,n)),
    // rotate(rotation*n), translate(-anchor).
    model::MotionMatrix3x2Value result;

    result.dx = sample.position.x * multiplier;
    result.dy = sample.position.y * multiplier;
    result.dx += sample.anchor.x;
    result.dy += sample.anchor.y;

    const float scaleX = sample.scale.x / 100.0F;
    const float scaleY = sample.scale.y / 100.0F;
    result.m11 = std::pow(scaleX, multiplier);
    result.m22 = std::pow(scaleY, multiplier);

    const float angle = sample.rotationDegrees * multiplier;
    if (angle != 0.0F) {
        float sine = 0.0F;
        float cosine = 0.0F;
        telegramSinCos(angle, sine, cosine);
        const float m11 = cosine * result.m11;
        const float m12 = sine * result.m22;
        const float m21 = -sine * result.m11;
        const float m22 = cosine * result.m22;
        result.m11 = m11;
        result.m12 = m12;
        result.m21 = m21;
        result.m22 = m22;
    }

    result.dx += -sample.anchor.x * result.m11
        + -sample.anchor.y * result.m21;
    result.dy += -sample.anchor.y * result.m22
        + -sample.anchor.x * result.m12;
    return result;
}

} // namespace

RepeaterCopyState evaluateRepeaterCopy(
    const RepeaterTransformSample& sample,
    std::uint32_t copyIndex) noexcept {
    RepeaterCopyState result;
    if (!finite(sample.copies) || !finite(sample.offset)
        || !finite(sample.position) || !finite(sample.scale)
        || !finite(sample.rotationDegrees) || !finite(sample.anchor)
        || !finite(sample.startOpacity) || !finite(sample.endOpacity)
        || sample.copies <= 0.0F
        || sample.copies > static_cast<float>(std::numeric_limits<std::int32_t>::max())) {
        return result;
    }

    const auto visibleCopies = static_cast<std::int32_t>(sample.copies);
    if (visibleCopies == 0) return result;
    result.visibleCopies = visibleCopies;

    const float index = static_cast<float>(copyIndex);
    const float multiplier = index + sample.offset;
    result.localMatrix = telegramRepeaterMatrix(sample, multiplier);
    const float progress = index / sample.copies;
    result.opacityMultiplier = sample.startOpacity / 100.0F
        + progress * ((sample.endOpacity - sample.startOpacity) / 100.0F);
    if (static_cast<std::int32_t>(copyIndex) >= visibleCopies) {
        result.opacityMultiplier = 0.0F;
    }
    result.visible = result.opacityMultiplier != 0.0F;
    result.valid = finite(result.localMatrix) && finite(result.opacityMultiplier);
    return result;
}

} // namespace avemotion::render::detail
