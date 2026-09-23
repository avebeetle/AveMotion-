#include "avemotion/render/HeadlessBackend.hpp"
#include "avemotion/render/RenderPlanner.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

namespace {

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

void require(bool condition, const std::string& message) {
    if (!condition) {
        fail(message);
    }
}

avemotion::runtime::EvaluatedScene makeScene(
    std::uint64_t instanceId,
    std::uint64_t sequence,
    float right,
    avemotion::runtime::Color8 color,
    avemotion::runtime::EvaluatedValueOrigin geometryOrigin,
    avemotion::runtime::EvaluatedValueOrigin paintOrigin) {
    using namespace avemotion::runtime;
    EvaluatedScene scene;
    scene.sourceAssetHash = 0x123456789ABCDEF0ULL;
    scene.instanceId = instanceId;
    scene.evaluationSequence = sequence;
    scene.frameIndex = static_cast<std::size_t>(sequence);
    scene.viewportWidth = 100U;
    scene.viewportHeight = 100U;

    EvaluatedLayer layer;
    layer.visible = true;
    layer.opacity = 1.0F;
    layer.firstDrawItem = 0U;
    layer.drawItemCount = 1U;
    layer.keyPath = "root.shape";
    scene.layers.push_back(layer);

    EvaluatedDrawItem item;
    item.sourceGeometryId = 11U;
    item.sourcePaintId = 22U;
    item.geometryOrigin = geometryOrigin;
    item.paintOrigin = paintOrigin;
    item.layerIndex = 0U;
    item.drawOrder = 0U;
    item.paint.kind = PaintKind::Solid;
    item.paint.solid = color;
    item.path.verbs = {
        PathVerb::MoveTo,
        PathVerb::LineTo,
        PathVerb::LineTo,
        PathVerb::LineTo,
        PathVerb::Close,
    };
    item.path.points = {
        {0.0F, 0.0F},
        {right, 0.0F},
        {right, 10.0F},
        {0.0F, 10.0F},
    };
    item.path.controlBounds = {true, 0.0F, 0.0F, right, 10.0F};
    scene.drawItems.push_back(item);
    scene.controlBounds = item.path.controlBounds;
    scene.statistics.layerCount = 1U;
    scene.statistics.visibleLayerCount = 1U;
    scene.statistics.drawItemCount = 1U;
    scene.statistics.pathVerbCount = item.path.verbs.size();
    scene.statistics.pathPointCount = item.path.points.size();
    scene.statistics.solidPaintCount = 1U;
    return scene;
}


avemotion::runtime::EvaluatedScene makeLocalSpaceScene(
    std::uint64_t instanceId,
    std::uint64_t sequence,
    bool includeLocalPaint) {
    using namespace avemotion::runtime;
    auto scene = makeScene(
        instanceId,
        sequence,
        50.0F,
        Color8{240U, 160U, 80U, 255U},
        EvaluatedValueOrigin::InstanceEvaluated,
        EvaluatedValueOrigin::InstanceEvaluated);
    auto& item = scene.drawItems[0];

    // The legacy/final path is already in viewport space.
    item.path.points = {
        {40.0F, 0.0F},
        {50.0F, 0.0F},
        {50.0F, 10.0F},
        {40.0F, 10.0F},
    };
    item.path.controlBounds = {true, 40.0F, 0.0F, 50.0F, 10.0F};
    scene.controlBounds = item.path.controlBounds;

    // The extracted path is in local space and needs a separate transform.
    item.localGeometryAvailable = true;
    item.localGeometryStaticCandidate = true;
    item.localToViewport.dx = 40.0F;
    item.localPath.verbs = item.path.verbs;
    item.localPath.points = {
        {0.0F, 0.0F},
        {10.0F, 0.0F},
        {10.0F, 10.0F},
        {0.0F, 10.0F},
    };
    item.localPath.controlBounds = {true, 0.0F, 0.0F, 10.0F, 10.0F};

    // Local geometry is safe only together with paint expressed in the same
    // coordinate space. This models the Part 5 complete-seam invariant.
    item.localPaintAvailable = includeLocalPaint;
    item.localPaintStaticCandidate = includeLocalPaint;
    if (includeLocalPaint) {
        item.localPaint = item.paint;
        item.localStroke = item.stroke;
    }
    return scene;
}

bool matrixIsIdentity(const avemotion::render::Matrix3x2& value) {
    return value.m11 == 1.0F && value.m12 == 0.0F
        && value.m21 == 0.0F && value.m22 == 1.0F
        && value.dx == 0.0F && value.dy == 0.0F;
}

} // namespace

int main() {
    using namespace avemotion;
    render::MotionRenderPlanner planner;
    render::HeadlessPlanBackend headless;

    const auto white = runtime::Color8{255U, 255U, 255U, 255U};
    auto first = planner.build(makeScene(
        1U,
        1U,
        10.0F,
        white,
        runtime::EvaluatedValueOrigin::AssetStatic,
        runtime::EvaluatedValueOrigin::AssetStatic));
    require(static_cast<bool>(first), "first plan build failed");
    require(first.plan.drawItems.size() == 1U, "first plan has wrong item count");
    require(first.plan.geometryUpdates.size() == 1U, "first geometry update missing");
    require(first.plan.paintUpdates.size() == 1U, "first paint update missing");
    require(first.plan.drawItems[0].geometry.scope
                == render::ResourceIdentityScope::Asset,
            "static geometry was not asset-scoped");
    require(first.plan.drawItems[0].paint.scope
                == render::ResourceIdentityScope::Asset,
            "static paint was not asset-scoped");
    require(first.plan.dirtyRegion.valid, "first plan needs a dirty region");
    const auto firstRecord = headless.record(first.plan);

    auto repeated = planner.build(makeScene(
        1U,
        2U,
        10.0F,
        white,
        runtime::EvaluatedValueOrigin::AssetStatic,
        runtime::EvaluatedValueOrigin::AssetStatic));
    require(static_cast<bool>(repeated), "repeated plan build failed");
    require(repeated.plan.geometryUpdates.empty(), "unchanged geometry was updated");
    require(repeated.plan.paintUpdates.empty(), "unchanged paint was updated");
    require(!repeated.plan.visualChanged, "unchanged plan classified as changed");
    require(!repeated.plan.dirtyRegion.valid, "unchanged plan produced dirty bounds");
    require(first.plan.drawItems[0].geometry == repeated.plan.drawItems[0].geometry,
            "static geometry identity changed");
    require(first.plan.drawItems[0].paint == repeated.plan.drawItems[0].paint,
            "static paint identity changed");
    require(firstRecord.fingerprints.plan
                == headless.record(repeated.plan).fingerprints.plan,
            "headless plan identity is not deterministic");

    render::PresentationState translated;
    translated.transform.dx = 25.0F;
    translated.layoutRevision = 1U;
    auto moved = planner.build(makeScene(
        1U,
        3U,
        10.0F,
        white,
        runtime::EvaluatedValueOrigin::AssetStatic,
        runtime::EvaluatedValueOrigin::AssetStatic), translated);
    require(static_cast<bool>(moved), "translated plan build failed");
    require(moved.plan.geometryUpdates.empty(),
            "presentation transform rebuilt static geometry");
    require(moved.plan.paintUpdates.empty(),
            "presentation transform rebuilt static paint");
    require(moved.plan.visualChanged, "presentation transform did not change plan");
    require(moved.plan.dirtyRegion.valid, "translated plan lacks dirty union");
    require(moved.plan.drawItems[0].geometry == first.plan.drawItems[0].geometry,
            "transform-only plan did not reuse static geometry identity");

    auto secondInstance = planner.build(makeScene(
        2U,
        1U,
        10.0F,
        white,
        runtime::EvaluatedValueOrigin::AssetStatic,
        runtime::EvaluatedValueOrigin::AssetStatic));
    require(static_cast<bool>(secondInstance), "second instance plan failed");
    require(secondInstance.plan.drawItems[0].geometry
                == first.plan.drawItems[0].geometry,
            "asset-static geometry is not shared across instances");
    require(secondInstance.plan.drawItems[0].paint
                == first.plan.drawItems[0].paint,
            "asset-static paint is not shared across instances");

    auto dynamicFirst = planner.build(makeScene(
        3U,
        1U,
        10.0F,
        white,
        runtime::EvaluatedValueOrigin::InstanceEvaluated,
        runtime::EvaluatedValueOrigin::InstanceEvaluated));
    require(static_cast<bool>(dynamicFirst), "dynamic first plan failed");
    const auto geometryRevision1 = dynamicFirst.plan.drawItems[0].geometry.revision;
    const auto paintRevision1 = dynamicFirst.plan.drawItems[0].paint.revision;
    require(geometryRevision1 == 1U && paintRevision1 == 1U,
            "dynamic revisions must begin at one");

    auto geometryChanged = planner.build(makeScene(
        3U,
        2U,
        20.0F,
        white,
        runtime::EvaluatedValueOrigin::InstanceEvaluated,
        runtime::EvaluatedValueOrigin::InstanceEvaluated));
    require(static_cast<bool>(geometryChanged), "geometry change plan failed");
    require(geometryChanged.plan.drawItems[0].geometry.revision
                == geometryRevision1 + 1U,
            "geometry revision did not advance");
    require(geometryChanged.plan.drawItems[0].paint.revision == paintRevision1,
            "geometry-only change advanced paint revision");
    require(geometryChanged.plan.geometryUpdates.size() == 1U,
            "geometry update packet missing");
    require(geometryChanged.plan.paintUpdates.empty(),
            "geometry-only change emitted paint update");

    const auto red = runtime::Color8{255U, 0U, 0U, 255U};
    auto paintChanged = planner.build(makeScene(
        3U,
        3U,
        20.0F,
        red,
        runtime::EvaluatedValueOrigin::InstanceEvaluated,
        runtime::EvaluatedValueOrigin::InstanceEvaluated));
    require(static_cast<bool>(paintChanged), "paint change plan failed");
    require(paintChanged.plan.drawItems[0].geometry.revision
                == geometryChanged.plan.drawItems[0].geometry.revision,
            "paint-only change advanced geometry revision");
    require(paintChanged.plan.drawItems[0].paint.revision
                == paintRevision1 + 1U,
            "paint revision did not advance");
    require(paintChanged.plan.geometryUpdates.empty(),
            "paint-only change emitted geometry update");
    require(paintChanged.plan.paintUpdates.size() == 1U,
            "paint update packet missing");

    const auto stale = planner.build(makeScene(
        3U,
        2U,
        20.0F,
        red,
        runtime::EvaluatedValueOrigin::InstanceEvaluated,
        runtime::EvaluatedValueOrigin::InstanceEvaluated));
    require(!stale, "stale evaluation sequence was accepted");
    require(stale.error.code == render::RenderPlanErrorCode::StaleSnapshot,
            "stale plan reported wrong error");

    planner.forgetInstance(3U);
    auto afterForget = planner.build(makeScene(
        3U,
        1U,
        20.0F,
        red,
        runtime::EvaluatedValueOrigin::InstanceEvaluated,
        runtime::EvaluatedValueOrigin::InstanceEvaluated));
    require(static_cast<bool>(afterForget), "forgotten instance did not accept a fresh sequence");
    require(afterForget.plan.firstPlan, "forgotten instance was not reset");

    // A local path without a paint in the same coordinate space must not be
    // selected. Telegram strokes and gradients may already be expressed in
    // final viewport space; combining them with a local path would apply the
    // transform twice.
    render::MotionRenderPlanner finalSpacePlanner;
    auto finalSpaceReference = finalSpacePlanner.build(
        makeLocalSpaceScene(10U, 1U, false));
    require(static_cast<bool>(finalSpaceReference),
            "final-space fallback plan failed");
    require(matrixIsIdentity(
                finalSpaceReference.plan.drawItems[0].geometryTransform),
            "local geometry was selected without a matching local paint");
    require(finalSpaceReference.plan.drawItems[0].presentedBounds.valid
                && finalSpaceReference.plan.drawItems[0].presentedBounds.left == 40.0F
                && finalSpaceReference.plan.drawItems[0].presentedBounds.right == 50.0F,
            "final-space fallback produced incorrect bounds");

    // Once both sides of the seam are available, the planner may use the
    // local path and carry its affine transform separately.
    render::MotionRenderPlanner localSpacePlanner;
    auto completeLocalSeam = localSpacePlanner.build(
        makeLocalSpaceScene(11U, 1U, true));
    require(static_cast<bool>(completeLocalSeam),
            "complete local-space seam plan failed");
    require(completeLocalSeam.plan.drawItems[0].geometryTransform.dx == 40.0F
                && completeLocalSeam.plan.drawItems[0].geometryTransform.dy == 0.0F,
            "complete local-space seam lost its geometry transform");
    require(completeLocalSeam.plan.drawItems[0].presentedBounds.valid
                && completeLocalSeam.plan.drawItems[0].presentedBounds.left == 40.0F
                && completeLocalSeam.plan.drawItems[0].presentedBounds.right == 50.0F,
            "complete local-space seam produced incorrect bounds");
    require(completeLocalSeam.plan.drawItems[0].geometry.contentHash
                != finalSpaceReference.plan.drawItems[0].geometry.contentHash,
            "local and final path identities unexpectedly collapsed");

    const auto diagnostics = planner.diagnostics();
    require(diagnostics.plansBuilt >= 8U, "planner diagnostics did not advance");
    require(diagnostics.plansRejected >= 1U, "rejected plan diagnostics missing");
    require(diagnostics.geometryUpdates >= 4U, "geometry update diagnostics missing");
    require(diagnostics.paintUpdates >= 4U, "paint update diagnostics missing");

    std::cout << "AveMotion render-plan tests passed\n";
    return EXIT_SUCCESS;
}
