#include "avemotion/evaluation/PropertyEvaluator.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

void require(bool condition, const std::string& message) {
    if (!condition) fail(message);
}

bool near(float left, float right, float epsilon = 1.0e-5F) {
    return std::fabs(left - right) <= epsilon;
}

avemotion::model::MotionValueRef scalar(
    avemotion::model::MotionAssetModel& model,
    float value) {
    const auto index = static_cast<std::uint32_t>(model.scalarValues.size());
    model.scalarValues.push_back(value);
    return {avemotion::model::PropertyValueType::Scalar, index};
}

avemotion::model::MotionValueRef vec2(
    avemotion::model::MotionAssetModel& model,
    float x,
    float y) {
    const auto index = static_cast<std::uint32_t>(model.vec2Values.size());
    model.vec2Values.push_back({x, y});
    return {avemotion::model::PropertyValueType::Vec2, index};
}

avemotion::model::MotionValueRef color(
    avemotion::model::MotionAssetModel& model,
    float r,
    float g,
    float b,
    float a = 1.0F) {
    const auto index = static_cast<std::uint32_t>(model.colorValues.size());
    model.colorValues.push_back({r, g, b, a});
    return {avemotion::model::PropertyValueType::Color, index};
}

avemotion::model::MotionValueRef shape(
    avemotion::model::MotionAssetModel& model,
    std::vector<avemotion::model::MotionVec2Value> points,
    bool closed) {
    const auto first = static_cast<std::uint32_t>(model.shapePoints.size());
    const auto count = static_cast<std::uint32_t>(points.size());
    model.shapePoints.insert(
        model.shapePoints.end(), points.begin(), points.end());
    const auto index = static_cast<std::uint32_t>(model.shapeValues.size());
    model.shapeValues.push_back({{first, count}, closed});
    return {avemotion::model::PropertyValueType::Shape, index};
}

avemotion::model::PropertyId addStatic(
    avemotion::model::MotionAssetModel& model,
    avemotion::model::PropertySemantic semantic,
    avemotion::model::MotionValueRef value,
    avemotion::model::SourceNodeId owner = {0U}) {
    using namespace avemotion::model;
    const auto id = makeId<PropertyId>(model.properties.size());
    model.properties.push_back({
        .present = true,
        .id = id,
        .owner = owner,
        .semantic = semantic,
        .valueType = value.type,
        .flags = PropertyFlagStatic,
        .staticValue = value,
        .track = {},
    });
    model.sourcePropertyIds.push_back(id);
    return id;
}

avemotion::model::PropertyId addAnimated(
    avemotion::model::MotionAssetModel& model,
    avemotion::model::PropertySemantic semantic,
    avemotion::model::MotionValueRef start,
    avemotion::model::MotionValueRef end,
    avemotion::model::SegmentInterpolation interpolation,
    avemotion::model::MotionVec2Value c1 = {0.0F, 0.0F},
    avemotion::model::MotionVec2Value c2 = {1.0F, 1.0F},
    avemotion::model::SpatialInterpolation spatial =
        avemotion::model::SpatialInterpolation::None,
    avemotion::model::MotionVec2Value spatialIn = {},
    avemotion::model::MotionVec2Value spatialOut = {}) {
    using namespace avemotion::model;
    const auto propertyId = makeId<PropertyId>(model.properties.size());
    const auto trackId = makeId<TrackId>(model.tracks.size());
    const auto segmentId = makeId<SegmentId>(model.segments.size());
    std::uint32_t flags = PropertyFlagAnimated;
    if (spatial != SpatialInterpolation::None) flags |= PropertyFlagSpatial;
    model.properties.push_back({
        .present = true,
        .id = propertyId,
        .owner = SourceNodeId{0U},
        .semantic = semantic,
        .valueType = start.type,
        .flags = flags,
        .staticValue = {},
        .track = trackId,
    });
    model.tracks.push_back({
        .present = true,
        .id = trackId,
        .property = propertyId,
        .segments = {segmentId.index(), 1U},
        .firstFrame = 0.0,
        .endFrame = 10.0,
    });
    model.segments.push_back({
        .present = true,
        .id = segmentId,
        .track = trackId,
        .firstFrame = 0.0,
        .endFrame = 10.0,
        .interpolation = interpolation,
        .spatialInterpolation = spatial,
        .startValue = start,
        .endValue = end,
        .temporalControl1 = c1,
        .temporalControl2 = c2,
        .spatialInTangent = spatialIn,
        .spatialOutTangent = spatialOut,
    });
    model.sourcePropertyIds.push_back(propertyId);
    return propertyId;
}

avemotion::model::PropertyId addTwoSegmentConstant(
    avemotion::model::MotionAssetModel& model,
    avemotion::model::PropertySemantic semantic,
    float value) {
    using namespace avemotion::model;
    const auto propertyId = makeId<PropertyId>(model.properties.size());
    const auto trackId = makeId<TrackId>(model.tracks.size());
    const auto firstSegmentId = makeId<SegmentId>(model.segments.size());
    const auto firstStart = scalar(model, value);
    const auto firstEnd = scalar(model, value);
    const auto secondStart = scalar(model, value);
    const auto secondEnd = scalar(model, value);
    model.properties.push_back({
        .present = true,
        .id = propertyId,
        .owner = SourceNodeId{0U},
        .semantic = semantic,
        .valueType = PropertyValueType::Scalar,
        .flags = PropertyFlagAnimated,
        .staticValue = {},
        .track = trackId,
    });
    model.tracks.push_back({
        .present = true,
        .id = trackId,
        .property = propertyId,
        .segments = {firstSegmentId.index(), 2U},
        .firstFrame = 0.0,
        .endFrame = 10.0,
    });
    model.segments.push_back({
        .present = true,
        .id = firstSegmentId,
        .track = trackId,
        .firstFrame = 0.0,
        .endFrame = 5.0,
        .interpolation = SegmentInterpolation::Linear,
        .spatialInterpolation = SpatialInterpolation::None,
        .startValue = firstStart,
        .endValue = firstEnd,
        .temporalControl1 = {0.0F, 0.0F},
        .temporalControl2 = {1.0F, 1.0F},
        .spatialInTangent = {},
        .spatialOutTangent = {},
    });
    const auto secondSegmentId = makeId<SegmentId>(model.segments.size());
    model.segments.push_back({
        .present = true,
        .id = secondSegmentId,
        .track = trackId,
        .firstFrame = 5.0,
        .endFrame = 10.0,
        .interpolation = SegmentInterpolation::Linear,
        .spatialInterpolation = SpatialInterpolation::None,
        .startValue = secondStart,
        .endValue = secondEnd,
        .temporalControl1 = {0.0F, 0.0F},
        .temporalControl2 = {1.0F, 1.0F},
        .spatialInTangent = {},
        .spatialOutTangent = {},
    });
    model.sourcePropertyIds.push_back(propertyId);
    return propertyId;
}

std::shared_ptr<const avemotion::model::MotionAssetModel> makeModel() {
    using namespace avemotion::model;
    auto model = std::make_shared<MotionAssetModel>();
    model->statistics.directParsedModel = true;
    model->parsedModelFingerprint = 0x12345678U;
    model->sourceAssetHash = 0xABCDEFU;
    MotionSourceNodeRecord rootNode;
    rootNode.present = true;
    rootNode.id = SourceNodeId{0U};
    rootNode.composition = CompositionId{0U};
    rootNode.kind = SourceNodeKind::Layer;
    model->sourceNodes.push_back(std::move(rootNode));
    model->compositions.push_back({
        .present = true,
        .id = CompositionId{0U},
        .rootNode = SourceNodeId{0U},
        .debugName = "synthetic",
        .logicalWidth = 100U,
        .logicalHeight = 100U,
        .firstFrame = 0.0,
        .endFrame = 10.0,
        .frameRate = 60.0,
    });

    addStatic(*model, PropertySemantic::FillOpacity, scalar(*model, 5.0F));
    addAnimated(
        *model, PropertySemantic::StrokeWidth,
        scalar(*model, 1.0F), scalar(*model, 9.0F),
        SegmentInterpolation::Hold);
    addAnimated(
        *model, PropertySemantic::TrimStart,
        scalar(*model, 0.0F), scalar(*model, 10.0F),
        SegmentInterpolation::Linear);
    addAnimated(
        *model, PropertySemantic::TrimEnd,
        scalar(*model, 0.0F), scalar(*model, 1.0F),
        SegmentInterpolation::CubicBezier,
        {0.25F, 0.1F}, {0.25F, 1.0F});
    addAnimated(
        *model, PropertySemantic::RectangleSize,
        vec2(*model, 0.0F, 0.0F), vec2(*model, 10.0F, 20.0F),
        SegmentInterpolation::Linear);
    addAnimated(
        *model, PropertySemantic::FillColor,
        color(*model, 1.0F, 0.0F, 0.0F),
        color(*model, 0.0F, 0.0F, 1.0F),
        SegmentInterpolation::Linear);

    addAnimated(
        *model, PropertySemantic::TransformPosition,
        vec2(*model, 0.0F, 0.0F), vec2(*model, 10.0F, 20.0F),
        SegmentInterpolation::Linear);
    addStatic(*model, PropertySemantic::TransformScale,
              vec2(*model, 100.0F, 100.0F));
    addAnimated(
        *model, PropertySemantic::TransformRotation,
        scalar(*model, 0.0F), scalar(*model, 90.0F),
        SegmentInterpolation::Linear);
    addStatic(*model, PropertySemantic::TransformAnchor,
              vec2(*model, 0.0F, 0.0F));
    addStatic(*model, PropertySemantic::TransformOpacity,
              scalar(*model, 50.0F));

    addAnimated(
        *model, PropertySemantic::EllipsePosition,
        vec2(*model, 0.0F, 0.0F), vec2(*model, 5.0F, 5.0F),
        SegmentInterpolation::Linear,
        {0.0F, 0.0F}, {1.0F, 1.0F},
        SpatialInterpolation::CubicBezier,
        {-3.0F, 0.0F},
        {8.0F, 0.0F});
    addTwoSegmentConstant(
        *model, PropertySemantic::RepeaterCopies, 3.0F);

    model->sourceNodes[0].properties = {
        0U, static_cast<std::uint32_t>(model->sourcePropertyIds.size())};
    model->statistics.propertyCount = model->properties.size();
    model->statistics.trackCount = model->tracks.size();
    model->statistics.segmentCount = model->segments.size();
    return model;
}

std::shared_ptr<const avemotion::model::MotionAssetModel> makeShapeModel() {
    using namespace avemotion::model;
    auto model = std::make_shared<MotionAssetModel>();
    model->statistics.directParsedModel = true;
    model->parsedModelFingerprint = 0x5348415045ULL;
    model->sourceAssetHash = 0x50415448ULL;

    MotionSourceNodeRecord root;
    root.present = true;
    root.id = SourceNodeId{0U};
    root.composition = CompositionId{0U};
    root.kind = SourceNodeKind::Shape;
    model->sourceNodes.push_back(root);
    model->compositions.push_back({
        .present = true,
        .id = CompositionId{0U},
        .rootNode = SourceNodeId{0U},
        .debugName = "shape-synthetic",
        .logicalWidth = 100U,
        .logicalHeight = 100U,
        .firstFrame = 0.0,
        .endFrame = 10.0,
        .frameRate = 60.0,
    });

    const auto start = shape(*model, {
        {0.0F, 0.0F},
        {0.0F, 0.0F},
        {10.0F, 0.0F},
        {10.0F, 10.0F},
    }, true);
    const auto end = shape(*model, {
        {0.0F, 0.0F},
        {0.0F, 10.0F},
        {10.0F, 10.0F},
        {20.0F, 20.0F},
    }, true);
    const auto longer = shape(*model, {
        {0.0F, 0.0F},
        {0.0F, 5.0F},
        {5.0F, 5.0F},
        {10.0F, 10.0F},
        {12.0F, 12.0F},
        {14.0F, 14.0F},
        {16.0F, 16.0F},
    }, false);

    addStatic(*model, PropertySemantic::ShapePath, start);
    addAnimated(
        *model, PropertySemantic::ShapePath, start, end,
        SegmentInterpolation::Linear);
    addAnimated(
        *model, PropertySemantic::MaskPath, start, end,
        SegmentInterpolation::Hold);
    addAnimated(
        *model, PropertySemantic::ShapePath, start, longer,
        SegmentInterpolation::Linear);

    model->sourceNodes[0].properties = {
        0U, static_cast<std::uint32_t>(model->sourcePropertyIds.size())};
    model->statistics.sourceNodeCount = model->sourceNodes.size();
    model->statistics.propertyCount = model->properties.size();
    model->statistics.staticPropertyCount = 1U;
    model->statistics.animatedPropertyCount = 3U;
    model->statistics.trackCount = model->tracks.size();
    model->statistics.segmentCount = model->segments.size();
    model->statistics.shapeValueCount = model->shapeValues.size();
    return model;
}

struct ShapeView final {
    std::span<const avemotion::model::MotionVec2Value> points;
    const avemotion::evaluation::EvaluatedShape* descriptor = nullptr;
};

ShapeView evaluatedShape(
    const avemotion::evaluation::PropertyEvaluationView& view,
    std::size_t propertyIndex) {
    require(propertyIndex < view.properties.size(), "shape property index out of range");
    const auto& value = view.properties[propertyIndex].value;
    require(value.type == avemotion::model::PropertyValueType::Shape
                && value.materialized(),
            "shape property was not materialized");
    require(value.shapeSlot < view.shapes.size(), "shape slot out of range");
    const auto& shape = view.shapes[value.shapeSlot];
    require(static_cast<std::size_t>(shape.firstPoint) + shape.pointCount
                <= view.shapePoints.size(),
            "shape point range out of bounds");
    return {
        view.shapePoints.subspan(shape.firstPoint, shape.pointCount),
        &shape,
    };
}

void requireSameShape(
    const ShapeView& left,
    const ShapeView& right,
    const std::string& message) {
    require(left.descriptor != nullptr && right.descriptor != nullptr,
            message + ": missing descriptor");
    require(left.descriptor->closed == right.descriptor->closed,
            message + ": closed differs");
    require(left.descriptor->topologyTruncated
                == right.descriptor->topologyTruncated,
            message + ": truncation differs");
    require(left.points.size() == right.points.size(),
            message + ": point count differs");
    for (std::size_t index = 0; index < left.points.size(); ++index) {
        require(left.points[index] == right.points[index],
                message + ": point differs at " + std::to_string(index));
    }
}

void testShapeEvaluation() {
    const auto model = makeShapeModel();
    avemotion::evaluation::PropertyEvaluator evaluator{model};
    require(evaluator.valid(), std::string{evaluator.errorMessage()});

    avemotion::evaluation::PropertyEvaluationWorkspace workspace;
    evaluator.prepare(workspace);
    require(workspace.shapeCount() == 3U,
            "animated shape workspace count differs");
    require(workspace.shapePointCapacity() == 15U,
            "animated shape point capacity differs");
    const auto storageGeneration = workspace.storageGeneration();

    const auto start = evaluator.evaluate(0.0, workspace);
    require(static_cast<bool>(start), "shape start evaluation failed");
    require(start.statistics.staticShapeReferences == 1U,
            "static shape reference was not counted");
    const auto startLinear = evaluatedShape(start, 1U);
    require(startLinear.descriptor->closed,
            "before-first shape endpoint lost closed state");
    require(startLinear.points.size() == 4U,
            "shape start point count differs");
    require(startLinear.points.back()
                == avemotion::model::MotionVec2Value{10.0F, 10.0F},
            "shape start endpoint differs");

    const auto middle = evaluator.evaluate(5.0, workspace);
    require(static_cast<bool>(middle), "shape midpoint evaluation failed");
    const auto linear = evaluatedShape(middle, 1U);
    require(!linear.descriptor->closed,
            "interpolated Telegram shape unexpectedly preserved closed state");
    require(linear.points.size() == 4U, "linear shape point count differs");
    require(linear.points[1] == avemotion::model::MotionVec2Value{0.0F, 5.0F}
                && linear.points[3]
                    == avemotion::model::MotionVec2Value{15.0F, 15.0F},
            "linear shape midpoint differs");

    const auto hold = evaluatedShape(middle, 2U);
    require(!hold.descriptor->closed,
            "active hold shape did not match Telegram closed semantics");
    require(hold.points.back()
                == avemotion::model::MotionVec2Value{10.0F, 10.0F},
            "hold shape did not preserve start points");

    const auto truncated = evaluatedShape(middle, 3U);
    require(truncated.descriptor->topologyTruncated,
            "mismatched path topology was not reported");
    require(truncated.points.size() == 4U,
            "mismatched path was not truncated to Telegram minimum");
    require(middle.statistics.shapeTopologyTruncations == 1U,
            "shape topology truncation statistic differs");
    require(middle.statistics.shapePointInterpolations == 12U,
            "shape point interpolation count differs");

    const auto linearRevision = linear.descriptor->revision;
    const auto repeated = evaluator.evaluate(5.0, workspace);
    require(static_cast<bool>(repeated), "repeated shape evaluation failed");
    require(repeated.statistics.changedShapes == 0U
                && repeated.statistics.changedProperties == 0U,
            "same-time shape evaluation reported changes");
    require(evaluatedShape(repeated, 1U).descriptor->revision == linearRevision,
            "same-time shape evaluation advanced geometry revision");
    require(workspace.storageGeneration() == storageGeneration,
            "shape evaluation resized retained workspace storage");

    const auto after = evaluator.evaluate(10.0, workspace);
    require(static_cast<bool>(after), "shape endpoint evaluation failed");
    const auto endShape = evaluatedShape(after, 1U);
    require(endShape.descriptor->closed,
            "after-last shape endpoint lost closed state");
    require(endShape.points.back()
                == avemotion::model::MotionVec2Value{20.0F, 20.0F},
            "shape endpoint differs");

    avemotion::evaluation::PropertyEvaluationWorkspace sequentialWorkspace;
    avemotion::evaluation::PropertyEvaluationWorkspace directWorkspace;
    evaluator.prepare(sequentialWorkspace);
    evaluator.prepare(directWorkspace);
    (void)evaluator.evaluate(1.0, sequentialWorkspace);
    (void)evaluator.evaluate(3.0, sequentialWorkspace);
    const auto sequential = evaluator.evaluate(6.0, sequentialWorkspace);
    const auto direct = evaluator.evaluate(6.0, directWorkspace);
    require(static_cast<bool>(sequential) && static_cast<bool>(direct),
            "shape direct/sequential evaluation failed");
    for (std::size_t property = 1U; property <= 3U; ++property) {
        requireSameShape(
            evaluatedShape(sequential, property),
            evaluatedShape(direct, property),
            "shape direct/sequential parity");
    }

    auto corrupt = std::make_shared<avemotion::model::MotionAssetModel>(*model);
    corrupt->shapeValues.front().points.count = 2U;
    avemotion::evaluation::PropertyEvaluator corruptEvaluator{corrupt};
    require(!corruptEvaluator.valid(),
            "invalid MoveTo+cubic shape encoding was accepted");
}

std::shared_ptr<const avemotion::model::MotionAssetModel> makeHierarchyModel() {
    using namespace avemotion::model;
    auto model = std::make_shared<MotionAssetModel>();
    model->statistics.directParsedModel = true;
    model->parsedModelFingerprint = 0x98765432U;
    model->sourceAssetHash = 0x1234ABCDU;

    model->sourceNodes.resize(4U);
    for (std::uint32_t index = 0; index < model->sourceNodes.size(); ++index) {
        auto& node = model->sourceNodes[index];
        node.present = true;
        node.id = SourceNodeId{index};
        node.composition = CompositionId{0U};
    }
    model->sourceNodes[0].kind = SourceNodeKind::Composition;
    model->sourceNodes[0].children = {0U, 2U};

    model->sourceNodes[1].kind = SourceNodeKind::Layer;
    model->sourceNodes[1].layerKind = SourceLayerKind::Null;
    model->sourceNodes[1].parent = SourceNodeId{0U};

    model->sourceNodes[2].kind = SourceNodeKind::Layer;
    model->sourceNodes[2].layerKind = SourceLayerKind::Null;
    model->sourceNodes[2].parent = SourceNodeId{0U};
    model->sourceNodes[2].transformParent = SourceNodeId{1U};
    model->sourceNodes[2].children = {2U, 1U};

    model->sourceNodes[3].kind = SourceNodeKind::ShapeGroup;
    model->sourceNodes[3].parent = SourceNodeId{2U};

    model->sourceChildIds = {SourceNodeId{1U}, SourceNodeId{2U}, SourceNodeId{3U}};
    model->compositions.push_back({
        .present = true,
        .id = CompositionId{0U},
        .rootNode = SourceNodeId{0U},
        .debugName = "hierarchy",
        .logicalWidth = 100U,
        .logicalHeight = 100U,
        .firstFrame = 0.0,
        .endFrame = 10.0,
        .frameRate = 60.0,
    });

    const auto addNodeTransform = [&](SourceNodeId owner,
                                      MotionVec2Value position,
                                      float opacity) {
        auto& node = model->sourceNodes[owner.index()];
        const auto first = static_cast<std::uint32_t>(model->sourcePropertyIds.size());
        addStatic(*model, PropertySemantic::TransformPosition,
                  vec2(*model, position.x, position.y), owner);
        addStatic(*model, PropertySemantic::TransformOpacity,
                  scalar(*model, opacity), owner);
        node.properties = {first, 2U};
    };

    addNodeTransform(SourceNodeId{1U}, {10.0F, 0.0F}, 50.0F);
    addNodeTransform(SourceNodeId{2U}, {5.0F, 0.0F}, 80.0F);
    addNodeTransform(SourceNodeId{3U}, {0.0F, 2.0F}, 50.0F);

    model->statistics.sourceNodeCount = model->sourceNodes.size();
    model->statistics.propertyCount = model->properties.size();
    model->statistics.staticPropertyCount = model->properties.size();
    return model;
}

void testWorldHierarchy() {
    const auto model = makeHierarchyModel();
    avemotion::evaluation::PropertyEvaluator evaluator{model};
    require(evaluator.valid(), std::string{evaluator.errorMessage()});
    avemotion::evaluation::PropertyEvaluationWorkspace workspace;
    evaluator.prepare(workspace);

    const auto first = evaluator.evaluate(0.0, workspace);
    require(static_cast<bool>(first), "hierarchy evaluation failed");
    require(first.nodeTransforms.size() == 4U,
            "hierarchy transform count differs");

    const auto& matrixParent = first.nodeTransforms[1];
    require(near(matrixParent.worldMatrix.dx, 10.0F)
                && near(matrixParent.worldMatrix.dy, 0.0F),
            "matrix parent world translation differs");
    require(near(matrixParent.worldOpacity, 0.5F),
            "matrix parent world opacity differs");

    const auto& parentedLayer = first.nodeTransforms[2];
    require(near(parentedLayer.worldMatrix.dx, 15.0F)
                && near(parentedLayer.worldMatrix.dy, 0.0F),
            "transform-parent matrix was not inherited");
    require(near(parentedLayer.worldOpacity, 0.8F),
            "layer opacity incorrectly inherited from transform parent");

    const auto& group = first.nodeTransforms[3];
    require(near(group.worldMatrix.dx, 15.0F)
                && near(group.worldMatrix.dy, 2.0F),
            "structural child world transform differs");
    require(near(group.worldOpacity, 0.4F),
            "structural opacity inheritance differs");
    require(first.statistics.transformParentEdges == 1U,
            "transform-parent edge was not counted");
    require(first.statistics.worldTransformsUnsupported == 0U,
            "supported hierarchy produced unsupported world transforms");

    const auto repeated = evaluator.evaluate(0.0, workspace);
    require(repeated && repeated.statistics.worldTransformsChanged == 0U,
            "same-time hierarchy evaluation reported world changes");
}

void testConcurrentWorkspaces(
    const std::shared_ptr<const avemotion::model::MotionAssetModel>& model) {
    avemotion::evaluation::PropertyEvaluator evaluator{model};
    require(evaluator.valid(), std::string{evaluator.errorMessage()});
    std::vector<std::uint64_t> revisions(2U, 0U);
    std::vector<std::thread> workers;
    workers.reserve(2U);
    for (std::size_t worker = 0; worker < 2U; ++worker) {
        workers.emplace_back([&, worker] {
            avemotion::evaluation::PropertyEvaluationWorkspace workspace;
            evaluator.prepare(workspace);
            for (int iteration = 0; iteration < 200; ++iteration) {
                const double frame = static_cast<double>((iteration * 7) % 11);
                const auto result = evaluator.evaluate(frame, workspace);
                if (!result) return;
            }
            const auto final = evaluator.evaluate(5.0, workspace);
            if (final && !final.properties.empty()) {
                revisions[worker] = final.properties[3].revision;
            }
        });
    }
    for (auto& worker : workers) worker.join();
    require(revisions[0] != 0U && revisions[0] == revisions[1],
            "concurrent workspaces produced different deterministic histories");
}

} // namespace

int main() {
    testShapeEvaluation();
    const auto model = makeModel();
    testWorldHierarchy();
    testConcurrentWorkspaces(model);
    avemotion::evaluation::PropertyEvaluator evaluator{model};
    require(evaluator.valid(), std::string{evaluator.errorMessage()});

    avemotion::evaluation::PropertyEvaluationWorkspace workspace;
    evaluator.prepare(workspace);
    require(workspace.propertyCount() == model->properties.size(),
            "workspace property count differs");
    require(workspace.trackCursorCount() == model->tracks.size(),
            "workspace cursor count differs");
    const auto generation = workspace.storageGeneration();

    auto first = evaluator.evaluate(0.0, workspace);
    require(static_cast<bool>(first), "frame zero evaluation failed");
    require(first.statistics.changedProperties == model->properties.size(),
            "first evaluation must publish all properties");
    require(first.properties[0].value.scalar == 5.0F,
            "static scalar differs");
    require(first.properties[1].value.scalar == 1.0F,
            "hold start differs");
    require(first.properties[2].value.scalar == 0.0F,
            "linear start differs");
    require(first.nodeTransforms[0].supported(),
            "synthetic 2D transform is unsupported");
    require(near(first.nodeTransforms[0].localOpacity, 0.5F),
            "transform opacity differs");
    require(near(first.nodeTransforms[0].localMatrix.m11, 1.0F)
                && near(first.nodeTransforms[0].localMatrix.m22, 1.0F)
                && near(first.nodeTransforms[0].localMatrix.dx, 0.0F)
                && near(first.nodeTransforms[0].localMatrix.dy, 0.0F),
            "frame-zero transform is not identity");

    auto middle = evaluator.evaluate(5.0, workspace);
    require(static_cast<bool>(middle), "middle evaluation failed");
    require(middle.properties[1].value.scalar == 1.0F,
            "hold interpolation changed inside segment");
    require(near(middle.properties[2].value.scalar, 5.0F),
            "linear scalar midpoint differs");
    require(middle.properties[3].value.scalar > 0.5F
                && middle.properties[3].value.scalar < 1.0F,
            "cubic easing midpoint is outside expected range");
    require(near(middle.properties[4].value.vec2.x, 5.0F)
                && near(middle.properties[4].value.vec2.y, 10.0F),
            "Vec2 interpolation differs");
    require(near(middle.properties[5].value.color.r, 0.5F)
                && near(middle.properties[5].value.color.b, 0.5F),
            "color interpolation differs");
    require(middle.properties[11].value.materialized(),
            "spatial track was not materialized");
    require(middle.statistics.spatialSamples == 1U,
            "spatial sample was not recorded");
    require(middle.properties[11].spatialAngleValid,
            "spatial tangent angle was not published");
    require(near(middle.properties[11].value.vec2.x, 4.15553F, 2.0e-4F)
                && near(middle.properties[11].value.vec2.y, 1.24479F, 2.0e-4F),
            "spatial midpoint differs from the expected arc-length sample");

    auto repeated = evaluator.evaluate(5.0, workspace);
    require(static_cast<bool>(repeated), "repeated evaluation failed");
    require(repeated.statistics.changedProperties == 0U,
            "same-time evaluation reported property changes");
    require(repeated.statistics.transformsChanged == 0U,
            "same-time evaluation reported transform changes");
    require(repeated.statistics.worldTransformsChanged == 0U,
            "same-time evaluation reported world-transform changes");
    require(workspace.storageGeneration() == generation,
            "steady evaluation resized workspace storage");

    auto adjacent = evaluator.evaluate(6.0, workspace);
    require(static_cast<bool>(adjacent), "adjacent evaluation failed");
    require(adjacent.statistics.cursorHits + adjacent.statistics.adjacentCursorMoves > 0U,
            "cursor fast path was not used");

    avemotion::evaluation::PropertyEvaluationWorkspace directWorkspace;
    evaluator.prepare(directWorkspace);
    const auto direct = evaluator.evaluate(6.0, directWorkspace);
    require(static_cast<bool>(direct), "direct seek evaluation failed");
    require(direct.statistics.binarySearches > 0U,
            "direct random seek did not use binary-search fallback");
    require(direct.properties.size() == adjacent.properties.size(),
            "direct and sequential property counts differ");
    for (std::size_t index = 0; index < direct.properties.size(); ++index) {
        require(direct.properties[index].value == adjacent.properties[index].value,
                "direct seek differs from sequential evaluation at property "
                    + std::to_string(index));
    }

    const auto before = evaluator.evaluate(-1.0, workspace);
    require(before && before.properties[2].value.scalar == 0.0F,
            "before-first sampling differs");
    const auto after = evaluator.evaluate(11.0, workspace);
    require(after && after.properties[2].value.scalar == 10.0F,
            "after-last sampling differs");

    avemotion::evaluation::PropertyEvaluationWorkspace boundaryWorkspace;
    evaluator.prepare(boundaryWorkspace);
    const auto beforeBoundary = evaluator.evaluate(4.0, boundaryWorkspace);
    require(static_cast<bool>(beforeBoundary), "pre-boundary evaluation failed");
    const auto constantPropertyIndex = model->properties.size() - 1U;
    const auto constantRevision = beforeBoundary.properties[constantPropertyIndex].revision;
    const auto onBoundary = evaluator.evaluate(5.0, boundaryWorkspace);
    require(static_cast<bool>(onBoundary), "boundary evaluation failed");
    require(!onBoundary.properties[constantPropertyIndex].changed,
            "equal values across segment boundary caused a false dirty result");
    require(onBoundary.properties[constantPropertyIndex].revision == constantRevision,
            "equal segment-boundary value advanced its visual revision");

    const auto nonFinite = evaluator.evaluate(
        std::numeric_limits<double>::infinity(), workspace);
    require(nonFinite.error
                == avemotion::evaluation::PropertyEvaluationErrorCode::NonFiniteFrame,
            "non-finite frame was not rejected");

    avemotion::evaluation::PropertyEvaluator nullEvaluator{nullptr};
    require(!nullEvaluator.valid(), "null evaluator unexpectedly valid");
    avemotion::evaluation::PropertyEvaluationWorkspace nullWorkspace;
    nullEvaluator.prepare(nullWorkspace);
    require(nullWorkspace.propertyCount() == 0U,
            "invalid evaluator prepared property storage");
    const auto nullResult = nullEvaluator.evaluate(0.0, nullWorkspace);
    require(nullResult.error
                == avemotion::evaluation::PropertyEvaluationErrorCode::InvalidModel,
            "invalid evaluator did not return InvalidModel");

    auto corruptModel = std::make_shared<avemotion::model::MotionAssetModel>(*model);
    corruptModel->segments.front().endFrame = -1.0;
    avemotion::evaluation::PropertyEvaluator corruptEvaluator{corruptModel};
    require(!corruptEvaluator.valid(), "corrupt segment table was accepted");

    std::cout << "AveMotion canonical property evaluator tests passed\n";
    return EXIT_SUCCESS;
}
