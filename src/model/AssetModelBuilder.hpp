#pragma once

#include "avemotion/model/AssetModel.hpp"
#include "avemotion/runtime/EvaluatedScene.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace avemotion::model::detail {

struct AssetModelDescriptor final {
    runtime::AssetHandle assetHandle;
    std::uint64_t sourceAssetHash = 0;
    std::size_t logicalWidth = 0;
    std::size_t logicalHeight = 0;
    double frameRate = 0.0;
    std::size_t totalFrames = 0;
    const char* debugName = nullptr;
};

struct AssetModelUpdateResult final {
    std::shared_ptr<const MotionAssetModel> model;
    bool changed = false;
    std::size_t geometriesCreated = 0;
    std::size_t paintsCreated = 0;
    std::size_t conflicts = 0;
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept {
        return model != nullptr && error.empty();
    }
};

struct AssetModelApplyResult final {
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept {
        return error.empty();
    }
};

// Merges one exact evaluated scene into a copy-on-write asset-model snapshot.
// This function is used only while the model is being prepared. Published
// models are immutable and are never extended during normal instance drawing.
[[nodiscard]] AssetModelUpdateResult updateAssetModel(
    std::shared_ptr<const MotionAssetModel> previous,
    const runtime::EvaluatedScene& scene,
    const AssetModelDescriptor& descriptor);

// Resolves resources that remained Unknown after the complete timeline scan.
// The returned snapshot is immutable and ready for publication.
[[nodiscard]] AssetModelUpdateResult finalizeAssetModel(
    std::shared_ptr<const MotionAssetModel> model);

// Applies a frozen immutable model to a newly evaluated scene. Static values
// are referenced through aliasing shared_ptrs whose owner is the model itself,
// so no per-item canonical allocation is introduced.
[[nodiscard]] AssetModelApplyResult applyAssetModel(
    const std::shared_ptr<const MotionAssetModel>& model,
    runtime::EvaluatedScene& scene);

} // namespace avemotion::model::detail
