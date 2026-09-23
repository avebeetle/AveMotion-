#pragma once

#include "avemotion/runtime/EvaluatedScene.hpp"

#include <string>

namespace avemotion::runtime {

struct SceneRecord final {
    SceneFingerprints fingerprints;
    SceneStatistics statistics;
    RectF controlBounds;
};

class RecordingBackend final {
public:
    [[nodiscard]] SceneRecord record(const EvaluatedScene& scene) const noexcept;
    [[nodiscard]] std::string describe(const EvaluatedScene& scene) const;
};

[[nodiscard]] SceneFingerprints computeSceneFingerprints(
    const EvaluatedScene& scene) noexcept;

} // namespace avemotion::runtime
