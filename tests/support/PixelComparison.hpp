#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace avemotion::testsupport {

struct PixelBGRA final {
    std::uint8_t b = 0;
    std::uint8_t g = 0;
    std::uint8_t r = 0;
    std::uint8_t a = 0;

    [[nodiscard]] friend constexpr bool operator==(
        const PixelBGRA&,
        const PixelBGRA&) noexcept = default;
};

[[nodiscard]] inline PixelBGRA pixelFromArgb32(
    std::uint32_t argb) noexcept {
    return {
        static_cast<std::uint8_t>(argb & 0xFFU),
        static_cast<std::uint8_t>((argb >> 8U) & 0xFFU),
        static_cast<std::uint8_t>((argb >> 16U) & 0xFFU),
        static_cast<std::uint8_t>((argb >> 24U) & 0xFFU),
    };
}

struct PixelBounds final {
    bool valid = false;
    std::size_t left = 0;
    std::size_t top = 0;
    std::size_t right = 0;  // inclusive
    std::size_t bottom = 0; // inclusive
};

struct PixelComparisonMetrics final {
    std::size_t width = 0;
    std::size_t height = 0;
    std::size_t pixelCount = 0;
    std::size_t referenceActivePixels = 0;
    std::size_t actualActivePixels = 0;
    std::size_t activeUnionPixels = 0;
    std::size_t activeIntersectionPixels = 0;
    std::size_t pixelsOver16 = 0;
    std::size_t pixelsOver32 = 0;
    std::size_t pixelsOver64 = 0;
    std::uint64_t referenceAlphaSum = 0;
    std::uint64_t actualAlphaSum = 0;
    std::uint8_t maxChannelDifference = 0;
    double activeIoU = 1.0;
    double meanAbsoluteDifferenceAll = 0.0;
    double meanAbsoluteDifferenceActive = 0.0;
    double rootMeanSquareDifferenceAll = 0.0;
    double alphaRelativeError = 0.0;
    double largeDifferenceFraction = 0.0;
    std::size_t maximumBoundsDelta = 0;
    PixelBounds referenceBounds;
    PixelBounds actualBounds;
};

struct PixelComparisonPolicy final {
    // Direct2D and rlottie's CPU scan converter are expected to differ at
    // antialiased edges. These limits are deliberately strict enough to catch
    // missing geometry, channel swaps, double DPI scaling and lost opacity,
    // while not requiring bit-identical rasterization from different engines.
    double minimumActiveIoU = 0.78;
    double maximumAlphaRelativeError = 0.12;
    double maximumMeanAbsoluteDifferenceAll = 12.0;
    double maximumMeanAbsoluteDifferenceActive = 30.0;
    double maximumLargeDifferenceFraction = 0.18;
    std::size_t maximumBoundsDelta = 4;
};

struct PixelComparisonDecision final {
    bool passed = true;
    std::vector<std::string> failures;
};

namespace detail {

inline void includePixel(
    PixelBounds& bounds,
    std::size_t x,
    std::size_t y) noexcept {
    if (!bounds.valid) {
        bounds = {true, x, y, x, y};
        return;
    }
    bounds.left = std::min(bounds.left, x);
    bounds.top = std::min(bounds.top, y);
    bounds.right = std::max(bounds.right, x);
    bounds.bottom = std::max(bounds.bottom, y);
}

[[nodiscard]] inline std::size_t absoluteDifference(
    std::size_t left,
    std::size_t right) noexcept {
    return left > right ? left - right : right - left;
}

[[nodiscard]] inline std::size_t boundsDelta(
    const PixelBounds& left,
    const PixelBounds& right,
    std::size_t fallback) noexcept {
    if (!left.valid && !right.valid) {
        return 0;
    }
    if (left.valid != right.valid) {
        return fallback;
    }
    return std::max({
        absoluteDifference(left.left, right.left),
        absoluteDifference(left.top, right.top),
        absoluteDifference(left.right, right.right),
        absoluteDifference(left.bottom, right.bottom),
    });
}

} // namespace detail

[[nodiscard]] inline PixelComparisonMetrics comparePixels(
    const std::vector<PixelBGRA>& reference,
    const std::vector<PixelBGRA>& actual,
    std::size_t width,
    std::size_t height,
    std::uint8_t activeAlphaThreshold = 8U) {
    PixelComparisonMetrics metrics;
    metrics.width = width;
    metrics.height = height;
    metrics.pixelCount = width * height;
    if (reference.size() != metrics.pixelCount
        || actual.size() != metrics.pixelCount
        || metrics.pixelCount == 0U) {
        metrics.activeIoU = 0.0;
        metrics.meanAbsoluteDifferenceAll =
            std::numeric_limits<double>::infinity();
        metrics.meanAbsoluteDifferenceActive =
            std::numeric_limits<double>::infinity();
        metrics.rootMeanSquareDifferenceAll =
            std::numeric_limits<double>::infinity();
        metrics.alphaRelativeError =
            std::numeric_limits<double>::infinity();
        metrics.largeDifferenceFraction = 1.0;
        metrics.maximumBoundsDelta = std::max(width, height);
        return metrics;
    }

    std::uint64_t absoluteSumAll = 0;
    std::uint64_t squaredSumAll = 0;
    std::uint64_t absoluteSumActive = 0;

    for (std::size_t index = 0; index < metrics.pixelCount; ++index) {
        const auto& expected = reference[index];
        const auto& observed = actual[index];
        const bool referenceActive = expected.a > activeAlphaThreshold;
        const bool actualActive = observed.a > activeAlphaThreshold;
        const bool unionActive = referenceActive || actualActive;
        const bool intersectionActive = referenceActive && actualActive;
        const auto x = index % width;
        const auto y = index / width;

        metrics.referenceAlphaSum += expected.a;
        metrics.actualAlphaSum += observed.a;
        if (referenceActive) {
            ++metrics.referenceActivePixels;
            detail::includePixel(metrics.referenceBounds, x, y);
        }
        if (actualActive) {
            ++metrics.actualActivePixels;
            detail::includePixel(metrics.actualBounds, x, y);
        }
        if (unionActive) {
            ++metrics.activeUnionPixels;
        }
        if (intersectionActive) {
            ++metrics.activeIntersectionPixels;
        }

        const std::array<unsigned, 4> differences{{
            static_cast<unsigned>(std::abs(
                static_cast<int>(expected.b) - static_cast<int>(observed.b))),
            static_cast<unsigned>(std::abs(
                static_cast<int>(expected.g) - static_cast<int>(observed.g))),
            static_cast<unsigned>(std::abs(
                static_cast<int>(expected.r) - static_cast<int>(observed.r))),
            static_cast<unsigned>(std::abs(
                static_cast<int>(expected.a) - static_cast<int>(observed.a))),
        }};
        const auto maximum = *std::max_element(
            differences.begin(), differences.end());
        metrics.maxChannelDifference = static_cast<std::uint8_t>(
            std::max<unsigned>(metrics.maxChannelDifference, maximum));
        if (maximum > 16U) ++metrics.pixelsOver16;
        if (maximum > 32U) ++metrics.pixelsOver32;
        if (maximum > 64U) ++metrics.pixelsOver64;
        for (const auto difference : differences) {
            absoluteSumAll += difference;
            squaredSumAll += static_cast<std::uint64_t>(difference)
                * static_cast<std::uint64_t>(difference);
            if (unionActive) {
                absoluteSumActive += difference;
            }
        }
    }

    const auto channelCount = static_cast<double>(metrics.pixelCount) * 4.0;
    metrics.meanAbsoluteDifferenceAll =
        static_cast<double>(absoluteSumAll) / channelCount;
    metrics.rootMeanSquareDifferenceAll = std::sqrt(
        static_cast<double>(squaredSumAll) / channelCount);
    metrics.meanAbsoluteDifferenceActive = metrics.activeUnionPixels == 0U
        ? 0.0
        : static_cast<double>(absoluteSumActive)
            / (static_cast<double>(metrics.activeUnionPixels) * 4.0);
    metrics.activeIoU = metrics.activeUnionPixels == 0U
        ? 1.0
        : static_cast<double>(metrics.activeIntersectionPixels)
            / static_cast<double>(metrics.activeUnionPixels);
    metrics.alphaRelativeError = metrics.referenceAlphaSum == 0U
        ? (metrics.actualAlphaSum == 0U ? 0.0 : 1.0)
        : std::abs(
            static_cast<double>(metrics.actualAlphaSum)
            - static_cast<double>(metrics.referenceAlphaSum))
            / static_cast<double>(metrics.referenceAlphaSum);
    metrics.largeDifferenceFraction = metrics.activeUnionPixels == 0U
        ? 0.0
        : static_cast<double>(metrics.pixelsOver64)
            / static_cast<double>(metrics.activeUnionPixels);
    metrics.maximumBoundsDelta = detail::boundsDelta(
        metrics.referenceBounds,
        metrics.actualBounds,
        std::max(width, height));
    return metrics;
}

[[nodiscard]] inline PixelComparisonDecision evaluateComparison(
    const PixelComparisonMetrics& metrics,
    const PixelComparisonPolicy& policy = {}) {
    PixelComparisonDecision decision;
    const auto reject = [&decision](std::string message) {
        decision.passed = false;
        decision.failures.push_back(std::move(message));
    };
    if (!std::isfinite(metrics.meanAbsoluteDifferenceAll)
        || !std::isfinite(metrics.meanAbsoluteDifferenceActive)) {
        reject("image dimensions or storage do not match");
        return decision;
    }
    if (metrics.activeIoU < policy.minimumActiveIoU) {
        reject("active-pixel intersection-over-union is below policy");
    }
    if (metrics.alphaRelativeError > policy.maximumAlphaRelativeError) {
        reject("total alpha differs beyond policy");
    }
    if (metrics.meanAbsoluteDifferenceAll
        > policy.maximumMeanAbsoluteDifferenceAll) {
        reject("full-surface mean absolute difference exceeds policy");
    }
    if (metrics.meanAbsoluteDifferenceActive
        > policy.maximumMeanAbsoluteDifferenceActive) {
        reject("active-region mean absolute difference exceeds policy");
    }
    if (metrics.largeDifferenceFraction
        > policy.maximumLargeDifferenceFraction) {
        reject("too many active pixels differ by more than 64 levels");
    }
    if (metrics.maximumBoundsDelta > policy.maximumBoundsDelta) {
        reject("nontransparent bounds differ beyond policy");
    }
    return decision;
}

} // namespace avemotion::testsupport
