#include "avemotion/model/AssetModel.hpp"

namespace avemotion::model {
namespace {

template <typename Record, typename Id>
[[nodiscard]] const Record* findPresent(
    const std::vector<Record>& values,
    Id id) noexcept {
    if (!id.valid() || id.index() >= values.size()) {
        return nullptr;
    }
    const auto& value = values[id.index()];
    return value.present ? &value : nullptr;
}

} // namespace

const MotionLayerRecord* MotionAssetModel::layer(LayerId id) const noexcept {
    return findPresent<MotionLayerRecord>(layers, id);
}

const MotionNodeRecord* MotionAssetModel::node(NodeId id) const noexcept {
    return findPresent<MotionNodeRecord>(nodes, id);
}

const MotionGeometryRecord* MotionAssetModel::geometry(
    GeometryId id) const noexcept {
    return findPresent<MotionGeometryRecord>(geometries, id);
}

const MotionPaintRecord* MotionAssetModel::paint(PaintId id) const noexcept {
    return findPresent<MotionPaintRecord>(paints, id);
}

const MotionClipRecord* MotionAssetModel::clip(ClipId id) const noexcept {
    return findPresent<MotionClipRecord>(clips, id);
}

const MotionCompositionRecord* MotionAssetModel::composition(
    CompositionId id) const noexcept {
    return findPresent<MotionCompositionRecord>(compositions, id);
}

const MotionSourceNodeRecord* MotionAssetModel::sourceNode(
    SourceNodeId id) const noexcept {
    return findPresent<MotionSourceNodeRecord>(sourceNodes, id);
}

const MotionPropertyRecord* MotionAssetModel::property(
    PropertyId id) const noexcept {
    return findPresent<MotionPropertyRecord>(properties, id);
}

const MotionTrackRecord* MotionAssetModel::track(TrackId id) const noexcept {
    return findPresent<MotionTrackRecord>(tracks, id);
}

const MotionSegmentRecord* MotionAssetModel::segment(
    SegmentId id) const noexcept {
    return findPresent<MotionSegmentRecord>(segments, id);
}

} // namespace avemotion::model
