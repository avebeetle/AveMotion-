#include "Win32Graphics.hpp"

#include <d2d1helper.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

namespace avemotion::preview {
namespace {

using Microsoft::WRL::ComPtr;

std::atomic<std::uint64_t> gNextDomainId{1U};

void setError(GraphicsError& error, HRESULT code, std::wstring message) {
    error.nativeCode = code;
    error.message = std::move(message);
}

std::wstring formatHresult(std::wstring_view operation, HRESULT value) {
    std::wostringstream stream;
    stream << operation << L" failed with HRESULT 0x"
           << std::hex << std::uppercase
           << static_cast<std::uint32_t>(value);
    return stream.str();
}

bool checkedPixelCount(
    std::uint32_t width,
    std::uint32_t height,
    std::size_t& output) noexcept {
    const auto count = static_cast<std::uint64_t>(width) * height;
    if (count > static_cast<std::uint64_t>(
            std::numeric_limits<std::size_t>::max() / 4U)) {
        return false;
    }
    output = static_cast<std::size_t>(count);
    return true;
}

} // namespace

Win32Graphics::Win32Graphics()
    : graphicsDomainId_(gNextDomainId.fetch_add(1U, std::memory_order_relaxed)) {
}

Win32Graphics::~Win32Graphics() {
    release();
}

bool Win32Graphics::initialize(
    HWND window,
    bool forceWarp,
    std::uint32_t pixelWidth,
    std::uint32_t pixelHeight,
    float dpi,
    GraphicsError& error) {
    error = {};
    release();
    if (window == nullptr || pixelWidth == 0U || pixelHeight == 0U
        || !std::isfinite(dpi) || dpi <= 0.0F) {
        setError(error, E_INVALIDARG, L"Invalid Win32 graphics initialization arguments");
        return false;
    }
    window_ = window;
    forceWarp_ = forceWarp;
    pixelWidth_ = pixelWidth;
    pixelHeight_ = pixelHeight;
    dpi_ = dpi;
    incrementGeneration();
    if (!createDevice(error) || !createSwapChain(error) || !createTarget(error)) {
        release();
        return false;
    }
    return true;
}

bool Win32Graphics::resize(
    std::uint32_t pixelWidth,
    std::uint32_t pixelHeight,
    float dpi,
    GraphicsError& error) {
    error = {};
    if (!std::isfinite(dpi) || dpi <= 0.0F) {
        setError(error, E_INVALIDARG, L"Invalid DPI supplied to Win32Graphics::resize");
        return false;
    }
    dpi_ = dpi;
    pixelWidth_ = pixelWidth;
    pixelHeight_ = pixelHeight;
    if (!swapChain_ || pixelWidth == 0U || pixelHeight == 0U) {
        releaseTarget();
        return true;
    }

    releaseTarget();
    const auto hr = swapChain_->ResizeBuffers(
        0U,
        pixelWidth,
        pixelHeight,
        DXGI_FORMAT_UNKNOWN,
        0U);
    if (FAILED(hr)) {
        setError(error, hr, formatHresult(L"IDXGISwapChain1::ResizeBuffers", hr));
        return false;
    }
    return createTarget(error);
}

bool Win32Graphics::recreate(GraphicsError& error) {
    error = {};
    if (window_ == nullptr || pixelWidth_ == 0U || pixelHeight_ == 0U) {
        setError(error, E_FAIL, L"Cannot recreate graphics without a valid window and size");
        return false;
    }
    const auto window = window_;
    const auto forceWarp = forceWarp_;
    const auto width = pixelWidth_;
    const auto height = pixelHeight_;
    const auto dpi = dpi_;
    release();
    window_ = window;
    forceWarp_ = forceWarp;
    pixelWidth_ = width;
    pixelHeight_ = height;
    dpi_ = dpi;
    incrementGeneration();
    if (!createDevice(error) || !createSwapChain(error) || !createTarget(error)) {
        release();
        return false;
    }
    return true;
}

void Win32Graphics::release() noexcept {
    if (drawing_ && d2dContext_) {
        static_cast<void>(d2dContext_->EndDraw());
    }
    drawing_ = false;
    releaseTarget();
    swapChain_.Reset();
    d2dContext_.Reset();
    d2dDevice_.Reset();
    d2dFactory_.Reset();
    d3dContext_.Reset();
    d3dDevice_.Reset();
    usingWarp_ = false;
}

bool Win32Graphics::beginFrame(
    const D2D1_COLOR_F& clearColor,
    GraphicsError& error) {
    error = {};
    if (!ready() || drawing_) {
        setError(error, E_FAIL, L"Direct2D frame cannot begin in the current state");
        return false;
    }
    d2dContext_->SetTarget(targetBitmap_.Get());
    d2dContext_->SetDpi(dpi_, dpi_);
    d2dContext_->SetUnitMode(D2D1_UNIT_MODE_DIPS);
    d2dContext_->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    d2dContext_->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
    d2dContext_->SetTransform(D2D1::Matrix3x2F::Identity());
    d2dContext_->BeginDraw();
    d2dContext_->Clear(clearColor);
    drawing_ = true;
    return true;
}

bool Win32Graphics::endFrame(GraphicsError& error) {
    error = {};
    if (!drawing_ || !d2dContext_) {
        setError(error, E_FAIL, L"Direct2D frame was not active");
        return false;
    }
    drawing_ = false;
    const auto hr = d2dContext_->EndDraw();
    if (FAILED(hr)) {
        setError(error, hr, formatHresult(L"ID2D1DeviceContext::EndDraw", hr));
        return false;
    }
    return true;
}

bool Win32Graphics::present(GraphicsError& error) {
    error = {};
    if (!swapChain_) {
        setError(error, E_FAIL, L"Swap chain is unavailable");
        return false;
    }
    const auto hr = swapChain_->Present(1U, 0U);
    if (hr == DXGI_STATUS_OCCLUDED) {
        return true;
    }
    if (FAILED(hr)) {
        setError(error, hr, formatHresult(L"IDXGISwapChain1::Present", hr));
        return false;
    }
    return true;
}

bool Win32Graphics::saveBackBufferPpm(
    const std::filesystem::path& output,
    GraphicsError& error) const {
    error = {};
    if (!swapChain_ || !d3dDevice_ || !d3dContext_
        || pixelWidth_ == 0U || pixelHeight_ == 0U) {
        setError(error, E_FAIL, L"Back buffer is unavailable for capture");
        return false;
    }

    ComPtr<ID3D11Texture2D> backBuffer;
    auto hr = swapChain_->GetBuffer(0U, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr)) {
        setError(error, hr, formatHresult(L"IDXGISwapChain1::GetBuffer", hr));
        return false;
    }

    D3D11_TEXTURE2D_DESC description{};
    backBuffer->GetDesc(&description);
    description.Usage = D3D11_USAGE_STAGING;
    description.BindFlags = 0U;
    description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    description.MiscFlags = 0U;

    ComPtr<ID3D11Texture2D> staging;
    hr = d3dDevice_->CreateTexture2D(&description, nullptr, &staging);
    if (FAILED(hr)) {
        setError(error, hr, formatHresult(L"ID3D11Device::CreateTexture2D(staging)", hr));
        return false;
    }
    d3dContext_->CopyResource(staging.Get(), backBuffer.Get());

    D3D11_MAPPED_SUBRESOURCE mapped{};
    hr = d3dContext_->Map(staging.Get(), 0U, D3D11_MAP_READ, 0U, &mapped);
    if (FAILED(hr)) {
        setError(error, hr, formatHresult(L"ID3D11DeviceContext::Map", hr));
        return false;
    }

    std::size_t pixelCount = 0U;
    if (!checkedPixelCount(pixelWidth_, pixelHeight_, pixelCount)) {
        d3dContext_->Unmap(staging.Get(), 0U);
        setError(error, E_OUTOFMEMORY, L"Back-buffer dimensions overflow addressable memory");
        return false;
    }
    std::vector<std::uint8_t> rgb(pixelCount * 3U);
    for (std::uint32_t y = 0U; y < pixelHeight_; ++y) {
        const auto* row = static_cast<const std::uint8_t*>(mapped.pData)
            + static_cast<std::size_t>(mapped.RowPitch) * y;
        for (std::uint32_t x = 0U; x < pixelWidth_; ++x) {
            const auto sourceOffset = static_cast<std::size_t>(x) * 4U;
            const auto targetOffset =
                (static_cast<std::size_t>(y) * pixelWidth_ + x) * 3U;
            rgb[targetOffset + 0U] = row[sourceOffset + 2U];
            rgb[targetOffset + 1U] = row[sourceOffset + 1U];
            rgb[targetOffset + 2U] = row[sourceOffset + 0U];
        }
    }
    d3dContext_->Unmap(staging.Get(), 0U);

    std::error_code filesystemError;
    if (output.has_parent_path()) {
        std::filesystem::create_directories(output.parent_path(), filesystemError);
        if (filesystemError) {
            setError(error, E_FAIL, L"Unable to create capture artifact directory");
            return false;
        }
    }
    std::ofstream stream(output, std::ios::binary);
    if (!stream) {
        setError(error, E_FAIL, L"Unable to create PPM capture file");
        return false;
    }
    stream << "P6\n" << pixelWidth_ << ' ' << pixelHeight_ << "\n255\n";
    stream.write(
        reinterpret_cast<const char*>(rgb.data()),
        static_cast<std::streamsize>(rgb.size()));
    if (!stream) {
        setError(error, E_FAIL, L"Unable to write PPM capture file");
        return false;
    }
    return true;
}

ID2D1DeviceContext* Win32Graphics::deviceContext() const noexcept {
    return d2dContext_.Get();
}

std::uint64_t Win32Graphics::graphicsDomainId() const noexcept {
    return graphicsDomainId_;
}

std::uint64_t Win32Graphics::graphicsGeneration() const noexcept {
    return graphicsGeneration_;
}

std::uint32_t Win32Graphics::pixelWidth() const noexcept {
    return pixelWidth_;
}

std::uint32_t Win32Graphics::pixelHeight() const noexcept {
    return pixelHeight_;
}

float Win32Graphics::dpi() const noexcept {
    return dpi_;
}

bool Win32Graphics::usingWarp() const noexcept {
    return usingWarp_;
}

bool Win32Graphics::ready() const noexcept {
    return d2dContext_ && targetBitmap_ && swapChain_
        && pixelWidth_ != 0U && pixelHeight_ != 0U;
}

bool Win32Graphics::isDeviceLoss(HRESULT value) noexcept {
    return value == D2DERR_RECREATE_TARGET
        || value == DXGI_ERROR_DEVICE_REMOVED
        || value == DXGI_ERROR_DEVICE_RESET
        || value == DXGI_ERROR_DRIVER_INTERNAL_ERROR;
}

bool Win32Graphics::createDevice(GraphicsError& error) {
    constexpr std::array<D3D_FEATURE_LEVEL, 5> featureLevels{
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
    };
    const auto create = [&](D3D_DRIVER_TYPE driverType) {
        D3D_FEATURE_LEVEL selected{};
        return D3D11CreateDevice(
            nullptr,
            driverType,
            nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            featureLevels.data(),
            static_cast<UINT>(featureLevels.size()),
            D3D11_SDK_VERSION,
            &d3dDevice_,
            &selected,
            &d3dContext_);
    };

    auto hr = forceWarp_ ? create(D3D_DRIVER_TYPE_WARP)
                         : create(D3D_DRIVER_TYPE_HARDWARE);
    usingWarp_ = forceWarp_;
    if (FAILED(hr) && !forceWarp_) {
        d3dDevice_.Reset();
        d3dContext_.Reset();
        hr = create(D3D_DRIVER_TYPE_WARP);
        usingWarp_ = SUCCEEDED(hr);
    }
    if (FAILED(hr)) {
        setError(error, hr, formatHresult(L"D3D11CreateDevice", hr));
        return false;
    }

    ComPtr<IDXGIDevice> dxgiDevice;
    hr = d3dDevice_.As(&dxgiDevice);
    if (FAILED(hr)) {
        setError(error, hr, formatHresult(L"ID3D11Device::As(IDXGIDevice)", hr));
        return false;
    }

    D2D1_FACTORY_OPTIONS factoryOptions{};
#if defined(_DEBUG)
    factoryOptions.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
    hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        __uuidof(ID2D1Factory1),
        &factoryOptions,
        reinterpret_cast<void**>(d2dFactory_.GetAddressOf()));
    if (FAILED(hr)) {
        // The D2D debug layer is optional. Retry without it when unavailable.
        factoryOptions.debugLevel = D2D1_DEBUG_LEVEL_NONE;
        hr = D2D1CreateFactory(
            D2D1_FACTORY_TYPE_SINGLE_THREADED,
            __uuidof(ID2D1Factory1),
            &factoryOptions,
            reinterpret_cast<void**>(d2dFactory_.ReleaseAndGetAddressOf()));
    }
    if (FAILED(hr)) {
        setError(error, hr, formatHresult(L"D2D1CreateFactory", hr));
        return false;
    }
    hr = d2dFactory_->CreateDevice(dxgiDevice.Get(), &d2dDevice_);
    if (FAILED(hr)) {
        setError(error, hr, formatHresult(L"ID2D1Factory1::CreateDevice", hr));
        return false;
    }
    hr = d2dDevice_->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
        &d2dContext_);
    if (FAILED(hr)) {
        setError(error, hr, formatHresult(L"ID2D1Device::CreateDeviceContext", hr));
        return false;
    }
    return true;
}

bool Win32Graphics::createSwapChain(GraphicsError& error) {
    ComPtr<IDXGIDevice> dxgiDevice;
    auto hr = d3dDevice_.As(&dxgiDevice);
    if (FAILED(hr)) {
        setError(error, hr, formatHresult(L"ID3D11Device::As(IDXGIDevice)", hr));
        return false;
    }
    ComPtr<IDXGIAdapter> adapter;
    hr = dxgiDevice->GetAdapter(&adapter);
    if (FAILED(hr)) {
        setError(error, hr, formatHresult(L"IDXGIDevice::GetAdapter", hr));
        return false;
    }
    ComPtr<IDXGIFactory2> factory;
    hr = adapter->GetParent(IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        setError(error, hr, formatHresult(L"IDXGIAdapter::GetParent(IDXGIFactory2)", hr));
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 description{};
    description.Width = pixelWidth_;
    description.Height = pixelHeight_;
    description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    description.SampleDesc.Count = 1U;
    description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.BufferCount = 2U;
    description.Scaling = DXGI_SCALING_STRETCH;
    description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    description.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    hr = factory->CreateSwapChainForHwnd(
        d3dDevice_.Get(),
        window_,
        &description,
        nullptr,
        nullptr,
        &swapChain_);
    if (FAILED(hr)) {
        setError(error, hr, formatHresult(L"IDXGIFactory2::CreateSwapChainForHwnd", hr));
        return false;
    }
    static_cast<void>(factory->MakeWindowAssociation(window_, DXGI_MWA_NO_ALT_ENTER));
    return true;
}

bool Win32Graphics::createTarget(GraphicsError& error) {
    if (!swapChain_ || !d2dContext_ || pixelWidth_ == 0U || pixelHeight_ == 0U) {
        setError(error, E_FAIL, L"Cannot create Direct2D target without a swap chain and size");
        return false;
    }
    ComPtr<IDXGISurface> surface;
    const auto hr = swapChain_->GetBuffer(0U, IID_PPV_ARGS(&surface));
    if (FAILED(hr)) {
        setError(error, hr, formatHresult(L"IDXGISwapChain1::GetBuffer", hr));
        return false;
    }
    const auto properties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(
            DXGI_FORMAT_B8G8R8A8_UNORM,
            D2D1_ALPHA_MODE_PREMULTIPLIED),
        dpi_,
        dpi_);
    const auto createResult = d2dContext_->CreateBitmapFromDxgiSurface(
        surface.Get(), &properties, &targetBitmap_);
    if (FAILED(createResult)) {
        setError(error, createResult,
            formatHresult(L"ID2D1DeviceContext::CreateBitmapFromDxgiSurface", createResult));
        return false;
    }
    d2dContext_->SetTarget(targetBitmap_.Get());
    return true;
}

void Win32Graphics::releaseTarget() noexcept {
    if (d2dContext_) {
        d2dContext_->SetTarget(nullptr);
    }
    targetBitmap_.Reset();
}

void Win32Graphics::incrementGeneration() noexcept {
    ++graphicsGeneration_;
    if (graphicsGeneration_ == 0U) {
        graphicsGeneration_ = 1U;
    }
}

} // namespace avemotion::preview
