#include "AssetModelBuilder.hpp"

#include "avemotion/core/Hash.hpp"

#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

namespace avemotion::model::detail {
namespace {

void appendColor(core::Fnv1a64& hash, const runtime::Color8& color) noexcept {
    hash.appendU8(color.r);
    hash.appendU8(color.g);
    hash.appendU8(color.b);
    hash.appendU8(color.a);
}

[[nodiscard]] std::uint64_t hashGeometry(
    runtime::FillRule fillRule,
    const runtime::EvaluatedPath& path) noexcept {
    core::Fnv1a64 hash;
    hash.appendU8(static_cast<std::uint8_t>(fillRule));
    hash.appendU64(path.hash);
    hash.appendU64(path.verbs.size());
    hash.appendU64(path.points.size());
    return hash.value();
}

[[nodiscard]] std::uint64_t hashPaint(
    const runtime::EvaluatedStroke& stroke,
    const runtime::EvaluatedPaint& paint) noexcept {
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
        hash.appendU8(paint.image.present ? 1U : 0U);
        hash.appendU64(paint.image.width);
        hash.appendU64(paint.image.height);
        for (const auto value : paint.image.matrix) hash.appendFloat(value);
        break;
    case runtime::PaintKind::None:
        break;
    }
    return hash.value();
}

[[nodiscard]] bool samePath(
    const runtime::EvaluatedPath& left,
    const runtime::EvaluatedPath& right) noexcept {
    if (left.verbs != right.verbs || left.points.size() != right.points.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.points.size(); ++index) {
        if (left.points[index].x != right.points[index].x
            || left.points[index].y != right.points[index].y) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool sameStroke(
    const runtime::EvaluatedStroke& left,
    const runtime::EvaluatedStroke& right) noexcept {
    return left.enabled == right.enabled
        && left.width == right.width
        && left.miterLimit == right.miterLimit
        && left.cap == right.cap
        && left.join == right.join
        && left.dashArray == right.dashArray;
}

[[nodiscard]] bool samePaint(
    const runtime::EvaluatedPaint& left,
    const runtime::EvaluatedPaint& right) noexcept {
    if (left.kind != right.kind) return false;
    switch (left.kind) {
    case runtime::PaintKind::Solid:
        return left.solid.r == right.solid.r
            && left.solid.g == right.solid.g
            && left.solid.b == right.solid.b
            && left.solid.a == right.solid.a;
    case runtime::PaintKind::Gradient:
        if (left.gradient.kind != right.gradient.kind
            || left.gradient.start.x != right.gradient.start.x
            || left.gradient.start.y != right.gradient.start.y
            || left.gradient.end.x != right.gradient.end.x
            || left.gradient.end.y != right.gradient.end.y
            || left.gradient.center.x != right.gradient.center.x
            || left.gradient.center.y != right.gradient.center.y
            || left.gradient.focal.x != right.gradient.focal.x
            || left.gradient.focal.y != right.gradient.focal.y
            || left.gradient.centerRadius != right.gradient.centerRadius
            || left.gradient.focalRadius != right.gradient.focalRadius
            || left.gradient.stops.size() != right.gradient.stops.size()) {
            return false;
        }
        for (std::size_t index = 0; index < left.gradient.stops.size(); ++index) {
            const auto& a = left.gradient.stops[index];
            const auto& b = right.gradient.stops[index];
            if (a.position != b.position
                || a.color.r != b.color.r || a.color.g != b.color.g
                || a.color.b != b.color.b || a.color.a != b.color.a) {
                return false;
            }
        }
        return true;
    case runtime::PaintKind::Image:
        return left.image.present == right.image.present
            && left.image.width == right.image.width
            && left.image.height == right.image.height
            && std::equal(
                std::begin(left.image.matrix), std::end(left.image.matrix),
                std::begin(right.image.matrix));
    case runtime::PaintKind::None:
        return true;
    }
    return false;
}

template <typename Id>
void appendUnique(std::vector<Id>& values, Id value) {
    if (!value.valid()) return;
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

[[nodiscard]] std::size_t requiredSize(
    std::uint32_t declared,
    std::size_t observed) noexcept {
    return std::max<std::size_t>(declared, observed);
}

void ensureTables(MotionAssetModel& model, const runtime::EvaluatedScene& scene) {
    model.layers.resize(requiredSize(scene.modelLayerCount, scene.layers.size()));
    model.nodes.resize(requiredSize(scene.modelNodeCount, scene.drawItems.size()));
    model.geometries.resize(requiredSize(
        scene.modelGeometryCount, scene.drawItems.size()));
    model.paints.resize(requiredSize(scene.modelPaintCount, scene.drawItems.size()));
}

[[nodiscard]] std::uint32_t dependencyBits(
    const runtime::EvaluatedDrawItem& item) noexcept {
    std::uint32_t result = StaticDependencyTransform;
    if (!item.localGeometryStaticCandidate) result |= StaticDependencyGeometry;
    if (!item.localPaintStaticCandidate) result |= StaticDependencyPaint;
    return result;
}

void computeStatistics(MotionAssetModel& model) noexcept {
    auto& stats = model.statistics;
    stats.observedLayerCount = static_cast<std::size_t>(std::count_if(
        model.layers.begin(), model.layers.end(),
        [](const auto& value) { return value.present; }));
    stats.observedNodeCount = static_cast<std::size_t>(std::count_if(
        model.nodes.begin(), model.nodes.end(),
        [](const auto& value) { return value.present; }));
    stats.observedGeometryCount = static_cast<std::size_t>(std::count_if(
        model.geometries.begin(), model.geometries.end(),
        [](const auto& value) { return value.present; }));
    stats.observedPaintCount = static_cast<std::size_t>(std::count_if(
        model.paints.begin(), model.paints.end(),
        [](const auto& value) { return value.present; }));
    stats.assetStaticGeometryCount = static_cast<std::size_t>(std::count_if(
        model.geometries.begin(), model.geometries.end(),
        [](const auto& value) {
            return value.present
                && value.resourceClass == ResourceClass::AssetStatic;
        }));
    stats.assetStaticPaintCount = static_cast<std::size_t>(std::count_if(
        model.paints.begin(), model.paints.end(),
        [](const auto& value) {
            return value.present
                && value.resourceClass == ResourceClass::AssetStatic;
        }));
    stats.clipCount = static_cast<std::size_t>(std::count_if(
        model.clips.begin(), model.clips.end(),
        [](const auto& value) { return value.present; }));
}

void computeFingerprints(MotionAssetModel& model) noexcept {
    core::Fnv1a64 topology;
    topology.appendU32(model.schemaVersion);
    topology.appendU64(model.layers.size());
    topology.appendU64(model.nodes.size());
    topology.appendU64(model.childLayerIds.size());
    topology.appendU64(model.layerNodeIds.size());
    topology.appendU64(model.drawOrder.size());
    for (const auto& layer : model.layers) {
        topology.appendU8(layer.present ? 1U : 0U);
        if (!layer.present) continue;
        topology.appendU32(layer.id.value);
        topology.appendU32(layer.parent.value);
        topology.appendU32(layer.children.first);
        topology.appendU32(layer.children.count);
        topology.appendU32(layer.nodes.first);
        topology.appendU32(layer.nodes.count);
        topology.appendU32(layer.masks.first);
        topology.appendU32(layer.masks.count);
        topology.appendU64(layer.nameHash);
        topology.appendU32(layer.dependencyBits);
        topology.appendU8(static_cast<std::uint8_t>(layer.matte));
    }
    for (const auto id : model.childLayerIds) topology.appendU32(id.value);
    for (const auto id : model.layerNodeIds) topology.appendU32(id.value);
    for (const auto& node : model.nodes) {
        topology.appendU8(node.present ? 1U : 0U);
        if (!node.present) continue;
        topology.appendU32(node.id.value);
        topology.appendU32(node.drawItem.value);
        topology.appendU32(node.layer.value);
        topology.appendU32(node.geometry.value);
        topology.appendU32(node.paint.value);
        topology.appendU32(node.drawOrder);
        topology.appendU32(node.dependencyBits);
    }
    for (const auto id : model.drawOrder) topology.appendU32(id.value);
    for (const auto& clip : model.clips) {
        topology.appendU8(clip.present ? 1U : 0U);
        if (!clip.present) continue;
        topology.appendU32(clip.id.value);
        topology.appendDouble(clip.firstFrame);
        topology.appendDouble(clip.endFrame);
        topology.appendU8(static_cast<std::uint8_t>(clip.defaultLoop));
    }
    model.topologyFingerprint = topology.value();

    core::Fnv1a64 resources;
    resources.appendU64(model.geometries.size());
    for (const auto& geometry : model.geometries) {
        resources.appendU8(geometry.present ? 1U : 0U);
        if (!geometry.present) continue;
        resources.appendU32(geometry.id.value);
        resources.appendU8(static_cast<std::uint8_t>(geometry.resourceClass));
        resources.appendU64(geometry.contentHash);
    }
    resources.appendU64(model.paints.size());
    for (const auto& paint : model.paints) {
        resources.appendU8(paint.present ? 1U : 0U);
        if (!paint.present) continue;
        resources.appendU32(paint.id.value);
        resources.appendU8(static_cast<std::uint8_t>(paint.resourceClass));
        resources.appendU64(paint.contentHash);
    }
    model.resourceFingerprint = resources.value();

    core::Fnv1a64 full;
    full.appendU32(model.schemaVersion);
    full.appendU64(model.sourceAssetHash);
    full.appendU64(model.logicalWidth);
    full.appendU64(model.logicalHeight);
    full.appendDouble(model.frameRate);
    full.appendU64(model.totalFrames);
    full.appendU64(model.topologyFingerprint);
    full.appendU64(model.resourceFingerprint);
    full.appendU64(model.parsedModelFingerprint);
    model.fingerprint = full.value();
}

[[nodiscard]] bool validRange(IndexRange range, std::size_t size) noexcept {
    return static_cast<std::size_t>(range.first) <= size
        && static_cast<std::size_t>(range.count) <= size - range.first;
}

} // namespace

AssetModelUpdateResult updateAssetModel(
    std::shared_ptr<const MotionAssetModel> previous,
    const runtime::EvaluatedScene& scene,
    const AssetModelDescriptor& descriptor) {
    AssetModelUpdateResult result;
    auto next = previous
        ? std::make_shared<MotionAssetModel>(*previous)
        : std::make_shared<MotionAssetModel>();
    bool changed = !previous;

    if (!previous) {
        next->assetHandle = descriptor.assetHandle;
        next->sourceAssetHash = descriptor.sourceAssetHash;
        next->logicalWidth = descriptor.logicalWidth;
        next->logicalHeight = descriptor.logicalHeight;
        next->frameRate = descriptor.frameRate;
        next->totalFrames = descriptor.totalFrames;
        next->debugName = descriptor.debugName != nullptr
            ? descriptor.debugName : std::string{};
        next->clips.resize(1U);
        next->clips[0] = {
            .present = true,
            .id = ClipId{0U},
            .debugName = "default",
            .firstFrame = 0U,
            .endFrame = static_cast<double>(descriptor.totalFrames),
            .defaultLoop = ClipLoopHint::Loop,
        };
    } else if (previous->sourceAssetHash != descriptor.sourceAssetHash
               || previous->assetHandle != descriptor.assetHandle) {
        result.error = "asset model descriptor does not match previous snapshot";
        return result;
    }

    const auto oldLayerCount = next->layers.size();
    const auto oldNodeCount = next->nodes.size();
    const auto oldGeometryCount = next->geometries.size();
    const auto oldPaintCount = next->paints.size();
    ensureTables(*next, scene);
    changed = changed || oldLayerCount != next->layers.size()
        || oldNodeCount != next->nodes.size()
        || oldGeometryCount != next->geometries.size()
        || oldPaintCount != next->paints.size();

    const auto updateMaximum = [&changed](auto& target, const auto value) {
        if (value > target) {
            target = value;
            changed = true;
        }
    };
    updateMaximum(next->statistics.declaredLayerCount,
        static_cast<std::size_t>(scene.modelLayerCount));
    updateMaximum(next->statistics.declaredNodeCount,
        static_cast<std::size_t>(scene.modelNodeCount));
    updateMaximum(next->statistics.declaredGeometryCount,
        static_cast<std::size_t>(scene.modelGeometryCount));
    updateMaximum(next->statistics.declaredPaintCount,
        static_cast<std::size_t>(scene.modelPaintCount));
    updateMaximum(next->statistics.maskCount, scene.masks.size());

    std::vector<std::vector<LayerId>> children(next->layers.size());
    std::vector<std::vector<NodeId>> layerNodes(next->layers.size());
    if (previous) {
        for (const auto& layer : previous->layers) {
            if (!layer.present || !layer.id.valid()) continue;
            if (validRange(layer.children, previous->childLayerIds.size())) {
                auto& output = children[layer.id.index()];
                output.insert(output.end(),
                    previous->childLayerIds.begin() + layer.children.first,
                    previous->childLayerIds.begin() + layer.children.end());
            }
            if (validRange(layer.nodes, previous->layerNodeIds.size())) {
                auto& output = layerNodes[layer.id.index()];
                output.insert(output.end(),
                    previous->layerNodeIds.begin() + layer.nodes.first,
                    previous->layerNodeIds.begin() + layer.nodes.end());
            }
        }
    }

    for (std::size_t index = 0; index < scene.layers.size(); ++index) {
        const auto& source = scene.layers[index];
        if (!source.modelLayer.valid()
            || source.modelLayer.index() >= next->layers.size()) {
            result.error = "evaluated layer has invalid stable ID";
            return result;
        }
        auto& record = next->layers[source.modelLayer.index()];
        if (!record.present) {
            record.present = true;
            record.id = source.modelLayer;
            record.debugName = source.keyPath;
            core::Fnv1a64 nameHash;
            nameHash.appendString(source.keyPath);
            record.nameHash = nameHash.value();
            record.matte = source.matte;
            changed = true;
        }
        if (source.parentLayer != runtime::kInvalidSceneIndex) {
            if (source.parentLayer >= scene.layers.size()) {
                result.error = "evaluated layer has invalid parent index";
                return result;
            }
            const auto parent = scene.layers[source.parentLayer].modelLayer;
            if (record.parent.valid() && record.parent != parent) {
                result.error = "stable layer changed parent identity";
                return result;
            }
            if (record.parent != parent) {
                record.parent = parent;
                changed = true;
            }
        }
        if (source.matte != record.matte) {
            result.error = "stable layer changed matte mode";
            return result;
        }
        if (source.maskCount > record.masks.count) {
            record.masks.count = source.maskCount;
            changed = true;
        }
        const auto previousLayerDependencies = record.dependencyBits;
        if (source.maskCount != 0U) record.dependencyBits |= StaticDependencyMask;
        if (source.matte != runtime::MatteMode::None) {
            record.dependencyBits |= StaticDependencyMatte;
        }
        changed = changed || previousLayerDependencies != record.dependencyBits;

        for (std::uint32_t child = 0; child < source.childCount; ++child) {
            const auto ref = source.firstChildReference + child;
            if (ref >= scene.childLayerIndices.size()
                || scene.childLayerIndices[ref] >= scene.layers.size()) {
                result.error = "evaluated layer has invalid child reference";
                return result;
            }
            appendUnique(children[record.id.index()],
                scene.layers[scene.childLayerIndices[ref]].modelLayer);
        }
        for (std::uint32_t draw = 0; draw < source.drawItemCount; ++draw) {
            const auto sourceDraw = source.firstDrawItem + draw;
            if (sourceDraw >= scene.drawItems.size()) {
                result.error = "evaluated layer has invalid draw-item range";
                return result;
            }
            appendUnique(layerNodes[record.id.index()],
                scene.drawItems[sourceDraw].modelNode);
        }
    }

    std::vector<NodeId> drawOrder = next->drawOrder;
    for (const auto& source : scene.drawItems) {
        if (!source.modelNode.valid() || source.modelNode.index() >= next->nodes.size()
            || !source.modelGeometry.valid()
            || source.modelGeometry.index() >= next->geometries.size()
            || !source.modelPaint.valid()
            || source.modelPaint.index() >= next->paints.size()
            || source.layerIndex >= scene.layers.size()) {
            result.error = "evaluated draw item has invalid stable IDs";
            return result;
        }
        appendUnique(drawOrder, source.modelNode);
        const auto order = static_cast<std::uint32_t>(
            std::find(drawOrder.begin(), drawOrder.end(), source.modelNode)
                - drawOrder.begin());

        auto& node = next->nodes[source.modelNode.index()];
        const auto layer = scene.layers[source.layerIndex].modelLayer;
        if (!node.present) {
            node.present = true;
            node.id = source.modelNode;
            node.drawItem = DrawItemId{source.modelNode.value};
            node.layer = layer;
            node.geometry = source.modelGeometry;
            node.paint = source.modelPaint;
            node.drawOrder = order;
            node.dependencyBits = dependencyBits(source);
            changed = true;
        } else if (node.layer != layer
                   || node.geometry != source.modelGeometry
                   || node.paint != source.modelPaint) {
            result.error = "stable node changed layer or resource identity";
            return result;
        } else {
            const auto previousNodeDependencies = node.dependencyBits;
            node.dependencyBits |= dependencyBits(source);
            changed = changed || previousNodeDependencies != node.dependencyBits;
        }

        auto& geometry = next->geometries[source.modelGeometry.index()];
        if (!geometry.present) {
            geometry.present = true;
            geometry.id = source.modelGeometry;
            changed = true;
        }
        const bool geometryAuthoritative = source.localGeometryAvailable
            && source.localPaintAvailable;
        const bool geometryCanBeStatic = geometryAuthoritative
            && source.localGeometryStaticCandidate;
        if (geometry.resourceClass == ResourceClass::Unknown
            && geometryAuthoritative) {
            if (geometryCanBeStatic) {
                const auto contentHash = hashGeometry(source.fillRule, source.localPath);
                geometry.resourceClass = ResourceClass::AssetStatic;
                geometry.contentHash = contentHash;
                geometry.staticValue = runtime::CanonicalGeometry{
                    .sourceKey = static_cast<std::uint64_t>(source.modelGeometry.value) + 1U,
                    .contentHash = contentHash,
                    .fillRule = source.fillRule,
                    .path = source.localPath,
                };
                ++result.geometriesCreated;
            } else {
                geometry.resourceClass = ResourceClass::InstanceEvaluated;
            }
            changed = true;
        } else if (geometry.resourceClass == ResourceClass::AssetStatic
                   && geometry.staticValue && geometryAuthoritative) {
            const auto contentHash = hashGeometry(source.fillRule, source.localPath);
            if (!geometryCanBeStatic || geometry.contentHash != contentHash
                || geometry.staticValue->fillRule != source.fillRule
                || !samePath(geometry.staticValue->path, source.localPath)) {
                geometry.resourceClass = ResourceClass::InstanceEvaluated;
                geometry.contentHash = 0U;
                geometry.staticValue.reset();
                ++result.conflicts;
                changed = true;
            }
        }

        auto& paint = next->paints[source.modelPaint.index()];
        if (!paint.present) {
            paint.present = true;
            paint.id = source.modelPaint;
            changed = true;
        }
        const bool paintAuthoritative = source.localPaintAvailable;
        const bool paintCanBeStatic = paintAuthoritative
            && source.localPaintStaticCandidate;
        if (paint.resourceClass == ResourceClass::Unknown && paintAuthoritative) {
            if (paintCanBeStatic) {
                const auto contentHash = hashPaint(source.localStroke, source.localPaint);
                paint.resourceClass = ResourceClass::AssetStatic;
                paint.contentHash = contentHash;
                paint.staticValue = runtime::CanonicalPaint{
                    .sourceKey = static_cast<std::uint64_t>(source.modelPaint.value) + 1U,
                    .contentHash = contentHash,
                    .stroke = source.localStroke,
                    .paint = source.localPaint,
                };
                ++result.paintsCreated;
            } else {
                paint.resourceClass = ResourceClass::InstanceEvaluated;
            }
            changed = true;
        } else if (paint.resourceClass == ResourceClass::AssetStatic
                   && paint.staticValue && paintAuthoritative) {
            const auto contentHash = hashPaint(source.localStroke, source.localPaint);
            if (!paintCanBeStatic || paint.contentHash != contentHash
                || !sameStroke(paint.staticValue->stroke, source.localStroke)
                || !samePaint(paint.staticValue->paint, source.localPaint)) {
                paint.resourceClass = ResourceClass::InstanceEvaluated;
                paint.contentHash = 0U;
                paint.staticValue.reset();
                ++result.conflicts;
                changed = true;
            }
        }
    }
    if (next->drawOrder != drawOrder) {
        next->drawOrder = std::move(drawOrder);
        changed = true;
    }

    std::vector<LayerId> flattenedChildren;
    std::vector<NodeId> flattenedNodes;
    for (auto& layer : next->layers) {
        if (!layer.present || !layer.id.valid()) continue;
        layer.children.first = static_cast<std::uint32_t>(flattenedChildren.size());
        layer.children.count = static_cast<std::uint32_t>(
            children[layer.id.index()].size());
        flattenedChildren.insert(flattenedChildren.end(),
            children[layer.id.index()].begin(), children[layer.id.index()].end());
        layer.nodes.first = static_cast<std::uint32_t>(flattenedNodes.size());
        layer.nodes.count = static_cast<std::uint32_t>(
            layerNodes[layer.id.index()].size());
        flattenedNodes.insert(flattenedNodes.end(),
            layerNodes[layer.id.index()].begin(), layerNodes[layer.id.index()].end());
    }
    if (next->childLayerIds != flattenedChildren
        || next->layerNodeIds != flattenedNodes) {
        next->childLayerIds = std::move(flattenedChildren);
        next->layerNodeIds = std::move(flattenedNodes);
        changed = true;
    }

    computeStatistics(*next);
    if (changed) {
        next->revision = previous ? previous->revision + 1U : 1U;
        computeFingerprints(*next);
        result.model = std::const_pointer_cast<const MotionAssetModel>(next);
    } else {
        result.model = std::move(previous);
    }
    result.changed = changed;
    return result;
}

AssetModelUpdateResult finalizeAssetModel(
    std::shared_ptr<const MotionAssetModel> model) {
    AssetModelUpdateResult result;
    if (!model) {
        result.error = "cannot finalize a null asset model";
        return result;
    }
    auto next = std::make_shared<MotionAssetModel>(*model);
    bool changed = false;
    for (auto& geometry : next->geometries) {
        if (geometry.present && geometry.resourceClass == ResourceClass::Unknown) {
            geometry.resourceClass = ResourceClass::InstanceEvaluated;
            changed = true;
        }
    }
    for (auto& paint : next->paints) {
        if (paint.present && paint.resourceClass == ResourceClass::Unknown) {
            paint.resourceClass = ResourceClass::InstanceEvaluated;
            changed = true;
        }
    }
    if (changed) {
        next->revision = model->revision + 1U;
        computeStatistics(*next);
        computeFingerprints(*next);
        result.model = std::const_pointer_cast<const MotionAssetModel>(next);
    } else {
        result.model = std::move(model);
    }
    result.changed = changed;
    return result;
}

AssetModelApplyResult applyAssetModel(
    const std::shared_ptr<const MotionAssetModel>& model,
    runtime::EvaluatedScene& scene) {
    AssetModelApplyResult result;
    if (!model) {
        result.error = "cannot apply a null asset model";
        return result;
    }
    if (model->sourceAssetHash != scene.sourceAssetHash) {
        result.error = "evaluated scene and asset model have different source identity";
        return result;
    }

    for (auto& item : scene.drawItems) {
        const auto* node = model->node(item.modelNode);
        const auto* geometry = model->geometry(item.modelGeometry);
        const auto* paint = model->paint(item.modelPaint);
        if (node == nullptr || geometry == nullptr || paint == nullptr) {
            result.error = "evaluated scene references an object absent from frozen model";
            return result;
        }
        item.modelDrawItem = node->drawItem;
        item.sourceGeometryId = static_cast<std::uint64_t>(item.modelGeometry.value) + 1U;
        item.sourcePaintId = static_cast<std::uint64_t>(item.modelPaint.value) + 1U;
        item.canonicalGeometry.reset();
        item.canonicalPaint.reset();

        // A local-space geometry may only be selected when the current
        // evaluated item also exposes a complete local geometry + paint seam.
        // Otherwise the paint could still be in final viewport space and the
        // backend would apply the transform twice.
        if (geometry->resourceClass == ResourceClass::AssetStatic
            && geometry->staticValue
            && item.localGeometryAvailable
            && item.localPaintAvailable) {
            item.geometryOrigin = runtime::EvaluatedValueOrigin::AssetStatic;
            item.canonicalGeometry = std::shared_ptr<const runtime::CanonicalGeometry>(
                model, std::addressof(*geometry->staticValue));
        } else {
            item.geometryOrigin = runtime::EvaluatedValueOrigin::InstanceEvaluated;
        }
        if (paint->resourceClass == ResourceClass::AssetStatic
            && paint->staticValue
            && item.localPaintAvailable) {
            item.paintOrigin = runtime::EvaluatedValueOrigin::AssetStatic;
            item.canonicalPaint = std::shared_ptr<const runtime::CanonicalPaint>(
                model, std::addressof(*paint->staticValue));
        } else {
            item.paintOrigin = runtime::EvaluatedValueOrigin::InstanceEvaluated;
        }
    }
    scene.assetModel = model;
    scene.assetModelApplied = true;
    return result;
}

} // namespace avemotion::model::detail
