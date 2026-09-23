#include "support/PixelComparison.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

void require(bool condition, const std::string& message) {
    if (!condition) fail(message);
}

} // namespace

int main() {
    using namespace avemotion::testsupport;

    constexpr std::size_t width = 8U;
    constexpr std::size_t height = 8U;
    std::vector<PixelBGRA> reference(width * height);
    for (std::size_t y = 2U; y < 6U; ++y) {
        for (std::size_t x = 2U; x < 6U; ++x) {
            reference[y * width + x] = {0U, 0U, 255U, 255U};
        }
    }

    const auto identical = comparePixels(reference, reference, width, height);
    const auto identicalDecision = evaluateComparison(identical);
    require(identicalDecision.passed, "identical images were rejected");
    require(identical.activeIoU == 1.0
            && identical.meanAbsoluteDifferenceAll == 0.0
            && identical.maximumBoundsDelta == 0U,
            "identical-image metrics are incorrect");

    auto edgeVariant = reference;
    for (std::size_t y = 2U; y < 6U; ++y) {
        edgeVariant[y * width + 2U] = {0U, 0U, 240U, 240U};
    }
    const auto edgeMetrics = comparePixels(
        reference, edgeVariant, width, height);
    require(evaluateComparison(edgeMetrics).passed,
            "small antialias-like edge differences were rejected");

    auto missing = std::vector<PixelBGRA>(width * height);
    const auto missingMetrics = comparePixels(reference, missing, width, height);
    require(!evaluateComparison(missingMetrics).passed,
            "missing geometry was accepted");

    auto channelSwap = reference;
    for (auto& pixel : channelSwap) {
        if (pixel.a != 0U) {
            pixel.b = pixel.r;
            pixel.r = 0U;
        }
    }
    const auto channelMetrics = comparePixels(
        reference, channelSwap, width, height);
    require(!evaluateComparison(channelMetrics).passed,
            "a complete red/blue channel swap was accepted");

    auto shifted = std::vector<PixelBGRA>(width * height);
    for (std::size_t y = 2U; y < 6U; ++y) {
        for (std::size_t x = 3U; x < 7U; ++x) {
            shifted[y * width + x] = {0U, 0U, 255U, 255U};
        }
    }
    const auto shiftedMetrics = comparePixels(reference, shifted, width, height);
    require(shiftedMetrics.maximumBoundsDelta == 1U,
            "bounds delta did not detect the one-pixel shift");

    require(pixelFromArgb32(0x80402010U)
                == PixelBGRA{0x10U, 0x20U, 0x40U, 0x80U},
            "ARGB32 to BGRA conversion is incorrect");

    std::cout << "AveMotion Direct2D capture metric tests passed\n";
    return EXIT_SUCCESS;
}
