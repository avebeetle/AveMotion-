#pragma once

#include <cstdint>

using HRESULT = std::int32_t;
using ULONG = unsigned long;
using UINT32 = std::uint32_t;
using FLOAT = float;

inline constexpr HRESULT S_OK = 0;
inline constexpr HRESULT E_FAIL = static_cast<HRESULT>(0x80004005u);
inline constexpr HRESULT E_INVALIDARG = static_cast<HRESULT>(0x80070057u);
inline constexpr HRESULT D2DERR_RECREATE_TARGET = static_cast<HRESULT>(0x8899000Cu);

#ifndef FAILED
#define FAILED(value) (static_cast<HRESULT>(value) < 0)
#endif
#ifndef SUCCEEDED
#define SUCCEEDED(value) (static_cast<HRESULT>(value) >= 0)
#endif

struct D2D1_POINT_2F final {
    FLOAT x = 0.0F;
    FLOAT y = 0.0F;
};

struct D2D1_BEZIER_SEGMENT final {
    D2D1_POINT_2F point1;
    D2D1_POINT_2F point2;
    D2D1_POINT_2F point3;
};

struct D2D1_MATRIX_3X2_F {
    FLOAT m11 = 1.0F;
    FLOAT m12 = 0.0F;
    FLOAT m21 = 0.0F;
    FLOAT m22 = 1.0F;
    FLOAT dx = 0.0F;
    FLOAT dy = 0.0F;
};

struct D2D1_COLOR_F {
    FLOAT r = 0.0F;
    FLOAT g = 0.0F;
    FLOAT b = 0.0F;
    FLOAT a = 0.0F;
};

enum D2D1_FILL_MODE : std::uint32_t {
    D2D1_FILL_MODE_ALTERNATE = 0,
    D2D1_FILL_MODE_WINDING = 1,
};

enum D2D1_FIGURE_BEGIN : std::uint32_t {
    D2D1_FIGURE_BEGIN_FILLED = 0,
    D2D1_FIGURE_BEGIN_HOLLOW = 1,
};

enum D2D1_FIGURE_END : std::uint32_t {
    D2D1_FIGURE_END_OPEN = 0,
    D2D1_FIGURE_END_CLOSED = 1,
};

enum D2D1_CAP_STYLE : std::uint32_t {
    D2D1_CAP_STYLE_FLAT = 0,
    D2D1_CAP_STYLE_SQUARE = 1,
    D2D1_CAP_STYLE_ROUND = 2,
};

enum D2D1_LINE_JOIN : std::uint32_t {
    D2D1_LINE_JOIN_MITER = 0,
    D2D1_LINE_JOIN_BEVEL = 1,
    D2D1_LINE_JOIN_ROUND = 2,
};

enum D2D1_DASH_STYLE : std::uint32_t {
    D2D1_DASH_STYLE_SOLID = 0,
};

struct D2D1_STROKE_STYLE_PROPERTIES final {
    D2D1_CAP_STYLE startCap = D2D1_CAP_STYLE_FLAT;
    D2D1_CAP_STYLE endCap = D2D1_CAP_STYLE_FLAT;
    D2D1_CAP_STYLE dashCap = D2D1_CAP_STYLE_FLAT;
    D2D1_LINE_JOIN lineJoin = D2D1_LINE_JOIN_MITER;
    FLOAT miterLimit = 10.0F;
    D2D1_DASH_STYLE dashStyle = D2D1_DASH_STYLE_SOLID;
    FLOAT dashOffset = 0.0F;
};

struct IUnknown {
    virtual ULONG AddRef() noexcept = 0;
    virtual ULONG Release() noexcept = 0;

protected:
    virtual ~IUnknown() = default;
};

struct ID2D1PathGeometry;
struct ID2D1GeometrySink;
struct ID2D1StrokeStyle;
struct ID2D1SolidColorBrush;

struct ID2D1Factory : IUnknown {
    virtual HRESULT CreatePathGeometry(ID2D1PathGeometry** geometry) noexcept = 0;
    virtual HRESULT CreateStrokeStyle(
        const D2D1_STROKE_STYLE_PROPERTIES& properties,
        const FLOAT* dashes,
        UINT32 dashCount,
        ID2D1StrokeStyle** strokeStyle) noexcept = 0;
};

struct ID2D1GeometrySink : IUnknown {
    virtual void SetFillMode(D2D1_FILL_MODE fillMode) noexcept = 0;
    virtual void BeginFigure(
        D2D1_POINT_2F startPoint,
        D2D1_FIGURE_BEGIN figureBegin) noexcept = 0;
    virtual void AddLine(D2D1_POINT_2F point) noexcept = 0;
    virtual void AddBezier(const D2D1_BEZIER_SEGMENT& bezier) noexcept = 0;
    virtual void EndFigure(D2D1_FIGURE_END figureEnd) noexcept = 0;
    virtual HRESULT Close() noexcept = 0;
};

struct ID2D1PathGeometry : IUnknown {
    virtual HRESULT Open(ID2D1GeometrySink** geometrySink) noexcept = 0;
};

struct ID2D1StrokeStyle : IUnknown {
};

struct ID2D1SolidColorBrush : IUnknown {
    virtual void SetColor(const D2D1_COLOR_F& color) noexcept = 0;
    virtual void SetOpacity(FLOAT opacity) noexcept = 0;
};

struct ID2D1DeviceContext : IUnknown {
    virtual void GetFactory(ID2D1Factory** factory) noexcept = 0;
    virtual HRESULT CreateSolidColorBrush(
        const D2D1_COLOR_F& color,
        ID2D1SolidColorBrush** brush) noexcept = 0;
    virtual void GetTransform(D2D1_MATRIX_3X2_F* transform) const noexcept = 0;
    virtual void SetTransform(const D2D1_MATRIX_3X2_F& transform) noexcept = 0;
    virtual void FillGeometry(
        ID2D1PathGeometry* geometry,
        ID2D1SolidColorBrush* brush) noexcept = 0;
    virtual void DrawGeometry(
        ID2D1PathGeometry* geometry,
        ID2D1SolidColorBrush* brush,
        FLOAT strokeWidth,
        ID2D1StrokeStyle* strokeStyle) noexcept = 0;
};
