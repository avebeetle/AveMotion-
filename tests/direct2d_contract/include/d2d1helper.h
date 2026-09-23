#pragma once

#include <d2d1_1.h>

namespace D2D1 {

class Matrix3x2F : public D2D1_MATRIX_3X2_F {
public:
    constexpr Matrix3x2F() noexcept = default;

    constexpr Matrix3x2F(
        FLOAT m11Value,
        FLOAT m12Value,
        FLOAT m21Value,
        FLOAT m22Value,
        FLOAT dxValue,
        FLOAT dyValue) noexcept {
        m11 = m11Value;
        m12 = m12Value;
        m21 = m21Value;
        m22 = m22Value;
        dx = dxValue;
        dy = dyValue;
    }

    constexpr explicit Matrix3x2F(
        const D2D1_MATRIX_3X2_F& value) noexcept
        : Matrix3x2F(
            value.m11,
            value.m12,
            value.m21,
            value.m22,
            value.dx,
            value.dy) {
    }

    [[nodiscard]] constexpr Matrix3x2F operator*(
        const Matrix3x2F& other) const noexcept {
        return {
            m11 * other.m11 + m12 * other.m21,
            m11 * other.m12 + m12 * other.m22,
            m21 * other.m11 + m22 * other.m21,
            m21 * other.m12 + m22 * other.m22,
            dx * other.m11 + dy * other.m21 + other.dx,
            dx * other.m12 + dy * other.m22 + other.dy,
        };
    }
};

[[nodiscard]] constexpr D2D1_POINT_2F Point2F(
    FLOAT x = 0.0F,
    FLOAT y = 0.0F) noexcept {
    return {x, y};
}

[[nodiscard]] constexpr D2D1_BEZIER_SEGMENT BezierSegment(
    D2D1_POINT_2F point1,
    D2D1_POINT_2F point2,
    D2D1_POINT_2F point3) noexcept {
    return {point1, point2, point3};
}

class ColorF : public D2D1_COLOR_F {
public:
    enum Enum : std::uint32_t {
        White,
    };

    constexpr explicit ColorF(Enum value) noexcept {
        if (value == White) {
            r = 1.0F;
            g = 1.0F;
            b = 1.0F;
            a = 1.0F;
        }
    }

    constexpr ColorF(
        FLOAT red,
        FLOAT green,
        FLOAT blue,
        FLOAT alpha = 1.0F) noexcept {
        r = red;
        g = green;
        b = blue;
        a = alpha;
    }
};

[[nodiscard]] constexpr D2D1_STROKE_STYLE_PROPERTIES StrokeStyleProperties(
    D2D1_CAP_STYLE startCap = D2D1_CAP_STYLE_FLAT,
    D2D1_CAP_STYLE endCap = D2D1_CAP_STYLE_FLAT,
    D2D1_CAP_STYLE dashCap = D2D1_CAP_STYLE_FLAT,
    D2D1_LINE_JOIN lineJoin = D2D1_LINE_JOIN_MITER,
    FLOAT miterLimit = 10.0F,
    D2D1_DASH_STYLE dashStyle = D2D1_DASH_STYLE_SOLID,
    FLOAT dashOffset = 0.0F) noexcept {
    return {
        startCap,
        endCap,
        dashCap,
        lineJoin,
        miterLimit,
        dashStyle,
        dashOffset,
    };
}

} // namespace D2D1
