#pragma once

#include "avemotion/model/Ids.hpp"
#include "avemotion/runtime/EvaluatedScene.hpp"
#include "avemotion/runtime/Handles.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace avemotion::model {

enum class ResourceClass : std::uint8_t {
    Unknown,
    InstanceEvaluated,
    AssetStatic,
};

enum StaticDependencyBits : std::uint32_t {
    StaticDependencyNone = 0,
    StaticDependencyTransform = 1U << 0U,
    StaticDependencyGeometry = 1U << 1U,
    StaticDependencyPaint = 1U << 2U,
    StaticDependencyVisibility = 1U << 3U,
    StaticDependencyMask = 1U << 4U,
    StaticDependencyMatte = 1U << 5U,
    StaticDependencyTimeline = 1U << 6U,
};

// The current render-facing layer/node tables retain the stable source IDs
// published by the Telegram runtime seam. Part 7 adds a separate parsed source
// graph so immutable authored properties no longer have to be discovered by
// sampling evaluated frames.
struct MotionLayerRecord final {
    bool present = false;
    LayerId id;
    LayerId parent;
    IndexRange children;
    IndexRange nodes;
    IndexRange masks;
    std::uint64_t nameHash = 0;
    std::string debugName;
    std::uint32_t dependencyBits = StaticDependencyNone;
    runtime::MatteMode matte = runtime::MatteMode::None;
};

struct MotionGeometryRecord final {
    bool present = false;
    GeometryId id;
    std::uint64_t contentHash = 0;
    ResourceClass resourceClass = ResourceClass::Unknown;
    std::optional<runtime::CanonicalGeometry> staticValue;
};

struct MotionPaintRecord final {
    bool present = false;
    PaintId id;
    std::uint64_t contentHash = 0;
    ResourceClass resourceClass = ResourceClass::Unknown;
    std::optional<runtime::CanonicalPaint> staticValue;
};

struct MotionNodeRecord final {
    bool present = false;
    NodeId id;
    DrawItemId drawItem;
    LayerId layer;
    GeometryId geometry;
    PaintId paint;
    std::uint32_t drawOrder = 0;
    std::uint32_t dependencyBits = StaticDependencyNone;
};

enum class ClipLoopHint : std::uint8_t {
    Once,
    Loop,
};

struct MotionClipRecord final {
    bool present = false;
    ClipId id;
    std::string debugName;
    double firstFrame = 0.0;
    double endFrame = 0.0; // half-open
    ClipLoopHint defaultLoop = ClipLoopHint::Loop;
};

enum class SourceNodeKind : std::uint8_t {
    Composition,
    Layer,
    ShapeGroup,
    Fill,
    Stroke,
    GradientFill,
    GradientStroke,
    Rectangle,
    Ellipse,
    Shape,
    Polystar,
    Trim,
    Repeater,
    Mask,
    Unknown,
};

enum class SourceLayerKind : std::uint8_t {
    None,
    Precomposition,
    Solid,
    Image,
    Null,
    Shape,
    Text,
    Unknown,
};


enum class SourceFillRule : std::uint8_t {
    Winding,
    EvenOdd,
};

enum class SourceStrokeCap : std::uint8_t {
    Flat,
    Square,
    Round,
};

enum class SourceStrokeJoin : std::uint8_t {
    Miter,
    Bevel,
    Round,
};

enum class SourceGradientType : std::uint8_t {
    None,
    Linear,
    Radial,
};

enum class SourceMaskMode : std::uint8_t {
    None,
    Add,
    Subtract,
    Intersect,
    Difference,
};

enum class SourceMatteMode : std::uint8_t {
    None,
    Alpha,
    AlphaInverted,
    Luma,
    LumaInverted,
};

enum class SourceBlendMode : std::uint8_t {
    Normal,
    Multiply,
    Screen,
    Overlay,
};

enum class SourcePathDirection : std::uint8_t {
    Clockwise,
    CounterClockwise,
};

enum class SourcePolystarType : std::uint8_t {
    None,
    Star,
    Polygon,
};

enum class SourceTrimMode : std::uint8_t {
    None,
    Simultaneous,
    Individual,
};

enum class PropertyValueType : std::uint8_t {
    None,
    Scalar,
    Vec2,
    Color,
    Matrix3x2,
    Shape,
    Gradient,
};

enum class PropertySemantic : std::uint16_t {
    Unknown,
    TransformMatrix,
    TransformPosition,
    TransformPositionX,
    TransformPositionY,
    TransformScale,
    TransformRotation,
    TransformRotationX,
    TransformRotationY,
    TransformRotationZ,
    TransformAnchor,
    TransformOpacity,
    LayerTimeRemap,
    FillColor,
    FillOpacity,
    StrokeColor,
    StrokeOpacity,
    StrokeWidth,
    StrokeDashValue,
    GradientStart,
    GradientEnd,
    GradientHighlightLength,
    GradientHighlightAngle,
    GradientOpacity,
    GradientStops,
    ShapePath,
    RectanglePosition,
    RectangleSize,
    RectangleRoundness,
    EllipsePosition,
    EllipseSize,
    PolystarPosition,
    PolystarPointCount,
    PolystarInnerRadius,
    PolystarOuterRadius,
    PolystarInnerRoundness,
    PolystarOuterRoundness,
    PolystarRotation,
    TrimStart,
    TrimEnd,
    TrimOffset,
    RepeaterCopies,
    RepeaterOffset,
    RepeaterPosition,
    RepeaterScale,
    RepeaterRotation,
    RepeaterAnchor,
    RepeaterStartOpacity,
    RepeaterEndOpacity,
    MaskPath,
    MaskOpacity,
};

enum class SegmentInterpolation : std::uint8_t {
    Hold,
    Linear,
    CubicBezier,
};

enum class SpatialInterpolation : std::uint8_t {
    None,
    CubicBezier,
};

enum PropertyFlags : std::uint32_t {
    PropertyFlagNone = 0,
    PropertyFlagStatic = 1U << 0U,
    PropertyFlagAnimated = 1U << 1U,
    PropertyFlagSpatial = 1U << 2U,
    PropertyFlagSeparatedDimension = 1U << 3U,
};

struct MotionVec2Value final {
    float x = 0.0F;
    float y = 0.0F;

    [[nodiscard]] friend bool operator==(
        const MotionVec2Value&,
        const MotionVec2Value&) noexcept = default;
};

struct MotionColorValue final {
    float r = 0.0F;
    float g = 0.0F;
    float b = 0.0F;
    float a = 1.0F;

    [[nodiscard]] friend bool operator==(
        const MotionColorValue&,
        const MotionColorValue&) noexcept = default;
};

struct MotionMatrix3x2Value final {
    float m11 = 1.0F;
    float m12 = 0.0F;
    float m21 = 0.0F;
    float m22 = 1.0F;
    float dx = 0.0F;
    float dy = 0.0F;

    [[nodiscard]] friend bool operator==(
        const MotionMatrix3x2Value&,
        const MotionMatrix3x2Value&) noexcept = default;
};

struct MotionValueRef final {
    PropertyValueType type = PropertyValueType::None;
    std::uint32_t index = kInvalidModelId;

    [[nodiscard]] bool valid() const noexcept {
        return type != PropertyValueType::None && index != kInvalidModelId;
    }

    [[nodiscard]] friend bool operator==(
        const MotionValueRef&,
        const MotionValueRef&) noexcept = default;
};

struct MotionShapeValueRecord final {
    IndexRange points;
    bool closed = false;
};

struct MotionGradientValueRecord final {
    IndexRange values;
};

struct MotionCompositionRecord final {
    bool present = false;
    CompositionId id;
    SourceNodeId rootNode;
    std::string debugName;
    std::size_t logicalWidth = 0;
    std::size_t logicalHeight = 0;
    double firstFrame = 0.0;
    double endFrame = 0.0;
    double frameRate = 0.0;
};

struct MotionSourceNodeRecord final {
    bool present = false;
    SourceNodeId id;
    SourceNodeId parent;
    SourceNodeId transformParent;
    CompositionId composition;
    CompositionId referencedComposition;
    SourceNodeKind kind = SourceNodeKind::Unknown;
    SourceLayerKind layerKind = SourceLayerKind::None;
    IndexRange children;
    IndexRange properties;
    std::uint64_t nameHash = 0;
    std::string debugName;
    bool hidden = false;
    bool authoredStatic = false;
    bool autoOrient = false;
    std::int32_t authoredLayerId = -1;
    std::int32_t authoredParentLayerId = -1;
    double inFrame = 0.0;
    double outFrame = 0.0;
    double startFrame = 0.0;
    float timeStretch = 1.0F;
    std::uint32_t dependencyBits = StaticDependencyNone;

    // Non-animatable authored semantics required by a future standalone
    // evaluator. Animated values remain in the typed property/track tables.
    SourceFillRule fillRule = SourceFillRule::Winding;
    SourceStrokeCap strokeCap = SourceStrokeCap::Flat;
    SourceStrokeJoin strokeJoin = SourceStrokeJoin::Miter;
    SourceGradientType gradientType = SourceGradientType::None;
    SourceMaskMode maskMode = SourceMaskMode::None;
    SourceMatteMode matteMode = SourceMatteMode::None;
    SourceBlendMode blendMode = SourceBlendMode::Normal;
    SourcePathDirection pathDirection = SourcePathDirection::Clockwise;
    SourcePolystarType polystarType = SourcePolystarType::None;
    SourceTrimMode trimMode = SourceTrimMode::None;
    float miterLimit = 0.0F;
    float repeaterMaximumCopies = 0.0F;
    std::int32_t gradientColorPointCount = 0;
    std::int32_t layerWidth = 0;
    std::int32_t layerHeight = 0;
    MotionColorValue solidColor;
    std::uint64_t sourceAssetRefHash = 0;
    bool enabled = true;
    bool maskInverted = false;
};

struct MotionPropertyRecord final {
    bool present = false;
    PropertyId id;
    SourceNodeId owner;
    PropertySemantic semantic = PropertySemantic::Unknown;
    std::uint16_t semanticIndex = 0;
    PropertyValueType valueType = PropertyValueType::None;
    std::uint32_t flags = PropertyFlagNone;
    MotionValueRef staticValue;
    TrackId track;
};

struct MotionTrackRecord final {
    bool present = false;
    TrackId id;
    PropertyId property;
    IndexRange segments;
    double firstFrame = 0.0;
    double endFrame = 0.0;
};

struct MotionSegmentRecord final {
    bool present = false;
    SegmentId id;
    TrackId track;
    double firstFrame = 0.0;
    double endFrame = 0.0;
    SegmentInterpolation interpolation = SegmentInterpolation::Hold;
    SpatialInterpolation spatialInterpolation = SpatialInterpolation::None;
    MotionValueRef startValue;
    MotionValueRef endValue;
    MotionVec2Value temporalControl1{0.0F, 0.0F};
    MotionVec2Value temporalControl2{1.0F, 1.0F};
    MotionVec2Value spatialInTangent;
    MotionVec2Value spatialOutTangent;
};

struct MotionAssetModelStatistics final {
    std::size_t declaredLayerCount = 0;
    std::size_t declaredNodeCount = 0;
    std::size_t declaredGeometryCount = 0;
    std::size_t declaredPaintCount = 0;
    std::size_t observedLayerCount = 0;
    std::size_t observedNodeCount = 0;
    std::size_t observedGeometryCount = 0;
    std::size_t observedPaintCount = 0;
    std::size_t assetStaticGeometryCount = 0;
    std::size_t assetStaticPaintCount = 0;
    std::size_t maskCount = 0;
    std::size_t clipCount = 0;

    bool directParsedModel = false;
    std::size_t compositionCount = 0;
    std::size_t sourceNodeCount = 0;
    std::size_t propertyCount = 0;
    std::size_t staticPropertyCount = 0;
    std::size_t animatedPropertyCount = 0;
    std::size_t trackCount = 0;
    std::size_t segmentCount = 0;
    std::size_t scalarValueCount = 0;
    std::size_t vec2ValueCount = 0;
    std::size_t colorValueCount = 0;
    std::size_t matrixValueCount = 0;
    std::size_t shapeValueCount = 0;
    std::size_t gradientValueCount = 0;
};

struct MotionAssetModel final {
    static constexpr std::uint32_t kSchemaVersion = 2U;

    std::uint32_t schemaVersion = kSchemaVersion;
    std::uint64_t revision = 0;
    runtime::AssetHandle assetHandle;
    std::uint64_t sourceAssetHash = 0;
    std::size_t logicalWidth = 0;
    std::size_t logicalHeight = 0;
    double frameRate = 0.0;
    std::size_t totalFrames = 0;
    std::string debugName;

    // Render-facing stable tables populated by the Telegram runtime seam.
    std::vector<MotionLayerRecord> layers;
    std::vector<LayerId> childLayerIds;
    std::vector<NodeId> layerNodeIds;
    std::vector<MotionNodeRecord> nodes;
    std::vector<MotionGeometryRecord> geometries;
    std::vector<MotionPaintRecord> paints;
    std::vector<NodeId> drawOrder;
    std::vector<MotionClipRecord> clips;

    // Direct parsed-model tables. These are immutable authored data and do not
    // depend on which frames have been evaluated by a MotionInstance.
    std::vector<MotionCompositionRecord> compositions;
    std::vector<MotionSourceNodeRecord> sourceNodes;
    std::vector<SourceNodeId> sourceChildIds;
    std::vector<PropertyId> sourcePropertyIds;
    std::vector<MotionPropertyRecord> properties;
    std::vector<MotionTrackRecord> tracks;
    std::vector<MotionSegmentRecord> segments;

    std::vector<float> scalarValues;
    std::vector<MotionVec2Value> vec2Values;
    std::vector<MotionColorValue> colorValues;
    std::vector<MotionMatrix3x2Value> matrixValues;
    std::vector<MotionShapeValueRecord> shapeValues;
    std::vector<MotionVec2Value> shapePoints;
    std::vector<MotionGradientValueRecord> gradientValues;
    std::vector<float> gradientFloats;

    MotionAssetModelStatistics statistics;
    std::uint64_t parsedModelFingerprint = 0;
    std::uint64_t topologyFingerprint = 0;
    std::uint64_t resourceFingerprint = 0;
    std::uint64_t fingerprint = 0;

    [[nodiscard]] const MotionLayerRecord* layer(LayerId id) const noexcept;
    [[nodiscard]] const MotionNodeRecord* node(NodeId id) const noexcept;
    [[nodiscard]] const MotionGeometryRecord* geometry(GeometryId id) const noexcept;
    [[nodiscard]] const MotionPaintRecord* paint(PaintId id) const noexcept;
    [[nodiscard]] const MotionClipRecord* clip(ClipId id) const noexcept;
    [[nodiscard]] const MotionCompositionRecord* composition(
        CompositionId id) const noexcept;
    [[nodiscard]] const MotionSourceNodeRecord* sourceNode(
        SourceNodeId id) const noexcept;
    [[nodiscard]] const MotionPropertyRecord* property(PropertyId id) const noexcept;
    [[nodiscard]] const MotionTrackRecord* track(TrackId id) const noexcept;
    [[nodiscard]] const MotionSegmentRecord* segment(SegmentId id) const noexcept;
};

struct AssetModelResult final {
    std::shared_ptr<const MotionAssetModel> model;
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept {
        return model != nullptr && error.empty();
    }
};

} // namespace avemotion::model
