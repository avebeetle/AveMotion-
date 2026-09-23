#include "CorpusAnalysis.hpp"

#include "avemotion/core/Hash.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <utility>

namespace avemotion::lab {
namespace {

struct FamilyPolicy final {
    CandidateFamily family;
    std::uint32_t effort;
    std::string_view rationale;
};

[[nodiscard]] constexpr FamilyPolicy policyFor(
    validation::FeatureKind feature) noexcept {
    using validation::FeatureKind;
    switch (feature) {
    case FeatureKind::TrimPath:
    case FeatureKind::Repeater:
        return {CandidateFamily::NativeSubsetCompletion, 5U,
                "finish the already-native Trim/Repeater subsets"};
    case FeatureKind::Precomposition:
        return {CandidateFamily::NestedCompositionTime, 8U,
                "remove nested-composition and local-time fallback"};
    case FeatureKind::GradientFill:
    case FeatureKind::GradientStroke:
        return {CandidateFamily::Gradients, 5U,
                "add evaluated gradients and Direct2D gradient resources"};
    case FeatureKind::DashedStroke:
        return {CandidateFamily::DashedStrokes, 4U,
                "complete stroke support with dash patterns and offset"};
    case FeatureKind::Mask:
    case FeatureKind::Matte:
        return {CandidateFamily::MasksAndMattes, 12U,
                "add bounded offscreen composition for masks and mattes"};
    case FeatureKind::BlendMode:
        return {CandidateFamily::BlendModes, 8U,
                "add explicit blend-mode composition"};
    case FeatureKind::TimeStretch:
    case FeatureKind::TimeRemap:
        return {CandidateFamily::TimelineRemapping, 8U,
                "add deterministic local timeline remapping"};
    case FeatureKind::SolidLayer:
    case FeatureKind::ImageLayer:
    case FeatureKind::TextLayer:
        return {CandidateFamily::LayerAndMediaSupport, 10U,
                "support non-shape authored layer resources"};
    default:
        return {CandidateFamily::NativeSubsetCompletion, 0U, {}};
    }
}

[[nodiscard]] constexpr std::int64_t supportWeight(
    validation::FeatureSupport support) noexcept {
    using validation::FeatureSupport;
    switch (support) {
    case FeatureSupport::Native: return 0;
    case FeatureSupport::NativeSubset: return 100;
    case FeatureSupport::ReferenceFallback: return 200;
    case FeatureSupport::Unsupported: return 400;
    }
    return 0;
}

[[nodiscard]] std::size_t saturatingAdd(
    std::size_t left,
    std::size_t right) noexcept {
    const auto maximum = std::numeric_limits<std::size_t>::max();
    return left > maximum - right ? maximum : left + right;
}

template <typename T>
[[nodiscard]] std::size_t vectorBytes(const std::vector<T>& values) noexcept {
    if (values.size() > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
        return std::numeric_limits<std::size_t>::max();
    }
    return values.size() * sizeof(T);
}

} // namespace

std::vector<FeaturePriority> rankFeaturePriorities(
    std::span<const FeatureObservation> observations) {
    struct Aggregate final {
        std::size_t occurrences = 0;
        std::set<std::string> assets;
        std::set<std::string> blockedAssets;
        std::uint32_t effort = 0;
        std::int64_t strongestSupport = 0;
        std::string rationale;
    };
    std::map<CandidateFamily, Aggregate> aggregates;
    for (const auto& observation : observations) {
        const auto policy = policyFor(observation.feature);
        if (policy.effort == 0U || observation.occurrences == 0U) continue;
        auto& aggregate = aggregates[policy.family];
        aggregate.occurrences = saturatingAdd(
            aggregate.occurrences, observation.occurrences);
        aggregate.assets.insert(
            observation.assetKeys.begin(), observation.assetKeys.end());
        if (observation.support != validation::FeatureSupport::Native) {
            aggregate.blockedAssets.insert(
                observation.assetKeys.begin(), observation.assetKeys.end());
        }
        aggregate.effort = policy.effort;
        aggregate.strongestSupport = std::max(
            aggregate.strongestSupport, supportWeight(observation.support));
        aggregate.rationale = std::string{policy.rationale};
    }

    std::vector<FeaturePriority> result;
    result.reserve(aggregates.size());
    for (auto& [family, aggregate] : aggregates) {
        const auto blockedAssets = aggregate.blockedAssets.size();
        const auto assets = aggregate.assets.size();
        const auto score = static_cast<std::int64_t>(blockedAssets) * 1000
            + static_cast<std::int64_t>(aggregate.occurrences) * 25
            + aggregate.strongestSupport
            - static_cast<std::int64_t>(aggregate.effort) * 50;
        result.push_back({family, aggregate.occurrences, assets,
                          blockedAssets, aggregate.effort, score,
                          std::move(aggregate.rationale)});
    }
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
        if (left.score != right.score) return left.score > right.score;
        return toString(left.family) < toString(right.family);
    });
    return result;
}

std::string_view toString(CandidateFamily value) noexcept {
    switch (value) {
    case CandidateFamily::NativeSubsetCompletion: return "native-subset-completion";
    case CandidateFamily::NestedCompositionTime: return "nested-composition-time";
    case CandidateFamily::Gradients: return "gradients";
    case CandidateFamily::DashedStrokes: return "dashed-strokes";
    case CandidateFamily::MasksAndMattes: return "masks-and-mattes";
    case CandidateFamily::BlendModes: return "blend-modes";
    case CandidateFamily::TimelineRemapping: return "timeline-remapping";
    case CandidateFamily::LayerAndMediaSupport: return "layer-and-media-support";
    }
    return "unknown";
}

std::string stableAssetAlias(
    std::uint64_t sourceHash,
    std::size_t collisionOrdinal) {
    std::ostringstream stream;
    stream << "asset-" << core::formatHash(sourceHash);
    if (collisionOrdinal != 0U) stream << '-' << collisionOrdinal;
    return stream.str();
}

std::size_t estimateCanonicalModelBytes(
    const model::MotionAssetModel& asset) noexcept {
    std::size_t total = sizeof(model::MotionAssetModel);
    const auto add = [&total](std::size_t value) noexcept {
        total = saturatingAdd(total, value);
    };
    add(vectorBytes(asset.layers));
    add(vectorBytes(asset.childLayerIds));
    add(vectorBytes(asset.layerNodeIds));
    add(vectorBytes(asset.nodes));
    add(vectorBytes(asset.geometries));
    add(vectorBytes(asset.paints));
    add(vectorBytes(asset.drawOrder));
    add(vectorBytes(asset.clips));
    add(vectorBytes(asset.compositions));
    add(vectorBytes(asset.sourceNodes));
    add(vectorBytes(asset.sourceChildIds));
    add(vectorBytes(asset.sourcePropertyIds));
    add(vectorBytes(asset.properties));
    add(vectorBytes(asset.tracks));
    add(vectorBytes(asset.segments));
    add(vectorBytes(asset.scalarValues));
    add(vectorBytes(asset.vec2Values));
    add(vectorBytes(asset.colorValues));
    add(vectorBytes(asset.matrixValues));
    add(vectorBytes(asset.shapeValues));
    add(vectorBytes(asset.shapePoints));
    add(vectorBytes(asset.gradientValues));
    add(vectorBytes(asset.gradientFloats));
    add(asset.debugName.size());
    for (const auto& value : asset.layers) add(value.debugName.size());
    for (const auto& value : asset.clips) add(value.debugName.size());
    for (const auto& value : asset.compositions) add(value.debugName.size());
    for (const auto& value : asset.sourceNodes) add(value.debugName.size());
    return total;
}

} // namespace avemotion::lab
