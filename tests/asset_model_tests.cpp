#include "avemotion/reference/UpstreamInfo.hpp"
#include "avemotion/runtime/Runtime.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

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

std::vector<fs::path> corpus() {
    std::vector<fs::path> result;
    for (const auto& entry : fs::directory_iterator{AVEMOTION_CORPUS_DIR}) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            result.push_back(entry.path());
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

void verifyModelPreparationLifetime(std::string_view variant) {
    using namespace avemotion::runtime;
    const auto path = fs::path{AVEMOTION_CORPUS_DIR} / "StickAndBall.json";
    Runtime runtime;
    const auto loaded = runtime.loadLottieJson(readText(path), path.filename().string());
    require(static_cast<bool>(loaded), "lifetime fixture did not load");
    runtime.resetDiagnostics(); // No Instance or model preparation is active.

    const auto prepared = loaded.asset->prepareModel();
    const auto afterFirst = runtime.diagnostics();
    if (variant == "telegram") {
        require(static_cast<bool>(prepared), "Telegram lifetime preparation failed: " + prepared.error);
        require(prepared.model == loaded.asset->model(), "first preparation was not published");
        const auto frames = loaded.asset->metadata().totalFrames;
        require(frames > 1U, "lifetime fixture must span multiple frames");
        require(afterFirst.referenceModelSessionsCreated == frames
                    && afterFirst.referenceModelSamples == frames,
                "fresh model scan must construct and sample one session per source frame");
        require(afterFirst.referenceSceneSessionsCreated == 0U
                    && afterFirst.referenceSceneSamples == 0U,
                "model scan leaked work into the scene role");
        require(afterFirst.assetModelBuildAttempts == 1U
                    && afterFirst.assetModelBuildsSucceeded == 1U
                    && afterFirst.assetModelBuildsFailed == 0U,
                "first model preparation did not record one successful build");

        const auto preparedAgain = loaded.asset->prepareModel();
        require(preparedAgain && preparedAgain.model == prepared.model
                    && loaded.asset->model() == prepared.model,
                "second preparation did not reuse the published immutable model");
        const auto afterSecond = runtime.diagnostics();
        require(afterSecond.referenceModelSessionsCreated == frames
                    && afterSecond.referenceModelSamples == frames
                    && afterSecond.assetModelBuildAttempts == 1U
                    && afterSecond.assetModelBuildsSucceeded == 1U
                    && afterSecond.assetModelBuildsFailed == 0U,
                "second preparation repeated model work");

        auto created = runtime.createInstance(loaded.asset);
        require(static_cast<bool>(created), "lifetime fixture instance creation failed");
        const auto evaluated = created.instance->evaluateFrame(0U, 128U, 128U);
        require(static_cast<bool>(evaluated), "lifetime fixture exact scene evaluation failed");
        const auto afterScene = runtime.diagnostics();
        require(afterScene.referenceModelSessionsCreated == frames
                    && afterScene.referenceModelSamples == frames
                    && afterScene.assetModelBuildAttempts == 1U
                    && afterScene.assetModelBuildsSucceeded == 1U,
                "exact Instance evaluation changed model preparation counters");
        require(afterScene.referenceSceneSessionsCreated == 2U
                    && afterScene.referenceSceneSamples == 1U,
                "exact Instance evaluation did not use its own scene role");
    } else {
        require(!prepared && !prepared.model
                    && prepared.error == "the selected comparison engine does not expose stable Telegram source IDs",
                "comparison preparation changed its unsupported result");
        require(!loaded.asset->model(), "comparison preparation published a model");
        require(afterFirst.assetModelBuildAttempts == 1U
                    && afterFirst.assetModelBuildsSucceeded == 0U
                    && afterFirst.assetModelBuildsFailed == 1U,
                "comparison preparation did not record its failure");
        require(afterFirst.referenceModelSessionsCreated == 0U
                    && afterFirst.referenceModelSamples == 0U
                    && afterFirst.referenceSceneSessionsCreated == 0U
                    && afterFirst.referenceSceneSamples == 0U,
                "unsupported preparation constructed a reference session");
    }
}

void verifyPreparationLimitRetry(std::string_view variant) {
    using namespace avemotion::runtime;
    const auto path = fs::path{AVEMOTION_CORPUS_DIR}.parent_path()
        / "fixtures" / "reference_sessions" / "dashed_stroke_session.json";
    auto json = readText(path);
    const auto original = std::string{"\"op\":61"};
    const auto rootEnd = json.find("\"layers\"");
    const auto rootOutPoint = json.find(original);
    require(rootOutPoint != std::string::npos && rootOutPoint < rootEnd,
            "long-timeline fixture has no root out-point to extend");
    json.replace(rootOutPoint, original.size(), "\"op\":10001");

    Runtime runtime;
    const auto loaded = runtime.loadLottieJson(json, "model-preparation-over-limit.json");
    require(static_cast<bool>(loaded), "valid long-timeline fixture did not load");
    require(loaded.asset->metadata().totalFrames > 10'000U,
            "long-timeline fixture did not cross the preparation limit");
    runtime.resetDiagnostics(); // Only the two retries below contribute.
    const auto expectedError = variant == "telegram"
        ? "asset timeline exceeds reference-model preparation limit"
        : "the selected comparison engine does not expose stable Telegram source IDs";
    for (std::uint64_t attempt = 1U; attempt <= 2U; ++attempt) {
        const auto prepared = loaded.asset->prepareModel();
        require(!prepared && !prepared.model && prepared.error == expectedError,
                "over-limit preparation did not preserve its variant-specific failure");
        require(!loaded.asset->model(), "failed preparation published a model");
        const auto counts = runtime.diagnostics();
        require(counts.assetModelBuildAttempts == attempt
                    && counts.assetModelBuildsFailed == attempt
                    && counts.assetModelBuildsSucceeded == 0U,
                "failed preparation was not retried and counted");
        require(counts.referenceModelSessionsCreated == 0U
                    && counts.referenceModelSamples == 0U
                    && counts.referenceSceneSessionsCreated == 0U
                    && counts.referenceSceneSamples == 0U,
                "ineligible preparation constructed or sampled a reference session");
    }
}

} // namespace

int main() {
    using namespace avemotion;
    const auto upstream = reference::selectedUpstream();
    const auto assets = corpus();
    require(!assets.empty(), "characterization corpus is empty");

    verifyModelPreparationLifetime(upstream.variant);
    verifyPreparationLimitRetry(upstream.variant);

    runtime::Runtime runtime;
    std::size_t totalLayers = 0;
    std::size_t totalNodes = 0;
    std::size_t totalGeometries = 0;
    std::size_t totalPaints = 0;
    std::size_t totalStaticGeometry = 0;
    std::size_t totalStaticPaint = 0;

    for (const auto& path : assets) {
        const auto json = readText(path);
        auto loaded = runtime.loadLottieJson(json, path.filename().string());
        require(static_cast<bool>(loaded), "asset load failed for " + path.string());

        const auto prepared = loaded.asset->prepareModel();
        if (upstream.variant != "telegram") {
            require(!prepared,
                    "comparison upstream unexpectedly exposed Telegram typed asset model");
            continue;
        }
        require(static_cast<bool>(prepared),
                "asset-model preparation failed for " + path.string()
                    + ": " + prepared.error);
        require(prepared.model == loaded.asset->model(),
                "Asset::model did not return the prepared model");
        const auto preparedAgain = loaded.asset->prepareModel();
        require(preparedAgain.model == prepared.model,
                "repeated preparation rebuilt the immutable model");

        const auto& model = *prepared.model;
        require(model.schemaVersion == model::MotionAssetModel::kSchemaVersion,
                "wrong asset-model schema version");
        require(model.revision != 0U, "asset-model revision must be non-zero");
        require(model.sourceAssetHash == loaded.asset->metadata().sourceHash,
                "asset model lost source identity");
        require(model.logicalWidth == loaded.asset->metadata().width
                    && model.logicalHeight == loaded.asset->metadata().height,
                "asset model lost logical size");
        require(model.totalFrames == loaded.asset->metadata().totalFrames,
                "asset model lost timeline frame count");
        require(model.fingerprint != 0U && model.topologyFingerprint != 0U
                    && model.resourceFingerprint != 0U,
                "asset-model fingerprints must be non-zero");
        require(!model.layers.empty() && !model.nodes.empty()
                    && !model.geometries.empty() && !model.paints.empty(),
                "asset model contains empty core tables");
        require(model.statistics.declaredLayerCount == model.layers.size()
                    && model.statistics.observedLayerCount == model.layers.size(),
                "layer-table statistics differ");
        require(model.statistics.declaredNodeCount == model.nodes.size()
                    && model.statistics.observedNodeCount <= model.nodes.size()
                    && model.statistics.observedNodeCount > 0U,
                "node-table statistics differ");
        require(model.statistics.declaredGeometryCount == model.geometries.size()
                    && model.statistics.observedGeometryCount <= model.geometries.size()
                    && model.statistics.observedGeometryCount > 0U,
                "geometry-table statistics differ");
        require(model.statistics.declaredPaintCount == model.paints.size()
                    && model.statistics.observedPaintCount <= model.paints.size()
                    && model.statistics.observedPaintCount > 0U,
                "paint-table statistics differ");
        require(model.drawOrder.size() == model.statistics.observedNodeCount,
                "draw order does not cover all observed nodes");
        std::unordered_set<std::uint32_t> orderedNodes;
        for (std::size_t order = 0; order < model.drawOrder.size(); ++order) {
            const auto nodeId = model.drawOrder[order];
            const auto* node = model.node(nodeId);
            require(node != nullptr, "draw order references an absent node");
            require(orderedNodes.insert(nodeId.value).second,
                    "draw order contains a duplicate node");
            require(node->drawOrder == order,
                    "node draw-order slot differs from immutable draw order");
        }
        require(model.clips.size() == 1U,
                "asset model must expose one default clip");
        const auto* clip = model.clip(model::ClipId{0U});
        require(clip != nullptr && clip->firstFrame == 0U
                    && clip->endFrame == static_cast<double>(model.totalFrames),
                "default clip is invalid");

        for (std::size_t index = 0; index < model.layers.size(); ++index) {
            const auto id = model::makeId<model::LayerId>(index);
            const auto* layer = model.layer(id);
            require(layer != nullptr && layer->id == id,
                    "layer table is not dense and index-addressable");
            require(layer->children.end() <= model.childLayerIds.size(),
                    "layer child range is invalid");
            require(layer->nodes.end() <= model.layerNodeIds.size(),
                    "layer node range is invalid");
            if (layer->parent.valid()) {
                require(model.layer(layer->parent) != nullptr,
                        "layer parent reference is invalid");
            }
            for (std::uint32_t offset = 0; offset < layer->children.count; ++offset) {
                const auto childId = model.childLayerIds[layer->children.first + offset];
                const auto* child = model.layer(childId);
                require(child != nullptr && child->parent == layer->id,
                        "layer child range is inconsistent with parent identity");
            }
            for (std::uint32_t offset = 0; offset < layer->nodes.count; ++offset) {
                const auto nodeId = model.layerNodeIds[layer->nodes.first + offset];
                const auto* node = model.node(nodeId);
                require(node != nullptr && node->layer == layer->id,
                        "layer node range is inconsistent with node ownership");
            }
        }
        for (std::size_t index = 0; index < model.nodes.size(); ++index) {
            const auto id = model::makeId<model::NodeId>(index);
            const auto* node = model.node(id);
            if (node == nullptr) {
                continue;
            }
            require(node->id == id,
                    "node table is not stable and index-addressable");
            require(node->drawItem.valid(), "node lacks stable draw-item ID");
            require(model.layer(node->layer) != nullptr,
                    "node layer reference is invalid");
            require(model.geometry(node->geometry) != nullptr,
                    "node geometry reference is invalid");
            require(model.paint(node->paint) != nullptr,
                    "node paint reference is invalid");
        }
        for (std::size_t index = 0; index < model.geometries.size(); ++index) {
            const auto id = model::makeId<model::GeometryId>(index);
            const auto* geometry = model.geometry(id);
            if (geometry == nullptr) {
                continue;
            }
            require(geometry->id == id,
                    "geometry table is not stable and index-addressable");
            if (geometry->resourceClass == model::ResourceClass::AssetStatic) {
                require(geometry->staticValue.has_value(),
                        "asset-static geometry lacks immutable value");
            }
        }
        for (std::size_t index = 0; index < model.paints.size(); ++index) {
            const auto id = model::makeId<model::PaintId>(index);
            const auto* paint = model.paint(id);
            if (paint == nullptr) {
                continue;
            }
            require(paint->id == id,
                    "paint table is not stable and index-addressable");
            if (paint->resourceClass == model::ResourceClass::AssetStatic) {
                require(paint->staticValue.has_value(),
                        "asset-static paint lacks immutable value");
            }
        }

        auto first = runtime.createInstance(loaded.asset);
        auto second = runtime.createInstance(loaded.asset);
        require(first && second, "instance creation failed");
        auto sceneA = first.instance->evaluateModelPosition(0.5, 128U, 128U);
        auto sceneB = second.instance->evaluateModelPosition(0.5, 128U, 128U);
        require(sceneA && sceneB, "model-aware scene evaluation failed");
        require(sceneA.scene.assetModelApplied && sceneB.scene.assetModelApplied,
                "model-aware scene did not apply immutable asset tables");
        require(sceneA.scene.assetModel == prepared.model
                    && sceneB.scene.assetModel == prepared.model,
                "instances did not share one immutable asset model");
        require(sceneA.scene.drawItems.size() <= model.statistics.observedNodeCount,
                "evaluated draw-item count exceeds immutable model");
        for (const auto& item : sceneA.scene.drawItems) {
            require(model.node(item.modelNode) != nullptr,
                    "evaluated node ID is invalid");
            require(model.geometry(item.modelGeometry) != nullptr,
                    "evaluated geometry ID is invalid");
            const auto* paint = model.paint(item.modelPaint);
            const auto* geometry = model.geometry(item.modelGeometry);
            require(paint != nullptr,
                    "evaluated paint ID is invalid");
            if (item.canonicalGeometry) {
                require(geometry != nullptr
                            && geometry->resourceClass == model::ResourceClass::AssetStatic,
                        "canonical geometry did not originate from static model data");
                require(item.localGeometryAvailable && item.localPaintAvailable,
                        "canonical local geometry was applied without a complete local seam");
            }
            if (item.canonicalPaint) {
                require(paint->resourceClass == model::ResourceClass::AssetStatic,
                        "canonical paint did not originate from static model data");
                require(item.localPaintAvailable,
                        "canonical local paint was applied without local paint data");
            }
        }

        // A separately loaded copy must preserve content identity even though
        // registry handles are deliberately different.
        auto loadedCopy = runtime.loadLottieJson(json, path.filename().string());
        require(static_cast<bool>(loadedCopy), "copy asset load failed");
        const auto copyModel = loadedCopy.asset->prepareModel();
        require(copyModel && copyModel.model->fingerprint == model.fingerprint,
                "identical bytes produced a different canonical model");

        totalLayers += model.layers.size();
        totalNodes += model.nodes.size();
        totalGeometries += model.geometries.size();
        totalPaints += model.paints.size();
        totalStaticGeometry += model.statistics.assetStaticGeometryCount;
        totalStaticPaint += model.statistics.assetStaticPaintCount;
    }

    const auto diagnostics = runtime.diagnostics();
    if (upstream.variant == "telegram") {
        require(totalLayers > 0U && totalNodes > 0U,
                "typed model extracted no layers or nodes");
        require(totalStaticGeometry > 0U,
                "typed model proved no static geometry");
        require(totalStaticPaint > 0U,
                "typed model proved no static paint");
        require(diagnostics.assetModelBuildsSucceeded == assets.size() * 2U,
                "model build diagnostics do not match unique loaded assets");
        require(diagnostics.assetModelBuildsFailed == 0U,
                "Telegram model preparation unexpectedly failed");
        require(diagnostics.modelEvaluations == assets.size() * 2U,
                "model evaluation diagnostics are wrong");
    } else {
        require(diagnostics.assetModelBuildsSucceeded == 0U,
                "comparison upstream unexpectedly built typed models");
        require(diagnostics.assetModelBuildsFailed == assets.size(),
                "comparison upstream model failures were not recorded");
    }

    std::cout << "AveMotion asset-model tests passed for " << assets.size()
              << " assets using " << upstream.variant
              << " layers=" << totalLayers
              << " nodes=" << totalNodes
              << " geometries=" << totalGeometries
              << " paints=" << totalPaints
              << " static-geometry=" << totalStaticGeometry
              << " static-paint=" << totalStaticPaint << '\n';
    return EXIT_SUCCESS;
}
