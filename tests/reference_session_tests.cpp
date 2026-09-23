#include "avemotion/reference/UpstreamInfo.hpp"
#include "avemotion/runtime/Runtime.hpp"
#include "support/ExactSceneComparison.hpp"

#include <array>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
using namespace avemotion::runtime;
namespace fs = std::filesystem;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error{message};
}

std::string readText(const fs::path& path) {
    std::ifstream input{path, std::ios::binary};
    require(static_cast<bool>(input), "unable to open " + path.string());
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

std::shared_ptr<const Asset> load(Runtime& runtime, const fs::path& path) {
    auto result = runtime.loadLottieJson(readText(path), path.filename().string());
    require(static_cast<bool>(result), "load " + path.string() + ": " + result.error.message);
    require(result.asset->metadata().totalFrames > 1U, "fixture must contain multiple source frames");
    return result.asset;
}

std::unique_ptr<Instance> create(Runtime& runtime, const std::shared_ptr<const Asset>& asset) {
    auto result = runtime.createInstance(asset);
    require(static_cast<bool>(result), "create instance: " + result.error.message);
    return std::move(result.instance);
}

EvaluatedScene evaluate(Instance& instance, std::size_t frame, std::size_t width, std::size_t height) {
    auto result = instance.evaluateFrame(frame, width, height);
    require(static_cast<bool>(result), "evaluate frame " + std::to_string(frame) + ": " + result.error.message);
    require(result.scene.frameIndex == frame, "valid source frame was unexpectedly clamped");
    require(result.scene.viewportWidth == width && result.scene.viewportHeight == height,
            "evaluation did not preserve requested viewport");
    return std::move(result.scene);
}

void same(const EvaluatedScene& expected, const EvaluatedScene& actual, const std::string& context) {
    avemotion::test::ExactSceneComparison comparison;
    const auto difference = comparison.difference(expected, actual);
    require(difference.empty(), context + ": " + difference);
}

void unchanged(const SceneChangeSummary& changes, const std::string& context) {
    require(!changes.firstEvaluation && !changes.topologyChanged && !changes.geometryChanged
                && !changes.paintChanged && !changes.visualChanged,
            context + ": repeated exact frame must report no changes");
}

void counts(const DiagnosticsSnapshot& value, std::uint64_t instances, std::uint64_t samples,
            std::uint64_t cpuSessions, std::uint64_t cpuFrames, const std::string& context) {
    require(value.referenceMetadataSessionsCreated == 0U, context + ": unexpected metadata session");
    require(value.referenceSceneSessionsCreated == instances,
            context + ": each Instance must create exactly one retained scene session");
    require(value.referenceModelSessionsCreated == 0U && value.referenceModelSamples == 0U,
            context + ": exact sampling must not create or sample a model session");
    require(value.referenceCpuSessionsCreated == cpuSessions, context + ": incorrect CPU session count");
    require(value.referenceSceneSamples == samples, context + ": incorrect scene sample count");
    require(value.instancesCreated == instances && value.sceneEvaluations == samples
                && value.sceneEvaluationFailures == 0U && value.cpuFramesRendered == cpuFrames,
            context + ": incorrect successful-operation counts");
}

constexpr std::array<std::array<std::size_t, 2>, 2> viewports{{{128U, 128U}, {96U, 160U}}};
using Oracles = std::array<std::vector<EvaluatedScene>, viewports.size()>;

void prepareIfSupported(const std::shared_ptr<const Asset>& asset) {
    if (avemotion::reference::selectedUpstream().variant == "telegram") {
        const auto prepared = asset->prepareModel();
        require(static_cast<bool>(prepared), "prepare shared asset before threads: " + prepared.error);
    }
}

Oracles freshOracles(const fs::path& path, bool prepareModel = false) {
    // A separate Runtime and Asset isolate all oracle diagnostics and canonical
    // caches. Every oracle scene is sampled by a newly constructed Instance.
    Runtime oracleRuntime;
    const auto asset = load(oracleRuntime, path);
    // Telegram preparation publishes source-node IDs into its parsed model.
    // The oracle must have the same preparation precondition as the workers.
    if (prepareModel) prepareIfSupported(asset);
    Oracles result;
    for (std::size_t viewport = 0; viewport < viewports.size(); ++viewport) {
        auto& scenes = result[viewport];
        scenes.reserve(asset->metadata().totalFrames);
        for (std::size_t frame = 0; frame < asset->metadata().totalFrames; ++frame) {
            auto instance = create(oracleRuntime, asset);
            scenes.push_back(evaluate(*instance, frame, viewports[viewport][0], viewports[viewport][1]));
            require(scenes.back().evaluationSequence == 1U && scenes.back().changes.firstEvaluation,
                    "oracle must be the first sample of a fresh instance");
        }
    }
    return result;
}

const EvaluatedDrawItem& onlyDrawItem(const EvaluatedScene& scene, const std::string& context) {
    require(scene.drawItems.size() == 1U, context + ": expected one active draw item");
    return scene.drawItems[0];
}

const EvaluatedLayer& drawItemLayer(const EvaluatedScene& scene, const std::string& context) {
    const auto& item = onlyDrawItem(scene, context);
    require(item.layerIndex < scene.layers.size(), context + ": active draw item has no layer");
    const auto& layer = scene.layers[item.layerIndex];
    require(layer.visible && layer.drawItemCount > 0U,
            context + ": draw item's layer is not active");
    return layer;
}

void verifyActiveStateEndpoints(const fs::path& path, const Oracles& oracle) {
    const auto& first = oracle[0][0]; // 128x128, independently fresh frame 0
    const auto& second = oracle[0][1]; // 128x128, independently fresh frame 1
    const auto name = path.filename().string();
    if (name == "translation-near-default.json") {
        const auto& before = onlyDrawItem(first, name);
        const auto& after = onlyDrawItem(second, name);
        require(before.path.points.size() > 3U && after.path.points.size() > 3U,
                name + ": expected rectangle path points");
        require(before.path.points[2].x != 0.0F && before.path.points[3].x != 0.0F,
                name + ": authored frame-0 translation must be observable");
        require(after.path.points[2].x == 0.0F && after.path.points[3].x == 0.0F,
                name + ": fresh frame-1 translation must preserve constructor identity threshold");
        if (avemotion::reference::selectedUpstream().variant == "telegram") {
            require(after.localToViewport.dx == 0.0F,
                    name + ": fresh Telegram frame-1 local matrix must remain identity");
        }
    } else if (name == "width.json") {
        const auto& before = onlyDrawItem(first, name);
        const auto& after = onlyDrawItem(second, name);
        require(before.stroke.enabled && after.stroke.enabled && before.stroke.width != after.stroke.width,
                name + ": tiny authored width change must survive fresh evaluation");
    } else if (name == "opacity.json") {
        const auto& before = onlyDrawItem(first, name);
        const auto& after = onlyDrawItem(second, name);
        if (avemotion::reference::selectedUpstream().variant == "telegram") {
            require(before.paint.kind == PaintKind::Solid && after.paint.kind == PaintKind::Solid
                        && before.paint.solid.a == 127U && after.paint.solid.a == 128U,
                    name + ": Telegram paint alpha must advance from 127 to 128");
        } else {
            const auto& beforeLayer = drawItemLayer(first, name + " frame 0");
            const auto& afterLayer = drawItemLayer(second, name + " frame 1");
            require(beforeLayer.opacity == 127.0F / 255.0F
                        && afterLayer.opacity == 128.0F / 255.0F,
                    name + ": Samsung active layer opacity must advance from 127/255 to 128/255");
        }
    }
}

// These mutations expose real blind spots in recorder-only comparisons. They
// keep the recorder fingerprints unchanged and independently require a precise
// field path from the full comparator. Shared objects are cloned before edits.
void verifyComparator(const fs::path& dashPath) {
    Runtime runtime;
    auto instance = create(runtime, load(runtime, dashPath));
    auto baseline = evaluate(*instance, 0U, 128U, 128U);
    require(!baseline.drawItems.empty() && !baseline.layers.empty(), "dash scene is empty");
    baseline.drawItems[0].localPath.points.push_back({1.0F, 2.0F});
    baseline.masks.push_back({});
    baseline.drawItems[0].canonicalGeometry = std::make_shared<const CanonicalGeometry>();
    baseline.drawItems[0].canonicalPaint = std::make_shared<const CanonicalPaint>();
    baseline.drawItems.push_back(baseline.drawItems[0]);
    baseline.assetModel = std::make_shared<const avemotion::model::MotionAssetModel>();
    same(baseline, baseline, "comparator reflexivity");
    auto equivalent = baseline;
    const auto geometryCopy = std::make_shared<const CanonicalGeometry>(*baseline.drawItems[0].canonicalGeometry);
    const auto paintCopy = std::make_shared<const CanonicalPaint>(*baseline.drawItems[0].canonicalPaint);
    for (auto& item : equivalent.drawItems) {
        item.canonicalGeometry = geometryCopy;
        item.canonicalPaint = paintCopy;
    }
    equivalent.assetModel = std::make_shared<const avemotion::model::MotionAssetModel>(*baseline.assetModel);
    same(baseline, equivalent, "equal values and aliasing at different addresses");

    const auto mutation = [&](const std::string& field, const auto& change) {
        auto modified = baseline;
        change(modified);
        const auto expected = RecordingBackend{}.record(baseline).fingerprints;
        const auto actual = RecordingBackend{}.record(modified).fingerprints;
        require(expected.scene == actual.scene && expected.topology == actual.topology
                    && expected.geometry == actual.geometry && expected.paint == actual.paint,
                "self-test mutation must demonstrate a fingerprint blind spot: " + field);
        avemotion::test::ExactSceneComparison comparison;
        const auto difference = comparison.difference(baseline, modified);
        require(difference.find(field) != std::string::npos,
                "comparator missed " + field + " (reported: " + difference + ")");
        const auto reverseDifference = comparison.difference(modified, baseline);
        require(reverseDifference.find(field) != std::string::npos,
                "reverse comparator missed " + field + " (reported: " + reverseDifference + ")");
    };
    mutation("drawItems[0].path.hash", [](auto& scene) { ++scene.drawItems[0].path.hash; });
    mutation("drawItems[0].localPath.points[0].x", [](auto& scene) { scene.drawItems[0].localPath.points[0].x += 1.0F; });
    mutation("drawItems[0].localStroke.width", [](auto& scene) { scene.drawItems[0].localStroke.width += 1.0F; });
    mutation("drawItems[0].paint.gradient.focalRadius", [](auto& scene) { scene.drawItems[0].paint.gradient.focalRadius += 1.0F; });
    mutation("layers[0].modelLayer", [](auto& scene) { scene.layers[0].modelLayer.value = 123U; });
    mutation("masks[0].path.hash", [](auto& scene) { ++scene.masks[0].path.hash; });
    mutation("drawItems[0].sourcePathCount", [](auto& scene) { ++scene.drawItems[0].sourcePathCount; });
    mutation("drawItems[0].localToViewport.dx", [](auto& scene) { scene.drawItems[0].localToViewport.dx += 1.0F; });
    mutation("drawItems[0].sourceRepeaterOpacityRevision", [](auto& scene) { ++scene.drawItems[0].sourceRepeaterOpacityRevision; });
    mutation("drawItems[0].canonicalGeometry.path.hash", [](auto& scene) {
        auto value = std::make_shared<CanonicalGeometry>(*scene.drawItems[0].canonicalGeometry);
        ++value->path.hash;
        scene.drawItems[0].canonicalGeometry = value;
    });
    mutation("drawItems[0].canonicalPaint.stroke.width", [](auto& scene) {
        auto value = std::make_shared<CanonicalPaint>(*scene.drawItems[0].canonicalPaint);
        value->stroke.width += 1.0F;
        scene.drawItems[0].canonicalPaint = value;
    });
    mutation("drawItems[1].canonicalGeometry.alias", [](auto& scene) {
        scene.drawItems[1].canonicalGeometry = std::make_shared<const CanonicalGeometry>(*scene.drawItems[1].canonicalGeometry);
    });
    mutation("drawItems[1].canonicalPaint.alias", [](auto& scene) {
        scene.drawItems[1].canonicalPaint = std::make_shared<const CanonicalPaint>(*scene.drawItems[1].canonicalPaint);
    });
    mutation("assetModel.totalFrames", [](auto& scene) {
        auto value = std::make_shared<avemotion::model::MotionAssetModel>(*scene.assetModel);
        ++value->totalFrames;
        scene.assetModel = value;
    });
    mutation("modelNodeCount", [](auto& scene) { ++scene.modelNodeCount; });

    auto history = baseline;
    ++history.instanceId;
    history.assetHandle = {};
    history.instanceHandle = {};
    ++history.evaluationSequence;
    history.changes = {false, false, false, false, false};
    history.drawItems[0].upstreamChangeBits ^= SceneChangePaint;
    same(baseline, history, "only approved identity/history exclusions");
    std::cout << "PASS comparator: 15 fingerprint-blind mutations, canonical values/aliases, exclusions\n";
}

void verifyAccessOrder(const fs::path& path) {
    const auto oracle = freshOracles(path);
    verifyActiveStateEndpoints(path, oracle);
    Runtime runtime;
    const auto asset = load(runtime, path);
    const auto totalFrames = asset->metadata().totalFrames;
    require(oracle[0].size() == totalFrames, "oracle changed variant-reported totalFrames");
    runtime.resetDiagnostics(); // quiescent, before the measured instance exists
    auto instance = create(runtime, asset);
    counts(runtime.diagnostics(), 1U, 0U, 0U, 0U, "eager construction");
    std::vector<DiagnosticsSnapshot> samples;
    std::uint64_t sequence = 0U;
    std::size_t previousFrame = totalFrames;
    std::size_t previousViewport = viewports.size();
    const auto sample = [&](std::size_t frame, std::size_t viewport, const std::string& order) {
        auto scene = evaluate(*instance, frame, viewports[viewport][0], viewports[viewport][1]);
        const auto context = path.filename().string() + " " + order + " frame=" + std::to_string(frame)
            + " viewport=" + std::to_string(viewports[viewport][0]) + "x" + std::to_string(viewports[viewport][1]);
        same(oracle[viewport][frame], scene, context);
        if (path.filename() == "width.json" && viewport == 0U && frame < 2U) {
            require(onlyDrawItem(scene, context).stroke.width
                        == onlyDrawItem(oracle[viewport][frame], context + " fresh oracle").stroke.width,
                    context + ": stroke width differs from independent fresh oracle");
        }
        require(scene.evaluationSequence == ++sequence, context + ": sequence did not advance exactly once");
        require(scene.changes.firstEvaluation == (sequence == 1U), context + ": incorrect firstEvaluation");
        if (previousFrame == frame && previousViewport == viewport) unchanged(scene.changes, context);
        previousFrame = frame;
        previousViewport = viewport;
        samples.push_back(runtime.diagnostics());
    };
    for (std::size_t frame = 0; frame < totalFrames; ++frame) sample(frame, 0U, "ascending");
    for (std::size_t frame = totalFrames; frame-- > 0U;) sample(frame, 0U, "reverse");
    const std::array<std::size_t, 12> seeks{
        totalFrames / 2U, totalFrames / 2U, 0U, totalFrames - 1U, 1U, 0U,
        totalFrames - 1U, totalFrames / 3U, totalFrames / 3U, 1U, totalFrames / 2U, 0U};
    for (const auto frame : seeks) {
        sample(frame, 0U, "seek");
        sample(frame, 0U, "repeat");
        sample(frame, 1U, "viewport-change");
        sample(frame, 1U, "viewport-repeat");
        sample(frame, 0U, "viewport-return");
    }
    // Check every sampled counter snapshot after content parity. This makes a
    // naive reuse regression report the actual scene corruption before its
    // accompanying change in allocation counts.
    for (std::size_t index = 0; index < samples.size(); ++index) {
        counts(samples[index], 1U, index + 1U, 0U, 0U, path.filename().string() + " sample=" + std::to_string(index));
    }
    std::cout << "PASS " << path.filename().string() << ": sourceFrames=" << totalFrames
              << " exactSamples=" << samples.size() << " ascending/reverse/seeks/viewports\n";
}

void verifyCpuIsolation(const fs::path& path) {
    Runtime runtime;
    const auto asset = load(runtime, path);
    runtime.resetDiagnostics();
    auto instance = create(runtime, asset);
    const auto before = evaluate(*instance, 0U, 128U, 128U);
    counts(runtime.diagnostics(), 1U, 1U, 0U, 0U, "before lazy CPU session");
    const auto firstCpu = instance->renderCpuFrame(0U, 128U, 128U);
    require(static_cast<bool>(firstCpu), "first CPU render: " + firstCpu.error.message);
    counts(runtime.diagnostics(), 1U, 1U, 1U, 1U, "first lazy CPU session");
    const auto secondCpu = instance->renderCpuFrame(1U, 96U, 160U);
    require(static_cast<bool>(secondCpu), "second CPU render: " + secondCpu.error.message);
    counts(runtime.diagnostics(), 1U, 1U, 1U, 2U, "reused lazy CPU session");
    require(firstCpu.frame.nonTransparentPixels > 0U && secondCpu.frame.nonTransparentPixels > 0U,
            "CPU fixture must produce visible pixels");
    const auto after = evaluate(*instance, 0U, 128U, 128U);
    same(before, after, "scene -> CPU -> CPU -> same scene");
    require(before.evaluationSequence == 1U && after.evaluationSequence == 2U,
            "CPU rendering must not advance the scene sequence");
    unchanged(after.changes, "scene after CPU renders");
    counts(runtime.diagnostics(), 1U, 2U, 1U, 2U, "CPU isolation final");
    std::cout << "PASS CPU isolation: sceneSessions=1 sceneSamples=2 cpuSessions=1 cpuRenders=2\n";
}

void verifySeparateInstanceThreads(const fs::path& path) {
    const auto oracle = freshOracles(path, true);
    Runtime runtime;
    const auto asset = load(runtime, path);
    prepareIfSupported(asset);
    runtime.resetDiagnostics(); // no worker or instance exists at this point
    std::array<std::unique_ptr<Instance>, 2> instances{create(runtime, asset), create(runtime, asset)};
    counts(runtime.diagnostics(), 2U, 0U, 0U, 0U, "two eager thread instances");
    const auto last = asset->metadata().totalFrames - 1U;
    const auto middle = last / 2U;
    const std::array<std::array<std::size_t, 10>, 2> orders{{
        {0U, middle, middle, last, 0U, 1U, last, 0U, 0U, middle},
        {last, 0U, 0U, middle, 1U, last, middle, middle, 0U, last}}};
    std::array<std::vector<EvaluatedScene>, 2> scenes;
    std::array<std::exception_ptr, 2> errors;
    const auto worker = [&](std::size_t index) {
        try {
            for (const auto frame : orders[index]) {
                scenes[index].push_back(evaluate(*instances[index], frame, 128U, 128U));
            }
        } catch (...) { errors[index] = std::current_exception(); }
    };
    std::thread first{worker, 0U};
    std::thread second{worker, 1U};
    first.join();
    second.join();
    // Neither worker touches the other Instance or result slot. Read results,
    // exceptions and shared relaxed diagnostic counters only after both joins.
    for (std::size_t index = 0; index < instances.size(); ++index) {
        if (errors[index]) std::rethrow_exception(errors[index]);
        require(scenes[index].size() == orders[index].size(), "worker lost a scene");
        for (std::size_t sample = 0; sample < scenes[index].size(); ++sample) {
            const auto& scene = scenes[index][sample];
            same(oracle[0][orders[index][sample]], scene,
                 "separate-instance worker=" + std::to_string(index) + " sample=" + std::to_string(sample));
            require(scene.evaluationSequence == sample + 1U, "worker sequence did not advance");
            require(scene.changes.firstEvaluation == (sample == 0U), "worker firstEvaluation incorrect");
            if (sample > 0U && orders[index][sample] == orders[index][sample - 1U]) {
                unchanged(scene.changes, "worker repeated frame");
            }
        }
    }
    counts(runtime.diagnostics(), 2U, 20U, 0U, 0U, "separate-instance concurrency");
    std::cout << "PASS two separate-instance threads: " << path.filename().string() << " 20 samples\n";
}
} // namespace

int main() {
    try {
        require(Runtime::compiledWithReferenceEngine(), "reference session tests require rlottie");
        const fs::path fixtures{AVEMOTION_FIXTURE_DIR};
        const fs::path corpus{AVEMOTION_CORPUS_DIR};
        const auto dash = fixtures / "reference_sessions" / "dashed_stroke_session.json";
        verifyComparator(dash);
        const std::array<fs::path, 12> assets{
            dash, corpus / "dynamic_path_test.json", fixtures / "multi_trim_path_geometry.json",
            fixtures / "repeater_geometry.json", fixtures / "repeater_content_group.json",
            corpus / "mask.json", corpus / "matte_two_item_with_lowerlayer.json",
            corpus / "ModernPictogramsForLottie_LoudMute.json", corpus / "1667-firework.json",
            fixtures / "reference_sessions" / "translation-near-default.json",
            fixtures / "reference_sessions" / "width.json",
            fixtures / "reference_sessions" / "opacity.json"};
        for (const auto& asset : assets) verifyAccessOrder(asset);
        verifyCpuIsolation(dash);
        verifySeparateInstanceThreads(corpus / "ModernPictogramsForLottie_LoudMute.json");
        std::cout << "PASS reference scene sessions (" << avemotion::reference::selectedUpstream().variant
                  << ")\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
