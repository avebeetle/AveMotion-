#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace avemotion::testsupport {

enum class CaptureAssetLocation {
    Corpus,
    Fixture,
};

struct CaptureAssetSpec final {
    std::string_view id;
    std::string_view fileName;
    CaptureAssetLocation location = CaptureAssetLocation::Fixture;
};

struct CaptureSample final {
    std::string_view id;
    double normalizedPosition = 0.0;
};

struct CaptureProfile final {
    std::string_view id;
    std::size_t logicalWidth = 0;
    std::size_t logicalHeight = 0;
    std::size_t pixelWidth = 0;
    std::size_t pixelHeight = 0;
    float dpiX = 96.0F;
    float dpiY = 96.0F;
};

inline constexpr std::array<CaptureAssetSpec, 5> kDirect2DCaptureAssets{{
    {"animated-shape", "dynamic_path_test.json", CaptureAssetLocation::Corpus},
    {"primitives", "primitive_geometry.json", CaptureAssetLocation::Fixture},
    {"polystar", "polystar_polygon_geometry.json", CaptureAssetLocation::Fixture},
    {"trim-path", "trim_path_geometry.json", CaptureAssetLocation::Fixture},
    {"repeater-content", "repeater_content_group.json", CaptureAssetLocation::Fixture},
}};

inline constexpr std::array<CaptureSample, 3> kDirect2DCaptureSamples{{
    {"p000", 0.0},
    {"p050", 0.5},
    {"p100", 1.0},
}};

// The first three profiles exercise size and aspect-ratio changes at 96 DPI.
// The last two keep a 128x128 logical viewport while scaling it through the
// native Direct2D DPI pipeline to 192x192 and 256x256 physical pixels.
inline constexpr std::array<CaptureProfile, 5> kDirect2DCaptureProfiles{{
    {"square64-dpi96", 64, 64, 64, 64, 96.0F, 96.0F},
    {"square128-dpi96", 128, 128, 128, 128, 96.0F, 96.0F},
    {"wide192x128-dpi96", 192, 128, 192, 128, 96.0F, 96.0F},
    {"square128-dpi144", 128, 128, 192, 192, 144.0F, 144.0F},
    {"square128-dpi192", 128, 128, 256, 256, 192.0F, 192.0F},
}};

inline constexpr std::size_t kDirect2DCaptureCaseCount =
    kDirect2DCaptureAssets.size()
    * kDirect2DCaptureSamples.size()
    * kDirect2DCaptureProfiles.size();

} // namespace avemotion::testsupport
