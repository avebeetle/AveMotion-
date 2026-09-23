#include "TrimPathGenerator.hpp"

#include <array>
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

bool near(float left, float right, float tolerance = 1.0e-3F) {
    return std::fabs(left - right) <= tolerance;
}

} // namespace

int main() {
    using avemotion::model::MotionVec2Value;
    using avemotion::render::detail::normalizeTrimSegment;
    using avemotion::render::detail::trimPath;
    using avemotion::render::detail::trimPathCollection;
    using avemotion::render::detail::TrimPathCollectionMode;
    using avemotion::render::detail::TrimPathInput;
    using avemotion::render::detail::TrimmedPathCollection;
    using avemotion::runtime::PathVerb;

    const std::array lineVerbs{PathVerb::MoveTo, PathVerb::LineTo};
    const std::array linePoints{
        MotionVec2Value{0.0F, 0.0F},
        MotionVec2Value{100.0F, 0.0F},
    };

    const auto middleSegment = normalizeTrimSegment(25.0F, 75.0F, 0.0F);
    require(middleSegment.valid && !middleSegment.empty && !middleSegment.full,
            "middle trim normalization failed");
    const auto middle = trimPath(lineVerbs, linePoints, middleSegment);
    require(middle.valid && !middle.empty && !middle.full,
            "middle line trim failed");
    require(middle.verbs.size() == 2U && middle.points.size() == 2U,
            "middle line trim topology changed");
    require(middle.verbs[0] == PathVerb::MoveTo
                && middle.verbs[1] == PathVerb::LineTo,
            "middle line trim verbs changed");
    require(near(middle.points[0].x, 25.0F)
                && near(middle.points[1].x, 75.0F),
            "middle line trim points changed");

    // Telegram only produces a wrapped segment when offset crossing moves
    // one endpoint across 0/1; authored start > end alone is sorted.
    const auto wrappedSegment = normalizeTrimSegment(50.0F, 90.0F, 90.0F);
    require(wrappedSegment.valid && wrappedSegment.start > wrappedSegment.end,
            "wrapped trim normalization changed");
    const auto wrapped = trimPath(lineVerbs, linePoints, wrappedSegment);
    require(wrapped.valid && wrapped.verbs.size() == 4U
                && wrapped.points.size() == 4U,
            "wrapped line trim topology changed");
    require(wrapped.verbs[0] == PathVerb::MoveTo
                && wrapped.verbs[1] == PathVerb::LineTo
                && wrapped.verbs[2] == PathVerb::MoveTo
                && wrapped.verbs[3] == PathVerb::LineTo,
            "wrapped trim contour order changed");
    require(near(wrapped.points[0].x, 0.0F)
                && near(wrapped.points[1].x, 15.0F)
                && near(wrapped.points[2].x, 75.0F)
                && near(wrapped.points[3].x, 100.0F),
            "wrapped trim points changed");

    const auto emptySegment = normalizeTrimSegment(50.0F, 50.0F, 0.0F);
    const auto empty = trimPath(lineVerbs, linePoints, emptySegment);
    require(empty.valid && empty.empty && empty.verbs.empty()
                && empty.points.empty(),
            "empty trim changed");

    const auto fullSegment = normalizeTrimSegment(0.0F, 100.0F, 123.0F);
    const auto full = trimPath(lineVerbs, linePoints, fullSegment);
    require(full.valid && full.full && full.verbs.size() == lineVerbs.size()
                && full.points.size() == linePoints.size(),
            "full trim changed");

    const auto positiveOffset = normalizeTrimSegment(50.0F, 90.0F, 90.0F);
    require(positiveOffset.valid && positiveOffset.start > positiveOffset.end
                && near(positiveOffset.start, 0.75F)
                && near(positiveOffset.end, 0.15F),
            "positive offset wrap changed");
    const auto negativeOffset = normalizeTrimSegment(10.0F, 50.0F, -90.0F);
    require(negativeOffset.valid && negativeOffset.start > negativeOffset.end
                && near(negativeOffset.start, 0.85F)
                && near(negativeOffset.end, 0.25F),
            "negative offset wrap changed");

    const std::array cubicVerbs{PathVerb::MoveTo, PathVerb::CubicTo};
    const std::array cubicPoints{
        MotionVec2Value{0.0F, 0.0F},
        MotionVec2Value{30.0F, 0.0F},
        MotionVec2Value{70.0F, 100.0F},
        MotionVec2Value{100.0F, 100.0F},
    };
    const auto cubic = trimPath(cubicVerbs, cubicPoints, middleSegment);
    require(cubic.valid && !cubic.empty && cubic.splitCount >= 1U,
            "cubic trim failed");
    require(cubic.verbs.size() == 2U && cubic.points.size() == 4U,
            "cubic trim topology changed");
    for (const auto point : cubic.points) {
        require(std::isfinite(point.x) && std::isfinite(point.y),
                "cubic trim produced non-finite points");
    }
    const auto cubicAgain = trimPath(cubicVerbs, cubicPoints, middleSegment);
    require(cubicAgain.verbs == cubic.verbs && cubicAgain.points == cubic.points,
            "cubic trim is history dependent");

    const std::array secondLineVerbs{PathVerb::MoveTo, PathVerb::LineTo};
    const std::array secondLinePoints{
        MotionVec2Value{200.0F, 0.0F},
        MotionVec2Value{500.0F, 0.0F},
    };
    const std::array multiInputs{
        TrimPathInput{lineVerbs, linePoints},
        TrimPathInput{secondLineVerbs, secondLinePoints},
    };
    TrimmedPathCollection collection;
    collection.prepare(2U, 16U, 16U, 16U, 16U);
    const auto preparedGeneration = collection.storageGeneration;

    require(trimPathCollection(
                multiInputs,
                middleSegment,
                TrimPathCollectionMode::Simultaneous,
                collection),
            "simultaneous multi-path trim failed");
    require(collection.paths.size() == 2U && collection.splitCount >= 2U,
            "simultaneous multi-path result changed");
    const auto simultaneousFirst = collection.pathPoints(0U);
    const auto simultaneousSecond = collection.pathPoints(1U);
    require(simultaneousFirst.size() == 2U
                && near(simultaneousFirst[0].x, 25.0F)
                && near(simultaneousFirst[1].x, 75.0F),
            "simultaneous first path changed");
    require(simultaneousSecond.size() == 2U
                && near(simultaneousSecond[0].x, 275.0F)
                && near(simultaneousSecond[1].x, 425.0F),
            "simultaneous second path changed");

    const auto individualSegment = normalizeTrimSegment(12.5F, 62.5F, 0.0F);
    require(trimPathCollection(
                multiInputs,
                individualSegment,
                TrimPathCollectionMode::Individual,
                collection),
            "individual multi-path trim failed");
    const auto individualFirst = collection.pathPoints(0U);
    const auto individualSecond = collection.pathPoints(1U);
    require(individualFirst.size() == 2U
                && near(individualFirst[0].x, 50.0F)
                && near(individualFirst[1].x, 100.0F),
            "individual first path distribution changed");
    require(individualSecond.size() == 2U
                && near(individualSecond[0].x, 200.0F)
                && near(individualSecond[1].x, 350.0F),
            "individual second path distribution changed");

    require(trimPathCollection(
                multiInputs,
                wrappedSegment,
                TrimPathCollectionMode::Individual,
                collection),
            "wrapped individual compatibility path failed");
    require(collection.individualWrappedNoOp
                && collection.pathPoints(0U).size() == linePoints.size()
                && collection.pathPoints(1U).size() == secondLinePoints.size(),
            "wrapped Individual must preserve Telegram no-op behaviour");

    require(trimPathCollection(
                multiInputs,
                individualSegment,
                TrimPathCollectionMode::Individual,
                collection),
            "repeated retained multi-path trim failed");
    require(collection.storageGeneration == preparedGeneration,
            "prepared trim collection allocated during steady-state reuse");

    const auto invalid = normalizeTrimSegment(
        std::numeric_limits<float>::quiet_NaN(), 50.0F, 0.0F);
    require(!invalid.valid, "non-finite trim input must fail closed");

    std::cout << "AveMotion trim path tests passed\n";
    return EXIT_SUCCESS;
}
