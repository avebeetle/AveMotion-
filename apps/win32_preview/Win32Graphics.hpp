#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <d2d1_1.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <cstdint>
#include <filesystem>
#include <string>

namespace avemotion::preview {

struct GraphicsError final {
    HRESULT nativeCode = S_OK;
    std::wstring message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return FAILED(nativeCode) || !message.empty();
    }
};

class Win32Graphics final {
public:
    Win32Graphics();
    Win32Graphics(const Win32Graphics&) = delete;
    Win32Graphics& operator=(const Win32Graphics&) = delete;
    ~Win32Graphics();

    [[nodiscard]] bool initialize(
        HWND window,
        bool forceWarp,
        std::uint32_t pixelWidth,
        std::uint32_t pixelHeight,
        float dpi,
        GraphicsError& error);

    [[nodiscard]] bool resize(
        std::uint32_t pixelWidth,
        std::uint32_t pixelHeight,
        float dpi,
        GraphicsError& error);

    [[nodiscard]] bool recreate(GraphicsError& error);
    void release() noexcept;

    [[nodiscard]] bool beginFrame(const D2D1_COLOR_F& clearColor, GraphicsError& error);
    [[nodiscard]] bool endFrame(GraphicsError& error);
    [[nodiscard]] bool present(GraphicsError& error);

    [[nodiscard]] bool saveBackBufferPpm(
        const std::filesystem::path& output,
        GraphicsError& error) const;

    [[nodiscard]] ID2D1DeviceContext* deviceContext() const noexcept;
    [[nodiscard]] std::uint64_t graphicsDomainId() const noexcept;
    [[nodiscard]] std::uint64_t graphicsGeneration() const noexcept;
    [[nodiscard]] std::uint32_t pixelWidth() const noexcept;
    [[nodiscard]] std::uint32_t pixelHeight() const noexcept;
    [[nodiscard]] float dpi() const noexcept;
    [[nodiscard]] bool usingWarp() const noexcept;
    [[nodiscard]] bool ready() const noexcept;

    [[nodiscard]] static bool isDeviceLoss(HRESULT value) noexcept;

private:
    [[nodiscard]] bool createDevice(GraphicsError& error);
    [[nodiscard]] bool createSwapChain(GraphicsError& error);
    [[nodiscard]] bool createTarget(GraphicsError& error);
    void releaseTarget() noexcept;
    void incrementGeneration() noexcept;

    HWND window_ = nullptr;
    bool forceWarp_ = false;
    bool usingWarp_ = false;
    bool drawing_ = false;
    std::uint32_t pixelWidth_ = 0;
    std::uint32_t pixelHeight_ = 0;
    float dpi_ = 96.0F;
    std::uint64_t graphicsDomainId_ = 0;
    std::uint64_t graphicsGeneration_ = 0;

    Microsoft::WRL::ComPtr<ID3D11Device> d3dDevice_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3dContext_;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain_;
    Microsoft::WRL::ComPtr<ID2D1Factory1> d2dFactory_;
    Microsoft::WRL::ComPtr<ID2D1Device> d2dDevice_;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> d2dContext_;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> targetBitmap_;
};

} // namespace avemotion::preview
