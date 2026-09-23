#include "avemotion/evaluation/PropertyEvaluator.hpp"
#include "avemotion/render/RenderPlanner.hpp"
#include "avemotion/render/SourceGeometryProjector.hpp"
#include "avemotion/runtime/Runtime.hpp"
#include "support/CaptureCorpus.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#ifndef AVEMOTION_CORPUS_DIR
#error "AVEMOTION_CORPUS_DIR is required"
#endif
#ifndef AVEMOTION_FIXTURE_DIR
#error "AVEMOTION_FIXTURE_DIR is required"
#endif

namespace {

namespace fs = std::filesystem;

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

void require(bool condition, const std::string& message) {
    if (!condition) fail(message);
}

std::string readText(const fs::path& path) {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) fail("unable to open " + path.string());
    return {
        std::istreambuf_iterator<char>{stream},
        std::istreambuf_iterator<char>{}};
}

fs::path assetPath(const avemotion::testsupport::CaptureAssetSpec& asset) {
    return (asset.location
            == avemotion::testsupport::CaptureAssetLocation::Corpus
        ? fs::path{AVEMOTION_CORPUS_DIR}
        : fs::path{AVEMOTION_FIXTURE_DIR})
        / asset.fileName;
}

} // namespace

int main() {
    using namespace avemotion;
    using namespace avemotion::testsupport;

    std::size_t cases = 0U;
    std::size_t drawItems = 0U;
    std::size_t projectedItems = 0U;

    for (const auto& assetSpec : kDirect2DCaptureAssets) {
        runtime::Runtime runtime;
        const auto path = assetPath(assetSpec);
        auto loaded = runtime.loadLottieJson(
            readText(path), path.filename().string());
        require(static_cast<bool>(loaded),
                "capture asset load failed: " + path.string());
        const auto prepared = loaded.asset->prepareModel();
        require(static_cast<bool>(prepared),
                "capture asset model failed: " + path.string());
        auto created = runtime.createInstance(loaded.asset);
        require(static_cast<bool>(created),
                "capture instance creation failed: " + path.string());

        evaluation::PropertyEvaluator evaluator{prepared.model};
        require(evaluator.valid(),
                "capture property evaluator invalid: " + path.string());
        evaluation::PropertyEvaluationWorkspace propertyWorkspace;
        evaluator.prepare(propertyWorkspace);
        render::SourceGeometryProjector projector{prepared.model};
        require(projector.valid(),
                "capture source projector invalid: " + path.string());
        render::SourceGeometryProjectionWorkspace projectionWorkspace;
        require(projector.prepare(projectionWorkspace),
                "capture projection workspace preparation failed");
        const auto propertyStorage = propertyWorkspace.storageGeneration();
        const auto projectionStorage = projectionWorkspace.storageGeneration();

        for (const auto& profile : kDirect2DCaptureProfiles) {
            render::MotionRenderPlanner planner;
            for (const auto& sample : kDirect2DCaptureSamples) {
                const auto frame = created.instance->frameAtPosition(
                    sample.normalizedPosition);
                const auto properties = evaluator.evaluate(
                    static_cast<double>(frame), propertyWorkspace);
                require(static_cast<bool>(properties),
                        "capture property evaluation failed");
                auto evaluated = created.instance->evaluateModelFrame(
                    frame, profile.logicalWidth, profile.logicalHeight);
                require(static_cast<bool>(evaluated),
                        "capture scene evaluation failed");
                const auto projected = projector.project(
                    evaluated.scene, properties, projectionWorkspace);
                require(static_cast<bool>(projected),
                        "capture source geometry projection failed");
                require(projected.statistics.projected != 0U,
                        "capture case did not use any AveMotion-owned geometry");

                auto planned = planner.build(std::move(evaluated.scene));
                require(static_cast<bool>(planned),
                        "capture render-plan build failed");
                require(!planned.plan.drawItems.empty(),
                        "capture render plan is empty");
                require(planned.plan.statistics.unsupportedFeatureItemCount == 0U,
                        "capture asset contains unsupported Direct2D features");
                require(planned.plan.statistics.visibleDrawItemCount
                            == planned.plan.drawItems.size(),
                        "capture render plan dropped visible items");
                require(planned.plan.sourceScene != nullptr,
                        "capture plan lost its evaluated scene");

                drawItems += planned.plan.drawItems.size();
                projectedItems += projected.statistics.projected;
                ++cases;
            }
        }
        require(propertyWorkspace.storageGeneration() == propertyStorage,
                "capture property workspace grew after prepare");
        require(projectionWorkspace.storageGeneration() == projectionStorage,
                "capture projection workspace grew after prepare");
    }

    require(cases == kDirect2DCaptureCaseCount,
            "capture preflight case count changed");
    std::cout << "AveMotion Direct2D capture preflight passed\n"
              << "cases=" << cases << '\n'
              << "drawItems=" << drawItems << '\n'
              << "projectedItems=" << projectedItems << '\n';
    return EXIT_SUCCESS;
}
