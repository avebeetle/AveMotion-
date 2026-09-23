#pragma once

#include "avemotion/model/AssetModel.hpp"
#include "avemotion/validation/AssetValidator.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace avemotion::lab {

enum class CandidateFamily : std::uint8_t {
    NativeSubsetCompletion,
    NestedCompositionTime,
    Gradients,
    DashedStrokes,
    MasksAndMattes,
    BlendModes,
    TimelineRemapping,
    LayerAndMediaSupport,
};

struct FeatureObservation final {
    validation::FeatureKind feature = validation::FeatureKind::Unknown;
    validation::FeatureSupport support = validation::FeatureSupport::Unsupported;
    std::size_t occurrences = 0;
    std::vector<std::string> assetKeys;
};

struct FeaturePriority final {
    CandidateFamily family = CandidateFamily::NativeSubsetCompletion;
    std::size_t occurrences = 0;
    std::size_t assets = 0;
    std::size_t blockedAssets = 0;
    std::uint32_t effort = 0;
    std::int64_t score = 0;
    std::string rationale;
};

[[nodiscard]] std::vector<FeaturePriority> rankFeaturePriorities(
    std::span<const FeatureObservation> observations);
[[nodiscard]] std::string_view toString(CandidateFamily value) noexcept;
[[nodiscard]] std::string stableAssetAlias(
    std::uint64_t sourceHash,
    std::size_t collisionOrdinal = 0U);
[[nodiscard]] std::size_t estimateCanonicalModelBytes(
    const model::MotionAssetModel& asset) noexcept;

} // namespace avemotion::lab
