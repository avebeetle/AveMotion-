#include "avemotion/reference/UpstreamInfo.hpp"
#include "avemotion/render/RenderPlanner.hpp"
#include "avemotion/runtime/Runtime.hpp"

#include <algorithm>
#include <cmath>
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

avemotion::runtime::Vec2 transformPoint(
    const avemotion::runtime::AffineTransform& matrix,
    avemotion::runtime::Vec2 point) noexcept {
    return {
        point.x * matrix.m11 + point.y * matrix.m21 + matrix.dx,
        point.x * matrix.m12 + point.y * matrix.m22 + matrix.dy,
    };
}

bool near(float left, float right, float tolerance = 0.0025F) noexcept {
    return std::abs(left - right) <= tolerance;
}

void verifyLocalGeometryParity(
    const avemotion::runtime::EvaluatedDrawItem& item,
    const std::string& assetName) {
    if (!item.localGeometryAvailable || !item.stroke.dashArray.empty()) {
        return;
    }
    const auto& local = item.canonicalGeometry
        ? item.canonicalGeometry->path
        : item.localPath;
    require(local.verbs == item.path.verbs,
            assetName + ": local/final path verb topology differs");
    require(local.points.size() == item.path.points.size(),
            assetName + ": local/final path point count differs");
    for (std::size_t index = 0; index < local.points.size(); ++index) {
        const auto transformed = transformPoint(
            item.localToViewport, local.points[index]);
        require(near(transformed.x, item.path.points[index].x)
                    && near(transformed.y, item.path.points[index].y),
                assetName + ": separated local transform does not reconstruct final path");
    }
}

} // namespace

int main() {
    using namespace avemotion;
    const auto upstream = reference::selectedUpstream();

    std::vector<fs::path> assets;
    for (const auto& entry : fs::directory_iterator{AVEMOTION_CORPUS_DIR}) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            assets.push_back(entry.path());
        }
    }
    std::sort(assets.begin(), assets.end());
    require(!assets.empty(), "characterization corpus is empty");

    runtime::Runtime runtime;
    std::size_t localGeometryItems = 0;
    std::size_t assetStaticGeometryItems = 0;
    std::size_t assetStaticPaintItems = 0;
    std::size_t sharedGeometryPointers = 0;
    std::size_t sharedPaintPointers = 0;

    for (const auto& path : assets) {
        const auto json = readText(path);
        auto loaded = runtime.loadLottieJson(json, path.filename().string());
        require(static_cast<bool>(loaded), "asset load failed for " + path.string());
        require(loaded.asset->handle().valid(), "asset handle is invalid");

        auto first = runtime.createInstance(loaded.asset);
        auto second = runtime.createInstance(loaded.asset);
        require(first && second, "instance creation failed for " + path.string());
        require(first.instance->handle().valid() && second.instance->handle().valid(),
                "instance handle is invalid");
        require(first.instance->handle() != second.instance->handle(),
                "two live instances received the same handle");

        const auto frame = loaded.asset->metadata().totalFrames / 2U;
        const std::size_t sampleFrames[] = {
            0U,
            loaded.asset->metadata().totalFrames / 4U,
            frame,
            (loaded.asset->metadata().totalFrames * 3U) / 4U,
            loaded.asset->metadata().totalFrames - 1U,
        };
        for (const auto sampleFrame : sampleFrames) {
            auto sampled = first.instance->evaluateFrame(sampleFrame, 128U, 128U);
            require(static_cast<bool>(sampled),
                    "sampled scene evaluation failed for " + path.string());
            for (const auto& item : sampled.scene.drawItems) {
                verifyLocalGeometryParity(item, path.filename().string());
            }
        }
        auto sceneA = first.instance->evaluateFrame(frame, 128U, 128U);
        auto sceneB = second.instance->evaluateFrame(frame, 128U, 128U);
        require(sceneA && sceneB, "scene evaluation failed for " + path.string());
        require(sceneA.scene.assetHandle == loaded.asset->handle(),
                "scene lost its asset handle");
        require(sceneA.scene.instanceHandle == first.instance->handle(),
                "scene lost its instance handle");

        std::unordered_map<std::uint64_t, const runtime::CanonicalGeometry*> geometryA;
        std::unordered_map<std::uint64_t, const runtime::CanonicalPaint*> paintA;
        for (const auto& item : sceneA.scene.drawItems) {
            if (item.localGeometryAvailable) {
                ++localGeometryItems;
                verifyLocalGeometryParity(item, path.filename().string());
            }
            if (item.geometryOrigin == runtime::EvaluatedValueOrigin::AssetStatic) {
                require(item.canonicalGeometry != nullptr,
                        "asset-static geometry has no canonical object");
                ++assetStaticGeometryItems;
                geometryA.emplace(item.sourceGeometryId, item.canonicalGeometry.get());
            }
            if (item.paintOrigin == runtime::EvaluatedValueOrigin::AssetStatic) {
                require(item.canonicalPaint != nullptr,
                        "asset-static paint has no canonical object");
                ++assetStaticPaintItems;
                paintA.emplace(item.sourcePaintId, item.canonicalPaint.get());
            }
        }
        for (const auto& item : sceneB.scene.drawItems) {
            if (item.canonicalGeometry) {
                const auto found = geometryA.find(item.sourceGeometryId);
                if (found != geometryA.end()) {
                    require(found->second == item.canonicalGeometry.get(),
                            "instances did not share canonical geometry storage");
                    ++sharedGeometryPointers;
                }
            }
            if (item.canonicalPaint) {
                const auto found = paintA.find(item.sourcePaintId);
                if (found != paintA.end()) {
                    require(found->second == item.canonicalPaint.get(),
                            "instances did not share canonical paint storage");
                    ++sharedPaintPointers;
                }
            }
        }

        render::MotionRenderPlanner planner;
        auto planA = planner.build(std::move(sceneA.scene));
        auto planB = planner.build(std::move(sceneB.scene));
        require(planA && planB, "render planning failed for " + path.string());
        for (const auto& itemA : planA.plan.drawItems) {
            if (itemA.geometry.scope != render::ResourceIdentityScope::Asset) {
                continue;
            }
            const auto match = std::find_if(
                planB.plan.drawItems.begin(), planB.plan.drawItems.end(),
                [&](const render::MotionDrawItem& itemB) {
                    return itemB.sourceItemKey == itemA.sourceItemKey;
                });
            if (match != planB.plan.drawItems.end()) {
                require(match->geometry == itemA.geometry,
                        "asset-static geometry cache key differs across instances");
                require(match->geometry.instanceIdentity == 0U,
                        "asset-static geometry key leaked instance identity");
            }
        }

        auto resizedScene = first.instance->evaluateFrame(frame, 256U, 192U);
        require(static_cast<bool>(resizedScene),
                "resized scene evaluation failed for " + path.string());
        for (const auto& item : resizedScene.scene.drawItems) {
            verifyLocalGeometryParity(item, path.filename().string());
        }
        auto resizedPlan = planner.build(std::move(resizedScene.scene));
        require(static_cast<bool>(resizedPlan),
                "resized render planning failed for " + path.string());
        for (const auto& update : resizedPlan.plan.geometryUpdates) {
            require(update.key.scope != render::ResourceIdentityScope::Asset,
                    "resize rebuilt an asset-static geometry resource");
        }
    }

    if (upstream.variant == "telegram") {
        require(localGeometryItems > 0U,
                "Telegram extension exported no local geometry");
        require(assetStaticGeometryItems > 0U,
                "Telegram extension proved no asset-static geometry");
        require(assetStaticPaintItems > 0U,
                "Telegram extension proved no asset-static paint");
        require(sharedGeometryPointers > 0U,
                "no canonical geometry was shared across instances");
        require(sharedPaintPointers > 0U,
                "no canonical paint was shared across instances");
    } else {
        require(localGeometryItems == 0U,
                "comparison upstream unexpectedly claimed Telegram extension metadata");
    }

    // Generation handles must invalidate a destroyed slot before reuse.
    const auto json = readText(assets.front());
    auto loaded = runtime.loadLottieJson(json, "handle-reuse");
    require(static_cast<bool>(loaded), "handle-reuse asset load failed");
    auto firstInstance = runtime.createInstance(loaded.asset);
    require(static_cast<bool>(firstInstance), "handle-reuse instance creation failed");
    const auto oldHandle = firstInstance.instance->handle();
    firstInstance.instance.reset();
    auto replacement = runtime.createInstance(loaded.asset);
    require(static_cast<bool>(replacement), "replacement instance creation failed");
    const auto newHandle = replacement.instance->handle();
    require(oldHandle.index == newHandle.index,
            "instance handle pool did not reuse the released slot");
    require(oldHandle.generation != newHandle.generation,
            "instance handle generation did not advance after destruction");

    runtime::Runtime assetHandleRuntime;
    auto firstAsset = assetHandleRuntime.loadLottieJson(json, "asset-handle-first");
    require(static_cast<bool>(firstAsset), "first asset-handle load failed");
    const auto oldAssetHandle = firstAsset.asset->handle();
    firstAsset.asset.reset();
    auto secondAsset = assetHandleRuntime.loadLottieJson(json, "asset-handle-second");
    require(static_cast<bool>(secondAsset), "second asset-handle load failed");
    const auto newAssetHandle = secondAsset.asset->handle();
    require(oldAssetHandle.index == newAssetHandle.index,
            "asset handle pool did not reuse the released slot");
    require(oldAssetHandle.generation != newAssetHandle.generation,
            "asset handle generation did not advance after destruction");

    const auto diagnostics = runtime.diagnostics();
    if (upstream.variant == "telegram") {
        require(diagnostics.canonicalGeometriesCreated > 0U,
                "canonical geometry diagnostics did not advance");
        require(diagnostics.canonicalPaintsCreated > 0U,
                "canonical paint diagnostics did not advance");
        require(diagnostics.canonicalResourceConflicts == 0U,
                "canonical resource proof produced conflicts");
    }

    std::cout << "AveMotion canonical geometry tests passed for "
              << upstream.variant << " (local=" << localGeometryItems
              << ", static geometry=" << assetStaticGeometryItems
              << ", static paint=" << assetStaticPaintItems << ")\n";
    return EXIT_SUCCESS;
}
