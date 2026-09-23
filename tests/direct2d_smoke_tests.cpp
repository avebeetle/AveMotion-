#define NOMINMAX
#include "avemotion/backends/direct2d/Direct2DBackend.hpp"

#include <d2d1_1.h>
#include <d2d1helper.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#ifndef AVEMOTION_D2D_ARTIFACT_DIR
#define AVEMOTION_D2D_ARTIFACT_DIR "."
#endif

namespace {

using Microsoft::WRL::ComPtr;

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

void require(bool condition, const std::string& message) {
    if (!condition) {
        fail(message);
    }
}

void requireHr(HRESULT value, const std::string& operation) {
    if (FAILED(value)) {
        std::ostringstream stream;
        stream << operation << " failed with HRESULT 0x"
               << std::hex << std::uppercase
               << static_cast<std::uint32_t>(value);
        fail(stream.str());
    }
}

struct Pixel final {
    std::uint8_t b = 0;
    std::uint8_t g = 0;
    std::uint8_t r = 0;
    std::uint8_t a = 0;
};

class WarpSurface final {
public:
    static constexpr UINT kWidth = 128U;
    static constexpr UINT kHeight = 64U;

    WarpSurface() {
        create();
    }

    [[nodiscard]] ID2D1DeviceContext* context() const noexcept {
        return d2dContext_.Get();
    }

    void begin() {
        d2dContext_->SetTarget(target_.Get());
        d2dContext_->SetUnitMode(D2D1_UNIT_MODE_PIXELS);
        d2dContext_->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
        d2dContext_->SetTransform(D2D1::Matrix3x2F::Identity());
        d2dContext_->BeginDraw();
        d2dContext_->Clear(D2D1::ColorF(0.0F, 0.0F, 0.0F, 0.0F));
    }

    void end() {
        requireHr(d2dContext_->EndDraw(), "ID2D1DeviceContext::EndDraw");
        d3dContext_->Flush();
    }

    [[nodiscard]] std::vector<Pixel> readPixels() const {
        D3D11_TEXTURE2D_DESC description{};
        texture_->GetDesc(&description);
        description.Usage = D3D11_USAGE_STAGING;
        description.BindFlags = 0U;
        description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        description.MiscFlags = 0U;

        ComPtr<ID3D11Texture2D> staging;
        requireHr(
            d3dDevice_->CreateTexture2D(&description, nullptr, &staging),
            "ID3D11Device::CreateTexture2D(staging)");
        d3dContext_->CopyResource(staging.Get(), texture_.Get());

        D3D11_MAPPED_SUBRESOURCE mapped{};
        requireHr(
            d3dContext_->Map(
                staging.Get(), 0U, D3D11_MAP_READ, 0U, &mapped),
            "ID3D11DeviceContext::Map");

        std::vector<Pixel> pixels(
            static_cast<std::size_t>(kWidth)
            * static_cast<std::size_t>(kHeight));
        for (UINT y = 0; y < kHeight; ++y) {
            const auto* row = static_cast<const std::uint8_t*>(mapped.pData)
                + static_cast<std::size_t>(mapped.RowPitch) * y;
            for (UINT x = 0; x < kWidth; ++x) {
                const auto offset = static_cast<std::size_t>(x) * 4U;
                pixels[static_cast<std::size_t>(y) * kWidth + x] = {
                    row[offset + 0U],
                    row[offset + 1U],
                    row[offset + 2U],
                    row[offset + 3U],
                };
            }
        }
        d3dContext_->Unmap(staging.Get(), 0U);
        return pixels;
    }

private:
    void create() {
        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        requireHr(
            D3D11CreateDevice(
                nullptr,
                D3D_DRIVER_TYPE_WARP,
                nullptr,
                flags,
                nullptr,
                0U,
                D3D11_SDK_VERSION,
                &d3dDevice_,
                nullptr,
                &d3dContext_),
            "D3D11CreateDevice(WARP)");

        ComPtr<IDXGIDevice> dxgiDevice;
        requireHr(d3dDevice_.As(&dxgiDevice), "ID3D11Device::As(IDXGIDevice)");

        D2D1_FACTORY_OPTIONS factoryOptions{};
        requireHr(
            D2D1CreateFactory(
                D2D1_FACTORY_TYPE_SINGLE_THREADED,
                __uuidof(ID2D1Factory1),
                &factoryOptions,
                reinterpret_cast<void**>(d2dFactory_.GetAddressOf())),
            "D2D1CreateFactory");
        requireHr(
            d2dFactory_->CreateDevice(dxgiDevice.Get(), &d2dDevice_),
            "ID2D1Factory1::CreateDevice");
        requireHr(
            d2dDevice_->CreateDeviceContext(
                D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &d2dContext_),
            "ID2D1Device::CreateDeviceContext");

        D3D11_TEXTURE2D_DESC textureDescription{};
        textureDescription.Width = kWidth;
        textureDescription.Height = kHeight;
        textureDescription.MipLevels = 1U;
        textureDescription.ArraySize = 1U;
        textureDescription.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        textureDescription.SampleDesc.Count = 1U;
        textureDescription.Usage = D3D11_USAGE_DEFAULT;
        textureDescription.BindFlags = D3D11_BIND_RENDER_TARGET
            | D3D11_BIND_SHADER_RESOURCE;
        requireHr(
            d3dDevice_->CreateTexture2D(
                &textureDescription, nullptr, &texture_),
            "ID3D11Device::CreateTexture2D(target)");

        ComPtr<IDXGISurface> surface;
        requireHr(texture_.As(&surface), "ID3D11Texture2D::As(IDXGISurface)");
        const auto properties = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(
                DXGI_FORMAT_B8G8R8A8_UNORM,
                D2D1_ALPHA_MODE_PREMULTIPLIED),
            96.0F,
            96.0F);
        requireHr(
            d2dContext_->CreateBitmapFromDxgiSurface(
                surface.Get(), &properties, &target_),
            "ID2D1DeviceContext::CreateBitmapFromDxgiSurface");
    }

    ComPtr<ID3D11Device> d3dDevice_;
    ComPtr<ID3D11DeviceContext> d3dContext_;
    ComPtr<ID2D1Factory1> d2dFactory_;
    ComPtr<ID2D1Device> d2dDevice_;
    ComPtr<ID2D1DeviceContext> d2dContext_;
    ComPtr<ID3D11Texture2D> texture_;
    ComPtr<ID2D1Bitmap1> target_;
};

avemotion::runtime::EvaluatedPath rectanglePath() {
    using namespace avemotion::runtime;
    EvaluatedPath path;
    path.verbs = {
        PathVerb::MoveTo,
        PathVerb::LineTo,
        PathVerb::LineTo,
        PathVerb::LineTo,
        PathVerb::Close,
    };
    path.points = {
        {0.0F, 0.0F},
        {24.0F, 0.0F},
        {24.0F, 24.0F},
        {0.0F, 24.0F},
    };
    path.controlBounds = {true, 0.0F, 0.0F, 24.0F, 24.0F};
    path.hash = 0xAA55AA55ULL;
    return path;
}

avemotion::runtime::EvaluatedDrawItem sourceItem(
    avemotion::runtime::Color8 color,
    bool stroke) {
    using namespace avemotion::runtime;
    EvaluatedDrawItem item;
    item.localGeometryAvailable = true;
    item.localGeometryStaticCandidate = true;
    item.localPath = rectanglePath();
    item.localPaintAvailable = true;
    item.localPaintStaticCandidate = true;
    item.localPaint.kind = PaintKind::Solid;
    item.localPaint.solid = color;
    item.localStroke.enabled = stroke;
    item.localStroke.width = stroke ? 4.0F : 0.0F;
    item.localStroke.miterLimit = 4.0F;
    item.localStroke.cap = LineCap::Round;
    item.localStroke.join = LineJoin::Bevel;
    item.fillRule = FillRule::Winding;
    return item;
}

avemotion::render::GeometryCacheKey geometryKey() {
    using namespace avemotion::render;
    return {
        .scope = ResourceIdentityScope::Asset,
        .assetHash = 0x1234ULL,
        .assetIdentity = 7ULL,
        .instanceId = 0ULL,
        .instanceIdentity = 0ULL,
        .sourceKey = 0x88ULL,
        .resourceId = {},
        .contentHash = 0xABCDULL,
        .revision = 0ULL,
    };
}

avemotion::render::PaintCacheKey paintKey(std::uint64_t sourceKey) {
    using namespace avemotion::render;
    return {
        .scope = ResourceIdentityScope::Asset,
        .assetHash = 0x1234ULL,
        .assetIdentity = 7ULL,
        .instanceId = 0ULL,
        .instanceIdentity = 0ULL,
        .sourceKey = sourceKey,
        .resourceId = {},
        .contentHash = sourceKey * 17ULL,
        .revision = 0ULL,
    };
}

avemotion::render::MotionDrawItem planItem(
    std::uint32_t sourceIndex,
    float x,
    float opacity,
    std::uint32_t features,
    std::uint64_t paintSourceKey) {
    avemotion::render::MotionDrawItem item;
    item.sourceDrawItemIndex = sourceIndex;
    item.geometry = geometryKey();
    item.paint = paintKey(paintSourceKey);
    item.presentationTransform.dx = x;
    item.presentationTransform.dy = 8.0F;
    item.effectiveOpacity = opacity;
    item.featureBits = features;
    return item;
}

avemotion::render::MotionRenderPlan makePlan() {
    using namespace avemotion;
    auto scene = std::make_shared<runtime::EvaluatedScene>();
    scene->drawItems.push_back(sourceItem(
        runtime::Color8{255U, 0U, 0U, 255U}, false));
    scene->drawItems.push_back(sourceItem(
        runtime::Color8{0U, 255U, 0U, 255U}, true));

    render::MotionRenderPlan plan;
    plan.sourceScene = std::move(scene);
    plan.drawItems.push_back(planItem(
        0U, 8.0F, 1.0F,
        render::RenderFeatureSolidPaint, 0x101ULL));
    plan.drawItems.push_back(planItem(
        0U, 48.0F, 0.5F,
        render::RenderFeatureSolidPaint, 0x101ULL));
    plan.drawItems.push_back(planItem(
        1U, 88.0F, 1.0F,
        render::RenderFeatureSolidPaint | render::RenderFeatureStroke,
        0x202ULL));
    return plan;
}

const Pixel& pixelAt(
    const std::vector<Pixel>& pixels,
    UINT x,
    UINT y) {
    return pixels[static_cast<std::size_t>(y) * WarpSurface::kWidth + x];
}

void validatePixels(const std::vector<Pixel>& pixels) {
    const auto outside = pixelAt(pixels, 2U, 2U);
    require(outside.r == 0U && outside.g == 0U
                && outside.b == 0U && outside.a == 0U,
            "transparent background pixel is incorrect");

    const auto solid = pixelAt(pixels, 20U, 20U);
    require(solid.r >= 250U && solid.g <= 2U
                && solid.b <= 2U && solid.a >= 250U,
            "solid red fill did not reach the target");

    const auto half = pixelAt(pixels, 60U, 20U);
    require(half.r >= 120U && half.r <= 136U
                && half.g <= 2U && half.b <= 2U
                && half.a >= 120U && half.a <= 136U,
            "half-opacity premultiplied fill is incorrect");

    const auto stroke = pixelAt(pixels, 88U, 20U);
    require(stroke.g >= 250U && stroke.r <= 2U
                && stroke.b <= 2U && stroke.a >= 250U,
            "green stroke did not reach the target");

    const auto strokeCenter = pixelAt(pixels, 100U, 20U);
    require(strokeCenter.r == 0U && strokeCenter.g == 0U
                && strokeCenter.b == 0U && strokeCenter.a == 0U,
            "stroke unexpectedly filled its interior");
}

void writePpm(
    const std::filesystem::path& path,
    const std::vector<Pixel>& pixels) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    require(static_cast<bool>(output), "unable to create Direct2D artifact");
    output << "P6\n" << WarpSurface::kWidth << ' ' << WarpSurface::kHeight
           << "\n255\n";
    for (const auto& pixel : pixels) {
        const auto unpremultiply = [alpha = pixel.a](std::uint8_t value) {
            if (alpha == 0U) {
                return std::uint8_t{0U};
            }
            const auto expanded = static_cast<unsigned>(value) * 255U
                / static_cast<unsigned>(alpha);
            return static_cast<std::uint8_t>(std::min(expanded, 255U));
        };
        const std::uint8_t rgb[] = {
            unpremultiply(pixel.r),
            unpremultiply(pixel.g),
            unpremultiply(pixel.b),
        };
        output.write(
            reinterpret_cast<const char*>(rgb),
            static_cast<std::streamsize>(sizeof(rgb)));
    }
    require(static_cast<bool>(output), "unable to write Direct2D artifact");
}

void drawAndValidate(
    avemotion::backends::direct2d::Backend& backend,
    WarpSurface& surface,
    const avemotion::render::MotionRenderPlan& plan,
    std::uint64_t domain,
    std::uint64_t generation) {
    surface.begin();
    const auto result = backend.draw(plan, {
        .deviceContext = surface.context(),
        .graphicsDomainId = domain,
        .graphicsGeneration = generation,
    });
    require(static_cast<bool>(result), "AveMotion Direct2D draw failed");
    require(result.itemsDrawn == 3U && result.itemsSkipped == 0U,
            "AveMotion Direct2D draw reported incorrect item counts");
    surface.end();
    validatePixels(surface.readPixels());
}

} // namespace

int main() {
    using namespace avemotion;

    const auto plan = makePlan();
    backends::direct2d::Backend backend;

    WarpSurface firstSurface;
    drawAndValidate(backend, firstSurface, plan, 1ULL, 1ULL);
    auto diagnostics = backend.diagnostics();
    require(diagnostics.geometryCacheMisses == 1U
                && diagnostics.geometryCacheHits == 2U
                && diagnostics.geometryResourcesCreated == 1U,
            "first native draw did not reuse shared geometry");
    require(diagnostics.strokeStylesCreated == 1U,
            "first native draw did not create the stroke style");

    drawAndValidate(backend, firstSurface, plan, 1ULL, 1ULL);
    diagnostics = backend.diagnostics();
    require(diagnostics.geometryCacheMisses == 1U
                && diagnostics.geometryCacheHits == 5U,
            "native repaint recreated unchanged geometry");
    require(diagnostics.strokeStyleCacheHits == 1U,
            "native repaint did not reuse the stroke style");

    // A second WARP/D2D device proves that backend resources are scoped to a
    // graphics domain rather than leaked across incompatible native devices.
    WarpSurface secondSurface;
    drawAndValidate(backend, secondSurface, plan, 2ULL, 1ULL);
    diagnostics = backend.diagnostics();
    require(diagnostics.resourceDomainResets >= 2U,
            "native graphics-domain transition did not invalidate resources");
    require(diagnostics.geometryResourcesCreated == 2U,
            "second native graphics domain did not rebuild geometry");

    backend.invalidateGraphicsDomain(2ULL, 1ULL);
    drawAndValidate(backend, secondSurface, plan, 2ULL, 1ULL);
    diagnostics = backend.diagnostics();
    require(diagnostics.geometryResourcesCreated == 3U,
            "explicit native domain invalidation did not rebuild geometry");

    const auto pixels = secondSurface.readPixels();
    const auto artifact = std::filesystem::path(AVEMOTION_D2D_ARTIFACT_DIR)
        / "avemotion-direct2d-warp.ppm";
    writePpm(artifact, pixels);

    std::cout << "AveMotion Direct2D WARP smoke test passed\n"
              << "artifact=" << artifact.string() << '\n'
              << "drawCalls=" << diagnostics.drawCalls << '\n'
              << "geometryCacheHits=" << diagnostics.geometryCacheHits << '\n'
              << "geometryCacheMisses=" << diagnostics.geometryCacheMisses << '\n'
              << "resourceDomainResets=" << diagnostics.resourceDomainResets
              << '\n';
    return EXIT_SUCCESS;
}
