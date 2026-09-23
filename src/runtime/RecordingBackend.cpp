#include "avemotion/runtime/RecordingBackend.hpp"

#include "avemotion/core/Hash.hpp"

#include <iomanip>
#include <sstream>

namespace avemotion::runtime {
namespace {

void appendRect(core::Fnv1a64& hash, const RectF& rect) noexcept {
    hash.appendU8(rect.valid ? 1U : 0U);
    if (!rect.valid) {
        return;
    }
    hash.appendFloat(rect.left);
    hash.appendFloat(rect.top);
    hash.appendFloat(rect.right);
    hash.appendFloat(rect.bottom);
}

void appendPathTopology(core::Fnv1a64& hash, const EvaluatedPath& path) noexcept {
    hash.appendU64(path.verbs.size());
    hash.appendU64(path.points.size());
    for (const auto verb : path.verbs) {
        hash.appendU8(static_cast<std::uint8_t>(verb));
    }
}

void appendPathGeometry(core::Fnv1a64& hash, const EvaluatedPath& path) noexcept {
    appendPathTopology(hash, path);
    for (const auto point : path.points) {
        hash.appendFloat(point.x);
        hash.appendFloat(point.y);
    }
    appendRect(hash, path.controlBounds);
}

void appendColor(core::Fnv1a64& hash, const Color8& color) noexcept {
    hash.appendU8(color.r);
    hash.appendU8(color.g);
    hash.appendU8(color.b);
    hash.appendU8(color.a);
}

void appendPaint(core::Fnv1a64& hash, const EvaluatedPaint& paint) noexcept {
    hash.appendU8(static_cast<std::uint8_t>(paint.kind));
    switch (paint.kind) {
    case PaintKind::Solid:
        appendColor(hash, paint.solid);
        break;
    case PaintKind::Gradient:
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
    case PaintKind::Image:
        hash.appendU64(paint.image.width);
        hash.appendU64(paint.image.height);
        for (const auto value : paint.image.matrix) {
            hash.appendFloat(value);
        }
        break;
    case PaintKind::None:
        break;
    }
}

void appendStroke(core::Fnv1a64& hash, const EvaluatedStroke& stroke) noexcept {
    hash.appendU8(stroke.enabled ? 1U : 0U);
    hash.appendFloat(stroke.width);
    hash.appendFloat(stroke.miterLimit);
    hash.appendU8(static_cast<std::uint8_t>(stroke.cap));
    hash.appendU8(static_cast<std::uint8_t>(stroke.join));
    hash.appendU64(stroke.dashArray.size());
    for (const auto dash : stroke.dashArray) {
        hash.appendFloat(dash);
    }
}

} // namespace

SceneFingerprints computeSceneFingerprints(const EvaluatedScene& scene) noexcept {
    core::Fnv1a64 topology;
    core::Fnv1a64 geometry;
    core::Fnv1a64 paint;
    core::Fnv1a64 full;

    topology.appendU64(scene.layers.size());
    topology.appendU64(scene.childLayerIndices.size());
    topology.appendU64(scene.drawItems.size());
    topology.appendU64(scene.masks.size());

    for (const auto child : scene.childLayerIndices) {
        topology.appendU32(child);
    }

    for (const auto& layer : scene.layers) {
        topology.appendU32(layer.parentLayer);
        topology.appendU32(layer.firstChildReference);
        topology.appendU32(layer.childCount);
        topology.appendU32(layer.firstDrawItem);
        topology.appendU32(layer.drawItemCount);
        topology.appendU32(layer.firstMask);
        topology.appendU32(layer.maskCount);
        topology.appendString(layer.keyPath);
        topology.appendU8(static_cast<std::uint8_t>(layer.matte));
        appendPathTopology(topology, layer.clipPath);

        geometry.appendU32(layer.parentLayer);
        appendPathGeometry(geometry, layer.clipPath);

        full.appendU8(layer.visible ? 1U : 0U);
        full.appendFloat(layer.opacity);
        full.appendU8(static_cast<std::uint8_t>(layer.matte));
    }

    for (const auto& mask : scene.masks) {
        topology.appendU32(mask.layerIndex);
        topology.appendU8(static_cast<std::uint8_t>(mask.mode));
        appendPathTopology(topology, mask.path);

        geometry.appendU32(mask.layerIndex);
        appendPathGeometry(geometry, mask.path);
        full.appendU8(static_cast<std::uint8_t>(mask.mode));
        full.appendFloat(mask.opacity);
    }

    for (const auto& item : scene.drawItems) {
        topology.appendU32(item.layerIndex);
        topology.appendU32(item.drawOrder);
        topology.appendU8(static_cast<std::uint8_t>(item.fillRule));
        topology.appendU8(item.stroke.enabled ? 1U : 0U);
        topology.appendU8(static_cast<std::uint8_t>(item.paint.kind));
        appendPathTopology(topology, item.path);

        geometry.appendU32(item.layerIndex);
        geometry.appendU32(item.drawOrder);
        appendPathGeometry(geometry, item.path);

        paint.appendU32(item.layerIndex);
        paint.appendU32(item.drawOrder);
        paint.appendU8(static_cast<std::uint8_t>(item.fillRule));
        appendStroke(paint, item.stroke);
        appendPaint(paint, item.paint);

    }

    appendRect(geometry, scene.controlBounds);

    full.appendU64(topology.value());
    full.appendU64(geometry.value());
    full.appendU64(paint.value());

    return {
        .scene = full.value(),
        .topology = topology.value(),
        .geometry = geometry.value(),
        .paint = paint.value(),
    };
}

SceneRecord RecordingBackend::record(const EvaluatedScene& scene) const noexcept {
    return {
        .fingerprints = computeSceneFingerprints(scene),
        .statistics = scene.statistics,
        .controlBounds = scene.controlBounds,
    };
}

std::string RecordingBackend::describe(const EvaluatedScene& scene) const {
    const auto recordValue = record(scene);
    std::ostringstream out;
    out << "AveMotion evaluated scene\n"
        << "  instance:   " << scene.instanceId << '\n'
        << "  frame:      " << scene.frameIndex << '\n'
        << "  viewport:   " << scene.viewportWidth << 'x' << scene.viewportHeight << '\n'
        << "  scene:      " << core::formatHash(recordValue.fingerprints.scene) << '\n'
        << "  topology:   " << core::formatHash(recordValue.fingerprints.topology) << '\n'
        << "  geometry:   " << core::formatHash(recordValue.fingerprints.geometry) << '\n'
        << "  paint:      " << core::formatHash(recordValue.fingerprints.paint) << '\n'
        << "  layers:     " << recordValue.statistics.layerCount << " (visible "
        << recordValue.statistics.visibleLayerCount << ")\n"
        << "  draw items: " << recordValue.statistics.drawItemCount << '\n'
        << "  masks:      " << recordValue.statistics.maskCount << '\n'
        << "  path verbs: " << recordValue.statistics.pathVerbCount << '\n'
        << "  path points:" << recordValue.statistics.pathPointCount << '\n'
        << "  changed:    topology=" << scene.changes.topologyChanged
        << " geometry=" << scene.changes.geometryChanged
        << " paint=" << scene.changes.paintChanged
        << " visual=" << scene.changes.visualChanged << '\n';
    return out.str();
}

} // namespace avemotion::runtime
