#include "TelegramParsedModelBuilder.hpp"
#include "RlottieSceneBridge.hpp"
#include "support/ExactSceneComparison.hpp"
#include "avemotion/core/Hash.hpp"
#include "avemotion/runtime/Runtime.hpp"
#include "avemotionanimationaccess.h"
#include "lottieloader.h"

#include <rlottie.h>

#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <latch>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
using avemotion::runtime::Asset;
using avemotion::runtime::EvaluatedScene;
using avemotion::model::MotionAssetModel;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::string corpusJson() {
    std::ifstream stream{
        std::filesystem::path{AVEMOTION_CORPUS_DIR} / "StickAndBall.json",
        std::ios::binary};
    require(bool(stream), "open StickAndBall fixture");
    return {std::istreambuf_iterator<char>{stream}, {}};
}

avemotion::model::detail::AssetModelDescriptor descriptor(const Asset& asset) {
    const auto& metadata = asset.metadata();
    return {asset.handle(), metadata.sourceHash, metadata.width,
        metadata.height, metadata.frameRate, metadata.totalFrames,
        metadata.debugName.c_str()};
}

// The oracle owns a distinct ordinary parsed source. Explicit extraction stamps
// that source before a newly constructed ordinary Animation samples it.
EvaluatedScene preparedOrdinaryScene(
    const std::string& json, const Asset& asset,
    const std::shared_ptr<const MotionAssetModel>& frozenModel,
    const std::string& key) {
    const auto parsed = avemotion::model::detail::buildTelegramParsedModel(
        json, key, descriptor(asset));
    require(bool(parsed), "oracle extraction: " + parsed.error);
    auto animation = rlottie::Animation::loadFromData(json, key, {}, true);
    require(bool(animation), "fresh ordinary oracle Animation");
    auto built = avemotion::runtime::detail::buildSceneFromRlottieTree(
        animation->renderTree(0, 128, 128), asset.metadata().sourceHash,
        0, 1, 0, 128, 128);
    require(bool(built), "ordinary oracle scene: " + built.error.message);
    built.scene.assetHandle = asset.handle();
    const auto applied = avemotion::model::detail::applyAssetModel(
        frozenModel, built.scene);
    require(bool(applied), "apply oracle model: " + applied.error);
    return std::move(built.scene);
}

void assertLiveBindings(const EvaluatedScene& scene, const std::string& phase) {
    require(scene.assetModelApplied && bool(scene.assetModel),
        "scene has frozen model");
    bool livePath = false;
    bool livePaint = false;
    for (const auto& item : scene.drawItems) {
        if (item.sourcePathNode.valid()) {
            const auto* node = scene.assetModel->sourceNode(item.sourcePathNode);
            require(node && node->present && !item.path.points.empty(),
                "live path resolves into source model with geometry");
            livePath = true;
        }
        if (item.sourcePaintNode.valid()) {
            const auto* node = scene.assetModel->sourceNode(item.sourcePaintNode);
            require(node && node->present, "live paint resolves into source model");
            livePaint = true;
        }
        if (item.canonicalGeometry) {
            const auto* record = scene.assetModel->geometry(item.modelGeometry);
            require(record && record->staticValue
                    && item.canonicalGeometry.get() == &*record->staticValue,
                "canonical geometry points into own frozen model");
            require(!item.canonicalGeometry.owner_before(scene.assetModel)
                    && !scene.assetModel.owner_before(item.canonicalGeometry),
                "canonical geometry shares frozen model owner");
        }
        if (item.canonicalPaint) {
            const auto* record = scene.assetModel->paint(item.modelPaint);
            require(record && record->staticValue
                    && item.canonicalPaint.get() == &*record->staticValue,
                "canonical paint points into own frozen model");
            require(!item.canonicalPaint.owner_before(scene.assetModel)
                    && !scene.assetModel.owner_before(item.canonicalPaint),
                "canonical paint shares frozen model owner");
        }
    }
    require(livePath && livePaint,
        phase + ": non-vacuous authored path and paint bindings");
}

void samePreparedScene(
    const std::string& json, const Asset& asset, const EvaluatedScene& actual,
    const std::string& key) {
    require(actual.assetHandle == asset.handle(), "scene asset handle");
    require(actual.assetModel == asset.model(), "scene uses Asset frozen model");
    auto expected = preparedOrdinaryScene(json, asset, asset.model(), key);
    assertLiveBindings(expected, key + " oracle");
    assertLiveBindings(actual, key + " candidate");
    const auto diff = avemotion::test::ExactSceneComparison{}.difference(
        expected, actual);
    require(diff.empty(), "full prepared scene mismatch: " + diff);
}

void evictionAfterPreparation(const std::string& json) {
    avemotion::runtime::Runtime runtime;
    auto loaded = runtime.loadLottieJson(json, "source-owner");
    require(bool(loaded), "load original Asset");
    require(runtime.diagnostics().referenceMetadataSessionsCreated == 1,
        "metadata epoch stays one");
    auto existing = runtime.createInstance(loaded.asset);
    require(bool(existing), "create existing Instance");
    auto cpuBefore = existing.instance->renderCpuFrame(0, 128, 128);
    require(bool(cpuBefore), "CPU before model preparation");
    auto prepared = loaded.asset->prepareModel();
    require(bool(prepared), "prepare original Asset: " + prepared.error);
    const auto key = "avemotion-asset-"
        + avemotion::core::formatHash(loaded.asset->metadata().sourceHash);
    auto observer = rlottie::Animation::loadFromData(json, key, {}, true);
    require(bool(observer), "observe exact cached source");
    std::weak_ptr<LOTModel> source = rlottie::AveMotionAnimationAccess::model(*observer);
    observer.reset();
    require(prepared.model->assetHandle == loaded.asset->handle(),
        "frozen model owns original Asset descriptor");
    require(runtime.diagnostics().referenceModelSessionsCreated == 1,
        "model preparation retains one temporary session");
    auto before = existing.instance->evaluateModelFrame(0, 128, 128);
    require(bool(before), "evaluate before eviction");
    assertLiveBindings(before.scene, "before eviction");
    auto evictor = runtime.loadLottieJson(json + "\n ", "source-evictor");
    require(bool(evictor), "load whitespace-distinct evictor");
    require(!source.expired(), "Asset source lease survives cache eviction");
    auto after = existing.instance->evaluateModelFrame(0, 128, 128);
    require(bool(after), "existing Instance after eviction");
    samePreparedScene(json, *loaded.asset, after.scene, "task2-oracle-after-existing");
    auto later = runtime.createInstance(loaded.asset);
    require(bool(later), "create later Instance after eviction");
    auto laterScene = later.instance->evaluateModelFrame(0, 128, 128);
    require(bool(laterScene), "new Instance after eviction");
    samePreparedScene(json, *loaded.asset, laterScene.scene, "task2-oracle-after-new");
    LottieLoader::configureModelCacheSize(0);
    require(!source.expired(), "Asset source lease survives cache disable");
    auto withoutCache = existing.instance->evaluateModelFrame(0, 128, 128);
    require(bool(withoutCache), "existing Instance with cache disabled");
    assertLiveBindings(withoutCache.scene, "cache disabled");
    require(avemotion::test::ExactSceneComparison{}.difference(
                after.scene, withoutCache.scene).empty(),
        "disabled-cache scene preserves every prepared field and alias");
    auto cpuAfter = existing.instance->renderCpuFrame(0, 128, 128);
    require(bool(cpuAfter), "CPU after eviction");
    require(cpuBefore.frame.argbPremultiplied == cpuAfter.frame.argbPremultiplied,
        "CPU pixels survive preparation and eviction");
    const auto epoch = runtime.diagnostics();
    require(epoch.referenceMetadataSessionsCreated == 2
            && epoch.referenceSceneSessionsCreated == 2
            && epoch.referenceModelSessionsCreated == 1
            && epoch.referenceCpuSessionsCreated == 1,
        "metadata/scene/model/lazy CPU session epoch");
    before = {};
    after = {};
    laterScene = {};
    withoutCache = {};
    prepared = {};
    existing.instance.reset();
    later.instance.reset();
    loaded.asset.reset();
    require(source.expired(), "source lease releases after all Asset owners die");
    LottieLoader::configureModelCacheSize(1);
}

void evictionBeforePreparation(const std::string& json) {
    avemotion::runtime::Runtime runtime;
    auto loaded = runtime.loadLottieJson(json, "prepare-after-eviction");
    require(bool(loaded), "load original before preparation");
    auto evictor = runtime.loadLottieJson(json + "\n  ", "early-evictor");
    require(bool(evictor), "evict before preparation");
    auto prepared = loaded.asset->prepareModel();
    require(bool(prepared), "prepare after cache eviction: " + prepared.error);
    auto instance = runtime.createInstance(loaded.asset);
    require(bool(instance), "create after early eviction");
    auto scene = instance.instance->evaluateModelFrame(0, 128, 128);
    require(bool(scene), "sample after early eviction");
    samePreparedScene(json, *loaded.asset, scene.scene, "task2-oracle-before-prepare");
}

void concurrentAssets(const std::string& json) {
    avemotion::runtime::Runtime runtime;
    auto first = runtime.loadLottieJson(json, "same-bytes-first");
    auto second = runtime.loadLottieJson(json, "same-bytes-second");
    require(bool(first) && bool(second), "load two same-byte Assets");
    require(first.asset->handle() != second.asset->handle(),
        "same-byte Assets receive distinct handles");
    std::latch start{4};
    auto prepareA = std::async(std::launch::async, [&] {
        start.arrive_and_wait();
        return first.asset->prepareModel();
    });
    auto prepareB = std::async(std::launch::async, [&] {
        start.arrive_and_wait();
        return second.asset->prepareModel();
    });
    auto sampleA = std::async(std::launch::async, [&] {
        start.arrive_and_wait();
        auto instance = runtime.createInstance(first.asset);
        require(bool(instance), "first concurrent Instance");
        return instance.instance->evaluateModelFrame(0, 128, 128);
    });
    auto sampleB = std::async(std::launch::async, [&] {
        start.arrive_and_wait();
        auto instance = runtime.createInstance(second.asset);
        require(bool(instance), "second concurrent Instance");
        return instance.instance->evaluateModelFrame(0, 128, 128);
    });
    const auto modelA = prepareA.get();
    const auto modelB = prepareB.get();
    require(bool(modelA) && bool(modelB), "concurrent Asset preparation");
    require(modelA.model != modelB.model
            && modelA.model->assetHandle == first.asset->handle()
            && modelB.model->assetHandle == second.asset->handle(),
        "independent final descriptors for same-byte Assets");
    auto sceneA = sampleA.get();
    auto sceneB = sampleB.get();
    require(bool(sceneA) && bool(sceneB), "concurrent scenes");
    samePreparedScene(json, *first.asset, sceneA.scene, "task2-oracle-concurrent-a");
    samePreparedScene(json, *second.asset, sceneB.scene, "task2-oracle-concurrent-b");
}
} // namespace

int main() {
    try {
        LottieLoader::configureModelCacheSize(1);
        const auto json = corpusJson();
        evictionAfterPreparation(json);
        evictionBeforePreparation(json);
        concurrentAssets(json);
        std::cout << "source ownership tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
