#pragma once

#include "avemotion/render/RenderPlan.hpp"

#include <cstdint>
#include <memory>
#include <string>

struct ID2D1DeviceContext;

namespace avemotion::backends::direct2d {

enum class BackendErrorCode : std::uint8_t {
    None,
    InvalidArgument,
    InvalidPlan,
    UnsupportedFeature,
    ResourceCreationFailed,
    DeviceLost,
    DrawFailed,
};

struct BackendError final {
    BackendErrorCode code = BackendErrorCode::None;
    long nativeCode = 0;
    std::string message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return code != BackendErrorCode::None;
    }
};

struct RenderSession final {
    ID2D1DeviceContext* deviceContext = nullptr;
    std::uint64_t graphicsDomainId = 0;
    std::uint64_t graphicsGeneration = 0;
};

struct BackendDiagnostics final {
    std::uint64_t drawCalls = 0;
    std::uint64_t drawItemsVisited = 0;
    std::uint64_t solidItemsDrawn = 0;
    std::uint64_t unsupportedItemsSkipped = 0;
    std::uint64_t geometryCacheHits = 0;
    std::uint64_t geometryCacheMisses = 0;
    std::uint64_t geometryResourcesCreated = 0;
    std::uint64_t geometryResourcesReplaced = 0;
    std::uint64_t strokeStyleCacheHits = 0;
    std::uint64_t strokeStyleCacheMisses = 0;
    std::uint64_t strokeStylesCreated = 0;
    std::uint64_t resourceDomainResets = 0;
};

struct DrawResult final {
    BackendError error;
    std::uint64_t itemsDrawn = 0;
    std::uint64_t itemsSkipped = 0;

    [[nodiscard]] explicit operator bool() const noexcept { return !error; }
};

class Backend final {
public:
    Backend();
    Backend(Backend&&) noexcept;
    Backend& operator=(Backend&&) noexcept;
    Backend(const Backend&) = delete;
    Backend& operator=(const Backend&) = delete;
    ~Backend();

    // The host owns BeginDraw/EndDraw, the target, and presentation. AveMotion
    // only records commands into the supplied device context.
    [[nodiscard]] DrawResult draw(
        const render::MotionRenderPlan& plan,
        const RenderSession& session);

    void invalidateGraphicsDomain(
        std::uint64_t graphicsDomainId,
        std::uint64_t graphicsGeneration) noexcept;
    void clear() noexcept;

    [[nodiscard]] BackendDiagnostics diagnostics() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace avemotion::backends::direct2d
