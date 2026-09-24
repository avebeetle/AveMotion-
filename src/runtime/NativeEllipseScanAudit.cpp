#include "NativeEllipseCertificate.hpp"

#include <algorithm>
#include <limits>
#include <variant>

namespace avemotion::runtime::detail {
namespace {

template <typename T>
bool inRange(const model::IndexRange range, const std::vector<T>& table) {
    return range.first <= table.size() && range.count <= table.size() - range.first;
}

bool sameColor(const Color8& a, const Color8& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

bool zero(const Vec2& point) { return point.x == 0.0F && point.y == 0.0F; }

bool defaultInactivePaint(const EvaluatedPaint& paint) {
    const auto& gradient = paint.gradient;
    const auto& image = paint.image;
    return gradient.kind == GradientKind::Linear && zero(gradient.start)
        && zero(gradient.end) && zero(gradient.center) && zero(gradient.focal)
        && gradient.centerRadius == 0.0F && gradient.focalRadius == 0.0F
        && gradient.stops.empty() && !image.present && image.width == 0
        && image.height == 0 && std::all_of(std::begin(image.matrix),
            std::end(image.matrix), [](float value) { return value == 0.0F; });
}

bool noClip(const EvaluatedPath& path) {
    return path.verbs.empty() && path.points.empty() && !path.controlBounds.valid
        && path.hash == 0;
}

bool layerMatches(const EvaluatedLayer& layer, const NativeEllipseLayerSlot& slot) {
    return layer.modelLayer == slot.id && layer.parentLayer == slot.parentIndex
        && layer.keyPath == slot.keyPath
        && layer.firstChildReference == slot.firstChildReference
        && layer.childCount == slot.childCount
        && layer.firstDrawItem == slot.firstDrawItem
        && layer.firstMask == 0 && layer.maskCount == 0
        && layer.opacity == 1.0F && layer.matte == MatteMode::None
        && noClip(layer.clipPath);
}

NativeEllipseLayerSlot captureLayer(const EvaluatedLayer& layer) {
    return {layer.modelLayer, layer.parentLayer, layer.keyPath,
            layer.firstChildReference, layer.childCount, layer.firstDrawItem};
}

NativeEllipseDrawSlot captureDraw(const EvaluatedDrawItem& item) {
    NativeEllipseDrawSlot result;
    result.draw = item.modelDrawItem;
    result.node = item.modelNode;
    result.geometry = item.modelGeometry;
    result.paint = item.modelPaint;
    result.layerIndex = item.layerIndex;
    result.drawOrder = item.drawOrder;
    result.sourcePath = item.sourcePathNode;
    result.sourcePaint = item.sourcePaintNode;
    result.sourcePathCount = item.sourcePathCount;
    result.sourcePathModifierFree = item.sourcePathModifierFree;
    result.localGeometryAvailable = item.localGeometryAvailable;
    result.localGeometryStaticCandidate = item.localGeometryStaticCandidate;
    result.localPaintAvailable = item.localPaintAvailable;
    result.localPaintStaticCandidate = item.localPaintStaticCandidate;
    result.fillRule = item.fillRule;
    result.solid = item.paint.solid;
    result.localSolid = item.localPaint.solid;
    result.strokeWidth = item.stroke.width;
    result.strokeMiterLimit = item.stroke.miterLimit;
    result.strokeCap = item.stroke.cap;
    result.strokeJoin = item.stroke.join;
    result.localStrokeWidth = item.localStroke.width;
    result.localStrokeMiterLimit = item.localStroke.miterLimit;
    result.localStrokeCap = item.localStroke.cap;
    result.localStrokeJoin = item.localStroke.join;
    result.opacitySeparated = item.opacitySeparated;
    result.separatedOpacity = item.separatedOpacity;
    return result;
}

bool drawMatches(const EvaluatedDrawItem& item, const NativeEllipseDrawSlot& slot) {
    return item.modelDrawItem == slot.draw && item.modelNode == slot.node
        && item.modelGeometry == slot.geometry && item.modelPaint == slot.paint
        && item.layerIndex == slot.layerIndex && item.drawOrder == slot.drawOrder
        && item.sourcePathNode == slot.sourcePath
        && item.sourcePaintNode == slot.sourcePaint
        && item.sourcePathCount == slot.sourcePathCount
        && item.sourcePathModifierFree == slot.sourcePathModifierFree
        && item.localGeometryAvailable == slot.localGeometryAvailable
        && item.localGeometryStaticCandidate == slot.localGeometryStaticCandidate
        && item.localPaintAvailable == slot.localPaintAvailable
        && item.localPaintStaticCandidate == slot.localPaintStaticCandidate
        && item.fillRule == slot.fillRule && sameColor(item.paint.solid, slot.solid)
        && sameColor(item.localPaint.solid, slot.localSolid)
        && item.stroke.width == slot.strokeWidth
        && item.stroke.miterLimit == slot.strokeMiterLimit
        && item.stroke.cap == slot.strokeCap && item.stroke.join == slot.strokeJoin
        && item.localStroke.width == slot.localStrokeWidth
        && item.localStroke.miterLimit == slot.localStrokeMiterLimit
        && item.localStroke.cap == slot.localStrokeCap
        && item.localStroke.join == slot.localStrokeJoin
        && item.opacitySeparated == slot.opacitySeparated
        && item.separatedOpacity == slot.separatedOpacity;
}

bool validRowCounts(const model::MotionAssetModel& model) {
    return model.layers.size() == model.statistics.declaredLayerCount
        && model.nodes.size() == model.statistics.declaredNodeCount
        && model.geometries.size() == model.statistics.declaredGeometryCount
        && model.paints.size() == model.statistics.declaredPaintCount
        && model.layers.size() <= std::numeric_limits<std::uint32_t>::max()
        && model.nodes.size() <= std::numeric_limits<std::uint32_t>::max()
        && model.geometries.size() <= std::numeric_limits<std::uint32_t>::max()
        && model.paints.size() <= std::numeric_limits<std::uint32_t>::max();
}

bool finalRowsAgree(const model::MotionAssetModel& model,
                    const NativeEllipseSlotMetadata& slot,
                    bool animatedPosition) {
    if (std::count_if(model.layers.begin(), model.layers.end(),
            [](const auto& row) { return row.present; }) != 2
        || std::count_if(model.nodes.begin(), model.nodes.end(),
            [](const auto& row) { return row.present; }) != 1
        || std::count_if(model.geometries.begin(), model.geometries.end(),
            [](const auto& row) { return row.present; }) != 1
        || std::count_if(model.paints.begin(), model.paints.end(),
            [](const auto& row) { return row.present; }) != 1
        || model.drawOrder.size() != 1 || model.clips.size() != 1
        || model.childLayerIds.size() != 1 || model.childLayerIds[0] != slot.shape.id
        || model.layerNodeIds.size() != 1 || model.layerNodeIds[0] != slot.draw.node)
        return false;
    const auto& clip = model.clips.front();
    if (!clip.present || clip.id != model::ClipId{0U}
        || clip.debugName != "default"
        || clip.firstFrame != 0.0 || clip.endFrame != static_cast<double>(slot.totalFrames)
        || clip.defaultLoop != model::ClipLoopHint::Loop)
        return false;
    const auto* root = model.layer(slot.root.id);
    const auto* shape = model.layer(slot.shape.id);
    const auto* node = model.node(slot.draw.node);
    const auto* geometry = model.geometry(slot.draw.geometry);
    const auto* paint = model.paint(slot.draw.paint);
    if (!root || !shape || !node || !geometry || !paint
        || root->parent.valid() || shape->parent != slot.root.id
        || root->matte != MatteMode::None || shape->matte != MatteMode::None
        || root->debugName != slot.root.keyPath
        || shape->debugName != slot.shape.keyPath
        || !inRange(root->children, model.childLayerIds)
        || root->children.count != 1
        || model.childLayerIds[root->children.first] != slot.shape.id
        || !inRange(shape->children, model.childLayerIds)
        || shape->children.count != 0
        || !inRange(root->nodes, model.layerNodeIds) || root->nodes.count != 0
        || !inRange(shape->nodes, model.layerNodeIds) || shape->nodes.count != 1
        || model.layerNodeIds[shape->nodes.first] != slot.draw.node
        || root->masks.first != 0 || root->masks.count != 0
        || shape->masks.first != 0 || shape->masks.count != 0
        || node->id != slot.draw.node || node->drawItem != slot.draw.draw
        || node->layer != slot.shape.id || node->geometry != slot.draw.geometry
        || node->paint != slot.draw.paint || node->drawOrder != slot.draw.drawOrder
        || model.drawOrder[0] != slot.draw.node
        || geometry->id != slot.draw.geometry || paint->id != slot.draw.paint)
        return false;
    if (geometry->resourceClass != (animatedPosition
            ? model::ResourceClass::InstanceEvaluated
            : model::ResourceClass::AssetStatic)
        || paint->resourceClass != model::ResourceClass::AssetStatic
        || !paint->staticValue || paint->staticValue->paint.kind != PaintKind::Solid
        || paint->contentHash != paint->staticValue->contentHash
        || !sameColor(paint->staticValue->paint.solid, slot.draw.localSolid)
        || paint->staticValue->stroke.enabled
        || paint->staticValue->stroke.width != slot.draw.localStrokeWidth
        || paint->staticValue->stroke.miterLimit != slot.draw.localStrokeMiterLimit
        || paint->staticValue->stroke.cap != slot.draw.localStrokeCap
        || paint->staticValue->stroke.join != slot.draw.localStrokeJoin
        || !paint->staticValue->stroke.dashArray.empty()
        || !defaultInactivePaint(paint->staticValue->paint)
        || paint->staticValue->sourceKey !=
            static_cast<std::uint64_t>(slot.draw.paint.value) + 1U)
        return false;
    if (animatedPosition) return !geometry->staticValue;
    return geometry->staticValue
        && geometry->staticValue->fillRule == slot.draw.fillRule
        && geometry->contentHash == geometry->staticValue->contentHash
        && geometry->staticValue->sourceKey ==
            static_cast<std::uint64_t>(slot.draw.geometry.value) + 1U;
}

} // namespace

NativeEllipseScanAudit::NativeEllipseScanAudit(
    const NativeEllipseInput& input, const NativeEllipseModelBinding& binding,
    const model::MotionAssetModel& frozenModel, AssetHandle expectedHandle,
    std::uint64_t expectedHash)
    : input_(input), binding_(binding), model_(frozenModel) {
    slot_.width = input.width;
    slot_.height = input.height;
    slot_.totalFrames = input.endFrame;
    slot_.activeFirstFrame = input.layerInFrame;
    slot_.activeEndFrame = input.layerOutFrame;
    slot_.frameRate = frozenModel.frameRate;
    slot_.assetHandle = expectedHandle;
    slot_.sourceHash = expectedHash;
    slot_.binding = binding;
    if (!expectedHandle.valid() || expectedHash == 0
        || frozenModel.assetHandle != expectedHandle
        || frozenModel.sourceAssetHash != expectedHash
        || frozenModel.logicalWidth != input.width
        || frozenModel.logicalHeight != input.height
        || frozenModel.totalFrames != input.endFrame
        || input.endFrame == 0 || input.layerInFrame >= input.layerOutFrame
        || input.layerOutFrame > input.endFrame
        || !validRowCounts(frozenModel)
        || !frozenModel.sourceNode(binding.root)
        || !frozenModel.sourceNode(binding.layer)
        || !frozenModel.sourceNode(binding.group)
        || !frozenModel.sourceNode(binding.ellipse)
        || !frozenModel.sourceNode(binding.fill)) {
        (void)fail(NativeEllipseScanCode::Identity);
    }
}

bool NativeEllipseScanAudit::fail(NativeEllipseScanCode code) noexcept {
    poisoned_ = true;
    code_ = code;
    return false;
}

bool NativeEllipseScanAudit::observe(std::size_t frame, const EvaluatedScene& scene) {
    if (poisoned_) return false;
    if (finished_ || frame != slot_.observedFrames || frame >= slot_.totalFrames)
        return fail(NativeEllipseScanCode::Timeline);
    if (scene.sourceAssetHash != slot_.sourceHash || scene.assetHandle != slot_.assetHandle
        || scene.assetModelApplied || scene.assetModel
        || scene.frameIndex != frame || scene.viewportWidth != slot_.width
        || scene.viewportHeight != slot_.height)
        return fail(NativeEllipseScanCode::Identity);
    if (scene.modelLayerCount != model_.layers.size()
        || scene.modelNodeCount != model_.nodes.size()
        || scene.modelGeometryCount != model_.geometries.size()
        || scene.modelPaintCount != model_.paints.size())
        return fail(NativeEllipseScanCode::ResourceBinding);
    if (scene.layers.size() != 2 || scene.childLayerIndices.size() != 1
        || scene.childLayerIndices[0] != 1 || !scene.masks.empty()
        || scene.statistics.layerCount != 2 || scene.statistics.maskCount != 0
        || scene.statistics.clipPathCount != 0)
        return fail(NativeEllipseScanCode::LayerLayout);
    const bool active = frame >= slot_.activeFirstFrame && frame < slot_.activeEndFrame;
    const auto& root = scene.layers[0];
    const auto& shape = scene.layers[1];
    if (!root.modelLayer.valid() || !shape.modelLayer.valid()
        || root.modelLayer == shape.modelLayer
        || root.parentLayer != kInvalidSceneIndex || shape.parentLayer != 0
        || root.firstChildReference != 0 || root.childCount != 1
        || shape.firstChildReference != 0 || shape.childCount != 0
        || root.firstDrawItem != 0 || shape.firstDrawItem != 0
        || root.drawItemCount != 0 || root.firstMask != 0 || shape.firstMask != 0
        || root.maskCount != 0 || shape.maskCount != 0
        || root.opacity != 1.0F || shape.opacity != 1.0F
        || root.matte != MatteMode::None || shape.matte != MatteMode::None
        || !noClip(root.clipPath) || !noClip(shape.clipPath)
        || !root.visible || shape.visible != active
        || scene.statistics.visibleLayerCount != (active ? 2U : 1U)
        || scene.statistics.solidPaintCount != (active ? 1U : 0U)
        || scene.statistics.gradientPaintCount != 0
        || scene.statistics.imagePaintCount != 0)
        return fail(NativeEllipseScanCode::LayerLayout);
    if (slot_.observedFrames == 0) {
        slot_.root = captureLayer(root);
        slot_.shape = captureLayer(shape);
        slot_.modelLayerCount = scene.modelLayerCount;
        slot_.modelNodeCount = scene.modelNodeCount;
        slot_.modelGeometryCount = scene.modelGeometryCount;
        slot_.modelPaintCount = scene.modelPaintCount;
    } else if (!layerMatches(root, slot_.root) || !layerMatches(shape, slot_.shape)) {
        return fail(NativeEllipseScanCode::LayerLayout);
    }
    if (scene.drawItems.size() != (active ? 1U : 0U)
        || shape.drawItemCount != (active ? 1U : 0U)
        || scene.statistics.drawItemCount != scene.drawItems.size())
        return fail(NativeEllipseScanCode::DrawMultiplicity);
    if (active) {
        const auto& item = scene.drawItems.front();
        if (item.sourcePathNode != binding_.ellipse
            || item.sourcePaintNode != binding_.fill
            || item.sourcePathCount != 1 || !item.sourcePathModifierFree)
            return fail(NativeEllipseScanCode::SourceBinding);
        if (!item.modelNode.valid() || !item.modelDrawItem.valid()
            || !item.modelGeometry.valid() || !item.modelPaint.valid()
            || item.layerIndex != 1 || item.drawOrder != 0
            || !model_.node(item.modelNode)
            || !model_.geometry(item.modelGeometry)
            || !model_.paint(item.modelPaint))
            return fail(NativeEllipseScanCode::ResourceBinding);
        const bool geometryStatic = std::holds_alternative<NativeEllipseStaticPosition>(
            input_.position);
        if (!item.localGeometryAvailable || !item.localPaintAvailable
            || item.localGeometryStaticCandidate != geometryStatic
            || !item.localPaintStaticCandidate
            || item.fillRule != FillRule::Winding
            || item.stroke.enabled || !item.stroke.dashArray.empty()
            || item.localStroke.enabled || !item.localStroke.dashArray.empty()
            || item.stroke.width != 0.0F || item.stroke.miterLimit != 0.0F
            || item.stroke.cap != LineCap::Flat || item.stroke.join != LineJoin::Miter
            || item.localStroke.width != 0.0F
            || item.localStroke.miterLimit != 0.0F
            || item.localStroke.cap != LineCap::Flat
            || item.localStroke.join != LineJoin::Miter
            || item.paint.kind != PaintKind::Solid
            || item.localPaint.kind != PaintKind::Solid
            || !defaultInactivePaint(item.paint)
            || !defaultInactivePaint(item.localPaint)
            || item.sourceRepeaterProjected || item.sourceRepeaterNode.valid()
            || item.sourceRepeaterCopyIndex != 0
            || item.sourceRepeaterVisibleCopies != 0
            || item.sourceRepeaterMaximumCopies != 0
            || item.sourceRepeaterTransformRevision != 0
            || item.sourceRepeaterOpacityRevision != 0
            || !item.opacitySeparated || item.separatedOpacity != 1.0F)
            return fail(NativeEllipseScanCode::UnsupportedSlot);
        if (!hasActive_) {
            slot_.draw = captureDraw(item);
            hasActive_ = true;
        } else if (!drawMatches(item, slot_.draw)) {
            return fail(NativeEllipseScanCode::ResourceBinding);
        }
        ++slot_.observedActiveFrames;
    }
    ++slot_.observedFrames;
    return true;
}

std::optional<NativeEllipseSlotMetadata> NativeEllipseScanAudit::finish() {
    if (poisoned_) return std::nullopt;
    if (finished_ || slot_.observedFrames != slot_.totalFrames || !hasActive_
        || slot_.observedActiveFrames != slot_.activeEndFrame - slot_.activeFirstFrame)
    {
        (void)fail(NativeEllipseScanCode::IncompleteScan);
        return std::nullopt;
    }
    if (!finalRowsAgree(model_, slot_,
            std::holds_alternative<NativeEllipseAnimatedPosition>(input_.position))) {
        (void)fail(NativeEllipseScanCode::ResourceBinding);
        return std::nullopt;
    }
    finished_ = true;
    code_ = NativeEllipseScanCode::Complete;
    return slot_;
}

} // namespace avemotion::runtime::detail
