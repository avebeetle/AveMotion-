#pragma once

#include "avemotion/model/AssetModel.hpp"
#include "avemotion/runtime/EvaluatedScene.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace avemotion::render::detail {

struct NormalizedTrimSegment final {
    float start = 0.0F;
    float end = 0.0F;
    bool valid = false;
    bool empty = true;
    bool full = false;
};

struct TrimmedPath final {
    std::vector<runtime::PathVerb> verbs;
    std::vector<model::MotionVec2Value> points;
    float sourceLength = 0.0F;
    std::size_t contourCount = 0U;
    std::size_t splitCount = 0U;
    bool valid = false;
    bool empty = true;
    bool full = false;
    std::uint64_t storageGeneration = 0U;

    void prepare(std::size_t verbCapacity, std::size_t pointCapacity);
    void reset() noexcept;
};

enum class TrimPathCollectionMode : std::uint8_t {
    Simultaneous,
    Individual,
};

struct TrimPathInput final {
    std::span<const runtime::PathVerb> verbs;
    std::span<const model::MotionVec2Value> points;
};

struct TrimmedPathSlice final {
    std::size_t firstVerb = 0U;
    std::size_t verbCount = 0U;
    std::size_t firstPoint = 0U;
    std::size_t pointCount = 0U;
    float sourceLength = 0.0F;
    bool empty = true;
    bool full = false;
};

// Retained output for a set of authored paths controlled by one Trim Path.
// The flattened buffers preserve path order exactly. The same workspace can
// be reused for every frame; storageGeneration changes only when a vector
// capacity has to grow.
struct TrimmedPathCollection final {
    std::vector<runtime::PathVerb> verbs;
    std::vector<model::MotionVec2Value> points;
    std::vector<TrimmedPathSlice> paths;
    std::vector<float> sourceLengths;
    TrimmedPath scratch;
    std::size_t splitCount = 0U;
    bool valid = false;
    bool empty = true;
    bool full = false;
    bool individualWrappedNoOp = false;
    std::uint64_t storageGeneration = 0U;

    void prepare(
        std::size_t pathCapacity,
        std::size_t verbCapacity,
        std::size_t pointCapacity,
        std::size_t singlePathVerbCapacity,
        std::size_t singlePathPointCapacity);
    void reset() noexcept;

    [[nodiscard]] std::span<const runtime::PathVerb> pathVerbs(
        std::size_t index) const noexcept;
    [[nodiscard]] std::span<const model::MotionVec2Value> pathPoints(
        std::size_t index) const noexcept;
};

// Reproduces LOTTrimData::segment() from the pinned Telegram baseline.
[[nodiscard]] NormalizedTrimSegment normalizeTrimSegment(
    float startPercent,
    float endPercent,
    float offsetDegrees) noexcept;

[[nodiscard]] float measurePath(
    std::span<const runtime::PathVerb> verbs,
    std::span<const model::MotionVec2Value> points,
    bool& valid) noexcept;

// Reproduces VPathMesure + VDasher behaviour for backend-neutral AveMotion
// path streams. Partial trims deliberately omit Close verbs, matching
// rlottie's dashed-path output; full trims preserve the original stream.
[[nodiscard]] TrimmedPath trimPath(
    std::span<const runtime::PathVerb> verbs,
    std::span<const model::MotionVec2Value> points,
    const NormalizedTrimSegment& segment);

[[nodiscard]] bool trimPathInto(
    std::span<const runtime::PathVerb> verbs,
    std::span<const model::MotionVec2Value> points,
    const NormalizedTrimSegment& segment,
    TrimmedPath& output);

// Telegram semantics:
//  * Simultaneous applies the same normalized segment to every path.
//  * Individual distributes a non-wrapped interval over the aggregate source
//    length in authored path order.
//  * The pinned Telegram implementation performs no operation for a wrapped
//    Individual interval (start > end); this compatibility quirk is exposed
//    through individualWrappedNoOp.
[[nodiscard]] bool trimPathCollection(
    std::span<const TrimPathInput> paths,
    const NormalizedTrimSegment& segment,
    TrimPathCollectionMode mode,
    TrimmedPathCollection& output);

} // namespace avemotion::render::detail
