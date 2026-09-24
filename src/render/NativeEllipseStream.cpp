#include "NativeEllipseStream.hpp"
#include "PrimitivePathGenerator.hpp"
#include "AssetModelBuilder.hpp"
#include "avemotion/core/Hash.hpp"
#include "avemotion/runtime/RecordingBackend.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace avemotion::render::detail {
namespace {

bool finite(const runtime::AffineTransform& value) noexcept {
    return std::isfinite(value.m11) && std::isfinite(value.m12)
        && std::isfinite(value.m21) && std::isfinite(value.m22)
        && std::isfinite(value.dx) && std::isfinite(value.dy);
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

bool materializePath(const PrimitivePath& primitive,
                     const runtime::AffineTransform* transform,
                     runtime::EvaluatedPath& path) {
    if (!primitive.valid) return false;
    path.verbs.assign(primitive.verbSpan().begin(), primitive.verbSpan().end());
    path.points.reserve(primitive.pointCount);
    for (const auto source : primitive.pointSpan()) {
        runtime::Vec2 point{source.x, source.y};
        if (transform) {
            point = {source.x * transform->m11 + source.y * transform->m21 + transform->dx,
                     source.x * transform->m12 + source.y * transform->m22 + transform->dy};
        }
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) return false;
        path.points.push_back(point);
        includePoint(path.controlBounds, point);
    }
    if (path.verbs.empty()) return true;
    core::Fnv1a64 hash;
    hash.appendU64(path.verbs.size());
    hash.appendU64(path.points.size());
    for (auto verb : path.verbs) hash.appendU8(static_cast<std::uint8_t>(verb));
    for (auto point : path.points) {
        hash.appendFloat(point.x);
        hash.appendFloat(point.y);
    }
    path.hash = hash.value();
    return true;
}

bool resolveVec2(const model::MotionAssetModel& model,
                 const evaluation::PropertyEvaluationView& view,
                 model::PropertyId id, model::MotionVec2Value& value) noexcept {
    const auto* property = model.property(id);
    if (!property || property->valueType != model::PropertyValueType::Vec2
        || id.index() >= view.properties.size()) return false;
    const auto& evaluated = view.properties[id.index()];
    if (evaluated.id != id || evaluated.value.type != model::PropertyValueType::Vec2)
        return false;
    if (evaluated.value.storage == evaluation::PropertyStorageKind::Materialized) {
        value = evaluated.value.vec2;
    } else if (evaluated.value.storage == evaluation::PropertyStorageKind::AssetReference) {
        const auto reference = evaluated.value.assetReference;
        if (reference.type != model::PropertyValueType::Vec2
            || reference.index >= model.vec2Values.size()) return false;
        value = model.vec2Values[reference.index];
    } else return false;
    return std::isfinite(value.x) && std::isfinite(value.y);
}

runtime::EvaluatedLayer makeLayer(const runtime::detail::NativeEllipseLayerSlot& slot,
                                  bool visible, std::uint32_t drawCount) {
    runtime::EvaluatedLayer layer;
    layer.modelLayer = slot.id;
    layer.parentLayer = slot.parentIndex;
    layer.firstChildReference = slot.firstChildReference;
    layer.childCount = slot.childCount;
    layer.firstDrawItem = slot.firstDrawItem;
    layer.drawItemCount = drawCount;
    layer.keyPath = slot.keyPath;
    layer.visible = visible;
    return layer;
}

} // namespace

NativeEllipseStream::NativeEllipseStream(
    std::shared_ptr<const runtime::detail::NativeEllipseCertificate> certificate,
    std::uint64_t instanceId)
    : certificate_(std::move(certificate)), instanceId_(instanceId),
      evaluator_(certificate_->model) {}

NativeEllipseStream::~NativeEllipseStream() = default;

NativeEllipseCreateResult NativeEllipseStream::create(
    std::shared_ptr<const runtime::detail::NativeEllipseCertificate> certificate,
    std::uint64_t instanceId) {
    if (!certificate || !certificate->input || !certificate->model
        || !certificate->matchesAsset(certificate->asset)
        || certificate->slot.totalFrames == 0
        || certificate->slot.width == 0 || certificate->slot.height == 0) {
        return {NativeEllipseCreateCode::InvalidCertificate, "invalid certificate", nullptr};
    }
    if (instanceId == 0) {
        return {NativeEllipseCreateCode::InvalidIdentity, "zero private identity", nullptr};
    }
    auto stream = std::unique_ptr<NativeEllipseStream>(
        new NativeEllipseStream(std::move(certificate), instanceId));
    if (!stream->evaluator_.valid()) {
        return {NativeEllipseCreateCode::EvaluationPreparationFailed,
                std::string(stream->evaluator_.errorMessage()), nullptr};
    }
    stream->evaluator_.prepare(stream->workspace_);
    return {NativeEllipseCreateCode::Ready, {}, std::move(stream)};
}

NativeEllipseFrameResult NativeEllipseStream::emit(
    std::size_t frame, std::size_t width, std::size_t height) {
    ++attemptSequence_;
    if (width == 0 || height == 0)
        return {NativeEllipseFrameCode::InvalidViewport, "zero viewport", std::nullopt};
    const auto& slot = certificate_->slot;
    const auto& model = *certificate_->model;
    frame = std::min(frame, slot.totalFrames - 1U);
    const auto view = evaluator_.evaluate(static_cast<double>(frame), workspace_);
    if (!view)
        return {NativeEllipseFrameCode::EvaluationFailed, "property evaluation failed", std::nullopt};

    runtime::EvaluatedScene scene;
    scene.sourceAssetHash = slot.sourceHash;
    scene.assetHandle = slot.assetHandle;
    scene.instanceId = instanceId_;
    scene.evaluationSequence = attemptSequence_;
    scene.frameIndex = frame;
    scene.viewportWidth = width;
    scene.viewportHeight = height;
    scene.modelLayerCount = slot.modelLayerCount;
    scene.modelNodeCount = slot.modelNodeCount;
    scene.modelGeometryCount = slot.modelGeometryCount;
    scene.modelPaintCount = slot.modelPaintCount;
    const bool active = frame >= slot.activeFirstFrame && frame < slot.activeEndFrame;
    scene.layers.push_back(makeLayer(slot.root, true, 0));
    scene.layers.push_back(makeLayer(slot.shape, active, active ? 1U : 0U));
    scene.childLayerIndices.push_back(1);
    scene.statistics.layerCount = 2;
    scene.statistics.visibleLayerCount = active ? 2 : 1;

    if (active) {
        model::MotionVec2Value position, size;
        if (!resolveVec2(model, view, slot.binding.position, position)
            || !resolveVec2(model, view, slot.binding.size, size)
            || slot.binding.group.index() >= view.nodeTransforms.size()) {
            return {NativeEllipseFrameCode::EvaluationFailed,
                    "bound ellipse property or transform unavailable", std::nullopt};
        }
        const auto& group = view.nodeTransforms[slot.binding.group.index()];
        if (group.node != slot.binding.group || !group.worldSupported()) {
            return {NativeEllipseFrameCode::EvaluationFailed,
                    "group world transform unavailable", std::nullopt};
        }
        const float scale = std::min(float(width) / float(model.logicalWidth),
                                     float(height) / float(model.logicalHeight));
        const float tx = (float(width) - float(model.logicalWidth) * scale) / 2.0F;
        const float ty = (float(height) - float(model.logicalHeight) * scale) / 2.0F;
        const auto& world = group.worldMatrix;
        runtime::AffineTransform transform{world.m11 * scale, world.m12 * scale,
            world.m21 * scale, world.m22 * scale, world.dx * scale + tx,
            world.dy * scale + ty};
        if (!finite(transform)) {
            return {NativeEllipseFrameCode::UnsupportedNumericOutput,
                    "non-finite viewport transform", std::nullopt};
        }
        const auto* ellipse = model.sourceNode(slot.binding.ellipse);
        if (!ellipse) {
            return {NativeEllipseFrameCode::EvaluationFailed,
                    "bound ellipse node unavailable", std::nullopt};
        }
        const auto primitive = generateEllipsePath(position, size, ellipse->pathDirection);
        runtime::EvaluatedDrawItem item;
        const auto& facts = slot.draw;
        item.modelDrawItem = facts.draw;
        item.modelNode = facts.node;
        item.modelGeometry = facts.geometry;
        item.modelPaint = facts.paint;
        item.sourcePathNode = facts.sourcePath;
        item.sourcePaintNode = facts.sourcePaint;
        item.sourcePathCount = facts.sourcePathCount;
        item.sourcePathModifierFree = facts.sourcePathModifierFree;
        item.layerIndex = facts.layerIndex;
        item.drawOrder = facts.drawOrder;
        item.fillRule = facts.fillRule;
        item.stroke.width = facts.strokeWidth;
        item.stroke.miterLimit = facts.strokeMiterLimit;
        item.stroke.cap = facts.strokeCap;
        item.stroke.join = facts.strokeJoin;
        item.paint.kind = runtime::PaintKind::Solid;
        item.paint.solid = facts.solid;
        item.localGeometryAvailable = facts.localGeometryAvailable;
        item.localGeometryStaticCandidate = facts.localGeometryStaticCandidate;
        item.localToViewport = transform;
        item.localPaintAvailable = facts.localPaintAvailable;
        item.localPaintStaticCandidate = facts.localPaintStaticCandidate;
        item.localStroke.width = facts.localStrokeWidth;
        item.localStroke.miterLimit = facts.localStrokeMiterLimit;
        item.localStroke.cap = facts.localStrokeCap;
        item.localStroke.join = facts.localStrokeJoin;
        item.localPaint.kind = runtime::PaintKind::Solid;
        item.localPaint.solid = facts.localSolid;
        item.opacitySeparated = facts.opacitySeparated;
        item.separatedOpacity = facts.separatedOpacity;
        if (!materializePath(primitive, nullptr, item.localPath)
            || !materializePath(primitive, &transform, item.path)) {
            return {NativeEllipseFrameCode::UnsupportedNumericOutput,
                    "non-finite ellipse path", std::nullopt};
        }
        scene.statistics.drawItemCount = 1;
        scene.statistics.solidPaintCount = 1;
        scene.statistics.pathVerbCount = item.path.verbs.size();
        scene.statistics.pathPointCount = item.path.points.size();
        scene.controlBounds = item.path.controlBounds;
        scene.drawItems.push_back(std::move(item));
    }
    const auto applied = model::detail::applyAssetModel(certificate_->model, scene);
    if (!applied) {
        return {NativeEllipseFrameCode::ModelApplicationFailed, applied.error, std::nullopt};
    }
    scene.fingerprints = runtime::computeSceneFingerprints(scene);
    scene.changes.firstEvaluation = !hasPrevious_;
    if (hasPrevious_) {
        scene.changes.topologyChanged = scene.fingerprints.topology != previous_.topology;
        scene.changes.geometryChanged = scene.fingerprints.geometry != previous_.geometry;
        scene.changes.paintChanged = scene.fingerprints.paint != previous_.paint;
        scene.changes.visualChanged = scene.fingerprints.scene != previous_.scene;
    }
    previous_ = scene.fingerprints;
    hasPrevious_ = true;
    return {NativeEllipseFrameCode::Emitted, {}, std::move(scene)};
}

} // namespace avemotion::render::detail
