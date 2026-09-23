#include "RepeaterTransform.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

void require(bool condition, const std::string& message) {
    if (!condition) fail(message);
}

void requireNear(float actual, float expected, const std::string& message) {
    if (std::fabs(actual - expected) > 3.0e-4F) {
        fail(message + ": actual=" + std::to_string(actual)
             + " expected=" + std::to_string(expected));
    }
}

} // namespace

int main() {
    using avemotion::render::detail::RepeaterTransformSample;
    using avemotion::render::detail::evaluateRepeaterCopy;

    RepeaterTransformSample translated;
    translated.copies = 4.0F;
    translated.position = {28.0F, 0.0F};
    translated.startOpacity = 100.0F;
    translated.endOpacity = 35.0F;

    const auto first = evaluateRepeaterCopy(translated, 0U);
    require(first.valid && first.visible && first.visibleCopies == 4,
            "first translated copy invalid");
    requireNear(first.localMatrix.dx, 0.0F, "first copy dx");
    requireNear(first.opacityMultiplier, 1.0F, "first copy opacity");

    const auto second = evaluateRepeaterCopy(translated, 1U);
    requireNear(second.localMatrix.dx, 28.0F, "second copy dx");
    requireNear(second.opacityMultiplier, 0.8375F, "second copy opacity");

    const auto fourth = evaluateRepeaterCopy(translated, 3U);
    requireNear(fourth.localMatrix.dx, 84.0F, "fourth copy dx");
    requireNear(fourth.opacityMultiplier, 0.5125F, "fourth copy opacity");

    RepeaterTransformSample fractional;
    fractional.copies = 3.75F;
    fractional.offset = 0.5F;
    fractional.position = {24.0F, 2.0F};
    fractional.scale = {92.0F, 108.0F};
    fractional.rotationDegrees = 13.0F;
    fractional.anchor = {3.0F, -2.0F};
    fractional.startOpacity = 85.0F;
    fractional.endOpacity = 20.0F;

    const auto fractionFirst = evaluateRepeaterCopy(fractional, 0U);
    require(fractionFirst.valid && fractionFirst.visibleCopies == 3,
            "fractional repeater copy invalid");
    requireNear(fractionFirst.localMatrix.m11, 0.953001F,
                "fractional copy m11");
    requireNear(fractionFirst.localMatrix.m12, 0.117644F,
                "fractional copy m12");
    requireNear(fractionFirst.localMatrix.m21, -0.108581F,
                "fractional copy m21");
    requireNear(fractionFirst.localMatrix.m22, 1.03255F,
                "fractional copy m22");
    requireNear(fractionFirst.localMatrix.dx, 11.924F,
                "fractional copy dx");
    requireNear(fractionFirst.localMatrix.dy, 0.712F,
                "fractional copy dy");
    requireNear(fractionFirst.opacityMultiplier, 0.85F,
                "fractional copy opacity");

    const auto hidden = evaluateRepeaterCopy(fractional, 3U);
    require(hidden.valid && !hidden.visible,
            "fractional hidden copy classification failed");
    requireNear(hidden.opacityMultiplier, 0.0F, "hidden copy opacity");

    RepeaterTransformSample invalid;
    invalid.copies = 0.0F;
    require(!evaluateRepeaterCopy(invalid, 0U).valid,
            "zero-copy repeater must fail closed");
    invalid.copies = -2.0F;
    require(!evaluateRepeaterCopy(invalid, 0U).valid,
            "negative-copy repeater must fail closed");

    std::cout << "AveMotion Telegram-compatible repeater transform passed\n";
    return EXIT_SUCCESS;
}
