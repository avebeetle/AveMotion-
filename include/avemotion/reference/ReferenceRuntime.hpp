#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace avemotion::reference {

struct PixelBounds final {
    bool valid = false;
    std::size_t x = 0;
    std::size_t y = 0;
    std::size_t width = 0;
    std::size_t height = 0;
};

struct AnimationMetadata final {
    bool loaded = false;
    std::size_t width = 0;
    std::size_t height = 0;
    double frameRate = 0.0;
    double durationSeconds = 0.0;
    std::size_t totalFrames = 0;
    std::string error;
};

struct RenderedFrame final {
    std::size_t frameIndex = 0;
    std::size_t width = 0;
    std::size_t height = 0;
    std::size_t strideBytes = 0;
    std::vector<std::uint32_t> argbPremultiplied;
    std::uint64_t fnv1a64 = 0;
    std::uint64_t alphaSum = 0;
    std::size_t nonTransparentPixels = 0;
    PixelBounds nonTransparentBounds;
};

class ReferenceAnimation final {
public:
    ReferenceAnimation(ReferenceAnimation&&) noexcept;
    ReferenceAnimation& operator=(ReferenceAnimation&&) noexcept;
    ReferenceAnimation(const ReferenceAnimation&) = delete;
    ReferenceAnimation& operator=(const ReferenceAnimation&) = delete;
    ~ReferenceAnimation();

    [[nodiscard]] const AnimationMetadata& metadata() const noexcept;
    [[nodiscard]] std::size_t frameAtPosition(double normalizedPosition) const noexcept;
    [[nodiscard]] RenderedFrame renderFrame(
        std::size_t frameIndex,
        std::size_t width,
        std::size_t height,
        bool keepAspectRatio = true);

private:
    struct Impl;
    explicit ReferenceAnimation(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
    friend class ReferenceRuntime;
};

struct AnimationLoadResult final {
    std::unique_ptr<ReferenceAnimation> animation;
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept {
        return animation != nullptr;
    }
};

class ReferenceRuntime final {
public:
    [[nodiscard]] static bool compiledWithRlottie() noexcept;

    [[nodiscard]] AnimationLoadResult loadJson(
        std::string_view json,
        std::string_view cacheKey = "avemotion-reference",
        bool useModelCache = false) const;

    [[nodiscard]] AnimationMetadata inspectJson(
        std::string_view json,
        std::string_view cacheKey = "avemotion-reference") const;
};

[[nodiscard]] std::uint64_t fnv1a64(std::span<const std::byte> bytes) noexcept;
[[nodiscard]] std::string formatHash(std::uint64_t hash);

} // namespace avemotion::reference
