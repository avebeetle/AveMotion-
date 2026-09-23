#include "CorpusAnalysis.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

namespace {
using namespace avemotion;
[[noreturn]] void fail(std::string_view message) {
    std::cerr << "AveMotion corpus analysis test failed: " << message << '\n';
    std::exit(1);
}
void require(bool condition, std::string_view message) {
    if (!condition) fail(message);
}
} // namespace

int main() {
    const std::vector<lab::FeatureObservation> observations{
        {validation::FeatureKind::Precomposition,
         validation::FeatureSupport::ReferenceFallback, 20U,
         {"pre-a", "pre-b", "shared", "pre-d"}},
        {validation::FeatureKind::GradientFill,
         validation::FeatureSupport::ReferenceFallback, 12U,
         {"gradient-a", "gradient-b", "shared"}},
        {validation::FeatureKind::GradientStroke,
         validation::FeatureSupport::ReferenceFallback, 3U,
         {"gradient-c", "shared"}},
        {validation::FeatureKind::TrimPath,
         validation::FeatureSupport::NativeSubset, 100U,
         {"trim-a", "trim-b", "trim-c", "trim-d"}},
        {validation::FeatureKind::Repeater,
         validation::FeatureSupport::NativeSubset, 10U,
         {"repeater-a", "trim-a"}},
    };    const auto ranking = lab::rankFeaturePriorities(observations);
    require(ranking.size() == 3U, "unexpected candidate count");
    require(ranking.front().family == lab::CandidateFamily::NativeSubsetCompletion,
            "impact ranking changed");
    require(ranking.front().blockedAssets == 5U,
            "blocked asset union changed");
    require(ranking[1].family == lab::CandidateFamily::Gradients,
            "gradient aggregation changed");
    require(ranking[1].occurrences == 15U,
            "gradient occurrence count changed");
    require(ranking[1].blockedAssets == 4U,
            "gradient asset union changed");
    require(lab::stableAssetAlias(0x1234ULL) == "asset-0000000000001234",
            "stable alias changed");
    require(lab::stableAssetAlias(0x1234ULL, 2U)
                == "asset-0000000000001234-2",
            "collision alias changed");

    model::MotionAssetModel model;
    model.debugName = "corpus-test";
    model.sourceNodes.resize(3U);
    model.properties.resize(5U);
    model.scalarValues.resize(7U);
    const auto bytes = lab::estimateCanonicalModelBytes(model);
    require(bytes >= sizeof(model::MotionAssetModel),
            "model estimate omitted root object");
    require(bytes >= sizeof(model::MotionAssetModel)
            + 3U * sizeof(model::MotionSourceNodeRecord),
            "model estimate omitted nodes");
    std::cout << "AveMotion corpus analysis tests passed\n";
    return 0;
}
