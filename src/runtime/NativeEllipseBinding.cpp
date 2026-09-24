#include "NativeEllipseBinding.hpp"

#include "avemotion/core/Hash.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <limits>
#include <string>

namespace avemotion::runtime::detail {
namespace {
using namespace model;

[[nodiscard]] NativeEllipseBindingResult fail(NativeEllipseBindingCode code) {
    return {code, std::nullopt};
}

[[nodiscard]] bool canonical(const NativeEllipseDecimal& value) {
    const auto digits = [](const std::string& text) {
        return !text.empty() && std::all_of(text.begin(), text.end(), [](char c) {
            return c >= '0' && c <= '9';
        });
    };
    if (!digits(value.digits) || !digits(value.power.magnitude)) return false;
    if (value.power.magnitude.size() > 1 && value.power.magnitude.front() == '0') return false;
    if (value.power.magnitude == "0" && value.power.negative) return false;
    if (value.digits == "0") {
        return !value.negative && !value.power.negative && value.power.magnitude == "0";
    }
    return value.digits.front() != '0' && value.digits.back() != '0';
}

[[nodiscard]] bool convert(const NativeEllipseDecimal& value, float& output,
                           bool positive = false) {
    if (!canonical(value)) return false;
    std::string token;
    token.reserve(value.digits.size() + value.power.magnitude.size() + 3);
    if (value.negative) token += '-';
    token += value.digits;
    token += 'e';
    token += value.power.negative ? '-' : '+';
    token += value.power.magnitude;
    double parsed = 0.0;
    const auto converted = std::from_chars(token.data(), token.data() + token.size(), parsed);
    if (converted.ec != std::errc{} || converted.ptr != token.data() + token.size()
        || !std::isfinite(parsed) || std::abs(parsed) > std::numeric_limits<float>::max()) {
        return false;
    }
    output = static_cast<float>(parsed);
    if (!std::isfinite(output) || (value.digits != "0" && output == 0.0F)
        || (positive && output <= 0.0F)) return false;
    if (output == 0.0F) output = 0.0F;
    return true;
}

[[nodiscard]] bool convert(const NativeEllipseVec2& value, MotionVec2Value& output,
                           bool positive = false) {
    return convert(value[0], output.x, positive) && convert(value[1], output.y, positive);
}

[[nodiscard]] bool range(IndexRange value, std::size_t size) noexcept {
    const auto first = static_cast<std::size_t>(value.first);
    return first <= size && static_cast<std::size_t>(value.count) <= size - first;
}

[[nodiscard]] std::uint64_t nameHash(const std::string& name) noexcept {
    core::Fnv1a64 hash;
    hash.appendString(name);
    return hash.value();
}

[[nodiscard]] std::string effective(const std::optional<std::string>& name,
                                    std::string fallback) {
    return name && !name->empty() ? *name : std::move(fallback);
}

[[nodiscard]] bool finite(const MotionVec2Value& value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y);
}
[[nodiscard]] bool finite(const MotionColorValue& value) noexcept {
    return std::isfinite(value.r) && std::isfinite(value.g)
        && std::isfinite(value.b) && std::isfinite(value.a);
}
[[nodiscard]] bool finite(const MotionMatrix3x2Value& value) noexcept {
    return std::isfinite(value.m11) && std::isfinite(value.m12)
        && std::isfinite(value.m21) && std::isfinite(value.m22)
        && std::isfinite(value.dx) && std::isfinite(value.dy);
}

[[nodiscard]] bool defaultNode(const MotionSourceNodeRecord& node,
                               bool animated) noexcept {
    return node.present && !node.hidden && node.enabled && !node.autoOrient
        && !node.transformParent.valid() && !node.referencedComposition.valid()
        && node.authoredStatic == !animated
        && node.dependencyBits == (animated ? StaticDependencyTimeline : StaticDependencyNone)
        && node.authoredParentLayerId == -1
        && node.matteMode == SourceMatteMode::None
        && node.maskMode == SourceMaskMode::None && !node.maskInverted
        && node.blendMode == SourceBlendMode::Normal
        && node.fillRule == SourceFillRule::Winding
        && node.strokeCap == SourceStrokeCap::Flat
        && node.strokeJoin == SourceStrokeJoin::Miter
        && node.gradientType == SourceGradientType::None
        && node.pathDirection == SourcePathDirection::Clockwise
        && node.polystarType == SourcePolystarType::None
        && node.trimMode == SourceTrimMode::None
        && node.miterLimit == 0.0F && node.repeaterMaximumCopies == 0.0F
        && node.gradientColorPointCount == 0 && node.layerWidth == 0
        && node.layerHeight == 0 && finite(node.solidColor)
        && node.solidColor == MotionColorValue{0, 0, 0, 1}
        && node.sourceAssetRefHash == 0
        && std::isfinite(node.inFrame) && std::isfinite(node.outFrame)
        && std::isfinite(node.startFrame) && std::isfinite(node.timeStretch)
        && node.startFrame == 0.0 && node.timeStretch == 1.0F;
}

[[nodiscard]] bool valueRef(const MotionAssetModel& model, MotionValueRef ref,
                            PropertyValueType type) noexcept {
    if (!ref.valid() || ref.type != type) return false;
    switch (type) {
    case PropertyValueType::Scalar: return ref.index < model.scalarValues.size();
    case PropertyValueType::Vec2: return ref.index < model.vec2Values.size();
    case PropertyValueType::Color: return ref.index < model.colorValues.size();
    case PropertyValueType::Matrix3x2: return ref.index < model.matrixValues.size();
    default: return false;
    }
}

[[nodiscard]] bool tableShape(const MotionAssetModel& model, bool animated) noexcept {
    const auto& stats = model.statistics;
    return model.schemaVersion == MotionAssetModel::kSchemaVersion && stats.directParsedModel
        && model.compositions.size() == 1 && model.sourceNodes.size() == 5
        && model.sourceChildIds.size() == 4 && model.sourcePropertyIds.size() == 8
        && model.properties.size() == 8 && model.tracks.size() == (animated ? 1U : 0U)
        && model.segments.size() == (animated ? 1U : 0U)
        && model.scalarValues.size() == 3 && model.vec2Values.size() == (animated ? 3U : 2U)
        && model.colorValues.size() == 1 && model.matrixValues.size() == 2
        && model.shapeValues.empty() && model.shapePoints.empty()
        && model.gradientValues.empty() && model.gradientFloats.empty()
        && stats.compositionCount == model.compositions.size()
        && stats.sourceNodeCount == model.sourceNodes.size()
        && stats.propertyCount == model.properties.size()
        && stats.staticPropertyCount == (animated ? 7U : 8U)
        && stats.animatedPropertyCount == (animated ? 1U : 0U)
        && stats.trackCount == model.tracks.size()
        && stats.segmentCount == model.segments.size()
        && stats.scalarValueCount == model.scalarValues.size()
        && stats.vec2ValueCount == model.vec2Values.size()
        && stats.colorValueCount == model.colorValues.size()
        && stats.matrixValueCount == model.matrixValues.size()
        && stats.shapeValueCount == 0 && stats.gradientValueCount == 0;
}

struct ExpectedValues final {
    float frameRate = 0;
    MotionVec2Value translation, size, start, end, outgoing, incoming;
    MotionColorValue color;
    bool animated = false;
    std::uint32_t firstFrame = 0, lastFrame = 0;
};

[[nodiscard]] bool interpret(const NativeEllipseInput& input, ExpectedValues& expected) {
    if (input.width == 0 || input.width > 8192 || input.height == 0 || input.height > 8192
        || input.endFrame < 2 || input.endFrame > 10000 || input.layerId < 1
        || input.layerInFrame >= input.layerOutFrame || input.layerOutFrame > input.endFrame
        || !convert(input.frameRate, expected.frameRate, true)
        || !convert(input.layerTranslation, expected.translation)
        || !convert(input.size, expected.size, true)
        || !convert(input.fillColor[0], expected.color.r)
        || !convert(input.fillColor[1], expected.color.g)
        || !convert(input.fillColor[2], expected.color.b)
        || !convert(input.fillColor[3], expected.color.a)
        || expected.frameRate > 240.0F
        || std::abs(expected.translation.x) > 32768.0F
        || std::abs(expected.translation.y) > 32768.0F
        || expected.size.x > 16384.0F || expected.size.y > 16384.0F
        || expected.color.r < 0.0F || expected.color.r > 1.0F
        || expected.color.g < 0.0F || expected.color.g > 1.0F
        || expected.color.b < 0.0F || expected.color.b > 1.0F
        || expected.color.a != 1.0F) return false;
    expected.animated = std::holds_alternative<NativeEllipseAnimatedPosition>(input.position);
    if (expected.animated) {
        const auto& motion = std::get<NativeEllipseAnimatedPosition>(input.position);
        expected.firstFrame = motion.firstFrame;
        expected.lastFrame = motion.lastFrame;
        return motion.firstFrame == 0 && motion.lastFrame == input.endFrame - 1
            && convert(motion.start, expected.start) && convert(motion.end, expected.end)
            && convert(motion.outgoing, expected.outgoing)
            && convert(motion.incoming, expected.incoming)
            && std::abs(expected.start.x) <= 32768.0F
            && std::abs(expected.start.y) <= 32768.0F
            && std::abs(expected.end.x) <= 32768.0F
            && std::abs(expected.end.y) <= 32768.0F
            && expected.outgoing.x >= 0.0F && expected.outgoing.x <= 1.0F
            && expected.outgoing.y >= 0.0F && expected.outgoing.y <= 1.0F
            && expected.incoming.x >= 0.0F && expected.incoming.x <= 1.0F
            && expected.incoming.y >= 0.0F && expected.incoming.y <= 1.0F;
    }
    return convert(std::get<NativeEllipseStaticPosition>(input.position).value, expected.start)
        && std::abs(expected.start.x) <= 32768.0F
        && std::abs(expected.start.y) <= 32768.0F;
}

[[nodiscard]] bool staticValue(const MotionAssetModel& model, const MotionPropertyRecord& property,
                               PropertyValueType type) noexcept {
    return property.flags == PropertyFlagStatic && property.valueType == type
        && !property.track.valid() && valueRef(model, property.staticValue, type);
}
}

NativeEllipseBindingResult bindNativeEllipseModel(
    const NativeEllipseInput& input, const MotionAssetModel& model) {
    ExpectedValues expected;
    if (!interpret(input, expected)) return fail(NativeEllipseBindingCode::UnsupportedNumericConversion);
    if (!tableShape(model, expected.animated)) return fail(NativeEllipseBindingCode::InvalidModelTable);

    const auto& composition = model.compositions.front();
    if (!composition.present || composition.id != makeId<CompositionId>(0)
        || !composition.rootNode.valid() || composition.debugName != "root"
        || composition.logicalWidth != input.width || composition.logicalHeight != input.height
        || !std::isfinite(composition.firstFrame) || !std::isfinite(composition.endFrame)
        || !std::isfinite(composition.frameRate) || composition.firstFrame != 0.0
        || composition.endFrame != static_cast<double>(input.endFrame)
        || composition.frameRate != static_cast<double>(expected.frameRate)
        || model.logicalWidth != input.width || model.logicalHeight != input.height
        || model.totalFrames != input.endFrame || !std::isfinite(model.frameRate)
        || model.frameRate != static_cast<double>(expected.frameRate)) {
        return fail(NativeEllipseBindingCode::CompositionMismatch);
    }

    // IDs address table rows; semantic roles come only from checked graph edges.
    for (std::size_t index = 0; index < model.sourceNodes.size(); ++index) {
        const auto& node = model.sourceNodes[index];
        if (!node.present || node.id != makeId<SourceNodeId>(index)
            || node.composition != composition.id
            || !range(node.children, model.sourceChildIds.size())
            || !range(node.properties, model.sourcePropertyIds.size())) {
            return fail(NativeEllipseBindingCode::InvalidModelTable);
        }
    }
    if (composition.rootNode.index() >= model.sourceNodes.size())
        return fail(NativeEllipseBindingCode::InvalidModelTable);
    const auto rootId = composition.rootNode;
    const auto& root = model.sourceNodes[rootId.index()];
    if (root.children.count != 1 || root.properties.count != 0)
        return fail(NativeEllipseBindingCode::TopologyMismatch);
    const auto layerId = model.sourceChildIds[root.children.first];
    if (!layerId.valid() || layerId.index() >= model.sourceNodes.size())
        return fail(NativeEllipseBindingCode::InvalidModelTable);
    const auto& layer = model.sourceNodes[layerId.index()];
    if (layer.children.count != 1 || layer.properties.count != 2)
        return fail(NativeEllipseBindingCode::TopologyMismatch);
    const auto groupId = model.sourceChildIds[layer.children.first];
    if (!groupId.valid() || groupId.index() >= model.sourceNodes.size())
        return fail(NativeEllipseBindingCode::InvalidModelTable);
    const auto& group = model.sourceNodes[groupId.index()];
    if (group.children.count != 2 || group.properties.count != 2)
        return fail(NativeEllipseBindingCode::TopologyMismatch);
    const auto ellipseId = model.sourceChildIds[group.children.first];
    const auto fillId = model.sourceChildIds[group.children.first + 1];
    if (!ellipseId.valid() || !fillId.valid()
        || ellipseId.index() >= model.sourceNodes.size()
        || fillId.index() >= model.sourceNodes.size()) {
        return fail(NativeEllipseBindingCode::InvalidModelTable);
    }
    const auto& ellipse = model.sourceNodes[ellipseId.index()];
    const auto& fill = model.sourceNodes[fillId.index()];
    const std::array ids{rootId, layerId, groupId, ellipseId, fillId};
    std::array<bool, 5> visited{};
    for (const auto id : ids) {
        if (visited[id.index()]) return fail(NativeEllipseBindingCode::TopologyMismatch);
        visited[id.index()] = true;
    }
    if (ellipse.children.count != 0 || fill.children.count != 0
        || ellipse.properties.count != 2 || fill.properties.count != 2
        || root.parent.valid() || layer.parent != rootId || group.parent != layerId
        || ellipse.parent != groupId || fill.parent != groupId) {
        return fail(NativeEllipseBindingCode::TopologyMismatch);
    }
    std::array<bool, 4> edgeUsed{};
    for (const auto& node : model.sourceNodes) {
        for (std::size_t offset = 0; offset < node.children.count; ++offset) {
            const auto edge = static_cast<std::size_t>(node.children.first) + offset;
            if (edgeUsed[edge]) return fail(NativeEllipseBindingCode::TopologyMismatch);
            edgeUsed[edge] = true;
            const auto child = model.sourceChildIds[edge];
            if (!child.valid() || child.index() >= model.sourceNodes.size()
                || model.sourceNodes[child.index()].parent != node.id) {
                return fail(NativeEllipseBindingCode::TopologyMismatch);
            }
        }
    }
    if (!std::all_of(edgeUsed.begin(), edgeUsed.end(), [](bool used) { return used; }))
        return fail(NativeEllipseBindingCode::TopologyMismatch);

    const std::array<const MotionSourceNodeRecord*, 5> nodes{&root, &layer, &group, &ellipse, &fill};
    const std::array<SourceNodeKind, 5> kinds{SourceNodeKind::Composition, SourceNodeKind::Layer,
        SourceNodeKind::ShapeGroup, SourceNodeKind::Ellipse, SourceNodeKind::Fill};
    const std::array<std::string, 5> names{"root",
        effective(input.layerName, "layer:" + std::to_string(input.layerId)),
        effective(input.groupName, "group"), effective(input.ellipseName, "source-node"),
        effective(input.fillName, "source-node")};
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        const auto& node = *nodes[index];
        const auto animated = expected.animated && index != 4;
        if (node.kind != kinds[index] || !defaultNode(node, animated)
            || node.layerKind != (index == 1 ? SourceLayerKind::Shape : SourceLayerKind::None)
            || node.authoredLayerId != (index == 1 ? input.layerId : -1)
            || node.inFrame != (index == 1 ? static_cast<double>(input.layerInFrame) : 0.0)
            || node.outFrame != (index == 1 ? static_cast<double>(input.layerOutFrame) : 0.0)) {
            return fail(NativeEllipseBindingCode::SourceIdentityMismatch);
        }
        if (node.debugName != names[index] || node.nameHash != nameHash(names[index]))
            return fail(NativeEllipseBindingCode::SourceIdentityMismatch);
    }

    NativeEllipseModelBinding binding{rootId, layerId, groupId, ellipseId, fillId};
    const std::array<PropertySemantic, 8> semantics{PropertySemantic::TransformMatrix,
        PropertySemantic::TransformOpacity, PropertySemantic::TransformMatrix,
        PropertySemantic::TransformOpacity, PropertySemantic::EllipsePosition,
        PropertySemantic::EllipseSize, PropertySemantic::FillColor, PropertySemantic::FillOpacity};
    const std::array<PropertyValueType, 8> types{PropertyValueType::Matrix3x2,
        PropertyValueType::Scalar, PropertyValueType::Matrix3x2, PropertyValueType::Scalar,
        PropertyValueType::Vec2, PropertyValueType::Vec2, PropertyValueType::Color,
        PropertyValueType::Scalar};
    std::array<PropertyId*, 8> destinations{&binding.layerTransform, &binding.layerOpacity,
        &binding.groupTransform, &binding.groupOpacity, &binding.position, &binding.size,
        &binding.color, &binding.fillOpacity};
    std::array<bool, 8> propertyEdges{};
    for (std::size_t nodeIndex = 1; nodeIndex < nodes.size(); ++nodeIndex) {
        const auto& node = *nodes[nodeIndex];
        const auto start = (nodeIndex - 1U) * 2U;
        for (std::size_t offset = 0; offset < node.properties.count; ++offset) {
            const auto edge = static_cast<std::size_t>(node.properties.first) + offset;
            if (propertyEdges[edge]) return fail(NativeEllipseBindingCode::PropertyShapeMismatch);
            propertyEdges[edge] = true;
            const auto propertyId = model.sourcePropertyIds[edge];
            const auto* property = model.property(propertyId);
            if (!property || !property->present || property->id != propertyId
                || property->owner != node.id || property->semanticIndex != 0) {
                return fail(NativeEllipseBindingCode::PropertyShapeMismatch);
            }
            bool matched = false;
            for (std::size_t slot = start; slot < start + 2; ++slot) {
                if (property->semantic == semantics[slot] && !destinations[slot]->valid()) {
                    *destinations[slot] = propertyId;
                    matched = true;
                    break;
                }
            }
            if (!matched) return fail(NativeEllipseBindingCode::PropertyShapeMismatch);
        }
    }
    if (!std::all_of(propertyEdges.begin(), propertyEdges.end(), [](bool used) { return used; }))
        return fail(NativeEllipseBindingCode::PropertyShapeMismatch);
    std::array<bool, 8> propertyRows{};
    for (std::size_t slot = 0; slot < destinations.size(); ++slot) {
        const auto id = *destinations[slot];
        if (!id.valid() || id.index() >= model.properties.size() || propertyRows[id.index()])
            return fail(NativeEllipseBindingCode::PropertyShapeMismatch);
        propertyRows[id.index()] = true;
        const auto& property = model.properties[id.index()];
        if (property.valueType != types[slot]) return fail(NativeEllipseBindingCode::PropertyShapeMismatch);
        if (slot == 4 && expected.animated) {
            if (property.flags != PropertyFlagAnimated || property.staticValue.valid()
                || property.track.index() >= model.tracks.size())
                return fail(NativeEllipseBindingCode::PropertyShapeMismatch);
        } else if (!staticValue(model, property, types[slot])) {
            return fail(NativeEllipseBindingCode::PropertyShapeMismatch);
        }
    }
    if (!std::all_of(propertyRows.begin(), propertyRows.end(), [](bool used) { return used; }))
        return fail(NativeEllipseBindingCode::PropertyShapeMismatch);

    const auto matrix = [&](PropertyId id, MotionVec2Value translation) {
        const auto& ref = model.properties[id.index()].staticValue;
        const auto& value = model.matrixValues[ref.index];
        return finite(value) && value == MotionMatrix3x2Value{1, 0, 0, 1,
            translation.x, translation.y};
    };
    const auto scalar = [&](PropertyId id) {
        const auto value = model.scalarValues[model.properties[id.index()].staticValue.index];
        return std::isfinite(value) && value == 100.0F;
    };
    const auto vec2 = [&](PropertyId id, MotionVec2Value expectedValue) {
        const auto value = model.vec2Values[model.properties[id.index()].staticValue.index];
        return finite(value) && value == expectedValue;
    };
    const auto color = model.colorValues[model.properties[binding.color.index()].staticValue.index];
    if (!matrix(binding.layerTransform, expected.translation)
        || !matrix(binding.groupTransform, {})
        || !scalar(binding.layerOpacity) || !scalar(binding.groupOpacity)
        || !scalar(binding.fillOpacity) || !vec2(binding.size, expected.size)
        || !finite(color) || color != expected.color
        || (!expected.animated && !vec2(binding.position, expected.start))) {
        return fail(NativeEllipseBindingCode::ValueMismatch);
    }
    if (expected.animated) {
        const auto& property = model.properties[binding.position.index()];
        const auto& track = model.tracks[property.track.index()];
        if (!track.present || track.id != property.track || track.property != binding.position
            || !range(track.segments, model.segments.size()) || track.segments.count != 1
            || track.firstFrame != expected.firstFrame || track.endFrame != expected.lastFrame) {
            return fail(NativeEllipseBindingCode::TrackMismatch);
        }
        const auto& segment = model.segments[track.segments.first];
        const auto linear = expected.outgoing == MotionVec2Value{0, 0}
            && expected.incoming == MotionVec2Value{1, 1};
        if (!segment.present || segment.id != makeId<SegmentId>(track.segments.first)
            || segment.track != track.id || segment.firstFrame != expected.firstFrame
            || segment.endFrame != expected.lastFrame
            || segment.interpolation != (linear ? SegmentInterpolation::Linear
                : SegmentInterpolation::CubicBezier)
            || segment.spatialInterpolation != SpatialInterpolation::None
            || !finite(segment.temporalControl1) || !finite(segment.temporalControl2)
            || segment.temporalControl1 != expected.outgoing
            || segment.temporalControl2 != expected.incoming
            || !finite(segment.spatialInTangent) || !finite(segment.spatialOutTangent)
            || segment.spatialInTangent != MotionVec2Value{}
            || segment.spatialOutTangent != MotionVec2Value{}
            || !valueRef(model, segment.startValue, PropertyValueType::Vec2)
            || !valueRef(model, segment.endValue, PropertyValueType::Vec2)) {
            return fail(NativeEllipseBindingCode::TrackMismatch);
        }
        if (model.vec2Values[segment.startValue.index] != expected.start
            || model.vec2Values[segment.endValue.index] != expected.end) {
            return fail(NativeEllipseBindingCode::ValueMismatch);
        }
    }
    // The authored subset uses every typed value row exactly once. Aliases can
    // otherwise conceal an unused conflicting value without changing counts.
    std::array<bool, 3> scalarRows{};
    std::array<bool, 3> vec2Rows{};
    std::array<bool, 1> colorRows{};
    std::array<bool, 2> matrixRows{};
    const auto claim = [](auto& rows, std::uint32_t index) {
        if (index >= rows.size() || rows[index]) return false;
        rows[index] = true;
        return true;
    };
    for (std::size_t slot = 0; slot < destinations.size(); ++slot) {
        if (slot == 4 && expected.animated) continue;
        const auto ref = model.properties[destinations[slot]->index()].staticValue;
        bool ok = false;
        switch (ref.type) {
        case PropertyValueType::Scalar: ok = claim(scalarRows, ref.index); break;
        case PropertyValueType::Vec2: ok = claim(vec2Rows, ref.index); break;
        case PropertyValueType::Color: ok = claim(colorRows, ref.index); break;
        case PropertyValueType::Matrix3x2: ok = claim(matrixRows, ref.index); break;
        default: break;
        }
        if (!ok) return fail(NativeEllipseBindingCode::PropertyShapeMismatch);
    }
    if (expected.animated) {
        const auto& track = model.tracks[model.properties[binding.position.index()].track.index()];
        const auto& segment = model.segments[track.segments.first];
        if (!claim(vec2Rows, segment.startValue.index)
            || !claim(vec2Rows, segment.endValue.index)) {
            return fail(NativeEllipseBindingCode::PropertyShapeMismatch);
        }
    }
    const auto all = [](const auto& rows, std::size_t count) {
        return std::all_of(rows.begin(), rows.begin() + count, [](bool used) { return used; });
    };
    if (!all(scalarRows, model.scalarValues.size())
        || !all(vec2Rows, model.vec2Values.size())
        || !all(colorRows, model.colorValues.size())
        || !all(matrixRows, model.matrixValues.size())) {
        return fail(NativeEllipseBindingCode::PropertyShapeMismatch);
    }
    return {NativeEllipseBindingCode::Bound, binding};
}

} // namespace avemotion::runtime::detail
