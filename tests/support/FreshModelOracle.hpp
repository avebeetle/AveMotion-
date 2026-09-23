#pragma once

#include "TelegramParsedModelBuilder.hpp"
#include "RlottieSceneBridge.hpp"
#include "avemotion/core/Hash.hpp"
#include "avemotion/runtime/Runtime.hpp"
#include <rlottie.h>
#include <algorithm>
#include <stdexcept>

namespace avemotion::test {
// Deliberately uses ordinary, newly constructed Animations for every frame.
// Only parsed extraction/update/finalize are shared with the candidate.
class FreshModelOracle final {
public:
    FreshModelOracle(const std::string& json, const runtime::Asset& asset)
        : json_(json), asset_(asset), key_("avemotion-asset-" + core::formatHash(asset.metadata().sourceHash)) {}

    model::detail::AssetModelDescriptor descriptor() const {
        const auto& m = asset_.metadata();
        return {asset_.handle(), m.sourceHash, m.width, m.height,
            m.frameRate, m.totalFrames, m.debugName.c_str()};
    }

    runtime::Instance::SceneResult scene(std::size_t frame, std::size_t width, std::size_t height) const {
        auto animation = rlottie::Animation::loadFromData(json_, key_, {}, true);
        if (!animation) throw std::runtime_error("fresh ordinary oracle load failed");
        auto result = runtime::detail::buildSceneFromRlottieTree(
            animation->renderTree(frame, width, height), asset_.metadata().sourceHash,
            0, frame + 1, frame, width, height);
        result.scene.assetHandle = asset_.handle();
        return {std::move(result.scene), std::move(result.error)};
    }

    std::shared_ptr<const model::MotionAssetModel> build() const {
        auto parsed = model::detail::buildTelegramParsedModel(json_, key_, descriptor());
        if (!parsed) throw std::runtime_error("oracle parsed extraction: " + parsed.error);
        auto model = parsed.model;
        const auto& m = asset_.metadata();
        for (std::size_t frame = 0; frame < m.totalFrames; ++frame) {
            auto sampled = scene(frame, std::max<std::size_t>(1, m.width), std::max<std::size_t>(1, m.height));
            if (!sampled) throw std::runtime_error("oracle sample: " + sampled.error.message);
            auto updated = model::detail::updateAssetModel(model, sampled.scene, descriptor());
            if (!updated) throw std::runtime_error("oracle update: " + updated.error);
            model = std::move(updated.model);
        }
        auto finalized = model::detail::finalizeAssetModel(model);
        if (!finalized) throw std::runtime_error("oracle finalize: " + finalized.error);
        return finalized.model;
    }
private:
    const std::string& json_;
    const runtime::Asset& asset_;
    std::string key_;
};
} // namespace avemotion::test
