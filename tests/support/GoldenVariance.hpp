#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace avemotion::testsupport {

enum class GoldenManifestKind {
    Scene,
    Plan,
};

struct GoldenRowComparison final {
    bool matches = false;
    bool acceptedKnownVariance = false;
    std::string message;
};

[[nodiscard]] inline std::string_view stripTrailingCarriageReturn(
    std::string_view value) noexcept {
    return !value.empty() && value.back() == '\r'
        ? value.substr(0U, value.size() - 1U)
        : value;
}

[[nodiscard]] inline std::vector<std::string_view> splitTsv(
    std::string_view row) {
    row = stripTrailingCarriageReturn(row);
    std::vector<std::string_view> fields;
    std::size_t begin = 0U;
    for (;;) {
        const auto end = row.find('\t', begin);
        if (end == std::string_view::npos) {
            fields.push_back(row.substr(begin));
            return fields;
        }
        fields.push_back(row.substr(begin, end - begin));
        begin = end + 1U;
    }
}

[[nodiscard]] inline bool isKnownTelegramMsvcPolystarEndpoint(
    const std::vector<std::string_view>& fields) noexcept {
    // Telegram's pinned CPU reference path evaluates this last Polystar + Trim
    // sample through CRT/libm-backed float trigonometry and recursive length
    // arithmetic. MSVC and Linux libm produce different least-significant
    // float bits while topology, paint, counts and the native Direct2D capture
    // remain equivalent. Keep the exception tied to one exact asset/sample.
    return fields.size() >= 6U
        && fields[0] == "telegram"
        && fields[1] == "polystar_line_clockwise_trim.json"
        && fields[2] == "8d8cd4aacce0760d"
        && fields[3] == "p100"
        && fields[4] == "149"
        && fields[5] == "128x128";
}

[[nodiscard]] inline GoldenRowComparison compareGoldenRows(
    GoldenManifestKind kind,
    std::string_view expectedRow,
    std::string_view actualRow,
    bool allowKnownMsvcFloatVariance) {
    expectedRow = stripTrailingCarriageReturn(expectedRow);
    actualRow = stripTrailingCarriageReturn(actualRow);
    if (expectedRow == actualRow) {
        return {true, false, {}};
    }

    const auto expected = splitTsv(expectedRow);
    const auto actual = splitTsv(actualRow);
    if (expected.size() != actual.size()) {
        return {
            false,
            false,
            "field count differs: expected " + std::to_string(expected.size())
                + ", got " + std::to_string(actual.size())};
    }

    if (!allowKnownMsvcFloatVariance) {
        return {false, false, "row differs and MSVC variance is disabled"};
    }

    if (!isKnownTelegramMsvcPolystarEndpoint(expected)
        || !isKnownTelegramMsvcPolystarEndpoint(actual)) {
        return {false, false, "row differs outside the pinned MSVC endpoint policy"};
    }

    struct KnownFieldVariance final {
        std::size_t field = 0U;
        std::string_view linuxGolden;
        std::string_view msvcObserved;
    };
    constexpr std::array<KnownFieldVariance, 2U> kSceneFloatFingerprints{{
        {6U, "0cca4a4d4395c248", "d16b938d33790784"},
        {8U, "c52135375f4b4fd9", "4c0fb2275b3068ab"},
    }};
    constexpr std::array<KnownFieldVariance, 3U> kPlanFloatFingerprints{{
        {6U, "4e7835d97cd1de98", "43cc77e5cb56f0e1"},
        {8U, "f392889bad747440", "4992a725d8c4bd2c"},
        {10U, "83bafa6dc131d21b", "38cbdf9a188f397b"},
    }};

    const auto approvedPair = [&](std::size_t index) noexcept {
        if (kind == GoldenManifestKind::Scene) {
            for (const auto& variance : kSceneFloatFingerprints) {
                if (variance.field == index
                    && expected[index] == variance.linuxGolden
                    && actual[index] == variance.msvcObserved) {
                    return true;
                }
            }
        } else {
            for (const auto& variance : kPlanFloatFingerprints) {
                if (variance.field == index
                    && expected[index] == variance.linuxGolden
                    && actual[index] == variance.msvcObserved) {
                    return true;
                }
            }
        }
        return false;
    };

    bool observedAllowedDifference = false;
    for (std::size_t index = 0U; index < expected.size(); ++index) {
        if (expected[index] == actual[index]) continue;
        if (!approvedPair(index)) {
            return {
                false,
                false,
                "field " + std::to_string(index)
                    + " differs outside the exact captured MSVC fingerprint set"};
        }
        observedAllowedDifference = true;
    }

    if (!observedAllowedDifference) {
        return {false, false, "row differs without an approved fingerprint difference"};
    }
    return {
        true,
        true,
        "accepted pinned Telegram rlottie MSVC endpoint float variance"};
}

} // namespace avemotion::testsupport
