#include "avemotion/validation/AssetValidator.hpp"

#include "avemotion/core/Hash.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace avemotion::validation {
namespace {

constexpr std::size_t kFeatureCount =
    static_cast<std::size_t>(FeatureKind::UnknownNode) + 1U;

[[nodiscard]] constexpr std::size_t featureIndex(FeatureKind feature) noexcept {
    return static_cast<std::size_t>(feature);
}

[[nodiscard]] bool finite(float value) noexcept {
    return std::isfinite(value);
}

[[nodiscard]] bool finite(double value) noexcept {
    return std::isfinite(value);
}

[[nodiscard]] bool validRange(model::IndexRange range, std::size_t size) noexcept {
    const auto first = static_cast<std::size_t>(range.first);
    const auto count = static_cast<std::size_t>(range.count);
    return first <= size && count <= size - first;
}

template <typename Id>
[[nodiscard]] bool validId(Id id, std::size_t size) noexcept {
    return id.valid() && static_cast<std::size_t>(id.index()) < size;
}

[[nodiscard]] std::size_t saturatingAdd(
    std::size_t left,
    std::size_t right) noexcept {
    const auto maximum = std::numeric_limits<std::size_t>::max();
    return left > maximum - right ? maximum : left + right;
}

[[nodiscard]] std::size_t saturatingMultiply(
    std::size_t left,
    std::size_t right) noexcept {
    if (left == 0U || right == 0U) {
        return 0U;
    }
    const auto maximum = std::numeric_limits<std::size_t>::max();
    return left > maximum / right ? maximum : left * right;
}

[[nodiscard]] FeatureSupport supportFor(FeatureKind feature) noexcept {
    switch (feature) {
    case FeatureKind::Composition:
    case FeatureKind::ShapeLayer:
    case FeatureKind::NullLayer:
    case FeatureKind::ShapePath:
    case FeatureKind::Rectangle:
    case FeatureKind::Ellipse:
    case FeatureKind::Polygon:
    case FeatureKind::Star:
    case FeatureKind::SolidFill:
    case FeatureKind::SolidStroke:
    case FeatureKind::AutoOrient:
    case FeatureKind::SpatialPosition:
        return FeatureSupport::Native;
    case FeatureKind::TrimPath:
    case FeatureKind::Repeater:
        return FeatureSupport::NativeSubset;
    case FeatureKind::Precomposition:
    case FeatureKind::SolidLayer:
    case FeatureKind::DashedStroke:
    case FeatureKind::GradientFill:
    case FeatureKind::GradientStroke:
    case FeatureKind::Mask:
    case FeatureKind::Matte:
    case FeatureKind::BlendMode:
    case FeatureKind::TimeStretch:
    case FeatureKind::TimeRemap:
        return FeatureSupport::ReferenceFallback;
    case FeatureKind::ImageLayer:
    case FeatureKind::TextLayer:
    case FeatureKind::UnknownNode:
    case FeatureKind::Unknown:
        return FeatureSupport::Unsupported;
    }
    return FeatureSupport::Unsupported;
}

[[nodiscard]] bool forbiddenByTelegramProfile(FeatureKind feature) noexcept {
    switch (feature) {
    case FeatureKind::SolidLayer:
    case FeatureKind::ImageLayer:
    case FeatureKind::TextLayer:
    case FeatureKind::Star:
    case FeatureKind::GradientStroke:
    case FeatureKind::Repeater:
    case FeatureKind::Mask:
    case FeatureKind::Matte:
    case FeatureKind::AutoOrient:
    case FeatureKind::TimeStretch:
    case FeatureKind::TimeRemap:
    case FeatureKind::UnknownNode:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool structuralCode(ValidationIssueCode code) noexcept {
    switch (code) {
    case ValidationIssueCode::SchemaVersionUnsupported:
    case ValidationIssueCode::ParsedModelUnavailable:
    case ValidationIssueCode::InvalidDimensions:
    case ValidationIssueCode::InvalidFrameRate:
    case ValidationIssueCode::InvalidDuration:
    case ValidationIssueCode::InvalidTableId:
    case ValidationIssueCode::InvalidReference:
    case ValidationIssueCode::InvalidRange:
    case ValidationIssueCode::HierarchyCycle:
    case ValidationIssueCode::CompositionCycle:
    case ValidationIssueCode::NonFiniteValue:
    case ValidationIssueCode::InvalidPropertyFlags:
    case ValidationIssueCode::InvalidPropertyValue:
    case ValidationIssueCode::InvalidTrack:
    case ValidationIssueCode::InvalidSegment:
    case ValidationIssueCode::UnsortedSegments:
    case ValidationIssueCode::ShapeTopologyInvalid:
    case ValidationIssueCode::LimitExceeded:
        return true;
    default:
        return false;
    }
}

class ReportBuilder final {
public:
    ReportBuilder(
        const AssetValidationOptions& options,
        AssetValidationReport& report) noexcept
        : options_(options), report_(report) {
    }

    void addFeature(FeatureKind feature, std::size_t count = 1U) noexcept {
        if (feature == FeatureKind::Unknown || count == 0U) {
            return;
        }
        featureCounts_[featureIndex(feature)] = saturatingAdd(
            featureCounts_[featureIndex(feature)], count);
    }

    void addIssue(
        ValidationSeverity severity,
        ValidationIssueCode code,
        std::string message,
        FeatureKind feature = FeatureKind::Unknown,
        model::SourceNodeId node = {},
        model::PropertyId property = {}) {
        report_.issues.push_back({
            .severity = severity,
            .code = code,
            .feature = feature,
            .node = node,
            .property = property,
            .message = std::move(message),
        });
        switch (severity) {
        case ValidationSeverity::Info: ++report_.infoCount; break;
        case ValidationSeverity::Warning: ++report_.warningCount; break;
        case ValidationSeverity::Error: ++report_.errorCount; break;
        case ValidationSeverity::Fatal: ++report_.fatalCount; break;
        }
        if (structuralCode(code)
            && (severity == ValidationSeverity::Error
                || severity == ValidationSeverity::Fatal)) {
            structuralFailure_ = true;
        } else if (severity == ValidationSeverity::Error
                   || severity == ValidationSeverity::Fatal) {
            profileFailure_ = true;
        }
        if (options_.treatWarningsAsErrors
            && severity == ValidationSeverity::Warning) {
            profileFailure_ = true;
        }
    }

    void finishFeatures() {
        report_.features.clear();
        for (std::size_t index = 0; index < featureCounts_.size(); ++index) {
            const auto count = featureCounts_[index];
            if (count == 0U) {
                continue;
            }
            const auto feature = static_cast<FeatureKind>(index);
            const auto support = supportFor(feature);
            report_.features.push_back({feature, support, count});
            switch (support) {
            case FeatureSupport::Native:
                report_.nativeFeatureOccurrences = saturatingAdd(
                    report_.nativeFeatureOccurrences, count);
                break;
            case FeatureSupport::NativeSubset:
                report_.nativeSubsetFeatureOccurrences = saturatingAdd(
                    report_.nativeSubsetFeatureOccurrences, count);
                break;
            case FeatureSupport::ReferenceFallback:
                report_.fallbackFeatureOccurrences = saturatingAdd(
                    report_.fallbackFeatureOccurrences, count);
                break;
            case FeatureSupport::Unsupported:
                report_.unsupportedFeatureOccurrences = saturatingAdd(
                    report_.unsupportedFeatureOccurrences, count);
                break;
            }
        }
    }

    [[nodiscard]] std::size_t featureCount(FeatureKind feature) const noexcept {
        return featureCounts_[featureIndex(feature)];
    }

    [[nodiscard]] bool structuralFailure() const noexcept {
        return structuralFailure_;
    }

    [[nodiscard]] bool profileFailure() const noexcept {
        return profileFailure_;
    }

private:
    const AssetValidationOptions& options_;
    AssetValidationReport& report_;
    std::array<std::size_t, kFeatureCount> featureCounts_{};
    bool structuralFailure_ = false;
    bool profileFailure_ = false;
};

[[nodiscard]] bool validateValueReference(
    const model::MotionAssetModel& asset,
    model::MotionValueRef value) noexcept {
    if (!value.valid()) {
        return false;
    }
    const auto index = static_cast<std::size_t>(value.index);
    switch (value.type) {
    case model::PropertyValueType::Scalar:
        return index < asset.scalarValues.size();
    case model::PropertyValueType::Vec2:
        return index < asset.vec2Values.size();
    case model::PropertyValueType::Color:
        return index < asset.colorValues.size();
    case model::PropertyValueType::Matrix3x2:
        return index < asset.matrixValues.size();
    case model::PropertyValueType::Shape:
        return index < asset.shapeValues.size();
    case model::PropertyValueType::Gradient:
        return index < asset.gradientValues.size();
    case model::PropertyValueType::None:
        return false;
    }
    return false;
}

[[nodiscard]] bool valueReferenceFinite(
    const model::MotionAssetModel& asset,
    model::MotionValueRef value) noexcept {
    if (!validateValueReference(asset, value)) {
        return false;
    }
    const auto index = static_cast<std::size_t>(value.index);
    switch (value.type) {
    case model::PropertyValueType::Scalar:
        return finite(asset.scalarValues[index]);
    case model::PropertyValueType::Vec2: {
        const auto& item = asset.vec2Values[index];
        return finite(item.x) && finite(item.y);
    }
    case model::PropertyValueType::Color: {
        const auto& item = asset.colorValues[index];
        return finite(item.r) && finite(item.g)
            && finite(item.b) && finite(item.a);
    }
    case model::PropertyValueType::Matrix3x2: {
        const auto& item = asset.matrixValues[index];
        return finite(item.m11) && finite(item.m12)
            && finite(item.m21) && finite(item.m22)
            && finite(item.dx) && finite(item.dy);
    }
    case model::PropertyValueType::Shape: {
        const auto& shape = asset.shapeValues[index];
        if (!validRange(shape.points, asset.shapePoints.size())) {
            return false;
        }
        for (std::size_t pointIndex = shape.points.first;
             pointIndex < static_cast<std::size_t>(shape.points.end());
             ++pointIndex) {
            const auto& point = asset.shapePoints[pointIndex];
            if (!finite(point.x) || !finite(point.y)) {
                return false;
            }
        }
        return true;
    }
    case model::PropertyValueType::Gradient: {
        const auto& gradient = asset.gradientValues[index];
        if (!validRange(gradient.values, asset.gradientFloats.size())) {
            return false;
        }
        for (std::size_t valueIndex = gradient.values.first;
             valueIndex < static_cast<std::size_t>(gradient.values.end());
             ++valueIndex) {
            if (!finite(asset.gradientFloats[valueIndex])) {
                return false;
            }
        }
        return true;
    }
    case model::PropertyValueType::None:
        return false;
    }
    return false;
}

[[nodiscard]] std::string nodeLabel(
    const model::MotionSourceNodeRecord& node) {
    std::ostringstream stream;
    stream << "source node " << node.id.index();
    if (!node.debugName.empty()) {
        stream << " ('" << node.debugName << "')";
    }
    return stream.str();
}

void collectFeatures(
    const model::MotionAssetModel& asset,
    ReportBuilder& builder) {
    std::vector<bool> strokeHasDash(asset.sourceNodes.size(), false);
    std::vector<bool> layerHasTimeRemap(asset.sourceNodes.size(), false);
    std::vector<bool> nodeHasSpatialProperty(asset.sourceNodes.size(), false);

    for (const auto& property : asset.properties) {
        if (!property.present || !validId(property.owner, asset.sourceNodes.size())) {
            continue;
        }
        const auto owner = static_cast<std::size_t>(property.owner.index());
        if (property.semantic == model::PropertySemantic::StrokeDashValue) {
            strokeHasDash[owner] = true;
        }
        if (property.semantic == model::PropertySemantic::LayerTimeRemap) {
            layerHasTimeRemap[owner] = true;
        }
        if ((property.flags & model::PropertyFlagSpatial) != 0U) {
            nodeHasSpatialProperty[owner] = true;
        }
    }

    for (std::size_t index = 0; index < asset.sourceNodes.size(); ++index) {
        const auto& node = asset.sourceNodes[index];
        if (!node.present) {
            continue;
        }
        switch (node.kind) {
        case model::SourceNodeKind::Composition:
            builder.addFeature(FeatureKind::Composition);
            break;
        case model::SourceNodeKind::Layer:
            switch (node.layerKind) {
            case model::SourceLayerKind::Precomposition:
                builder.addFeature(FeatureKind::Precomposition);
                break;
            case model::SourceLayerKind::Solid:
                builder.addFeature(FeatureKind::SolidLayer);
                break;
            case model::SourceLayerKind::Image:
                builder.addFeature(FeatureKind::ImageLayer);
                break;
            case model::SourceLayerKind::Null:
                builder.addFeature(FeatureKind::NullLayer);
                break;
            case model::SourceLayerKind::Shape:
                builder.addFeature(FeatureKind::ShapeLayer);
                break;
            case model::SourceLayerKind::Text:
                builder.addFeature(FeatureKind::TextLayer);
                break;
            case model::SourceLayerKind::Unknown:
            case model::SourceLayerKind::None:
                builder.addFeature(FeatureKind::UnknownNode);
                break;
            }
            break;
        case model::SourceNodeKind::ShapeGroup:
            break;
        case model::SourceNodeKind::Fill:
            builder.addFeature(FeatureKind::SolidFill);
            break;
        case model::SourceNodeKind::Stroke:
            builder.addFeature(
                strokeHasDash[index]
                    ? FeatureKind::DashedStroke
                    : FeatureKind::SolidStroke);
            break;
        case model::SourceNodeKind::GradientFill:
            builder.addFeature(FeatureKind::GradientFill);
            break;
        case model::SourceNodeKind::GradientStroke:
            builder.addFeature(FeatureKind::GradientStroke);
            break;
        case model::SourceNodeKind::Rectangle:
            builder.addFeature(FeatureKind::Rectangle);
            break;
        case model::SourceNodeKind::Ellipse:
            builder.addFeature(FeatureKind::Ellipse);
            break;
        case model::SourceNodeKind::Shape:
            builder.addFeature(FeatureKind::ShapePath);
            break;
        case model::SourceNodeKind::Polystar:
            builder.addFeature(
                node.polystarType == model::SourcePolystarType::Polygon
                    ? FeatureKind::Polygon
                    : FeatureKind::Star);
            break;
        case model::SourceNodeKind::Trim:
            builder.addFeature(FeatureKind::TrimPath);
            break;
        case model::SourceNodeKind::Repeater:
            builder.addFeature(FeatureKind::Repeater);
            break;
        case model::SourceNodeKind::Mask:
            builder.addFeature(FeatureKind::Mask);
            break;
        case model::SourceNodeKind::Unknown:
            builder.addFeature(FeatureKind::UnknownNode);
            break;
        }

        if (node.autoOrient) {
            builder.addFeature(FeatureKind::AutoOrient);
        }
        if (nodeHasSpatialProperty[index]) {
            builder.addFeature(FeatureKind::SpatialPosition);
        }
        if (std::abs(node.timeStretch - 1.0F) > 1.0e-6F) {
            builder.addFeature(FeatureKind::TimeStretch);
        }
        if (layerHasTimeRemap[index]) {
            builder.addFeature(FeatureKind::TimeRemap);
        }
        if (node.matteMode != model::SourceMatteMode::None) {
            builder.addFeature(FeatureKind::Matte);
        }
        if (node.blendMode != model::SourceBlendMode::Normal) {
            builder.addFeature(FeatureKind::BlendMode);
        }
    }
}

void validateTableLimits(
    const model::MotionAssetModel& asset,
    const AssetValidationLimits& limits,
    ReportBuilder& builder) {
    const auto check = [&](std::size_t value, std::size_t maximum, std::string_view label) {
        if (value > maximum) {
            std::ostringstream message;
            message << label << " count " << value
                    << " exceeds limit " << maximum;
            builder.addIssue(
                ValidationSeverity::Fatal,
                ValidationIssueCode::LimitExceeded,
                message.str());
        }
    };
    check(asset.compositions.size(), limits.maximumCompositions, "composition");
    check(asset.sourceNodes.size(), limits.maximumSourceNodes, "source node");
    check(asset.properties.size(), limits.maximumProperties, "property");
    check(asset.tracks.size(), limits.maximumTracks, "track");
    check(asset.segments.size(), limits.maximumSegments, "segment");
    check(asset.shapePoints.size(), limits.maximumShapePoints, "shape point");
    check(asset.gradientFloats.size(), limits.maximumGradientValues, "gradient value");
}

void validateCompositions(
    const model::MotionAssetModel& asset,
    ReportBuilder& builder) {
    if (asset.compositions.empty()) {
        builder.addIssue(
            ValidationSeverity::Fatal,
            ValidationIssueCode::InvalidReference,
            "asset contains no compositions");
        return;
    }
    for (std::size_t index = 0; index < asset.compositions.size(); ++index) {
        const auto& composition = asset.compositions[index];
        if (!composition.present || composition.id.index() != index) {
            builder.addIssue(
                ValidationSeverity::Fatal,
                ValidationIssueCode::InvalidTableId,
                "composition table contains a missing or non-canonical ID");
            continue;
        }
        if (!validId(composition.rootNode, asset.sourceNodes.size())) {
            builder.addIssue(
                ValidationSeverity::Fatal,
                ValidationIssueCode::InvalidReference,
                "composition root node is invalid");
        } else {
            const auto& root = asset.sourceNodes[composition.rootNode.index()];
            if (!root.present || root.kind != model::SourceNodeKind::Composition
                || root.composition != composition.id) {
                builder.addIssue(
                    ValidationSeverity::Fatal,
                    ValidationIssueCode::InvalidReference,
                    "composition root node does not reference its composition");
            }
        }
        if (composition.logicalWidth == 0U || composition.logicalHeight == 0U) {
            builder.addIssue(
                ValidationSeverity::Error,
                ValidationIssueCode::InvalidDimensions,
                "composition has zero logical dimensions");
        }
        if (!finite(composition.firstFrame) || !finite(composition.endFrame)
            || composition.endFrame < composition.firstFrame) {
            builder.addIssue(
                ValidationSeverity::Fatal,
                ValidationIssueCode::InvalidDuration,
                "composition has invalid frame bounds");
        }
        if (!finite(composition.frameRate) || composition.frameRate <= 0.0) {
            builder.addIssue(
                ValidationSeverity::Fatal,
                ValidationIssueCode::InvalidFrameRate,
                "composition has invalid frame rate");
        }
    }
}

void validateSourceNodes(
    const model::MotionAssetModel& asset,
    const AssetValidationLimits& limits,
    ReportBuilder& builder) {
    for (std::size_t index = 0; index < asset.sourceNodes.size(); ++index) {
        const auto& node = asset.sourceNodes[index];
        if (!node.present || node.id.index() != index) {
            builder.addIssue(
                ValidationSeverity::Fatal,
                ValidationIssueCode::InvalidTableId,
                "source-node table contains a missing or non-canonical ID");
            continue;
        }
        if (!validId(node.composition, asset.compositions.size())) {
            builder.addIssue(
                ValidationSeverity::Fatal,
                ValidationIssueCode::InvalidReference,
                nodeLabel(node) + " has an invalid composition reference",
                FeatureKind::Unknown,
                node.id);
        }
        if (node.parent.valid() && !validId(node.parent, asset.sourceNodes.size())) {
            builder.addIssue(
                ValidationSeverity::Fatal,
                ValidationIssueCode::InvalidReference,
                nodeLabel(node) + " has an invalid structural parent",
                FeatureKind::Unknown,
                node.id);
        }
        if (node.transformParent.valid()
            && !validId(node.transformParent, asset.sourceNodes.size())) {
            builder.addIssue(
                ValidationSeverity::Fatal,
                ValidationIssueCode::InvalidReference,
                nodeLabel(node) + " has an invalid transform parent",
                FeatureKind::Unknown,
                node.id);
        }
        if (node.referencedComposition.valid()
            && !validId(node.referencedComposition, asset.compositions.size())) {
            builder.addIssue(
                ValidationSeverity::Fatal,
                ValidationIssueCode::InvalidReference,
                nodeLabel(node) + " has an invalid referenced composition",
                FeatureKind::Precomposition,
                node.id);
        }
        if (!validRange(node.children, asset.sourceChildIds.size())
            || !validRange(node.properties, asset.sourcePropertyIds.size())) {
            builder.addIssue(
                ValidationSeverity::Fatal,
                ValidationIssueCode::InvalidRange,
                nodeLabel(node) + " contains an invalid child/property range",
                FeatureKind::Unknown,
                node.id);
            continue;
        }
        if (node.properties.count > limits.maximumPropertiesPerNode) {
            builder.addIssue(
                ValidationSeverity::Fatal,
                ValidationIssueCode::LimitExceeded,
                nodeLabel(node) + " exceeds the per-node property limit",
                FeatureKind::Unknown,
                node.id);
        }
        if (!finite(node.inFrame) || !finite(node.outFrame)
            || !finite(node.startFrame) || !finite(node.timeStretch)
            || !finite(node.miterLimit) || !finite(node.repeaterMaximumCopies)
            || !finite(node.solidColor.r) || !finite(node.solidColor.g)
            || !finite(node.solidColor.b) || !finite(node.solidColor.a)) {
            builder.addIssue(
                ValidationSeverity::Fatal,
                ValidationIssueCode::NonFiniteValue,
                nodeLabel(node) + " contains a non-finite authored value",
                FeatureKind::Unknown,
                node.id);
        }
        if (node.repeaterMaximumCopies < 0.0F
            || node.repeaterMaximumCopies > limits.maximumRepeaterCopies) {
            builder.addIssue(
                ValidationSeverity::Fatal,
                ValidationIssueCode::LimitExceeded,
                nodeLabel(node) + " exceeds the Repeater copy limit",
                FeatureKind::Repeater,
                node.id);
        }

        for (std::size_t childOffset = 0; childOffset < node.children.count;
             ++childOffset) {
            const auto child = asset.sourceChildIds[
                static_cast<std::size_t>(node.children.first) + childOffset];
            if (!validId(child, asset.sourceNodes.size())) {
                builder.addIssue(
                    ValidationSeverity::Fatal,
                    ValidationIssueCode::InvalidReference,
                    nodeLabel(node) + " contains an invalid child ID",
                    FeatureKind::Unknown,
                    node.id);
                continue;
            }
            const auto& childNode = asset.sourceNodes[child.index()];
            if (childNode.parent != node.id) {
                builder.addIssue(
                    ValidationSeverity::Fatal,
                    ValidationIssueCode::InvalidReference,
                    nodeLabel(node) + " child does not point back to its parent",
                    FeatureKind::Unknown,
                    childNode.id);
            }
        }
        for (std::size_t propertyOffset = 0;
             propertyOffset < node.properties.count;
             ++propertyOffset) {
            const auto property = asset.sourcePropertyIds[
                static_cast<std::size_t>(node.properties.first) + propertyOffset];
            if (!validId(property, asset.properties.size())) {
                builder.addIssue(
                    ValidationSeverity::Fatal,
                    ValidationIssueCode::InvalidReference,
                    nodeLabel(node) + " contains an invalid property ID",
                    FeatureKind::Unknown,
                    node.id);
                continue;
            }
            if (asset.properties[property.index()].owner != node.id) {
                builder.addIssue(
                    ValidationSeverity::Fatal,
                    ValidationIssueCode::InvalidReference,
                    nodeLabel(node) + " property does not point back to its owner",
                    FeatureKind::Unknown,
                    node.id,
                    property);
            }
        }
    }
}

void validateParentGraph(
    const model::MotionAssetModel& asset,
    bool transformGraph,
    ReportBuilder& builder) {
    std::vector<std::uint8_t> state(asset.sourceNodes.size(), 0U);
    const auto visit = [&](auto&& self, std::size_t index) -> bool {
        if (state[index] == 1U) {
            return false;
        }
        if (state[index] == 2U) {
            return true;
        }
        state[index] = 1U;
        const auto& node = asset.sourceNodes[index];
        const auto parent = transformGraph ? node.transformParent : node.parent;
        if (parent.valid() && validId(parent, asset.sourceNodes.size())) {
            if (!self(self, parent.index())) {
                return false;
            }
        }
        state[index] = 2U;
        return true;
    };

    for (std::size_t index = 0; index < asset.sourceNodes.size(); ++index) {
        if (!asset.sourceNodes[index].present || state[index] != 0U) {
            continue;
        }
        if (!visit(visit, index)) {
            builder.addIssue(
                ValidationSeverity::Fatal,
                ValidationIssueCode::HierarchyCycle,
                transformGraph
                    ? "transform-parent graph contains a cycle"
                    : "structural parent graph contains a cycle",
                FeatureKind::Unknown,
                asset.sourceNodes[index].id);
            return;
        }
    }
}

[[nodiscard]] std::size_t computeHierarchyDepth(
    const model::MotionAssetModel& asset,
    ReportBuilder& builder,
    const AssetValidationLimits& limits) {
    std::vector<std::size_t> depths(asset.sourceNodes.size(), 0U);
    std::size_t maximum = 0U;
    for (std::size_t index = 0; index < asset.sourceNodes.size(); ++index) {
        if (!asset.sourceNodes[index].present) {
            continue;
        }
        std::size_t depth = 1U;
        auto current = asset.sourceNodes[index].parent;
        std::size_t guard = 0U;
        while (current.valid() && validId(current, asset.sourceNodes.size())) {
            ++depth;
            current = asset.sourceNodes[current.index()].parent;
            ++guard;
            if (guard > asset.sourceNodes.size()) {
                break;
            }
        }
        depths[index] = depth;
        maximum = std::max(maximum, depth);
    }
    if (maximum > limits.maximumHierarchyDepth) {
        std::ostringstream message;
        message << "hierarchy depth " << maximum
                << " exceeds limit " << limits.maximumHierarchyDepth;
        builder.addIssue(
            ValidationSeverity::Fatal,
            ValidationIssueCode::LimitExceeded,
            message.str());
    }
    return maximum;
}

void validateCompositionGraph(
    const model::MotionAssetModel& asset,
    ReportBuilder& builder) {
    std::vector<std::vector<std::size_t>> edges(asset.compositions.size());
    for (const auto& node : asset.sourceNodes) {
        if (!node.present || !node.referencedComposition.valid()
            || !validId(node.composition, asset.compositions.size())
            || !validId(node.referencedComposition, asset.compositions.size())) {
            continue;
        }
        edges[node.composition.index()].push_back(node.referencedComposition.index());
    }
    std::vector<std::uint8_t> state(asset.compositions.size(), 0U);
    const auto visit = [&](auto&& self, std::size_t index) -> bool {
        if (state[index] == 1U) {
            return false;
        }
        if (state[index] == 2U) {
            return true;
        }
        state[index] = 1U;
        for (const auto target : edges[index]) {
            if (!self(self, target)) {
                return false;
            }
        }
        state[index] = 2U;
        return true;
    };
    for (std::size_t index = 0; index < edges.size(); ++index) {
        if (state[index] == 0U && !visit(visit, index)) {
            builder.addIssue(
                ValidationSeverity::Fatal,
                ValidationIssueCode::CompositionCycle,
                "composition-reference graph contains a cycle");
            return;
        }
    }
}

void validateProperties(
    const model::MotionAssetModel& asset,
    ReportBuilder& builder) {
    for (std::size_t index = 0; index < asset.properties.size(); ++index) {
        const auto& property = asset.properties[index];
        if (!property.present || property.id.index() != index) {
            builder.addIssue(
                ValidationSeverity::Fatal,
                ValidationIssueCode::InvalidTableId,
                "property table contains a missing or non-canonical ID");
            continue;
        }
        if (!validId(property.owner, asset.sourceNodes.size())) {
            builder.addIssue(
                ValidationSeverity::Fatal,
                ValidationIssueCode::InvalidReference,
                "property has an invalid owner",
                FeatureKind::Unknown,
                {},
                property.id);
        }
        const auto isStatic = (property.flags & model::PropertyFlagStatic) != 0U;
        const auto isAnimated = (property.flags & model::PropertyFlagAnimated) != 0U;
        if (isStatic == isAnimated || property.valueType == model::PropertyValueType::None) {
            builder.addIssue(
                ValidationSeverity::Fatal,
                ValidationIssueCode::InvalidPropertyFlags,
                "property must be exactly one of static or animated",
                FeatureKind::Unknown,
                property.owner,
                property.id);
            continue;
        }
        if (isStatic) {
            if (!validateValueReference(asset, property.staticValue)
                || property.staticValue.type != property.valueType
                || property.track.valid()) {
                builder.addIssue(
                    ValidationSeverity::Fatal,
                    ValidationIssueCode::InvalidPropertyValue,
                    "static property has an invalid value or track",
                    FeatureKind::Unknown,
                    property.owner,
                    property.id);
            } else if (!valueReferenceFinite(asset, property.staticValue)) {
                builder.addIssue(
                    ValidationSeverity::Fatal,
                    ValidationIssueCode::NonFiniteValue,
                    "static property contains non-finite data",
                    FeatureKind::Unknown,
                    property.owner,
                    property.id);
            }
        } else {
            if (!validId(property.track, asset.tracks.size())) {
                builder.addIssue(
                    ValidationSeverity::Fatal,
                    ValidationIssueCode::InvalidTrack,
                    "animated property has an invalid track",
                    FeatureKind::Unknown,
                    property.owner,
                    property.id);
            } else if (asset.tracks[property.track.index()].property != property.id) {
                builder.addIssue(
                    ValidationSeverity::Fatal,
                    ValidationIssueCode::InvalidTrack,
                    "animated property's track points to a different property",
                    FeatureKind::Unknown,
                    property.owner,
                    property.id);
            }
        }
    }
}

void validateTracksAndSegments(
    const model::MotionAssetModel& asset,
    const AssetValidationLimits& limits,
    ReportBuilder& builder,
    AssetComplexity& complexity) {
    for (std::size_t trackIndex = 0; trackIndex < asset.tracks.size(); ++trackIndex) {
        const auto& track = asset.tracks[trackIndex];
        if (!track.present || track.id.index() != trackIndex) {
            builder.addIssue(
                ValidationSeverity::Fatal,
                ValidationIssueCode::InvalidTableId,
                "track table contains a missing or non-canonical ID");
            continue;
        }
        if (!validId(track.property, asset.properties.size())
            || !validRange(track.segments, asset.segments.size())
            || track.segments.empty()) {
            builder.addIssue(
                ValidationSeverity::Fatal,
                ValidationIssueCode::InvalidTrack,
                "track has an invalid property or segment range");
            continue;
        }
        if (track.segments.count > limits.maximumSegmentsPerTrack) {
            builder.addIssue(
                ValidationSeverity::Fatal,
                ValidationIssueCode::LimitExceeded,
                "track exceeds the per-track segment limit");
        }
        complexity.maximumSegmentsOnTrack = std::max(
            complexity.maximumSegmentsOnTrack,
            static_cast<std::size_t>(track.segments.count));
        if (!finite(track.firstFrame) || !finite(track.endFrame)
            || track.endFrame < track.firstFrame) {
            builder.addIssue(
                ValidationSeverity::Fatal,
                ValidationIssueCode::InvalidTrack,
                "track has invalid frame bounds");
        }

        double previousFirst = -std::numeric_limits<double>::infinity();
        double previousEnd = -std::numeric_limits<double>::infinity();
        for (std::size_t offset = 0; offset < track.segments.count; ++offset) {
            const auto segmentIndex =
                static_cast<std::size_t>(track.segments.first) + offset;
            const auto& segment = asset.segments[segmentIndex];
            if (!segment.present || segment.id.index() != segmentIndex) {
                builder.addIssue(
                    ValidationSeverity::Fatal,
                    ValidationIssueCode::InvalidTableId,
                    "segment table contains a missing or non-canonical ID");
                continue;
            }
            if (segment.track != track.id
                || !finite(segment.firstFrame) || !finite(segment.endFrame)
                || segment.endFrame < segment.firstFrame) {
                builder.addIssue(
                    ValidationSeverity::Fatal,
                    ValidationIssueCode::InvalidSegment,
                    "segment has an invalid track or frame range");
                continue;
            }
            if (segment.firstFrame < previousFirst
                || segment.firstFrame < previousEnd) {
                builder.addIssue(
                    ValidationSeverity::Fatal,
                    ValidationIssueCode::UnsortedSegments,
                    "track segments overlap or are not sorted");
            }
            previousFirst = segment.firstFrame;
            previousEnd = segment.endFrame;

            const auto valueType = asset.properties[track.property.index()].valueType;
            if (!validateValueReference(asset, segment.startValue)
                || !validateValueReference(asset, segment.endValue)
                || segment.startValue.type != valueType
                || segment.endValue.type != valueType) {
                builder.addIssue(
                    ValidationSeverity::Fatal,
                    ValidationIssueCode::InvalidPropertyValue,
                    "segment values do not match the owning property type");
            } else if (!valueReferenceFinite(asset, segment.startValue)
                       || !valueReferenceFinite(asset, segment.endValue)) {
                builder.addIssue(
                    ValidationSeverity::Fatal,
                    ValidationIssueCode::NonFiniteValue,
                    "segment contains non-finite value data");
            }
            if (!finite(segment.temporalControl1.x)
                || !finite(segment.temporalControl1.y)
                || !finite(segment.temporalControl2.x)
                || !finite(segment.temporalControl2.y)
                || !finite(segment.spatialInTangent.x)
                || !finite(segment.spatialInTangent.y)
                || !finite(segment.spatialOutTangent.x)
                || !finite(segment.spatialOutTangent.y)) {
                builder.addIssue(
                    ValidationSeverity::Fatal,
                    ValidationIssueCode::NonFiniteValue,
                    "segment contains non-finite interpolation metadata");
            }
        }
    }
}

void validateTypedStorage(
    const model::MotionAssetModel& asset,
    ReportBuilder& builder,
    AssetComplexity& complexity) {
    for (const auto value : asset.scalarValues) {
        if (!finite(value)) {
            builder.addIssue(
                ValidationSeverity::Fatal,
                ValidationIssueCode::NonFiniteValue,
                "scalar storage contains a non-finite value");
            break;
        }
    }
    for (const auto& value : asset.vec2Values) {
        if (!finite(value.x) || !finite(value.y)) {
            builder.addIssue(
                ValidationSeverity::Fatal,
                ValidationIssueCode::NonFiniteValue,
                "Vec2 storage contains a non-finite value");
            break;
        }
    }
    for (const auto& value : asset.colorValues) {
        if (!finite(value.r) || !finite(value.g)
            || !finite(value.b) || !finite(value.a)) {
            builder.addIssue(
                ValidationSeverity::Fatal,
                ValidationIssueCode::NonFiniteValue,
                "color storage contains a non-finite value");
            break;
        }
    }
    for (const auto& value : asset.matrixValues) {
        if (!finite(value.m11) || !finite(value.m12)
            || !finite(value.m21) || !finite(value.m22)
            || !finite(value.dx) || !finite(value.dy)) {
            builder.addIssue(
                ValidationSeverity::Fatal,
                ValidationIssueCode::NonFiniteValue,
                "matrix storage contains a non-finite value");
            break;
        }
    }
    for (const auto& shape : asset.shapeValues) {
        if (!validRange(shape.points, asset.shapePoints.size())) {
            builder.addIssue(
                ValidationSeverity::Fatal,
                ValidationIssueCode::InvalidRange,
                "shape value references an invalid point range");
            continue;
        }
        const auto pointCount = static_cast<std::size_t>(shape.points.count);
        complexity.shapePointCount = saturatingAdd(
            complexity.shapePointCount, pointCount);
        if (pointCount != 0U && (pointCount < 1U || (pointCount - 1U) % 3U != 0U)) {
            builder.addIssue(
                ValidationSeverity::Fatal,
                ValidationIssueCode::ShapeTopologyInvalid,
                "shape value does not use MoveTo + cubic triplet topology");
        }
    }
    for (const auto& point : asset.shapePoints) {
        if (!finite(point.x) || !finite(point.y)) {
            builder.addIssue(
                ValidationSeverity::Fatal,
                ValidationIssueCode::NonFiniteValue,
                "shape-point storage contains a non-finite coordinate");
            break;
        }
    }
    for (const auto& gradient : asset.gradientValues) {
        if (!validRange(gradient.values, asset.gradientFloats.size())) {
            builder.addIssue(
                ValidationSeverity::Fatal,
                ValidationIssueCode::InvalidRange,
                "gradient value references an invalid component range");
            continue;
        }
        complexity.gradientValueCount = saturatingAdd(
            complexity.gradientValueCount,
            static_cast<std::size_t>(gradient.values.count));
    }
    for (const auto value : asset.gradientFloats) {
        if (!finite(value)) {
            builder.addIssue(
                ValidationSeverity::Fatal,
                ValidationIssueCode::NonFiniteValue,
                "gradient storage contains a non-finite value");
            break;
        }
    }
}

void addSupportIssues(
    const AssetValidationOptions& options,
    const std::vector<FeatureUsage>& features,
    ReportBuilder& builder) {
    for (const auto& usage : features) {
        std::ostringstream message;
        message << toString(usage.feature) << " occurs " << usage.occurrences
                << " time(s) and is classified as " << toString(usage.support);
        switch (usage.support) {
        case FeatureSupport::Native:
            break;
        case FeatureSupport::NativeSubset:
            builder.addIssue(
                options.profile == ValidationProfile::Direct2DNative
                    ? ValidationSeverity::Error
                    : ValidationSeverity::Info,
                ValidationIssueCode::NativeSubsetRequired,
                message.str(),
                usage.feature);
            break;
        case FeatureSupport::ReferenceFallback:
            builder.addIssue(
                options.profile == ValidationProfile::Direct2DNative
                    ? ValidationSeverity::Error
                    : ValidationSeverity::Warning,
                ValidationIssueCode::ReferenceFallbackRequired,
                message.str(),
                usage.feature);
            break;
        case FeatureSupport::Unsupported:
            builder.addIssue(
                options.rejectUnknownFeatures
                    ? ValidationSeverity::Error
                    : ValidationSeverity::Warning,
                usage.feature == FeatureKind::UnknownNode
                    ? ValidationIssueCode::UnknownFeature
                    : ValidationIssueCode::UnsupportedFeature,
                message.str(),
                usage.feature);
            break;
        }
    }
}

// The Telegram profile needs the finalized feature list. This helper is kept
// separate so the structural pass remains independent of authoring policy.
void validateTelegramFeatures(
    const std::vector<FeatureUsage>& features,
    ReportBuilder& builder) {
    for (const auto& usage : features) {
        if (!forbiddenByTelegramProfile(usage.feature)) {
            continue;
        }
        std::ostringstream message;
        message << toString(usage.feature) << " occurs " << usage.occurrences
                << " time(s) but is forbidden by the Telegram animated-sticker authoring profile";
        builder.addIssue(
            ValidationSeverity::Error,
            ValidationIssueCode::TelegramForbiddenFeature,
            message.str(),
            usage.feature);
    }
}

void finalizeFingerprint(AssetValidationReport& report) {
    core::Fnv1a64 hash;
    hash.appendU8(static_cast<std::uint8_t>(report.profile));
    hash.appendU8(report.structurallyValid ? 1U : 0U);
    hash.appendU8(report.profileCompliant ? 1U : 0U);
    hash.appendU8(report.nativeDirect2DReady ? 1U : 0U);
    hash.appendU8(report.requiresReferenceFallback ? 1U : 0U);
    hash.appendU8(report.containsUnsupportedFeatures ? 1U : 0U);
    hash.appendU64(report.infoCount);
    hash.appendU64(report.warningCount);
    hash.appendU64(report.errorCount);
    hash.appendU64(report.fatalCount);
    hash.appendU64(report.complexity.hierarchyDepth);
    hash.appendU64(report.complexity.maximumPropertiesOnNode);
    hash.appendU64(report.complexity.maximumSegmentsOnTrack);
    hash.appendU64(report.complexity.shapePointCount);
    hash.appendU64(report.complexity.gradientValueCount);
    hash.appendU64(report.complexity.animatedPropertyCount);
    hash.appendU64(report.complexity.estimatedWorkUnits);
    for (const auto& feature : report.features) {
        hash.appendU32(static_cast<std::uint32_t>(feature.feature));
        hash.appendU8(static_cast<std::uint8_t>(feature.support));
        hash.appendU64(feature.occurrences);
    }
    for (const auto& issue : report.issues) {
        hash.appendU8(static_cast<std::uint8_t>(issue.severity));
        hash.appendU32(static_cast<std::uint32_t>(issue.code));
        hash.appendU32(static_cast<std::uint32_t>(issue.feature));
        hash.appendU32(issue.node.value);
        hash.appendU32(issue.property.value);
    }
    report.fingerprint = hash.value();
}

} // namespace

AssetValidationLimits AssetValidationLimits::applicationAsset() noexcept {
    return {};
}

AssetValidationLimits AssetValidationLimits::telegramSticker() noexcept {
    AssetValidationLimits limits;
    limits.maximumCompositions = 256;
    limits.maximumSourceNodes = 16384;
    limits.maximumProperties = 65536;
    limits.maximumTracks = 32768;
    limits.maximumSegments = 262144;
    limits.maximumShapePoints = 2000000;
    limits.maximumGradientValues = 262144;
    limits.maximumHierarchyDepth = 128;
    limits.maximumPropertiesPerNode = 1024;
    limits.maximumSegmentsPerTrack = 16384;
    limits.maximumRepeaterCopies = 4096.0F;
    return limits;
}

AssetValidationOptions AssetValidationOptions::applicationAsset() noexcept {
    return {};
}

AssetValidationOptions AssetValidationOptions::telegramSticker() noexcept {
    AssetValidationOptions options;
    options.profile = ValidationProfile::TelegramSticker;
    options.limits = AssetValidationLimits::telegramSticker();
    options.rejectUnknownFeatures = true;
    return options;
}

AssetValidationOptions AssetValidationOptions::direct2DNative() noexcept {
    AssetValidationOptions options;
    options.profile = ValidationProfile::Direct2DNative;
    options.limits = AssetValidationLimits::applicationAsset();
    options.rejectUnknownFeatures = true;
    return options;
}

AssetValidationReport AssetValidator::validate(
    const model::MotionAssetModel& asset,
    const AssetValidationContext& context,
    const AssetValidationOptions& options) const {
    AssetValidationReport report;
    report.profile = options.profile;
    ReportBuilder builder(options, report);

    if (asset.schemaVersion != model::MotionAssetModel::kSchemaVersion) {
        builder.addIssue(
            ValidationSeverity::Fatal,
            ValidationIssueCode::SchemaVersionUnsupported,
            "unsupported canonical asset-model schema version");
    }
    if (!asset.statistics.directParsedModel) {
        builder.addIssue(
            ValidationSeverity::Fatal,
            ValidationIssueCode::ParsedModelUnavailable,
            "direct parsed-model tables are unavailable");
    }
    if (asset.logicalWidth == 0U || asset.logicalHeight == 0U) {
        builder.addIssue(
            ValidationSeverity::Fatal,
            ValidationIssueCode::InvalidDimensions,
            "asset has zero logical dimensions");
    }
    if (!finite(asset.frameRate) || asset.frameRate <= 0.0) {
        builder.addIssue(
            ValidationSeverity::Fatal,
            ValidationIssueCode::InvalidFrameRate,
            "asset has an invalid frame rate");
    }
    if (asset.totalFrames == 0U) {
        builder.addIssue(
            ValidationSeverity::Fatal,
            ValidationIssueCode::InvalidDuration,
            "asset has no timeline frames");
    }

    validateTableLimits(asset, options.limits, builder);
    validateCompositions(asset, builder);
    validateSourceNodes(asset, options.limits, builder);
    validateParentGraph(asset, false, builder);
    validateParentGraph(asset, true, builder);
    validateCompositionGraph(asset, builder);
    validateProperties(asset, builder);
    validateTracksAndSegments(asset, options.limits, builder, report.complexity);
    validateTypedStorage(asset, builder, report.complexity);

    report.complexity.hierarchyDepth = computeHierarchyDepth(
        asset, builder, options.limits);
    for (const auto& node : asset.sourceNodes) {
        if (!node.present) {
            continue;
        }
        report.complexity.maximumPropertiesOnNode = std::max(
            report.complexity.maximumPropertiesOnNode,
            static_cast<std::size_t>(node.properties.count));
    }
    report.complexity.animatedPropertyCount = asset.statistics.animatedPropertyCount;
    report.complexity.estimatedWorkUnits = asset.sourceNodes.size();
    report.complexity.estimatedWorkUnits = saturatingAdd(
        report.complexity.estimatedWorkUnits,
        saturatingMultiply(asset.properties.size(), 2U));
    report.complexity.estimatedWorkUnits = saturatingAdd(
        report.complexity.estimatedWorkUnits,
        saturatingMultiply(asset.segments.size(), 3U));
    report.complexity.estimatedWorkUnits = saturatingAdd(
        report.complexity.estimatedWorkUnits,
        report.complexity.shapePointCount);
    report.complexity.estimatedWorkUnits = saturatingAdd(
        report.complexity.estimatedWorkUnits,
        report.complexity.gradientValueCount);

    collectFeatures(asset, builder);
    builder.finishFeatures();
    addSupportIssues(options, report.features, builder);

    if (options.profile == ValidationProfile::TelegramSticker) {
        if (context.sourceFormat != AssetSourceFormat::TelegramTgs) {
            builder.addIssue(
                ValidationSeverity::Error,
                ValidationIssueCode::TelegramSourceMustBeTgs,
                "Telegram sticker profile requires a .tgs gzip container");
        }
        if (context.encodedBytes > 64U * 1024U) {
            builder.addIssue(
                ValidationSeverity::Error,
                ValidationIssueCode::TelegramCompressedSizeExceeded,
                "Telegram sticker exceeds the 64 KiB compressed-size limit");
        }
        if (asset.logicalWidth != 512U || asset.logicalHeight != 512U) {
            builder.addIssue(
                ValidationSeverity::Error,
                ValidationIssueCode::TelegramCanvasMismatch,
                "Telegram animated stickers require a 512x512 canvas");
        }
        if (!finite(asset.frameRate) || asset.frameRate < 30.0 || asset.frameRate > 60.0) {
            builder.addIssue(
                ValidationSeverity::Error,
                ValidationIssueCode::TelegramFrameRateOutOfRange,
                "Telegram animated sticker frame rate must be between 30 and 60 FPS");
        } else if (std::abs(asset.frameRate - 60.0) > 1.0e-6) {
            builder.addIssue(
                ValidationSeverity::Warning,
                ValidationIssueCode::TelegramFrameRateNotPreferred,
                "Telegram's primary animated-sticker authoring profile uses 60 FPS");
        }
        // Pinned Telegram rlottie exposes totalFrame() as end-start while
        // duration() uses frameDuration() = end-start-1.  Use the same
        // endpoint semantics so a 181-sample 60-FPS composition is exactly
        // three seconds rather than 3.0166 seconds.
        const auto authoredFrameDuration = asset.totalFrames > 0U
            ? asset.totalFrames - 1U
            : 0U;
        const auto duration = asset.frameRate > 0.0
            ? static_cast<double>(authoredFrameDuration) / asset.frameRate
            : 0.0;
        if (!finite(duration) || duration > 3.0 + 1.0e-9) {
            builder.addIssue(
                ValidationSeverity::Error,
                ValidationIssueCode::TelegramDurationExceeded,
                "Telegram animated sticker duration exceeds 3 seconds");
        }
        if (!context.loopIntentKnown) {
            builder.addIssue(
                ValidationSeverity::Warning,
                ValidationIssueCode::TelegramLoopUnknown,
                "loop intent is not represented by the Lottie payload and must be supplied by the host");
        } else if (!context.intendedLoop) {
            builder.addIssue(
                ValidationSeverity::Error,
                ValidationIssueCode::TelegramLoopRequired,
                "Telegram animated stickers are required to loop");
        }
        builder.addIssue(
            ValidationSeverity::Info,
            ValidationIssueCode::TelegramBoundsNotProven,
            "authored-model validation cannot prove that all animated bounds remain inside the canvas");
        validateTelegramFeatures(report.features, builder);
    }

    report.structurallyValid = !builder.structuralFailure();
    report.profileCompliant = report.structurallyValid && !builder.profileFailure();
    report.requiresReferenceFallback = report.fallbackFeatureOccurrences != 0U;
    report.containsUnsupportedFeatures = report.unsupportedFeatureOccurrences != 0U;
    report.nativeDirect2DReady = report.structurallyValid
        && report.fallbackFeatureOccurrences == 0U
        && report.unsupportedFeatureOccurrences == 0U
        && report.nativeSubsetFeatureOccurrences == 0U;
    finalizeFingerprint(report);
    return report;
}

std::string_view toString(AssetSourceFormat value) noexcept {
    switch (value) {
    case AssetSourceFormat::Unknown: return "unknown";
    case AssetSourceFormat::LottieJson: return "lottie-json";
    case AssetSourceFormat::TelegramTgs: return "telegram-tgs";
    }
    return "unknown";
}

std::string_view toString(ValidationProfile value) noexcept {
    switch (value) {
    case ValidationProfile::ApplicationAsset: return "application";
    case ValidationProfile::TelegramSticker: return "telegram-sticker";
    case ValidationProfile::Direct2DNative: return "direct2d-native";
    }
    return "application";
}

std::string_view toString(ValidationSeverity value) noexcept {
    switch (value) {
    case ValidationSeverity::Info: return "info";
    case ValidationSeverity::Warning: return "warning";
    case ValidationSeverity::Error: return "error";
    case ValidationSeverity::Fatal: return "fatal";
    }
    return "info";
}

std::string_view toString(FeatureSupport value) noexcept {
    switch (value) {
    case FeatureSupport::Native: return "native";
    case FeatureSupport::NativeSubset: return "native-subset";
    case FeatureSupport::ReferenceFallback: return "reference-fallback";
    case FeatureSupport::Unsupported: return "unsupported";
    }
    return "unsupported";
}

std::string_view toString(FeatureKind value) noexcept {
    switch (value) {
    case FeatureKind::Unknown: return "unknown";
    case FeatureKind::Composition: return "composition";
    case FeatureKind::Precomposition: return "precomposition";
    case FeatureKind::ShapeLayer: return "shape-layer";
    case FeatureKind::SolidLayer: return "solid-layer";
    case FeatureKind::ImageLayer: return "image-layer";
    case FeatureKind::TextLayer: return "text-layer";
    case FeatureKind::NullLayer: return "null-layer";
    case FeatureKind::ShapePath: return "shape-path";
    case FeatureKind::Rectangle: return "rectangle";
    case FeatureKind::Ellipse: return "ellipse";
    case FeatureKind::Polygon: return "polygon";
    case FeatureKind::Star: return "star";
    case FeatureKind::SolidFill: return "solid-fill";
    case FeatureKind::SolidStroke: return "solid-stroke";
    case FeatureKind::DashedStroke: return "dashed-stroke";
    case FeatureKind::GradientFill: return "gradient-fill";
    case FeatureKind::GradientStroke: return "gradient-stroke";
    case FeatureKind::TrimPath: return "trim-path";
    case FeatureKind::Repeater: return "repeater";
    case FeatureKind::Mask: return "mask";
    case FeatureKind::Matte: return "matte";
    case FeatureKind::BlendMode: return "blend-mode";
    case FeatureKind::AutoOrient: return "auto-orient";
    case FeatureKind::SpatialPosition: return "spatial-position";
    case FeatureKind::TimeStretch: return "time-stretch";
    case FeatureKind::TimeRemap: return "time-remap";
    case FeatureKind::UnknownNode: return "unknown-node";
    }
    return "unknown";
}

std::string_view toString(ValidationIssueCode value) noexcept {
    switch (value) {
    case ValidationIssueCode::None: return "none";
    case ValidationIssueCode::SchemaVersionUnsupported: return "schema-version-unsupported";
    case ValidationIssueCode::ParsedModelUnavailable: return "parsed-model-unavailable";
    case ValidationIssueCode::InvalidDimensions: return "invalid-dimensions";
    case ValidationIssueCode::InvalidFrameRate: return "invalid-frame-rate";
    case ValidationIssueCode::InvalidDuration: return "invalid-duration";
    case ValidationIssueCode::InvalidTableId: return "invalid-table-id";
    case ValidationIssueCode::InvalidReference: return "invalid-reference";
    case ValidationIssueCode::InvalidRange: return "invalid-range";
    case ValidationIssueCode::HierarchyCycle: return "hierarchy-cycle";
    case ValidationIssueCode::CompositionCycle: return "composition-cycle";
    case ValidationIssueCode::NonFiniteValue: return "non-finite-value";
    case ValidationIssueCode::InvalidPropertyFlags: return "invalid-property-flags";
    case ValidationIssueCode::InvalidPropertyValue: return "invalid-property-value";
    case ValidationIssueCode::InvalidTrack: return "invalid-track";
    case ValidationIssueCode::InvalidSegment: return "invalid-segment";
    case ValidationIssueCode::UnsortedSegments: return "unsorted-segments";
    case ValidationIssueCode::ShapeTopologyInvalid: return "shape-topology-invalid";
    case ValidationIssueCode::LimitExceeded: return "limit-exceeded";
    case ValidationIssueCode::UnknownFeature: return "unknown-feature";
    case ValidationIssueCode::NativeSubsetRequired: return "native-subset-required";
    case ValidationIssueCode::ReferenceFallbackRequired: return "reference-fallback-required";
    case ValidationIssueCode::UnsupportedFeature: return "unsupported-feature";
    case ValidationIssueCode::TelegramSourceMustBeTgs: return "telegram-source-must-be-tgs";
    case ValidationIssueCode::TelegramCompressedSizeExceeded: return "telegram-compressed-size-exceeded";
    case ValidationIssueCode::TelegramCanvasMismatch: return "telegram-canvas-mismatch";
    case ValidationIssueCode::TelegramFrameRateOutOfRange: return "telegram-frame-rate-out-of-range";
    case ValidationIssueCode::TelegramFrameRateNotPreferred: return "telegram-frame-rate-not-preferred";
    case ValidationIssueCode::TelegramDurationExceeded: return "telegram-duration-exceeded";
    case ValidationIssueCode::TelegramLoopUnknown: return "telegram-loop-unknown";
    case ValidationIssueCode::TelegramLoopRequired: return "telegram-loop-required";
    case ValidationIssueCode::TelegramForbiddenFeature: return "telegram-forbidden-feature";
    case ValidationIssueCode::TelegramBoundsNotProven: return "telegram-bounds-not-proven";
    }
    return "none";
}

} // namespace avemotion::validation
