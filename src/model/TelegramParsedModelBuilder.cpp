#include "TelegramParsedModelBuilder.hpp"

#include "avemotion/core/Hash.hpp"
#include "avemotion/evaluation/PropertyEvaluator.hpp"

#include "lottieloader.h"
#include "lottiemodel.h"
#include "vinterpolator.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace avemotion::model::detail {
namespace {

constexpr std::size_t kMaximumSourceNodes = 1'000'000U;
constexpr std::size_t kMaximumProperties = 4'000'000U;
constexpr std::size_t kMaximumSegments = 8'000'000U;
constexpr std::size_t kMaximumValueElements = 32'000'000U;

[[nodiscard]] std::uint64_t hashName(const std::string& value) noexcept {
    core::Fnv1a64 hash;
    hash.appendString(value);
    return hash.value();
}

[[nodiscard]] bool finite(float value) noexcept {
    return std::isfinite(value);
}

[[nodiscard]] MotionVec2Value toVec2(const VPointF& value) noexcept {
    return {value.x(), value.y()};
}

[[nodiscard]] MotionMatrix3x2Value toMatrix(const VMatrix& value) noexcept {
    return {
        value.m_11(), value.m_12(),
        value.m_21(), value.m_22(),
        value.m_tx(), value.m_ty(),
    };
}

[[nodiscard]] bool linearControls(
    MotionVec2Value first,
    MotionVec2Value second) noexcept {
    return first.x == 0.0F && first.y == 0.0F
        && second.x == 1.0F && second.y == 1.0F;
}

[[nodiscard]] SourceLayerKind sourceLayerKind(LayerType value) noexcept {
    switch (value) {
    case LayerType::Precomp: return SourceLayerKind::Precomposition;
    case LayerType::Solid: return SourceLayerKind::Solid;
    case LayerType::Image: return SourceLayerKind::Image;
    case LayerType::Null: return SourceLayerKind::Null;
    case LayerType::Shape: return SourceLayerKind::Shape;
    case LayerType::Text: return SourceLayerKind::Text;
    }
    return SourceLayerKind::Unknown;
}

[[nodiscard]] SourceFillRule sourceFillRule(FillRule value) noexcept {
    return value == FillRule::EvenOdd
        ? SourceFillRule::EvenOdd : SourceFillRule::Winding;
}

[[nodiscard]] SourceStrokeCap sourceStrokeCap(CapStyle value) noexcept {
    switch (value) {
    case CapStyle::Flat: return SourceStrokeCap::Flat;
    case CapStyle::Square: return SourceStrokeCap::Square;
    case CapStyle::Round: return SourceStrokeCap::Round;
    }
    return SourceStrokeCap::Flat;
}

[[nodiscard]] SourceStrokeJoin sourceStrokeJoin(JoinStyle value) noexcept {
    switch (value) {
    case JoinStyle::Miter: return SourceStrokeJoin::Miter;
    case JoinStyle::Bevel: return SourceStrokeJoin::Bevel;
    case JoinStyle::Round: return SourceStrokeJoin::Round;
    }
    return SourceStrokeJoin::Miter;
}

[[nodiscard]] SourceMaskMode sourceMaskMode(LOTMaskData::Mode value) noexcept {
    switch (value) {
    case LOTMaskData::Mode::None: return SourceMaskMode::None;
    case LOTMaskData::Mode::Add: return SourceMaskMode::Add;
    case LOTMaskData::Mode::Substarct: return SourceMaskMode::Subtract;
    case LOTMaskData::Mode::Intersect: return SourceMaskMode::Intersect;
    case LOTMaskData::Mode::Difference: return SourceMaskMode::Difference;
    }
    return SourceMaskMode::None;
}

[[nodiscard]] SourceMatteMode sourceMatteMode(MatteType value) noexcept {
    switch (value) {
    case MatteType::None: return SourceMatteMode::None;
    case MatteType::Alpha: return SourceMatteMode::Alpha;
    case MatteType::AlphaInv: return SourceMatteMode::AlphaInverted;
    case MatteType::Luma: return SourceMatteMode::Luma;
    case MatteType::LumaInv: return SourceMatteMode::LumaInverted;
    }
    return SourceMatteMode::None;
}

[[nodiscard]] SourceBlendMode sourceBlendMode(LottieBlendMode value) noexcept {
    switch (value) {
    case LottieBlendMode::Normal: return SourceBlendMode::Normal;
    case LottieBlendMode::Multiply: return SourceBlendMode::Multiply;
    case LottieBlendMode::Screen: return SourceBlendMode::Screen;
    case LottieBlendMode::OverLay: return SourceBlendMode::Overlay;
    }
    return SourceBlendMode::Normal;
}

[[nodiscard]] SourcePathDirection sourcePathDirection(int value) noexcept {
    return value == 3
        ? SourcePathDirection::CounterClockwise
        : SourcePathDirection::Clockwise;
}

[[nodiscard]] SourcePolystarType sourcePolystarType(
    LOTPolystarData::PolyType value) noexcept {
    return value == LOTPolystarData::PolyType::Star
        ? SourcePolystarType::Star : SourcePolystarType::Polygon;
}

[[nodiscard]] SourceTrimMode sourceTrimMode(LOTTrimData::TrimType value) noexcept {
    return value == LOTTrimData::TrimType::Individually
        ? SourceTrimMode::Individual : SourceTrimMode::Simultaneous;
}

[[nodiscard]] SourceGradientType sourceGradientType(int value) noexcept {
    return value == 2 ? SourceGradientType::Radial : SourceGradientType::Linear;
}

[[nodiscard]] std::uint64_t layerScopeKey(
    CompositionId composition,
    std::int32_t authoredLayerId) noexcept {
    return (static_cast<std::uint64_t>(composition.value) << 32U)
        | static_cast<std::uint32_t>(authoredLayerId);
}

struct PendingTransformParent final {
    SourceNodeId node;
    CompositionId composition;
    std::int32_t authoredParentId = -1;
};

class Extractor final {
public:
    Extractor(
        const LOTModel& source,
        const AssetModelDescriptor& descriptor,
        std::optional<int> oracleFrame = std::nullopt)
        : source_(source), oracleFrame_(oracleFrame) {
        model_ = std::make_shared<MotionAssetModel>();
        model_->assetHandle = descriptor.assetHandle;
        model_->sourceAssetHash = descriptor.sourceAssetHash;
        model_->logicalWidth = descriptor.logicalWidth;
        model_->logicalHeight = descriptor.logicalHeight;
        model_->frameRate = descriptor.frameRate;
        model_->totalFrames = descriptor.totalFrames;
        model_->debugName = descriptor.debugName != nullptr
            ? descriptor.debugName : std::string{};
        model_->statistics.directParsedModel = true;
    }

    ParsedModelBuildResult build() {
        if (!source_.mRoot || !source_.mRoot->mRootLayer) {
            return {nullptr, "Telegram parsed model has no root composition"};
        }

        if (!registerCompositions()) {
            return {nullptr, error_};
        }
        if (!buildCompositions()) {
            return {nullptr, error_};
        }
        resolveTransformParents();
        if (!error_.empty()) {
            return {nullptr, error_};
        }
        if (oracleFrame_ && !finalizeOracleWorldTransforms()) {
            return {nullptr, error_};
        }

        if (model_->clips.empty()) {
            model_->clips.push_back({
                .present = true,
                .id = ClipId{0U},
                .debugName = "default",
                .firstFrame = static_cast<double>(source_.mRoot->mStartFrame),
                .endFrame = static_cast<double>(source_.mRoot->mEndFrame),
                .defaultLoop = ClipLoopHint::Loop,
            });
        }

        populateStatistics();
        if (!validateModel()) {
            return {nullptr, error_};
        }
        model_->parsedModelFingerprint = computeFingerprint();
        model_->revision = 1U;
        return {
            std::const_pointer_cast<const MotionAssetModel>(model_),
            {},
        };
    }

    [[nodiscard]] std::vector<evaluation::MotionPropertyValue> takeOracleProperties() {
        return std::move(oracleProperties_);
    }

    [[nodiscard]] std::vector<evaluation::EvaluatedNodeTransform> takeOracleTransforms() {
        return std::move(oracleTransforms_);
    }

    [[nodiscard]] std::vector<evaluation::EvaluatedShape> takeOracleShapes() {
        return std::move(oracleShapes_);
    }

    [[nodiscard]] std::vector<MotionVec2Value> takeOracleShapePoints() {
        return std::move(oracleShapePoints_);
    }

    [[nodiscard]] bool registerCompositions() {
        collectCompositionSizeHints(source_.mRoot->mRootLayer->mChildren);
        for (const auto& [_, asset] : source_.mRoot->mAssets) {
            if (asset && asset->mAssetType == LOTAsset::Type::Precomp) {
                collectCompositionSizeHints(asset->mLayers);
            }
        }

        const auto rootId = CompositionId{0U};
        model_->compositions.push_back({
            .present = true,
            .id = rootId,
            .rootNode = {},
            .debugName = "root",
            .logicalWidth = static_cast<std::size_t>(
                std::max(0, source_.mRoot->mSize.width())),
            .logicalHeight = static_cast<std::size_t>(
                std::max(0, source_.mRoot->mSize.height())),
            .firstFrame = static_cast<double>(source_.mRoot->mStartFrame),
            .endFrame = static_cast<double>(source_.mRoot->mEndFrame),
            .frameRate = static_cast<double>(source_.mRoot->mFrameRate),
        });

        std::vector<std::pair<std::string, const LOTAsset*>> precompositions;
        precompositions.reserve(source_.mRoot->mAssets.size());
        for (const auto& [key, asset] : source_.mRoot->mAssets) {
            if (asset && asset->mAssetType == LOTAsset::Type::Precomp) {
                precompositions.emplace_back(key, asset.get());
            }
        }
        std::sort(
            precompositions.begin(), precompositions.end(),
            [](const auto& left, const auto& right) {
                return left.first < right.first;
            });

        for (const auto& [key, asset] : precompositions) {
            if (compositionByRefId_.contains(key)) {
                setError("parsed model contains duplicate precomposition IDs");
                return false;
            }
            if (model_->compositions.size() >= kMaximumSourceNodes) {
                setError("parsed model exceeds the supported composition limit");
                return false;
            }

            double firstFrame = static_cast<double>(source_.mRoot->mStartFrame);
            double endFrame = static_cast<double>(source_.mRoot->mEndFrame);
            if (!asset->mLayers.empty()) {
                firstFrame = std::numeric_limits<double>::infinity();
                endFrame = -std::numeric_limits<double>::infinity();
                for (const auto& entry : asset->mLayers) {
                    if (!entry || entry->type() != LOTData::Type::Layer) continue;
                    const auto* layer = static_cast<const LOTLayerData*>(entry.get());
                    firstFrame = std::min(
                        firstFrame, static_cast<double>(layer->mInFrame));
                    endFrame = std::max(
                        endFrame, static_cast<double>(layer->mOutFrame));
                }
                if (!std::isfinite(firstFrame) || !std::isfinite(endFrame)) {
                    firstFrame = static_cast<double>(source_.mRoot->mStartFrame);
                    endFrame = static_cast<double>(source_.mRoot->mEndFrame);
                }
            }

            int width = asset->mWidth;
            int height = asset->mHeight;
            if (const auto hint = compositionSizeHints_.find(key);
                hint != compositionSizeHints_.end()) {
                if (width <= 0) width = hint->second.first;
                if (height <= 0) height = hint->second.second;
            }
            if (width <= 0) width = source_.mRoot->mSize.width();
            if (height <= 0) height = source_.mRoot->mSize.height();

            const auto id = makeId<CompositionId>(model_->compositions.size());
            compositionByRefId_.emplace(key, id);
            precompositionAssets_.push_back(asset);
            model_->compositions.push_back({
                .present = true,
                .id = id,
                .rootNode = {},
                .debugName = key,
                .logicalWidth = static_cast<std::size_t>(std::max(0, width)),
                .logicalHeight = static_cast<std::size_t>(std::max(0, height)),
                .firstFrame = firstFrame,
                .endFrame = endFrame,
                .frameRate = static_cast<double>(source_.mRoot->mFrameRate),
            });
        }
        return true;
    }

    void collectCompositionSizeHints(
        const std::vector<std::shared_ptr<LOTData>>& layers) {
        for (const auto& entry : layers) {
            if (!entry || entry->type() != LOTData::Type::Layer) continue;
            const auto* layer = static_cast<const LOTLayerData*>(entry.get());
            if (layer->mLayerType != LayerType::Precomp
                || !layer->mExtra
                || layer->mExtra->mPreCompRefId.empty()) {
                continue;
            }
            const auto width = layer->mLayerSize.width();
            const auto height = layer->mLayerSize.height();
            auto [position, inserted] = compositionSizeHints_.emplace(
                layer->mExtra->mPreCompRefId,
                std::pair{width, height});
            if (!inserted) {
                if (position->second.first <= 0 && width > 0) {
                    position->second.first = width;
                }
                if (position->second.second <= 0 && height > 0) {
                    position->second.second = height;
                }
            }
        }
    }

    [[nodiscard]] bool buildCompositions() {
        if (!appendComposition(
                CompositionId{0U},
                source_.mRoot->mRootLayer->mChildren)) {
            return false;
        }
        for (std::size_t index = 0; index < precompositionAssets_.size(); ++index) {
            if (!appendComposition(
                    makeId<CompositionId>(index + 1U),
                    precompositionAssets_[index]->mLayers)) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool appendComposition(
        CompositionId composition,
        const std::vector<std::shared_ptr<LOTData>>& layers) {
        if (!composition.valid() || composition.index() >= model_->compositions.size()) {
            setError("parsed composition ID is invalid");
            return false;
        }
        const auto root = beginNode(
            SourceNodeKind::Composition,
            {},
            composition,
            model_->compositions[composition.index()].debugName,
            false,
            std::all_of(
                layers.begin(), layers.end(),
                [](const auto& value) {
                    return value == nullptr || value->isStatic();
                }));
        if (!root.valid()) return false;

        std::vector<SourceNodeId> children;
        children.reserve(layers.size());
        for (const auto& entry : layers) {
            if (!entry || entry->type() != LOTData::Type::Layer) {
                setError("parsed composition contains a non-layer root item");
                return false;
            }
            const auto* layer = static_cast<const LOTLayerData*>(entry.get());
            const auto child = appendLayer(layer, root, composition);
            if (!child.valid()) return false;
            children.push_back(child);
        }
        finishNode(root, children, {});
        model_->compositions[composition.index()].rootNode = root;
        return error_.empty();
    }

    [[nodiscard]] SourceNodeId beginNode(
        SourceNodeKind kind,
        SourceNodeId parent,
        CompositionId composition,
        std::string debugName,
        bool hidden,
        bool authoredStatic) {
        if (model_->sourceNodes.size() >= kMaximumSourceNodes) {
            setError("parsed source graph exceeds the supported node limit");
            return {};
        }
        const auto id = makeId<SourceNodeId>(model_->sourceNodes.size());
        MotionSourceNodeRecord record;
        record.present = true;
        record.id = id;
        record.parent = parent;
        record.composition = composition;
        record.kind = kind;
        record.nameHash = hashName(debugName);
        record.debugName = std::move(debugName);
        record.hidden = hidden;
        record.authoredStatic = authoredStatic;
        record.dependencyBits = authoredStatic
            ? StaticDependencyNone : StaticDependencyTimeline;
        model_->sourceNodes.push_back(std::move(record));
        if (oracleFrame_) {
            evaluation::EvaluatedNodeTransform oracle;
            oracle.node = id;
            oracleTransforms_.push_back(oracle);
            oracleLocalMatrices_.emplace_back();
            oracleLocalMatrixSupported_.push_back(true);
        }
        return id;
    }

    void finishNode(
        SourceNodeId node,
        const std::vector<SourceNodeId>& children,
        const std::vector<PropertyId>& properties) {
        if (!node.valid() || node.index() >= model_->sourceNodes.size()) {
            setError("invalid source node during parsed-model finalization");
            return;
        }
        auto& record = model_->sourceNodes[node.index()];
        record.children = {
            static_cast<std::uint32_t>(model_->sourceChildIds.size()),
            static_cast<std::uint32_t>(children.size()),
        };
        model_->sourceChildIds.insert(
            model_->sourceChildIds.end(), children.begin(), children.end());
        record.properties = {
            static_cast<std::uint32_t>(model_->sourcePropertyIds.size()),
            static_cast<std::uint32_t>(properties.size()),
        };
        model_->sourcePropertyIds.insert(
            model_->sourcePropertyIds.end(), properties.begin(), properties.end());
    }

    [[nodiscard]] SourceNodeId appendLayer(
        const LOTLayerData* layer,
        SourceNodeId parent,
        CompositionId composition) {
        if (layer == nullptr) {
            setError("parsed source graph contains a null layer");
            return {};
        }
        if (!activeData_.insert(layer).second) {
            setError("cycle detected in parsed layer graph");
            return {};
        }

        auto name = layer->mName;
        if (name.empty()) {
            name = "layer:" + std::to_string(layer->mId);
        }
        const auto id = beginNode(
            SourceNodeKind::Layer,
            parent,
            composition,
            std::move(name),
            layer->hidden(),
            layer->isStatic());
        if (!id.valid()) {
            activeData_.erase(layer);
            return {};
        }
        layer->setAveMotionSourceNodeId(id.value);
        auto& layerRecord = model_->sourceNodes[id.index()];
        layerRecord.layerKind = sourceLayerKind(layer->mLayerType);
        layerRecord.authoredLayerId = layer->mId;
        layerRecord.authoredParentLayerId = layer->mParentId;
        layerRecord.inFrame = static_cast<double>(layer->mInFrame);
        layerRecord.outFrame = static_cast<double>(layer->mOutFrame);
        layerRecord.startFrame = static_cast<double>(layer->mStartFrame);
        layerRecord.timeStretch = layer->mTimeStreatch;
        layerRecord.autoOrient = layer->mAutoOrient;
        layerRecord.matteMode = sourceMatteMode(layer->mMatteType);
        layerRecord.blendMode = sourceBlendMode(layer->mBlendMode);
        layerRecord.layerWidth = layer->mLayerSize.width();
        layerRecord.layerHeight = layer->mLayerSize.height();
        if (layer->mExtra) {
            layerRecord.solidColor = {
                layer->mExtra->mSolidColor.r,
                layer->mExtra->mSolidColor.g,
                layer->mExtra->mSolidColor.b,
                1.0F,
            };
            if (!layer->mExtra->mPreCompRefId.empty()) {
                layerRecord.sourceAssetRefHash = hashName(
                    layer->mExtra->mPreCompRefId);
            }
        }
        if (layer->mLayerType == LayerType::Precomp) {
            if (!layer->mExtra || layer->mExtra->mPreCompRefId.empty()) {
                setError("precomposition layer has no referenced asset ID");
                activeData_.erase(layer);
                return {};
            }
            const auto found = compositionByRefId_.find(
                layer->mExtra->mPreCompRefId);
            if (found == compositionByRefId_.end()) {
                setError("precomposition layer references an unknown asset");
                activeData_.erase(layer);
                return {};
            }
            layerRecord.referencedComposition = found->second;
        }
        if (layer->mId >= 0) {
            const auto [_, inserted] = layerByScopedId_.emplace(
                layerScopeKey(composition, layer->mId), id);
            if (!inserted) {
                setError("parsed composition contains duplicate layer IDs");
                activeData_.erase(layer);
                return {};
            }
        }
        if (layer->mParentId >= 0) {
            pendingTransformParents_.push_back({id, composition, layer->mParentId});
        }

        std::vector<PropertyId> properties;
        appendTransformProperties(id, layer->mTransform.get(), properties, layer->mAutoOrient);
        if (layer->mExtra && layer->mExtra->mHasTimeRemap) {
            properties.push_back(addAnimatable(
                id,
                PropertySemantic::LayerTimeRemap,
                layer->mExtra->mTimeRemap));
        }

        std::vector<SourceNodeId> children;
        if (layer->mExtra) {
            for (std::size_t index = 0; index < layer->mExtra->mMasks.size(); ++index) {
                const auto mask = appendMask(
                    layer->mExtra->mMasks[index].get(), id, composition, index);
                if (!mask.valid()) {
                    activeData_.erase(layer);
                    return {};
                }
                children.push_back(mask);
            }
        }
        if (layer->mLayerType != LayerType::Precomp) {
            for (const auto& child : layer->mChildren) {
                const auto sourceChild = appendData(child.get(), id, composition);
                if (!sourceChild.valid()) {
                    activeData_.erase(layer);
                    return {};
                }
                children.push_back(sourceChild);
            }
        }
        finishNode(id, children, properties);
        activeData_.erase(layer);
        return id;
    }

    void resolveTransformParents() {
        for (const auto& pending : pendingTransformParents_) {
            const auto found = layerByScopedId_.find(layerScopeKey(
                pending.composition, pending.authoredParentId));
            if (found == layerByScopedId_.end()) {
                setError("parsed layer references an unresolved transform parent");
                return;
            }
            if (!pending.node.valid()
                || pending.node.index() >= model_->sourceNodes.size()) {
                setError("parsed transform-parent record references an invalid node");
                return;
            }
            model_->sourceNodes[pending.node.index()].transformParent = found->second;
        }
    }

    [[nodiscard]] SourceNodeId appendGroup(
        const LOTGroupData* group,
        SourceNodeId parent,
        CompositionId composition,
        SourceNodeKind kind = SourceNodeKind::ShapeGroup) {
        if (group == nullptr) {
            setError("parsed source graph contains a null group");
            return {};
        }
        if (!activeData_.insert(group).second) {
            setError("cycle detected in parsed shape graph");
            return {};
        }
        auto name = group->mName;
        if (name.empty()) name = "group";
        const auto id = beginNode(
            kind,
            parent,
            composition,
            std::move(name),
            group->hidden(),
            group->isStatic());
        if (!id.valid()) {
            activeData_.erase(group);
            return {};
        }
        group->setAveMotionSourceNodeId(id.value);

        std::vector<PropertyId> properties;
        appendTransformProperties(id, group->mTransform.get(), properties);
        std::vector<SourceNodeId> children;
        for (const auto& child : group->mChildren) {
            const auto sourceChild = appendData(child.get(), id, composition);
            if (!sourceChild.valid()) {
                activeData_.erase(group);
                return {};
            }
            children.push_back(sourceChild);
        }
        finishNode(id, children, properties);
        activeData_.erase(group);
        return id;
    }

    [[nodiscard]] SourceNodeId appendMask(
        const LOTMaskData* mask,
        SourceNodeId parent,
        CompositionId composition,
        std::size_t ordinal) {
        if (mask == nullptr) {
            setError("parsed source graph contains a null mask");
            return {};
        }
        const auto id = beginNode(
            SourceNodeKind::Mask,
            parent,
            composition,
            "mask:" + std::to_string(ordinal),
            false,
            mask->isStatic());
        if (!id.valid()) return {};
        auto& maskRecord = model_->sourceNodes[id.index()];
        maskRecord.maskMode = sourceMaskMode(mask->mMode);
        maskRecord.maskInverted = mask->mInv;
        std::vector<PropertyId> properties;
        properties.push_back(addAnimatable(
            id, PropertySemantic::MaskPath, mask->mShape));
        properties.push_back(addAnimatable(
            id, PropertySemantic::MaskOpacity, mask->mOpacity));
        finishNode(id, {}, properties);
        return id;
    }

    [[nodiscard]] SourceNodeId appendData(
        const LOTData* data,
        SourceNodeId parent,
        CompositionId composition) {
        if (data == nullptr) {
            setError("parsed source graph contains null content");
            return {};
        }
        if (data->type() == LOTData::Type::Layer) {
            return appendLayer(
                static_cast<const LOTLayerData*>(data), parent, composition);
        }
        if (data->type() == LOTData::Type::ShapeGroup) {
            return appendGroup(
                static_cast<const LOTShapeGroupData*>(data), parent, composition);
        }

        SourceNodeKind kind = SourceNodeKind::Unknown;
        switch (data->type()) {
        case LOTData::Type::Fill: kind = SourceNodeKind::Fill; break;
        case LOTData::Type::Stroke: kind = SourceNodeKind::Stroke; break;
        case LOTData::Type::GFill: kind = SourceNodeKind::GradientFill; break;
        case LOTData::Type::GStroke: kind = SourceNodeKind::GradientStroke; break;
        case LOTData::Type::Rect: kind = SourceNodeKind::Rectangle; break;
        case LOTData::Type::Ellipse: kind = SourceNodeKind::Ellipse; break;
        case LOTData::Type::Shape: kind = SourceNodeKind::Shape; break;
        case LOTData::Type::Polystar: kind = SourceNodeKind::Polystar; break;
        case LOTData::Type::Trim: kind = SourceNodeKind::Trim; break;
        case LOTData::Type::Repeater: kind = SourceNodeKind::Repeater; break;
        default: kind = SourceNodeKind::Unknown; break;
        }
        auto name = data->mName;
        if (name.empty()) name = "source-node";
        const auto id = beginNode(
            kind,
            parent,
            composition,
            std::move(name),
            data->hidden(),
            data->isStatic());
        if (!id.valid()) return {};
        data->setAveMotionSourceNodeId(id.value);

        std::vector<PropertyId> properties;
        std::vector<SourceNodeId> children;
        switch (data->type()) {
        case LOTData::Type::Fill: {
            const auto* value = static_cast<const LOTFillData*>(data);
            auto& record = model_->sourceNodes[id.index()];
            record.fillRule = sourceFillRule(value->mFillRule);
            record.enabled = value->mEnabled;
            properties.push_back(addAnimatable(
                id, PropertySemantic::FillColor, value->mColor));
            properties.push_back(addAnimatable(
                id, PropertySemantic::FillOpacity, value->mOpacity));
            break;
        }
        case LOTData::Type::Stroke: {
            const auto* value = static_cast<const LOTStrokeData*>(data);
            auto& record = model_->sourceNodes[id.index()];
            record.strokeCap = sourceStrokeCap(value->mCapStyle);
            record.strokeJoin = sourceStrokeJoin(value->mJoinStyle);
            record.miterLimit = value->mMiterLimit;
            record.enabled = value->mEnabled;
            properties.push_back(addAnimatable(
                id, PropertySemantic::StrokeColor, value->mColor));
            properties.push_back(addAnimatable(
                id, PropertySemantic::StrokeOpacity, value->mOpacity));
            properties.push_back(addAnimatable(
                id, PropertySemantic::StrokeWidth, value->mWidth));
            appendDashProperties(id, value->mDash, properties);
            break;
        }
        case LOTData::Type::GFill: {
            const auto& value = *static_cast<const LOTGFillData*>(data);
            auto& record = model_->sourceNodes[id.index()];
            record.fillRule = sourceFillRule(value.mFillRule);
            record.gradientType = sourceGradientType(value.mGradientType);
            record.gradientColorPointCount = value.mColorPoints;
            record.enabled = value.mEnabled;
            appendGradientProperties(id, value, properties);
            break;
        }
        case LOTData::Type::GStroke: {
            const auto& value = *static_cast<const LOTGStrokeData*>(data);
            auto& record = model_->sourceNodes[id.index()];
            record.strokeCap = sourceStrokeCap(value.mCapStyle);
            record.strokeJoin = sourceStrokeJoin(value.mJoinStyle);
            record.miterLimit = value.mMiterLimit;
            record.gradientType = sourceGradientType(value.mGradientType);
            record.gradientColorPointCount = value.mColorPoints;
            record.enabled = value.mEnabled;
            appendGradientProperties(id, value, properties);
            properties.push_back(addAnimatable(
                id, PropertySemantic::StrokeWidth, value.mWidth));
            appendDashProperties(id, value.mDash, properties);
            break;
        }
        case LOTData::Type::Rect: {
            const auto* value = static_cast<const LOTRectData*>(data);
            model_->sourceNodes[id.index()].pathDirection =
                sourcePathDirection(value->mDirection);
            properties.push_back(addAnimatable(
                id, PropertySemantic::RectanglePosition, value->mPos));
            properties.push_back(addAnimatable(
                id, PropertySemantic::RectangleSize, value->mSize));
            properties.push_back(addAnimatable(
                id, PropertySemantic::RectangleRoundness, value->mRound));
            break;
        }
        case LOTData::Type::Ellipse: {
            const auto* value = static_cast<const LOTEllipseData*>(data);
            model_->sourceNodes[id.index()].pathDirection =
                sourcePathDirection(value->mDirection);
            properties.push_back(addAnimatable(
                id, PropertySemantic::EllipsePosition, value->mPos));
            properties.push_back(addAnimatable(
                id, PropertySemantic::EllipseSize, value->mSize));
            break;
        }
        case LOTData::Type::Shape: {
            const auto* value = static_cast<const LOTShapeData*>(data);
            model_->sourceNodes[id.index()].pathDirection =
                sourcePathDirection(value->mDirection);
            properties.push_back(addAnimatable(
                id, PropertySemantic::ShapePath, value->mShape));
            break;
        }
        case LOTData::Type::Polystar: {
            const auto* value = static_cast<const LOTPolystarData*>(data);
            auto& record = model_->sourceNodes[id.index()];
            record.pathDirection = sourcePathDirection(value->mDirection);
            record.polystarType = sourcePolystarType(value->mPolyType);
            properties.push_back(addAnimatable(
                id, PropertySemantic::PolystarPosition, value->mPos));
            properties.push_back(addAnimatable(
                id, PropertySemantic::PolystarPointCount, value->mPointCount));
            properties.push_back(addAnimatable(
                id, PropertySemantic::PolystarInnerRadius, value->mInnerRadius));
            properties.push_back(addAnimatable(
                id, PropertySemantic::PolystarOuterRadius, value->mOuterRadius));
            properties.push_back(addAnimatable(
                id, PropertySemantic::PolystarInnerRoundness,
                value->mInnerRoundness));
            properties.push_back(addAnimatable(
                id, PropertySemantic::PolystarOuterRoundness,
                value->mOuterRoundness));
            properties.push_back(addAnimatable(
                id, PropertySemantic::PolystarRotation, value->mRotation));
            break;
        }
        case LOTData::Type::Trim: {
            const auto* value = static_cast<const LOTTrimData*>(data);
            model_->sourceNodes[id.index()].trimMode = sourceTrimMode(value->mTrimType);
            properties.push_back(addAnimatable(
                id, PropertySemantic::TrimStart, value->mStart));
            properties.push_back(addAnimatable(
                id, PropertySemantic::TrimEnd, value->mEnd));
            properties.push_back(addAnimatable(
                id, PropertySemantic::TrimOffset, value->mOffset));
            break;
        }
        case LOTData::Type::Repeater: {
            const auto* value = static_cast<const LOTRepeaterData*>(data);
            model_->sourceNodes[id.index()].repeaterMaximumCopies = value->mMaxCopies;
            properties.push_back(addAnimatable(
                id, PropertySemantic::RepeaterCopies, value->mCopies));
            properties.push_back(addAnimatable(
                id, PropertySemantic::RepeaterOffset, value->mOffset));
            appendRepeaterTransformProperties(id, value->mTransform, properties);
            if (value->content() != nullptr) {
                const auto child = appendGroup(
                    value->content(), id, composition, SourceNodeKind::ShapeGroup);
                if (!child.valid()) return {};
                children.push_back(child);
            }
            break;
        }
        default:
            break;
        }
        finishNode(id, children, properties);
        return id;
    }

    void appendTransformProperties(
        SourceNodeId owner,
        const LOTTransformData* transform,
        std::vector<PropertyId>& output,
        bool autoOrient = false) {
        if (transform == nullptr) return;
        appendOracleTransform(owner, transform, autoOrient);
        if (transform->isStatic()) {
            output.push_back(addStaticProperty(
                owner,
                PropertySemantic::TransformMatrix,
                appendValue(transform->aveMotionStaticMatrix())));
            output.push_back(addStaticProperty(
                owner,
                PropertySemantic::TransformOpacity,
                appendValue(transform->aveMotionStaticOpacity() * 100.0F)));
            return;
        }
        const auto* data = transform->aveMotionDynamicData();
        if (data == nullptr) {
            setError("animated transform has no component data");
            return;
        }
        output.push_back(addAnimatable(
            owner, PropertySemantic::TransformRotation, data->mRotation));
        output.push_back(addAnimatable(
            owner, PropertySemantic::TransformScale, data->mScale));
        output.push_back(addAnimatable(
            owner, PropertySemantic::TransformAnchor, data->mAnchor));
        output.push_back(addAnimatable(
            owner, PropertySemantic::TransformOpacity, data->mOpacity));
        if (data->mExtra && data->mExtra->mSeparate) {
            output.push_back(addAnimatable(
                owner,
                PropertySemantic::TransformPositionX,
                data->mExtra->mSeparateX,
                PropertyFlagSeparatedDimension));
            output.push_back(addAnimatable(
                owner,
                PropertySemantic::TransformPositionY,
                data->mExtra->mSeparateY,
                PropertyFlagSeparatedDimension));
        } else {
            output.push_back(addAnimatable(
                owner, PropertySemantic::TransformPosition, data->mPosition));
        }
        if (data->mExtra && data->mExtra->m3DData) {
            output.push_back(addAnimatable(
                owner, PropertySemantic::TransformRotationX, data->mExtra->m3DRx));
            output.push_back(addAnimatable(
                owner, PropertySemantic::TransformRotationY, data->mExtra->m3DRy));
            output.push_back(addAnimatable(
                owner, PropertySemantic::TransformRotationZ, data->mExtra->m3DRz));
        }
    }

    void appendRepeaterTransformProperties(
        SourceNodeId owner,
        const LOTRepeaterTransform& transform,
        std::vector<PropertyId>& output) {
        output.push_back(addAnimatable(
            owner, PropertySemantic::RepeaterPosition, transform.mPosition));
        output.push_back(addAnimatable(
            owner, PropertySemantic::RepeaterScale, transform.mScale));
        output.push_back(addAnimatable(
            owner, PropertySemantic::RepeaterRotation, transform.mRotation));
        output.push_back(addAnimatable(
            owner, PropertySemantic::RepeaterAnchor, transform.mAnchor));
        output.push_back(addAnimatable(
            owner, PropertySemantic::RepeaterStartOpacity,
            transform.mStartOpacity));
        output.push_back(addAnimatable(
            owner, PropertySemantic::RepeaterEndOpacity,
            transform.mEndOpacity));
    }

    template <typename Gradient>
    void appendGradientProperties(
        SourceNodeId owner,
        const Gradient& gradient,
        std::vector<PropertyId>& output) {
        output.push_back(addAnimatable(
            owner, PropertySemantic::GradientStart, gradient.mStartPoint));
        output.push_back(addAnimatable(
            owner, PropertySemantic::GradientEnd, gradient.mEndPoint));
        output.push_back(addAnimatable(
            owner, PropertySemantic::GradientHighlightLength,
            gradient.mHighlightLength));
        output.push_back(addAnimatable(
            owner, PropertySemantic::GradientHighlightAngle,
            gradient.mHighlightAngle));
        output.push_back(addAnimatable(
            owner, PropertySemantic::GradientOpacity, gradient.mOpacity));
        output.push_back(addAnimatable(
            owner, PropertySemantic::GradientStops, gradient.mGradient));
    }

    void appendDashProperties(
        SourceNodeId owner,
        const LOTDashProperty& dash,
        std::vector<PropertyId>& output) {
        if (dash.mData.size()
            > static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max())
                + 1U) {
            setError("parsed dash pattern exceeds component-index limit");
            return;
        }
        for (std::size_t index = 0; index < dash.mData.size(); ++index) {
            output.push_back(addAnimatable(
                owner,
                PropertySemantic::StrokeDashValue,
                dash.mData[index],
                PropertyFlagNone,
                static_cast<std::uint16_t>(index)));
        }
    }

    [[nodiscard]] PropertyId addStaticProperty(
        SourceNodeId owner,
        PropertySemantic semantic,
        MotionValueRef value,
        std::uint32_t extraFlags = PropertyFlagNone,
        std::uint16_t semanticIndex = 0U) {
        if (model_->properties.size() >= kMaximumProperties) {
            setError("parsed model exceeds property limit");
            return {};
        }
        const auto id = makeId<PropertyId>(model_->properties.size());
        model_->properties.push_back({
            .present = true,
            .id = id,
            .owner = owner,
            .semantic = semantic,
            .semanticIndex = semanticIndex,
            .valueType = value.type,
            .flags = PropertyFlagStatic | extraFlags,
            .staticValue = value,
            .track = {},
        });
        appendOracleStatic(id, value);
        return id;
    }

    template <typename T>
    [[nodiscard]] PropertyId addAnimatable(
        SourceNodeId owner,
        PropertySemantic semantic,
        const LOTAnimatable<T>& value,
        std::uint32_t extraFlags = PropertyFlagNone,
        std::uint16_t semanticIndex = 0U) {
        if (value.isStatic()) {
            return addStaticProperty(
                owner,
                semantic,
                appendValue(value.value()),
                extraFlags,
                semanticIndex);
        }
        if (model_->properties.size() >= kMaximumProperties) {
            setError("parsed model exceeds property limit");
            return {};
        }
        const auto& keyframes = value.animation().mKeyFrames;
        if (keyframes.empty()) {
            setError("animated property contains no keyframes");
            return {};
        }
        const auto propertyId = makeId<PropertyId>(model_->properties.size());
        const auto trackId = makeId<TrackId>(model_->tracks.size());
        const auto firstSegment = static_cast<std::uint32_t>(model_->segments.size());
        double trackStart = 0.0;
        double trackEnd = 0.0;
        bool first = true;
        std::uint32_t propertyFlags = PropertyFlagAnimated | extraFlags;

        double previousStart = -std::numeric_limits<double>::infinity();
        for (const auto& keyframe : keyframes) {
            if (model_->segments.size() >= kMaximumSegments) {
                setError("parsed model exceeds segment limit");
                return {};
            }
            const auto segmentStart = static_cast<double>(keyframe.mStartFrame);
            const auto segmentEnd = static_cast<double>(keyframe.mEndFrame);
            if (!std::isfinite(segmentStart) || !std::isfinite(segmentEnd)
                || segmentEnd < segmentStart || segmentStart < previousStart) {
                setError("parsed model contains invalid or unsorted keyframe times");
                return {};
            }
            previousStart = segmentStart;
            MotionSegmentRecord segment;
            segment.present = true;
            segment.id = makeId<SegmentId>(model_->segments.size());
            segment.track = trackId;
            segment.firstFrame = segmentStart;
            segment.endFrame = segmentEnd;
            segment.startValue = appendValue(keyframe.mValue.mStartValue);
            segment.endValue = appendValue(keyframe.mValue.mEndValue);
            if (!segment.startValue.valid() || !segment.endValue.valid()
                || segment.startValue.type != segment.endValue.type) {
                setError("parsed keyframe contains invalid or mismatched values");
                return {};
            }
            if (keyframe.mInterpolator) {
                const auto firstControl =
                    keyframe.mInterpolator->aveMotionControlPoint1();
                const auto secondControl =
                    keyframe.mInterpolator->aveMotionControlPoint2();
                segment.temporalControl1 = toVec2(firstControl);
                segment.temporalControl2 = toVec2(secondControl);
                if (!finite(segment.temporalControl1.x)
                    || !finite(segment.temporalControl1.y)
                    || !finite(segment.temporalControl2.x)
                    || !finite(segment.temporalControl2.y)) {
                    setError("parsed keyframe contains non-finite easing controls");
                    return {};
                }
                segment.interpolation = linearControls(
                    segment.temporalControl1,
                    segment.temporalControl2)
                    ? SegmentInterpolation::Linear
                    : SegmentInterpolation::CubicBezier;
            } else {
                segment.interpolation = SegmentInterpolation::Hold;
            }
            if constexpr (std::is_same_v<T, VPointF>) {
                if (keyframe.mValue.mPathKeyFrame) {
                    segment.spatialInterpolation = SpatialInterpolation::CubicBezier;
                    segment.spatialInTangent = toVec2(keyframe.mValue.mInTangent);
                    segment.spatialOutTangent = toVec2(keyframe.mValue.mOutTangent);
                    if (!finite(segment.spatialInTangent.x)
                        || !finite(segment.spatialInTangent.y)
                        || !finite(segment.spatialOutTangent.x)
                        || !finite(segment.spatialOutTangent.y)) {
                        setError("parsed keyframe contains non-finite spatial tangents");
                        return {};
                    }
                    propertyFlags |= PropertyFlagSpatial;
                }
            }
            if (first) {
                trackStart = segment.firstFrame;
                first = false;
            }
            trackEnd = segment.endFrame;
            model_->segments.push_back(std::move(segment));
        }

        model_->tracks.push_back({
            .present = true,
            .id = trackId,
            .property = propertyId,
            .segments = {
                firstSegment,
                static_cast<std::uint32_t>(model_->segments.size() - firstSegment),
            },
            .firstFrame = trackStart,
            .endFrame = trackEnd,
        });
        model_->properties.push_back({
            .present = true,
            .id = propertyId,
            .owner = owner,
            .semantic = semantic,
            .semanticIndex = semanticIndex,
            .valueType = valueType<T>(),
            .flags = propertyFlags,
            .staticValue = {},
            .track = trackId,
        });
        appendOracleAnimated(propertyId, value);
        return propertyId;
    }

    template <typename T>
    [[nodiscard]] static constexpr PropertyValueType valueType() noexcept {
        if constexpr (std::is_same_v<T, float>) {
            return PropertyValueType::Scalar;
        } else if constexpr (std::is_same_v<T, VPointF>) {
            return PropertyValueType::Vec2;
        } else if constexpr (std::is_same_v<T, LottieColor>) {
            return PropertyValueType::Color;
        } else if constexpr (std::is_same_v<T, LottieShapeData>) {
            return PropertyValueType::Shape;
        } else if constexpr (std::is_same_v<T, LottieGradient>) {
            return PropertyValueType::Gradient;
        } else {
            return PropertyValueType::None;
        }
    }

    [[nodiscard]] MotionValueRef appendValue(float value) {
        if (!finite(value)) {
            setError("parsed model contains a non-finite scalar value");
            return {};
        }
        if (model_->scalarValues.size() >= kMaximumValueElements) {
            setError("parsed model scalar-value storage exceeds limit");
            return {};
        }
        const auto index = static_cast<std::uint32_t>(model_->scalarValues.size());
        model_->scalarValues.push_back(value);
        return {PropertyValueType::Scalar, index};
    }

    [[nodiscard]] MotionValueRef appendValue(const VPointF& value) {
        if (!finite(value.x()) || !finite(value.y())) {
            setError("parsed model contains a non-finite Vec2 value");
            return {};
        }
        if (model_->vec2Values.size() >= kMaximumValueElements) {
            setError("parsed model Vec2 storage exceeds limit");
            return {};
        }
        const auto index = static_cast<std::uint32_t>(model_->vec2Values.size());
        model_->vec2Values.push_back(toVec2(value));
        return {PropertyValueType::Vec2, index};
    }

    [[nodiscard]] MotionValueRef appendValue(const LottieColor& value) {
        if (!finite(value.r) || !finite(value.g) || !finite(value.b)) {
            setError("parsed model contains a non-finite color value");
            return {};
        }
        if (model_->colorValues.size() >= kMaximumValueElements) {
            setError("parsed model color storage exceeds limit");
            return {};
        }
        const auto index = static_cast<std::uint32_t>(model_->colorValues.size());
        model_->colorValues.push_back({value.r, value.g, value.b, 1.0F});
        return {PropertyValueType::Color, index};
    }

    [[nodiscard]] MotionValueRef appendValue(const VMatrix& value) {
        const auto matrix = toMatrix(value);
        if (!finite(matrix.m11) || !finite(matrix.m12)
            || !finite(matrix.m21) || !finite(matrix.m22)
            || !finite(matrix.dx) || !finite(matrix.dy)) {
            setError("parsed model contains a non-finite transform matrix");
            return {};
        }
        if (model_->matrixValues.size() >= kMaximumValueElements) {
            setError("parsed model matrix storage exceeds limit");
            return {};
        }
        const auto index = static_cast<std::uint32_t>(model_->matrixValues.size());
        model_->matrixValues.push_back(matrix);
        return {PropertyValueType::Matrix3x2, index};
    }

    [[nodiscard]] MotionValueRef appendValue(const LottieShapeData& value) {
        if (model_->shapeValues.size() >= kMaximumValueElements
            || value.mPoints.size() > kMaximumValueElements
            || model_->shapePoints.size()
                > kMaximumValueElements - value.mPoints.size()) {
            setError("parsed model shape storage exceeds limit");
            return {};
        }
        const auto first = static_cast<std::uint32_t>(model_->shapePoints.size());
        for (const auto& point : value.mPoints) {
            if (!finite(point.x()) || !finite(point.y())) {
                setError("parsed model contains a non-finite shape point");
                return {};
            }
            model_->shapePoints.push_back(toVec2(point));
        }
        const auto index = static_cast<std::uint32_t>(model_->shapeValues.size());
        model_->shapeValues.push_back({
            .points = {first, static_cast<std::uint32_t>(value.mPoints.size())},
            .closed = value.mClosed,
        });
        return {PropertyValueType::Shape, index};
    }

    [[nodiscard]] MotionValueRef appendValue(const LottieGradient& value) {
        if (model_->gradientValues.size() >= kMaximumValueElements
            || value.mGradient.size() > kMaximumValueElements
            || model_->gradientFloats.size()
                > kMaximumValueElements - value.mGradient.size()) {
            setError("parsed model gradient storage exceeds limit");
            return {};
        }
        const auto first = static_cast<std::uint32_t>(model_->gradientFloats.size());
        for (const auto component : value.mGradient) {
            if (!finite(component)) {
                setError("parsed model contains a non-finite gradient component");
                return {};
            }
            model_->gradientFloats.push_back(component);
        }
        const auto index = static_cast<std::uint32_t>(model_->gradientValues.size());
        model_->gradientValues.push_back({
            .values = {first, static_cast<std::uint32_t>(value.mGradient.size())},
        });
        return {PropertyValueType::Gradient, index};
    }

    [[nodiscard]] evaluation::MotionPropertyValue oracleValue(
        MotionValueRef reference) const noexcept {
        evaluation::MotionPropertyValue result;
        result.type = reference.type;
        if (!reference.valid()) return result;
        switch (reference.type) {
        case PropertyValueType::Scalar:
            if (reference.index < model_->scalarValues.size()) {
                result.storage = evaluation::PropertyStorageKind::Materialized;
                result.scalar = model_->scalarValues[reference.index];
            }
            break;
        case PropertyValueType::Vec2:
            if (reference.index < model_->vec2Values.size()) {
                result.storage = evaluation::PropertyStorageKind::Materialized;
                result.vec2 = model_->vec2Values[reference.index];
            }
            break;
        case PropertyValueType::Color:
            if (reference.index < model_->colorValues.size()) {
                result.storage = evaluation::PropertyStorageKind::Materialized;
                result.color = model_->colorValues[reference.index];
            }
            break;
        case PropertyValueType::Matrix3x2:
            if (reference.index < model_->matrixValues.size()) {
                result.storage = evaluation::PropertyStorageKind::Materialized;
                result.matrix = model_->matrixValues[reference.index];
            }
            break;
        case PropertyValueType::Shape:
        case PropertyValueType::Gradient:
            result.storage = evaluation::PropertyStorageKind::AssetReference;
            result.assetReference = reference;
            break;
        case PropertyValueType::None:
            break;
        }
        return result;
    }

    void appendOracleStatic(PropertyId id, MotionValueRef reference) {
        if (!oracleFrame_) return;
        if (id.index() != oracleProperties_.size()) {
            setError("Telegram property oracle order differs from canonical IDs");
            return;
        }
        oracleProperties_.push_back(oracleValue(reference));
    }

    template <typename T>
    [[nodiscard]] evaluation::MotionPropertyValue oracleMaterialized(
        PropertyId property,
        const T& value) {
        evaluation::MotionPropertyValue result;
        if constexpr (std::is_same_v<T, float>) {
            result.type = PropertyValueType::Scalar;
            result.storage = evaluation::PropertyStorageKind::Materialized;
            result.scalar = value;
        } else if constexpr (std::is_same_v<T, VPointF>) {
            result.type = PropertyValueType::Vec2;
            result.storage = evaluation::PropertyStorageKind::Materialized;
            result.vec2 = toVec2(value);
        } else if constexpr (std::is_same_v<T, LottieColor>) {
            result.type = PropertyValueType::Color;
            result.storage = evaluation::PropertyStorageKind::Materialized;
            result.color = {value.r, value.g, value.b, 1.0F};
        } else if constexpr (std::is_same_v<T, LottieShapeData>) {
            if (oracleShapePoints_.size()
                    > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())
                || value.mPoints.size()
                    > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())
                || oracleShapePoints_.size() + value.mPoints.size()
                    > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
                setError("Telegram property oracle shape storage exceeds limits");
                return result;
            }
            const auto first = static_cast<std::uint32_t>(oracleShapePoints_.size());
            core::Fnv1a64 hash;
            hash.appendU8(value.mClosed ? 1U : 0U);
            hash.appendU8(0U);
            hash.appendU32(static_cast<std::uint32_t>(value.mPoints.size()));
            for (const auto point : value.mPoints) {
                const auto converted = toVec2(point);
                oracleShapePoints_.push_back(converted);
                hash.appendFloat(converted.x == 0.0F ? 0.0F : converted.x);
                hash.appendFloat(converted.y == 0.0F ? 0.0F : converted.y);
            }
            const auto slot = static_cast<std::uint32_t>(oracleShapes_.size());
            oracleShapes_.push_back({
                .property = property,
                .firstPoint = first,
                .pointCount = static_cast<std::uint32_t>(value.mPoints.size()),
                .pointCapacity = static_cast<std::uint32_t>(value.mPoints.size()),
                .closed = value.mClosed,
                .topologyTruncated = false,
                .contentHash = hash.value(),
                .revision = 1U,
                .changed = true,
            });
            result.type = PropertyValueType::Shape;
            result.storage = evaluation::PropertyStorageKind::Materialized;
            result.shapeSlot = slot;
        } else {
            result.type = valueType<T>();
            result.storage = evaluation::PropertyStorageKind::Unsupported;
        }
        return result;
    }

    template <typename T>
    void appendOracleAnimated(PropertyId id, const LOTAnimatable<T>& value) {
        if (!oracleFrame_) return;
        if (id.index() != oracleProperties_.size()) {
            setError("Telegram animated property oracle order differs from canonical IDs");
            return;
        }
        oracleProperties_.push_back(oracleMaterialized(
            id,
            value.isStatic() ? value.value() : value.animation().value(*oracleFrame_)));
    }

    void appendOracleTransform(
        SourceNodeId owner,
        const LOTTransformData* transform,
        bool autoOrient) {
        if (!oracleFrame_ || transform == nullptr || !owner.valid()
            || owner.index() >= oracleTransforms_.size()) return;
        evaluation::EvaluatedNodeTransform result;
        result.node = owner;
        const auto matrix = transform->matrix(*oracleFrame_, autoOrient);
        oracleLocalMatrices_[owner.index()] = matrix;
        oracleLocalMatrixSupported_[owner.index()] = matrix.isAffine();
        if (matrix.isAffine()) {
            result.localMatrix = toMatrix(matrix);
            result.localOpacity = transform->opacity(*oracleFrame_);
            result.state = transform->isStatic()
                ? evaluation::NodeTransformState::StaticMatrix
                : evaluation::NodeTransformState::Evaluated2D;
        } else {
            result.state = evaluation::NodeTransformState::Unsupported;
        }
        oracleTransforms_[owner.index()] = result;
    }

    [[nodiscard]] bool finalizeOracleWorldTransforms() {
        if (!oracleFrame_) return true;
        const std::size_t count = model_->sourceNodes.size();
        if (oracleTransforms_.size() != count
            || oracleLocalMatrices_.size() != count
            || oracleLocalMatrixSupported_.size() != count) {
            setError("Telegram transform oracle storage differs from source graph");
            return false;
        }

        std::vector<std::vector<std::uint32_t>> dependents(count);
        std::vector<std::uint32_t> indegree(count, 0U);
        const auto addDependency = [&](std::size_t nodeIndex,
                                       SourceNodeId dependency) -> bool {
            if (!dependency.valid()) return true;
            if (dependency.index() >= count || dependency.index() == nodeIndex) {
                setError("Telegram transform oracle found an invalid hierarchy edge");
                return false;
            }
            const auto& node = model_->sourceNodes[nodeIndex];
            const auto& parent = model_->sourceNodes[dependency.index()];
            if (!parent.present || parent.composition != node.composition) {
                setError("Telegram transform oracle found a cross-composition edge");
                return false;
            }
            auto& list = dependents[dependency.index()];
            const auto child = static_cast<std::uint32_t>(nodeIndex);
            if (std::find(list.begin(), list.end(), child) == list.end()) {
                list.push_back(child);
                ++indegree[nodeIndex];
            }
            return true;
        };

        for (std::size_t index = 0; index < count; ++index) {
            const auto& node = model_->sourceNodes[index];
            if (!addDependency(index, node.parent)) return false;
            if (node.transformParent.valid()
                && node.transformParent != node.parent
                && !addDependency(index, node.transformParent)) {
                return false;
            }
        }

        struct GreaterIndex final {
            bool operator()(std::uint32_t left, std::uint32_t right) const noexcept {
                return left > right;
            }
        };
        std::priority_queue<
            std::uint32_t,
            std::vector<std::uint32_t>,
            GreaterIndex> ready;
        for (std::uint32_t index = 0; index < indegree.size(); ++index) {
            if (indegree[index] == 0U) ready.push(index);
        }

        std::vector<VMatrix> worldMatrices(count);
        std::size_t visited = 0U;
        while (!ready.empty()) {
            const auto index = ready.top();
            ready.pop();
            ++visited;
            const auto& node = model_->sourceNodes[index];
            auto& output = oracleTransforms_[index];
            const auto matrixParent = node.transformParent.valid()
                ? node.transformParent : node.parent;
            const auto opacityParent = node.parent;

            bool supported = oracleLocalMatrixSupported_[index]
                && output.state != evaluation::NodeTransformState::Unsupported;
            VMatrix world;
            auto matrixParentState = evaluation::NodeTransformState::NotPresent;
            auto opacityParentState = evaluation::NodeTransformState::NotPresent;
            if (matrixParent.valid()) {
                const auto& parent = oracleTransforms_[matrixParent.index()];
                supported = supported && parent.worldSupported();
                matrixParentState = parent.worldState;
            }
            if (opacityParent.valid()) {
                const auto& parent = oracleTransforms_[opacityParent.index()];
                supported = supported && parent.worldSupported();
                opacityParentState = parent.worldState;
            }

            if (supported) {
                world = oracleLocalMatrices_[index];
                if (matrixParent.valid()) {
                    world *= worldMatrices[matrixParent.index()];
                }
                worldMatrices[index] = world;
                output.worldMatrix = toMatrix(world);
                output.worldOpacity = output.localOpacity
                    * (opacityParent.valid()
                        ? oracleTransforms_[opacityParent.index()].worldOpacity
                        : 1.0F);
                const bool locallyStatic =
                    output.state == evaluation::NodeTransformState::NotPresent
                    || output.state == evaluation::NodeTransformState::StaticMatrix;
                const bool matrixStatic =
                    matrixParentState == evaluation::NodeTransformState::NotPresent
                    || matrixParentState == evaluation::NodeTransformState::StaticMatrix;
                const bool opacityStatic =
                    opacityParentState == evaluation::NodeTransformState::NotPresent
                    || opacityParentState == evaluation::NodeTransformState::StaticMatrix;
                output.worldState = locallyStatic && matrixStatic && opacityStatic
                    ? evaluation::NodeTransformState::StaticMatrix
                    : evaluation::NodeTransformState::Evaluated2D;
            } else {
                output.worldState = evaluation::NodeTransformState::Unsupported;
            }

            for (const auto child : dependents[index]) {
                if (--indegree[child] == 0U) ready.push(child);
            }
        }
        if (visited != count) {
            setError("Telegram transform oracle found a hierarchy cycle");
            return false;
        }
        return true;
    }

    void populateStatistics() noexcept {
        auto& stats = model_->statistics;
        stats.compositionCount = model_->compositions.size();
        stats.sourceNodeCount = model_->sourceNodes.size();
        stats.propertyCount = model_->properties.size();
        stats.staticPropertyCount = static_cast<std::size_t>(std::count_if(
            model_->properties.begin(), model_->properties.end(),
            [](const auto& value) {
                return (value.flags & PropertyFlagStatic) != 0U;
            }));
        stats.animatedPropertyCount = stats.propertyCount - stats.staticPropertyCount;
        stats.trackCount = model_->tracks.size();
        stats.segmentCount = model_->segments.size();
        stats.scalarValueCount = model_->scalarValues.size();
        stats.vec2ValueCount = model_->vec2Values.size();
        stats.colorValueCount = model_->colorValues.size();
        stats.matrixValueCount = model_->matrixValues.size();
        stats.shapeValueCount = model_->shapeValues.size();
        stats.gradientValueCount = model_->gradientValues.size();
        stats.clipCount = model_->clips.size();
    }

    [[nodiscard]] bool validRange(IndexRange range, std::size_t size) const noexcept {
        return static_cast<std::size_t>(range.first) <= size
            && static_cast<std::size_t>(range.count)
                <= size - static_cast<std::size_t>(range.first);
    }

    [[nodiscard]] bool validValue(MotionValueRef value) const noexcept {
        if (!value.valid()) return false;
        const auto index = static_cast<std::size_t>(value.index);
        switch (value.type) {
        case PropertyValueType::Scalar: return index < model_->scalarValues.size();
        case PropertyValueType::Vec2: return index < model_->vec2Values.size();
        case PropertyValueType::Color: return index < model_->colorValues.size();
        case PropertyValueType::Matrix3x2: return index < model_->matrixValues.size();
        case PropertyValueType::Shape: return index < model_->shapeValues.size();
        case PropertyValueType::Gradient: return index < model_->gradientValues.size();
        case PropertyValueType::None: return false;
        }
        return false;
    }

    [[nodiscard]] bool validateModel() {
        for (std::size_t index = 0; index < model_->compositions.size(); ++index) {
            const auto& value = model_->compositions[index];
            if (!value.present || value.id != makeId<CompositionId>(index)
                || !value.rootNode.valid()
                || value.rootNode.index() >= model_->sourceNodes.size()
                || !std::isfinite(value.firstFrame)
                || !std::isfinite(value.endFrame)
                || !std::isfinite(value.frameRate)
                || value.endFrame < value.firstFrame
                || value.frameRate <= 0.0) {
                setError("parsed composition table failed validation");
                return false;
            }
            const auto& root = model_->sourceNodes[value.rootNode.index()];
            if (root.kind != SourceNodeKind::Composition
                || root.composition != value.id
                || root.parent.valid()) {
                setError("parsed composition root is inconsistent");
                return false;
            }
        }
        for (std::size_t index = 0; index < model_->sourceNodes.size(); ++index) {
            const auto& value = model_->sourceNodes[index];
            if (!value.present || value.id != makeId<SourceNodeId>(index)
                || !value.composition.valid()
                || value.composition.index() >= model_->compositions.size()
                || !validRange(value.children, model_->sourceChildIds.size())
                || !validRange(value.properties, model_->sourcePropertyIds.size())
                || !std::isfinite(value.inFrame)
                || !std::isfinite(value.outFrame)
                || !std::isfinite(value.startFrame)
                || !finite(value.timeStretch)
                || !finite(value.miterLimit)
                || !finite(value.repeaterMaximumCopies)
                || !finite(value.solidColor.r)
                || !finite(value.solidColor.g)
                || !finite(value.solidColor.b)
                || !finite(value.solidColor.a)
                || value.layerWidth < 0
                || value.layerHeight < 0
                || value.gradientColorPointCount < 0) {
                setError("parsed source-node table failed validation");
                return false;
            }
            if (value.parent.valid()) {
                if (value.parent.index() >= model_->sourceNodes.size()
                    || model_->sourceNodes[value.parent.index()].composition
                        != value.composition) {
                    setError("parsed source node has an invalid structural parent");
                    return false;
                }
            }
            if (value.transformParent.valid()) {
                if (value.transformParent.index() >= model_->sourceNodes.size()) {
                    setError("parsed source node has an invalid transform parent");
                    return false;
                }
                const auto& parent = model_->sourceNodes[value.transformParent.index()];
                if (parent.kind != SourceNodeKind::Layer
                    || parent.composition != value.composition) {
                    setError("parsed transform parent crosses composition boundaries");
                    return false;
                }
            }
            if (value.layerKind == SourceLayerKind::Precomposition) {
                if (!value.referencedComposition.valid()
                    || value.referencedComposition.index()
                        >= model_->compositions.size()) {
                    setError("parsed precomposition layer has an invalid reference");
                    return false;
                }
            } else if (value.referencedComposition.valid()) {
                setError("non-precomposition source node references a composition");
                return false;
            }
            for (std::uint32_t offset = 0; offset < value.children.count; ++offset) {
                const auto child = model_->sourceChildIds[value.children.first + offset];
                if (!child.valid() || child.index() >= model_->sourceNodes.size()
                    || model_->sourceNodes[child.index()].parent != value.id
                    || model_->sourceNodes[child.index()].composition
                        != value.composition) {
                    setError("parsed source child range is inconsistent");
                    return false;
                }
            }
            for (std::uint32_t offset = 0; offset < value.properties.count; ++offset) {
                const auto property =
                    model_->sourcePropertyIds[value.properties.first + offset];
                if (!property.valid() || property.index() >= model_->properties.size()
                    || model_->properties[property.index()].owner != value.id) {
                    setError("parsed source property range is inconsistent");
                    return false;
                }
            }
        }
        std::vector<std::vector<CompositionId>> compositionEdges(
            model_->compositions.size());
        for (const auto& node : model_->sourceNodes) {
            if (node.referencedComposition.valid()) {
                compositionEdges[node.composition.index()].push_back(
                    node.referencedComposition);
            }
        }
        std::vector<std::uint8_t> compositionVisit(
            model_->compositions.size(), 0U);
        const auto visitComposition = [&](auto&& self, CompositionId id) -> bool {
            auto& state = compositionVisit[id.index()];
            if (state == 1U) return false;
            if (state == 2U) return true;
            state = 1U;
            for (const auto next : compositionEdges[id.index()]) {
                if (!self(self, next)) return false;
            }
            state = 2U;
            return true;
        };
        for (std::size_t index = 0; index < model_->compositions.size(); ++index) {
            if (!visitComposition(
                    visitComposition, makeId<CompositionId>(index))) {
                setError("parsed precomposition graph contains a cycle");
                return false;
            }
        }

        for (std::size_t index = 0; index < model_->properties.size(); ++index) {
            const auto& value = model_->properties[index];
            const bool isStatic = (value.flags & PropertyFlagStatic) != 0U;
            const bool isAnimated = (value.flags & PropertyFlagAnimated) != 0U;
            if (!value.present || value.id != makeId<PropertyId>(index)
                || !value.owner.valid() || value.owner.index() >= model_->sourceNodes.size()
                || value.valueType == PropertyValueType::None
                || isStatic == isAnimated) {
                setError("parsed property table failed validation");
                return false;
            }
            if (isStatic) {
                if (!validValue(value.staticValue)
                    || value.staticValue.type != value.valueType
                    || value.track.valid()) {
                    setError("parsed static property has invalid value storage");
                    return false;
                }
            } else if (!value.track.valid()
                       || value.track.index() >= model_->tracks.size()) {
                setError("parsed animated property has invalid track");
                return false;
            }
        }
        for (std::size_t index = 0; index < model_->tracks.size(); ++index) {
            const auto& value = model_->tracks[index];
            if (!value.present || value.id != makeId<TrackId>(index)
                || !value.property.valid()
                || value.property.index() >= model_->properties.size()
                || model_->properties[value.property.index()].track != value.id
                || !validRange(value.segments, model_->segments.size())
                || value.segments.empty()
                || !std::isfinite(value.firstFrame)
                || !std::isfinite(value.endFrame)
                || value.endFrame < value.firstFrame) {
                setError("parsed track table failed validation");
                return false;
            }
            double previousEnd = -std::numeric_limits<double>::infinity();
            for (std::uint32_t offset = 0; offset < value.segments.count; ++offset) {
                const auto& segment = model_->segments[value.segments.first + offset];
                if (segment.track != value.id
                    || segment.firstFrame < previousEnd
                    || segment.endFrame < segment.firstFrame) {
                    setError("parsed track segments are not monotonic");
                    return false;
                }
                previousEnd = segment.endFrame;
            }
        }
        for (std::size_t index = 0; index < model_->segments.size(); ++index) {
            const auto& value = model_->segments[index];
            if (!value.present || value.id != makeId<SegmentId>(index)
                || !value.track.valid() || value.track.index() >= model_->tracks.size()
                || !std::isfinite(value.firstFrame)
                || !std::isfinite(value.endFrame)
                || !validValue(value.startValue)
                || !validValue(value.endValue)
                || value.startValue.type != value.endValue.type) {
                setError("parsed segment table failed validation");
                return false;
            }
        }
        for (const auto& value : model_->shapeValues) {
            if (!validRange(value.points, model_->shapePoints.size())) {
                setError("parsed shape value range is invalid");
                return false;
            }
        }
        for (const auto& value : model_->gradientValues) {
            if (!validRange(value.values, model_->gradientFloats.size())) {
                setError("parsed gradient value range is invalid");
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] std::uint64_t computeFingerprint() const noexcept {
        core::Fnv1a64 hash;
        hash.appendU32(MotionAssetModel::kSchemaVersion);
        hash.appendU64(model_->sourceAssetHash);
        hash.appendU64(model_->compositions.size());
        hash.appendU64(model_->sourceNodes.size());
        hash.appendU64(model_->properties.size());
        hash.appendU64(model_->tracks.size());
        hash.appendU64(model_->segments.size());
        for (const auto& composition : model_->compositions) {
            hash.appendU32(composition.id.value);
            hash.appendU32(composition.rootNode.value);
            hash.appendU64(composition.logicalWidth);
            hash.appendU64(composition.logicalHeight);
            hash.appendDouble(composition.firstFrame);
            hash.appendDouble(composition.endFrame);
            hash.appendDouble(composition.frameRate);
        }
        for (const auto& node : model_->sourceNodes) {
            hash.appendU32(node.id.value);
            hash.appendU32(node.parent.value);
            hash.appendU32(node.transformParent.value);
            hash.appendU32(node.composition.value);
            hash.appendU32(node.referencedComposition.value);
            hash.appendU8(static_cast<std::uint8_t>(node.kind));
            hash.appendU8(static_cast<std::uint8_t>(node.layerKind));
            hash.appendU32(node.children.first);
            hash.appendU32(node.children.count);
            hash.appendU32(node.properties.first);
            hash.appendU32(node.properties.count);
            hash.appendU64(node.nameHash);
            hash.appendU8(node.hidden ? 1U : 0U);
            hash.appendU8(node.authoredStatic ? 1U : 0U);
            hash.appendU8(node.autoOrient ? 1U : 0U);
            hash.appendI32(node.authoredLayerId);
            hash.appendI32(node.authoredParentLayerId);
            hash.appendDouble(node.inFrame);
            hash.appendDouble(node.outFrame);
            hash.appendDouble(node.startFrame);
            hash.appendFloat(node.timeStretch);
            hash.appendU32(node.dependencyBits);
            hash.appendU8(static_cast<std::uint8_t>(node.fillRule));
            hash.appendU8(static_cast<std::uint8_t>(node.strokeCap));
            hash.appendU8(static_cast<std::uint8_t>(node.strokeJoin));
            hash.appendU8(static_cast<std::uint8_t>(node.gradientType));
            hash.appendU8(static_cast<std::uint8_t>(node.maskMode));
            hash.appendU8(static_cast<std::uint8_t>(node.matteMode));
            hash.appendU8(static_cast<std::uint8_t>(node.blendMode));
            hash.appendU8(static_cast<std::uint8_t>(node.pathDirection));
            hash.appendU8(static_cast<std::uint8_t>(node.polystarType));
            hash.appendU8(static_cast<std::uint8_t>(node.trimMode));
            hash.appendFloat(node.miterLimit);
            hash.appendFloat(node.repeaterMaximumCopies);
            hash.appendI32(node.gradientColorPointCount);
            hash.appendI32(node.layerWidth);
            hash.appendI32(node.layerHeight);
            hash.appendFloat(node.solidColor.r);
            hash.appendFloat(node.solidColor.g);
            hash.appendFloat(node.solidColor.b);
            hash.appendFloat(node.solidColor.a);
            hash.appendU64(node.sourceAssetRefHash);
            hash.appendU8(node.enabled ? 1U : 0U);
            hash.appendU8(node.maskInverted ? 1U : 0U);
        }
        for (const auto id : model_->sourceChildIds) hash.appendU32(id.value);
        for (const auto id : model_->sourcePropertyIds) hash.appendU32(id.value);
        for (const auto& clip : model_->clips) {
            hash.appendU32(clip.id.value);
            hash.appendDouble(clip.firstFrame);
            hash.appendDouble(clip.endFrame);
            hash.appendU8(static_cast<std::uint8_t>(clip.defaultLoop));
        }
        for (const auto& property : model_->properties) {
            hash.appendU32(property.id.value);
            hash.appendU32(property.owner.value);
            hash.appendU32(static_cast<std::uint32_t>(property.semantic));
            hash.appendU32(property.semanticIndex);
            hash.appendU8(static_cast<std::uint8_t>(property.valueType));
            hash.appendU32(property.flags);
            hash.appendU8(static_cast<std::uint8_t>(property.staticValue.type));
            hash.appendU32(property.staticValue.index);
            hash.appendU32(property.track.value);
        }
        for (const auto& track : model_->tracks) {
            hash.appendU32(track.id.value);
            hash.appendU32(track.property.value);
            hash.appendU32(track.segments.first);
            hash.appendU32(track.segments.count);
            hash.appendDouble(track.firstFrame);
            hash.appendDouble(track.endFrame);
        }
        for (const auto& segment : model_->segments) {
            hash.appendU32(segment.id.value);
            hash.appendU32(segment.track.value);
            hash.appendDouble(segment.firstFrame);
            hash.appendDouble(segment.endFrame);
            hash.appendU8(static_cast<std::uint8_t>(segment.interpolation));
            hash.appendU8(static_cast<std::uint8_t>(segment.spatialInterpolation));
            hash.appendU8(static_cast<std::uint8_t>(segment.startValue.type));
            hash.appendU32(segment.startValue.index);
            hash.appendU8(static_cast<std::uint8_t>(segment.endValue.type));
            hash.appendU32(segment.endValue.index);
            hash.appendFloat(segment.temporalControl1.x);
            hash.appendFloat(segment.temporalControl1.y);
            hash.appendFloat(segment.temporalControl2.x);
            hash.appendFloat(segment.temporalControl2.y);
            hash.appendFloat(segment.spatialInTangent.x);
            hash.appendFloat(segment.spatialInTangent.y);
            hash.appendFloat(segment.spatialOutTangent.x);
            hash.appendFloat(segment.spatialOutTangent.y);
        }
        for (const auto value : model_->scalarValues) hash.appendFloat(value);
        for (const auto value : model_->vec2Values) {
            hash.appendFloat(value.x);
            hash.appendFloat(value.y);
        }
        for (const auto value : model_->colorValues) {
            hash.appendFloat(value.r);
            hash.appendFloat(value.g);
            hash.appendFloat(value.b);
            hash.appendFloat(value.a);
        }
        for (const auto value : model_->matrixValues) {
            hash.appendFloat(value.m11);
            hash.appendFloat(value.m12);
            hash.appendFloat(value.m21);
            hash.appendFloat(value.m22);
            hash.appendFloat(value.dx);
            hash.appendFloat(value.dy);
        }
        for (const auto& value : model_->shapeValues) {
            hash.appendU32(value.points.first);
            hash.appendU32(value.points.count);
            hash.appendU8(value.closed ? 1U : 0U);
        }
        for (const auto value : model_->shapePoints) {
            hash.appendFloat(value.x);
            hash.appendFloat(value.y);
        }
        for (const auto& value : model_->gradientValues) {
            hash.appendU32(value.values.first);
            hash.appendU32(value.values.count);
        }
        for (const auto value : model_->gradientFloats) hash.appendFloat(value);
        return hash.value();
    }

    void setError(std::string message) {
        if (error_.empty()) error_ = std::move(message);
    }

    const LOTModel& source_;
    std::shared_ptr<MotionAssetModel> model_;
    std::unordered_set<const LOTData*> activeData_;
    std::unordered_map<std::string, CompositionId> compositionByRefId_;
    std::unordered_map<std::string, std::pair<int, int>> compositionSizeHints_;
    std::vector<const LOTAsset*> precompositionAssets_;
    std::unordered_map<std::uint64_t, SourceNodeId> layerByScopedId_;
    std::vector<PendingTransformParent> pendingTransformParents_;
    std::optional<int> oracleFrame_;
    std::vector<evaluation::MotionPropertyValue> oracleProperties_;
    std::vector<evaluation::EvaluatedNodeTransform> oracleTransforms_;
    std::vector<evaluation::EvaluatedShape> oracleShapes_;
    std::vector<MotionVec2Value> oracleShapePoints_;
    std::vector<VMatrix> oracleLocalMatrices_;
    std::vector<bool> oracleLocalMatrixSupported_;
    std::string error_;
};

// Destruction publishes even partial stamping on failure or exception unwinding.
// Declare after the lock so publication always precedes unlocking the source.
class BindingPublication final {
public:
    explicit BindingPublication(LOTModel& source) : source_(source) {}
    ~BindingPublication() { source_.mAveMotionBindingEpoch.fetch_add(1, std::memory_order_release); }
    BindingPublication(const BindingPublication&) = delete;
    BindingPublication& operator=(const BindingPublication&) = delete;
private:
    LOTModel& source_;
};

ParsedModelBuildResult extractLocked(
    const std::shared_ptr<LOTModel>& source, Extractor& extractor) {
    std::lock_guard lock(source->mAveMotionBindingMutex);
    BindingPublication publication(*source);
    return extractor.build();
}

} // namespace

ParsedModelBuildResult buildTelegramParsedModel(
    const std::shared_ptr<LOTModel>& source,
    const AssetModelDescriptor& descriptor) {
    if (!source) return {nullptr, "Telegram loader returned a null LOTModel"};
    Extractor extractor{*source, descriptor};
    return extractLocked(source, extractor);
}

ParsedModelBuildResult buildTelegramParsedModel(
    std::string_view json,
    std::string_view cacheKey,
    const AssetModelDescriptor& descriptor) {
    if (json.empty()) return {nullptr, "parsed-model source JSON is empty"};

    LottieLoader loader;
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> replacements;
    if (!loader.loadFromData(
            std::string{json},
            std::string{cacheKey},
            " ",
            true,
            replacements,
            rlottie::FitzModifier::None)) {
        return {nullptr, "Telegram loader rejected parsed-model source"};
    }
    const auto model = loader.model();
    if (!model) return {nullptr, "Telegram loader returned a null LOTModel"};
    return buildTelegramParsedModel(model, descriptor);
}

TelegramPropertyOracleResult evaluateTelegramParsedProperties(
    std::string_view json,
    std::string_view cacheKey,
    const AssetModelDescriptor& descriptor,
    int frame) {
    if (json.empty()) {
        return {nullptr, {}, {}, {}, {}, "property-oracle source JSON is empty"};
    }
    LottieLoader loader;
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> replacements;
    if (!loader.loadFromData(
            std::string{json},
            std::string{cacheKey},
            " ",
            true,
            replacements,
            rlottie::FitzModifier::None)) {
        return {nullptr, {}, {}, {}, {}, "Telegram loader rejected property-oracle source"};
    }
    const auto source = loader.model();
    if (!source) {
        return {nullptr, {}, {}, {}, {}, "Telegram property oracle returned a null LOTModel"};
    }
    Extractor extractor{*source, descriptor, frame};
    auto built = extractLocked(source, extractor);
    if (!built) return {nullptr, {}, {}, {}, {}, std::move(built.error)};
    auto properties = extractor.takeOracleProperties();
    auto transforms = extractor.takeOracleTransforms();
    auto shapes = extractor.takeOracleShapes();
    auto shapePoints = extractor.takeOracleShapePoints();
    if (properties.size() != built.model->properties.size()
        || transforms.size() != built.model->sourceNodes.size()) {
        return {nullptr, {}, {}, {}, {}, "Telegram property oracle size differs from canonical model"};
    }
    return {
        std::move(built.model),
        std::move(properties),
        std::move(transforms),
        std::move(shapes),
        std::move(shapePoints),
        {},
    };
}

} // namespace avemotion::model::detail
