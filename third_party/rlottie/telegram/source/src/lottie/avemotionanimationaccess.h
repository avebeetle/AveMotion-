// Private AveMotion adapter for the pinned Telegram source; not a public API.
#pragma once

#include "rlottie.h"

class LOTModel;

namespace rlottie {
struct LOT_EXPORT AveMotionAnimationAccess final {
    static std::shared_ptr<LOTModel> model(const Animation& animation);
    static std::unique_ptr<Animation> fromModel(const std::shared_ptr<LOTModel>& model);
};
} // namespace rlottie
