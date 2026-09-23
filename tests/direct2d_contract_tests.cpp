#include "avemotion/backends/direct2d/Direct2DBackend.hpp"

#include <d2d1_1.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

void require(bool condition, const std::string& message) {
    if (!condition) {
        fail(message);
    }
}

bool near(float left, float right, float epsilon = 1.0e-6F) {
    return std::fabs(left - right) <= epsilon;
}

class RefCounted {
public:
    ULONG addRef() noexcept { return ++references_; }

    template <typename T>
    ULONG release(T* self) noexcept {
        const auto remaining = --references_;
        if (remaining == 0U) {
            delete self;
        }
        return remaining;
    }

protected:
    ~RefCounted() = default;

private:
    ULONG references_ = 1U;
};

enum class FakeVerb : std::uint8_t {
    Move,
    Line,
    Cubic,
    Close,
};

class FakePathGeometry;

class FakeGeometrySink final : public ID2D1GeometrySink, private RefCounted {
public:
    explicit FakeGeometrySink(FakePathGeometry* geometry) noexcept;
    ~FakeGeometrySink() override;

    ULONG AddRef() noexcept override { return addRef(); }
    ULONG Release() noexcept override { return release(this); }

    void SetFillMode(D2D1_FILL_MODE fillMode) noexcept override;
    void BeginFigure(
        D2D1_POINT_2F startPoint,
        D2D1_FIGURE_BEGIN figureBegin) noexcept override;
    void AddLine(D2D1_POINT_2F point) noexcept override;
    void AddBezier(const D2D1_BEZIER_SEGMENT& bezier) noexcept override;
    void EndFigure(D2D1_FIGURE_END figureEnd) noexcept override;
    HRESULT Close() noexcept override;

private:
    FakePathGeometry* geometry_ = nullptr;
    bool figureOpen_ = false;
};

class FakePathGeometry final : public ID2D1PathGeometry, private RefCounted {
public:
    ULONG AddRef() noexcept override { return addRef(); }
    ULONG Release() noexcept override { return release(this); }

    HRESULT Open(ID2D1GeometrySink** geometrySink) noexcept override {
        if (geometrySink == nullptr || opened_) {
            return E_INVALIDARG;
        }
        opened_ = true;
        *geometrySink = new FakeGeometrySink(this);
        return S_OK;
    }

    D2D1_FILL_MODE fillMode = D2D1_FILL_MODE_WINDING;
    std::vector<FakeVerb> verbs;
    std::vector<D2D1_POINT_2F> points;
    bool opened_ = false;
    bool closed_ = false;
};

FakeGeometrySink::FakeGeometrySink(FakePathGeometry* geometry) noexcept
    : geometry_(geometry) {
    geometry_->AddRef();
}

FakeGeometrySink::~FakeGeometrySink() {
    geometry_->Release();
}

void FakeGeometrySink::SetFillMode(D2D1_FILL_MODE fillMode) noexcept {
    geometry_->fillMode = fillMode;
}

void FakeGeometrySink::BeginFigure(
    D2D1_POINT_2F startPoint,
    D2D1_FIGURE_BEGIN /*figureBegin*/) noexcept {
    geometry_->verbs.push_back(FakeVerb::Move);
    geometry_->points.push_back(startPoint);
    figureOpen_ = true;
}

void FakeGeometrySink::AddLine(D2D1_POINT_2F point) noexcept {
    geometry_->verbs.push_back(FakeVerb::Line);
    geometry_->points.push_back(point);
}

void FakeGeometrySink::AddBezier(const D2D1_BEZIER_SEGMENT& bezier) noexcept {
    geometry_->verbs.push_back(FakeVerb::Cubic);
    geometry_->points.push_back(bezier.point1);
    geometry_->points.push_back(bezier.point2);
    geometry_->points.push_back(bezier.point3);
}

void FakeGeometrySink::EndFigure(D2D1_FIGURE_END figureEnd) noexcept {
    if (figureEnd == D2D1_FIGURE_END_CLOSED) {
        geometry_->verbs.push_back(FakeVerb::Close);
    }
    figureOpen_ = false;
}

HRESULT FakeGeometrySink::Close() noexcept {
    if (figureOpen_) {
        return E_FAIL;
    }
    geometry_->closed_ = true;
    return S_OK;
}

class FakeStrokeStyle final : public ID2D1StrokeStyle, private RefCounted {
public:
    explicit FakeStrokeStyle(D2D1_STROKE_STYLE_PROPERTIES value) noexcept
        : properties(value) {
    }

    ULONG AddRef() noexcept override { return addRef(); }
    ULONG Release() noexcept override { return release(this); }

    D2D1_STROKE_STYLE_PROPERTIES properties;
};

class FakeSolidColorBrush final
    : public ID2D1SolidColorBrush,
      private RefCounted {
public:
    explicit FakeSolidColorBrush(D2D1_COLOR_F value) noexcept
        : color(value) {
    }

    ULONG AddRef() noexcept override { return addRef(); }
    ULONG Release() noexcept override { return release(this); }

    void SetColor(const D2D1_COLOR_F& value) noexcept override { color = value; }
    void SetOpacity(FLOAT value) noexcept override { opacity = value; }

    D2D1_COLOR_F color;
    FLOAT opacity = 1.0F;
};

class FakeFactory final : public ID2D1Factory, private RefCounted {
public:
    ULONG AddRef() noexcept override { return addRef(); }
    ULONG Release() noexcept override { return release(this); }

    HRESULT CreatePathGeometry(ID2D1PathGeometry** geometry) noexcept override {
        if (geometry == nullptr) {
            return E_INVALIDARG;
        }
        ++pathGeometryCreations;
        *geometry = new FakePathGeometry;
        return S_OK;
    }

    HRESULT CreateStrokeStyle(
        const D2D1_STROKE_STYLE_PROPERTIES& properties,
        const FLOAT* /*dashes*/,
        UINT32 dashCount,
        ID2D1StrokeStyle** strokeStyle) noexcept override {
        if (strokeStyle == nullptr || dashCount != 0U) {
            return E_INVALIDARG;
        }
        ++strokeStyleCreations;
        *strokeStyle = new FakeStrokeStyle(properties);
        return S_OK;
    }

    std::uint64_t pathGeometryCreations = 0;
    std::uint64_t strokeStyleCreations = 0;
};

struct FakeDrawRecord final {
    bool stroke = false;
    FakePathGeometry* geometry = nullptr;
    D2D1_COLOR_F color;
    FLOAT opacity = 1.0F;
    FLOAT strokeWidth = 0.0F;
    D2D1_STROKE_STYLE_PROPERTIES strokeProperties;
    D2D1_MATRIX_3X2_F transform;
};

class FakeDeviceContext final
    : public ID2D1DeviceContext,
      private RefCounted {
public:
    explicit FakeDeviceContext(FakeFactory* factory) noexcept
        : factory_(factory) {
        factory_->AddRef();
    }

    ~FakeDeviceContext() override {
        factory_->Release();
    }

    ULONG AddRef() noexcept override { return addRef(); }
    ULONG Release() noexcept override { return release(this); }

    void GetFactory(ID2D1Factory** factory) noexcept override {
        if (factory == nullptr) {
            return;
        }
        factory_->AddRef();
        *factory = factory_;
    }

    HRESULT CreateSolidColorBrush(
        const D2D1_COLOR_F& color,
        ID2D1SolidColorBrush** brush) noexcept override {
        if (brush == nullptr) {
            return E_INVALIDARG;
        }
        ++solidBrushCreations;
        *brush = new FakeSolidColorBrush(color);
        return S_OK;
    }

    void GetTransform(D2D1_MATRIX_3X2_F* transform) const noexcept override {
        if (transform != nullptr) {
            *transform = transform_;
        }
    }

    void SetTransform(const D2D1_MATRIX_3X2_F& transform) noexcept override {
        transform_ = transform;
    }

    void FillGeometry(
        ID2D1PathGeometry* geometry,
        ID2D1SolidColorBrush* brush) noexcept override {
        record(false, geometry, brush, 0.0F, nullptr);
    }

    void DrawGeometry(
        ID2D1PathGeometry* geometry,
        ID2D1SolidColorBrush* brush,
        FLOAT strokeWidth,
        ID2D1StrokeStyle* strokeStyle) noexcept override {
        record(true, geometry, brush, strokeWidth, strokeStyle);
    }

    [[nodiscard]] const D2D1_MATRIX_3X2_F& transform() const noexcept {
        return transform_;
    }

    std::uint64_t solidBrushCreations = 0;
    std::vector<FakeDrawRecord> records;

private:
    void record(
        bool stroke,
        ID2D1PathGeometry* geometry,
        ID2D1SolidColorBrush* brush,
        FLOAT strokeWidth,
        ID2D1StrokeStyle* strokeStyle) noexcept {
        auto* fakeGeometry = dynamic_cast<FakePathGeometry*>(geometry);
        auto* fakeBrush = dynamic_cast<FakeSolidColorBrush*>(brush);
        auto* fakeStroke = dynamic_cast<FakeStrokeStyle*>(strokeStyle);
        FakeDrawRecord value;
        value.stroke = stroke;
        value.geometry = fakeGeometry;
        value.color = fakeBrush != nullptr ? fakeBrush->color : D2D1_COLOR_F{};
        value.opacity = fakeBrush != nullptr ? fakeBrush->opacity : 0.0F;
        value.strokeWidth = strokeWidth;
        value.strokeProperties = fakeStroke != nullptr
            ? fakeStroke->properties
            : D2D1_STROKE_STYLE_PROPERTIES{};
        value.transform = transform_;
        records.push_back(value);
    }

    FakeFactory* factory_ = nullptr;
    D2D1_MATRIX_3X2_F transform_{};
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
    item.localStroke.width = stroke ? 3.0F : 0.0F;
    item.localStroke.miterLimit = 4.0F;
    item.localStroke.cap = LineCap::Round;
    item.localStroke.join = LineJoin::Bevel;
    item.fillRule = FillRule::Winding;
    return item;
}

avemotion::render::GeometryCacheKey sharedGeometryKey() {
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
    float y,
    float opacity,
    std::uint32_t features,
    std::uint64_t paintSourceKey) {
    avemotion::render::MotionDrawItem item;
    item.sourceDrawItemIndex = sourceIndex;
    item.geometry = sharedGeometryKey();
    item.paint = paintKey(paintSourceKey);
    item.presentationTransform.dx = x;
    item.presentationTransform.dy = y;
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
        0U, 10.0F, 12.0F, 1.0F,
        render::RenderFeatureSolidPaint, 0x101ULL));
    plan.drawItems.push_back(planItem(
        0U, 50.0F, 12.0F, 0.5F,
        render::RenderFeatureSolidPaint, 0x101ULL));
    plan.drawItems.push_back(planItem(
        1U, 90.0F, 12.0F, 1.0F,
        render::RenderFeatureSolidPaint | render::RenderFeatureStroke,
        0x202ULL));
    return plan;
}

bool sameMatrix(
    const D2D1_MATRIX_3X2_F& left,
    const D2D1_MATRIX_3X2_F& right) {
    return near(left.m11, right.m11)
        && near(left.m12, right.m12)
        && near(left.m21, right.m21)
        && near(left.m22, right.m22)
        && near(left.dx, right.dx)
        && near(left.dy, right.dy);
}

} // namespace

int main() {
    using namespace avemotion;

    auto* factory = new FakeFactory;
    auto* context = new FakeDeviceContext(factory);
    const D2D1_MATRIX_3X2_F hostTransform{
        1.0F, 0.0F, 0.0F, 1.0F, 2.0F, 3.0F};
    context->SetTransform(hostTransform);

    backends::direct2d::Backend backend;
    const backends::direct2d::RenderSession session{
        .deviceContext = context,
        .graphicsDomainId = 42ULL,
        .graphicsGeneration = 1ULL,
    };
    const auto plan = makePlan();

    const auto first = backend.draw(plan, session);
    require(static_cast<bool>(first), "first Direct2D contract draw failed");
    require(first.itemsDrawn == 3U && first.itemsSkipped == 0U,
            "first draw reported incorrect item counts");
    require(context->records.size() == 3U,
            "fake context did not receive all draw calls");
    require(factory->pathGeometryCreations == 1U,
            "shared geometry was not reused inside the first draw");
    require(factory->strokeStyleCreations == 1U,
            "stroke style was not created exactly once");
    require(context->solidBrushCreations == 1U,
            "solid brush was not retained per device context");
    require(context->records[0].geometry == context->records[1].geometry
                && context->records[1].geometry == context->records[2].geometry,
            "fill and stroke applications did not share native geometry");
    require(context->records[0].geometry != nullptr
                && context->records[0].geometry->closed_
                && context->records[0].geometry->verbs.size() == 5U
                && context->records[0].geometry->points.size() == 4U,
            "path geometry command stream was not translated correctly");
    require(!context->records[0].stroke && !context->records[1].stroke
                && context->records[2].stroke,
            "fill/stroke dispatch is incorrect");
    require(near(context->records[0].color.r, 1.0F)
                && near(context->records[0].color.g, 0.0F)
                && near(context->records[0].opacity, 1.0F),
            "solid fill brush state is incorrect");
    require(near(context->records[1].opacity, 0.5F),
            "per-copy opacity was not applied");
    require(near(context->records[2].color.g, 1.0F)
                && near(context->records[2].strokeWidth, 3.0F)
                && context->records[2].strokeProperties.lineJoin
                    == D2D1_LINE_JOIN_BEVEL
                && context->records[2].strokeProperties.startCap
                    == D2D1_CAP_STYLE_ROUND,
            "stroke state is incorrect");
    require(near(context->records[0].transform.dx, 12.0F)
                && near(context->records[0].transform.dy, 15.0F)
                && near(context->records[1].transform.dx, 52.0F)
                && near(context->records[2].transform.dx, 92.0F),
            "geometry/presentation/host transform composition is incorrect");
    require(sameMatrix(context->transform(), hostTransform),
            "backend did not restore the host transform");

    auto diagnostics = backend.diagnostics();
    require(diagnostics.drawCalls == 1U
                && diagnostics.drawItemsVisited == 3U
                && diagnostics.solidItemsDrawn == 3U,
            "draw diagnostics are incorrect");
    require(diagnostics.geometryCacheMisses == 1U
                && diagnostics.geometryCacheHits == 2U
                && diagnostics.geometryResourcesCreated == 1U,
            "geometry cache diagnostics are incorrect");
    require(diagnostics.strokeStyleCacheMisses == 1U
                && diagnostics.strokeStylesCreated == 1U,
            "stroke-style cache diagnostics are incorrect");
    require(diagnostics.resourceDomainResets == 1U,
            "initial graphics-domain adoption performed redundant resets");

    context->records.clear();
    const auto second = backend.draw(plan, session);
    require(static_cast<bool>(second), "second Direct2D contract draw failed");
    diagnostics = backend.diagnostics();
    require(factory->pathGeometryCreations == 1U,
            "repaint recreated unchanged native geometry");
    require(factory->strokeStyleCreations == 1U,
            "repaint recreated unchanged stroke style");
    require(diagnostics.geometryCacheHits == 5U
                && diagnostics.geometryCacheMisses == 1U,
            "repaint did not hit the native geometry cache");
    require(diagnostics.strokeStyleCacheHits == 1U,
            "repaint did not hit the stroke-style cache");

    backend.invalidateGraphicsDomain(42ULL, 1ULL);
    context->records.clear();
    const auto afterInvalidation = backend.draw(plan, session);
    require(static_cast<bool>(afterInvalidation),
            "draw after graphics-domain invalidation failed");
    diagnostics = backend.diagnostics();
    require(factory->pathGeometryCreations == 2U,
            "graphics-domain invalidation did not rebuild native geometry");
    require(factory->strokeStyleCreations == 2U,
            "graphics-domain invalidation did not rebuild stroke style");
    require(context->solidBrushCreations == 2U,
            "graphics-domain invalidation did not rebuild device brush");
    require(diagnostics.resourceDomainResets == 2U,
            "explicit invalidation caused an unexpected reset count");

    auto invalidPlan = plan;
    invalidPlan.drawItems[1].sourceDrawItemIndex = 999U;
    context->SetTransform(hostTransform);
    const auto invalidResult = backend.draw(invalidPlan, session);
    require(!invalidResult
                && invalidResult.error.code
                    == backends::direct2d::BackendErrorCode::InvalidPlan,
            "invalid source index was not rejected");
    require(sameMatrix(context->transform(), hostTransform),
            "host transform was not restored on an error path");

    auto unsupportedPlan = plan;
    unsupportedPlan.drawItems.resize(1U);
    unsupportedPlan.drawItems[0].featureBits
        = render::RenderFeatureSolidPaint | render::RenderFeatureGradientPaint;
    const auto unsupported = backend.draw(unsupportedPlan, session);
    require(static_cast<bool>(unsupported)
                && unsupported.itemsDrawn == 0U
                && unsupported.itemsSkipped == 1U,
            "unsupported feature was not skipped fail-closed");

    context->Release();
    factory->Release();

    std::cout << "AveMotion Direct2D contract tests passed\n";
    return EXIT_SUCCESS;
}
