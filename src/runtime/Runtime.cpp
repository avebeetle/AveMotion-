#include "avemotion/runtime/Runtime.hpp"

#include "RlottieSceneBridge.hpp"
#include "../model/AssetModelBuilder.hpp"
#if AVEMOTION_TELEGRAM_PARSED_MODEL
#include "../model/TelegramParsedModelBuilder.hpp"
#include "avemotionanimationaccess.h"
#endif
#include "avemotion/core/Hash.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <limits>
#include <mutex>
#include <string>
#include <utility>
#include <unordered_map>
#include <vector>

#if AVEMOTION_HAS_RLOTTIE
#include <rlottie.h>
#endif

namespace avemotion::runtime {
namespace detail {

template <typename Handle>
class HandlePool final {
public:
    [[nodiscard]] Handle acquire() {
        std::lock_guard lock{mutex_};
        std::uint32_t index = 0;
        if (!free_.empty()) {
            index = free_.back();
            free_.pop_back();
        } else {
            index = static_cast<std::uint32_t>(generations_.size());
            generations_.push_back(1U);
        }
        return {index, generations_[index]};
    }

    void release(Handle handle) noexcept {
        if (!handle.valid()) return;
        std::lock_guard lock{mutex_};
        if (handle.index >= generations_.size()
            || generations_[handle.index] != handle.generation) {
            return;
        }
        auto& generation = generations_[handle.index];
        ++generation;
        if (generation == 0U) generation = 1U;
        free_.push_back(handle.index);
    }

private:
    std::mutex mutex_;
    std::vector<std::uint32_t> generations_;
    std::vector<std::uint32_t> free_;
};

struct RuntimeState final {
    HandlePool<AssetHandle> assetHandles;
    HandlePool<InstanceHandle> instanceHandles;
    std::atomic<std::uint64_t> nextInstanceId{1U};
    std::atomic<std::uint64_t> assetLoadAttempts{0U};
    std::atomic<std::uint64_t> assetLoadsSucceeded{0U};
    std::atomic<std::uint64_t> assetLoadsFailed{0U};
    std::atomic<std::uint64_t> tgsDecodeAttempts{0U};
    std::atomic<std::uint64_t> tgsDecodesSucceeded{0U};
    std::atomic<std::uint64_t> tgsDecodesFailed{0U};
    std::atomic<std::uint64_t> tgsCompressedBytes{0U};
    std::atomic<std::uint64_t> tgsJsonBytes{0U};
    std::atomic<std::uint64_t> instancesCreated{0U};
    std::atomic<std::uint64_t> referenceMetadataSessionsCreated{0U};
    std::atomic<std::uint64_t> referenceSceneSessionsCreated{0U};
    std::atomic<std::uint64_t> referenceModelSessionsCreated{0U};
    std::atomic<std::uint64_t> referenceCpuSessionsCreated{0U};
    std::atomic<std::uint64_t> referenceSceneSamples{0U};
    std::atomic<std::uint64_t> referenceModelSamples{0U};
    std::atomic<std::uint64_t> sceneEvaluations{0U};
    std::atomic<std::uint64_t> sceneEvaluationFailures{0U};
    std::atomic<std::uint64_t> cpuFramesRendered{0U};
    std::atomic<std::uint64_t> evaluatedLayers{0U};
    std::atomic<std::uint64_t> evaluatedDrawItems{0U};
    std::atomic<std::uint64_t> evaluatedMasks{0U};
    std::atomic<std::uint64_t> copiedPathPoints{0U};
    std::atomic<std::uint64_t> canonicalGeometriesCreated{0U};
    std::atomic<std::uint64_t> canonicalPaintsCreated{0U};
    std::atomic<std::uint64_t> canonicalResourceConflicts{0U};
    std::atomic<std::uint64_t> sceneEvaluationNanoseconds{0U};
    std::atomic<std::uint64_t> cpuRenderNanoseconds{0U};
    std::atomic<std::uint64_t> assetModelBuildAttempts{0U};
    std::atomic<std::uint64_t> assetModelBuildsSucceeded{0U};
    std::atomic<std::uint64_t> assetModelBuildsFailed{0U};
    std::atomic<std::uint64_t> assetModelLayers{0U};
    std::atomic<std::uint64_t> assetModelNodes{0U};
    std::atomic<std::uint64_t> assetModelStaticGeometries{0U};
    std::atomic<std::uint64_t> assetModelStaticPaints{0U};
    std::atomic<std::uint64_t> modelEvaluations{0U};
    std::atomic<std::uint64_t> playbackEvaluations{0U};

    [[nodiscard]] DiagnosticsSnapshot snapshot() const noexcept {
        return {
            assetLoadAttempts.load(std::memory_order_relaxed),
            assetLoadsSucceeded.load(std::memory_order_relaxed),
            assetLoadsFailed.load(std::memory_order_relaxed),
            tgsDecodeAttempts.load(std::memory_order_relaxed),
            tgsDecodesSucceeded.load(std::memory_order_relaxed),
            tgsDecodesFailed.load(std::memory_order_relaxed),
            tgsCompressedBytes.load(std::memory_order_relaxed),
            tgsJsonBytes.load(std::memory_order_relaxed),
            instancesCreated.load(std::memory_order_relaxed),
            referenceMetadataSessionsCreated.load(std::memory_order_relaxed),
            referenceSceneSessionsCreated.load(std::memory_order_relaxed),
            referenceModelSessionsCreated.load(std::memory_order_relaxed),
            referenceCpuSessionsCreated.load(std::memory_order_relaxed),
            referenceSceneSamples.load(std::memory_order_relaxed),
            referenceModelSamples.load(std::memory_order_relaxed),
            sceneEvaluations.load(std::memory_order_relaxed),
            sceneEvaluationFailures.load(std::memory_order_relaxed),
            cpuFramesRendered.load(std::memory_order_relaxed),
            evaluatedLayers.load(std::memory_order_relaxed),
            evaluatedDrawItems.load(std::memory_order_relaxed),
            evaluatedMasks.load(std::memory_order_relaxed),
            copiedPathPoints.load(std::memory_order_relaxed),
            canonicalGeometriesCreated.load(std::memory_order_relaxed),
            canonicalPaintsCreated.load(std::memory_order_relaxed),
            canonicalResourceConflicts.load(std::memory_order_relaxed),
            sceneEvaluationNanoseconds.load(std::memory_order_relaxed),
            cpuRenderNanoseconds.load(std::memory_order_relaxed),
            assetModelBuildAttempts.load(std::memory_order_relaxed),
            assetModelBuildsSucceeded.load(std::memory_order_relaxed),
            assetModelBuildsFailed.load(std::memory_order_relaxed),
            assetModelLayers.load(std::memory_order_relaxed),
            assetModelNodes.load(std::memory_order_relaxed),
            assetModelStaticGeometries.load(std::memory_order_relaxed),
            assetModelStaticPaints.load(std::memory_order_relaxed),
            modelEvaluations.load(std::memory_order_relaxed),
            playbackEvaluations.load(std::memory_order_relaxed),
        };
    }

    void reset() noexcept {
#define AVEMOTION_RESET_COUNTER(name) name.store(0U, std::memory_order_relaxed)
        AVEMOTION_RESET_COUNTER(assetLoadAttempts);
        AVEMOTION_RESET_COUNTER(assetLoadsSucceeded);
        AVEMOTION_RESET_COUNTER(assetLoadsFailed);
        AVEMOTION_RESET_COUNTER(tgsDecodeAttempts);
        AVEMOTION_RESET_COUNTER(tgsDecodesSucceeded);
        AVEMOTION_RESET_COUNTER(tgsDecodesFailed);
        AVEMOTION_RESET_COUNTER(tgsCompressedBytes);
        AVEMOTION_RESET_COUNTER(tgsJsonBytes);
        AVEMOTION_RESET_COUNTER(instancesCreated);
        AVEMOTION_RESET_COUNTER(referenceMetadataSessionsCreated);
        AVEMOTION_RESET_COUNTER(referenceSceneSessionsCreated);
        AVEMOTION_RESET_COUNTER(referenceModelSessionsCreated);
        AVEMOTION_RESET_COUNTER(referenceCpuSessionsCreated);
        AVEMOTION_RESET_COUNTER(referenceSceneSamples);
        AVEMOTION_RESET_COUNTER(referenceModelSamples);
        AVEMOTION_RESET_COUNTER(sceneEvaluations);
        AVEMOTION_RESET_COUNTER(sceneEvaluationFailures);
        AVEMOTION_RESET_COUNTER(cpuFramesRendered);
        AVEMOTION_RESET_COUNTER(evaluatedLayers);
        AVEMOTION_RESET_COUNTER(evaluatedDrawItems);
        AVEMOTION_RESET_COUNTER(evaluatedMasks);
        AVEMOTION_RESET_COUNTER(copiedPathPoints);
        AVEMOTION_RESET_COUNTER(canonicalGeometriesCreated);
        AVEMOTION_RESET_COUNTER(canonicalPaintsCreated);
        AVEMOTION_RESET_COUNTER(canonicalResourceConflicts);
        AVEMOTION_RESET_COUNTER(sceneEvaluationNanoseconds);
        AVEMOTION_RESET_COUNTER(cpuRenderNanoseconds);
        AVEMOTION_RESET_COUNTER(assetModelBuildAttempts);
        AVEMOTION_RESET_COUNTER(assetModelBuildsSucceeded);
        AVEMOTION_RESET_COUNTER(assetModelBuildsFailed);
        AVEMOTION_RESET_COUNTER(assetModelLayers);
        AVEMOTION_RESET_COUNTER(assetModelNodes);
        AVEMOTION_RESET_COUNTER(assetModelStaticGeometries);
        AVEMOTION_RESET_COUNTER(assetModelStaticPaints);
        AVEMOTION_RESET_COUNTER(modelEvaluations);
        AVEMOTION_RESET_COUNTER(playbackEvaluations);
#undef AVEMOTION_RESET_COUNTER
    }
};

struct AssetData final {
    AssetMetadata metadata;
    AssetHandle handle;
    std::string json;
    std::string cacheKey;
#if AVEMOTION_TELEGRAM_PARSED_MODEL
    std::shared_ptr<LOTModel> sourceModel;
#endif
    std::shared_ptr<RuntimeState> runtimeState;

    // Part 5 compatibility oracle. The legacy characterization path keeps
    // its exact source-key semantics so pixel/scene/render-plan goldens remain
    // unchanged while Part 7 owns a separate direct parsed source model.
    mutable std::mutex canonicalMutex;
    mutable std::unordered_map<
        std::uint64_t,
        std::shared_ptr<const CanonicalGeometry>> canonicalGeometries;
    mutable std::unordered_map<
        std::uint64_t,
        std::shared_ptr<const CanonicalPaint>> canonicalPaints;

    mutable std::mutex modelMutex;
    mutable std::shared_ptr<const model::MotionAssetModel> model;
    mutable std::string modelError;

    ~AssetData() {
        if (runtimeState) runtimeState->assetHandles.release(handle);
    }
};

struct PlaybackControl final {
    PlaybackStatus status = PlaybackStatus::Stopped;
    PlaybackDirection direction = PlaybackDirection::Forward;
    PlaybackLoopMode loopMode = PlaybackLoopMode::Loop;
    model::ClipId clip{0U};
    double playbackRate = 1.0;
    double anchorPosition = 0.0;
    MotionTime anchorTime;
    std::uint64_t revision = 1U;
};

struct InstanceData final {
    std::shared_ptr<const Asset> asset;
    std::shared_ptr<const AssetData> assetData;
    std::shared_ptr<RuntimeState> runtimeState;
    InstanceHandle handle;
    std::uint64_t id = 0;
    std::uint64_t evaluationSequence = 0;
    bool hasPreviousFingerprints = false;
    SceneFingerprints previousFingerprints;
    PlaybackControl playback;
#if AVEMOTION_HAS_RLOTTIE
    std::unique_ptr<rlottie::Animation> sceneAnimation;
    std::unique_ptr<rlottie::Animation> cpuAnimation;
#endif

    ~InstanceData() {
        if (runtimeState) runtimeState->instanceHandles.release(handle);
    }
};

} // namespace detail
namespace {

[[nodiscard]] double clampNormalized(double value) noexcept {
    return std::isfinite(value) ? std::clamp(value, 0.0, 1.0) : 0.0;
}

#if AVEMOTION_HAS_RLOTTIE
using Clock = std::chrono::steady_clock;

[[nodiscard]] std::uint64_t elapsedNanoseconds(Clock::time_point start) noexcept {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start)
            .count());
}

[[nodiscard]] std::string makeCacheKey(std::uint64_t sourceHash) {
    return "avemotion-asset-" + core::formatHash(sourceHash);
}

[[nodiscard]] std::size_t clampFrame(
    std::size_t requested,
    const AssetMetadata& metadata) noexcept {
    return metadata.totalFrames == 0U
        ? 0U
        : std::min(requested, metadata.totalFrames - 1U);
}

[[nodiscard]] double positiveUnitModulo(double value) noexcept {
    if (!std::isfinite(value)) return 0.0;
    const auto result = value - std::floor(value);
    return result >= 1.0 ? 0.0 : (result < 0.0 ? result + 1.0 : result);
}

struct ResolvedPlayback final {
    double position = 0.0;
    bool completed = false;
};

[[nodiscard]] ResolvedPlayback resolvePlayback(
    const detail::InstanceData& instance,
    MotionTime presentationTime) noexcept {
    const auto& control = instance.playback;
    if (control.status != PlaybackStatus::Playing) {
        return {
            clampNormalized(control.anchorPosition),
            control.status == PlaybackStatus::Holding};
    }
    const auto duration = instance.asset->metadata().durationSeconds;
    if (!std::isfinite(duration) || duration <= 0.0
        || !std::isfinite(control.playbackRate)
        || control.playbackRate <= 0.0) {
        return {clampNormalized(control.anchorPosition), true};
    }
    const long double deltaNanoseconds =
        static_cast<long double>(presentationTime.nanoseconds)
        - static_cast<long double>(control.anchorTime.nanoseconds);
    const long double elapsedSeconds = deltaNanoseconds / 1'000'000'000.0L;
    const auto sign = control.direction == PlaybackDirection::Forward
        ? 1.0L : -1.0L;
    const long double raw = static_cast<long double>(control.anchorPosition)
        + sign * elapsedSeconds
            * static_cast<long double>(control.playbackRate)
            / static_cast<long double>(duration);
    const auto rawDouble = static_cast<double>(raw);
    if (control.loopMode == PlaybackLoopMode::Loop) {
        // Reverse playback authored from the terminal endpoint must expose the
        // last sample at its exact anchor time before wrapping on later ticks.
        if (control.direction == PlaybackDirection::Reverse
            && control.anchorPosition == 1.0
            && presentationTime == control.anchorTime) {
            return {1.0, false};
        }
        return {positiveUnitModulo(rawDouble), false};
    }
    if (control.direction == PlaybackDirection::Forward) {
        return {clampNormalized(rawDouble), rawDouble >= 1.0};
    }
    return {clampNormalized(rawDouble), rawDouble <= 0.0};
}

void reanchor(
    detail::InstanceData& instance,
    MotionTime presentationTime) noexcept {
    const auto resolved = resolvePlayback(instance, presentationTime);
    instance.playback.anchorPosition = resolved.position;
    instance.playback.anchorTime = presentationTime;
}

[[nodiscard]] std::uint64_t deriveSourceItemKey(
    const EvaluatedScene& scene,
    const EvaluatedDrawItem& item,
    std::uint32_t localOrdinal) noexcept {
    core::Fnv1a64 hash;
    hash.appendU64(scene.sourceAssetHash);
    hash.appendU32(item.layerIndex);
    hash.appendU32(localOrdinal);
    return hash.value();
}

[[nodiscard]] std::uint32_t localOrdinal(
    const EvaluatedScene& scene,
    std::uint32_t sourceItemIndex,
    const EvaluatedDrawItem& item) noexcept {
    if (item.layerIndex >= scene.layers.size()) {
        return sourceItemIndex;
    }
    const auto first = scene.layers[item.layerIndex].firstDrawItem;
    return sourceItemIndex >= first ? sourceItemIndex - first : sourceItemIndex;
}

void appendColor(core::Fnv1a64& hash, const Color8& color) noexcept {
    hash.appendU8(color.r);
    hash.appendU8(color.g);
    hash.appendU8(color.b);
    hash.appendU8(color.a);
}

[[nodiscard]] std::uint64_t hashCanonicalGeometry(
    FillRule fillRule,
    const EvaluatedPath& path) noexcept {
    core::Fnv1a64 hash;
    hash.appendU8(static_cast<std::uint8_t>(fillRule));
    hash.appendU64(path.hash);
    hash.appendU64(path.verbs.size());
    hash.appendU64(path.points.size());
    return hash.value();
}

[[nodiscard]] std::uint64_t hashCanonicalPaint(
    const EvaluatedStroke& stroke,
    const EvaluatedPaint& paint) noexcept {
    core::Fnv1a64 hash;
    hash.appendU8(static_cast<std::uint8_t>(paint.kind));
    hash.appendU8(stroke.enabled ? 1U : 0U);
    hash.appendFloat(stroke.width);
    hash.appendFloat(stroke.miterLimit);
    hash.appendU8(static_cast<std::uint8_t>(stroke.cap));
    hash.appendU8(static_cast<std::uint8_t>(stroke.join));
    hash.appendU64(stroke.dashArray.size());
    for (const auto value : stroke.dashArray) {
        hash.appendFloat(value);
    }
    switch (paint.kind) {
    case PaintKind::Solid:
        appendColor(hash, paint.solid);
        break;
    case PaintKind::Gradient:
        hash.appendU8(static_cast<std::uint8_t>(paint.gradient.kind));
        hash.appendFloat(paint.gradient.start.x);
        hash.appendFloat(paint.gradient.start.y);
        hash.appendFloat(paint.gradient.end.x);
        hash.appendFloat(paint.gradient.end.y);
        hash.appendFloat(paint.gradient.center.x);
        hash.appendFloat(paint.gradient.center.y);
        hash.appendFloat(paint.gradient.focal.x);
        hash.appendFloat(paint.gradient.focal.y);
        hash.appendFloat(paint.gradient.centerRadius);
        hash.appendFloat(paint.gradient.focalRadius);
        hash.appendU64(paint.gradient.stops.size());
        for (const auto& stop : paint.gradient.stops) {
            hash.appendFloat(stop.position);
            appendColor(hash, stop.color);
        }
        break;
    case PaintKind::Image:
        hash.appendU64(paint.image.width);
        hash.appendU64(paint.image.height);
        for (const auto value : paint.image.matrix) {
            hash.appendFloat(value);
        }
        break;
    case PaintKind::None:
        break;
    }
    return hash.value();
}

[[nodiscard]] bool samePath(
    const EvaluatedPath& left,
    const EvaluatedPath& right) noexcept {
    if (left.verbs != right.verbs || left.points.size() != right.points.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.points.size(); ++index) {
        if (left.points[index].x != right.points[index].x
            || left.points[index].y != right.points[index].y) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool sameStroke(
    const EvaluatedStroke& left,
    const EvaluatedStroke& right) noexcept {
    return left.enabled == right.enabled
        && left.width == right.width
        && left.miterLimit == right.miterLimit
        && left.cap == right.cap
        && left.join == right.join
        && left.dashArray == right.dashArray;
}

[[nodiscard]] bool samePaint(
    const EvaluatedPaint& left,
    const EvaluatedPaint& right) noexcept {
    if (left.kind != right.kind) {
        return false;
    }
    switch (left.kind) {
    case PaintKind::Solid:
        return left.solid.r == right.solid.r
            && left.solid.g == right.solid.g
            && left.solid.b == right.solid.b
            && left.solid.a == right.solid.a;
    case PaintKind::Gradient:
        if (left.gradient.kind != right.gradient.kind
            || left.gradient.start.x != right.gradient.start.x
            || left.gradient.start.y != right.gradient.start.y
            || left.gradient.end.x != right.gradient.end.x
            || left.gradient.end.y != right.gradient.end.y
            || left.gradient.center.x != right.gradient.center.x
            || left.gradient.center.y != right.gradient.center.y
            || left.gradient.focal.x != right.gradient.focal.x
            || left.gradient.focal.y != right.gradient.focal.y
            || left.gradient.centerRadius != right.gradient.centerRadius
            || left.gradient.focalRadius != right.gradient.focalRadius
            || left.gradient.stops.size() != right.gradient.stops.size()) {
            return false;
        }
        for (std::size_t index = 0; index < left.gradient.stops.size(); ++index) {
            const auto& a = left.gradient.stops[index];
            const auto& b = right.gradient.stops[index];
            if (a.position != b.position
                || a.color.r != b.color.r || a.color.g != b.color.g
                || a.color.b != b.color.b || a.color.a != b.color.a) {
                return false;
            }
        }
        return true;
    case PaintKind::Image:
        if (left.image.present != right.image.present
            || left.image.width != right.image.width
            || left.image.height != right.image.height) {
            return false;
        }
        for (std::size_t index = 0; index < std::size(left.image.matrix); ++index) {
            if (left.image.matrix[index] != right.image.matrix[index]) {
                return false;
            }
        }
        return true;
    case PaintKind::None:
        return true;
    }
    return false;
}

void promoteCanonicalResources(
    EvaluatedScene& scene,
    const detail::AssetData& asset) {
    scene.assetHandle = asset.handle;
    std::lock_guard lock{asset.canonicalMutex};
    for (std::size_t sourceIndex = 0; sourceIndex < scene.drawItems.size(); ++sourceIndex) {
        auto& item = scene.drawItems[sourceIndex];
        const auto sourceIndex32 = static_cast<std::uint32_t>(sourceIndex);
        const auto ordinal = localOrdinal(scene, sourceIndex32, item);
        const auto sourceKey = deriveSourceItemKey(scene, item, ordinal);

        if (item.localGeometryAvailable) {
            item.sourceGeometryId = sourceKey;
        }
        if (item.localPaintAvailable) {
            item.sourcePaintId = sourceKey;
        }

        if (item.localGeometryAvailable
            && item.localPaintAvailable
            && item.localGeometryStaticCandidate) {
            const auto contentHash = hashCanonicalGeometry(item.fillRule, item.localPath);
            const auto found = asset.canonicalGeometries.find(sourceKey);
            if (found == asset.canonicalGeometries.end()) {
                auto canonical = std::make_shared<CanonicalGeometry>();
                canonical->sourceKey = sourceKey;
                canonical->contentHash = contentHash;
                canonical->fillRule = item.fillRule;
                canonical->path = item.localPath;
                item.canonicalGeometry = canonical;
                asset.canonicalGeometries.emplace(sourceKey, std::move(canonical));
                asset.runtimeState->canonicalGeometriesCreated.fetch_add(
                    1U, std::memory_order_relaxed);
            } else if (found->second->contentHash == contentHash
                       && found->second->fillRule == item.fillRule
                       && samePath(found->second->path, item.localPath)) {
                item.canonicalGeometry = found->second;
            } else {
                asset.runtimeState->canonicalResourceConflicts.fetch_add(
                    1U, std::memory_order_relaxed);
            }
            if (item.canonicalGeometry) {
                item.geometryOrigin = EvaluatedValueOrigin::AssetStatic;
                // The immutable asset-owned object is now authoritative. Drop
                // the per-evaluation duplicate while retaining the separated
                // transform and provenance metadata.
                item.localPath = {};
            }
        }

        if (item.localPaintAvailable && item.localPaintStaticCandidate) {
            const auto contentHash = hashCanonicalPaint(
                item.localStroke, item.localPaint);
            const auto found = asset.canonicalPaints.find(sourceKey);
            if (found == asset.canonicalPaints.end()) {
                auto canonical = std::make_shared<CanonicalPaint>();
                canonical->sourceKey = sourceKey;
                canonical->contentHash = contentHash;
                canonical->stroke = item.localStroke;
                canonical->paint = item.localPaint;
                item.canonicalPaint = canonical;
                asset.canonicalPaints.emplace(sourceKey, std::move(canonical));
                asset.runtimeState->canonicalPaintsCreated.fetch_add(
                    1U, std::memory_order_relaxed);
            } else if (found->second->contentHash == contentHash
                       && sameStroke(found->second->stroke, item.localStroke)
                       && samePaint(found->second->paint, item.localPaint)) {
                item.canonicalPaint = found->second;
            } else {
                asset.runtimeState->canonicalResourceConflicts.fetch_add(
                    1U, std::memory_order_relaxed);
            }
            if (item.canonicalPaint) {
                item.paintOrigin = EvaluatedValueOrigin::AssetStatic;
            }
        }
    }
}

enum class ReferenceSessionRole { Metadata, Scene, ModelPreparation, Cpu };
enum class ReferenceSampleRole { Scene, ModelPreparation };

[[nodiscard]] std::unique_ptr<rlottie::Animation> loadUpstreamAnimation(
    const detail::AssetData& asset,
    ReferenceSessionRole role) {
#if AVEMOTION_TELEGRAM_PARSED_MODEL
    auto animation = role == ReferenceSessionRole::Metadata
        ? rlottie::Animation::loadFromData(asset.json, asset.cacheKey, {}, true)
        : rlottie::AveMotionAnimationAccess::fromModel(asset.sourceModel);
#else
    auto animation = rlottie::Animation::loadFromData(
        asset.json, asset.cacheKey, {}, true);
#endif
    if (animation) {
        auto& state = *asset.runtimeState;
        switch (role) {
        case ReferenceSessionRole::Metadata:
            state.referenceMetadataSessionsCreated.fetch_add(
                1U, std::memory_order_relaxed);
            break;
        case ReferenceSessionRole::Scene:
            state.referenceSceneSessionsCreated.fetch_add(
                1U, std::memory_order_relaxed);
            break;
        case ReferenceSessionRole::ModelPreparation:
            state.referenceModelSessionsCreated.fetch_add(
                1U, std::memory_order_relaxed);
            break;
        case ReferenceSessionRole::Cpu:
            state.referenceCpuSessionsCreated.fetch_add(
                1U, std::memory_order_relaxed);
            break;
        }
    }
    return animation;
}

[[nodiscard]] CpuFrame analyzeCpuFrame(
    std::size_t frameIndex,
    std::size_t width,
    std::size_t height,
    std::vector<std::uint32_t> pixels) {
    CpuFrame result;
    result.frameIndex = frameIndex;
    result.width = width;
    result.height = height;
    result.strideBytes = width * sizeof(std::uint32_t);
    result.argbPremultiplied = std::move(pixels);
    result.fnv1a64 = core::fnv1a64({
        reinterpret_cast<const std::byte*>(result.argbPremultiplied.data()),
        result.argbPremultiplied.size() * sizeof(std::uint32_t)});

    float minX = static_cast<float>(width);
    float minY = static_cast<float>(height);
    float maxX = 0.0F;
    float maxY = 0.0F;
    for (std::size_t y = 0; y < height; ++y) {
        for (std::size_t x = 0; x < width; ++x) {
            const auto pixel = result.argbPremultiplied[y * width + x];
            const auto alpha = static_cast<std::uint8_t>((pixel >> 24U) & 0xFFU);
            result.alphaSum += alpha;
            if (alpha == 0U) continue;
            ++result.nonTransparentPixels;
            minX = std::min(minX, static_cast<float>(x));
            minY = std::min(minY, static_cast<float>(y));
            maxX = std::max(maxX, static_cast<float>(x + 1U));
            maxY = std::max(maxY, static_cast<float>(y + 1U));
        }
    }
    if (result.nonTransparentPixels != 0U) {
        result.nonTransparentBounds = {true, minX, minY, maxX, maxY};
    }
    return result;
}

[[nodiscard]] model::detail::AssetModelDescriptor modelDescriptor(
    const detail::AssetData& asset) noexcept {
    return {
        .assetHandle = asset.handle,
        .sourceAssetHash = asset.metadata.sourceHash,
        .logicalWidth = asset.metadata.width,
        .logicalHeight = asset.metadata.height,
        .frameRate = asset.metadata.frameRate,
        .totalFrames = asset.metadata.totalFrames,
        .debugName = asset.metadata.debugName.c_str(),
    };
}

[[nodiscard]] constexpr bool supportsStableAssetModel() noexcept {
#if AVEMOTION_TELEGRAM_LOCAL_GEOMETRY_EXTENSIONS
    return true;
#else
    return false;
#endif
}

[[nodiscard]] Instance::SceneResult extractExactScene(
    rlottie::Animation& animation,
    const detail::AssetData& asset,
    AssetHandle assetHandle,
    InstanceHandle instanceHandle,
    std::uint64_t instanceId,
    std::uint64_t evaluationSequence,
    std::size_t frameIndex,
    std::size_t viewportWidth,
    std::size_t viewportHeight,
    ReferenceSampleRole role) {
    Instance::SceneResult result;
    if (viewportWidth == 0U || viewportHeight == 0U) {
        result.error = {
            RuntimeErrorCode::InvalidArgument,
            "scene viewport dimensions must be positive"};
        return result;
    }

    frameIndex = clampFrame(frameIndex, asset.metadata);
    auto& samples = role == ReferenceSampleRole::Scene
        ? asset.runtimeState->referenceSceneSamples
        : asset.runtimeState->referenceModelSamples;
    samples.fetch_add(1U, std::memory_order_relaxed);
    const auto* tree = animation.renderTreeForRecording(
        frameIndex, viewportWidth, viewportHeight);
    auto built = detail::buildSceneFromRlottieTree(
        tree,
        asset.metadata.sourceHash,
        instanceId,
        evaluationSequence,
        frameIndex,
        viewportWidth,
        viewportHeight);
    if (!built) {
        result.error = std::move(built.error);
        return result;
    }
    built.scene.assetHandle = assetHandle;
    built.scene.instanceHandle = instanceHandle;
    result.scene = std::move(built.scene);
    return result;
}

[[nodiscard]] model::AssetModelResult prepareStableAssetModel(
    const detail::AssetData& asset) {
    std::lock_guard lock{asset.modelMutex};
    if (asset.model) return {asset.model, {}};

    asset.runtimeState->assetModelBuildAttempts.fetch_add(
        1U, std::memory_order_relaxed);
    if (!supportsStableAssetModel()) {
        asset.modelError =
            "the selected comparison engine does not expose stable Telegram source IDs";
        asset.runtimeState->assetModelBuildsFailed.fetch_add(
            1U, std::memory_order_relaxed);
        return {nullptr, asset.modelError};
    }
    constexpr std::size_t kMaximumPreparationFrames = 10'000U;
    if (asset.metadata.totalFrames == 0U
        || asset.metadata.totalFrames > kMaximumPreparationFrames) {
        asset.modelError = "asset timeline exceeds reference-model preparation limit";
        asset.runtimeState->assetModelBuildsFailed.fetch_add(
            1U, std::memory_order_relaxed);
        return {nullptr, asset.modelError};
    }

    std::shared_ptr<const model::MotionAssetModel> model;
#if AVEMOTION_TELEGRAM_PARSED_MODEL
    {
        const auto parsed = model::detail::buildTelegramParsedModel(
            asset.sourceModel, modelDescriptor(asset));
        if (!parsed) {
            asset.modelError = parsed.error.empty()
                ? "direct Telegram parsed-model extraction failed"
                : parsed.error;
            asset.runtimeState->assetModelBuildsFailed.fetch_add(
                1U, std::memory_order_relaxed);
            return {nullptr, asset.modelError};
        }
        model = parsed.model;
    }
#endif
    std::size_t geometriesCreated = 0U;
    std::size_t paintsCreated = 0U;
    std::size_t conflicts = 0U;
    const auto width = std::max<std::size_t>(1U, asset.metadata.width);
    const auto height = std::max<std::size_t>(1U, asset.metadata.height);

    auto scan = loadUpstreamAnimation(asset, ReferenceSessionRole::ModelPreparation);
    if (!scan || !scan->enableRecordingLifecycle()) {
        asset.modelError = "unable to create a recording model-preparation runtime tree";
        asset.runtimeState->assetModelBuildsFailed.fetch_add(1U, std::memory_order_relaxed);
        return {nullptr, asset.modelError};
    }

    for (std::size_t frame = 0; frame < asset.metadata.totalFrames; ++frame) {
        auto extracted = extractExactScene(
            *scan, asset, asset.handle, {}, 0U, frame + 1U, frame, width, height,
            ReferenceSampleRole::ModelPreparation);
        if (!extracted) {
            asset.modelError = extracted.error.message;
            asset.runtimeState->assetModelBuildsFailed.fetch_add(
                1U, std::memory_order_relaxed);
            return {nullptr, asset.modelError};
        }
        auto updated = model::detail::updateAssetModel(
            model, extracted.scene, modelDescriptor(asset));
        if (!updated) {
            asset.modelError = updated.error.empty()
                ? "asset model update failed" : updated.error;
            asset.runtimeState->assetModelBuildsFailed.fetch_add(
                1U, std::memory_order_relaxed);
            return {nullptr, asset.modelError};
        }
        model = std::move(updated.model);
        geometriesCreated += updated.geometriesCreated;
        paintsCreated += updated.paintsCreated;
        conflicts += updated.conflicts;
    }

    if (!model) {
        asset.modelError = "asset model preparation produced no snapshot";
        asset.runtimeState->assetModelBuildsFailed.fetch_add(
            1U, std::memory_order_relaxed);
        return {nullptr, asset.modelError};
    }
    auto finalized = model::detail::finalizeAssetModel(model);
    if (!finalized) {
        asset.modelError = finalized.error;
        asset.runtimeState->assetModelBuildsFailed.fetch_add(
            1U, std::memory_order_relaxed);
        return {nullptr, asset.modelError};
    }
    model = std::move(finalized.model);

    asset.model = model;
    asset.modelError.clear();
    asset.runtimeState->assetModelBuildsSucceeded.fetch_add(
        1U, std::memory_order_relaxed);
    asset.runtimeState->assetModelLayers.fetch_add(
        model->statistics.observedLayerCount, std::memory_order_relaxed);
    asset.runtimeState->assetModelNodes.fetch_add(
        model->statistics.observedNodeCount, std::memory_order_relaxed);
    asset.runtimeState->assetModelStaticGeometries.fetch_add(
        model->statistics.assetStaticGeometryCount, std::memory_order_relaxed);
    asset.runtimeState->assetModelStaticPaints.fetch_add(
        model->statistics.assetStaticPaintCount, std::memory_order_relaxed);
    asset.runtimeState->canonicalGeometriesCreated.fetch_add(
        geometriesCreated, std::memory_order_relaxed);
    asset.runtimeState->canonicalPaintsCreated.fetch_add(
        paintsCreated, std::memory_order_relaxed);
    asset.runtimeState->canonicalResourceConflicts.fetch_add(
        conflicts, std::memory_order_relaxed);
    return {asset.model, {}};
}

[[nodiscard]] RuntimeError applyPreparedAssetModel(
    EvaluatedScene& scene,
    const detail::AssetData& asset) {
    if (!supportsStableAssetModel()) return {};
    const auto prepared = prepareStableAssetModel(asset);
    if (!prepared) {
        return {
            RuntimeErrorCode::AssetModelPreparationFailed,
            prepared.error.empty() ? "asset model preparation failed" : prepared.error};
    }
    const auto applied = model::detail::applyAssetModel(prepared.model, scene);
    if (!applied) {
        return {
            RuntimeErrorCode::AssetModelPreparationFailed,
            applied.error.empty() ? "asset model application failed" : applied.error};
    }
    return {};
}

[[nodiscard]] Instance::SceneResult evaluateExactFrame(
    detail::InstanceData& instance,
    std::size_t frameIndex,
    std::size_t viewportWidth,
    std::size_t viewportHeight,
    bool classifyChanges,
    bool applyModel) {
    const auto started = Clock::now();
    auto result = extractExactScene(
        *instance.sceneAnimation,
        *instance.assetData,
        instance.asset->handle(),
        instance.handle,
        instance.id,
        ++instance.evaluationSequence,
        frameIndex,
        viewportWidth,
        viewportHeight,
        ReferenceSampleRole::Scene);
    instance.runtimeState->sceneEvaluations.fetch_add(
        1U, std::memory_order_relaxed);
    instance.runtimeState->sceneEvaluationNanoseconds.fetch_add(
        elapsedNanoseconds(started), std::memory_order_relaxed);
    if (!result) {
        instance.runtimeState->sceneEvaluationFailures.fetch_add(
            1U, std::memory_order_relaxed);
        return result;
    }

    auto& scene = result.scene;
    if (applyModel) {
        const auto modelError = applyPreparedAssetModel(
            scene, *instance.assetData);
        if (modelError) {
            result.error = modelError;
            instance.runtimeState->sceneEvaluationFailures.fetch_add(
                1U, std::memory_order_relaxed);
            return result;
        }
    } else {
        promoteCanonicalResources(scene, *instance.assetData);
    }
    if (classifyChanges) {
        scene.changes.firstEvaluation = !instance.hasPreviousFingerprints;
        if (instance.hasPreviousFingerprints) {
            scene.changes.topologyChanged =
                scene.fingerprints.topology != instance.previousFingerprints.topology;
            scene.changes.geometryChanged =
                scene.fingerprints.geometry != instance.previousFingerprints.geometry;
            scene.changes.paintChanged =
                scene.fingerprints.paint != instance.previousFingerprints.paint;
            scene.changes.visualChanged =
                scene.fingerprints.scene != instance.previousFingerprints.scene;
        }
        instance.previousFingerprints = scene.fingerprints;
        instance.hasPreviousFingerprints = true;
    }

    instance.runtimeState->evaluatedLayers.fetch_add(
        scene.statistics.layerCount, std::memory_order_relaxed);
    instance.runtimeState->evaluatedDrawItems.fetch_add(
        scene.statistics.drawItemCount, std::memory_order_relaxed);
    instance.runtimeState->evaluatedMasks.fetch_add(
        scene.statistics.maskCount, std::memory_order_relaxed);
    instance.runtimeState->copiedPathPoints.fetch_add(
        scene.statistics.pathPointCount, std::memory_order_relaxed);
    return result;
}
#endif

} // namespace

Asset::Asset(std::shared_ptr<const detail::AssetData> data) noexcept
    : data_(std::move(data)) {
}

Asset::~Asset() = default;

const AssetMetadata& Asset::metadata() const noexcept { return data_->metadata; }
AssetHandle Asset::handle() const noexcept { return data_->handle; }

std::size_t Asset::canonicalGeometryCount() const noexcept {
    std::lock_guard lock{data_->canonicalMutex};
    return data_->canonicalGeometries.size();
}

std::size_t Asset::canonicalPaintCount() const noexcept {
    std::lock_guard lock{data_->canonicalMutex};
    return data_->canonicalPaints.size();
}

model::AssetModelResult Asset::prepareModel() const {
#if AVEMOTION_HAS_RLOTTIE
    return prepareStableAssetModel(*data_);
#else
    return {nullptr, "asset model preparation requires rlottie"};
#endif
}

std::shared_ptr<const model::MotionAssetModel> Asset::model() const {
    std::lock_guard lock{data_->modelMutex};
    return data_->model;
}

Instance::Instance(std::unique_ptr<detail::InstanceData> data) noexcept
    : data_(std::move(data)) {
}
Instance::Instance(Instance&&) noexcept = default;
Instance& Instance::operator=(Instance&&) noexcept = default;
Instance::~Instance() = default;

const AssetMetadata& Instance::assetMetadata() const noexcept {
    return data_->asset->metadata();
}
std::uint64_t Instance::id() const noexcept { return data_->id; }
InstanceHandle Instance::handle() const noexcept { return data_->handle; }

std::size_t Instance::frameAtPosition(double normalizedPosition) const noexcept {
#if AVEMOTION_HAS_RLOTTIE
    const auto& metadata = data_->asset->metadata();
    if (metadata.totalFrames > static_cast<std::size_t>(std::numeric_limits<long>::max())) {
        return clampFrame(
            data_->sceneAnimation->frameAtPos(clampNormalized(normalizedPosition)),
            metadata);
    }
    if (metadata.totalFrames <= 1U) return 0U;
    const double scaled = clampNormalized(normalizedPosition)
        * static_cast<double>(metadata.totalFrames - 1U);
#if AVEMOTION_REFERENCE_ROUND_FRAME_POSITION
    const auto frame = static_cast<std::size_t>(std::round(scaled));
#else
    const auto frame = static_cast<std::size_t>(scaled);
#endif
    return clampFrame(frame, metadata);
#else
    static_cast<void>(normalizedPosition);
    return 0U;
#endif
}

void Instance::play(MotionTime presentationTime) noexcept {
#if AVEMOTION_HAS_RLOTTIE
    if (data_->playback.status == PlaybackStatus::Playing) return;
    if (data_->playback.status == PlaybackStatus::Holding) {
        data_->playback.anchorPosition =
            data_->playback.direction == PlaybackDirection::Forward ? 0.0 : 1.0;
    }
    data_->playback.anchorTime = presentationTime;
    data_->playback.status = PlaybackStatus::Playing;
    ++data_->playback.revision;
#else
    static_cast<void>(presentationTime);
#endif
}

void Instance::pause(MotionTime presentationTime) noexcept {
#if AVEMOTION_HAS_RLOTTIE
    if (data_->playback.status != PlaybackStatus::Playing) return;
    reanchor(*data_, presentationTime);
    data_->playback.status = PlaybackStatus::Paused;
    ++data_->playback.revision;
#else
    static_cast<void>(presentationTime);
#endif
}

void Instance::resume(MotionTime presentationTime) noexcept {
#if AVEMOTION_HAS_RLOTTIE
    if (data_->playback.status == PlaybackStatus::Playing) return;
    data_->playback.anchorTime = presentationTime;
    data_->playback.status = PlaybackStatus::Playing;
    ++data_->playback.revision;
#else
    static_cast<void>(presentationTime);
#endif
}

void Instance::stop() noexcept {
    data_->playback.status = PlaybackStatus::Stopped;
    data_->playback.anchorPosition =
        data_->playback.direction == PlaybackDirection::Forward ? 0.0 : 1.0;
    ++data_->playback.revision;
}

void Instance::seekNormalized(
    double normalizedPosition,
    MotionTime presentationTime) noexcept {
    data_->playback.anchorPosition = clampNormalized(normalizedPosition);
    data_->playback.anchorTime = presentationTime;
    if (data_->playback.status == PlaybackStatus::Holding) {
        data_->playback.status = PlaybackStatus::Paused;
    }
    ++data_->playback.revision;
}

void Instance::setControlledProgress(double normalizedPosition) noexcept {
    data_->playback.anchorPosition = clampNormalized(normalizedPosition);
    data_->playback.status = PlaybackStatus::Controlled;
    ++data_->playback.revision;
}

void Instance::setDirection(
    PlaybackDirection direction,
    MotionTime presentationTime) noexcept {
#if AVEMOTION_HAS_RLOTTIE
    if (data_->playback.direction == direction) return;
    if (data_->playback.status == PlaybackStatus::Stopped) {
        data_->playback.anchorPosition =
            direction == PlaybackDirection::Forward ? 0.0 : 1.0;
        data_->playback.anchorTime = presentationTime;
    } else {
        reanchor(*data_, presentationTime);
    }
#else
    static_cast<void>(presentationTime);
#endif
    data_->playback.direction = direction;
    ++data_->playback.revision;
}

bool Instance::setPlaybackRate(
    double playbackRate,
    MotionTime presentationTime) noexcept {
    if (!std::isfinite(playbackRate) || playbackRate <= 0.0) return false;
#if AVEMOTION_HAS_RLOTTIE
    reanchor(*data_, presentationTime);
#else
    static_cast<void>(presentationTime);
#endif
    data_->playback.playbackRate = playbackRate;
    ++data_->playback.revision;
    return true;
}

void Instance::setLoopMode(PlaybackLoopMode loopMode) noexcept {
    data_->playback.loopMode = loopMode;
    ++data_->playback.revision;
}

PlaybackSnapshot Instance::playbackSnapshot(
    MotionTime presentationTime) const noexcept {
    PlaybackSnapshot snapshot;
    snapshot.status = data_->playback.status;
    snapshot.direction = data_->playback.direction;
    snapshot.loopMode = data_->playback.loopMode;
    snapshot.clip = data_->playback.clip;
    snapshot.playbackRate = data_->playback.playbackRate;
    snapshot.revision = data_->playback.revision;
#if AVEMOTION_HAS_RLOTTIE
    const auto resolved = resolvePlayback(*data_, presentationTime);
    snapshot.normalizedPosition = resolved.position;
    snapshot.frameIndex = frameAtPosition(resolved.position);
    snapshot.completed = resolved.completed;
    if (resolved.completed && snapshot.status == PlaybackStatus::Playing) {
        snapshot.status = PlaybackStatus::Holding;
    }
#else
    static_cast<void>(presentationTime);
#endif
    return snapshot;
}

Instance::SceneResult Instance::evaluateFrame(
    std::size_t frameIndex,
    std::size_t viewportWidth,
    std::size_t viewportHeight) {
#if AVEMOTION_HAS_RLOTTIE
    return evaluateExactFrame(
        *data_, frameIndex, viewportWidth, viewportHeight, true, false);
#else
    static_cast<void>(frameIndex);
    static_cast<void>(viewportWidth);
    static_cast<void>(viewportHeight);
    return {{}, {RuntimeErrorCode::ReferenceUnavailable,
        "AveMotion runtime was built without rlottie"}};
#endif
}

Instance::SceneResult Instance::evaluatePosition(
    double normalizedPosition,
    std::size_t viewportWidth,
    std::size_t viewportHeight) {
    return evaluateFrame(
        frameAtPosition(normalizedPosition), viewportWidth, viewportHeight);
}

Instance::SceneResult Instance::evaluateModelFrame(
    std::size_t frameIndex,
    std::size_t viewportWidth,
    std::size_t viewportHeight) {
#if AVEMOTION_HAS_RLOTTIE
    const auto prepared = data_->asset->prepareModel();
    if (!prepared) {
        return {{}, {RuntimeErrorCode::AssetModelPreparationFailed,
            prepared.error.empty() ? "asset model preparation failed"
                                   : prepared.error}};
    }
    auto result = evaluateExactFrame(
        *data_, frameIndex, viewportWidth, viewportHeight, true, true);
    if (result) {
        data_->runtimeState->modelEvaluations.fetch_add(
            1U, std::memory_order_relaxed);
    }
    return result;
#else
    static_cast<void>(frameIndex);
    static_cast<void>(viewportWidth);
    static_cast<void>(viewportHeight);
    return {{}, {RuntimeErrorCode::ReferenceUnavailable,
        "AveMotion runtime was built without rlottie"}};
#endif
}

Instance::SceneResult Instance::evaluateModelPosition(
    double normalizedPosition,
    std::size_t viewportWidth,
    std::size_t viewportHeight) {
    return evaluateModelFrame(
        frameAtPosition(normalizedPosition), viewportWidth, viewportHeight);
}

Instance::SceneResult Instance::evaluateAt(
    MotionTime presentationTime,
    std::size_t viewportWidth,
    std::size_t viewportHeight) {
    auto snapshot = playbackSnapshot(presentationTime);
    auto result = evaluateModelFrame(
        snapshot.frameIndex, viewportWidth, viewportHeight);
    if (result) {
        data_->runtimeState->playbackEvaluations.fetch_add(
            1U, std::memory_order_relaxed);
        if (snapshot.completed
            && data_->playback.status == PlaybackStatus::Playing
            && data_->playback.loopMode == PlaybackLoopMode::Once) {
            data_->playback.anchorPosition = snapshot.normalizedPosition;
            data_->playback.anchorTime = presentationTime;
            data_->playback.status = PlaybackStatus::Holding;
            ++data_->playback.revision;
        }
    }
    return result;
}

Instance::CpuFrameResult Instance::renderCpuFrame(
    std::size_t frameIndex,
    std::size_t width,
    std::size_t height,
    bool keepAspectRatio) {
    CpuFrameResult result;
    if (width == 0U || height == 0U) {
        result.error = {RuntimeErrorCode::InvalidArgument,
            "CPU render dimensions must be positive"};
        return result;
    }
#if AVEMOTION_HAS_RLOTTIE
    const auto started = Clock::now();
    if (!data_->cpuAnimation) {
        data_->cpuAnimation = loadUpstreamAnimation(
            *data_->assetData, ReferenceSessionRole::Cpu);
        if (!data_->cpuAnimation) {
            result.error = {RuntimeErrorCode::CpuRenderFailed,
                "unable to create the isolated CPU oracle animation"};
            return result;
        }
    }
    frameIndex = clampFrame(frameIndex, data_->asset->metadata());
    std::vector<std::uint32_t> pixels(width * height, 0U);
    rlottie::Surface surface{
        pixels.data(), width, height, width * sizeof(std::uint32_t)};
    data_->cpuAnimation->renderSync(frameIndex, surface, keepAspectRatio);
    result.frame = analyzeCpuFrame(frameIndex, width, height, std::move(pixels));
    data_->runtimeState->cpuFramesRendered.fetch_add(
        1U, std::memory_order_relaxed);
    data_->runtimeState->cpuRenderNanoseconds.fetch_add(
        elapsedNanoseconds(started), std::memory_order_relaxed);
#else
    static_cast<void>(frameIndex);
    static_cast<void>(keepAspectRatio);
    result.error = {RuntimeErrorCode::ReferenceUnavailable,
        "CPU oracle rendering is unavailable without rlottie"};
#endif
    return result;
}

Runtime::Runtime()
    : state_(std::make_shared<detail::RuntimeState>()) {
}
Runtime::Runtime(Runtime&&) noexcept = default;
Runtime& Runtime::operator=(Runtime&&) noexcept = default;
Runtime::~Runtime() = default;

bool Runtime::compiledWithReferenceEngine() noexcept {
#if AVEMOTION_HAS_RLOTTIE
    return true;
#else
    return false;
#endif
}

AssetLoadResult Runtime::loadLottieJson(
    std::string_view json,
    std::string_view debugName) {
    AssetLoadResult result;
    state_->assetLoadAttempts.fetch_add(1U, std::memory_order_relaxed);
    if (json.empty()) {
        state_->assetLoadsFailed.fetch_add(1U, std::memory_order_relaxed);
        result.error = {RuntimeErrorCode::InvalidAsset, "Lottie JSON is empty"};
        return result;
    }
#if AVEMOTION_HAS_RLOTTIE
    const auto sourceHash = core::fnv1a64({
        reinterpret_cast<const std::byte*>(json.data()), json.size()});
    auto data = std::make_shared<detail::AssetData>();
    data->handle = state_->assetHandles.acquire();
    data->json.assign(json.begin(), json.end());
    data->cacheKey = makeCacheKey(sourceHash);
    data->runtimeState = state_;
    data->metadata.sourceHash = sourceHash;
    data->metadata.debugName = debugName.empty()
        ? core::formatHash(sourceHash)
        : std::string{debugName};

    auto animation = loadUpstreamAnimation(
        *data, ReferenceSessionRole::Metadata);
    if (!animation) {
        state_->assetLoadsFailed.fetch_add(1U, std::memory_order_relaxed);
        result.error = {RuntimeErrorCode::InvalidAsset,
            "rlottie rejected the Lottie JSON asset"};
        return result;
    }
    animation->size(data->metadata.width, data->metadata.height);
    data->metadata.frameRate = animation->frameRate();
    data->metadata.durationSeconds = animation->duration();
    data->metadata.totalFrames = animation->totalFrame();
    if (data->metadata.width == 0U || data->metadata.height == 0U
        || data->metadata.totalFrames == 0U
        || !std::isfinite(data->metadata.frameRate)
        || data->metadata.frameRate <= 0.0) {
        state_->assetLoadsFailed.fetch_add(1U, std::memory_order_relaxed);
        result.error = {RuntimeErrorCode::InvalidAsset,
            "the loaded asset has invalid metadata"};
        return result;
    }
#if AVEMOTION_TELEGRAM_PARSED_MODEL
    data->sourceModel = rlottie::AveMotionAnimationAccess::model(*animation);
    if (!data->sourceModel) {
        state_->assetLoadsFailed.fetch_add(1U, std::memory_order_relaxed);
        result.error = {RuntimeErrorCode::InvalidAsset,
            "rlottie returned no parsed Lottie source"};
        return result;
    }
#endif

    auto asset = std::shared_ptr<const Asset>{new Asset{data}};
    result.asset = std::move(asset);
    state_->assetLoadsSucceeded.fetch_add(1U, std::memory_order_relaxed);
#else
    static_cast<void>(debugName);
    state_->assetLoadsFailed.fetch_add(1U, std::memory_order_relaxed);
    result.error = {RuntimeErrorCode::ReferenceUnavailable,
        "AveMotion runtime was built without rlottie"};
#endif
    return result;
}


AssetLoadResult Runtime::loadTgs(
    std::span<const std::byte> bytes,
    std::string_view debugName,
    formats::TgsDecodeLimits limits) {
    state_->tgsDecodeAttempts.fetch_add(1U, std::memory_order_relaxed);
    auto decoded = formats::decodeTgs(bytes, limits);
    if (!decoded) {
        state_->tgsDecodesFailed.fetch_add(1U, std::memory_order_relaxed);
        state_->assetLoadAttempts.fetch_add(1U, std::memory_order_relaxed);
        state_->assetLoadsFailed.fetch_add(1U, std::memory_order_relaxed);
        AssetLoadResult result;
        result.error = {
            RuntimeErrorCode::ContainerDecodeFailed,
            std::string{"TGS decode failed ("}
                + formats::toString(decoded.error.code)
                + ") at byte " + std::to_string(decoded.error.offset)
                + ": " + decoded.error.message};
        return result;
    }

    state_->tgsDecodesSucceeded.fetch_add(1U, std::memory_order_relaxed);
    state_->tgsCompressedBytes.fetch_add(
        decoded.metadata.compressedBytes, std::memory_order_relaxed);
    state_->tgsJsonBytes.fetch_add(
        decoded.metadata.jsonBytes, std::memory_order_relaxed);
    return loadLottieJson(decoded.json, debugName);
}

AssetLoadResult Runtime::loadTgsFile(
    const std::filesystem::path& path,
    std::string_view debugName,
    formats::TgsDecodeLimits limits) {
    state_->tgsDecodeAttempts.fetch_add(1U, std::memory_order_relaxed);
    auto decoded = formats::decodeTgsFile(path, limits);
    if (!decoded) {
        state_->tgsDecodesFailed.fetch_add(1U, std::memory_order_relaxed);
        state_->assetLoadAttempts.fetch_add(1U, std::memory_order_relaxed);
        state_->assetLoadsFailed.fetch_add(1U, std::memory_order_relaxed);
        AssetLoadResult result;
        result.error = {
            RuntimeErrorCode::ContainerDecodeFailed,
            std::string{"TGS file decode failed ("}
                + formats::toString(decoded.error.code)
                + ") at byte " + std::to_string(decoded.error.offset)
                + ": " + decoded.error.message};
        return result;
    }

    state_->tgsDecodesSucceeded.fetch_add(1U, std::memory_order_relaxed);
    state_->tgsCompressedBytes.fetch_add(
        decoded.metadata.compressedBytes, std::memory_order_relaxed);
    state_->tgsJsonBytes.fetch_add(
        decoded.metadata.jsonBytes, std::memory_order_relaxed);

    std::string pathName;
    if (debugName.empty()) {
        const auto utf8 = path.filename().u8string();
        pathName.assign(
            reinterpret_cast<const char*>(utf8.data()), utf8.size());
        debugName = pathName;
    }
    return loadLottieJson(decoded.json, debugName);
}

AssetLoadResult Runtime::loadAssetData(
    std::span<const std::byte> bytes,
    std::string_view debugName,
    formats::TgsDecodeLimits tgsLimits) {
    switch (formats::detectAssetFormat(bytes)) {
    case formats::AssetFormat::TelegramTgs:
        return loadTgs(bytes, debugName, tgsLimits);
    case formats::AssetFormat::LottieJson: {
        const auto json = std::string_view{
            reinterpret_cast<const char*>(bytes.data()), bytes.size()};
        if (!formats::isValidUtf8(json)) {
            state_->assetLoadAttempts.fetch_add(1U, std::memory_order_relaxed);
            state_->assetLoadsFailed.fetch_add(1U, std::memory_order_relaxed);
            AssetLoadResult result;
            result.error = {RuntimeErrorCode::InvalidAsset,
                "the Lottie JSON input is not valid UTF-8"};
            return result;
        }
        return loadLottieJson(json, debugName);
    }
    case formats::AssetFormat::Unknown:
        break;
    }

    state_->assetLoadAttempts.fetch_add(1U, std::memory_order_relaxed);
    state_->assetLoadsFailed.fetch_add(1U, std::memory_order_relaxed);
    AssetLoadResult result;
    result.error = {RuntimeErrorCode::InvalidAsset,
        "the animation input is neither a JSON object nor a gzip TGS member"};
    return result;
}

InstanceCreateResult Runtime::createInstance(
    std::shared_ptr<const Asset> asset) const {
    InstanceCreateResult result;
    if (!asset || !asset->data_) {
        result.error = {RuntimeErrorCode::InvalidArgument,
            "createInstance requires a valid asset"};
        return result;
    }
    if (asset->data_->runtimeState != state_) {
        result.error = {RuntimeErrorCode::InvalidArgument,
            "the asset belongs to a different AveMotion Runtime"};
        return result;
    }
#if AVEMOTION_HAS_RLOTTIE
    auto data = std::make_unique<detail::InstanceData>();
    data->asset = std::move(asset);
    data->assetData = data->asset->data_;
    data->runtimeState = state_;
    data->handle = state_->instanceHandles.acquire();
    data->id = state_->nextInstanceId.fetch_add(1U, std::memory_order_relaxed);
    data->sceneAnimation = loadUpstreamAnimation(
        *data->asset->data_, ReferenceSessionRole::Scene);
    if (!data->sceneAnimation || !data->sceneAnimation->enableRecordingLifecycle()) {
        result.error = {RuntimeErrorCode::InstanceCreationFailed,
            "unable to create the upstream scene-evaluation instance"};
        return result;
    }
    result.instance = std::unique_ptr<Instance>{new Instance{std::move(data)}};
    state_->instancesCreated.fetch_add(1U, std::memory_order_relaxed);
#else
    result.error = {RuntimeErrorCode::ReferenceUnavailable,
        "AveMotion runtime was built without rlottie"};
#endif
    return result;
}

DiagnosticsSnapshot Runtime::diagnostics() const noexcept {
    return state_->snapshot();
}
void Runtime::resetDiagnostics() noexcept { state_->reset(); }

} // namespace avemotion::runtime
