#pragma once

#include "avemotion/evaluation/PropertyEvaluator.hpp"
#include "avemotion/runtime/EvaluatedScene.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

namespace avemotion::render {

enum class SourceGeometryProjectionErrorCode : std::uint8_t {
    None,
    InvalidModel,
    InvalidScene,
    InvalidEvaluation,
    InvalidWorkspace,
    CorruptBinding,
    CorruptShape,
};

struct SourceGeometryProjectionStatistics final {
    std::size_t drawItemsVisited = 0;
    std::size_t candidates = 0;
    std::size_t projected = 0;
    std::size_t projectedStatic = 0;
    std::size_t projectedAnimated = 0;
    std::size_t projectedPoints = 0;
    std::size_t primitiveCandidates = 0;
    std::size_t rectangleCandidates = 0;
    std::size_t ellipseCandidates = 0;
    std::size_t polystarCandidates = 0;
    std::size_t starCandidates = 0;
    std::size_t polygonCandidates = 0;
    std::size_t projectedRectangles = 0;
    std::size_t projectedRoundedRectangles = 0;
    std::size_t projectedEllipses = 0;
    std::size_t projectedStars = 0;
    std::size_t projectedPolygons = 0;
    std::size_t trimCandidates = 0;
    std::size_t trimSimultaneousCandidates = 0;
    std::size_t trimIndividualCandidates = 0;
    std::size_t trimMultiPathCandidates = 0;
    std::size_t trimPathsVisited = 0;
    std::size_t trimPathsProjected = 0;
    std::size_t trimIndividualWrappedNoOps = 0;
    std::size_t projectedTrimmed = 0;
    std::size_t projectedTrimEmpty = 0;
    std::size_t projectedTrimFull = 0;
    std::size_t projectedTrimPartial = 0;
    std::size_t trimSplitCount = 0;
    std::size_t repeaterCandidates = 0;
    std::size_t repeaterCopiesVisited = 0;
    std::size_t repeaterCopiesProjected = 0;
    std::size_t repeaterVisibleCopies = 0;
    std::size_t repeaterHiddenCopies = 0;
    std::size_t repeaterStaticGeometryCopies = 0;
    std::size_t repeaterAnimatedGeometryCopies = 0;
    std::size_t repeaterTransformAnimatedCopies = 0;
    std::size_t repeaterOpacityAnimatedCopies = 0;
    std::size_t repeaterFillApplicationsProjected = 0;
    std::size_t repeaterStrokeApplicationsProjected = 0;
    std::size_t repeaterNestedPaintApplicationsProjected = 0;
    std::size_t repeaterAnimatedPaintApplicationsProjected = 0;
    std::size_t skippedRepeaterBinding = 0;
    std::size_t skippedRepeaterProperties = 0;
    std::size_t skippedRepeaterEvaluation = 0;
    std::size_t rejectedRepeaterInput = 0;
    std::size_t repeaterParityMismatches = 0;
    std::size_t skippedTrimBinding = 0;
    std::size_t skippedTrimProperties = 0;
    std::size_t skippedTrimEvaluation = 0;
    std::size_t rejectedTrimInput = 0;
    std::size_t skippedPrimitiveProperties = 0;
    std::size_t skippedPrimitiveEvaluation = 0;
    std::size_t skippedPolystarProperties = 0;
    std::size_t skippedPolystarEvaluation = 0;
    std::size_t rejectedPolystarInputs = 0;
    std::size_t skippedUnbound = 0;
    std::size_t skippedMultiplePaths = 0;
    std::size_t skippedModified = 0;
    std::size_t nestedCompositionCandidates = 0;
    std::size_t skippedNestedComposition = 0;
    std::size_t skippedPaint = 0;
    std::size_t skippedWrongNodeKind = 0;
    std::size_t skippedNoShapeProperty = 0;
    std::size_t skippedShapeEvaluation = 0;
    std::size_t skippedTransform = 0;
    std::size_t skippedUnsupportedShape = 0;
    std::size_t parityMismatches = 0;
};

struct SourceGeometryProjectionResult final {
    SourceGeometryProjectionStatistics statistics;
    SourceGeometryProjectionErrorCode error =
        SourceGeometryProjectionErrorCode::None;

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == SourceGeometryProjectionErrorCode::None;
    }
};

// Per-evaluation-stream retained scratch storage. A workspace belongs to one
// MotionInstance (or one serialized evaluation stream) and must not be used
// concurrently. prepare() establishes conservative capacities from the
// immutable canonical model; storageGeneration changes only if a later input
// forces a capacity increase.
class SourceGeometryProjectionWorkspace final {
public:
    SourceGeometryProjectionWorkspace();
    SourceGeometryProjectionWorkspace(
        SourceGeometryProjectionWorkspace&&) noexcept;
    SourceGeometryProjectionWorkspace& operator=(
        SourceGeometryProjectionWorkspace&&) noexcept;
    SourceGeometryProjectionWorkspace(
        const SourceGeometryProjectionWorkspace&) = delete;
    SourceGeometryProjectionWorkspace& operator=(
        const SourceGeometryProjectionWorkspace&) = delete;
    ~SourceGeometryProjectionWorkspace();

    [[nodiscard]] std::size_t retainedBytes() const noexcept;
    [[nodiscard]] std::uint64_t storageGeneration() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    friend class SourceGeometryProjector;
};

// Projects canonical ShapePath, Rectangle, Ellipse, Polystar and Polygon
// properties into the already-owned local path storage of an EvaluatedScene.
// The projector is intentionally conservative: it accepts modifier-free
// single paths, direct path groups controlled by exactly one Trim Path and one
// direct Repeater content group with supported nested ShapeGroups, local solid
// fills and dashless local solid strokes. Geometry identity is shared across
// compatible paint applications while transform, opacity and paint values are
// evaluated independently. Merge paths, chained modifiers and unsupported
// paint types remain fail-closed. Telegram's extracted state is the parity
// oracle and is left untouched whenever any precondition fails.
class SourceGeometryProjector final {
public:
    explicit SourceGeometryProjector(
        std::shared_ptr<const model::MotionAssetModel> model);
    SourceGeometryProjector(SourceGeometryProjector&&) noexcept;
    SourceGeometryProjector& operator=(SourceGeometryProjector&&) noexcept;
    SourceGeometryProjector(const SourceGeometryProjector&) = delete;
    SourceGeometryProjector& operator=(const SourceGeometryProjector&) = delete;
    ~SourceGeometryProjector();

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] std::string_view errorMessage() const noexcept;

    [[nodiscard]] bool prepare(
        SourceGeometryProjectionWorkspace& workspace) const;

    // Mutates only explicit source-geometry fields and localPath for accepted
    // draw items. Existing legacy/final geometry remains available as oracle.
    // No container is resized when the Telegram local-path storage already has
    // the expected capacity, which is the normal characterization path.
    [[nodiscard]] SourceGeometryProjectionResult project(
        runtime::EvaluatedScene& scene,
        const evaluation::PropertyEvaluationView& properties,
        SourceGeometryProjectionWorkspace& workspace) const;

    // Convenience path for one-shot tools. It prepares a temporary workspace
    // and therefore is not the steady-state API.
    [[nodiscard]] SourceGeometryProjectionResult project(
        runtime::EvaluatedScene& scene,
        const evaluation::PropertyEvaluationView& properties) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace avemotion::render
