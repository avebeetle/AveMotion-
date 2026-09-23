#include "avemotion/render/SourceGeometryProjector.hpp"

#include "PrimitivePathGenerator.hpp"
#include "RepeaterTransform.hpp"
#include "TrimPathGenerator.hpp"
#include "avemotion/core/Hash.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace avemotion::render {
namespace {

constexpr float kParityTolerance = 2.0e-4F;

[[nodiscard]] bool finite(float value) noexcept {
    return std::isfinite(value);
}

[[nodiscard]] bool finite(model::MotionVec2Value value) noexcept {
    return finite(value.x) && finite(value.y);
}

[[nodiscard]] bool finite(const model::MotionMatrix3x2Value& matrix) noexcept {
    return finite(matrix.m11) && finite(matrix.m12)
        && finite(matrix.m21) && finite(matrix.m22)
        && finite(matrix.dx) && finite(matrix.dy);
}

[[nodiscard]] bool checkedMultiply(
    std::size_t left,
    std::size_t right,
    std::size_t& result) noexcept {
    if (left != 0U && right > std::numeric_limits<std::size_t>::max() / left) {
        return false;
    }
    result = left * right;
    return true;
}

[[nodiscard]] bool checkedAdd(
    std::size_t left,
    std::size_t right,
    std::size_t& result) noexcept {
    if (right > std::numeric_limits<std::size_t>::max() - left) {
        return false;
    }
    result = left + right;
    return true;
}

[[nodiscard]] model::MotionMatrix3x2Value multiply(
    const model::MotionMatrix3x2Value& left,
    const model::MotionMatrix3x2Value& right) noexcept {
    return {
        left.m11 * right.m11 + left.m12 * right.m21,
        left.m11 * right.m12 + left.m12 * right.m22,
        left.m21 * right.m11 + left.m22 * right.m21,
        left.m21 * right.m12 + left.m22 * right.m22,
        left.dx * right.m11 + left.dy * right.m21 + right.dx,
        left.dx * right.m12 + left.dy * right.m22 + right.dy,
    };
}

[[nodiscard]] bool inverse(
    const model::MotionMatrix3x2Value& value,
    model::MotionMatrix3x2Value& result) noexcept {
    if (!finite(value)) return false;
    const auto determinant = value.m11 * value.m22 - value.m12 * value.m21;
    if (!finite(determinant) || std::fabs(determinant) <= 1.0e-12F) {
        return false;
    }
    const auto reciprocal = 1.0F / determinant;
    result.m11 = value.m22 * reciprocal;
    result.m12 = -value.m12 * reciprocal;
    result.m21 = -value.m21 * reciprocal;
    result.m22 = value.m11 * reciprocal;
    result.dx = -(value.dx * result.m11 + value.dy * result.m21);
    result.dy = -(value.dx * result.m12 + value.dy * result.m22);
    return finite(result);
}

[[nodiscard]] runtime::Vec2 transformPoint(
    model::MotionVec2Value point,
    const model::MotionMatrix3x2Value& matrix) noexcept {
    return {
        point.x * matrix.m11 + point.y * matrix.m21 + matrix.dx,
        point.x * matrix.m12 + point.y * matrix.m22 + matrix.dy,
    };
}

void includePoint(runtime::RectF& bounds, runtime::Vec2 point) noexcept {
    if (!bounds.valid) {
        bounds = {true, point.x, point.y, point.x, point.y};
        return;
    }
    bounds.left = std::min(bounds.left, point.x);
    bounds.top = std::min(bounds.top, point.y);
    bounds.right = std::max(bounds.right, point.x);
    bounds.bottom = std::max(bounds.bottom, point.y);
}

void finalizePathHash(runtime::EvaluatedPath& path) noexcept {
    core::Fnv1a64 hash;
    hash.appendU64(path.verbs.size());
    hash.appendU64(path.points.size());
    for (const auto verb : path.verbs) {
        hash.appendU8(static_cast<std::uint8_t>(verb));
    }
    for (const auto point : path.points) {
        hash.appendFloat(point.x);
        hash.appendFloat(point.y);
    }
    path.hash = hash.value();
}

struct SourcePropertyIds final {
    model::PropertyId shape;
    model::PropertyId position;
    model::PropertyId size;
    model::PropertyId roundness;
    model::PropertyId pointCount;
    model::PropertyId innerRadius;
    model::PropertyId outerRadius;
    model::PropertyId innerRoundness;
    model::PropertyId outerRoundness;
    model::PropertyId rotation;
    model::PropertyId trimStart;
    model::PropertyId trimEnd;
    model::PropertyId trimOffset;
    model::PropertyId repeaterCopies;
    model::PropertyId repeaterOffset;
    model::PropertyId repeaterPosition;
    model::PropertyId repeaterScale;
    model::PropertyId repeaterRotation;
    model::PropertyId repeaterAnchor;
    model::PropertyId repeaterStartOpacity;
    model::PropertyId repeaterEndOpacity;
    model::PropertyId fillColor;
    model::PropertyId fillOpacity;
    model::PropertyId strokeColor;
    model::PropertyId strokeOpacity;
    model::PropertyId strokeWidth;
    bool hasStrokeDash = false;
};

struct TrimBinding final {
    model::SourceNodeId trimNode;
    model::SourceTrimMode mode = model::SourceTrimMode::None;
    model::IndexRange pathNodes;

    [[nodiscard]] bool valid() const noexcept {
        return trimNode.valid() && mode != model::SourceTrimMode::None
            && !pathNodes.empty();
    }
};

enum class RepeaterPaintKind : std::uint8_t {
    None,
    SolidFill,
    SolidStroke,
};

struct RepeaterGeometryBinding final {
    model::SourceNodeId repeaterNode;
    model::SourceNodeId contentNode;
    model::SourceNodeId paintSpaceNode;
    model::IndexRange pathNodes;
    runtime::FillRule fillRule = runtime::FillRule::Winding;
    std::uint32_t maximumCopies = 0U;
    bool relativeTransformAnimated = false;

    [[nodiscard]] bool valid() const noexcept {
        return repeaterNode.valid() && contentNode.valid()
            && paintSpaceNode.valid() && !pathNodes.empty()
            && maximumCopies != 0U;
    }
};

struct RepeaterBinding final {
    model::SourceNodeId repeaterNode;
    model::SourceNodeId contentNode;
    model::SourceNodeId paintNode;
    std::uint32_t geometryBindingIndex = model::kInvalidModelId;
    std::uint32_t maximumCopies = 0U;
    RepeaterPaintKind paintKind = RepeaterPaintKind::None;

    [[nodiscard]] bool valid() const noexcept {
        return repeaterNode.valid() && contentNode.valid() && paintNode.valid()
            && geometryBindingIndex != model::kInvalidModelId
            && maximumCopies != 0U && paintKind != RepeaterPaintKind::None;
    }
};

enum class PrimitiveKind : std::uint8_t {
    None,
    Rectangle,
    Ellipse,
    Star,
    Polygon,
};

struct CandidatePathRecord final {
    model::SourceNodeId sourceNode;
    model::IndexRange verbs;
    model::IndexRange points;
    model::MotionMatrix3x2Value relative;
    bool animated = false;
    bool roundedRectangle = false;
    PrimitiveKind primitiveKind = PrimitiveKind::None;
};

[[nodiscard]] bool pathLike(model::SourceNodeKind kind) noexcept {
    return kind == model::SourceNodeKind::Shape
        || kind == model::SourceNodeKind::Rectangle
        || kind == model::SourceNodeKind::Ellipse
        || kind == model::SourceNodeKind::Polystar;
}

[[nodiscard]] bool transformSemantic(
    model::PropertySemantic semantic) noexcept {
    switch (semantic) {
    case model::PropertySemantic::TransformMatrix:
    case model::PropertySemantic::TransformPosition:
    case model::PropertySemantic::TransformPositionX:
    case model::PropertySemantic::TransformPositionY:
    case model::PropertySemantic::TransformScale:
    case model::PropertySemantic::TransformRotation:
    case model::PropertySemantic::TransformRotationX:
    case model::PropertySemantic::TransformRotationY:
    case model::PropertySemantic::TransformRotationZ:
    case model::PropertySemantic::TransformAnchor:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool nodeTransformAnimated(
    const model::MotionAssetModel& modelValue,
    model::SourceNodeId nodeId) noexcept {
    const auto* node = modelValue.sourceNode(nodeId);
    if (!node || node->properties.end() > modelValue.sourcePropertyIds.size()) {
        return true;
    }
    for (std::uint32_t offset = 0U; offset < node->properties.count; ++offset) {
        const auto propertyId = modelValue.sourcePropertyIds[
            node->properties.first + offset];
        const auto* sourceProperty = modelValue.property(propertyId);
        if (sourceProperty && transformSemantic(sourceProperty->semantic)
            && (sourceProperty->flags & model::PropertyFlagAnimated) != 0U) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool relativeTransformAnimated(
    const model::MotionAssetModel& modelValue,
    model::SourceNodeId pathNode,
    model::SourceNodeId paintSpaceNode,
    bool& animated) noexcept {
    animated = false;
    auto current = pathNode;
    std::size_t depth = 0U;
    while (current.valid() && current != paintSpaceNode) {
        if (depth++ > modelValue.sourceNodes.size()) return false;
        animated = animated || nodeTransformAnimated(modelValue, current);
        const auto* node = modelValue.sourceNode(current);
        if (!node) return false;
        current = node->parent;
    }
    return current == paintSpaceNode;
}

[[nodiscard]] model::MotionMatrix3x2Value matrixValue(
    const runtime::AffineTransform& value) noexcept {
    return {value.m11, value.m12, value.m21, value.m22, value.dx, value.dy};
}

[[nodiscard]] runtime::AffineTransform affineValue(
    const model::MotionMatrix3x2Value& value) noexcept {
    return {value.m11, value.m12, value.m21, value.m22, value.dx, value.dy};
}

[[nodiscard]] bool nearMatrix(
    const model::MotionMatrix3x2Value& expected,
    const runtime::AffineTransform& actual) noexcept {
    const auto close = [](float left, float right) noexcept {
        return finite(left) && finite(right)
            && std::fabs(left - right) <= kParityTolerance;
    };
    return close(expected.m11, actual.m11)
        && close(expected.m12, actual.m12)
        && close(expected.m21, actual.m21)
        && close(expected.m22, actual.m22)
        && close(expected.dx, actual.dx)
        && close(expected.dy, actual.dy);
}

[[nodiscard]] model::MotionMatrix3x2Value viewportMatrix(
    const model::MotionCompositionRecord& composition,
    const runtime::EvaluatedScene& scene) noexcept {
    if (composition.logicalWidth == 0U || composition.logicalHeight == 0U
        || scene.viewportWidth == 0U || scene.viewportHeight == 0U) {
        return {};
    }
    const float sx = static_cast<float>(scene.viewportWidth)
        / static_cast<float>(composition.logicalWidth);
    const float sy = static_cast<float>(scene.viewportHeight)
        / static_cast<float>(composition.logicalHeight);
    const float scale = std::min(sx, sy);
    const float tx = (static_cast<float>(scene.viewportWidth)
        - static_cast<float>(composition.logicalWidth) * scale) * 0.5F;
    const float ty = (static_cast<float>(scene.viewportHeight)
        - static_cast<float>(composition.logicalHeight) * scale) * 0.5F;
    return {scale, 0.0F, 0.0F, scale, tx, ty};
}

[[nodiscard]] std::uint64_t matrixRevision(
    const model::MotionMatrix3x2Value& value) noexcept {
    core::Fnv1a64 hash;
    hash.appendFloat(value.m11);
    hash.appendFloat(value.m12);
    hash.appendFloat(value.m21);
    hash.appendFloat(value.m22);
    hash.appendFloat(value.dx);
    hash.appendFloat(value.dy);
    const auto result = hash.value();
    return result == 0U ? 1U : result;
}

[[nodiscard]] std::uint64_t scalarRevision(float value) noexcept {
    core::Fnv1a64 hash;
    hash.appendFloat(value);
    const auto result = hash.value();
    return result == 0U ? 1U : result;
}

[[nodiscard]] bool assignUnique(
    model::PropertyId& destination,
    model::PropertyId value) noexcept {
    if (destination.valid()) return false;
    destination = value;
    return true;
}

struct ShapeSample final {
    std::span<const model::MotionVec2Value> points;
    bool closed = false;
    bool animated = false;
};

[[nodiscard]] bool resolveShape(
    const model::MotionAssetModel& modelValue,
    const model::MotionPropertyRecord& property,
    const evaluation::PropertyEvaluationView& view,
    ShapeSample& result) noexcept {
    if (property.valueType != model::PropertyValueType::Shape
        || property.id.index() >= view.properties.size()) {
        return false;
    }
    const auto& evaluated = view.properties[property.id.index()];
    if (evaluated.id != property.id || !evaluated.value.supported()) return false;

    if (evaluated.value.storage == evaluation::PropertyStorageKind::AssetReference) {
        const auto reference = evaluated.value.assetReference;
        if (reference.type != model::PropertyValueType::Shape
            || reference.index >= modelValue.shapeValues.size()) {
            return false;
        }
        const auto& shape = modelValue.shapeValues[reference.index];
        if (shape.points.end() > modelValue.shapePoints.size()) return false;
        result.points = std::span<const model::MotionVec2Value>{
            modelValue.shapePoints.data() + shape.points.first,
            shape.points.count};
        result.closed = shape.closed;
        result.animated = false;
        return true;
    }

    if (evaluated.value.storage != evaluation::PropertyStorageKind::Materialized
        || evaluated.value.shapeSlot >= view.shapes.size()) {
        return false;
    }
    const auto& shape = view.shapes[evaluated.value.shapeSlot];
    if (shape.property != property.id
        || static_cast<std::size_t>(shape.firstPoint) + shape.pointCount
            > view.shapePoints.size()) {
        return false;
    }
    result.points = view.shapePoints.subspan(shape.firstPoint, shape.pointCount);
    result.closed = shape.closed;
    result.animated = true;
    return true;
}

[[nodiscard]] const model::MotionPropertyRecord* property(
    const model::MotionAssetModel& modelValue,
    model::PropertyId id) noexcept;

struct ScalarSample final {
    float value = 0.0F;
    bool animated = false;
};

struct Vec2Sample final {
    model::MotionVec2Value value;
    bool animated = false;
};

struct ColorSample final {
    model::MotionColorValue value;
    bool animated = false;
};

[[nodiscard]] const evaluation::EvaluatedProperty* evaluatedProperty(
    const model::MotionPropertyRecord& property,
    const evaluation::PropertyEvaluationView& view) noexcept {
    if (!property.id.valid() || property.id.index() >= view.properties.size()) {
        return nullptr;
    }
    const auto& evaluated = view.properties[property.id.index()];
    if (evaluated.id != property.id || !evaluated.value.materialized()) {
        return nullptr;
    }
    return &evaluated;
}

[[nodiscard]] bool resolveScalar(
    const model::MotionPropertyRecord& property,
    const evaluation::PropertyEvaluationView& view,
    ScalarSample& result) noexcept {
    if (property.valueType != model::PropertyValueType::Scalar) return false;
    const auto* evaluated = evaluatedProperty(property, view);
    if (!evaluated || evaluated->value.type != model::PropertyValueType::Scalar
        || !finite(evaluated->value.scalar)) {
        return false;
    }
    result.value = evaluated->value.scalar;
    result.animated = (property.flags & model::PropertyFlagAnimated) != 0U;
    return true;
}

[[nodiscard]] bool resolveVec2(
    const model::MotionPropertyRecord& property,
    const evaluation::PropertyEvaluationView& view,
    Vec2Sample& result) noexcept {
    if (property.valueType != model::PropertyValueType::Vec2) return false;
    const auto* evaluated = evaluatedProperty(property, view);
    if (!evaluated || evaluated->value.type != model::PropertyValueType::Vec2
        || !finite(evaluated->value.vec2)) {
        return false;
    }
    result.value = evaluated->value.vec2;
    result.animated = (property.flags & model::PropertyFlagAnimated) != 0U;
    return true;
}

[[nodiscard]] bool resolveColor(
    const model::MotionPropertyRecord& property,
    const evaluation::PropertyEvaluationView& view,
    ColorSample& result) noexcept {
    if (property.valueType != model::PropertyValueType::Color) return false;
    const auto* evaluated = evaluatedProperty(property, view);
    if (!evaluated || evaluated->value.type != model::PropertyValueType::Color
        || !finite(evaluated->value.color.r)
        || !finite(evaluated->value.color.g)
        || !finite(evaluated->value.color.b)
        || !finite(evaluated->value.color.a)) {
        return false;
    }
    result.value = evaluated->value.color;
    result.animated = (property.flags & model::PropertyFlagAnimated) != 0U;
    return true;
}

[[nodiscard]] bool telegramColorByte(
    float value,
    std::uint8_t& output) noexcept {
    if (!finite(value) || value < 0.0F || value > 1.0F) return false;
    output = static_cast<std::uint8_t>(255.0F * value);
    return true;
}

[[nodiscard]] bool telegramOpacityByte(
    float percentage,
    std::uint8_t& output) noexcept {
    if (!finite(percentage) || percentage < 0.0F || percentage > 100.0F) {
        return false;
    }
    output = static_cast<std::uint8_t>(255.0F * (percentage / 100.0F));
    return true;
}

[[nodiscard]] runtime::LineCap mapCap(model::SourceStrokeCap value) noexcept {
    switch (value) {
    case model::SourceStrokeCap::Square: return runtime::LineCap::Square;
    case model::SourceStrokeCap::Round: return runtime::LineCap::Round;
    case model::SourceStrokeCap::Flat: return runtime::LineCap::Flat;
    }
    return runtime::LineCap::Flat;
}

[[nodiscard]] runtime::LineJoin mapJoin(
    model::SourceStrokeJoin value) noexcept {
    switch (value) {
    case model::SourceStrokeJoin::Bevel: return runtime::LineJoin::Bevel;
    case model::SourceStrokeJoin::Round: return runtime::LineJoin::Round;
    case model::SourceStrokeJoin::Miter: return runtime::LineJoin::Miter;
    }
    return runtime::LineJoin::Miter;
}

[[nodiscard]] float telegramStrokeScale(
    const model::MotionMatrix3x2Value& matrix) noexcept {
    constexpr float kSqrt2 = 1.41421F;
    const model::MotionVec2Value origin{};
    const model::MotionVec2Value diagonal{kSqrt2, kSqrt2};
    const auto p1 = transformPoint(origin, matrix);
    const auto p2 = transformPoint(diagonal, matrix);
    const auto dx = p2.x - p1.x;
    const auto dy = p2.y - p1.y;
    return std::sqrt(dx * dx + dy * dy) / 2.0F;
}

[[nodiscard]] bool sameColor(
    const runtime::Color8& left,
    const runtime::Color8& right) noexcept {
    return left.r == right.r && left.g == right.g
        && left.b == right.b && left.a == right.a;
}

struct RepeaterPaintSample final {
    runtime::EvaluatedStroke stroke;
    runtime::EvaluatedPaint paint;
    bool animated = false;
};

[[nodiscard]] bool evaluateRepeaterPaint(
    const model::MotionAssetModel& modelValue,
    std::span<const SourcePropertyIds> propertiesByNode,
    const RepeaterBinding& binding,
    const evaluation::PropertyEvaluationView& properties,
    RepeaterPaintSample& result) noexcept {
    if (!binding.paintNode.valid()
        || binding.paintNode.index() >= propertiesByNode.size()) {
        return false;
    }
    const auto* paintNode = modelValue.sourceNode(binding.paintNode);
    if (!paintNode || !paintNode->present || !paintNode->enabled
        || paintNode->hidden) {
        return false;
    }
    const auto& ids = propertiesByNode[binding.paintNode.index()];
    const auto colorId = binding.paintKind == RepeaterPaintKind::SolidFill
        ? ids.fillColor : ids.strokeColor;
    const auto opacityId = binding.paintKind == RepeaterPaintKind::SolidFill
        ? ids.fillOpacity : ids.strokeOpacity;
    const auto* colorProperty = property(modelValue, colorId);
    const auto* opacityProperty = property(modelValue, opacityId);
    if (!colorProperty || !opacityProperty) return false;

    ColorSample color;
    ScalarSample opacity;
    if (!resolveColor(*colorProperty, properties, color)
        || !resolveScalar(*opacityProperty, properties, opacity)) {
        return false;
    }

    runtime::Color8 localColor;
    if (!telegramColorByte(color.value.r, localColor.r)
        || !telegramColorByte(color.value.g, localColor.g)
        || !telegramColorByte(color.value.b, localColor.b)
        || !telegramOpacityByte(opacity.value, localColor.a)) {
        return false;
    }
    result = {};
    result.paint.kind = runtime::PaintKind::Solid;
    result.paint.solid = localColor;
    result.animated = color.animated || opacity.animated;

    if (binding.paintKind == RepeaterPaintKind::SolidFill) {
        return paintNode->kind == model::SourceNodeKind::Fill;
    }
    if (binding.paintKind != RepeaterPaintKind::SolidStroke
        || paintNode->kind != model::SourceNodeKind::Stroke
        || ids.hasStrokeDash) {
        return false;
    }
    const auto* widthProperty = property(modelValue, ids.strokeWidth);
    if (!widthProperty) return false;
    ScalarSample width;
    if (!resolveScalar(*widthProperty, properties, width)
        || width.value < 0.0F || !finite(paintNode->miterLimit)) {
        return false;
    }
    result.stroke.enabled = true;
    result.stroke.width = width.value;
    result.stroke.miterLimit = paintNode->miterLimit;
    result.stroke.cap = mapCap(paintNode->strokeCap);
    result.stroke.join = mapJoin(paintNode->strokeJoin);
    result.animated = result.animated || width.animated;
    return true;
}

[[nodiscard]] runtime::Color8 applySeparatedOpacity(
    runtime::Color8 color,
    float opacity) noexcept {
    color.a = static_cast<std::uint8_t>(
        static_cast<float>(color.a) * opacity);
    return color;
}

[[nodiscard]] bool matchesAndMaterializesShape(
    const ShapeSample& shape,
    const model::MotionMatrix3x2Value& relative,
    runtime::EvaluatedPath& output) noexcept {
    if (!finite(relative)) return false;
    if (!shape.points.empty() && (shape.points.size() - 1U) % 3U != 0U) {
        return false;
    }
    const auto segmentCount = shape.points.empty()
        ? 0U : (shape.points.size() - 1U) / 3U;
    const auto verbCount = shape.points.empty()
        ? 0U : 1U + segmentCount + (shape.closed ? 1U : 0U);
    if (output.verbs.size() != verbCount
        || output.points.size() != shape.points.size()) {
        return false;
    }
    if (!shape.points.empty()) {
        if (output.verbs.front() != runtime::PathVerb::MoveTo) return false;
        for (std::size_t index = 0; index < segmentCount; ++index) {
            if (output.verbs[index + 1U] != runtime::PathVerb::CubicTo) {
                return false;
            }
        }
        if (shape.closed && output.verbs.back() != runtime::PathVerb::Close) {
            return false;
        }
    }

    for (std::size_t index = 0; index < shape.points.size(); ++index) {
        const auto point = transformPoint(shape.points[index], relative);
        if (!finite(point.x) || !finite(point.y)
            || std::fabs(point.x - output.points[index].x) > kParityTolerance
            || std::fabs(point.y - output.points[index].y) > kParityTolerance) {
            return false;
        }
    }

    output.controlBounds = {};
    for (std::size_t index = 0; index < shape.points.size(); ++index) {
        const auto point = transformPoint(shape.points[index], relative);
        output.points[index] = point;
        includePoint(output.controlBounds, point);
    }
    finalizePathHash(output);
    return true;
}

[[nodiscard]] bool matchesAndMaterializesPrimitive(
    const detail::PrimitivePath& candidate,
    const model::MotionMatrix3x2Value& relative,
    runtime::EvaluatedPath& output) noexcept {
    if (!candidate.valid || !finite(relative)
        || output.verbs.size() != candidate.verbCount
        || output.points.size() != candidate.pointCount) {
        return false;
    }
    const auto verbs = candidate.verbSpan();
    const auto points = candidate.pointSpan();
    for (std::size_t index = 0; index < verbs.size(); ++index) {
        if (output.verbs[index] != verbs[index]) return false;
    }
    for (std::size_t index = 0; index < points.size(); ++index) {
        const auto point = transformPoint(points[index], relative);
        if (!finite(point.x) || !finite(point.y)
            || std::fabs(point.x - output.points[index].x) > kParityTolerance
            || std::fabs(point.y - output.points[index].y) > kParityTolerance) {
            return false;
        }
    }

    output.controlBounds = {};
    for (std::size_t index = 0; index < verbs.size(); ++index) {
        output.verbs[index] = verbs[index];
    }
    for (std::size_t index = 0; index < points.size(); ++index) {
        const auto point = transformPoint(points[index], relative);
        output.points[index] = point;
        includePoint(output.controlBounds, point);
    }
    finalizePathHash(output);
    return true;
}

[[nodiscard]] bool pathMatchesTransformed(
    std::span<const runtime::PathVerb> verbs,
    std::span<const model::MotionVec2Value> points,
    const model::MotionMatrix3x2Value& matrix,
    const runtime::EvaluatedPath& oracle) noexcept {
    if (!finite(matrix) || oracle.verbs.size() != verbs.size()
        || oracle.points.size() != points.size()) {
        return false;
    }
    for (std::size_t index = 0U; index < verbs.size(); ++index) {
        if (oracle.verbs[index] != verbs[index]) return false;
    }
    for (std::size_t index = 0U; index < points.size(); ++index) {
        const auto point = transformPoint(points[index], matrix);
        if (!finite(point.x) || !finite(point.y)
            || std::fabs(point.x - oracle.points[index].x) > kParityTolerance
            || std::fabs(point.y - oracle.points[index].y) > kParityTolerance) {
            return false;
        }
    }
    return true;
}

void materializeTransformed(
    std::span<const runtime::PathVerb> verbs,
    std::span<const model::MotionVec2Value> points,
    const model::MotionMatrix3x2Value& matrix,
    runtime::EvaluatedPath& output) {
    output.verbs.assign(verbs.begin(), verbs.end());
    output.points.resize(points.size());
    output.controlBounds = {};
    for (std::size_t index = 0U; index < points.size(); ++index) {
        const auto point = transformPoint(points[index], matrix);
        output.points[index] = point;
        includePoint(output.controlBounds, point);
    }
    finalizePathHash(output);
}

[[nodiscard]] bool shapePathStream(
    const ShapeSample& shape,
    std::vector<runtime::PathVerb>& verbs,
    std::vector<model::MotionVec2Value>& points) {
    if (!shape.points.empty() && (shape.points.size() - 1U) % 3U != 0U) {
        return false;
    }
    points.assign(shape.points.begin(), shape.points.end());
    verbs.clear();
    if (shape.points.empty()) return true;
    const auto segmentCount = (shape.points.size() - 1U) / 3U;
    verbs.reserve(1U + segmentCount + (shape.closed ? 1U : 0U));
    verbs.push_back(runtime::PathVerb::MoveTo);
    for (std::size_t index = 0U; index < segmentCount; ++index) {
        verbs.push_back(runtime::PathVerb::CubicTo);
    }
    if (shape.closed) verbs.push_back(runtime::PathVerb::Close);
    return true;
}

[[nodiscard]] bool primitivePathStream(
    const detail::PrimitivePath& path,
    std::vector<runtime::PathVerb>& verbs,
    std::vector<model::MotionVec2Value>& points) {
    if (!path.valid) return false;
    const auto verbSpan = path.verbSpan();
    const auto pointSpan = path.pointSpan();
    verbs.assign(verbSpan.begin(), verbSpan.end());
    points.assign(pointSpan.begin(), pointSpan.end());
    return true;
}

[[nodiscard]] const model::MotionPropertyRecord* property(
    const model::MotionAssetModel& modelValue,
    model::PropertyId id) noexcept;

[[nodiscard]] bool buildCandidatePath(
    const model::MotionAssetModel& modelValue,
    std::span<const SourcePropertyIds> propertiesByNode,
    model::SourceNodeId nodeId,
    const evaluation::PropertyEvaluationView& properties,
    std::vector<runtime::PathVerb>& verbs,
    std::vector<model::MotionVec2Value>& points,
    CandidatePathRecord& candidate,
    SourceGeometryProjectionStatistics& statistics) {
    const auto* pathNode = modelValue.sourceNode(nodeId);
    if (!pathNode || !pathNode->present
        || nodeId.index() >= propertiesByNode.size()) {
        return false;
    }
    candidate = {};
    candidate.sourceNode = nodeId;
    verbs.clear();
    points.clear();
    const auto& ids = propertiesByNode[nodeId.index()];

    switch (pathNode->kind) {
    case model::SourceNodeKind::Shape: {
        const auto* sourceProperty = property(modelValue, ids.shape);
        if (!sourceProperty) {
            ++statistics.skippedNoShapeProperty;
            return false;
        }
        ShapeSample shape;
        if (!resolveShape(modelValue, *sourceProperty, properties, shape)) {
            ++statistics.skippedShapeEvaluation;
            return false;
        }
        candidate.animated = shape.animated;
        return shapePathStream(shape, verbs, points);
    }
    case model::SourceNodeKind::Rectangle: {
        ++statistics.primitiveCandidates;
        ++statistics.rectangleCandidates;
        candidate.primitiveKind = PrimitiveKind::Rectangle;
        const auto* positionProperty = property(modelValue, ids.position);
        const auto* sizeProperty = property(modelValue, ids.size);
        const auto* roundnessProperty = property(modelValue, ids.roundness);
        if (!positionProperty || !sizeProperty || !roundnessProperty) {
            ++statistics.skippedPrimitiveProperties;
            return false;
        }
        Vec2Sample position;
        Vec2Sample size;
        ScalarSample roundness;
        if (!resolveVec2(*positionProperty, properties, position)
            || !resolveVec2(*sizeProperty, properties, size)
            || !resolveScalar(*roundnessProperty, properties, roundness)) {
            ++statistics.skippedPrimitiveEvaluation;
            return false;
        }
        const auto generated = detail::generateRectanglePath(
            position.value,
            size.value,
            roundness.value,
            pathNode->pathDirection);
        candidate.roundedRectangle = generated.rounded;
        candidate.animated = position.animated || size.animated
            || roundness.animated;
        return primitivePathStream(generated, verbs, points);
    }
    case model::SourceNodeKind::Ellipse: {
        ++statistics.primitiveCandidates;
        ++statistics.ellipseCandidates;
        candidate.primitiveKind = PrimitiveKind::Ellipse;
        const auto* positionProperty = property(modelValue, ids.position);
        const auto* sizeProperty = property(modelValue, ids.size);
        if (!positionProperty || !sizeProperty) {
            ++statistics.skippedPrimitiveProperties;
            return false;
        }
        Vec2Sample position;
        Vec2Sample size;
        if (!resolveVec2(*positionProperty, properties, position)
            || !resolveVec2(*sizeProperty, properties, size)) {
            ++statistics.skippedPrimitiveEvaluation;
            return false;
        }
        const auto generated = detail::generateEllipsePath(
            position.value, size.value, pathNode->pathDirection);
        candidate.animated = position.animated || size.animated;
        return primitivePathStream(generated, verbs, points);
    }
    case model::SourceNodeKind::Polystar: {
        ++statistics.primitiveCandidates;
        ++statistics.polystarCandidates;
        const auto* positionProperty = property(modelValue, ids.position);
        const auto* pointCountProperty = property(modelValue, ids.pointCount);
        const auto* outerRadiusProperty = property(modelValue, ids.outerRadius);
        const auto* outerRoundnessProperty = property(
            modelValue, ids.outerRoundness);
        const auto* rotationProperty = property(modelValue, ids.rotation);
        if (!positionProperty || !pointCountProperty || !outerRadiusProperty
            || !outerRoundnessProperty || !rotationProperty) {
            ++statistics.skippedPrimitiveProperties;
            ++statistics.skippedPolystarProperties;
            return false;
        }

        Vec2Sample position;
        ScalarSample pointCount;
        ScalarSample outerRadius;
        ScalarSample outerRoundness;
        ScalarSample rotation;
        if (!resolveVec2(*positionProperty, properties, position)
            || !resolveScalar(*pointCountProperty, properties, pointCount)
            || !resolveScalar(*outerRadiusProperty, properties, outerRadius)
            || !resolveScalar(
                *outerRoundnessProperty, properties, outerRoundness)
            || !resolveScalar(*rotationProperty, properties, rotation)) {
            ++statistics.skippedPrimitiveEvaluation;
            ++statistics.skippedPolystarEvaluation;
            return false;
        }

        detail::PrimitivePath generated;
        candidate.animated = position.animated || pointCount.animated
            || outerRadius.animated || outerRoundness.animated
            || rotation.animated;
        if (pathNode->polystarType == model::SourcePolystarType::Star) {
            ++statistics.starCandidates;
            candidate.primitiveKind = PrimitiveKind::Star;
            const auto* innerRadiusProperty = property(
                modelValue, ids.innerRadius);
            const auto* innerRoundnessProperty = property(
                modelValue, ids.innerRoundness);
            if (!innerRadiusProperty || !innerRoundnessProperty) {
                ++statistics.skippedPrimitiveProperties;
                ++statistics.skippedPolystarProperties;
                return false;
            }
            ScalarSample innerRadius;
            ScalarSample innerRoundness;
            if (!resolveScalar(
                    *innerRadiusProperty, properties, innerRadius)
                || !resolveScalar(
                    *innerRoundnessProperty, properties, innerRoundness)) {
                ++statistics.skippedPrimitiveEvaluation;
                ++statistics.skippedPolystarEvaluation;
                return false;
            }
            candidate.animated = candidate.animated || innerRadius.animated
                || innerRoundness.animated;
            generated = detail::generatePolystarPath(
                position.value,
                pointCount.value,
                innerRadius.value,
                outerRadius.value,
                innerRoundness.value,
                outerRoundness.value,
                rotation.value,
                pathNode->pathDirection);
        } else if (pathNode->polystarType
            == model::SourcePolystarType::Polygon) {
            ++statistics.polygonCandidates;
            candidate.primitiveKind = PrimitiveKind::Polygon;
            generated = detail::generatePolygonPath(
                position.value,
                pointCount.value,
                outerRadius.value,
                outerRoundness.value,
                rotation.value,
                pathNode->pathDirection);
        } else {
            ++statistics.rejectedPolystarInputs;
            return false;
        }
        if (!generated.valid) {
            ++statistics.rejectedPolystarInputs;
            return false;
        }
        return primitivePathStream(generated, verbs, points);
    }
    default:
        ++statistics.skippedWrongNodeKind;
        return false;
    }
}

[[nodiscard]] const model::MotionPropertyRecord* property(
    const model::MotionAssetModel& modelValue,
    model::PropertyId id) noexcept {
    return id.valid() ? modelValue.property(id) : nullptr;
}

} // namespace

struct SourceGeometryProjectionWorkspace::Impl final {
    std::uint64_t modelFingerprint = 0U;
    std::vector<runtime::PathVerb> temporaryVerbs;
    std::vector<model::MotionVec2Value> temporaryPoints;
    std::vector<runtime::PathVerb> sourceVerbs;
    std::vector<model::MotionVec2Value> sourcePoints;
    std::vector<CandidatePathRecord> candidates;
    std::vector<detail::TrimPathInput> trimInputs;
    detail::TrimmedPathCollection trimmed;
    std::vector<runtime::PathVerb> combinedVerbs;
    std::vector<model::MotionVec2Value> combinedPoints;
    std::vector<std::uint32_t> repeaterCopyCounters;
    std::uint64_t storageGeneration = 0U;
};

struct SourceGeometryProjector::Impl final {
    std::shared_ptr<const model::MotionAssetModel> model;
    std::vector<SourcePropertyIds> propertiesByNode;
    std::vector<TrimBinding> trimByPaintNode;
    std::vector<TrimBinding> trimByPathNode;
    std::vector<model::SourceNodeId> trimPathNodes;
    std::vector<RepeaterBinding> repeaterByPaintNode;
    std::vector<RepeaterGeometryBinding> repeaterGeometryBindings;
    std::vector<model::SourceNodeId> repeaterPathNodes;
    std::size_t maximumTrimPathCount = 1U;
    std::size_t maximumRepeaterPathCount = 1U;
    std::size_t maximumShapePointCount = 0U;
    bool valid = false;
    std::string error;
};

SourceGeometryProjectionWorkspace::SourceGeometryProjectionWorkspace()
    : impl_(std::make_unique<Impl>()) {}

SourceGeometryProjectionWorkspace::SourceGeometryProjectionWorkspace(
    SourceGeometryProjectionWorkspace&&) noexcept = default;

SourceGeometryProjectionWorkspace&
SourceGeometryProjectionWorkspace::operator=(
    SourceGeometryProjectionWorkspace&&) noexcept = default;

SourceGeometryProjectionWorkspace::~SourceGeometryProjectionWorkspace() = default;

std::size_t SourceGeometryProjectionWorkspace::retainedBytes() const noexcept {
    if (!impl_) return 0U;
    const auto& state = *impl_;
    return state.temporaryVerbs.capacity() * sizeof(runtime::PathVerb)
        + state.temporaryPoints.capacity() * sizeof(model::MotionVec2Value)
        + state.sourceVerbs.capacity() * sizeof(runtime::PathVerb)
        + state.sourcePoints.capacity() * sizeof(model::MotionVec2Value)
        + state.candidates.capacity() * sizeof(CandidatePathRecord)
        + state.trimInputs.capacity() * sizeof(detail::TrimPathInput)
        + state.trimmed.verbs.capacity() * sizeof(runtime::PathVerb)
        + state.trimmed.points.capacity() * sizeof(model::MotionVec2Value)
        + state.trimmed.paths.capacity() * sizeof(detail::TrimmedPathSlice)
        + state.trimmed.sourceLengths.capacity() * sizeof(float)
        + state.trimmed.scratch.verbs.capacity() * sizeof(runtime::PathVerb)
        + state.trimmed.scratch.points.capacity() * sizeof(model::MotionVec2Value)
        + state.combinedVerbs.capacity() * sizeof(runtime::PathVerb)
        + state.combinedPoints.capacity() * sizeof(model::MotionVec2Value)
        + state.repeaterCopyCounters.capacity() * sizeof(std::uint32_t);
}

std::uint64_t SourceGeometryProjectionWorkspace::storageGeneration() const noexcept {
    return impl_ ? impl_->storageGeneration : 0U;
}

SourceGeometryProjector::SourceGeometryProjector(
    std::shared_ptr<const model::MotionAssetModel> modelValue)
    : impl_(std::make_unique<Impl>()) {
    impl_->model = std::move(modelValue);
    if (!impl_->model || !impl_->model->statistics.directParsedModel
        || impl_->model->compositions.empty()) {
        impl_->error = "source geometry projector requires a parsed canonical model";
        return;
    }
    impl_->propertiesByNode.resize(impl_->model->sourceNodes.size());
    impl_->trimByPaintNode.resize(impl_->model->sourceNodes.size());
    impl_->trimByPathNode.resize(impl_->model->sourceNodes.size());
    impl_->repeaterByPaintNode.resize(impl_->model->sourceNodes.size());
    for (const auto& shape : impl_->model->shapeValues) {
        impl_->maximumShapePointCount = std::max<std::size_t>(
            impl_->maximumShapePointCount, shape.points.count);
    }
    for (const auto& sourceProperty : impl_->model->properties) {
        if (!sourceProperty.present || !sourceProperty.owner.valid()
            || sourceProperty.owner.index() >= impl_->propertiesByNode.size()) {
            continue;
        }
        auto& ids = impl_->propertiesByNode[sourceProperty.owner.index()];
        bool accepted = true;
        switch (sourceProperty.semantic) {
        case model::PropertySemantic::ShapePath:
            accepted = assignUnique(ids.shape, sourceProperty.id);
            break;
        case model::PropertySemantic::RectanglePosition:
        case model::PropertySemantic::EllipsePosition:
        case model::PropertySemantic::PolystarPosition:
            accepted = assignUnique(ids.position, sourceProperty.id);
            break;
        case model::PropertySemantic::RectangleSize:
        case model::PropertySemantic::EllipseSize:
            accepted = assignUnique(ids.size, sourceProperty.id);
            break;
        case model::PropertySemantic::RectangleRoundness:
            accepted = assignUnique(ids.roundness, sourceProperty.id);
            break;
        case model::PropertySemantic::PolystarPointCount:
            accepted = assignUnique(ids.pointCount, sourceProperty.id);
            break;
        case model::PropertySemantic::PolystarInnerRadius:
            accepted = assignUnique(ids.innerRadius, sourceProperty.id);
            break;
        case model::PropertySemantic::PolystarOuterRadius:
            accepted = assignUnique(ids.outerRadius, sourceProperty.id);
            break;
        case model::PropertySemantic::PolystarInnerRoundness:
            accepted = assignUnique(ids.innerRoundness, sourceProperty.id);
            break;
        case model::PropertySemantic::PolystarOuterRoundness:
            accepted = assignUnique(ids.outerRoundness, sourceProperty.id);
            break;
        case model::PropertySemantic::PolystarRotation:
            accepted = assignUnique(ids.rotation, sourceProperty.id);
            break;
        case model::PropertySemantic::TrimStart:
            accepted = assignUnique(ids.trimStart, sourceProperty.id);
            break;
        case model::PropertySemantic::TrimEnd:
            accepted = assignUnique(ids.trimEnd, sourceProperty.id);
            break;
        case model::PropertySemantic::TrimOffset:
            accepted = assignUnique(ids.trimOffset, sourceProperty.id);
            break;
        case model::PropertySemantic::RepeaterCopies:
            accepted = assignUnique(ids.repeaterCopies, sourceProperty.id);
            break;
        case model::PropertySemantic::RepeaterOffset:
            accepted = assignUnique(ids.repeaterOffset, sourceProperty.id);
            break;
        case model::PropertySemantic::RepeaterPosition:
            accepted = assignUnique(ids.repeaterPosition, sourceProperty.id);
            break;
        case model::PropertySemantic::RepeaterScale:
            accepted = assignUnique(ids.repeaterScale, sourceProperty.id);
            break;
        case model::PropertySemantic::RepeaterRotation:
            accepted = assignUnique(ids.repeaterRotation, sourceProperty.id);
            break;
        case model::PropertySemantic::RepeaterAnchor:
            accepted = assignUnique(ids.repeaterAnchor, sourceProperty.id);
            break;
        case model::PropertySemantic::RepeaterStartOpacity:
            accepted = assignUnique(ids.repeaterStartOpacity, sourceProperty.id);
            break;
        case model::PropertySemantic::RepeaterEndOpacity:
            accepted = assignUnique(ids.repeaterEndOpacity, sourceProperty.id);
            break;
        case model::PropertySemantic::FillColor:
            accepted = assignUnique(ids.fillColor, sourceProperty.id);
            break;
        case model::PropertySemantic::FillOpacity:
            accepted = assignUnique(ids.fillOpacity, sourceProperty.id);
            break;
        case model::PropertySemantic::StrokeColor:
            accepted = assignUnique(ids.strokeColor, sourceProperty.id);
            break;
        case model::PropertySemantic::StrokeOpacity:
            accepted = assignUnique(ids.strokeOpacity, sourceProperty.id);
            break;
        case model::PropertySemantic::StrokeWidth:
            accepted = assignUnique(ids.strokeWidth, sourceProperty.id);
            break;
        case model::PropertySemantic::StrokeDashValue:
            ids.hasStrokeDash = true;
            break;
        default:
            break;
        }
        if (!accepted) {
            impl_->error = "source geometry node has duplicate primitive properties";
            return;
        }
    }
    // Preserve the Part 14 single-path binding seam. Telegram may bind one
    // authored path to a Trim Path even when the paint appears between the
    // path and the trim item (for example a stroked polystar). This binding is
    // also useful as a conservative classification/fallback when the paint is
    // not yet supported by AveMotion's local-paint seam.
    for (const auto& parent : impl_->model->sourceNodes) {
        if (!parent.present || parent.children.end()
                > impl_->model->sourceChildIds.size()) {
            continue;
        }
        model::SourceNodeId onlyPath;
        model::SourceNodeId onlyTrim;
        std::size_t pathPosition = 0U;
        std::size_t trimPosition = 0U;
        std::size_t pathCount = 0U;
        std::size_t trimCount = 0U;
        bool hasRepeater = false;
        bool hasNestedGroup = false;
        for (std::size_t offset = 0U; offset < parent.children.count; ++offset) {
            const auto childId = impl_->model->sourceChildIds[
                parent.children.first + offset];
            const auto* child = impl_->model->sourceNode(childId);
            if (!child || !child->present) continue;
            if (pathLike(child->kind)) {
                ++pathCount;
                onlyPath = childId;
                pathPosition = offset;
            } else if (child->kind == model::SourceNodeKind::Trim
                && child->enabled && !child->hidden
                && child->trimMode != model::SourceTrimMode::None) {
                ++trimCount;
                onlyTrim = childId;
                trimPosition = offset;
            } else if (child->kind == model::SourceNodeKind::Repeater) {
                hasRepeater = true;
            } else if (child->kind == model::SourceNodeKind::ShapeGroup) {
                hasNestedGroup = true;
            }
        }
        if (pathCount != 1U || trimCount != 1U || hasRepeater
            || hasNestedGroup || pathPosition >= trimPosition
            || !onlyPath.valid() || !onlyTrim.valid()) {
            continue;
        }
        const auto* trim = impl_->model->sourceNode(onlyTrim);
        if (!trim) continue;
        if (impl_->trimPathNodes.size()
            == static_cast<std::size_t>(model::kInvalidModelId)) {
            impl_->error = "trim path binding exceeds canonical ID range";
            return;
        }
        const auto first = static_cast<std::uint32_t>(
            impl_->trimPathNodes.size());
        impl_->trimPathNodes.push_back(onlyPath);
        impl_->trimByPathNode[onlyPath.index()] = {
            onlyTrim,
            trim->trimMode,
            {first, 1U},
        };
    }

    // Reproduce the direct-group subset of Telegram's processPaintItems /
    // processTrimItems ordering. Source children are stored in authored order,
    // which is also the order of LOTPaintDataItem::mPathItems after rlottie's
    // internal back-to-front reversal. Part 15 accepts one direct Trim Path,
    // one or more direct paths before it, and local solid fills after it.
    // Nested groups, repeaters and chained trims remain fail-closed.
    for (const auto& parent : impl_->model->sourceNodes) {
        if (!parent.present || parent.children.end()
                > impl_->model->sourceChildIds.size()) {
            continue;
        }
        model::SourceNodeId onlyTrim;
        std::size_t trimPosition = 0U;
        std::size_t trimCount = 0U;
        bool hasRepeater = false;
        bool hasNestedGroup = false;
        std::vector<model::SourceNodeId> paths;
        paths.reserve(parent.children.count);
        for (std::size_t offset = 0U; offset < parent.children.count; ++offset) {
            const auto childId = impl_->model->sourceChildIds[
                parent.children.first + offset];
            const auto* child = impl_->model->sourceNode(childId);
            if (!child || !child->present) continue;
            if (pathLike(child->kind)) {
                paths.push_back(childId);
            } else if (child->kind == model::SourceNodeKind::Trim
                && child->enabled && !child->hidden
                && child->trimMode != model::SourceTrimMode::None) {
                ++trimCount;
                onlyTrim = childId;
                trimPosition = offset;
            } else if (child->kind == model::SourceNodeKind::Repeater) {
                hasRepeater = true;
            } else if (child->kind == model::SourceNodeKind::ShapeGroup) {
                hasNestedGroup = true;
            }
        }
        if (paths.empty() || trimCount != 1U || hasRepeater || hasNestedGroup
            || !onlyTrim.valid()) {
            continue;
        }
        const auto* trim = impl_->model->sourceNode(onlyTrim);
        if (!trim) continue;

        bool pathsBeforeTrim = true;
        for (std::size_t offset = trimPosition + 1U;
             offset < parent.children.count; ++offset) {
            const auto childId = impl_->model->sourceChildIds[
                parent.children.first + offset];
            const auto* child = impl_->model->sourceNode(childId);
            if (child && child->present && pathLike(child->kind)) {
                pathsBeforeTrim = false;
                break;
            }
        }
        if (!pathsBeforeTrim) continue;

        if (impl_->trimPathNodes.size()
            > static_cast<std::size_t>(model::kInvalidModelId)
                - paths.size()) {
            impl_->error = "trim path binding exceeds canonical ID range";
            return;
        }
        const auto first = static_cast<std::uint32_t>(
            impl_->trimPathNodes.size());
        impl_->trimPathNodes.insert(
            impl_->trimPathNodes.end(), paths.begin(), paths.end());
        const model::IndexRange pathRange{
            first, static_cast<std::uint32_t>(paths.size())};
        impl_->maximumTrimPathCount = std::max(
            impl_->maximumTrimPathCount, paths.size());

        for (std::size_t offset = trimPosition + 1U;
             offset < parent.children.count; ++offset) {
            const auto paintId = impl_->model->sourceChildIds[
                parent.children.first + offset];
            const auto* paint = impl_->model->sourceNode(paintId);
            if (!paint || !paint->present
                || paint->kind != model::SourceNodeKind::Fill
                || !paint->enabled || paint->hidden) {
                continue;
            }
            impl_->trimByPaintNode[paintId.index()] = {
                onlyTrim, trim->trimMode, pathRange};
        }
    }

    // Part 17 Repeater content-group seam. Telegram's processPaintItems()
    // walks authored children recursively while keeping one shared path list.
    // Each paint receives the paths accumulated since entry into its own
    // structural group. AveMotion records one binding per authored solid
    // fill/stroke and deduplicates geometry when multiple paints address the
    // same path set in the same local paint space.
    constexpr float kMaximumSupportedRepeaterCopies = 4096.0F;
    constexpr std::size_t kMaximumRepeaterNestingDepth = 64U;
    struct PendingRepeaterPaint final {
        model::SourceNodeId paintNode;
        model::SourceNodeId paintSpaceNode;
        RepeaterPaintKind paintKind = RepeaterPaintKind::None;
        std::vector<model::SourceNodeId> paths;
    };

    for (const auto& repeater : impl_->model->sourceNodes) {
        if (!repeater.present || repeater.kind != model::SourceNodeKind::Repeater
            || repeater.children.count != 1U
            || repeater.children.end() > impl_->model->sourceChildIds.size()
            || !finite(repeater.repeaterMaximumCopies)
            || repeater.repeaterMaximumCopies < 1.0F
            || repeater.repeaterMaximumCopies > kMaximumSupportedRepeaterCopies) {
            continue;
        }
        const auto contentId = impl_->model->sourceChildIds[
            repeater.children.first];
        const auto* content = impl_->model->sourceNode(contentId);
        if (!content || !content->present
            || content->kind != model::SourceNodeKind::ShapeGroup
            || content->parent != repeater.id
            || content->children.end() > impl_->model->sourceChildIds.size()) {
            continue;
        }

        std::vector<model::SourceNodeId> activePaths;
        std::vector<PendingRepeaterPaint> pendingPaints;
        activePaths.reserve(content->children.count);
        pendingPaints.reserve(content->children.count);
        bool unsupported = false;

        const auto visitGroup = [&](auto&& self,
                                    model::SourceNodeId groupId,
                                    std::size_t depth) -> void {
            if (unsupported || depth > kMaximumRepeaterNestingDepth) {
                unsupported = true;
                return;
            }
            const auto* group = impl_->model->sourceNode(groupId);
            if (!group || !group->present
                || group->kind != model::SourceNodeKind::ShapeGroup
                || group->children.end()
                    > impl_->model->sourceChildIds.size()) {
                unsupported = true;
                return;
            }
            const auto groupPathStart = activePaths.size();
            for (std::size_t offset = 0U; offset < group->children.count;
                 ++offset) {
                const auto childId = impl_->model->sourceChildIds[
                    group->children.first + offset];
                const auto* child = impl_->model->sourceNode(childId);
                if (!child || !child->present || child->parent != groupId) {
                    unsupported = true;
                    return;
                }
                if (pathLike(child->kind)) {
                    if (child->enabled && !child->hidden) {
                        activePaths.push_back(childId);
                    }
                    continue;
                }
                if (child->kind == model::SourceNodeKind::ShapeGroup) {
                    self(self, childId, depth + 1U);
                    if (unsupported) return;
                    continue;
                }
                RepeaterPaintKind paintKind = RepeaterPaintKind::None;
                if (child->kind == model::SourceNodeKind::Fill
                    && child->enabled && !child->hidden) {
                    paintKind = RepeaterPaintKind::SolidFill;
                } else if (child->kind == model::SourceNodeKind::Stroke
                           && child->enabled && !child->hidden
                           && childId.index() < impl_->propertiesByNode.size()
                           && !impl_->propertiesByNode[childId.index()]
                                   .hasStrokeDash) {
                    paintKind = RepeaterPaintKind::SolidStroke;
                } else {
                    // Chained repeaters, trims, gradients and all other
                    // modifiers remain on Telegram's proven path.
                    unsupported = true;
                    return;
                }
                if (activePaths.size() <= groupPathStart) {
                    unsupported = true;
                    return;
                }
                PendingRepeaterPaint pending;
                pending.paintNode = childId;
                pending.paintSpaceNode = groupId;
                pending.paintKind = paintKind;
                pending.paths.assign(
                    activePaths.begin()
                        + static_cast<std::ptrdiff_t>(groupPathStart),
                    activePaths.end());
                pendingPaints.push_back(std::move(pending));
            }
        };
        visitGroup(visitGroup, contentId, 0U);
        if (unsupported || pendingPaints.empty()) continue;

        const auto maximumCopies = static_cast<std::uint32_t>(
            static_cast<std::int32_t>(repeater.repeaterMaximumCopies));
        if (maximumCopies == 0U) continue;

        const auto samePendingGeometry = [](const PendingRepeaterPaint& left,
                                               const PendingRepeaterPaint& right) {
            return left.paintSpaceNode == right.paintSpaceNode
                && left.paths == right.paths;
        };
        const auto sourceFillRule = [&](model::SourceNodeId paintNode) {
            const auto* node = impl_->model->sourceNode(paintNode);
            return node && node->fillRule == model::SourceFillRule::EvenOdd
                ? runtime::FillRule::EvenOdd
                : runtime::FillRule::Winding;
        };

        for (const auto& pending : pendingPaints) {
            auto geometryFillRule = runtime::FillRule::Winding;
            if (pending.paintKind == RepeaterPaintKind::SolidFill) {
                geometryFillRule = sourceFillRule(pending.paintNode);
            } else {
                // A stroke does not consume the path fill mode. Bind it to the
                // first authored Fill over the same path set so both paint
                // applications can share one native path resource. If no Fill
                // exists, the deterministic fallback is winding.
                for (const auto& candidate : pendingPaints) {
                    if (candidate.paintKind == RepeaterPaintKind::SolidFill
                        && samePendingGeometry(candidate, pending)) {
                        geometryFillRule = sourceFillRule(candidate.paintNode);
                        break;
                    }
                }
            }

            bool geometryTransformAnimated = false;
            bool transformPathValid = true;
            for (const auto pathId : pending.paths) {
                bool pathTransformAnimated = false;
                if (!relativeTransformAnimated(
                        *impl_->model,
                        pathId,
                        pending.paintSpaceNode,
                        pathTransformAnimated)) {
                    transformPathValid = false;
                    break;
                }
                geometryTransformAnimated = geometryTransformAnimated
                    || pathTransformAnimated;
            }
            if (!transformPathValid) continue;

            std::uint32_t geometryBindingIndex = model::kInvalidModelId;
            for (std::size_t index = 0U;
                 index < impl_->repeaterGeometryBindings.size(); ++index) {
                const auto& existing = impl_->repeaterGeometryBindings[index];
                if (existing.repeaterNode != repeater.id
                    || existing.paintSpaceNode != pending.paintSpaceNode
                    || existing.fillRule != geometryFillRule
                    || existing.pathNodes.count != pending.paths.size()
                    || existing.pathNodes.end()
                        > impl_->repeaterPathNodes.size()) {
                    continue;
                }
                bool equal = true;
                for (std::size_t pathIndex = 0U;
                     pathIndex < pending.paths.size(); ++pathIndex) {
                    if (impl_->repeaterPathNodes[
                            existing.pathNodes.first + pathIndex]
                        != pending.paths[pathIndex]) {
                        equal = false;
                        break;
                    }
                }
                if (equal) {
                    geometryBindingIndex = static_cast<std::uint32_t>(index);
                    break;
                }
            }

            if (geometryBindingIndex == model::kInvalidModelId) {
                if (impl_->repeaterGeometryBindings.size()
                        >= static_cast<std::size_t>(model::kInvalidModelId)
                    || impl_->repeaterPathNodes.size()
                        > static_cast<std::size_t>(model::kInvalidModelId)
                            - pending.paths.size()) {
                    impl_->error = "repeater binding exceeds canonical ID range";
                    return;
                }
                const auto first = static_cast<std::uint32_t>(
                    impl_->repeaterPathNodes.size());
                impl_->repeaterPathNodes.insert(
                    impl_->repeaterPathNodes.end(),
                    pending.paths.begin(),
                    pending.paths.end());
                geometryBindingIndex = static_cast<std::uint32_t>(
                    impl_->repeaterGeometryBindings.size());
                impl_->repeaterGeometryBindings.push_back({
                    repeater.id,
                    contentId,
                    pending.paintSpaceNode,
                    {first, static_cast<std::uint32_t>(pending.paths.size())},
                    geometryFillRule,
                    maximumCopies,
                    geometryTransformAnimated,
                });
                impl_->maximumRepeaterPathCount = std::max(
                    impl_->maximumRepeaterPathCount,
                    pending.paths.size());
            }

            impl_->repeaterByPaintNode[pending.paintNode.index()] = {
                repeater.id,
                contentId,
                pending.paintNode,
                geometryBindingIndex,
                maximumCopies,
                pending.paintKind,
            };
        }
    }

    impl_->valid = true;
}

SourceGeometryProjector::SourceGeometryProjector(
    SourceGeometryProjector&&) noexcept = default;
SourceGeometryProjector& SourceGeometryProjector::operator=(
    SourceGeometryProjector&&) noexcept = default;
SourceGeometryProjector::~SourceGeometryProjector() = default;

bool SourceGeometryProjector::valid() const noexcept {
    return impl_ && impl_->valid;
}

std::string_view SourceGeometryProjector::errorMessage() const noexcept {
    return impl_ ? std::string_view{impl_->error} : std::string_view{};
}

bool SourceGeometryProjector::prepare(
    SourceGeometryProjectionWorkspace& workspace) const {
    if (!valid() || !workspace.impl_) return false;
    auto& state = *workspace.impl_;
    bool changed = false;
    auto reserve = [&changed](auto& values, std::size_t capacity) {
        const auto before = values.capacity();
        if (capacity > before) values.reserve(capacity);
        changed = changed || values.capacity() != before;
    };

    const std::size_t maximumShapePoints = std::max<std::size_t>(
        impl_->maximumShapePointCount, 1U);
    const std::size_t maximumShapeVerbs = 2U
        + (maximumShapePoints > 0U ? (maximumShapePoints - 1U) / 3U : 0U);
    const std::size_t singlePathPoints = std::max(
        maximumShapePoints, detail::PrimitivePath::kMaximumPointCount);
    const std::size_t singlePathVerbs = std::max(
        maximumShapeVerbs, detail::PrimitivePath::kMaximumVerbCount);
    const std::size_t pathCount = std::max<std::size_t>({
        impl_->maximumTrimPathCount,
        impl_->maximumRepeaterPathCount,
        1U});
    std::size_t sourceVerbCapacity = 0U;
    std::size_t sourcePointCapacity = 0U;
    std::size_t outputVerbCapacity = 0U;
    std::size_t outputPointCapacity = 0U;
    std::size_t scaledCapacity = 0U;
    std::size_t pathPadding = 0U;
    if (!checkedMultiply(pathCount, singlePathVerbs, sourceVerbCapacity)
        || !checkedMultiply(pathCount, singlePathPoints, sourcePointCapacity)
        || !checkedMultiply(sourceVerbCapacity, 2U, scaledCapacity)
        || !checkedMultiply(pathCount, 8U, pathPadding)
        || !checkedAdd(scaledCapacity, pathPadding, outputVerbCapacity)
        || !checkedMultiply(sourcePointCapacity, 2U, scaledCapacity)
        || !checkedMultiply(pathCount, 16U, pathPadding)
        || !checkedAdd(scaledCapacity, pathPadding, outputPointCapacity)) {
        return false;
    }

    reserve(state.temporaryVerbs, singlePathVerbs);
    reserve(state.temporaryPoints, singlePathPoints);
    reserve(state.sourceVerbs, sourceVerbCapacity);
    reserve(state.sourcePoints, sourcePointCapacity);
    reserve(state.candidates, pathCount);
    reserve(state.trimInputs, pathCount);
    reserve(state.combinedVerbs, outputVerbCapacity);
    reserve(state.combinedPoints, outputPointCapacity);
    reserve(state.repeaterCopyCounters, impl_->model->sourceNodes.size());
    if (state.repeaterCopyCounters.size() != impl_->model->sourceNodes.size()) {
        state.repeaterCopyCounters.resize(impl_->model->sourceNodes.size());
        changed = true;
    }

    const auto trimGeneration = state.trimmed.storageGeneration;
    state.trimmed.prepare(
        pathCount,
        outputVerbCapacity,
        outputPointCapacity,
        singlePathVerbs * 2U + 8U,
        singlePathPoints * 2U + 16U);
    changed = changed || state.trimmed.storageGeneration != trimGeneration;
    if (changed) ++state.storageGeneration;
    state.modelFingerprint = impl_->model->parsedModelFingerprint;
    return true;
}

SourceGeometryProjectionResult SourceGeometryProjector::project(
    runtime::EvaluatedScene& scene,
    const evaluation::PropertyEvaluationView& properties,
    SourceGeometryProjectionWorkspace& workspace) const {
    SourceGeometryProjectionResult result;
    if (!valid()) {
        result.error = SourceGeometryProjectionErrorCode::InvalidModel;
        return result;
    }
    if (!properties || properties.properties.size() != impl_->model->properties.size()
        || properties.nodeTransforms.size() != impl_->model->sourceNodes.size()) {
        result.error = SourceGeometryProjectionErrorCode::InvalidEvaluation;
        return result;
    }
    if (!scene.assetModelApplied || !scene.assetModel
        || scene.assetModel->parsedModelFingerprint
            != impl_->model->parsedModelFingerprint
        || scene.sourceAssetHash != impl_->model->sourceAssetHash) {
        result.error = SourceGeometryProjectionErrorCode::InvalidScene;
        return result;
    }
    if (!workspace.impl_
        || workspace.impl_->modelFingerprint
            != impl_->model->parsedModelFingerprint) {
        result.error = SourceGeometryProjectionErrorCode::InvalidWorkspace;
        return result;
    }
    auto& scratch = *workspace.impl_;
    struct WorkspaceCapacityGuard final {
        SourceGeometryProjectionWorkspace::Impl& state;
        std::size_t temporaryVerbCapacity;
        std::size_t temporaryPointCapacity;
        std::size_t sourceVerbCapacity;
        std::size_t sourcePointCapacity;
        std::size_t candidateCapacity;
        std::size_t inputCapacity;
        std::size_t combinedVerbCapacity;
        std::size_t combinedPointCapacity;
        std::size_t repeaterCounterCapacity;
        std::uint64_t trimGeneration;

        ~WorkspaceCapacityGuard() {
            if (state.temporaryVerbs.capacity() != temporaryVerbCapacity
                || state.temporaryPoints.capacity() != temporaryPointCapacity
                || state.sourceVerbs.capacity() != sourceVerbCapacity
                || state.sourcePoints.capacity() != sourcePointCapacity
                || state.candidates.capacity() != candidateCapacity
                || state.trimInputs.capacity() != inputCapacity
                || state.combinedVerbs.capacity() != combinedVerbCapacity
                || state.combinedPoints.capacity() != combinedPointCapacity
                || state.repeaterCopyCounters.capacity() != repeaterCounterCapacity
                || state.trimmed.storageGeneration != trimGeneration) {
                ++state.storageGeneration;
            }
        }
    } capacityGuard{
        scratch,
        scratch.temporaryVerbs.capacity(),
        scratch.temporaryPoints.capacity(),
        scratch.sourceVerbs.capacity(),
        scratch.sourcePoints.capacity(),
        scratch.candidates.capacity(),
        scratch.trimInputs.capacity(),
        scratch.combinedVerbs.capacity(),
        scratch.combinedPoints.capacity(),
        scratch.repeaterCopyCounters.capacity(),
        scratch.trimmed.storageGeneration,
    };

    std::fill(
        scratch.repeaterCopyCounters.begin(),
        scratch.repeaterCopyCounters.end(),
        0U);

    const auto rootComposition = impl_->model->compositions.front().id;
    const auto& rootCompositionRecord = impl_->model->compositions.front();
    const auto rootViewport = viewportMatrix(rootCompositionRecord, scene);
    std::uint32_t activeRepeaterGeometryBinding = model::kInvalidModelId;
    bool activeRepeaterGeometryReady = false;
    bool activeRepeaterGeometryAnimated = false;
    std::uint64_t activeRepeaterGeometryId = 0U;

    auto prepareRepeaterGeometry = [&](const RepeaterBinding& binding) {
        if (binding.geometryBindingIndex
                >= impl_->repeaterGeometryBindings.size()) {
            return false;
        }
        if (activeRepeaterGeometryReady
            && activeRepeaterGeometryBinding
                == binding.geometryBindingIndex) {
            return true;
        }
        activeRepeaterGeometryBinding = binding.geometryBindingIndex;
        activeRepeaterGeometryReady = false;
        activeRepeaterGeometryAnimated = false;
        activeRepeaterGeometryId = 0U;
        scratch.temporaryVerbs.clear();
        scratch.temporaryPoints.clear();
        scratch.combinedVerbs.clear();
        scratch.combinedPoints.clear();

        const auto& geometryBinding = impl_->repeaterGeometryBindings[
            binding.geometryBindingIndex];
        if (!geometryBinding.valid()
            || geometryBinding.repeaterNode != binding.repeaterNode
            || geometryBinding.maximumCopies != binding.maximumCopies
            || geometryBinding.pathNodes.end()
                > impl_->repeaterPathNodes.size()
            || geometryBinding.paintSpaceNode.index()
                >= properties.nodeTransforms.size()) {
            return false;
        }
        const auto& paintSpaceTransform = properties.nodeTransforms[
            geometryBinding.paintSpaceNode.index()];
        if (!paintSpaceTransform.worldSupported()
            || !finite(paintSpaceTransform.worldMatrix)) {
            return false;
        }
        model::MotionMatrix3x2Value inversePaintSpace;
        if (!inverse(paintSpaceTransform.worldMatrix, inversePaintSpace)) {
            return false;
        }

        core::Fnv1a64 identityHash;
        identityHash.appendU64(0x4156455250544745ULL); // "AVERPTGE"
        identityHash.appendU32(geometryBinding.repeaterNode.value);
        identityHash.appendU32(geometryBinding.paintSpaceNode.value);
        identityHash.appendU8(static_cast<std::uint8_t>(geometryBinding.fillRule));
        identityHash.appendU32(geometryBinding.pathNodes.count);
        activeRepeaterGeometryAnimated =
            geometryBinding.relativeTransformAnimated;
        for (std::uint32_t offset = 0U;
             offset < geometryBinding.pathNodes.count; ++offset) {
            const auto pathId = impl_->repeaterPathNodes[
                geometryBinding.pathNodes.first + offset];
            if (pathId.index() >= properties.nodeTransforms.size()) {
                return false;
            }
            const auto& pathTransform = properties.nodeTransforms[
                pathId.index()];
            if (!pathTransform.worldSupported()
                || !finite(pathTransform.worldMatrix)) {
                return false;
            }
            CandidatePathRecord candidate;
            if (!buildCandidatePath(
                    *impl_->model,
                    impl_->propertiesByNode,
                    pathId,
                    properties,
                    scratch.temporaryVerbs,
                    scratch.temporaryPoints,
                    candidate,
                    result.statistics)) {
                return false;
            }
            candidate.relative = multiply(
                pathTransform.worldMatrix, inversePaintSpace);
            if (!finite(candidate.relative)) return false;

            scratch.combinedVerbs.insert(
                scratch.combinedVerbs.end(),
                scratch.temporaryVerbs.begin(),
                scratch.temporaryVerbs.end());
            for (const auto point : scratch.temporaryPoints) {
                const auto transformed = transformPoint(point, candidate.relative);
                if (!finite(transformed.x) || !finite(transformed.y)) {
                    return false;
                }
                scratch.combinedPoints.push_back({
                    transformed.x,
                    transformed.y});
            }
            activeRepeaterGeometryAnimated = activeRepeaterGeometryAnimated
                || candidate.animated;
            identityHash.appendU32(pathId.value);
        }
        activeRepeaterGeometryId = identityHash.value();
        if (activeRepeaterGeometryId == 0U) activeRepeaterGeometryId = 1U;
        activeRepeaterGeometryReady = true;
        return true;
    };

    auto projectRepeater = [&](runtime::EvaluatedDrawItem& item,
                               const RepeaterBinding& binding) {
        ++result.statistics.repeaterCandidates;
        ++result.statistics.repeaterCopiesVisited;
        if (!binding.valid() || item.sourcePaintNode != binding.paintNode
            || binding.paintNode.index()
                >= scratch.repeaterCopyCounters.size()
            || binding.geometryBindingIndex
                >= impl_->repeaterGeometryBindings.size()) {
            ++result.statistics.skippedRepeaterBinding;
            return false;
        }
        const auto& geometryBinding = impl_->repeaterGeometryBindings[
            binding.geometryBindingIndex];
        if (!geometryBinding.valid()
            || item.sourcePathCount != geometryBinding.pathNodes.count
            || !item.localGeometryAvailable
            || item.paint.kind != runtime::PaintKind::Solid) {
            ++result.statistics.skippedRepeaterBinding;
            return false;
        }

        const auto copyIndex = scratch.repeaterCopyCounters[
            binding.paintNode.index()]++;
        if (copyIndex >= binding.maximumCopies) {
            ++result.statistics.rejectedRepeaterInput;
            return false;
        }
        if (!prepareRepeaterGeometry(binding)) {
            ++result.statistics.rejectedRepeaterInput;
            return false;
        }

        RepeaterPaintSample paintSample;
        if (!evaluateRepeaterPaint(
                *impl_->model,
                impl_->propertiesByNode,
                binding,
                properties,
                paintSample)) {
            ++result.statistics.skippedRepeaterProperties;
            return false;
        }

        const auto& ids = impl_->propertiesByNode[binding.repeaterNode.index()];
        const auto* copiesProperty = property(*impl_->model, ids.repeaterCopies);
        const auto* offsetProperty = property(*impl_->model, ids.repeaterOffset);
        const auto* positionProperty = property(*impl_->model, ids.repeaterPosition);
        const auto* scaleProperty = property(*impl_->model, ids.repeaterScale);
        const auto* rotationProperty = property(*impl_->model, ids.repeaterRotation);
        const auto* anchorProperty = property(*impl_->model, ids.repeaterAnchor);
        const auto* startOpacityProperty = property(
            *impl_->model, ids.repeaterStartOpacity);
        const auto* endOpacityProperty = property(
            *impl_->model, ids.repeaterEndOpacity);
        if (!copiesProperty || !offsetProperty || !positionProperty
            || !scaleProperty || !rotationProperty || !anchorProperty
            || !startOpacityProperty || !endOpacityProperty) {
            ++result.statistics.skippedRepeaterProperties;
            return false;
        }

        ScalarSample copies;
        ScalarSample offset;
        Vec2Sample position;
        Vec2Sample scale;
        ScalarSample rotation;
        Vec2Sample anchor;
        ScalarSample startOpacity;
        ScalarSample endOpacity;
        if (!resolveScalar(*copiesProperty, properties, copies)
            || !resolveScalar(*offsetProperty, properties, offset)
            || !resolveVec2(*positionProperty, properties, position)
            || !resolveVec2(*scaleProperty, properties, scale)
            || !resolveScalar(*rotationProperty, properties, rotation)
            || !resolveVec2(*anchorProperty, properties, anchor)
            || !resolveScalar(*startOpacityProperty, properties, startOpacity)
            || !resolveScalar(*endOpacityProperty, properties, endOpacity)) {
            ++result.statistics.skippedRepeaterEvaluation;
            return false;
        }

        const auto* repeaterNode = impl_->model->sourceNode(binding.repeaterNode);
        const auto* paintNode = impl_->model->sourceNode(binding.paintNode);
        if (!repeaterNode || !paintNode || !repeaterNode->parent.valid()
            || repeaterNode->composition != rootComposition
            || repeaterNode->parent.index() >= properties.nodeTransforms.size()
            || binding.paintNode.index() >= properties.nodeTransforms.size()) {
            ++result.statistics.skippedRepeaterBinding;
            return false;
        }
        const auto& parent = properties.nodeTransforms[
            repeaterNode->parent.index()];
        const auto& paintTransform = properties.nodeTransforms[
            binding.paintNode.index()];
        if (!parent.worldSupported() || !paintTransform.worldSupported()
            || !finite(parent.worldMatrix) || !finite(parent.worldOpacity)
            || !finite(paintTransform.worldMatrix)
            || !finite(paintTransform.worldOpacity)) {
            ++result.statistics.skippedTransform;
            return false;
        }
        model::MotionMatrix3x2Value inverseParent;
        if (!inverse(parent.worldMatrix, inverseParent)) {
            ++result.statistics.skippedTransform;
            return false;
        }
        const auto paintRelative = multiply(
            paintTransform.worldMatrix, inverseParent);
        if (!finite(paintRelative)) {
            ++result.statistics.skippedTransform;
            return false;
        }

        const detail::RepeaterTransformSample sample{
            copies.value,
            offset.value,
            position.value,
            scale.value,
            rotation.value,
            anchor.value,
            startOpacity.value,
            endOpacity.value,
        };
        const auto copy = detail::evaluateRepeaterCopy(sample, copyIndex);
        if (!copy.valid
            || static_cast<std::uint32_t>(copy.visibleCopies)
                > binding.maximumCopies) {
            ++result.statistics.rejectedRepeaterInput;
            return false;
        }
        const auto expectedMatrix = multiply(
            multiply(
                multiply(paintRelative, copy.localMatrix),
                parent.worldMatrix),
            rootViewport);
        const float expectedOpacity = paintTransform.worldOpacity
            * copy.opacityMultiplier;
        if (!finite(expectedMatrix) || !finite(expectedOpacity)
            || expectedOpacity < 0.0F || expectedOpacity > 1.0F) {
            ++result.statistics.rejectedRepeaterInput;
            return false;
        }

        const auto expectedFinalColor = applySeparatedOpacity(
            paintSample.paint.solid, expectedOpacity);
        bool paintParity = item.paint.kind == runtime::PaintKind::Solid
            && sameColor(item.paint.solid, expectedFinalColor);
        if (binding.paintKind == RepeaterPaintKind::SolidFill) {
            paintParity = paintParity && !item.stroke.enabled;
        } else if (binding.paintKind == RepeaterPaintKind::SolidStroke) {
            const auto expectedWidth = paintSample.stroke.width
                * telegramStrokeScale(expectedMatrix);
            paintParity = paintParity && item.stroke.enabled
                && item.stroke.dashArray.empty()
                && std::fabs(item.stroke.width - expectedWidth)
                    <= kParityTolerance
                && std::fabs(
                    item.stroke.miterLimit - paintSample.stroke.miterLimit)
                    <= kParityTolerance
                && item.stroke.cap == paintSample.stroke.cap
                && item.stroke.join == paintSample.stroke.join;
        } else {
            paintParity = false;
        }

        const model::MotionMatrix3x2Value identity{};
        if (!pathMatchesTransformed(
                scratch.combinedVerbs,
                scratch.combinedPoints,
                identity,
                item.localPath)
            || !pathMatchesTransformed(
                scratch.combinedVerbs,
                scratch.combinedPoints,
                expectedMatrix,
                item.path)
            || !nearMatrix(expectedMatrix, item.localToViewport)
            || !paintParity) {
            ++result.statistics.repeaterParityMismatches;
            return false;
        }

        materializeTransformed(
            scratch.combinedVerbs,
            scratch.combinedPoints,
            identity,
            item.localPath);
        item.localToViewport = affineValue(expectedMatrix);
        item.localPaintAvailable = true;
        item.localPaintStaticCandidate = !paintSample.animated;
        item.localPaint = paintSample.paint;
        item.localStroke = paintSample.stroke;
        item.separatedOpacity = expectedOpacity;
        item.opacitySeparated = true;
        item.localGeometryAvailable = true;
        item.sourceGeometryProjected = true;
        item.sourceRepeaterProjected = true;
        item.sourceRepeaterNode = binding.repeaterNode;
        item.sourceRepeaterCopyIndex = copyIndex;
        item.sourceRepeaterVisibleCopies = static_cast<std::uint32_t>(
            copy.visibleCopies);
        item.sourceRepeaterMaximumCopies = binding.maximumCopies;
        item.sourceRepeaterTransformRevision = matrixRevision(expectedMatrix);
        item.sourceRepeaterOpacityRevision = scalarRevision(expectedOpacity);
        item.sourceGeometryId = activeRepeaterGeometryId;
        item.geometryOrigin = activeRepeaterGeometryAnimated
            ? runtime::EvaluatedValueOrigin::InstanceEvaluated
            : runtime::EvaluatedValueOrigin::AssetStatic;
        item.sourceGeometryRevision = activeRepeaterGeometryAnimated
            ? (item.localPath.hash == 0U ? 1U : item.localPath.hash)
            : 0U;
        item.sourceGeometryRevisionAuthoritative = true;
        item.modelGeometry = {};
        item.canonicalGeometry.reset();

        core::Fnv1a64 paintIdentityHash;
        paintIdentityHash.appendU64(0x4156455250545041ULL); // "AVERPTPA"
        paintIdentityHash.appendU32(binding.repeaterNode.value);
        paintIdentityHash.appendU32(binding.paintNode.value);
        item.sourcePaintId = paintIdentityHash.value();
        if (item.sourcePaintId == 0U) item.sourcePaintId = 1U;
        item.paintOrigin = paintSample.animated
            ? runtime::EvaluatedValueOrigin::InstanceEvaluated
            : runtime::EvaluatedValueOrigin::AssetStatic;
        item.modelPaint = {};
        item.canonicalPaint.reset();
        item.fillRule = geometryBinding.fillRule;

        ++result.statistics.projected;
        ++result.statistics.repeaterCopiesProjected;
        if (binding.paintKind == RepeaterPaintKind::SolidFill) {
            ++result.statistics.repeaterFillApplicationsProjected;
        } else {
            ++result.statistics.repeaterStrokeApplicationsProjected;
        }
        if (paintNode->parent != binding.contentNode) {
            ++result.statistics.repeaterNestedPaintApplicationsProjected;
        }
        if (paintSample.animated) {
            ++result.statistics.repeaterAnimatedPaintApplicationsProjected;
        }
        result.statistics.projectedPoints += item.localPath.points.size();
        if (copy.visible) {
            ++result.statistics.repeaterVisibleCopies;
        } else {
            ++result.statistics.repeaterHiddenCopies;
        }
        if (activeRepeaterGeometryAnimated) {
            ++result.statistics.projectedAnimated;
            ++result.statistics.repeaterAnimatedGeometryCopies;
        } else {
            ++result.statistics.projectedStatic;
            ++result.statistics.repeaterStaticGeometryCopies;
        }
        if (offset.animated || position.animated || scale.animated
            || rotation.animated || anchor.animated) {
            ++result.statistics.repeaterTransformAnimatedCopies;
        }
        if (copies.animated || startOpacity.animated || endOpacity.animated) {
            ++result.statistics.repeaterOpacityAnimatedCopies;
        }
        return true;
    };

    for (auto& item : scene.drawItems) {
        ++result.statistics.drawItemsVisited;
        if (!item.sourcePaintNode.valid()) {
            ++result.statistics.skippedUnbound;
            continue;
        }
        const auto repeaterBinding = item.sourcePaintNode.index()
                < impl_->repeaterByPaintNode.size()
            ? impl_->repeaterByPaintNode[item.sourcePaintNode.index()]
            : RepeaterBinding{};
        if (repeaterBinding.valid()
            && projectRepeater(item, repeaterBinding)) {
            continue;
        }
        const auto paintTrimBinding = item.sourcePaintNode.index()
                < impl_->trimByPaintNode.size()
            ? impl_->trimByPaintNode[item.sourcePaintNode.index()]
            : TrimBinding{};
        const auto pathTrimBinding = item.sourcePathNode.valid()
                && item.sourcePathNode.index() < impl_->trimByPathNode.size()
            ? impl_->trimByPathNode[item.sourcePathNode.index()]
            : TrimBinding{};
        const auto trimBinding = paintTrimBinding.valid()
                && paintTrimBinding.pathNodes.count == item.sourcePathCount
            ? paintTrimBinding
            : pathTrimBinding;
        const bool hasSupportedTrim = !item.sourcePathModifierFree
            && trimBinding.valid()
            && trimBinding.pathNodes.count == item.sourcePathCount;
        if (!item.sourcePathNode.valid() && !hasSupportedTrim) {
            ++result.statistics.skippedUnbound;
            continue;
        }
        ++result.statistics.candidates;
        if (!item.sourcePathModifierFree && !hasSupportedTrim) {
            ++result.statistics.skippedModified;
            continue;
        }
        if (item.sourcePathCount != 1U && !hasSupportedTrim) {
            ++result.statistics.skippedMultiplePaths;
            continue;
        }
        if (hasSupportedTrim) {
            ++result.statistics.trimCandidates;
            if (trimBinding.mode == model::SourceTrimMode::Simultaneous) {
                ++result.statistics.trimSimultaneousCandidates;
            } else {
                ++result.statistics.trimIndividualCandidates;
            }
            result.statistics.trimPathsVisited += trimBinding.pathNodes.count;
            if (trimBinding.pathNodes.count > 1U) {
                ++result.statistics.trimMultiPathCandidates;
            }
        }
        const auto* paintNode = impl_->model->sourceNode(item.sourcePaintNode);
        if (!paintNode) {
            result.error = SourceGeometryProjectionErrorCode::CorruptBinding;
            return result;
        }
        const auto* itemPathNode = item.sourcePathNode.valid()
            ? impl_->model->sourceNode(item.sourcePathNode) : nullptr;
        if (item.sourcePathNode.valid() && !itemPathNode) {
            result.error = SourceGeometryProjectionErrorCode::CorruptBinding;
            return result;
        }
        const bool nestedCompositionCandidate =
            paintNode->composition != rootComposition
            || (itemPathNode
                && itemPathNode->composition != rootComposition);
        if (nestedCompositionCandidate) {
            ++result.statistics.nestedCompositionCandidates;
        }
        if (paintNode->kind != model::SourceNodeKind::Fill
            || !item.localPaintAvailable
            || item.localPaint.kind != runtime::PaintKind::Solid) {
            ++result.statistics.skippedPaint;
            continue;
        }
        if (item.sourcePaintNode.index() >= properties.nodeTransforms.size()) {
            result.error = SourceGeometryProjectionErrorCode::CorruptBinding;
            return result;
        }
        const auto& paintTransform =
            properties.nodeTransforms[item.sourcePaintNode.index()];
        if (!paintTransform.worldSupported()) {
            ++result.statistics.skippedTransform;
            continue;
        }
        model::MotionMatrix3x2Value inversePaint;
        if (!inverse(paintTransform.worldMatrix, inversePaint)) {
            ++result.statistics.skippedTransform;
            continue;
        }

        if (hasSupportedTrim) {
            if (trimBinding.pathNodes.end() > impl_->trimPathNodes.size()) {
                result.error = SourceGeometryProjectionErrorCode::CorruptBinding;
                return result;
            }
            scratch.temporaryVerbs.clear();
            scratch.temporaryPoints.clear();
            scratch.sourceVerbs.clear();
            scratch.sourcePoints.clear();
            scratch.candidates.clear();
            scratch.trimInputs.clear();
            scratch.combinedVerbs.clear();
            scratch.combinedPoints.clear();

            bool groupAnimated = false;
            bool candidateFailure = false;
            for (std::uint32_t offset = 0U;
                 offset < trimBinding.pathNodes.count; ++offset) {
                const auto pathId = impl_->trimPathNodes[
                    trimBinding.pathNodes.first + offset];
                if (!pathId.valid()
                    || pathId.index() >= properties.nodeTransforms.size()) {
                    result.error = SourceGeometryProjectionErrorCode::CorruptBinding;
                    return result;
                }
                const auto* pathNode = impl_->model->sourceNode(pathId);
                if (!pathNode || pathNode->composition != rootComposition) {
                    candidateFailure = true;
                    break;
                }
                const auto& pathTransform = properties.nodeTransforms[pathId.index()];
                if (!pathTransform.worldSupported()) {
                    ++result.statistics.skippedTransform;
                    candidateFailure = true;
                    break;
                }

                CandidatePathRecord candidate;
                if (!buildCandidatePath(
                        *impl_->model,
                        impl_->propertiesByNode,
                        pathId,
                        properties,
                        scratch.temporaryVerbs,
                        scratch.temporaryPoints,
                        candidate,
                        result.statistics)) {
                    ++result.statistics.rejectedTrimInput;
                    candidateFailure = true;
                    break;
                }
                if (scratch.sourceVerbs.size()
                        > static_cast<std::size_t>(model::kInvalidModelId)
                            - scratch.temporaryVerbs.size()
                    || scratch.sourcePoints.size()
                        > static_cast<std::size_t>(model::kInvalidModelId)
                            - scratch.temporaryPoints.size()) {
                    result.error = SourceGeometryProjectionErrorCode::CorruptShape;
                    return result;
                }
                candidate.verbs = {
                    static_cast<std::uint32_t>(scratch.sourceVerbs.size()),
                    static_cast<std::uint32_t>(scratch.temporaryVerbs.size())};
                candidate.points = {
                    static_cast<std::uint32_t>(scratch.sourcePoints.size()),
                    static_cast<std::uint32_t>(scratch.temporaryPoints.size())};
                candidate.relative = multiply(
                    pathTransform.worldMatrix, inversePaint);
                if (!finite(candidate.relative)) {
                    ++result.statistics.skippedTransform;
                    candidateFailure = true;
                    break;
                }
                scratch.sourceVerbs.insert(
                    scratch.sourceVerbs.end(),
                    scratch.temporaryVerbs.begin(),
                    scratch.temporaryVerbs.end());
                scratch.sourcePoints.insert(
                    scratch.sourcePoints.end(),
                    scratch.temporaryPoints.begin(),
                    scratch.temporaryPoints.end());
                groupAnimated = groupAnimated || candidate.animated;
                scratch.candidates.push_back(candidate);
            }
            if (candidateFailure
                || scratch.candidates.size() != trimBinding.pathNodes.count) {
                continue;
            }

            for (const auto& candidate : scratch.candidates) {
                if (candidate.verbs.end() > scratch.sourceVerbs.size()
                    || candidate.points.end() > scratch.sourcePoints.size()) {
                    result.error = SourceGeometryProjectionErrorCode::CorruptShape;
                    return result;
                }
                scratch.trimInputs.push_back({
                    std::span<const runtime::PathVerb>{
                        scratch.sourceVerbs.data() + candidate.verbs.first,
                        candidate.verbs.count},
                    std::span<const model::MotionVec2Value>{
                        scratch.sourcePoints.data() + candidate.points.first,
                        candidate.points.count},
                });
            }

            const auto& trimIds = impl_->propertiesByNode[
                trimBinding.trimNode.index()];
            const auto* startProperty = property(*impl_->model, trimIds.trimStart);
            const auto* endProperty = property(*impl_->model, trimIds.trimEnd);
            const auto* offsetProperty = property(*impl_->model, trimIds.trimOffset);
            if (!startProperty || !endProperty || !offsetProperty) {
                ++result.statistics.skippedTrimProperties;
                continue;
            }
            ScalarSample start;
            ScalarSample end;
            ScalarSample offset;
            if (!resolveScalar(*startProperty, properties, start)
                || !resolveScalar(*endProperty, properties, end)
                || !resolveScalar(*offsetProperty, properties, offset)) {
                ++result.statistics.skippedTrimEvaluation;
                continue;
            }
            groupAnimated = groupAnimated || start.animated || end.animated
                || offset.animated;
            const auto segment = detail::normalizeTrimSegment(
                start.value, end.value, offset.value);
            const auto mode = trimBinding.mode == model::SourceTrimMode::Individual
                ? detail::TrimPathCollectionMode::Individual
                : detail::TrimPathCollectionMode::Simultaneous;
            if (!detail::trimPathCollection(
                    scratch.trimInputs, segment, mode, scratch.trimmed)) {
                ++result.statistics.rejectedTrimInput;
                continue;
            }

            for (std::size_t index = 0U;
                 index < scratch.candidates.size(); ++index) {
                const auto verbs = scratch.trimmed.pathVerbs(index);
                const auto points = scratch.trimmed.pathPoints(index);
                const auto& candidate = scratch.candidates[index];
                scratch.combinedVerbs.insert(
                    scratch.combinedVerbs.end(), verbs.begin(), verbs.end());
                for (const auto point : points) {
                    const auto transformed = transformPoint(point, candidate.relative);
                    if (!finite(transformed.x) || !finite(transformed.y)) {
                        candidateFailure = true;
                        break;
                    }
                    scratch.combinedPoints.push_back(
                        {transformed.x, transformed.y});
                }
                if (candidateFailure) break;
            }
            if (candidateFailure) {
                ++result.statistics.rejectedTrimInput;
                continue;
            }

            const model::MotionMatrix3x2Value identity{};
            const auto viewportMatrix = matrixValue(item.localToViewport);
            if (!pathMatchesTransformed(
                    scratch.combinedVerbs,
                    scratch.combinedPoints,
                    identity,
                    item.localPath)
                || !pathMatchesTransformed(
                    scratch.combinedVerbs,
                    scratch.combinedPoints,
                    viewportMatrix,
                    item.path)) {
                ++result.statistics.parityMismatches;
                continue;
            }
            materializeTransformed(
                scratch.combinedVerbs,
                scratch.combinedPoints,
                identity,
                item.localPath);

            item.localGeometryAvailable = true;
            item.sourceGeometryProjected = true;
            core::Fnv1a64 identityHash;
            identityHash.appendU32(item.sourcePaintNode.value);
            identityHash.appendU32(trimBinding.trimNode.value);
            identityHash.appendU32(trimBinding.pathNodes.count);
            for (const auto& candidate : scratch.candidates) {
                identityHash.appendU32(candidate.sourceNode.value);
            }
            item.sourceGeometryId = identityHash.value();
            const auto assetStatic = !groupAnimated
                && item.localGeometryStaticCandidate;
            item.geometryOrigin = assetStatic
                ? runtime::EvaluatedValueOrigin::AssetStatic
                : runtime::EvaluatedValueOrigin::InstanceEvaluated;
            item.sourceGeometryRevision = assetStatic
                ? 0U : (item.localPath.hash == 0U ? 1U : item.localPath.hash);
            item.sourceGeometryRevisionAuthoritative = true;
            item.modelGeometry = {};
            item.fillRule = paintNode->fillRule == model::SourceFillRule::EvenOdd
                ? runtime::FillRule::EvenOdd
                : runtime::FillRule::Winding;

            ++result.statistics.projected;
            ++result.statistics.projectedTrimmed;
            result.statistics.trimPathsProjected += scratch.candidates.size();
            result.statistics.projectedPoints += item.localPath.points.size();
            result.statistics.trimSplitCount += scratch.trimmed.splitCount;
            if (scratch.trimmed.individualWrappedNoOp) {
                ++result.statistics.trimIndividualWrappedNoOps;
            }
            if (scratch.trimmed.empty) {
                ++result.statistics.projectedTrimEmpty;
            } else if (scratch.trimmed.full
                || scratch.trimmed.individualWrappedNoOp) {
                ++result.statistics.projectedTrimFull;
            } else {
                ++result.statistics.projectedTrimPartial;
            }
            if (assetStatic) {
                ++result.statistics.projectedStatic;
            } else {
                ++result.statistics.projectedAnimated;
            }
            for (const auto& candidate : scratch.candidates) {
                switch (candidate.primitiveKind) {
                case PrimitiveKind::Rectangle:
                    if (candidate.roundedRectangle) {
                        ++result.statistics.projectedRoundedRectangles;
                    } else {
                        ++result.statistics.projectedRectangles;
                    }
                    break;
                case PrimitiveKind::Ellipse:
                    ++result.statistics.projectedEllipses;
                    break;
                case PrimitiveKind::Star:
                    ++result.statistics.projectedStars;
                    break;
                case PrimitiveKind::Polygon:
                    ++result.statistics.projectedPolygons;
                    break;
                case PrimitiveKind::None:
                    break;
                }
            }
            continue;
        }

        const auto* pathNode = impl_->model->sourceNode(item.sourcePathNode);
        if (!pathNode) {
            result.error = SourceGeometryProjectionErrorCode::CorruptBinding;
            return result;
        }
        if (item.sourcePathNode.index() >= properties.nodeTransforms.size()) {
            result.error = SourceGeometryProjectionErrorCode::CorruptBinding;
            return result;
        }
        const auto& pathTransform =
            properties.nodeTransforms[item.sourcePathNode.index()];
        if (!pathTransform.worldSupported()) {
            ++result.statistics.skippedTransform;
            continue;
        }
        const auto relative = multiply(pathTransform.worldMatrix, inversePaint);

        bool materialized = false;
        bool animated = false;
        bool roundedRectangle = false;
        PrimitiveKind primitiveKind = PrimitiveKind::None;

        const auto& ids = impl_->propertiesByNode[item.sourcePathNode.index()];
        switch (pathNode->kind) {
        case model::SourceNodeKind::Shape: {
            const auto* sourceProperty = property(*impl_->model, ids.shape);
            if (!sourceProperty) {
                ++result.statistics.skippedNoShapeProperty;
                continue;
            }
            ShapeSample shape;
            if (!resolveShape(*impl_->model, *sourceProperty, properties, shape)) {
                ++result.statistics.skippedShapeEvaluation;
                continue;
            }
            animated = shape.animated;
            materialized = matchesAndMaterializesShape(
                shape, relative, item.localPath);
            break;
        }
        case model::SourceNodeKind::Rectangle: {
            ++result.statistics.primitiveCandidates;
            ++result.statistics.rectangleCandidates;
            primitiveKind = PrimitiveKind::Rectangle;
            const auto* positionProperty = property(*impl_->model, ids.position);
            const auto* sizeProperty = property(*impl_->model, ids.size);
            const auto* roundnessProperty = property(*impl_->model, ids.roundness);
            if (!positionProperty || !sizeProperty || !roundnessProperty) {
                ++result.statistics.skippedPrimitiveProperties;
                continue;
            }
            Vec2Sample position;
            Vec2Sample size;
            ScalarSample roundness;
            if (!resolveVec2(*positionProperty, properties, position)
                || !resolveVec2(*sizeProperty, properties, size)
                || !resolveScalar(*roundnessProperty, properties, roundness)) {
                ++result.statistics.skippedPrimitiveEvaluation;
                continue;
            }
            const auto generated = detail::generateRectanglePath(
                position.value,
                size.value,
                roundness.value,
                pathNode->pathDirection);
            roundedRectangle = generated.rounded;
            animated = position.animated || size.animated || roundness.animated;
            materialized = matchesAndMaterializesPrimitive(
                generated, relative, item.localPath);
            break;
        }
        case model::SourceNodeKind::Ellipse: {
            ++result.statistics.primitiveCandidates;
            ++result.statistics.ellipseCandidates;
            primitiveKind = PrimitiveKind::Ellipse;
            const auto* positionProperty = property(*impl_->model, ids.position);
            const auto* sizeProperty = property(*impl_->model, ids.size);
            if (!positionProperty || !sizeProperty) {
                ++result.statistics.skippedPrimitiveProperties;
                continue;
            }
            Vec2Sample position;
            Vec2Sample size;
            if (!resolveVec2(*positionProperty, properties, position)
                || !resolveVec2(*sizeProperty, properties, size)) {
                ++result.statistics.skippedPrimitiveEvaluation;
                continue;
            }
            const auto generated = detail::generateEllipsePath(
                position.value,
                size.value,
                pathNode->pathDirection);
            animated = position.animated || size.animated;
            materialized = matchesAndMaterializesPrimitive(
                generated, relative, item.localPath);
            break;
        }
        case model::SourceNodeKind::Polystar: {
            ++result.statistics.primitiveCandidates;
            ++result.statistics.polystarCandidates;
            const auto* positionProperty = property(*impl_->model, ids.position);
            const auto* pointCountProperty = property(*impl_->model, ids.pointCount);
            const auto* outerRadiusProperty = property(*impl_->model, ids.outerRadius);
            const auto* outerRoundnessProperty = property(
                *impl_->model, ids.outerRoundness);
            const auto* rotationProperty = property(*impl_->model, ids.rotation);
            if (!positionProperty || !pointCountProperty || !outerRadiusProperty
                || !outerRoundnessProperty || !rotationProperty) {
                ++result.statistics.skippedPrimitiveProperties;
                ++result.statistics.skippedPolystarProperties;
                continue;
            }

            Vec2Sample position;
            ScalarSample pointCount;
            ScalarSample outerRadius;
            ScalarSample outerRoundness;
            ScalarSample rotation;
            if (!resolveVec2(*positionProperty, properties, position)
                || !resolveScalar(*pointCountProperty, properties, pointCount)
                || !resolveScalar(*outerRadiusProperty, properties, outerRadius)
                || !resolveScalar(
                    *outerRoundnessProperty, properties, outerRoundness)
                || !resolveScalar(*rotationProperty, properties, rotation)) {
                ++result.statistics.skippedPrimitiveEvaluation;
                ++result.statistics.skippedPolystarEvaluation;
                continue;
            }

            detail::PrimitivePath generated;
            animated = position.animated || pointCount.animated
                || outerRadius.animated || outerRoundness.animated
                || rotation.animated;
            if (pathNode->polystarType == model::SourcePolystarType::Star) {
                ++result.statistics.starCandidates;
                primitiveKind = PrimitiveKind::Star;
                const auto* innerRadiusProperty = property(
                    *impl_->model, ids.innerRadius);
                const auto* innerRoundnessProperty = property(
                    *impl_->model, ids.innerRoundness);
                if (!innerRadiusProperty || !innerRoundnessProperty) {
                    ++result.statistics.skippedPrimitiveProperties;
                    ++result.statistics.skippedPolystarProperties;
                    continue;
                }
                ScalarSample innerRadius;
                ScalarSample innerRoundness;
                if (!resolveScalar(
                        *innerRadiusProperty, properties, innerRadius)
                    || !resolveScalar(
                        *innerRoundnessProperty, properties, innerRoundness)) {
                    ++result.statistics.skippedPrimitiveEvaluation;
                    ++result.statistics.skippedPolystarEvaluation;
                    continue;
                }
                animated = animated || innerRadius.animated
                    || innerRoundness.animated;
                generated = detail::generatePolystarPath(
                    position.value,
                    pointCount.value,
                    innerRadius.value,
                    outerRadius.value,
                    innerRoundness.value,
                    outerRoundness.value,
                    rotation.value,
                    pathNode->pathDirection);
            } else if (pathNode->polystarType
                == model::SourcePolystarType::Polygon) {
                ++result.statistics.polygonCandidates;
                primitiveKind = PrimitiveKind::Polygon;
                generated = detail::generatePolygonPath(
                    position.value,
                    pointCount.value,
                    outerRadius.value,
                    outerRoundness.value,
                    rotation.value,
                    pathNode->pathDirection);
            } else {
                ++result.statistics.rejectedPolystarInputs;
                continue;
            }

            if (!generated.valid) {
                ++result.statistics.rejectedPolystarInputs;
                continue;
            }
            materialized = matchesAndMaterializesPrimitive(
                generated, relative, item.localPath);
            break;
        }
        default:
            ++result.statistics.skippedWrongNodeKind;
            continue;
        }

        if (!materialized) {
            ++result.statistics.parityMismatches;
            continue;
        }

        item.localGeometryAvailable = true;
        item.sourceGeometryProjected = true;
        item.sourceGeometryId = static_cast<std::uint64_t>(
            item.sourcePathNode.value) + 1U;
        const auto assetStatic = !animated && item.localGeometryStaticCandidate;
        item.geometryOrigin = assetStatic
            ? runtime::EvaluatedValueOrigin::AssetStatic
            : runtime::EvaluatedValueOrigin::InstanceEvaluated;
        item.sourceGeometryRevision = assetStatic
            ? 0U
            : (item.localPath.hash == 0U ? 1U : item.localPath.hash);
        item.sourceGeometryRevisionAuthoritative = true;
        item.modelGeometry = {};
        item.fillRule = paintNode->fillRule == model::SourceFillRule::EvenOdd
            ? runtime::FillRule::EvenOdd
            : runtime::FillRule::Winding;

        ++result.statistics.projected;
        result.statistics.projectedPoints += item.localPath.points.size();
        if (assetStatic) {
            ++result.statistics.projectedStatic;
        } else {
            ++result.statistics.projectedAnimated;
        }
        if (primitiveKind == PrimitiveKind::Rectangle) {
            if (roundedRectangle) {
                ++result.statistics.projectedRoundedRectangles;
            } else {
                ++result.statistics.projectedRectangles;
            }
        } else if (primitiveKind == PrimitiveKind::Ellipse) {
            ++result.statistics.projectedEllipses;
        } else if (primitiveKind == PrimitiveKind::Star) {
            ++result.statistics.projectedStars;
        } else if (primitiveKind == PrimitiveKind::Polygon) {
            ++result.statistics.projectedPolygons;
        }
    }
    return result;
}

SourceGeometryProjectionResult SourceGeometryProjector::project(
    runtime::EvaluatedScene& scene,
    const evaluation::PropertyEvaluationView& properties) const {
    SourceGeometryProjectionWorkspace workspace;
    if (!prepare(workspace)) {
        SourceGeometryProjectionResult result;
        result.error = SourceGeometryProjectionErrorCode::InvalidWorkspace;
        return result;
    }
    return project(scene, properties, workspace);
}

} // namespace avemotion::render
