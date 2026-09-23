#include "avemotion/evaluation/PropertyEvaluator.hpp"
#include "avemotion/render/RenderPlanner.hpp"
#include "avemotion/render/SourceGeometryProjector.hpp"
#include "avemotion/runtime/Runtime.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace {

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

void require(bool condition, const std::string& message) {
    if (!condition) fail(message);
}

[[nodiscard]] bool sameMatrix(
    const avemotion::render::Matrix3x2& left,
    const avemotion::render::Matrix3x2& right) noexcept {
    return left.m11 == right.m11 && left.m12 == right.m12
        && left.m21 == right.m21 && left.m22 == right.m22
        && left.dx == right.dx && left.dy == right.dy;
}

std::string readText(const fs::path& path) {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) fail("unable to open " + path.string());
    return {std::istreambuf_iterator<char>{stream},
            std::istreambuf_iterator<char>{}};
}

struct ProjectedSample final {
    avemotion::runtime::EvaluatedScene scene;
    avemotion::render::SourceGeometryProjectionStatistics statistics;
};

ProjectedSample evaluateProjected(
    avemotion::runtime::Instance& instance,
    const avemotion::evaluation::PropertyEvaluator& evaluator,
    avemotion::evaluation::PropertyEvaluationWorkspace& workspace,
    const avemotion::render::SourceGeometryProjector& projector,
    std::size_t frame,
    std::size_t size = 128U,
    avemotion::render::SourceGeometryProjectionWorkspace*
        projectionWorkspace = nullptr) {
    const auto view = evaluator.evaluate(static_cast<double>(frame), workspace);
    require(static_cast<bool>(view), "property evaluation failed");
    auto scene = instance.evaluateModelFrame(frame, size, size);
    require(static_cast<bool>(scene), "model scene evaluation failed");
    const auto projected = projectionWorkspace
        ? projector.project(scene.scene, view, *projectionWorkspace)
        : projector.project(scene.scene, view);
    require(static_cast<bool>(projected), "source geometry projection failed");
    // Mismatched candidates are deliberately rejected and leave Telegram's
    // local path untouched. Accepted candidates are equal by construction.
    return {std::move(scene.scene), projected.statistics};
}

std::vector<std::uint64_t> projectedHashes(
    const avemotion::runtime::EvaluatedScene& scene) {
    std::vector<std::uint64_t> values;
    for (const auto& item : scene.drawItems) {
        if (item.sourceGeometryProjected) values.push_back(item.localPath.hash);
    }

    return values;
}

void accumulate(
    avemotion::render::SourceGeometryProjectionStatistics& total,
    const avemotion::render::SourceGeometryProjectionStatistics& sample) {
    total.drawItemsVisited += sample.drawItemsVisited;
    total.candidates += sample.candidates;
    total.projected += sample.projected;
    total.projectedStatic += sample.projectedStatic;
    total.projectedAnimated += sample.projectedAnimated;
    total.projectedPoints += sample.projectedPoints;
    total.primitiveCandidates += sample.primitiveCandidates;
    total.rectangleCandidates += sample.rectangleCandidates;
    total.ellipseCandidates += sample.ellipseCandidates;
    total.polystarCandidates += sample.polystarCandidates;
    total.starCandidates += sample.starCandidates;
    total.polygonCandidates += sample.polygonCandidates;
    total.projectedRectangles += sample.projectedRectangles;
    total.projectedRoundedRectangles += sample.projectedRoundedRectangles;
    total.projectedEllipses += sample.projectedEllipses;
    total.projectedStars += sample.projectedStars;
    total.projectedPolygons += sample.projectedPolygons;
    total.trimCandidates += sample.trimCandidates;
    total.trimSimultaneousCandidates += sample.trimSimultaneousCandidates;
    total.trimIndividualCandidates += sample.trimIndividualCandidates;
    total.trimMultiPathCandidates += sample.trimMultiPathCandidates;
    total.trimPathsVisited += sample.trimPathsVisited;
    total.trimPathsProjected += sample.trimPathsProjected;
    total.trimIndividualWrappedNoOps += sample.trimIndividualWrappedNoOps;
    total.projectedTrimmed += sample.projectedTrimmed;
    total.projectedTrimEmpty += sample.projectedTrimEmpty;
    total.projectedTrimFull += sample.projectedTrimFull;
    total.projectedTrimPartial += sample.projectedTrimPartial;
    total.trimSplitCount += sample.trimSplitCount;
    total.repeaterCandidates += sample.repeaterCandidates;
    total.repeaterCopiesVisited += sample.repeaterCopiesVisited;
    total.repeaterCopiesProjected += sample.repeaterCopiesProjected;
    total.repeaterVisibleCopies += sample.repeaterVisibleCopies;
    total.repeaterHiddenCopies += sample.repeaterHiddenCopies;
    total.repeaterStaticGeometryCopies += sample.repeaterStaticGeometryCopies;
    total.repeaterAnimatedGeometryCopies += sample.repeaterAnimatedGeometryCopies;
    total.repeaterTransformAnimatedCopies += sample.repeaterTransformAnimatedCopies;
    total.repeaterOpacityAnimatedCopies += sample.repeaterOpacityAnimatedCopies;
    total.repeaterFillApplicationsProjected +=
        sample.repeaterFillApplicationsProjected;
    total.repeaterStrokeApplicationsProjected +=
        sample.repeaterStrokeApplicationsProjected;
    total.repeaterNestedPaintApplicationsProjected +=
        sample.repeaterNestedPaintApplicationsProjected;
    total.repeaterAnimatedPaintApplicationsProjected +=
        sample.repeaterAnimatedPaintApplicationsProjected;
    total.skippedRepeaterBinding += sample.skippedRepeaterBinding;
    total.skippedRepeaterProperties += sample.skippedRepeaterProperties;
    total.skippedRepeaterEvaluation += sample.skippedRepeaterEvaluation;
    total.rejectedRepeaterInput += sample.rejectedRepeaterInput;
    total.repeaterParityMismatches += sample.repeaterParityMismatches;
    total.skippedTrimBinding += sample.skippedTrimBinding;
    total.skippedTrimProperties += sample.skippedTrimProperties;
    total.skippedTrimEvaluation += sample.skippedTrimEvaluation;
    total.rejectedTrimInput += sample.rejectedTrimInput;
    total.skippedPrimitiveProperties += sample.skippedPrimitiveProperties;
    total.skippedPrimitiveEvaluation += sample.skippedPrimitiveEvaluation;
    total.skippedPolystarProperties += sample.skippedPolystarProperties;
    total.skippedPolystarEvaluation += sample.skippedPolystarEvaluation;
    total.rejectedPolystarInputs += sample.rejectedPolystarInputs;
    total.skippedUnbound += sample.skippedUnbound;
    total.skippedMultiplePaths += sample.skippedMultiplePaths;
    total.skippedModified += sample.skippedModified;
    total.nestedCompositionCandidates += sample.nestedCompositionCandidates;
    total.skippedNestedComposition += sample.skippedNestedComposition;
    total.skippedPaint += sample.skippedPaint;
    total.skippedWrongNodeKind += sample.skippedWrongNodeKind;
    total.skippedNoShapeProperty += sample.skippedNoShapeProperty;
    total.skippedShapeEvaluation += sample.skippedShapeEvaluation;
    total.skippedTransform += sample.skippedTransform;
    total.skippedUnsupportedShape += sample.skippedUnsupportedShape;
    total.parityMismatches += sample.parityMismatches;
}

} // namespace

int main() {
    using namespace avemotion;
    const fs::path corpus{AVEMOTION_CORPUS_DIR};
    const fs::path fixtures{AVEMOTION_FIXTURE_DIR};
    const std::array<const char*, 8> names{
        "1667-firework.json",
        "ModernPictogramsForLottie_LoudMute.json",
        "StickAndBall.json",
        "dynamic_path_test.json",
        "gradient_animated_background.json",
        "mask.json",
        "matte_two_item_with_lowerlayer.json",
        "polystar_line_clockwise_trim.json",
    };

    render::SourceGeometryProjectionStatistics total;
    for (const auto* name : names) {
        runtime::Runtime runtime;
        auto loaded = runtime.loadLottieJson(readText(corpus / name), name);
        require(static_cast<bool>(loaded), std::string{"load failed: "} + name);
        const auto prepared = loaded.asset->prepareModel();
        require(static_cast<bool>(prepared), std::string{"model failed: "} + name);
        auto created = runtime.createInstance(loaded.asset);
        require(static_cast<bool>(created), std::string{"instance failed: "} + name);

        evaluation::PropertyEvaluator evaluator{prepared.model};
        require(evaluator.valid(), std::string{"evaluator invalid: "} + name);
        evaluation::PropertyEvaluationWorkspace workspace;
        evaluator.prepare(workspace);
        const auto storageGeneration = workspace.storageGeneration();
        render::SourceGeometryProjector projector{prepared.model};
        require(projector.valid(), std::string{"projector invalid: "} + name);

        for (const double position : {0.0, 0.25, 0.5, 0.75, 1.0}) {
            const auto frame = created.instance->frameAtPosition(position);
            const auto sample = evaluateProjected(
                *created.instance, evaluator, workspace, projector, frame);
            accumulate(total, sample.statistics);
            require(workspace.storageGeneration() == storageGeneration,
                    "property workspace allocated after prepare");
        }
    }

    require(total.candidates > 0U, "Telegram seam exposed no source geometry candidates");
    require(total.projected > 0U, "no modifier-free Shape geometry was projected");
    require(total.projectedStatic > 0U, "no asset-static Shape geometry was projected");
    require(total.projectedAnimated > 0U, "no animated Shape geometry was projected");
    require(total.projected + total.parityMismatches > 0U,
            "no source geometry candidates reached the parity gate");

    // Exercise exact Telegram primitive semantics independently of the legacy
    // corpus: CW/CCW, sharp/rounded/clamped rectangles, ellipses and animated
    // primitive properties must all pass the verb/point parity gate.
    {
        const fs::path fixture = fs::path{AVEMOTION_FIXTURE_DIR}
            / "primitive_geometry.json";
        runtime::Runtime primitiveRuntime;
        auto primitiveLoaded = primitiveRuntime.loadLottieJson(
            readText(fixture), fixture.filename().string());
        require(static_cast<bool>(primitiveLoaded), "primitive fixture load failed");
        const auto primitiveModel = primitiveLoaded.asset->prepareModel();
        require(static_cast<bool>(primitiveModel), "primitive fixture model failed");
        auto primitiveInstance = primitiveRuntime.createInstance(primitiveLoaded.asset);
        require(static_cast<bool>(primitiveInstance),
                "primitive fixture instance failed");
        evaluation::PropertyEvaluator primitiveEvaluator{primitiveModel.model};
        evaluation::PropertyEvaluationWorkspace primitiveWorkspace;
        primitiveEvaluator.prepare(primitiveWorkspace);
        const auto primitiveStorageGeneration =
            primitiveWorkspace.storageGeneration();
        render::SourceGeometryProjector primitiveProjector{primitiveModel.model};
        require(primitiveProjector.valid(), "primitive fixture projector invalid");

        std::size_t sampleIndex = 0U;
        for (const double position : {0.0, 0.25, 0.5, 0.75, 1.0}) {
            const auto frame = primitiveInstance.instance->frameAtPosition(position);
            const auto sample = evaluateProjected(
                *primitiveInstance.instance, primitiveEvaluator,
                primitiveWorkspace, primitiveProjector, frame, 280U);
            const auto& statistics = sample.statistics;
            accumulate(total, statistics);
            require(statistics.primitiveCandidates == 7U,
                    "primitive fixture candidate count changed");
            require(statistics.rectangleCandidates == 4U,
                    "primitive fixture rectangle count changed");
            require(statistics.ellipseCandidates == 3U,
                    "primitive fixture ellipse count changed");
            require(statistics.projected == 7U,
                    "primitive fixture projection was rejected");
            require(statistics.projectedStatic == 5U,
                    "primitive fixture static identity changed");
            require(statistics.projectedAnimated == 2U,
                    "primitive fixture animated identity changed");
            require(statistics.projectedEllipses == 3U,
                    "primitive fixture ellipse projection changed");
            require(statistics.parityMismatches == 0U,
                    "primitive fixture failed Telegram parity");
            require(statistics.skippedPrimitiveProperties == 0U
                    && statistics.skippedPrimitiveEvaluation == 0U,
                    "primitive fixture properties were unavailable");
            if (sampleIndex == 0U) {
                require(statistics.projectedRectangles == 2U
                        && statistics.projectedRoundedRectangles == 2U,
                        "primitive fixture initial roundness semantics changed");
            } else {
                require(statistics.projectedRectangles == 1U
                        && statistics.projectedRoundedRectangles == 3U,
                        "primitive fixture animated roundness semantics changed");
            }
            require(primitiveWorkspace.storageGeneration()
                        == primitiveStorageGeneration,
                    "primitive evaluation allocated after prepare");
            ++sampleIndex;
        }
    }

    // Exercise Telegram's exact Polystar/Polygon path semantics independently
    // of trim/repeater modifiers. This fixture covers integer/fractional stars,
    // sharp/rounded polygons, both directions and animated topology.
    {
        const fs::path fixture = fs::path{AVEMOTION_FIXTURE_DIR}
            / "polystar_polygon_geometry.json";
        runtime::Runtime polystarRuntime;
        auto polystarLoaded = polystarRuntime.loadLottieJson(
            readText(fixture), fixture.filename().string());
        require(static_cast<bool>(polystarLoaded), "polystar fixture load failed");
        const auto polystarModel = polystarLoaded.asset->prepareModel();
        require(static_cast<bool>(polystarModel), "polystar fixture model failed");
        auto polystarInstance = polystarRuntime.createInstance(polystarLoaded.asset);
        require(static_cast<bool>(polystarInstance),
                "polystar fixture instance failed");
        evaluation::PropertyEvaluator polystarEvaluator{polystarModel.model};
        evaluation::PropertyEvaluationWorkspace polystarWorkspace;
        polystarEvaluator.prepare(polystarWorkspace);
        const auto polystarStorageGeneration =
            polystarWorkspace.storageGeneration();
        render::SourceGeometryProjector polystarProjector{polystarModel.model};
        require(polystarProjector.valid(), "polystar fixture projector invalid");

        for (const double position : {0.0, 0.25, 0.5, 0.75, 1.0}) {
            const auto frame = polystarInstance.instance->frameAtPosition(position);
            const auto sample = evaluateProjected(
                *polystarInstance.instance, polystarEvaluator,
                polystarWorkspace, polystarProjector, frame, 450U);
            const auto& statistics = sample.statistics;
            accumulate(total, statistics);
            require(statistics.primitiveCandidates == 8U,
                    "polystar fixture primitive count changed");
            require(statistics.polystarCandidates == 8U,
                    "polystar fixture candidate count changed");
            require(statistics.starCandidates == 5U,
                    "polystar fixture star count changed");
            require(statistics.polygonCandidates == 3U,
                    "polystar fixture polygon count changed");
            require(statistics.projected == 8U
                    && statistics.projectedStars == 5U
                    && statistics.projectedPolygons == 3U,
                    "polystar fixture projection was rejected");
            require(statistics.projectedStatic == 6U
                    && statistics.projectedAnimated == 2U,
                    "polystar fixture static/animated identity changed");
            require(statistics.parityMismatches == 0U,
                    "polystar fixture failed Telegram parity");
            require(statistics.skippedPrimitiveProperties == 0U
                    && statistics.skippedPrimitiveEvaluation == 0U
                    && statistics.skippedPolystarProperties == 0U
                    && statistics.skippedPolystarEvaluation == 0U
                    && statistics.rejectedPolystarInputs == 0U,
                    "polystar fixture properties or generation failed");
            require(polystarWorkspace.storageGeneration()
                        == polystarStorageGeneration,
                    "polystar evaluation allocated after prepare");
        }

        const auto polystarFrameCount =
            polystarLoaded.asset->metadata().totalFrames;
        require(polystarFrameCount == 61U,
                "polystar fixture frame count changed");
        polystarWorkspace.resetHistory();
        for (std::size_t frame = 0U; frame < polystarFrameCount; ++frame) {
            const auto sample = evaluateProjected(
                *polystarInstance.instance, polystarEvaluator,
                polystarWorkspace, polystarProjector, frame, 450U);
            require(sample.statistics.projected == 8U
                    && sample.statistics.projectedStars == 5U
                    && sample.statistics.projectedPolygons == 3U,
                    "polystar fixture frame failed projection");
            require(sample.statistics.parityMismatches == 0U
                    && sample.statistics.skippedPolystarProperties == 0U
                    && sample.statistics.skippedPolystarEvaluation == 0U
                    && sample.statistics.rejectedPolystarInputs == 0U,
                    "polystar fixture frame failed exact Telegram parity");
        }
        require(polystarWorkspace.storageGeneration()
                    == polystarStorageGeneration,
                "polystar exhaustive evaluation allocated after prepare");

        const auto targetFrame =
            polystarInstance.instance->frameAtPosition(0.75);
        polystarWorkspace.resetHistory();
        for (std::size_t frame = 0U; frame <= targetFrame; ++frame) {
            (void)evaluateProjected(
                *polystarInstance.instance, polystarEvaluator,
                polystarWorkspace, polystarProjector, frame, 450U);
        }
        const auto sequential = evaluateProjected(
            *polystarInstance.instance, polystarEvaluator,
            polystarWorkspace, polystarProjector, targetFrame, 450U);
        render::MotionRenderPlanner sequentialPlanner;
        const auto sequentialPlan = sequentialPlanner.build(sequential.scene);
        require(static_cast<bool>(sequentialPlan),
                "sequential polystar plan failed");

        polystarWorkspace.resetHistory();
        const auto direct = evaluateProjected(
            *polystarInstance.instance, polystarEvaluator,
            polystarWorkspace, polystarProjector, targetFrame, 450U);
        require(projectedHashes(direct.scene) == projectedHashes(sequential.scene),
                "direct and sequential polystar geometry differ");
        render::MotionRenderPlanner directPlanner;
        const auto directPlan = directPlanner.build(direct.scene);
        require(static_cast<bool>(directPlan), "direct polystar plan failed");
        require(directPlan.plan.drawItems.size()
                    == sequentialPlan.plan.drawItems.size(),
                "direct and sequential polystar item counts differ");
        for (std::size_t index = 0U;
             index < directPlan.plan.drawItems.size(); ++index) {
            const auto& directItem = directPlan.plan.drawItems[index];
            const auto& sequentialItem = sequentialPlan.plan.drawItems[index];
            if (directItem.sourceDrawItemIndex >= direct.scene.drawItems.size()
                || !direct.scene.drawItems[directItem.sourceDrawItemIndex]
                        .sourceGeometryProjected) {
                continue;
            }
            require(directItem.geometry == sequentialItem.geometry,
                    "direct and sequential polystar cache keys differ");
        }

        render::MotionRenderPlanner repeatedPlanner;
        const auto firstPlan = repeatedPlanner.build(direct.scene);
        require(static_cast<bool>(firstPlan), "first polystar repeat plan failed");
        const auto repeated = evaluateProjected(
            *polystarInstance.instance, polystarEvaluator,
            polystarWorkspace, polystarProjector, targetFrame, 450U);
        const auto repeatedPlan = repeatedPlanner.build(repeated.scene);
        require(static_cast<bool>(repeatedPlan),
                "repeated polystar plan failed");
        require(repeatedPlan.plan.geometryUpdates.empty(),
                "repeated exact-time polystar plan rebuilt geometry");
    }

    // Prove the one-path Trim Path subset against Telegram on every source
    // frame. The fixture covers empty/full/partial/wrapped/offset, animated
    // values and Individual mode with exactly one path.
    {
        const fs::path fixture = fs::path{AVEMOTION_FIXTURE_DIR}
            / "trim_path_geometry.json";
        runtime::Runtime trimRuntime;
        auto trimLoaded = trimRuntime.loadLottieJson(
            readText(fixture), fixture.filename().string());
        require(static_cast<bool>(trimLoaded), "trim fixture load failed");
        const auto trimModel = trimLoaded.asset->prepareModel();
        require(static_cast<bool>(trimModel), "trim fixture model failed");
        auto trimInstance = trimRuntime.createInstance(trimLoaded.asset);
        require(static_cast<bool>(trimInstance), "trim fixture instance failed");
        evaluation::PropertyEvaluator trimEvaluator{trimModel.model};
        evaluation::PropertyEvaluationWorkspace trimWorkspace;
        trimEvaluator.prepare(trimWorkspace);
        const auto trimStorageGeneration = trimWorkspace.storageGeneration();
        render::SourceGeometryProjector trimProjector{trimModel.model};
        require(trimProjector.valid(), "trim fixture projector invalid");

        for (std::size_t frame = 0U; frame < trimLoaded.asset->metadata().totalFrames;
             ++frame) {
            const auto sample = evaluateProjected(
                *trimInstance.instance, trimEvaluator, trimWorkspace,
                trimProjector, frame, 400U);
            const auto& statistics = sample.statistics;
            require(statistics.trimCandidates == 8U,
                    "trim fixture candidate count changed");
            require(statistics.trimSimultaneousCandidates == 7U,
                    "trim fixture simultaneous count changed");
            require(statistics.trimIndividualCandidates == 1U,
                    "trim fixture Individual count changed");
            require(statistics.projectedTrimmed == 8U,
                    "trim fixture projection was rejected");
            require(statistics.projected == 8U,
                    "trim fixture total projected count changed");
            require(statistics.projectedStatic == 7U
                        && statistics.projectedAnimated == 1U,
                    "trim fixture static/animated identity changed");
            require(statistics.skippedTrimBinding == 0U
                        && statistics.skippedTrimProperties == 0U
                        && statistics.skippedTrimEvaluation == 0U
                        && statistics.rejectedTrimInput == 0U,
                    "trim fixture failed its canonical binding/evaluation seam");
            require(statistics.parityMismatches == 0U,
                    "trim fixture failed Telegram path parity");
            require(trimWorkspace.storageGeneration() == trimStorageGeneration,
                    "trim fixture property workspace allocated after prepare");
        }

        const std::size_t targetFrame = 37U;
        trimWorkspace.resetHistory();
        for (std::size_t frame = 0U; frame <= targetFrame; ++frame) {
            (void)evaluateProjected(
                *trimInstance.instance, trimEvaluator, trimWorkspace,
                trimProjector, frame, 400U);
        }
        const auto sequential = evaluateProjected(
            *trimInstance.instance, trimEvaluator, trimWorkspace,
            trimProjector, targetFrame, 400U);
        render::MotionRenderPlanner sequentialPlanner;
        const auto sequentialPlan = sequentialPlanner.build(sequential.scene);
        require(static_cast<bool>(sequentialPlan),
                "sequential trim plan failed");

        trimWorkspace.resetHistory();
        const auto direct = evaluateProjected(
            *trimInstance.instance, trimEvaluator, trimWorkspace,
            trimProjector, targetFrame, 400U);
        require(projectedHashes(direct.scene) == projectedHashes(sequential.scene),
                "direct and sequential trim geometry differ");
        render::MotionRenderPlanner directPlanner;
        const auto directPlan = directPlanner.build(direct.scene);
        require(static_cast<bool>(directPlan), "direct trim plan failed");
        require(directPlan.plan.drawItems.size()
                    == sequentialPlan.plan.drawItems.size(),
                "direct and sequential trim item counts differ");
        for (std::size_t index = 0U; index < directPlan.plan.drawItems.size();
             ++index) {
            const auto& directItem = directPlan.plan.drawItems[index];
            const auto& sequentialItem = sequentialPlan.plan.drawItems[index];
            if (directItem.sourceDrawItemIndex >= direct.scene.drawItems.size()
                || !direct.scene.drawItems[directItem.sourceDrawItemIndex]
                        .sourceGeometryProjected) {
                continue;
            }
            require(directItem.geometry == sequentialItem.geometry,
                    "direct and sequential trim cache keys differ");
        }

        render::MotionRenderPlanner repeatedPlanner;
        const auto firstPlan = repeatedPlanner.build(direct.scene);
        require(static_cast<bool>(firstPlan), "first trim repeat plan failed");
        const auto repeated = evaluateProjected(
            *trimInstance.instance, trimEvaluator, trimWorkspace,
            trimProjector, targetFrame, 400U);
        const auto repeatedPlan = repeatedPlanner.build(repeated.scene);
        require(static_cast<bool>(repeatedPlan), "repeated trim plan failed");
        require(repeatedPlan.plan.geometryUpdates.empty(),
                "repeated exact-time trim plan rebuilt geometry");
    }

    // Part 15: one Trim Path controls multiple direct paths. Simultaneous
    // trims every path independently; Individual distributes one interval
    // over aggregate authored path length. The prepared projection workspace
    // must remain allocation-stable across all 61 source frames.
    {
        const auto multiTrimPath = fixtures / "multi_trim_path_geometry.json";
        runtime::Runtime multiRuntime;
        auto multiLoaded = multiRuntime.loadLottieJson(
            readText(multiTrimPath), multiTrimPath.filename().string());
        require(static_cast<bool>(multiLoaded), "multi-trim fixture load failed");
        const auto multiModel = multiLoaded.asset->prepareModel();
        require(static_cast<bool>(multiModel), "multi-trim model failed");
        auto multiInstance = multiRuntime.createInstance(multiLoaded.asset);
        require(static_cast<bool>(multiInstance), "multi-trim instance failed");
        evaluation::PropertyEvaluator multiEvaluator{multiModel.model};
        evaluation::PropertyEvaluationWorkspace multiWorkspace;
        multiEvaluator.prepare(multiWorkspace);
        render::SourceGeometryProjector multiProjector{multiModel.model};
        require(multiProjector.valid(), "multi-trim projector invalid");
        render::SourceGeometryProjectionWorkspace projectionWorkspace;
        require(multiProjector.prepare(projectionWorkspace),
                "multi-trim workspace prepare failed");
        const auto preparedStorageGeneration =
            projectionWorkspace.storageGeneration();

        for (std::size_t frame = 0U; frame <= 60U; ++frame) {
            const auto sample = evaluateProjected(
                *multiInstance.instance,
                multiEvaluator,
                multiWorkspace,
                multiProjector,
                frame,
                600U,
                &projectionWorkspace);
            const auto& statistics = sample.statistics;
            require(statistics.trimCandidates == 6U,
                    "multi-trim candidate count changed");
            require(statistics.trimMultiPathCandidates == 6U,
                    "multi-trim multi-path count changed");
            require(statistics.trimSimultaneousCandidates == 2U
                        && statistics.trimIndividualCandidates == 4U,
                    "multi-trim mode counts changed");
            require(statistics.trimPathsVisited == 15U
                        && statistics.trimPathsProjected == 15U,
                    "multi-trim path distribution count changed");
            const std::size_t expectedWrappedNoOps = frame < 33U ? 1U : 2U;
            require(statistics.trimIndividualWrappedNoOps == expectedWrappedNoOps,
                    "wrapped Individual compatibility count changed");
            require(statistics.projectedTrimmed == 6U
                        && statistics.projected == 6U,
                    "multi-trim projection count changed");
            require(statistics.projectedStatic == 5U
                        && statistics.projectedAnimated == 1U,
                    "multi-trim static/animated classification changed");
            require(statistics.skippedMultiplePaths == 0U
                        && statistics.rejectedTrimInput == 0U
                        && statistics.parityMismatches == 0U,
                    "multi-trim fixture must project without fallback");
            require(projectionWorkspace.storageGeneration()
                        == preparedStorageGeneration,
                    "prepared source-geometry workspace allocated in steady state");
            for (const auto& item : sample.scene.drawItems) {
                require(item.sourcePathCount > 1U,
                        "multi-trim fixture unexpectedly emitted single path");
                require(item.sourceGeometryProjected,
                        "multi-trim draw item remained on Telegram fallback");
            }
        }

        constexpr std::size_t targetFrame = 37U;
        for (std::size_t frame = 0U; frame <= targetFrame; ++frame) {
            (void)evaluateProjected(
                *multiInstance.instance,
                multiEvaluator,
                multiWorkspace,
                multiProjector,
                frame,
                600U,
                &projectionWorkspace);
        }
        const auto sequential = evaluateProjected(
            *multiInstance.instance,
            multiEvaluator,
            multiWorkspace,
            multiProjector,
            targetFrame,
            600U,
            &projectionWorkspace);
        render::MotionRenderPlanner sequentialPlanner;
        const auto sequentialPlan = sequentialPlanner.build(sequential.scene);
        require(static_cast<bool>(sequentialPlan),
                "sequential multi-trim plan failed");

        multiWorkspace.resetHistory();
        const auto direct = evaluateProjected(
            *multiInstance.instance,
            multiEvaluator,
            multiWorkspace,
            multiProjector,
            targetFrame,
            600U,
            &projectionWorkspace);
        require(projectedHashes(direct.scene) == projectedHashes(sequential.scene),
                "direct and sequential multi-trim geometry differ");
        render::MotionRenderPlanner directPlanner;
        const auto directPlan = directPlanner.build(direct.scene);
        require(static_cast<bool>(directPlan), "direct multi-trim plan failed");
        require(directPlan.plan.drawItems.size()
                    == sequentialPlan.plan.drawItems.size(),
                "multi-trim plan item counts differ");
        for (std::size_t index = 0U;
             index < directPlan.plan.drawItems.size(); ++index) {
            require(directPlan.plan.drawItems[index].geometry
                        == sequentialPlan.plan.drawItems[index].geometry,
                    "multi-trim direct/sequential cache key differs");
        }

        render::MotionRenderPlanner repeatedPlanner;
        const auto firstPlan = repeatedPlanner.build(direct.scene);
        require(static_cast<bool>(firstPlan),
                "first multi-trim repeat plan failed");
        const auto repeated = evaluateProjected(
            *multiInstance.instance,
            multiEvaluator,
            multiWorkspace,
            multiProjector,
            targetFrame,
            600U,
            &projectionWorkspace);
        const auto repeatedPlan = repeatedPlanner.build(repeated.scene);
        require(static_cast<bool>(repeatedPlan),
                "repeated multi-trim plan failed");
        require(repeatedPlan.plan.geometryUpdates.empty(),
                "repeated exact-time multi-trim rebuilt geometry");
        require(projectionWorkspace.storageGeneration()
                    == preparedStorageGeneration,
                "multi-trim workspace grew after direct/repeated evaluation");
    }

    // Prove asset-static source geometry shares an identical cache key across
    // two instances and presentation-only changes do not rebuild geometry.
    {
        const auto iconPath = corpus / "ModernPictogramsForLottie_LoudMute.json";
        runtime::Runtime iconRuntime;
        auto iconLoaded = iconRuntime.loadLottieJson(
            readText(iconPath), iconPath.filename().string());
        require(static_cast<bool>(iconLoaded), "icon asset load failed");
        const auto iconModel = iconLoaded.asset->prepareModel();
        require(static_cast<bool>(iconModel), "icon model failed");
        auto firstInstance = iconRuntime.createInstance(iconLoaded.asset);
        auto secondInstance = iconRuntime.createInstance(iconLoaded.asset);
        require(firstInstance && secondInstance, "icon instance creation failed");
        evaluation::PropertyEvaluator iconEvaluator{iconModel.model};
        evaluation::PropertyEvaluationWorkspace firstWorkspace;
        evaluation::PropertyEvaluationWorkspace secondWorkspace;
        iconEvaluator.prepare(firstWorkspace);
        iconEvaluator.prepare(secondWorkspace);
        render::SourceGeometryProjector iconProjector{iconModel.model};
        const auto frame = firstInstance.instance->frameAtPosition(0.25);
        auto firstSample = evaluateProjected(
            *firstInstance.instance, iconEvaluator, firstWorkspace,
            iconProjector, frame);
        auto secondSample = evaluateProjected(
            *secondInstance.instance, iconEvaluator, secondWorkspace,
            iconProjector, frame);
        render::MotionRenderPlanner firstPlanner;
        render::MotionRenderPlanner secondPlanner;
        auto firstPlan = firstPlanner.build(firstSample.scene);
        auto secondPlan = secondPlanner.build(secondSample.scene);
        require(firstPlan && secondPlan, "projected icon plan failed");
        bool sharedStatic = false;
        for (const auto& left : firstPlan.plan.drawItems) {
            if (left.geometry.scope != render::ResourceIdentityScope::Asset) continue;
            for (const auto& right : secondPlan.plan.drawItems) {
                if (right.geometry.scope == render::ResourceIdentityScope::Asset
                    && right.geometry.sourceKey == left.geometry.sourceKey) {
                    require(right.geometry == left.geometry,
                            "asset-static projected geometry key differs between instances");
                    sharedStatic = true;
                }
            }
        }
        require(sharedStatic, "no shared asset-static projected geometry key found");

        render::PresentationState translated;
        translated.transform.dx = 25.0F;
        translated.transform.dy = 10.0F;
        translated.layoutRevision = 1U;
        const auto repeatProperties = iconEvaluator.evaluate(
            static_cast<double>(frame), firstWorkspace);
        auto repeatScene = firstInstance.instance->evaluateModelFrame(
            frame, 128U, 128U);
        require(repeatScene && repeatProperties, "icon repeat evaluation failed");
        const auto repeatProjection = iconProjector.project(
            repeatScene.scene, repeatProperties);
        require(static_cast<bool>(repeatProjection), "icon repeat projection failed");
        auto translatedPlan = firstPlanner.build(
            std::move(repeatScene.scene), translated);
        require(static_cast<bool>(translatedPlan), "translated plan failed");
        require(translatedPlan.plan.geometryUpdates.empty(),
                "presentation-only transform rebuilt projected geometry");
        require(translatedPlan.plan.visualChanged,
                "presentation-only transform was not visible to the plan");
    }

    // Prove direct seek and sequential evaluation produce identical projected
    // geometry, and that repeated exact-time plans emit no native geometry work.
    const auto dynamicPath = corpus / "dynamic_path_test.json";
    runtime::Runtime runtime;
    auto loaded = runtime.loadLottieJson(
        readText(dynamicPath), dynamicPath.filename().string());
    require(static_cast<bool>(loaded), "dynamic path asset load failed");
    const auto prepared = loaded.asset->prepareModel();
    require(static_cast<bool>(prepared), "dynamic path model failed");
    auto created = runtime.createInstance(loaded.asset);
    require(static_cast<bool>(created), "dynamic path instance failed");
    evaluation::PropertyEvaluator evaluator{prepared.model};
    evaluation::PropertyEvaluationWorkspace workspace;
    evaluator.prepare(workspace);
    render::SourceGeometryProjector projector{prepared.model};
    const auto targetFrame = created.instance->frameAtPosition(0.5);

    for (std::size_t frame = 0; frame <= targetFrame; ++frame) {
        (void)evaluateProjected(
            *created.instance, evaluator, workspace, projector, frame);
    }
    const auto sequential = evaluateProjected(
        *created.instance, evaluator, workspace, projector, targetFrame);
    const auto sequentialHashes = projectedHashes(sequential.scene);
    render::MotionRenderPlanner sequentialPlanner;
    auto sequentialPlan = sequentialPlanner.build(sequential.scene);
    require(static_cast<bool>(sequentialPlan), "sequential projected plan failed");

    workspace.resetHistory();
    const auto direct = evaluateProjected(
        *created.instance, evaluator, workspace, projector, targetFrame);
    require(projectedHashes(direct.scene) == sequentialHashes,
            "direct seek and sequential source geometry differ");
    render::MotionRenderPlanner directPlanner;
    auto directPlan = directPlanner.build(direct.scene);
    require(static_cast<bool>(directPlan), "direct projected plan failed");
    require(directPlan.plan.drawItems.size() == sequentialPlan.plan.drawItems.size(),
            "direct and sequential plan item counts differ");
    for (std::size_t index = 0; index < directPlan.plan.drawItems.size(); ++index) {
        const auto& directItem = directPlan.plan.drawItems[index];
        const auto& sequentialItem = sequentialPlan.plan.drawItems[index];
        if (directItem.sourceDrawItemIndex >= direct.scene.drawItems.size()
            || !direct.scene.drawItems[directItem.sourceDrawItemIndex]
                    .sourceGeometryProjected) {
            continue;
        }
        require(directItem.geometry == sequentialItem.geometry,
                "direct seek and sequential geometry cache keys differ");
    }

    render::MotionRenderPlanner planner;
    auto firstPlan = planner.build(direct.scene);
    require(static_cast<bool>(firstPlan), "first projected plan failed");
    const auto repeated = evaluateProjected(
        *created.instance, evaluator, workspace, projector, targetFrame);
    auto repeatedPlan = planner.build(repeated.scene);
    require(static_cast<bool>(repeatedPlan), "repeated projected plan failed");
    require(repeatedPlan.plan.geometryUpdates.empty(),
            "repeated exact-time projected plan rebuilt geometry");

    // Part 16: a canonical Repeater keeps one base geometry per authored
    // content group and evaluates copy transform/opacity independently. Every
    // emitted Telegram copy must pass path, matrix and opacity parity before
    // AveMotion replaces the extracted local-space data.
    {
        const auto repeaterPath = fixtures / "repeater_geometry.json";
        runtime::Runtime repeaterRuntime;
        auto repeaterLoaded = repeaterRuntime.loadLottieJson(
            readText(repeaterPath), repeaterPath.filename().string());
        require(static_cast<bool>(repeaterLoaded),
                "repeater fixture load failed");
        const auto repeaterModel = repeaterLoaded.asset->prepareModel();
        require(static_cast<bool>(repeaterModel),
                "repeater fixture model failed");
        auto repeaterInstance = repeaterRuntime.createInstance(
            repeaterLoaded.asset);
        require(static_cast<bool>(repeaterInstance),
                "repeater fixture instance failed");
        evaluation::PropertyEvaluator repeaterEvaluator{repeaterModel.model};
        evaluation::PropertyEvaluationWorkspace repeaterWorkspace;
        repeaterEvaluator.prepare(repeaterWorkspace);
        render::SourceGeometryProjector repeaterProjector{repeaterModel.model};
        require(repeaterProjector.valid(), "repeater projector invalid");
        render::SourceGeometryProjectionWorkspace projectionWorkspace;
        require(repeaterProjector.prepare(projectionWorkspace),
                "repeater projection workspace prepare failed");
        const auto preparedProjectionGeneration =
            projectionWorkspace.storageGeneration();

        for (std::size_t frame = 0U; frame <= 60U; ++frame) {
            const auto sample = evaluateProjected(
                *repeaterInstance.instance,
                repeaterEvaluator,
                repeaterWorkspace,
                repeaterProjector,
                frame,
                640U,
                &projectionWorkspace);
            const auto& statistics = sample.statistics;
            require(statistics.repeaterCandidates == 24U
                        && statistics.repeaterCopiesVisited == 24U
                        && statistics.repeaterCopiesProjected == 24U,
                    "repeater copy count changed");
            require(statistics.projected == 24U
                        && statistics.projectedStatic == 24U
                        && statistics.projectedAnimated == 0U,
                    "repeater geometry sharing classification changed");
            require(statistics.repeaterStaticGeometryCopies == 24U
                        && statistics.repeaterAnimatedGeometryCopies == 0U,
                    "repeater base geometry was not asset-scoped");
            require(statistics.repeaterTransformAnimatedCopies == 9U
                        && statistics.repeaterOpacityAnimatedCopies == 9U,
                    "repeater animated-property classification changed");
            require(statistics.repeaterVisibleCopies
                            + statistics.repeaterHiddenCopies == 24U
                        && statistics.repeaterVisibleCopies >= 20U
                        && statistics.repeaterVisibleCopies <= 24U,
                    "repeater visibility distribution changed");
            require(statistics.skippedRepeaterBinding == 0U
                        && statistics.skippedRepeaterProperties == 0U
                        && statistics.skippedRepeaterEvaluation == 0U
                        && statistics.rejectedRepeaterInput == 0U
                        && statistics.repeaterParityMismatches == 0U,
                    "repeater fixture fell back from AveMotion parity path");
            require(projectionWorkspace.storageGeneration()
                        == preparedProjectionGeneration,
                    "repeater projection workspace grew after prepare");

            struct GroupState final {
                std::uint32_t maximumCopies = 0U;
                std::uint64_t geometryId = 0U;
                std::uint64_t pathHash = 0U;
                std::vector<bool> observed;
            };
            std::unordered_map<std::uint32_t, GroupState> groups;
            for (const auto& item : sample.scene.drawItems) {
                require(item.sourceRepeaterProjected,
                        "repeater item remained on Telegram fallback");
                require(item.sourceGeometryProjected
                            && item.geometryOrigin
                                == runtime::EvaluatedValueOrigin::AssetStatic
                            && item.sourceGeometryRevision == 0U,
                        "repeater copy did not reuse static base geometry");
                auto& group = groups[item.sourceRepeaterNode.value];
                if (group.observed.empty()) {
                    group.maximumCopies = item.sourceRepeaterMaximumCopies;
                    group.geometryId = item.sourceGeometryId;
                    group.pathHash = item.localPath.hash;
                    group.observed.assign(group.maximumCopies, false);
                }
                require(item.sourceRepeaterMaximumCopies == group.maximumCopies
                            && item.sourceGeometryId == group.geometryId
                            && item.localPath.hash == group.pathHash,
                        "repeater copies did not share one geometry identity");
                require(item.sourceRepeaterCopyIndex < group.observed.size()
                            && !group.observed[item.sourceRepeaterCopyIndex],
                        "repeater copy index is duplicate or out of range");
                group.observed[item.sourceRepeaterCopyIndex] = true;
                require(item.sourceRepeaterTransformRevision != 0U
                            && item.sourceRepeaterOpacityRevision != 0U,
                        "repeater copy revisions were not published");
                if (item.sourceRepeaterCopyIndex
                    >= item.sourceRepeaterVisibleCopies) {
                    require(item.separatedOpacity == 0.0F,
                            "hidden repeater copy retained opacity");
                }
            }
            require(groups.size() == 6U,
                    "repeater fixture group count changed");
            for (const auto& [_, group] : groups) {
                require(!group.observed.empty(),
                        "repeater group has no copies");
                for (const bool observed : group.observed) {
                    require(observed, "repeater group missed an allocated copy");
                }
            }

            render::MotionRenderPlanner repeaterFramePlanner;
            const auto plan = repeaterFramePlanner.build(sample.scene);
            require(static_cast<bool>(plan), "repeater plan build failed");
            std::unordered_map<std::uint32_t, render::GeometryCacheKey>
                geometryByRepeater;
            std::unordered_map<std::uint32_t, render::PaintCacheKey>
                paintByRepeater;
            for (const auto& drawItem : plan.plan.drawItems) {
                require(drawItem.sourceDrawItemIndex
                            < sample.scene.drawItems.size(),
                        "repeater plan source index invalid");
                const auto& source = sample.scene.drawItems[
                    drawItem.sourceDrawItemIndex];
                if (!source.sourceRepeaterProjected) continue;
                const auto [it, inserted] = geometryByRepeater.emplace(
                    source.sourceRepeaterNode.value, drawItem.geometry);
                if (!inserted) {
                    require(it->second == drawItem.geometry,
                            "repeater copies use different geometry cache keys");
                }
                const auto [paintIt, paintInserted] = paintByRepeater.emplace(
                    source.sourceRepeaterNode.value, drawItem.paint);
                if (!paintInserted) {
                    require(paintIt->second == drawItem.paint,
                            "repeater copies use different paint cache keys");
                }
            }
        }

        constexpr std::size_t repeaterTargetFrame = 37U;
        repeaterWorkspace.resetHistory();
        ProjectedSample sequentialRepeater;
        for (std::size_t frame = 0U; frame <= repeaterTargetFrame; ++frame) {
            sequentialRepeater = evaluateProjected(
                *repeaterInstance.instance,
                repeaterEvaluator,
                repeaterWorkspace,
                repeaterProjector,
                frame,
                640U,
                &projectionWorkspace);
        }
        render::MotionRenderPlanner sequentialRepeaterPlanner;
        const auto sequentialRepeaterPlan = sequentialRepeaterPlanner.build(
            sequentialRepeater.scene);
        require(static_cast<bool>(sequentialRepeaterPlan),
                "sequential repeater plan failed");

        repeaterWorkspace.resetHistory();
        const auto directRepeater = evaluateProjected(
            *repeaterInstance.instance,
            repeaterEvaluator,
            repeaterWorkspace,
            repeaterProjector,
            repeaterTargetFrame,
            640U,
            &projectionWorkspace);
        require(directRepeater.scene.drawItems.size()
                    == sequentialRepeater.scene.drawItems.size(),
                "direct/sequential repeater item count differs");
        for (std::size_t index = 0U;
             index < directRepeater.scene.drawItems.size(); ++index) {
            const auto& directItem = directRepeater.scene.drawItems[index];
            const auto& sequentialItem = sequentialRepeater.scene.drawItems[index];
            require(directItem.localPath.hash == sequentialItem.localPath.hash
                        && directItem.sourceRepeaterNode
                            == sequentialItem.sourceRepeaterNode
                        && directItem.sourceRepeaterCopyIndex
                            == sequentialItem.sourceRepeaterCopyIndex
                        && directItem.sourceRepeaterTransformRevision
                            == sequentialItem.sourceRepeaterTransformRevision
                        && directItem.sourceRepeaterOpacityRevision
                            == sequentialItem.sourceRepeaterOpacityRevision,
                    "direct/sequential repeater state differs");
        }
        render::MotionRenderPlanner directRepeaterPlanner;
        const auto directRepeaterPlan = directRepeaterPlanner.build(
            directRepeater.scene);
        require(static_cast<bool>(directRepeaterPlan),
                "direct repeater plan failed");
        require(directRepeaterPlan.plan.drawItems.size()
                    == sequentialRepeaterPlan.plan.drawItems.size(),
                "direct/sequential repeater plan size differs");
        for (std::size_t index = 0U;
             index < directRepeaterPlan.plan.drawItems.size(); ++index) {
            require(directRepeaterPlan.plan.drawItems[index].geometry
                        == sequentialRepeaterPlan.plan.drawItems[index].geometry,
                    "direct/sequential repeater geometry cache key differs");
            require(directRepeaterPlan.plan.drawItems[index].paint
                        == sequentialRepeaterPlan.plan.drawItems[index].paint,
                    "direct/sequential repeater paint cache key differs");
        }

        render::MotionRenderPlanner repeatedRepeaterPlanner;
        const auto firstRepeaterPlan = repeatedRepeaterPlanner.build(
            directRepeater.scene);
        require(static_cast<bool>(firstRepeaterPlan),
                "first repeated repeater plan failed");
        const auto repeatedRepeater = evaluateProjected(
            *repeaterInstance.instance,
            repeaterEvaluator,
            repeaterWorkspace,
            repeaterProjector,
            repeaterTargetFrame,
            640U,
            &projectionWorkspace);
        const auto repeatedRepeaterPlan = repeatedRepeaterPlanner.build(
            repeatedRepeater.scene);
        require(static_cast<bool>(repeatedRepeaterPlan),
                "repeated repeater plan failed");
        require(repeatedRepeaterPlan.plan.geometryUpdates.empty(),
                "repeated repeater frame rebuilt shared geometry");
        require(repeatedRepeaterPlan.plan.paintUpdates.empty(),
                "repeated repeater frame rebuilt shared paint");
        require(projectionWorkspace.storageGeneration()
                    == preparedProjectionGeneration,
                "repeater workspace grew after direct/repeated evaluation");
    }

    // Part 17: extend one Repeater content group to nested direct groups,
    // multiple authored paint applications and local solid strokes. Geometry
    // must be shared by fill/stroke applications while each authored paint
    // retains its own cache identity across all generated copies.
    {
        const auto fixture = fixtures / "repeater_content_group.json";
        runtime::Runtime contentRuntime;
        auto contentLoaded = contentRuntime.loadLottieJson(
            readText(fixture), fixture.filename().string());
        require(static_cast<bool>(contentLoaded),
                "repeater content-group fixture load failed");
        const auto contentModel = contentLoaded.asset->prepareModel();
        require(static_cast<bool>(contentModel),
                "repeater content-group model failed");
        auto contentInstance = contentRuntime.createInstance(
            contentLoaded.asset);
        require(static_cast<bool>(contentInstance),
                "repeater content-group instance failed");
        evaluation::PropertyEvaluator contentEvaluator{contentModel.model};
        evaluation::PropertyEvaluationWorkspace contentWorkspace;
        contentEvaluator.prepare(contentWorkspace);
        render::SourceGeometryProjector contentProjector{contentModel.model};
        require(contentProjector.valid(),
                "repeater content-group projector invalid");
        render::SourceGeometryProjectionWorkspace contentProjectionWorkspace;
        require(contentProjector.prepare(contentProjectionWorkspace),
                "repeater content-group workspace prepare failed");
        const auto preparedGeneration =
            contentProjectionWorkspace.storageGeneration();

        for (std::size_t frame = 0U; frame <= 60U; ++frame) {
            const auto sample = evaluateProjected(
                *contentInstance.instance,
                contentEvaluator,
                contentWorkspace,
                contentProjector,
                frame,
                640U,
                &contentProjectionWorkspace);
            const auto& statistics = sample.statistics;
            require(statistics.repeaterCandidates == 34U
                        && statistics.repeaterCopiesVisited == 34U
                        && statistics.repeaterCopiesProjected == 34U,
                    "content-group Repeater copy count changed");
            require(statistics.projected == 34U
                        && statistics.projectedStatic == 34U
                        && statistics.projectedAnimated == 0U,
                    "content-group Repeater geometry scope changed");
            require(statistics.repeaterFillApplicationsProjected == 17U
                        && statistics.repeaterStrokeApplicationsProjected == 17U,
                    "content-group Repeater paint application count changed");
            require(statistics.repeaterNestedPaintApplicationsProjected == 16U,
                    "nested Repeater paint application count changed");
            require(statistics.repeaterAnimatedPaintApplicationsProjected == 6U,
                    "animated Repeater paint application count changed");
            require(statistics.skippedRepeaterBinding == 0U
                        && statistics.skippedRepeaterProperties == 0U
                        && statistics.skippedRepeaterEvaluation == 0U
                        && statistics.rejectedRepeaterInput == 0U
                        && statistics.repeaterParityMismatches == 0U,
                    "content-group Repeater fell back from parity path");
            require(contentProjectionWorkspace.storageGeneration()
                        == preparedGeneration,
                    "content-group Repeater workspace grew after prepare");

            render::MotionRenderPlanner contentFramePlanner;
            const auto planned = contentFramePlanner.build(sample.scene);
            require(static_cast<bool>(planned),
                    "content-group Repeater plan failed");
            require(planned.plan.drawItems.size() == 34U,
                    "content-group Repeater plan item count changed");

            std::unordered_map<std::uint64_t, render::GeometryCacheKey>
                geometryKeys;
            std::unordered_map<std::uint64_t, std::vector<std::uint32_t>>
                paintsByGeometry;
            std::unordered_map<std::uint32_t, render::PaintCacheKey> paintKeys;
            std::unordered_map<std::uint32_t, std::vector<bool>> copiesByPaint;
            std::unordered_map<std::uint32_t, bool> repeaters;
            std::size_t strokes = 0U;
            std::size_t fills = 0U;
            std::size_t staticPaints = 0U;
            std::size_t animatedPaints = 0U;
            for (const auto& drawItem : planned.plan.drawItems) {
                require(drawItem.sourceDrawItemIndex
                            < sample.scene.drawItems.size(),
                        "content-group Repeater source index invalid");
                const auto& source = sample.scene.drawItems[
                    drawItem.sourceDrawItemIndex];
                require(source.sourceRepeaterProjected
                            && source.sourceGeometryProjected,
                        "content-group draw item remained on Telegram fallback");
                require(source.localPaintAvailable
                            && source.opacitySeparated,
                        "content-group draw item lacks local paint seam");
                require(source.geometryOrigin
                            == runtime::EvaluatedValueOrigin::AssetStatic
                            && source.sourceGeometryRevision == 0U,
                        "content-group geometry was not shared asset state");
                repeaters[source.sourceRepeaterNode.value] = true;

                const auto [geometryIt, geometryInserted] = geometryKeys.emplace(
                    source.sourceGeometryId, drawItem.geometry);
                if (!geometryInserted) {
                    require(geometryIt->second == drawItem.geometry,
                            "fill/stroke copies use different geometry keys");
                }
                auto& geometryPaints = paintsByGeometry[
                    source.sourceGeometryId];
                if (std::find(
                        geometryPaints.begin(),
                        geometryPaints.end(),
                        source.sourcePaintNode.value) == geometryPaints.end()) {
                    geometryPaints.push_back(source.sourcePaintNode.value);
                }

                const auto [paintIt, paintInserted] = paintKeys.emplace(
                    source.sourcePaintNode.value, drawItem.paint);
                if (!paintInserted) {
                    require(paintIt->second == drawItem.paint,
                            "copies of one authored paint use different keys");
                }
                auto& observed = copiesByPaint[source.sourcePaintNode.value];
                if (observed.empty()) {
                    observed.assign(source.sourceRepeaterMaximumCopies, false);
                }
                require(source.sourceRepeaterCopyIndex < observed.size()
                            && !observed[source.sourceRepeaterCopyIndex],
                        "paint application copy index is duplicate or invalid");
                observed[source.sourceRepeaterCopyIndex] = true;

                if (source.localStroke.enabled) {
                    ++strokes;
                    require(source.localStroke.width > 0.0F
                                && source.stroke.enabled,
                            "local stroke was not preserved");
                } else {
                    ++fills;
                    require(!source.stroke.enabled,
                            "fill application unexpectedly retained stroke");
                }
                if (source.paintOrigin
                    == runtime::EvaluatedValueOrigin::AssetStatic) {
                    ++staticPaints;
                } else {
                    ++animatedPaints;
                }
            }
            require(repeaters.size() == 4U,
                    "content-group Repeater group count changed");
            require(geometryKeys.size() == 5U,
                    "content-group geometry sharing count changed");
            require(paintKeys.size() == 10U,
                    "content-group authored paint count changed");
            require(strokes == 17U && fills == 17U,
                    "content-group fill/stroke draw count changed");
            require(staticPaints == 28U && animatedPaints == 6U,
                    "content-group static/animated paint scope changed");
            for (const auto& [_, paints] : paintsByGeometry) {
                require(paints.size() == 2U,
                        "one path set is not shared by fill and stroke");
            }
            for (const auto& [_, observed] : copiesByPaint) {
                require(!observed.empty(), "paint application has no copies");
                for (const bool value : observed) {
                    require(value, "paint application missed a generated copy");
                }
            }
        }

        constexpr std::size_t contentTargetFrame = 37U;
        contentWorkspace.resetHistory();
        ProjectedSample contentSequential;
        for (std::size_t frame = 0U; frame <= contentTargetFrame; ++frame) {
            contentSequential = evaluateProjected(
                *contentInstance.instance,
                contentEvaluator,
                contentWorkspace,
                contentProjector,
                frame,
                640U,
                &contentProjectionWorkspace);
        }
        render::MotionRenderPlanner contentSequentialPlanner;
        const auto contentSequentialPlan = contentSequentialPlanner.build(
            contentSequential.scene);
        require(static_cast<bool>(contentSequentialPlan),
                "sequential content-group plan failed");

        contentWorkspace.resetHistory();
        const auto contentDirect = evaluateProjected(
            *contentInstance.instance,
            contentEvaluator,
            contentWorkspace,
            contentProjector,
            contentTargetFrame,
            640U,
            &contentProjectionWorkspace);
        render::MotionRenderPlanner contentDirectPlanner;
        const auto contentDirectPlan = contentDirectPlanner.build(
            contentDirect.scene);
        require(static_cast<bool>(contentDirectPlan),
                "direct content-group plan failed");
        require(contentDirectPlan.plan.drawItems.size()
                    == contentSequentialPlan.plan.drawItems.size(),
                "direct/sequential content-group plan size differs");
        for (std::size_t index = 0U;
             index < contentDirectPlan.plan.drawItems.size(); ++index) {
            require(contentDirectPlan.plan.drawItems[index].geometry
                        == contentSequentialPlan.plan.drawItems[index].geometry,
                    "direct/sequential content-group geometry key differs");
            require(contentDirectPlan.plan.drawItems[index].paint
                        == contentSequentialPlan.plan.drawItems[index].paint,
                    "direct/sequential content-group paint key differs");
            require(sameMatrix(
                        contentDirectPlan.plan.drawItems[index].geometryTransform,
                        contentSequentialPlan.plan.drawItems[index].geometryTransform),
                    "direct/sequential content-group transform differs");
            require(contentDirectPlan.plan.drawItems[index].effectiveOpacity
                        == contentSequentialPlan.plan.drawItems[index].effectiveOpacity,
                    "direct/sequential content-group opacity differs");
        }

        render::MotionRenderPlanner repeatedPlanner;
        const auto firstContentPlan = repeatedPlanner.build(
            contentDirect.scene);
        require(static_cast<bool>(firstContentPlan),
                "first repeated content-group plan failed");
        const auto contentRepeated = evaluateProjected(
            *contentInstance.instance,
            contentEvaluator,
            contentWorkspace,
            contentProjector,
            contentTargetFrame,
            640U,
            &contentProjectionWorkspace);
        const auto contentRepeatedPlan = repeatedPlanner.build(
            contentRepeated.scene);
        require(static_cast<bool>(contentRepeatedPlan),
                "repeated content-group plan failed");
        require(contentRepeatedPlan.plan.geometryUpdates.empty(),
                "repeated content-group frame rebuilt shared geometry");
        require(contentRepeatedPlan.plan.paintUpdates.empty(),
                "repeated content-group frame rebuilt paint resources");
        require(contentProjectionWorkspace.storageGeneration()
                    == preparedGeneration,
                "content-group workspace grew after repeated evaluation");
    }


    std::cout
        << "AveMotion source geometry projection passed: projected="
        << total.projected
        << " static=" << total.projectedStatic
        << " animated=" << total.projectedAnimated
        << " points=" << total.projectedPoints
        << " rectangles=" << total.projectedRectangles
        << " rounded=" << total.projectedRoundedRectangles
        << " ellipses=" << total.projectedEllipses
        << " stars=" << total.projectedStars
        << " polygons=" << total.projectedPolygons
        << " trimmed=" << total.projectedTrimmed
        << " trim-splits=" << total.trimSplitCount
        << " skipped-modified=" << total.skippedModified
        << " skipped-multiple=" << total.skippedMultiplePaths
        << '\n';
    return EXIT_SUCCESS;
}
