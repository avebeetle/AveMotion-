#pragma once

#include "avemotion/model/AssetModel.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace avemotion::evaluation {

// The Part 10 evaluator is backend-neutral. It reads immutable authored tables
// and publishes retained value buffers; it never calls rlottie or a renderer.
enum class PropertyStorageKind : std::uint8_t {
    Invalid,
    Materialized,
    AssetReference,
    Unsupported,
};

struct MotionPropertyValue final {
    model::PropertyValueType type = model::PropertyValueType::None;
    PropertyStorageKind storage = PropertyStorageKind::Invalid;
    model::MotionValueRef assetReference;
    float scalar = 0.0F;
    model::MotionVec2Value vec2;
    model::MotionColorValue color;
    model::MotionMatrix3x2Value matrix;
    // For materialized Shape values this indexes PropertyEvaluationView::shapes.
    // Static shapes remain AssetReference values and do not consume instance storage.
    std::uint32_t shapeSlot = model::kInvalidModelId;

    [[nodiscard]] bool materialized() const noexcept {
        return storage == PropertyStorageKind::Materialized;
    }

    [[nodiscard]] bool supported() const noexcept {
        return storage == PropertyStorageKind::Materialized
            || storage == PropertyStorageKind::AssetReference;
    }

    [[nodiscard]] friend bool operator==(
        const MotionPropertyValue&,
        const MotionPropertyValue&) noexcept = default;
};

struct EvaluatedShape final {
    model::PropertyId property;
    std::uint32_t firstPoint = 0;
    std::uint32_t pointCount = 0;
    std::uint32_t pointCapacity = 0;
    bool closed = false;
    bool topologyTruncated = false;
    std::uint64_t contentHash = 0;
    std::uint64_t revision = 0;
    bool changed = false;
};

struct EvaluatedProperty final {
    model::PropertyId id;
    model::SourceNodeId owner;
    model::PropertySemantic semantic = model::PropertySemantic::Unknown;
    std::uint16_t semanticIndex = 0;
    MotionPropertyValue value;
    model::SegmentId activeSegment;
    // Spatial motion paths publish the tangent angle used by Lottie
    // auto-orient. This is evaluation metadata, not part of the authored
    // property value itself.
    float spatialAngleDegrees = 0.0F;
    bool spatialAngleValid = false;
    std::uint64_t revision = 0;
    bool changed = false;
};

struct MotionTrackCursor final {
    model::SegmentId segment;
    double lastFrame = 0.0;
    bool valid = false;
};

enum class NodeTransformState : std::uint8_t {
    NotPresent,
    Evaluated2D,
    StaticMatrix,
    Unsupported,
};

struct EvaluatedNodeTransform final {
    model::SourceNodeId node;
    model::MotionMatrix3x2Value localMatrix;
    float localOpacity = 1.0F;
    NodeTransformState state = NodeTransformState::NotPresent;
    std::uint64_t revision = 0;
    bool changed = false;

    // World values are resolved in canonical composition space. Matrix
    // inheritance follows transform-parent links for layers and structural
    // parent links for shape/content nodes. Opacity always follows the
    // structural parent, matching rlottie/After Effects parenting semantics.
    model::MotionMatrix3x2Value worldMatrix;
    float worldOpacity = 1.0F;
    NodeTransformState worldState = NodeTransformState::NotPresent;
    std::uint64_t worldRevision = 0;
    bool worldChanged = false;

    [[nodiscard]] bool supported() const noexcept {
        return state == NodeTransformState::Evaluated2D
            || state == NodeTransformState::StaticMatrix;
    }

    [[nodiscard]] bool worldSupported() const noexcept {
        return worldState == NodeTransformState::Evaluated2D
            || worldState == NodeTransformState::StaticMatrix;
    }
};

struct PropertyEvaluationStatistics final {
    std::size_t propertiesVisited = 0;
    std::size_t staticProperties = 0;
    std::size_t animatedProperties = 0;
    std::size_t materializedProperties = 0;
    std::size_t assetReferences = 0;
    std::size_t unsupportedProperties = 0;
    std::size_t changedProperties = 0;
    std::size_t cursorHits = 0;
    std::size_t adjacentCursorMoves = 0;
    std::size_t binarySearches = 0;
    std::size_t beforeFirstSamples = 0;
    std::size_t afterLastSamples = 0;
    std::size_t gapSamples = 0;
    std::size_t holdSamples = 0;
    std::size_t linearSamples = 0;
    std::size_t cubicSamples = 0;
    std::size_t spatialSamples = 0;
    std::size_t spatialAngleSamples = 0;
    std::size_t spatialLengthSearchIterations = 0;
    std::size_t spatialLengthSearchMaximum = 0;
    std::size_t staticShapeReferences = 0;
    std::size_t animatedShapeSamples = 0;
    std::size_t shapePointInterpolations = 0;
    std::size_t shapeTopologyTruncations = 0;
    std::size_t changedShapes = 0;
    std::size_t transformsVisited = 0;
    std::size_t transformsEvaluated = 0;
    std::size_t transformsUnsupported = 0;
    std::size_t transformsChanged = 0;
    std::size_t worldTransformsEvaluated = 0;
    std::size_t worldTransformsUnsupported = 0;
    std::size_t worldTransformsChanged = 0;
    std::size_t transformParentEdges = 0;
    std::size_t structuralParentEdges = 0;
};

enum class PropertyEvaluationErrorCode : std::uint8_t {
    None,
    InvalidModel,
    WorkspaceNotPrepared,
    NonFiniteFrame,
    CorruptProperty,
    CorruptTrack,
    CorruptSegment,
    SpatialSearchDidNotConverge,
    CorruptHierarchy,
    CorruptShapeStorage,
};

struct PropertyEvaluationView final {
    double requestedFrame = 0.0;
    std::uint64_t sequence = 0;
    std::span<const EvaluatedProperty> properties;
    std::span<const EvaluatedNodeTransform> nodeTransforms;
    // Animated shape properties reference retained point storage through
    // MotionPropertyValue::shapeSlot. The spans remain valid until this
    // workspace is evaluated or prepared again.
    std::span<const EvaluatedShape> shapes;
    std::span<const model::MotionVec2Value> shapePoints;
    PropertyEvaluationStatistics statistics;
    PropertyEvaluationErrorCode error = PropertyEvaluationErrorCode::None;

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == PropertyEvaluationErrorCode::None;
    }
};

// Mutable per-instance/per-evaluation-stream storage. A workspace is not
// thread-safe and must not be shared by concurrent evaluate() calls. prepare()
// performs all vector sizing; subsequent evaluate() calls do not resize it.
class PropertyEvaluationWorkspace final {
public:
    PropertyEvaluationWorkspace();
    PropertyEvaluationWorkspace(PropertyEvaluationWorkspace&&) noexcept;
    PropertyEvaluationWorkspace& operator=(PropertyEvaluationWorkspace&&) noexcept;
    PropertyEvaluationWorkspace(const PropertyEvaluationWorkspace&) = delete;
    PropertyEvaluationWorkspace& operator=(const PropertyEvaluationWorkspace&) = delete;
    ~PropertyEvaluationWorkspace();

    void resetHistory() noexcept;

    [[nodiscard]] std::size_t propertyCount() const noexcept;
    [[nodiscard]] std::size_t trackCursorCount() const noexcept;
    [[nodiscard]] std::size_t nodeTransformCount() const noexcept;
    [[nodiscard]] std::size_t shapeCount() const noexcept;
    [[nodiscard]] std::size_t shapePointCapacity() const noexcept;
    [[nodiscard]] std::size_t retainedBytes() const noexcept;
    [[nodiscard]] std::uint64_t storageGeneration() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    friend class PropertyEvaluator;
};

// Immutable evaluator compiled from one MotionAssetModel. The evaluator may
// be shared across threads when every thread/instance owns a separate
// PropertyEvaluationWorkspace.
class PropertyEvaluator final {
public:
    explicit PropertyEvaluator(
        std::shared_ptr<const model::MotionAssetModel> model);
    PropertyEvaluator(PropertyEvaluator&&) noexcept;
    PropertyEvaluator& operator=(PropertyEvaluator&&) noexcept;
    PropertyEvaluator(const PropertyEvaluator&) = delete;
    PropertyEvaluator& operator=(const PropertyEvaluator&) = delete;
    ~PropertyEvaluator();

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] std::string_view errorMessage() const noexcept;
    [[nodiscard]] const std::shared_ptr<const model::MotionAssetModel>& model() const noexcept;

    // Binds and preallocates a workspace. Calling prepare() resets its history.
    void prepare(PropertyEvaluationWorkspace& workspace) const;

    // Evaluates an exact source/authoring frame. Returned spans remain valid
    // only until the same workspace is prepared, reset or evaluated again.
    // Part 10 additionally materializes animated Shape tracks into retained
    // instance-local point buffers with Telegram-compatible interpolation and
    // geometry revisions. Animated Gradient values remain explicit Unsupported
    // values rather than approximations.
    [[nodiscard]] PropertyEvaluationView evaluate(
        double assetFrame,
        PropertyEvaluationWorkspace& workspace) const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace avemotion::evaluation
