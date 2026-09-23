#include "avemotion/reference/ReferenceRuntime.hpp"
#include "avemotion/runtime/RecordingBackend.hpp"
#include "avemotion/runtime/Runtime.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <type_traits>
#include <vector>

using avemotion::runtime::DiagnosticsSnapshot;
static_assert(std::is_same_v<decltype(DiagnosticsSnapshot{}.referenceMetadataSessionsCreated), std::uint64_t>);
static_assert(std::is_same_v<decltype(DiagnosticsSnapshot{}.referenceSceneSessionsCreated), std::uint64_t>);
static_assert(std::is_same_v<decltype(DiagnosticsSnapshot{}.referenceModelSessionsCreated), std::uint64_t>);
static_assert(std::is_same_v<decltype(DiagnosticsSnapshot{}.referenceCpuSessionsCreated), std::uint64_t>);
static_assert(std::is_same_v<decltype(DiagnosticsSnapshot{}.referenceSceneSamples), std::uint64_t>);
static_assert(std::is_same_v<decltype(DiagnosticsSnapshot{}.referenceModelSamples), std::uint64_t>);

namespace fs = std::filesystem;

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

std::string readText(const fs::path& path) {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        fail("unable to open " + path.string());
    }
    return {
        std::istreambuf_iterator<char>{stream},
        std::istreambuf_iterator<char>{}};
}

void verifyReferenceDiagnostics() {
    avemotion::runtime::Runtime runtime;
    runtime.resetDiagnostics();
    const auto json = readText(fs::path{AVEMOTION_CORPUS_DIR} / "StickAndBall.json");
    auto loaded = runtime.loadLottieJson(json, "diagnostic-roles");
    require(static_cast<bool>(loaded), "diagnostic fixture failed to load");
    auto counters = runtime.diagnostics();
    require(counters.referenceMetadataSessionsCreated == 1U,
            "asset load must create one metadata session");
    require(counters.referenceSceneSessionsCreated == 0U
                && counters.referenceModelSessionsCreated == 0U
                && counters.referenceCpuSessionsCreated == 0U,
            "asset load must not create non-metadata sessions");

    auto created = runtime.createInstance(loaded.asset);
    require(static_cast<bool>(created), "diagnostic fixture instance failed to create");
    counters = runtime.diagnostics();
    require(counters.referenceSceneSessionsCreated == 1U,
            "instance creation must create one scene session");

    const auto exact = created.instance->evaluateFrame(0U, 128U, 128U);
    require(static_cast<bool>(exact), "diagnostic exact scene evaluation failed");
    counters = runtime.diagnostics();
    // Task 3 replaces this pre-optimization characterization with zero hot-path creations.
    require(counters.referenceSceneSessionsCreated == 2U,
            "current exact scene evaluation must create a scene-role session");
    require(counters.referenceSceneSamples == 1U
                && counters.referenceModelSamples == 0U,
            "exact scene sample must have the scene role");

    const auto cpuFirst = created.instance->renderCpuFrame(0U, 64U, 64U);
    const auto cpuSecond = created.instance->renderCpuFrame(1U, 64U, 64U);
    require(cpuFirst && cpuSecond, "diagnostic CPU renders failed");
    counters = runtime.diagnostics();
    require(counters.referenceCpuSessionsCreated == 1U,
            "repeated CPU renders must reuse the lazy CPU session");

    runtime.resetDiagnostics();
    counters = runtime.diagnostics();
    require(counters.referenceMetadataSessionsCreated == 0U
                && counters.referenceSceneSessionsCreated == 0U
                && counters.referenceModelSessionsCreated == 0U
                && counters.referenceCpuSessionsCreated == 0U
                && counters.referenceSceneSamples == 0U
                && counters.referenceModelSamples == 0U,
            "diagnostic reset must zero the current counter epoch");
    require(static_cast<bool>(created.instance->renderCpuFrame(2U, 64U, 64U)),
            "CPU session must remain usable after diagnostic reset");
    require(runtime.diagnostics().referenceCpuSessionsCreated == 0U,
            "diagnostic reset must not recreate a live CPU session");

    const auto prepared = loaded.asset->prepareModel();
    require(static_cast<bool>(prepared), "diagnostic fixture model preparation failed");
    counters = runtime.diagnostics();
    require(counters.referenceModelSessionsCreated == loaded.asset->metadata().totalFrames,
            "current model preparation must classify per-frame sessions as model");
    require(counters.referenceModelSamples == loaded.asset->metadata().totalFrames,
            "model preparation must count its renderTree samples");
    require(counters.referenceSceneSessionsCreated == 0U
                && counters.referenceSceneSamples == 0U
                && counters.sceneEvaluations == 0U,
            "model preparation must not count as instance scene evaluation");
    require(static_cast<bool>(loaded.asset->prepareModel()),
            "repeated model preparation failed");
    require(runtime.diagnostics().referenceModelSessionsCreated
                == counters.referenceModelSessionsCreated,
            "repeated model preparation must reuse the prepared model");
}
} // namespace

int main() {
    require(avemotion::runtime::Runtime::compiledWithReferenceEngine(),
            "runtime seams require a selected rlottie variant");

    verifyReferenceDiagnostics();

    avemotion::runtime::Runtime runtime;
    avemotion::runtime::RecordingBackend recorder;
    avemotion::reference::ReferenceRuntime reference;

    const auto invalid = runtime.loadLottieJson("{}", "invalid");
    require(!invalid, "malformed asset must be rejected");
    require(invalid.error.code == avemotion::runtime::RuntimeErrorCode::InvalidAsset,
            "malformed asset must report InvalidAsset");

    std::vector<fs::path> assets;
    for (const auto& entry : fs::directory_iterator{AVEMOTION_CORPUS_DIR}) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            assets.push_back(entry.path());
        }
    }
    std::sort(assets.begin(), assets.end());
    require(assets.size() >= 8U, "the characterization corpus is incomplete");

    for (const auto& path : assets) {
        const auto json = readText(path);
        auto loaded = runtime.loadLottieJson(json, path.filename().string());
        require(static_cast<bool>(loaded), "runtime asset load failed for " + path.string());
        auto first = runtime.createInstance(loaded.asset);
        auto second = runtime.createInstance(loaded.asset);
        require(first && second, "instance creation failed for " + path.string());
        require(first.instance->id() != second.instance->id(),
                "independent instances need unique IDs");

        auto referenceLoaded = reference.loadJson(
            json, path.filename().string() + "-reference", false);
        require(static_cast<bool>(referenceLoaded), "reference load failed for " + path.string());
        const auto& metadata = loaded.asset->metadata();
        require(metadata.width == referenceLoaded.animation->metadata().width,
                "asset width differs from the CPU oracle");
        require(metadata.height == referenceLoaded.animation->metadata().height,
                "asset height differs from the CPU oracle");
        require(metadata.totalFrames == referenceLoaded.animation->metadata().totalFrames,
                "asset frame count differs from the CPU oracle");

        const auto middle = metadata.totalFrames / 2U;
        auto frame0 = first.instance->evaluateFrame(0U, 128U, 128U);
        auto middleA = first.instance->evaluateFrame(middle, 128U, 128U);
        auto middleRepeat = first.instance->evaluateFrame(middle, 128U, 128U);
        auto last = first.instance->evaluatePosition(1.0, 128U, 128U);
        auto frame0AfterSeek = first.instance->evaluateFrame(0U, 128U, 128U);
        auto middleIndependent = second.instance->evaluateFrame(middle, 128U, 128U);
        require(frame0 && middleA && middleRepeat && last
                    && frame0AfterSeek && middleIndependent,
                "scene evaluation failed for " + path.string());
        require(!middleA.scene.layers.empty(), "evaluated scene has no root layer");
        require(middleA.scene.frameIndex == middle,
                "evaluated frame index does not match the request");
        require(last.scene.frameIndex == metadata.totalFrames - 1U,
                "normalized position 1.0 must map to the last valid frame");

        const auto frame0Record = recorder.record(frame0.scene);
        const auto frame0SeekRecord = recorder.record(frame0AfterSeek.scene);
        const auto middleRecord = recorder.record(middleA.scene);
        const auto repeatRecord = recorder.record(middleRepeat.scene);
        const auto independentRecord = recorder.record(middleIndependent.scene);
        require(frame0Record.fingerprints.scene
                    == frame0SeekRecord.fingerprints.scene,
                "random seek changed the recorded frame-zero scene");
        require(middleRecord.fingerprints.scene
                    == repeatRecord.fingerprints.scene,
                "repeated same-frame evaluation changed the scene");
        require(middleRecord.fingerprints.scene
                    == independentRecord.fingerprints.scene,
                "independent instances disagree on evaluated scene");
        require(!middleRepeat.scene.changes.firstEvaluation,
                "repeated evaluation was incorrectly classified as first");
        require(!middleRepeat.scene.changes.visualChanged,
                "repeated same-frame evaluation must be visually unchanged");
        require(!middleRepeat.scene.changes.geometryChanged,
                "repeated same-frame evaluation must not change geometry");
        require(!middleRepeat.scene.changes.paintChanged,
                "repeated same-frame evaluation must not change paint");

        auto cpu = first.instance->renderCpuFrame(middle, 96U, 96U);
        const auto oracle = referenceLoaded.animation->renderFrame(
            middle, 96U, 96U);
        require(static_cast<bool>(cpu), "isolated CPU oracle render failed");
        require(cpu.frame.fnv1a64 == oracle.fnv1a64,
                "AveMotion instance CPU oracle differs from ReferenceRuntime");

        const auto zeroSize = first.instance->evaluateFrame(middle, 0U, 128U);
        require(!zeroSize, "zero-width scene evaluation must fail");
    }

    avemotion::runtime::Runtime foreignRuntime;
    const auto foreignJson = readText(assets.front());
    auto foreignAsset = foreignRuntime.loadLottieJson(foreignJson, "foreign");
    require(static_cast<bool>(foreignAsset), "foreign runtime asset load failed");
    const auto rejected = runtime.createInstance(foreignAsset.asset);
    require(!rejected, "runtime must reject an asset from another runtime");

    const auto diagnostics = runtime.diagnostics();
    require(diagnostics.assetLoadAttempts >= assets.size() + 1U,
            "asset load diagnostics did not advance");
    require(diagnostics.instancesCreated >= assets.size() * 2U,
            "instance diagnostics did not advance");
    require(diagnostics.sceneEvaluations >= assets.size() * 6U,
            "scene evaluation diagnostics did not advance");
    require(diagnostics.cpuFramesRendered >= assets.size(),
            "CPU oracle diagnostics did not advance");
    require(diagnostics.evaluatedLayers > 0U,
            "evaluated layer diagnostics are empty");
    require(diagnostics.copiedPathPoints > 0U,
            "path-copy diagnostics are empty");

    std::cout << "AveMotion runtime seam tests passed for " << assets.size()
              << " assets\n";
    return EXIT_SUCCESS;
}
