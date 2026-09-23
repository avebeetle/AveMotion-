#pragma once

#include "avemotion/model/Ids.hpp"
#include "avemotion/runtime/Handles.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace avemotion::model { struct MotionAssetModel; }

namespace avemotion::runtime {

inline constexpr std::uint32_t kInvalidSceneIndex =
    std::numeric_limits<std::uint32_t>::max();

struct Vec2 final {
    float x = 0.0F;
    float y = 0.0F;
};

struct RectF final {
    bool valid = false;
    float left = 0.0F;
    float top = 0.0F;
    float right = 0.0F;
    float bottom = 0.0F;
};

struct AffineTransform final {
    float m11 = 1.0F;
    float m12 = 0.0F;
    float m21 = 0.0F;
    float m22 = 1.0F;
    float dx = 0.0F;
    float dy = 0.0F;

    [[nodiscard]] static constexpr AffineTransform identity() noexcept {
        return {};
    }
};

enum class PathVerb : std::uint8_t {
    MoveTo,
    LineTo,
    CubicTo,
    Close,
};

struct EvaluatedPath final {
    std::vector<PathVerb> verbs;
    std::vector<Vec2> points;
    RectF controlBounds;
    std::uint64_t hash = 0;
};

enum class FillRule : std::uint8_t {
    EvenOdd,
    Winding,
};

enum class LineCap : std::uint8_t {
    Flat,
    Square,
    Round,
};

enum class LineJoin : std::uint8_t {
    Miter,
    Bevel,
    Round,
};

enum class GradientKind : std::uint8_t {
    Linear,
    Radial,
};

enum class PaintKind : std::uint8_t {
    None,
    Solid,
    Gradient,
    Image,
};

struct Color8 final {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 0;
};

struct EvaluatedGradientStop final {
    float position = 0.0F;
    Color8 color;
};

struct EvaluatedGradient final {
    GradientKind kind = GradientKind::Linear;
    Vec2 start;
    Vec2 end;
    Vec2 center;
    Vec2 focal;
    float centerRadius = 0.0F;
    float focalRadius = 0.0F;
    std::vector<EvaluatedGradientStop> stops;
};

struct EvaluatedStroke final {
    bool enabled = false;
    float width = 0.0F;
    float miterLimit = 0.0F;
    LineCap cap = LineCap::Flat;
    LineJoin join = LineJoin::Miter;
    std::vector<float> dashArray;
};

struct EvaluatedImage final {
    bool present = false;
    std::size_t width = 0;
    std::size_t height = 0;
    float matrix[9] = {};
};

struct EvaluatedPaint final {
    PaintKind kind = PaintKind::None;
    Color8 solid;
    EvaluatedGradient gradient;
    EvaluatedImage image;
};

struct CanonicalGeometry final {
    std::uint64_t sourceKey = 0;
    std::uint64_t contentHash = 0;
    FillRule fillRule = FillRule::Winding;
    EvaluatedPath path;
};

struct CanonicalPaint final {
    std::uint64_t sourceKey = 0;
    std::uint64_t contentHash = 0;
    EvaluatedStroke stroke;
    EvaluatedPaint paint;
};

enum class MaskMode : std::uint8_t {
    Add,
    Subtract,
    Intersect,
    Difference,
};

enum class MatteMode : std::uint8_t {
    None,
    Alpha,
    AlphaInverted,
    Luma,
    LumaInverted,
};

enum class EvaluatedValueOrigin : std::uint8_t {
    Unknown,
    AssetStatic,
    InstanceEvaluated,
};

enum SceneChangeBits : std::uint32_t {
    SceneChangeNone = 0,
    SceneChangeGeometry = 1U << 0U,
    SceneChangePaint = 1U << 1U,
};

struct EvaluatedMask final {
    std::uint32_t layerIndex = kInvalidSceneIndex;
    MaskMode mode = MaskMode::Add;
    float opacity = 1.0F;
    EvaluatedPath path;
};

struct EvaluatedDrawItem final {
    model::DrawItemId modelDrawItem;
    model::NodeId modelNode;
    model::GeometryId modelGeometry;
    model::PaintId modelPaint;
    std::uint64_t sourceGeometryId = 0;
    std::uint64_t sourcePaintId = 0;
    // Parsed-model binding published by the Telegram extraction seam. These
    // IDs address MotionAssetModel::sourceNodes, not render-facing draw IDs.
    model::SourceNodeId sourcePathNode;
    model::SourceNodeId sourcePaintNode;
    std::uint32_t sourcePathCount = 0;
    bool sourcePathModifierFree = false;
    // When SourceGeometryProjector accepts this item, localPath is backed by
    // AveMotion property-evaluation storage rather than Telegram geometry.
    bool sourceGeometryProjected = false;
    // When this item is one generated Repeater paint/copy application, the
    // base geometry and authored paint identity remain shareable while
    // transform and opacity are evaluated per copy by AveMotion.
    bool sourceRepeaterProjected = false;
    model::SourceNodeId sourceRepeaterNode;
    std::uint32_t sourceRepeaterCopyIndex = 0;
    std::uint32_t sourceRepeaterVisibleCopies = 0;
    std::uint32_t sourceRepeaterMaximumCopies = 0;
    std::uint64_t sourceRepeaterTransformRevision = 0;
    std::uint64_t sourceRepeaterOpacityRevision = 0;
    std::uint64_t sourceGeometryRevision = 0;
    bool sourceGeometryRevisionAuthoritative = false;
    EvaluatedValueOrigin geometryOrigin = EvaluatedValueOrigin::InstanceEvaluated;
    EvaluatedValueOrigin paintOrigin = EvaluatedValueOrigin::InstanceEvaluated;
    std::uint32_t layerIndex = kInvalidSceneIndex;
    std::uint32_t drawOrder = 0;
    std::uint32_t upstreamChangeBits = SceneChangeNone;
    FillRule fillRule = FillRule::Winding;
    EvaluatedStroke stroke;
    EvaluatedPaint paint;
    EvaluatedPath path;
    bool localGeometryAvailable = false;
    bool localGeometryStaticCandidate = false;
    AffineTransform localToViewport;
    EvaluatedPath localPath;
    bool localPaintAvailable = false;
    bool localPaintStaticCandidate = false;
    EvaluatedStroke localStroke;
    EvaluatedPaint localPaint;
    bool opacitySeparated = false;
    float separatedOpacity = 1.0F;
    std::shared_ptr<const CanonicalGeometry> canonicalGeometry;
    std::shared_ptr<const CanonicalPaint> canonicalPaint;
};

struct EvaluatedLayer final {
    model::LayerId modelLayer;
    std::uint32_t parentLayer = kInvalidSceneIndex;
    std::uint32_t firstChildReference = 0;
    std::uint32_t childCount = 0;
    std::uint32_t firstDrawItem = 0;
    std::uint32_t drawItemCount = 0;
    std::uint32_t firstMask = 0;
    std::uint32_t maskCount = 0;
    std::string keyPath;
    bool visible = false;
    float opacity = 1.0F;
    MatteMode matte = MatteMode::None;
    EvaluatedPath clipPath;
};

struct SceneStatistics final {
    std::size_t layerCount = 0;
    std::size_t visibleLayerCount = 0;
    std::size_t drawItemCount = 0;
    std::size_t maskCount = 0;
    std::size_t clipPathCount = 0;
    std::size_t pathVerbCount = 0;
    std::size_t pathPointCount = 0;
    std::size_t gradientStopCount = 0;
    std::size_t solidPaintCount = 0;
    std::size_t gradientPaintCount = 0;
    std::size_t imagePaintCount = 0;
};

struct SceneFingerprints final {
    std::uint64_t scene = 0;
    std::uint64_t topology = 0;
    std::uint64_t geometry = 0;
    std::uint64_t paint = 0;
};

struct SceneChangeSummary final {
    bool firstEvaluation = true;
    bool topologyChanged = true;
    bool geometryChanged = true;
    bool paintChanged = true;
    bool visualChanged = true;
};

struct EvaluatedScene final {
    std::uint64_t sourceAssetHash = 0;
    std::uint64_t instanceId = 0;
    AssetHandle assetHandle;
    InstanceHandle instanceHandle;
    std::uint64_t evaluationSequence = 0;
    std::size_t frameIndex = 0;
    std::size_t viewportWidth = 0;
    std::size_t viewportHeight = 0;
    std::vector<EvaluatedLayer> layers;
    std::vector<std::uint32_t> childLayerIndices;
    std::vector<EvaluatedDrawItem> drawItems;
    std::vector<EvaluatedMask> masks;
    std::uint32_t modelLayerCount = 0;
    std::uint32_t modelNodeCount = 0;
    std::uint32_t modelGeometryCount = 0;
    std::uint32_t modelPaintCount = 0;
    std::shared_ptr<const model::MotionAssetModel> assetModel;
    bool assetModelApplied = false;
    RectF controlBounds;
    SceneStatistics statistics;
    SceneFingerprints fingerprints;
    SceneChangeSummary changes;
};

} // namespace avemotion::runtime
