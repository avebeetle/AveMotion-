#include "PrimitivePathGenerator.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {

[[noreturn]] void fail(const char* message) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

void require(bool condition, const char* message) {
    if (!condition) fail(message);
}

bool near(float left, float right, float tolerance = 1.0e-4F) {
    return std::fabs(left - right) <= tolerance;
}

std::size_t countVerb(
    const avemotion::render::detail::PrimitivePath& path,
    avemotion::runtime::PathVerb verb) {
    std::size_t count = 0U;
    for (const auto value : path.verbSpan()) {
        if (value == verb) ++count;
    }
    return count;
}

} // namespace

int main() {
    using avemotion::model::MotionVec2Value;
    using avemotion::model::SourcePathDirection;
    using avemotion::render::detail::generatePolygonPath;
    using avemotion::render::detail::generatePolystarPath;
    using avemotion::runtime::PathVerb;

    const auto sharp = generatePolystarPath(
        MotionVec2Value{0.0F, 0.0F},
        5.0F,
        5.0F,
        10.0F,
        0.0F,
        0.0F,
        90.0F,
        SourcePathDirection::Clockwise);
    require(sharp.valid, "sharp star must be valid");
    require(!sharp.rounded, "sharp star must not be rounded");
    require(countVerb(sharp, PathVerb::MoveTo) == 1U,
            "sharp star must start with one MoveTo");
    require(countVerb(sharp, PathVerb::LineTo) >= 10U,
            "sharp star must emit line segments");
    require(countVerb(sharp, PathVerb::CubicTo) == 0U,
            "sharp star must not emit cubic segments");
    require(countVerb(sharp, PathVerb::Close) == 1U,
            "sharp star must close its contour");
    require(sharp.pointCount > 0U, "sharp star must contain points");
    // The pinned Telegram LOTPolystarItem applies the authored rotation twice.
    // A 90-degree authored rotation therefore maps the initial top point to
    // the bottom of the local star.
    require(near(sharp.points[0].x, 0.0F)
                && near(sharp.points[0].y, 10.0F),
            "Telegram double-rotation semantics changed");

    const auto rounded = generatePolystarPath(
        MotionVec2Value{12.0F, -3.0F},
        5.5F,
        4.0F,
        11.0F,
        35.0F,
        60.0F,
        -23.0F,
        SourcePathDirection::CounterClockwise);
    require(rounded.valid, "fractional rounded star must be valid");
    require(rounded.rounded, "rounded star flag changed");
    require(countVerb(rounded, PathVerb::CubicTo) == 12U,
            "fractional 5.5-point star topology changed");
    require(countVerb(rounded, PathVerb::Close) == 1U,
            "rounded star must close its contour");

    const auto polygon = generatePolygonPath(
        MotionVec2Value{0.0F, 0.0F},
        5.8F,
        20.0F,
        0.0F,
        0.0F,
        SourcePathDirection::Clockwise);
    require(polygon.valid, "fractional polygon input must be accepted");
    require(!polygon.rounded, "sharp polygon must not be rounded");
    require(countVerb(polygon, PathVerb::LineTo) >= 5U,
            "Telegram polygon must floor fractional point count");
    require(countVerb(polygon, PathVerb::CubicTo) == 0U,
            "sharp polygon must not emit cubic segments");

    const auto integerPolygon = generatePolygonPath(
        MotionVec2Value{0.0F, 0.0F},
        5.0F,
        20.0F,
        0.0F,
        0.0F,
        SourcePathDirection::Clockwise);
    require(integerPolygon.valid, "integer polygon must be valid");
    require(integerPolygon.verbCount == polygon.verbCount
                && integerPolygon.pointCount == polygon.pointCount,
            "fractional polygon must floor authored point count");
    for (std::size_t index = 0U; index < polygon.verbCount; ++index) {
        require(integerPolygon.verbs[index] == polygon.verbs[index],
                "floored polygon verb stream changed");
    }
    for (std::size_t index = 0U; index < polygon.pointCount; ++index) {
        require(near(integerPolygon.points[index].x, polygon.points[index].x)
                    && near(integerPolygon.points[index].y, polygon.points[index].y),
                "floored polygon point stream changed");
    }

    const auto roundedPolygon = generatePolygonPath(
        MotionVec2Value{0.0F, 0.0F},
        7.0F,
        20.0F,
        55.0F,
        31.0F,
        SourcePathDirection::CounterClockwise);
    require(roundedPolygon.valid, "rounded polygon must be valid");
    require(roundedPolygon.rounded, "rounded polygon flag changed");
    require(countVerb(roundedPolygon, PathVerb::CubicTo) == 7U,
            "rounded seven-sided polygon topology changed");

    const auto zeroPoints = generatePolystarPath(
        {}, 0.0F, 1.0F, 2.0F, 0.0F, 0.0F, 0.0F,
        SourcePathDirection::Clockwise);
    require(!zeroPoints.valid, "zero-point star must fail closed");

    const auto tooManyPoints = generatePolystarPath(
        {}, 257.0F, 1.0F, 2.0F, 0.0F, 0.0F, 0.0F,
        SourcePathDirection::Clockwise);
    require(!tooManyPoints.valid, "oversized star must fail closed");

    const auto oversizedPolygon = generatePolygonPath(
        {}, 257.0F, 2.0F, 0.0F, 0.0F,
        SourcePathDirection::Clockwise);
    require(!oversizedPolygon.valid, "oversized polygon must fail closed");

    const auto nonFinite = generatePolygonPath(
        {}, std::numeric_limits<float>::quiet_NaN(), 2.0F, 0.0F, 0.0F,
        SourcePathDirection::Clockwise);
    require(!nonFinite.valid, "non-finite polygon must fail closed");

    const auto nonFiniteRadius = generatePolystarPath(
        {}, 5.0F, 1.0F, std::numeric_limits<float>::infinity(),
        0.0F, 0.0F, 0.0F, SourcePathDirection::Clockwise);
    require(!nonFiniteRadius.valid, "non-finite radius must fail closed");

    std::cout << "AveMotion polystar geometry tests passed\n";
    return EXIT_SUCCESS;
}
