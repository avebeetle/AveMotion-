#include "avemotion/reference/ReferenceRuntime.hpp"

#include "avemotion/core/Hash.hpp"

#include <algorithm>
#include <array>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

#if AVEMOTION_HAS_RLOTTIE
#include <rlottie.h>
#endif

namespace avemotion::reference {

namespace {
#if AVEMOTION_HAS_RLOTTIE
RenderedFrame analyzeFrame(
    std::size_t frameIndex,
    std::size_t width,
    std::size_t height,
    std::vector<std::uint32_t> pixels) {
    RenderedFrame result;
    result.frameIndex = frameIndex;
    result.width = width;
    result.height = height;
    result.strideBytes = width * sizeof(std::uint32_t);
    result.argbPremultiplied = std::move(pixels);

    const auto byteSpan = std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(result.argbPremultiplied.data()),
        result.argbPremultiplied.size() * sizeof(std::uint32_t)};
    result.fnv1a64 = fnv1a64(byteSpan);

    std::size_t minX = width;
    std::size_t minY = height;
    std::size_t maxX = 0;
    std::size_t maxY = 0;
    for (std::size_t y = 0; y < height; ++y) {
        for (std::size_t x = 0; x < width; ++x) {
            const auto pixel = result.argbPremultiplied[y * width + x];
            const auto alpha = static_cast<std::uint8_t>((pixel >> 24U) & 0xFFU);
            result.alphaSum += alpha;
            if (alpha == 0U) {
                continue;
            }
            ++result.nonTransparentPixels;
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
        }
    }
    if (result.nonTransparentPixels != 0U) {
        result.nonTransparentBounds = {
            true,
            minX,
            minY,
            maxX - minX + 1U,
            maxY - minY + 1U};
    }
    return result;
}
#endif

} // namespace

std::uint64_t fnv1a64(std::span<const std::byte> bytes) noexcept {
    return core::fnv1a64(bytes);
}

std::string formatHash(std::uint64_t hash) {
    return core::formatHash(hash);
}

struct ReferenceAnimation::Impl final {
#if AVEMOTION_HAS_RLOTTIE
    std::unique_ptr<rlottie::Animation> animation;
#endif
    AnimationMetadata metadata;
};

ReferenceAnimation::ReferenceAnimation(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {
}

ReferenceAnimation::ReferenceAnimation(ReferenceAnimation&&) noexcept = default;
ReferenceAnimation& ReferenceAnimation::operator=(ReferenceAnimation&&) noexcept = default;
ReferenceAnimation::~ReferenceAnimation() = default;

const AnimationMetadata& ReferenceAnimation::metadata() const noexcept {
    return impl_->metadata;
}

std::size_t ReferenceAnimation::frameAtPosition(double normalizedPosition) const noexcept {
#if AVEMOTION_HAS_RLOTTIE
    const auto position = std::clamp(normalizedPosition, 0.0, 1.0);
    return impl_->animation->frameAtPos(position);
#else
    static_cast<void>(normalizedPosition);
    return 0U;
#endif
}

RenderedFrame ReferenceAnimation::renderFrame(
    std::size_t frameIndex,
    std::size_t width,
    std::size_t height,
    bool keepAspectRatio) {
    if (width == 0U || height == 0U) {
        return {};
    }
#if AVEMOTION_HAS_RLOTTIE
    const auto totalFrames = impl_->metadata.totalFrames;
    if (totalFrames != 0U) {
        frameIndex = std::min(frameIndex, totalFrames - 1U);
    }
    std::vector<std::uint32_t> pixels(width * height, 0U);
    rlottie::Surface surface{
        pixels.data(), width, height, width * sizeof(std::uint32_t)};
    impl_->animation->renderSync(frameIndex, surface, keepAspectRatio);
    return analyzeFrame(frameIndex, width, height, std::move(pixels));
#else
    static_cast<void>(frameIndex);
    static_cast<void>(keepAspectRatio);
    return {};
#endif
}

bool ReferenceRuntime::compiledWithRlottie() noexcept {
#if AVEMOTION_HAS_RLOTTIE
    return true;
#else
    return false;
#endif
}

AnimationLoadResult ReferenceRuntime::loadJson(
    std::string_view json,
    std::string_view cacheKey,
    bool useModelCache) const {
    AnimationLoadResult result;
#if AVEMOTION_HAS_RLOTTIE
    auto animation = rlottie::Animation::loadFromData(
        std::string{json}, std::string{cacheKey}, {}, useModelCache);
    if (!animation) {
        result.error = "rlottie rejected the JSON asset";
        return result;
    }

    auto impl = std::make_unique<ReferenceAnimation::Impl>();
    impl->animation = std::move(animation);
    impl->animation->size(impl->metadata.width, impl->metadata.height);
    impl->metadata.frameRate = impl->animation->frameRate();
    impl->metadata.durationSeconds = impl->animation->duration();
    impl->metadata.totalFrames = impl->animation->totalFrame();
    impl->metadata.loaded = true;
    result.animation = std::unique_ptr<ReferenceAnimation>{
        new ReferenceAnimation{std::move(impl)}};
#else
    static_cast<void>(json);
    static_cast<void>(cacheKey);
    static_cast<void>(useModelCache);
    result.error = "AveMotion was built without an rlottie reference variant";
#endif
    return result;
}

AnimationMetadata ReferenceRuntime::inspectJson(
    std::string_view json,
    std::string_view cacheKey) const {
    auto loaded = loadJson(json, cacheKey, false);
    if (!loaded) {
        AnimationMetadata metadata;
        metadata.error = std::move(loaded.error);
        return metadata;
    }
    return loaded.animation->metadata();
}

} // namespace avemotion::reference
