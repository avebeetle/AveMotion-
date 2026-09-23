#pragma once

#include "avemotion/model/Ids.hpp"
#include "avemotion/runtime/EvaluatedScene.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace avemotion::render {

struct Matrix3x2 final {
    float m11 = 1.0F;
    float m12 = 0.0F;
    float m21 = 0.0F;
    float m22 = 1.0F;
    float dx = 0.0F;
    float dy = 0.0F;

    [[nodiscard]] static constexpr Matrix3x2 identity() noexcept { return {}; }
};

struct PresentationState final {
    Matrix3x2 transform;
    float opacity = 1.0F;
    bool visible = true;
    std::uint64_t layoutRevision = 0;
};

enum class ResourceIdentityScope : std::uint8_t {
    Asset,
    Instance,
};

struct GeometryCacheKey final {
    ResourceIdentityScope scope = ResourceIdentityScope::Instance;
    std::uint64_t assetHash = 0;
    std::uint64_t assetIdentity = 0;
    std::uint64_t instanceId = 0;
    std::uint64_t instanceIdentity = 0;
    // Legacy/source identity remains part of the stable oracle schema.
    // resourceId is the typed canonical identity used by model-aware paths.
    std::uint64_t sourceKey = 0;
    model::GeometryId resourceId;
    std::uint64_t contentHash = 0;
    std::uint64_t revision = 0;

    [[nodiscard]] friend constexpr bool operator==(
        const GeometryCacheKey&,
        const GeometryCacheKey&) noexcept = default;
};

struct PaintCacheKey final {
    ResourceIdentityScope scope = ResourceIdentityScope::Instance;
    std::uint64_t assetHash = 0;
    std::uint64_t assetIdentity = 0;
    std::uint64_t instanceId = 0;
    std::uint64_t instanceIdentity = 0;
    std::uint64_t sourceKey = 0;
    model::PaintId resourceId;
    std::uint64_t contentHash = 0;
    std::uint64_t revision = 0;

    [[nodiscard]] friend constexpr bool operator==(
        const PaintCacheKey&,
        const PaintCacheKey&) noexcept = default;
};

enum RenderFeatureBits : std::uint32_t {
    RenderFeatureNone = 0,
    RenderFeatureSolidPaint = 1U << 0U,
    RenderFeatureGradientPaint = 1U << 1U,
    RenderFeatureImagePaint = 1U << 2U,
    RenderFeatureStroke = 1U << 3U,
    RenderFeatureClip = 1U << 4U,
    RenderFeatureMask = 1U << 5U,
    RenderFeatureMatte = 1U << 6U,
    RenderFeatureDash = 1U << 7U,
};

struct RenderPlanStamp final {
    std::uint64_t assetHash = 0;
    std::uint64_t assetIdentity = 0;
    std::uint64_t instanceId = 0;
    std::uint64_t instanceIdentity = 0;
    std::uint64_t evaluationSequence = 0;
    std::uint64_t planSequence = 0;
    std::uint64_t layoutRevision = 0;
};

struct MotionDrawItem final {
    model::DrawItemId drawItem;
    model::NodeId node;
    std::uint64_t sourceItemKey = 0;
    std::uint32_t sourceDrawItemIndex = runtime::kInvalidSceneIndex;
    std::uint32_t drawOrder = 0;
    GeometryCacheKey geometry;
    PaintCacheKey paint;
    Matrix3x2 geometryTransform;
    Matrix3x2 presentationTransform;
    float effectiveOpacity = 1.0F;
    runtime::RectF presentedBounds;
    std::uint32_t featureBits = RenderFeatureNone;
};

struct MotionGeometryUpdate final {
    std::uint32_t planDrawItemIndex = runtime::kInvalidSceneIndex;
    GeometryCacheKey key;
    std::uint32_t sourceDrawItemIndex = runtime::kInvalidSceneIndex;
};

struct MotionPaintUpdate final {
    std::uint32_t planDrawItemIndex = runtime::kInvalidSceneIndex;
    PaintCacheKey key;
    std::uint32_t sourceDrawItemIndex = runtime::kInvalidSceneIndex;
};

struct RenderPlanStatistics final {
    std::size_t sourceDrawItemCount = 0;
    std::size_t visibleDrawItemCount = 0;
    std::size_t geometryUpdateCount = 0;
    std::size_t paintUpdateCount = 0;
    std::size_t assetStaticGeometryCount = 0;
    std::size_t instanceGeometryCount = 0;
    std::size_t assetStaticPaintCount = 0;
    std::size_t instancePaintCount = 0;
    std::size_t unsupportedFeatureItemCount = 0;
};

struct RenderPlanFingerprints final {
    std::uint64_t plan = 0;
    std::uint64_t topology = 0;
    std::uint64_t geometryIdentity = 0;
    std::uint64_t paintIdentity = 0;
    std::uint64_t presentation = 0;
};

struct MotionRenderPlan final {
    RenderPlanStamp stamp;
    std::shared_ptr<const runtime::EvaluatedScene> sourceScene;
    std::vector<MotionDrawItem> drawItems;
    std::vector<MotionGeometryUpdate> geometryUpdates;
    std::vector<MotionPaintUpdate> paintUpdates;
    runtime::RectF presentedBounds;
    runtime::RectF dirtyRegion;
    RenderPlanStatistics statistics;
    RenderPlanFingerprints fingerprints;
    bool firstPlan = true;
    bool visualChanged = true;
    bool reusableForRepaint = true;
};

} // namespace avemotion::render
