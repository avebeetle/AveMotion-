#pragma once

#include "TelegramParsedModelBuilder.hpp"
#include "RlottieSceneBridge.hpp"
#include "avemotion/core/Hash.hpp"
#include "avemotion/runtime/RecordingBackend.hpp"
#include "avemotionanimationaccess.h"

#include <rlottie.h>

#include <algorithm>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>

namespace avemotion::test {

class NativeEllipseOracle final {
public:
    explicit NativeEllipseOracle(std::string json) : json_(std::move(json)) {
        static std::uint64_t nextSuffix = 0;
        const auto key = "native-ellipse-oracle-" + std::to_string(++nextSuffix);
        auto seed = rlottie::Animation::loadFromData(json_, key, {}, false);
        if (!seed) throw std::runtime_error("oracle seed parse failed");
        ++directParseCount_;
        seed->size(width_, height_);
        frameRate_ = seed->frameRate();
        totalFrames_ = seed->totalFrame();
        sourceHash_ = core::fnv1a64(std::as_bytes(std::span{json_.data(), json_.size()}));
        debugName_ = core::formatHash(sourceHash_);
        source_ = rlottie::AveMotionAnimationAccess::model(*seed);
        if (!source_) throw std::runtime_error("oracle source lease missing");
        auto anotherSeed = rlottie::Animation::loadFromData(json_, key + "-second", {}, false);
        if (!anotherSeed || !rlottie::AveMotionAnimationAccess::model(*anotherSeed)
            || rlottie::AveMotionAnimationAccess::model(*anotherSeed).get() == source_.get())
            throw std::runtime_error("cache-disabled parses shared source");
        ++directParseCount_;
        auto first = rlottie::AveMotionAnimationAccess::fromModel(source_);
        auto second = rlottie::AveMotionAnimationAccess::fromModel(source_);
        if (!first || !second || first.get() == second.get()
            || rlottie::AveMotionAnimationAccess::model(*first).get() != source_.get()
            || rlottie::AveMotionAnimationAccess::model(*second).get() != source_.get())
            throw std::runtime_error("ordinary Animation lease independence failed");
        seed.reset();
        anotherSeed.reset();
        auto parsed = model::detail::buildTelegramParsedModel(source_, descriptor());
        if (!parsed) throw std::runtime_error("oracle parsed extraction: " + parsed.error);
        model_ = parsed.model;
        for (std::size_t frame = 0; frame < totalFrames_; ++frame) {
            auto scene = sample(frame, std::max<std::size_t>(1, width_),
                                std::max<std::size_t>(1, height_), frame + 1);
            auto updated = model::detail::updateAssetModel(model_, scene, descriptor());
            if (!updated) throw std::runtime_error("oracle model update: " + updated.error);
            model_ = std::move(updated.model);
        }
        auto finalized = model::detail::finalizeAssetModel(model_);
        if (!finalized) throw std::runtime_error("oracle finalization: " + finalized.error);
        model_ = std::move(finalized.model);
    }

    [[nodiscard]] runtime::EvaluatedScene freshScene(std::size_t frame,
                                                     std::size_t width,
                                                     std::size_t height) {
        ++attemptOrdinal_;
        auto scene = sample(std::min(frame, totalFrames_ - 1U), width, height,
                            attemptOrdinal_);
        auto applied = model::detail::applyAssetModel(model_, scene);
        if (!applied) throw std::runtime_error("oracle model application: " + applied.error);
        scene.fingerprints = runtime::computeSceneFingerprints(scene);
        scene.changes.firstEvaluation = !hasPrevious_;
        if (hasPrevious_) {
            scene.changes.topologyChanged = scene.fingerprints.topology != previous_.topology;
            scene.changes.geometryChanged = scene.fingerprints.geometry != previous_.geometry;
            scene.changes.paintChanged = scene.fingerprints.paint != previous_.paint;
            scene.changes.visualChanged = scene.fingerprints.scene != previous_.scene;
        }
        previous_ = scene.fingerprints;
        hasPrevious_ = true;
        return scene;
    }

    [[nodiscard]] const std::shared_ptr<const model::MotionAssetModel>& model() const noexcept {
        return model_;
    }
    [[nodiscard]] std::size_t directSampleCount() const noexcept { return directSamples_; }
    [[nodiscard]] std::size_t directParseCount() const noexcept { return directParseCount_; }

private:
    [[nodiscard]] model::detail::AssetModelDescriptor descriptor() const noexcept {
        return {{700001, 1}, sourceHash_, width_, height_, frameRate_, totalFrames_,
                debugName_.c_str()};
    }
    [[nodiscard]] runtime::EvaluatedScene sample(std::size_t frame, std::size_t width,
                                                 std::size_t height, std::uint64_t sequence) {
        auto animation = rlottie::AveMotionAnimationAccess::fromModel(source_);
        if (!animation || rlottie::AveMotionAnimationAccess::model(*animation).get() != source_.get())
            throw std::runtime_error("oracle request Animation source mismatch");
        ++directSamples_;
        auto built = runtime::detail::buildSceneFromRlottieTree(
            animation->renderTree(frame, width, height), sourceHash_, 0, sequence,
            frame, width, height);
        if (!built) throw std::runtime_error("oracle bridge: " + built.error.message);
        built.scene.assetHandle = descriptor().assetHandle;
        return std::move(built.scene);
    }
    std::string json_;
    std::string debugName_;
    std::shared_ptr<LOTModel> source_;
    std::shared_ptr<const model::MotionAssetModel> model_;
    std::size_t width_ = 0, height_ = 0, totalFrames_ = 0;
    double frameRate_ = 0.0;
    std::uint64_t sourceHash_ = 0, attemptOrdinal_ = 0;
    runtime::SceneFingerprints previous_;
    bool hasPrevious_ = false;
    std::size_t directParseCount_ = 0, directSamples_ = 0;
};

inline void normalizeNativeOracleIdentity(
    runtime::EvaluatedScene& scene,
    const runtime::detail::NativeEllipseCertificate& certificate,
    std::uint64_t instanceId, std::uint64_t sequence) {
    scene.assetHandle = certificate.asset->handle();
    scene.instanceHandle = {};
    scene.instanceId = instanceId;
    scene.evaluationSequence = sequence;
}

} // namespace avemotion::test
