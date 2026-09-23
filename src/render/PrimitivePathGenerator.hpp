#pragma once

#include "avemotion/model/AssetModel.hpp"
#include "avemotion/runtime/EvaluatedScene.hpp"

#include <array>
#include <cstddef>
#include <span>

namespace avemotion::render::detail {

struct PrimitivePath final {
    // Telegram's polystar implementation accepts fractional point counts and
    // can emit two cubic segments for every authored star point. Keep this
    // laboratory path bounded and fail closed for unusually large assets.
    static constexpr std::size_t kMaximumAuthoredPointCount = 256U;
    static constexpr std::size_t kMaximumVerbCount =
        kMaximumAuthoredPointCount * 2U + 3U;
    static constexpr std::size_t kMaximumPointCount =
        kMaximumAuthoredPointCount * 6U + 2U;

    std::array<runtime::PathVerb, kMaximumVerbCount> verbs{};
    std::array<model::MotionVec2Value, kMaximumPointCount> points{};
    std::size_t verbCount = 0U;
    std::size_t pointCount = 0U;
    bool rounded = false;
    bool valid = true;

    [[nodiscard]] std::span<const runtime::PathVerb> verbSpan() const noexcept {
        return {verbs.data(), verbCount};
    }

    [[nodiscard]] std::span<const model::MotionVec2Value> pointSpan() const noexcept {
        return {points.data(), pointCount};
    }
};

[[nodiscard]] PrimitivePath generateRectanglePath(
    model::MotionVec2Value position,
    model::MotionVec2Value size,
    float roundness,
    model::SourcePathDirection direction) noexcept;

[[nodiscard]] PrimitivePath generateEllipsePath(
    model::MotionVec2Value position,
    model::MotionVec2Value size,
    model::SourcePathDirection direction) noexcept;

[[nodiscard]] PrimitivePath generatePolystarPath(
    model::MotionVec2Value position,
    float pointCount,
    float innerRadius,
    float outerRadius,
    float innerRoundness,
    float outerRoundness,
    float rotationDegrees,
    model::SourcePathDirection direction) noexcept;

[[nodiscard]] PrimitivePath generatePolygonPath(
    model::MotionVec2Value position,
    float pointCount,
    float outerRadius,
    float outerRoundness,
    float rotationDegrees,
    model::SourcePathDirection direction) noexcept;

} // namespace avemotion::render::detail
