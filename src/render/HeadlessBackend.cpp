#include "avemotion/render/HeadlessBackend.hpp"

#include "avemotion/core/Hash.hpp"

#include <iomanip>
#include <sstream>

namespace avemotion::render {
namespace {

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

void appendMatrix(core::Fnv1a64& hash, const Matrix3x2& matrix) noexcept {
    hash.appendFloat(matrix.m11);
    hash.appendFloat(matrix.m12);
    hash.appendFloat(matrix.m21);
    hash.appendFloat(matrix.m22);
    hash.appendFloat(matrix.dx);
    hash.appendFloat(matrix.dy);
}

void appendGeometryKey(core::Fnv1a64& hash, const GeometryCacheKey& key) noexcept {
    hash.appendU8(static_cast<std::uint8_t>(key.scope));
    hash.appendU64(key.assetHash);
    hash.appendU64(key.assetIdentity);
    hash.appendU64(key.instanceId);
    hash.appendU64(key.instanceIdentity);
    hash.appendU64(key.sourceKey);
    hash.appendU64(key.contentHash);
    hash.appendU64(key.revision);
}

void appendPaintKey(core::Fnv1a64& hash, const PaintCacheKey& key) noexcept {
    hash.appendU8(static_cast<std::uint8_t>(key.scope));
    hash.appendU64(key.assetHash);
    hash.appendU64(key.assetIdentity);
    hash.appendU64(key.instanceId);
    hash.appendU64(key.instanceIdentity);
    hash.appendU64(key.sourceKey);
    hash.appendU64(key.contentHash);
    hash.appendU64(key.revision);
}

} // namespace

RenderPlanFingerprints computeRenderPlanFingerprints(
    const MotionRenderPlan& plan) noexcept {
    core::Fnv1a64 topology;
    core::Fnv1a64 geometry;
    core::Fnv1a64 paint;
    core::Fnv1a64 presentation;
    core::Fnv1a64 full;

    topology.appendU64(plan.stamp.assetHash);
    topology.appendU64(plan.stamp.assetIdentity);
    topology.appendU64(plan.stamp.instanceIdentity);
    topology.appendU64(plan.drawItems.size());
    for (const auto& item : plan.drawItems) {
        topology.appendU64(item.sourceItemKey);
        topology.appendU32(item.sourceDrawItemIndex);
        topology.appendU32(item.drawOrder);
        topology.appendU32(item.featureBits);

        geometry.appendU64(item.sourceItemKey);
        appendGeometryKey(geometry, item.geometry);

        paint.appendU64(item.sourceItemKey);
        appendPaintKey(paint, item.paint);

        presentation.appendU64(item.sourceItemKey);
        appendMatrix(presentation, item.geometryTransform);
        appendMatrix(presentation, item.presentationTransform);
        presentation.appendFloat(item.effectiveOpacity);
        appendRect(presentation, item.presentedBounds);
    }

    appendRect(presentation, plan.presentedBounds);
    full.appendU64(topology.value());
    full.appendU64(geometry.value());
    full.appendU64(paint.value());
    full.appendU64(presentation.value());

    return {
        .plan = full.value(),
        .topology = topology.value(),
        .geometryIdentity = geometry.value(),
        .paintIdentity = paint.value(),
        .presentation = presentation.value(),
    };
}

HeadlessPlanRecord HeadlessPlanBackend::record(
    const MotionRenderPlan& plan) const noexcept {
    return {
        .fingerprints = computeRenderPlanFingerprints(plan),
        .statistics = plan.statistics,
        .presentedBounds = plan.presentedBounds,
        .dirtyRegion = plan.dirtyRegion,
    };
}

std::string HeadlessPlanBackend::describe(const MotionRenderPlan& plan) const {
    const auto value = record(plan);
    std::ostringstream out;
    out << "AveMotion render plan\n"
        << "  instance:        " << plan.stamp.instanceId << '\n'
        << "  evaluation:      " << plan.stamp.evaluationSequence << '\n'
        << "  plan sequence:   " << plan.stamp.planSequence << '\n'
        << "  plan:            " << core::formatHash(value.fingerprints.plan) << '\n'
        << "  topology:        " << core::formatHash(value.fingerprints.topology) << '\n'
        << "  geometry ids:    "
        << core::formatHash(value.fingerprints.geometryIdentity) << '\n'
        << "  paint ids:       "
        << core::formatHash(value.fingerprints.paintIdentity) << '\n'
        << "  presentation:    "
        << core::formatHash(value.fingerprints.presentation) << '\n'
        << "  source items:    " << value.statistics.sourceDrawItemCount << '\n'
        << "  visible items:   " << value.statistics.visibleDrawItemCount << '\n'
        << "  geometry updates:" << value.statistics.geometryUpdateCount << '\n'
        << "  paint updates:   " << value.statistics.paintUpdateCount << '\n'
        << "  visual changed:  " << plan.visualChanged << '\n'
        << "  dirty valid:     " << plan.dirtyRegion.valid << '\n';
    return out.str();
}

} // namespace avemotion::render
