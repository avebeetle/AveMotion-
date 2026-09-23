#include "avemotion/render/RenderPlanner.hpp"

#include "avemotion/core/Hash.hpp"
#include "avemotion/render/HeadlessBackend.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <unordered_map>
#include <utility>

namespace avemotion::render {
namespace {

struct ItemState final {
    ResourceIdentityScope geometryScope = ResourceIdentityScope::Instance;
    ResourceIdentityScope paintScope = ResourceIdentityScope::Instance;
    std::uint64_t geometryHash = 0;
    std::uint64_t paintHash = 0;
    std::uint64_t geometryRevision = 0;
    std::uint64_t sourceGeometryRevision = 0;
    std::uint64_t paintRevision = 0;
    bool observed = false;
};

struct InstancePlanState final {
    std::uint64_t assetHash = 0;
    std::uint64_t assetIdentity = 0;
    std::uint64_t lastEvaluationSequence = 0;
    std::uint64_t nextPlanSequence = 1;
    std::uint64_t previousVisualFingerprint = 0;
    bool hasPreviousPlan = false;
    runtime::RectF previousPresentedBounds;
    std::unordered_map<std::uint64_t, ItemState> items;
};

[[nodiscard]] bool finite(float value) noexcept {
    return std::isfinite(value);
}

[[nodiscard]] bool finite(const Matrix3x2& matrix) noexcept {
    return finite(matrix.m11) && finite(matrix.m12)
        && finite(matrix.m21) && finite(matrix.m22)
        && finite(matrix.dx) && finite(matrix.dy);
}

[[nodiscard]] Matrix3x2 toMatrix(
    const runtime::AffineTransform& value) noexcept {
    return {
        value.m11,
        value.m12,
        value.m21,
        value.m22,
        value.dx,
        value.dy,
    };
}

// Row-vector convention, matching Direct2D:
// point * left * right applies left first and right second.
[[nodiscard]] Matrix3x2 multiply(
    const Matrix3x2& left,
    const Matrix3x2& right) noexcept {
    return {
        left.m11 * right.m11 + left.m12 * right.m21,
        left.m11 * right.m12 + left.m12 * right.m22,
        left.m21 * right.m11 + left.m22 * right.m21,
        left.m21 * right.m12 + left.m22 * right.m22,
        left.dx * right.m11 + left.dy * right.m21 + right.dx,
        left.dx * right.m12 + left.dy * right.m22 + right.dy,
    };
}

void includePoint(runtime::RectF& rect, float x, float y) noexcept {
    if (!rect.valid) {
        rect = {true, x, y, x, y};
        return;
    }
    rect.left = std::min(rect.left, x);
    rect.top = std::min(rect.top, y);
    rect.right = std::max(rect.right, x);
    rect.bottom = std::max(rect.bottom, y);
}

void includeRect(runtime::RectF& target, const runtime::RectF& value) noexcept {
    if (!value.valid) {
        return;
    }
    includePoint(target, value.left, value.top);
    includePoint(target, value.right, value.bottom);
}

[[nodiscard]] runtime::RectF expandedRect(
    runtime::RectF rect,
    float amount) noexcept {
    if (!rect.valid || amount <= 0.0F || !finite(amount)) {
        return rect;
    }
    rect.left -= amount;
    rect.top -= amount;
    rect.right += amount;
    rect.bottom += amount;
    return rect;
}

[[nodiscard]] runtime::Vec2 transformPoint(
    const Matrix3x2& matrix,
    runtime::Vec2 point) noexcept {
    return {
        point.x * matrix.m11 + point.y * matrix.m21 + matrix.dx,
        point.x * matrix.m12 + point.y * matrix.m22 + matrix.dy,
    };
}

[[nodiscard]] runtime::RectF transformRect(
    const runtime::RectF& rect,
    const Matrix3x2& matrix) noexcept {
    runtime::RectF result;
    if (!rect.valid) {
        return result;
    }
    const runtime::Vec2 corners[] = {
        {rect.left, rect.top},
        {rect.right, rect.top},
        {rect.right, rect.bottom},
        {rect.left, rect.bottom},
    };
    for (const auto corner : corners) {
        const auto transformed = transformPoint(matrix, corner);
        includePoint(result, transformed.x, transformed.y);
    }
    return result;
}

void appendRect(core::Fnv1a64& hash, const runtime::RectF& rect) noexcept {
    hash.appendU8(rect.valid ? 1U : 0U);
    if (!rect.valid) {
        return;
    }
    hash.appendFloat(rect.left);
    hash.appendFloat(rect.top);
    hash.appendFloat(rect.right);
    hash.appendFloat(rect.bottom);
}

void appendColor(core::Fnv1a64& hash, const runtime::Color8& color) noexcept {
    hash.appendU8(color.r);
    hash.appendU8(color.g);
    hash.appendU8(color.b);
    hash.appendU8(color.a);
}

[[nodiscard]] const runtime::CanonicalGeometry* staticGeometry(
    const runtime::EvaluatedScene&,
    const runtime::EvaluatedDrawItem& item) noexcept {
    // AssetModelBuilder attaches this alias only when the current frame has a
    // complete local geometry + local paint seam. The frozen model alone is
    // not sufficient: a final-space paint combined with a local path would
    // apply the geometry transform twice.
    return item.canonicalGeometry.get();
}

[[nodiscard]] const runtime::CanonicalPaint* staticPaint(
    const runtime::EvaluatedScene&,
    const runtime::EvaluatedDrawItem& item) noexcept {
    return item.canonicalPaint.get();
}

[[nodiscard]] bool usesLocalRenderingSpace(
    const runtime::EvaluatedScene& scene,
    const runtime::EvaluatedDrawItem& item) noexcept {
    return staticGeometry(scene, item) != nullptr
        || (item.localGeometryAvailable && item.localPaintAvailable);
}

[[nodiscard]] const runtime::EvaluatedPath& selectedPath(
    const runtime::EvaluatedScene& scene,
    const runtime::EvaluatedDrawItem& item) noexcept {
    if (const auto* value = staticGeometry(scene, item)) {
        return value->path;
    }
    return usesLocalRenderingSpace(scene, item) ? item.localPath : item.path;
}

[[nodiscard]] runtime::FillRule selectedFillRule(
    const runtime::EvaluatedScene& scene,
    const runtime::EvaluatedDrawItem& item) noexcept {
    if (const auto* value = staticGeometry(scene, item)) {
        return value->fillRule;
    }
    return item.fillRule;
}

[[nodiscard]] const runtime::EvaluatedStroke& selectedStroke(
    const runtime::EvaluatedScene& scene,
    const runtime::EvaluatedDrawItem& item) noexcept {
    if (const auto* value = staticPaint(scene, item)) {
        return value->stroke;
    }
    return item.localPaintAvailable ? item.localStroke : item.stroke;
}

[[nodiscard]] const runtime::EvaluatedPaint& selectedPaint(
    const runtime::EvaluatedScene& scene,
    const runtime::EvaluatedDrawItem& item) noexcept {
    if (const auto* value = staticPaint(scene, item)) {
        return value->paint;
    }
    return item.localPaintAvailable ? item.localPaint : item.paint;
}

[[nodiscard]] std::uint64_t hashPath(
    const runtime::EvaluatedScene& scene,
    const runtime::EvaluatedDrawItem& item) noexcept {
    if (const auto* value = staticGeometry(scene, item)) {
        return value->contentHash;
    }
    const auto& path = selectedPath(scene, item);
    core::Fnv1a64 hash;
    hash.appendU8(static_cast<std::uint8_t>(selectedFillRule(scene, item)));
    hash.appendU64(path.verbs.size());
    hash.appendU64(path.points.size());
    for (const auto verb : path.verbs) {
        hash.appendU8(static_cast<std::uint8_t>(verb));
    }
    for (const auto point : path.points) {
        hash.appendFloat(point.x);
        hash.appendFloat(point.y);
    }
    appendRect(hash, path.controlBounds);
    return hash.value();
}

[[nodiscard]] std::uint64_t hashPaint(
    const runtime::EvaluatedScene& scene,
    const runtime::EvaluatedDrawItem& item) noexcept {
    if (const auto* value = staticPaint(scene, item)) {
        return value->contentHash;
    }
    const auto& stroke = selectedStroke(scene, item);
    const auto& paint = selectedPaint(scene, item);
    core::Fnv1a64 hash;
    hash.appendU8(static_cast<std::uint8_t>(paint.kind));
    hash.appendU8(stroke.enabled ? 1U : 0U);
    hash.appendFloat(stroke.width);
    hash.appendFloat(stroke.miterLimit);
    hash.appendU8(static_cast<std::uint8_t>(stroke.cap));
    hash.appendU8(static_cast<std::uint8_t>(stroke.join));
    hash.appendU64(stroke.dashArray.size());
    for (const auto value : stroke.dashArray) hash.appendFloat(value);
    switch (paint.kind) {
    case runtime::PaintKind::Solid:
        appendColor(hash, paint.solid);
        break;
    case runtime::PaintKind::Gradient:
        hash.appendU8(static_cast<std::uint8_t>(paint.gradient.kind));
        hash.appendFloat(paint.gradient.start.x);
        hash.appendFloat(paint.gradient.start.y);
        hash.appendFloat(paint.gradient.end.x);
        hash.appendFloat(paint.gradient.end.y);
        hash.appendFloat(paint.gradient.center.x);
        hash.appendFloat(paint.gradient.center.y);
        hash.appendFloat(paint.gradient.focal.x);
        hash.appendFloat(paint.gradient.focal.y);
        hash.appendFloat(paint.gradient.centerRadius);
        hash.appendFloat(paint.gradient.focalRadius);
        hash.appendU64(paint.gradient.stops.size());
        for (const auto& stop : paint.gradient.stops) {
            hash.appendFloat(stop.position);
            appendColor(hash, stop.color);
        }
        break;
    case runtime::PaintKind::Image:
        hash.appendU64(paint.image.width);
        hash.appendU64(paint.image.height);
        for (const auto value : paint.image.matrix) hash.appendFloat(value);
        break;
    case runtime::PaintKind::None:
        break;
    }
    return hash.value();
}

[[nodiscard]] std::uint64_t deriveSourceItemKey(
    const runtime::EvaluatedScene& scene,
    const runtime::EvaluatedDrawItem& item,
    std::uint32_t ordinal) noexcept {
    core::Fnv1a64 hash;
    hash.appendU64(scene.sourceAssetHash);
    hash.appendU32(item.layerIndex);
    hash.appendU32(ordinal);
    return hash.value();
}

[[nodiscard]] std::uint32_t localOrdinal(
    const runtime::EvaluatedScene& scene,
    std::uint32_t sourceItemIndex,
    const runtime::EvaluatedDrawItem& item) noexcept {
    if (item.layerIndex >= scene.layers.size()) {
        return sourceItemIndex;
    }
    const auto first = scene.layers[item.layerIndex].firstDrawItem;
    return sourceItemIndex >= first ? sourceItemIndex - first : sourceItemIndex;
}

[[nodiscard]] ResourceIdentityScope mapOrigin(
    runtime::EvaluatedValueOrigin origin) noexcept {
    return origin == runtime::EvaluatedValueOrigin::AssetStatic
        ? ResourceIdentityScope::Asset
        : ResourceIdentityScope::Instance;
}

[[nodiscard]] std::uint32_t featureBits(
    const runtime::EvaluatedScene& scene,
    const runtime::EvaluatedDrawItem& item) noexcept {
    const auto& paint = selectedPaint(scene, item);
    const auto& stroke = selectedStroke(scene, item);
    std::uint32_t bits = RenderFeatureNone;
    switch (paint.kind) {
    case runtime::PaintKind::Solid: bits |= RenderFeatureSolidPaint; break;
    case runtime::PaintKind::Gradient: bits |= RenderFeatureGradientPaint; break;
    case runtime::PaintKind::Image: bits |= RenderFeatureImagePaint; break;
    case runtime::PaintKind::None: break;
    }
    if (stroke.enabled) {
        bits |= RenderFeatureStroke;
        if (!stroke.dashArray.empty()) {
            bits |= RenderFeatureDash;
        }
    }
    if (item.layerIndex < scene.layers.size()) {
        const auto& layer = scene.layers[item.layerIndex];
        if (!layer.clipPath.verbs.empty()) {
            bits |= RenderFeatureClip;
        }
        if (layer.maskCount != 0U) {
            bits |= RenderFeatureMask;
        }
        if (layer.matte != runtime::MatteMode::None) {
            bits |= RenderFeatureMatte;
        }
    }
    return bits;
}

[[nodiscard]] bool containsUnsupportedMvpFeatures(std::uint32_t bits) noexcept {
    constexpr auto supported = RenderFeatureSolidPaint | RenderFeatureStroke;
    return (bits & ~supported) != 0U;
}

[[nodiscard]] std::uint64_t sceneInstanceIdentity(
    const runtime::EvaluatedScene& scene) noexcept {
    return scene.instanceHandle.valid()
        ? scene.instanceHandle.packed()
        : scene.instanceId;
}

[[nodiscard]] std::uint64_t sceneAssetIdentity(
    const runtime::EvaluatedScene& scene) noexcept {
    return scene.assetHandle.valid() ? scene.assetHandle.packed() : 0U;
}

} // namespace

struct MotionRenderPlanner::Impl final {
    std::unordered_map<std::uint64_t, InstancePlanState> instances;
    RenderPlannerDiagnostics diagnostics;
};

MotionRenderPlanner::MotionRenderPlanner()
    : impl_(std::make_unique<Impl>()) {
}

MotionRenderPlanner::MotionRenderPlanner(MotionRenderPlanner&&) noexcept = default;
MotionRenderPlanner& MotionRenderPlanner::operator=(MotionRenderPlanner&&) noexcept = default;
MotionRenderPlanner::~MotionRenderPlanner() = default;

RenderPlanBuildResult MotionRenderPlanner::build(
    runtime::EvaluatedScene scene,
    const PresentationState& presentation) {
    return build(
        std::make_shared<const runtime::EvaluatedScene>(std::move(scene)),
        presentation);
}

RenderPlanBuildResult MotionRenderPlanner::build(
    std::shared_ptr<const runtime::EvaluatedScene> scene,
    const PresentationState& presentation) {
    RenderPlanBuildResult result;
    if (!scene) {
        result.error = {RenderPlanErrorCode::InvalidArgument, "scene is null"};
        ++impl_->diagnostics.plansRejected;
        return result;
    }
    if (scene->instanceId == 0U || scene->sourceAssetHash == 0U) {
        result.error = {
            RenderPlanErrorCode::InvalidScene,
            "scene must contain non-zero asset and instance identities"};
        ++impl_->diagnostics.plansRejected;
        return result;
    }
    if (!finite(presentation.transform) || !finite(presentation.opacity)
        || presentation.opacity < 0.0F) {
        result.error = {
            RenderPlanErrorCode::InvalidArgument,
            "presentation transform and opacity must be finite"};
        ++impl_->diagnostics.plansRejected;
        return result;
    }

    const auto instanceIdentity = sceneInstanceIdentity(*scene);
    const auto assetIdentity = sceneAssetIdentity(*scene);
    auto& state = impl_->instances[instanceIdentity];
    if (state.hasPreviousPlan
        && (state.assetHash != scene->sourceAssetHash
            || state.assetIdentity != assetIdentity)) {
        state = {};
    }
    if (state.hasPreviousPlan
        && scene->evaluationSequence < state.lastEvaluationSequence) {
        result.error = {
            RenderPlanErrorCode::StaleSnapshot,
            "evaluation sequence is older than the last accepted snapshot"};
        ++impl_->diagnostics.plansRejected;
        return result;
    }
    state.assetHash = scene->sourceAssetHash;
    state.assetIdentity = assetIdentity;

    auto& plan = result.plan;
    plan.sourceScene = std::move(scene);
    plan.stamp = {
        .assetHash = plan.sourceScene->sourceAssetHash,
        .assetIdentity = assetIdentity,
        .instanceId = plan.sourceScene->instanceId,
        .instanceIdentity = instanceIdentity,
        .evaluationSequence = plan.sourceScene->evaluationSequence,
        .planSequence = state.nextPlanSequence++,
        .layoutRevision = presentation.layoutRevision,
    };
    plan.firstPlan = !state.hasPreviousPlan;
    plan.statistics.sourceDrawItemCount = plan.sourceScene->drawItems.size();
    plan.drawItems.reserve(plan.sourceScene->drawItems.size());
    plan.geometryUpdates.reserve(plan.sourceScene->drawItems.size());
    plan.paintUpdates.reserve(plan.sourceScene->drawItems.size());

    for (std::size_t sourceIndex = 0;
         sourceIndex < plan.sourceScene->drawItems.size();
         ++sourceIndex) {
        const auto& sourceItem = plan.sourceScene->drawItems[sourceIndex];
        if (sourceItem.layerIndex >= plan.sourceScene->layers.size()) {
            result.error = {
                RenderPlanErrorCode::InvalidScene,
                "draw item references an invalid layer"};
            ++impl_->diagnostics.plansRejected;
            return result;
        }
        const auto& layer = plan.sourceScene->layers[sourceItem.layerIndex];
        const auto inheritedOpacity = sourceItem.opacitySeparated
            ? sourceItem.separatedOpacity
            : layer.opacity;
        const auto opacity = std::clamp(
            presentation.opacity * inheritedOpacity,
            0.0F,
            1.0F);
        if (!presentation.visible || !layer.visible || opacity <= 0.0F) {
            continue;
        }

        const auto sourceIndex32 = static_cast<std::uint32_t>(sourceIndex);
        const auto ordinal = localOrdinal(
            *plan.sourceScene, sourceIndex32, sourceItem);
        const auto fallbackKey = deriveSourceItemKey(
            *plan.sourceScene, sourceItem, ordinal);
        const bool modelAware = plan.sourceScene->assetModelApplied;
        const auto stableItemKey = modelAware && sourceItem.modelNode.valid()
            ? static_cast<std::uint64_t>(sourceItem.modelNode.value) + 1U
            : fallbackKey;
        const auto geometrySourceKey = sourceItem.sourceGeometryId != 0U
            ? sourceItem.sourceGeometryId
            : fallbackKey;
        const auto paintSourceKey = sourceItem.sourcePaintId != 0U
            ? sourceItem.sourcePaintId
            : fallbackKey;
        const auto geometryId = !sourceItem.sourceGeometryProjected
                && modelAware && sourceItem.modelGeometry.valid()
            ? sourceItem.modelGeometry
            : model::GeometryId{};
        const auto paintId = modelAware && sourceItem.modelPaint.valid()
            ? sourceItem.modelPaint
            : model::PaintId{};
        const auto geometryHash = hashPath(*plan.sourceScene, sourceItem);
        const auto paintHash = hashPaint(*plan.sourceScene, sourceItem);
        const auto geometryScope = mapOrigin(sourceItem.geometryOrigin);
        const auto paintScope = mapOrigin(sourceItem.paintOrigin);

        auto& itemState = state.items[stableItemKey];
        const auto firstItemObservation = !itemState.observed;
        const auto sourceGeometryRevision =
            sourceItem.sourceGeometryRevisionAuthoritative
            ? sourceItem.sourceGeometryRevision
            : 0U;
        const auto geometryChanged = firstItemObservation
            || itemState.geometryScope != geometryScope
            || itemState.geometryHash != geometryHash
            || (sourceItem.sourceGeometryRevisionAuthoritative
                && itemState.sourceGeometryRevision != sourceGeometryRevision);
        const auto paintChanged = firstItemObservation
            || itemState.paintScope != paintScope
            || itemState.paintHash != paintHash;
        if (geometryChanged) {
            if (sourceItem.sourceGeometryRevisionAuthoritative) {
                itemState.geometryRevision = sourceGeometryRevision;
            } else {
                ++itemState.geometryRevision;
            }
            itemState.sourceGeometryRevision = sourceGeometryRevision;
            itemState.geometryHash = geometryHash;
            itemState.geometryScope = geometryScope;
        }
        if (paintChanged) {
            ++itemState.paintRevision;
            itemState.paintHash = paintHash;
            itemState.paintScope = paintScope;
        }
        itemState.observed = true;

        MotionDrawItem item;
        item.drawItem = modelAware && sourceItem.modelDrawItem.valid()
            ? sourceItem.modelDrawItem
            : model::DrawItemId{};
        item.node = modelAware && sourceItem.modelNode.valid()
            ? sourceItem.modelNode
            : model::NodeId{};
        item.sourceItemKey = stableItemKey;
        item.sourceDrawItemIndex = sourceIndex32;
        item.drawOrder = static_cast<std::uint32_t>(plan.drawItems.size());
        item.geometry = {
            .scope = geometryScope,
            .assetHash = plan.stamp.assetHash,
            .assetIdentity = plan.stamp.assetIdentity,
            .instanceId = geometryScope == ResourceIdentityScope::Instance
                ? plan.stamp.instanceId
                : 0U,
            .instanceIdentity = geometryScope == ResourceIdentityScope::Instance
                ? plan.stamp.instanceIdentity
                : 0U,
            .sourceKey = geometrySourceKey,
            .resourceId = geometryId,
            .contentHash = geometryHash,
            .revision = geometryScope == ResourceIdentityScope::Instance
                ? itemState.geometryRevision
                : 0U,
        };
        item.paint = {
            .scope = paintScope,
            .assetHash = plan.stamp.assetHash,
            .assetIdentity = plan.stamp.assetIdentity,
            .instanceId = paintScope == ResourceIdentityScope::Instance
                ? plan.stamp.instanceId
                : 0U,
            .instanceIdentity = paintScope == ResourceIdentityScope::Instance
                ? plan.stamp.instanceIdentity
                : 0U,
            .sourceKey = paintSourceKey,
            .resourceId = paintId,
            .contentHash = paintHash,
            .revision = paintScope == ResourceIdentityScope::Instance
                ? itemState.paintRevision
                : 0U,
        };
        item.geometryTransform = usesLocalRenderingSpace(*plan.sourceScene, sourceItem)
            ? toMatrix(sourceItem.localToViewport)
            : Matrix3x2::identity();
        item.presentationTransform = presentation.transform;
        item.effectiveOpacity = opacity;
        item.featureBits = featureBits(*plan.sourceScene, sourceItem);

        const auto& stroke = selectedStroke(*plan.sourceScene, sourceItem);
        const auto& path = selectedPath(*plan.sourceScene, sourceItem);
        const auto strokeExpansion = stroke.enabled
            ? std::max(0.0F, stroke.width * 0.5F)
            : 0.0F;
        const auto geometryToPresentation = multiply(
            item.geometryTransform,
            item.presentationTransform);
        item.presentedBounds = transformRect(
            expandedRect(path.controlBounds, strokeExpansion),
            geometryToPresentation);
        includeRect(plan.presentedBounds, item.presentedBounds);

        const auto planIndex = static_cast<std::uint32_t>(plan.drawItems.size());
        plan.drawItems.push_back(item);
        if (geometryChanged) {
            plan.geometryUpdates.push_back({
                .planDrawItemIndex = planIndex,
                .key = item.geometry,
                .sourceDrawItemIndex = sourceIndex32,
            });
        }
        if (paintChanged) {
            plan.paintUpdates.push_back({
                .planDrawItemIndex = planIndex,
                .key = item.paint,
                .sourceDrawItemIndex = sourceIndex32,
            });
        }

        if (geometryScope == ResourceIdentityScope::Asset) {
            ++plan.statistics.assetStaticGeometryCount;
        } else {
            ++plan.statistics.instanceGeometryCount;
        }
        if (paintScope == ResourceIdentityScope::Asset) {
            ++plan.statistics.assetStaticPaintCount;
        } else {
            ++plan.statistics.instancePaintCount;
        }
        if (containsUnsupportedMvpFeatures(item.featureBits)) {
            ++plan.statistics.unsupportedFeatureItemCount;
        }
    }

    plan.statistics.visibleDrawItemCount = plan.drawItems.size();
    plan.statistics.geometryUpdateCount = plan.geometryUpdates.size();
    plan.statistics.paintUpdateCount = plan.paintUpdates.size();
    plan.fingerprints = computeRenderPlanFingerprints(plan);
    plan.visualChanged = !state.hasPreviousPlan
        || plan.fingerprints.plan != state.previousVisualFingerprint;
    if (plan.visualChanged) {
        includeRect(plan.dirtyRegion, state.previousPresentedBounds);
        includeRect(plan.dirtyRegion, plan.presentedBounds);
    }

    state.lastEvaluationSequence = plan.sourceScene->evaluationSequence;
    state.previousVisualFingerprint = plan.fingerprints.plan;
    state.previousPresentedBounds = plan.presentedBounds;
    state.hasPreviousPlan = true;

    ++impl_->diagnostics.plansBuilt;
    impl_->diagnostics.drawItemsPlanned += plan.drawItems.size();
    impl_->diagnostics.geometryUpdates += plan.geometryUpdates.size();
    impl_->diagnostics.paintUpdates += plan.paintUpdates.size();
    if (!plan.visualChanged) {
        ++impl_->diagnostics.unchangedPlans;
    }
    return result;
}

void MotionRenderPlanner::forgetInstance(std::uint64_t instanceId) noexcept {
    if (impl_->instances.erase(instanceId) != 0U) {
        ++impl_->diagnostics.forgottenInstances;
    }
}

void MotionRenderPlanner::forgetInstance(
    runtime::InstanceHandle instance) noexcept {
    if (instance.valid() && impl_->instances.erase(instance.packed()) != 0U) {
        ++impl_->diagnostics.forgottenInstances;
    }
}

void MotionRenderPlanner::reset() noexcept {
    impl_->instances.clear();
    impl_->diagnostics = {};
}

RenderPlannerDiagnostics MotionRenderPlanner::diagnostics() const noexcept {
    return impl_->diagnostics;
}

} // namespace avemotion::render
