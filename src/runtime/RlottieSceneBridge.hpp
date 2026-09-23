#pragma once

#include "avemotion/runtime/EvaluatedScene.hpp"
#include "avemotion/runtime/Runtime.hpp"

#include <cstddef>
#include <cstdint>

struct LOTLayerNode;

namespace avemotion::runtime::detail {

struct SceneBuildOutcome final {
    EvaluatedScene scene;
    RuntimeError error;
    [[nodiscard]] explicit operator bool() const noexcept { return !error; }
};

[[nodiscard]] SceneBuildOutcome buildSceneFromRlottieTree(
    const LOTLayerNode* root,
    std::uint64_t sourceAssetHash,
    std::uint64_t instanceId,
    std::uint64_t evaluationSequence,
    std::size_t frameIndex,
    std::size_t viewportWidth,
    std::size_t viewportHeight);

} // namespace avemotion::runtime::detail
