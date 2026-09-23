#include "support/FreshModelOracle.hpp"
#include "support/ExactSceneComparison.hpp"
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <latch>
#include <vector>

namespace {
using namespace avemotion;
void require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}
std::string read(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    require(bool(input), "open " + path.string());
    return {std::istreambuf_iterator<char>{input}, {}};
}
bool bindings(const runtime::EvaluatedScene& scene) {
    require(scene.assetModelApplied && bool(scene.assetModel), "model applied");
    bool staticGeometry = false, staticPaint = false, sourcePath = false, sourcePaint = false;
    for (const auto& item : scene.drawItems) {
        require(scene.assetModel->node(item.modelNode)
                && scene.assetModel->geometry(item.modelGeometry)
                && scene.assetModel->paint(item.modelPaint), "resolved model IDs");
        if (item.sourcePathNode.valid()) {
            require(scene.assetModel->sourceNode(item.sourcePathNode) != nullptr, "resolved source path");
            sourcePath = true;
        }
        if (item.sourcePaintNode.valid()) {
            require(scene.assetModel->sourceNode(item.sourcePaintNode) != nullptr, "resolved source paint");
            sourcePaint = true;
        }
        if (item.canonicalGeometry) {
            const auto* row = scene.assetModel->geometry(item.modelGeometry);
            require(row->staticValue && item.canonicalGeometry.get() == &*row->staticValue,
                "static geometry address belongs to own model");
            require(!item.canonicalGeometry.owner_before(scene.assetModel)
                    && !scene.assetModel.owner_before(item.canonicalGeometry), "geometry alias owner");
            staticGeometry = true;
        }
        if (item.canonicalPaint) {
            const auto* row = scene.assetModel->paint(item.modelPaint);
            require(row->staticValue && item.canonicalPaint.get() == &*row->staticValue,
                "static paint address belongs to own model");
            require(!item.canonicalPaint.owner_before(scene.assetModel)
                    && !scene.assetModel.owner_before(item.canonicalPaint), "paint alias owner");
            staticPaint = true;
        }
    }
    return staticGeometry && staticPaint && sourcePath && sourcePaint;
}
bool parity(const std::filesystem::path& path, bool sampleBeforePrepare) {
    // Unique exact source bytes prevent an earlier test's stamped cache entry
    // from hiding cold constructor-to-preparation ordering.
    auto json = read(path) + (sampleBeforePrepare ? "\n \t" : "\n\t ");
    runtime::Runtime runtime;
    auto loaded = runtime.loadLottieJson(json, path.filename().string());
    require(bool(loaded), "load " + path.string());
    auto created = runtime.createInstance(loaded.asset);
    require(bool(created), "create instance");
    runtime::Instance::SceneResult cold;
    if (sampleBeforePrepare) {
        cold = created.instance->evaluateFrame(0, 128, 128);
        require(bool(cold), "cold exact sample");
        for (const auto& item : cold.scene.drawItems)
            require(!item.sourcePathNode.valid() && !item.sourcePaintNode.valid(), "cold source IDs unstamped");
    }
    const auto prepared = loaded.asset->prepareModel();
    require(bool(prepared), path.string() + ": prepare: " + prepared.error);
    test::FreshModelOracle oracle{json, *loaded.asset};
    const auto expectedModel = oracle.build();
    require(prepared.model->assetHandle == loaded.asset->handle()
            && expectedModel->assetHandle == loaded.asset->handle(), "independent model asset handles");
    const auto frames = loaded.asset->metadata().totalFrames;
    bool liveStaticAliases = false;
    for (std::size_t frame : {std::size_t{0}, frames / 2, frames - 1, std::size_t{0}, frames / 2}) {
        for (const auto width : {128U, 79U}) {
            auto expected = oracle.scene(frame, width, 113);
            auto actual = created.instance->evaluateModelFrame(frame, width, 113);
            require(bool(expected) && bool(actual), "modeled scene sample");
            auto applied = model::detail::applyAssetModel(expectedModel, expected.scene);
            require(bool(applied), "oracle model application");
            const auto difference = test::ExactSceneComparison{}.difference(expected.scene, actual.scene);
            require(difference.empty(), path.string() + ": whole-model/scene parity: " + difference);
            try {
                const bool expectedLive = bindings(expected.scene);
                const bool actualLive = bindings(actual.scene);
                require(expectedLive == actualLive, "live alias correspondence");
                liveStaticAliases = liveStaticAliases || (expectedLive && actualLive);
            } catch (const std::exception& error) {
                throw std::runtime_error(path.string() + " frame=" + std::to_string(frame)
                    + " width=" + std::to_string(width) + ": " + error.what());
            }
        }
    }
    if (sampleBeforePrepare) for (const auto& item : cold.scene.drawItems)
        require(!item.sourcePathNode.valid() && !item.sourcePaintNode.valid(), "old copied cold scene unchanged");
    require(loaded.asset->prepareModel().model == prepared.model, "repeat publication identity");
    const auto counts = runtime.diagnostics();
    require(counts.referenceSceneSessionsCreated == 1 && counts.referenceModelSessionsCreated == 1
            && counts.referenceModelSamples == frames && counts.referenceCpuSessionsCreated == 0
            && counts.assetModelBuildAttempts == 1 && counts.assetModelBuildsSucceeded == 1,
        "one scene and model session, exactly one complete scan");
    runtime.resetDiagnostics();
    require(bool(created.instance->evaluateModelFrame(0, 128, 128)), "post-reset model scene");
    const auto quiet = runtime.diagnostics();
    require(quiet.referenceSceneSessionsCreated == 0 && quiet.referenceModelSessionsCreated == 0
            && quiet.referenceMetadataSessionsCreated == 0 && quiet.referenceCpuSessionsCreated == 0
            && quiet.referenceModelSamples == 0 && quiet.referenceSceneSamples == 1,
        "prepared sampling performs no hidden model/session work after reset");
    if (path.filename() == "primitive_geometry.json")
        require(liveStaticAliases, "authored-static primitive fixture must exercise both canonical aliases");
    return liveStaticAliases;
}
void lateFailure() {
    const auto json = read(std::filesystem::path{AVEMOTION_FIXTURE_DIR} / "reference_sessions/late-overflow.json");
    runtime::Runtime runtime;
    auto loaded = runtime.loadLottieJson(json, "late-overflow");
    require(bool(loaded), "late fixture load");
    auto instance = runtime.createInstance(loaded.asset);
    require(bool(instance), "late fixture instance");
    test::FreshModelOracle oracle{json, *loaded.asset};
    std::string failure;
    runtime::Instance::SceneResult initial;
    for (std::size_t frame : {0U, 1U, 0U}) {
        auto expected = oracle.scene(frame, 128, 128);
        auto actual = instance.instance->evaluateFrame(frame, 128, 128);
        require(bool(actual) == (frame == 0) && bool(actual) == bool(expected), "late error/recovery state");
        require(actual.error.code == expected.error.code && actual.error.message == expected.error.message,
            "late typed ordinary error parity");
        if (actual) {
            // Runtime also promotes its own canonical resources; compare that
            // complete result before/after failure, and ordinary typed errors above.
            if (!failure.empty()) {
                const auto difference = test::ExactSceneComparison{}.difference(initial.scene, actual.scene);
                require(difference.empty(), "late recovery complete scene parity: " + difference);
            } else initial = actual;
        }
        else failure = actual.error.message;
    }
    runtime.resetDiagnostics();
    for (std::uint64_t attempt = 1; attempt <= 2; ++attempt) {
        auto prepared = loaded.asset->prepareModel();
        require(!prepared && !loaded.asset->model() && prepared.error == failure, "late failure never publishes");
        const auto counts = runtime.diagnostics();
        require(counts.referenceModelSessionsCreated == attempt && counts.referenceModelSamples == attempt * 2
                && counts.assetModelBuildAttempts == attempt && counts.assetModelBuildsFailed == attempt
                && counts.assetModelBuildsSucceeded == 0 && counts.referenceSceneSessionsCreated == 0,
            "one model session and two attempted samples per failed retry");
    }
}
void singleFlight() {
    runtime::Runtime runtime;
    auto loaded = runtime.loadLottieJson(read(std::filesystem::path{AVEMOTION_CORPUS_DIR} / "StickAndBall.json"), "single-flight");
    require(bool(loaded), "single-flight load");
    std::latch start{4};
    std::vector<std::future<model::AssetModelResult>> pending;
    for (int i = 0; i < 4; ++i) pending.push_back(std::async(std::launch::async, [&] {
        start.arrive_and_wait();
        return loaded.asset->prepareModel();
    }));
    std::shared_ptr<const model::MotionAssetModel> published;
    for (auto& future : pending) {
        auto result = future.get();
        require(bool(result), "concurrent successful prepare");
        if (!published) published = result.model;
        require(result.model == published, "same-Asset concurrent pointer identity");
    }
    const auto c = runtime.diagnostics();
    require(c.assetModelBuildAttempts == 1 && c.assetModelBuildsSucceeded == 1 && c.assetModelBuildsFailed == 0
            && c.referenceModelSessionsCreated == 1 && c.referenceModelSamples == loaded.asset->metadata().totalFrames,
        "same-Asset single-flight counts");
}
} // namespace
int main() {
    try {
        lateFailure();
        singleFlight();
        std::vector<std::filesystem::path> paths;
        for (const auto& directory : {AVEMOTION_CORPUS_DIR, AVEMOTION_FIXTURE_DIR})
            for (const auto& entry : std::filesystem::directory_iterator{directory})
                if (entry.path().extension() == ".json") paths.push_back(entry.path());
        require(paths.size() == 16, "complete top-level smoke corpus");
        std::sort(paths.begin(), paths.end());
        for (const char* name : {"translation-near-default.json", "opacity.json", "width.json",
                "dashed_stroke_session.json", "recording-zero-dash.json", "recording-skipped-masks.json",
                "recording-nested-repeater.json", "recording-outer-trim.json"})
            paths.push_back(std::filesystem::path{AVEMOTION_FIXTURE_DIR} / "reference_sessions" / name);
        std::size_t liveFixtures = 0;
        for (const auto& path : paths) {
            const bool first = parity(path, true);
            const bool second = parity(path, false);
            if (first && second) {
                ++liveFixtures;
                std::cout << "Live static geometry+paint aliases: " << path.filename().string() << '\n';
            }
        }
        require(liveFixtures > 0, "at least one fixture exercises live static geometry/paint and source IDs in both orders");
        std::cout << "persistent model sessions passed for " << paths.size() << " fixtures and both cold orders\n";
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
