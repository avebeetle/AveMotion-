#include "avemotion/core/Hash.hpp"
#include "avemotion/evaluation/PropertyEvaluator.hpp"
#include "avemotion/reference/UpstreamInfo.hpp"
#include "avemotion/render/RenderPlanner.hpp"
#include "avemotion/render/SourceGeometryProjector.hpp"
#include "avemotion/runtime/Runtime.hpp"

#include <bit>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::string readText(const fs::path& path) {
    std::ifstream stream{path, std::ios::binary};
    return {std::istreambuf_iterator<char>{stream},
            std::istreambuf_iterator<char>{}};
}

void usage(const char* executable) {
    std::cerr << "Usage: " << executable
              << " --output DIR [--size PIXELS] ASSET.json...\n";
}

std::uint64_t projectedFingerprint(
    const avemotion::runtime::EvaluatedScene& scene) {
    avemotion::core::Fnv1a64 hash;
    for (const auto& item : scene.drawItems) {
        if (!item.sourceGeometryProjected) continue;
        hash.appendU32(item.sourcePathNode.value);
        hash.appendU32(item.sourcePaintNode.value);
        hash.appendU8(static_cast<std::uint8_t>(item.geometryOrigin));
        hash.appendU64(item.sourceGeometryRevision);
        hash.appendU64(item.localPath.hash);
        hash.appendU64(item.localPath.verbs.size());
        hash.appendU64(item.localPath.points.size());
        hash.appendU8(item.sourceRepeaterProjected ? 1U : 0U);
        if (item.sourceRepeaterProjected) {
            hash.appendU32(item.sourceRepeaterNode.value);
            hash.appendU32(item.sourceRepeaterCopyIndex);
            hash.appendU32(item.sourceRepeaterVisibleCopies);
            hash.appendU32(item.sourceRepeaterMaximumCopies);
            hash.appendU64(item.sourceRepeaterTransformRevision);
            hash.appendU64(item.sourceRepeaterOpacityRevision);
        }
    }
    return hash.value();
}

// Raw geometry hashes intentionally preserve exact IEEE-754 bits because they
// are useful while characterizing one compiler/runtime. They are not a
// portable semantic identity: legacy rlottie trigonometry and recursive path
// arithmetic can differ by a few ULPs between libm and the MSVC CRT.
//
// This second fingerprint quantizes only projected path coordinates to a
// 1/64 asset-unit grid while keeping topology and source identities exact.
// The grid is much finer than a display pixel in the supported fixtures, but
// stable across the observed low-order CRT differences. It exists only at the
// characterization boundary; runtime geometry revisions and cache keys remain
// exact and unquantized.
constexpr double kPortableGeometryUnitsPerAssetUnit = 64.0;

[[nodiscard]] std::int64_t portableCoordinate(float value) noexcept {
    if (value == 0.0F) return 0;
    if (std::isnan(value)) return std::numeric_limits<std::int64_t>::min();
    if (value == std::numeric_limits<float>::infinity()) {
        return std::numeric_limits<std::int64_t>::max();
    }
    if (value == -std::numeric_limits<float>::infinity()) {
        return std::numeric_limits<std::int64_t>::min() + 1;
    }

    const auto scaled = static_cast<double>(value)
        * kPortableGeometryUnitsPerAssetUnit;
    constexpr auto kUpper = static_cast<double>(
        std::numeric_limits<std::int64_t>::max() - 1);
    constexpr auto kLower = static_cast<double>(
        std::numeric_limits<std::int64_t>::min() + 2);
    if (scaled >= kUpper) return std::numeric_limits<std::int64_t>::max() - 1;
    if (scaled <= kLower) return std::numeric_limits<std::int64_t>::min() + 2;

    // Conversion to an integer truncates toward zero. Adding/subtracting 0.5
    // implements deterministic round-to-nearest for the finite values used by
    // canonical geometry without calling a platform-specific rounding helper.
    return static_cast<std::int64_t>(
        scaled >= 0.0 ? scaled + 0.5 : scaled - 0.5);
}

void appendPortableCoordinate(
    avemotion::core::Fnv1a64& hash,
    float value) noexcept {
    hash.appendU64(std::bit_cast<std::uint64_t>(portableCoordinate(value)));
}

std::uint64_t projectedPortableFingerprint(
    const avemotion::runtime::EvaluatedScene& scene) {
    avemotion::core::Fnv1a64 hash;
    for (const auto& item : scene.drawItems) {
        if (!item.sourceGeometryProjected) continue;
        hash.appendU32(item.sourcePathNode.value);
        hash.appendU32(item.sourcePaintNode.value);
        hash.appendU8(static_cast<std::uint8_t>(item.geometryOrigin));
        hash.appendU64(item.sourceGeometryId);
        hash.appendU8(static_cast<std::uint8_t>(item.fillRule));
        hash.appendU64(item.localPath.verbs.size());
        hash.appendU64(item.localPath.points.size());
        for (const auto verb : item.localPath.verbs) {
            hash.appendU8(static_cast<std::uint8_t>(verb));
        }
        for (const auto point : item.localPath.points) {
            appendPortableCoordinate(hash, point.x);
            appendPortableCoordinate(hash, point.y);
        }
        hash.appendU8(item.sourceRepeaterProjected ? 1U : 0U);
        if (item.sourceRepeaterProjected) {
            hash.appendU32(item.sourceRepeaterNode.value);
            hash.appendU32(item.sourceRepeaterCopyIndex);
            hash.appendU32(item.sourceRepeaterVisibleCopies);
            hash.appendU32(item.sourceRepeaterMaximumCopies);
        }
    }
    return hash.value();
}

void writeHeader(std::ostream& out) {
    out << "variant\tasset\tasset_fnv64\tsample\tframe\tprojected_fingerprint"
           "\tprojected_portable_fingerprint"
           "\tvisited\tcandidates\tprojected\tstatic\tanimated\tpoints"
           "\tprimitive_candidates\trectangle_candidates\tellipse_candidates"
           "\tpolystar_candidates\tstar_candidates\tpolygon_candidates"
           "\trectangles\trounded_rectangles\tellipses\tstars\tpolygons"
           "\ttrim_candidates\ttrim_simultaneous\ttrim_individual"
           "\ttrim_multi_path\ttrim_paths_visited\ttrim_paths_projected"
           "\ttrim_individual_wrapped_noop"
           "\ttrim_projected\ttrim_empty\ttrim_full\ttrim_partial"
           "\ttrim_splits"
           "\trepeater_candidates\trepeater_copies_visited"
           "\trepeater_copies_projected\trepeater_visible_copies"
           "\trepeater_hidden_copies\trepeater_static_geometry"
           "\trepeater_animated_geometry\trepeater_transform_animated"
           "\trepeater_opacity_animated\trepeater_fill_applications"
           "\trepeater_stroke_applications\trepeater_nested_paint_applications"
           "\trepeater_animated_paint_applications\trepeater_binding_skips"
           "\trepeater_property_skips\trepeater_evaluation_skips"
           "\trepeater_input_rejections\trepeater_parity_rejections"
           "\ttrim_binding_skips\ttrim_property_skips"
           "\ttrim_evaluation_skips\ttrim_input_rejections"
           "\tprimitive_property_skips\tprimitive_evaluation_skips"
           "\tpolystar_property_skips\tpolystar_evaluation_skips"
           "\tpolystar_input_rejections"
           "\tnested_candidates\tmodified_skips\tmultiple_skips"
           "\ttransform_skips\tparity_rejections\tgeometry_updates"
           "\tasset_geometry\tinstance_geometry\n";
}

} // namespace

int main(int argc, char** argv) {
    fs::path output;
    std::size_t renderSize = 128U;
    std::vector<fs::path> assets;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--output" && index + 1 < argc) {
            output = argv[++index];
        } else if (argument == "--size" && index + 1 < argc) {
            renderSize = static_cast<std::size_t>(std::stoul(argv[++index]));
        } else if (!argument.empty() && argument.front() == '-') {
            usage(argv[0]);
            return EXIT_FAILURE;
        } else {
            assets.emplace_back(argument);
        }
    }
    if (output.empty() || assets.empty() || renderSize == 0U) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    fs::create_directories(output);
    std::ofstream manifest{output / "source_geometry_manifest.tsv", std::ios::binary};
    if (!manifest) return EXIT_FAILURE;
    writeHeader(manifest);

    const auto upstream = avemotion::reference::selectedUpstream();
    avemotion::runtime::Runtime runtime;
    std::size_t rows = 0U;
    for (const auto& assetPath : assets) {
        auto loaded = runtime.loadLottieJson(
            readText(assetPath), assetPath.filename().string());
        if (!loaded) {
            std::cerr << "Unable to load " << assetPath << '\n';
            return EXIT_FAILURE;
        }
        const auto prepared = loaded.asset->prepareModel();
        if (!prepared) {
            std::cerr << "Unable to prepare " << assetPath << '\n';
            return EXIT_FAILURE;
        }
        auto created = runtime.createInstance(loaded.asset);
        if (!created) return EXIT_FAILURE;

        avemotion::evaluation::PropertyEvaluator evaluator{prepared.model};
        avemotion::evaluation::PropertyEvaluationWorkspace workspace;
        evaluator.prepare(workspace);
        avemotion::render::SourceGeometryProjector projector{prepared.model};
        if (!projector.valid()) return EXIT_FAILURE;
        avemotion::render::SourceGeometryProjectionWorkspace projectionWorkspace;
        if (!projector.prepare(projectionWorkspace)) return EXIT_FAILURE;
        const auto projectionStorageGeneration =
            projectionWorkspace.storageGeneration();
        avemotion::render::MotionRenderPlanner planner;

        const auto& metadata = loaded.asset->metadata();
        const std::vector<std::pair<std::string_view, std::size_t>> samples{
            {"p000", 0U},
            {"p025", metadata.totalFrames / 4U},
            {"p050", metadata.totalFrames / 2U},
            {"p075", (metadata.totalFrames * 3U) / 4U},
            {"p100", metadata.totalFrames - 1U},
        };
        for (const auto& [sample, frame] : samples) {
            const auto properties = evaluator.evaluate(
                static_cast<double>(frame), workspace);
            if (!properties) return EXIT_FAILURE;
            auto evaluated = created.instance->evaluateModelFrame(
                frame, renderSize, renderSize);
            if (!evaluated) return EXIT_FAILURE;
            const auto projected = projector.project(
                evaluated.scene, properties, projectionWorkspace);
            if (!projected) return EXIT_FAILURE;
            if (projectionWorkspace.storageGeneration()
                != projectionStorageGeneration) {
                std::cerr << "Source-geometry workspace grew after prepare for "
                          << assetPath << '\n';
                return EXIT_FAILURE;
            }
            const auto fingerprint = projectedFingerprint(evaluated.scene);
            const auto portableFingerprint =
                projectedPortableFingerprint(evaluated.scene);
            auto planned = planner.build(evaluated.scene);
            if (!planned) return EXIT_FAILURE;
            const auto& s = projected.statistics;
            const auto& p = planned.plan.statistics;
            manifest << upstream.variant << '\t'
                     << assetPath.filename().string() << '\t'
                     << avemotion::core::formatHash(metadata.sourceHash) << '\t'
                     << sample << '\t'
                     << evaluated.scene.frameIndex << '\t'
                     << avemotion::core::formatHash(fingerprint) << '\t'
                     << avemotion::core::formatHash(portableFingerprint) << '\t'
                     << s.drawItemsVisited << '\t'
                     << s.candidates << '\t'
                     << s.projected << '\t'
                     << s.projectedStatic << '\t'
                     << s.projectedAnimated << '\t'
                     << s.projectedPoints << '\t'
                     << s.primitiveCandidates << '\t'
                     << s.rectangleCandidates << '\t'
                     << s.ellipseCandidates << '\t'
                     << s.polystarCandidates << '\t'
                     << s.starCandidates << '\t'
                     << s.polygonCandidates << '\t'
                     << s.projectedRectangles << '\t'
                     << s.projectedRoundedRectangles << '\t'
                     << s.projectedEllipses << '\t'
                     << s.projectedStars << '\t'
                     << s.projectedPolygons << '\t'
                     << s.trimCandidates << '\t'
                     << s.trimSimultaneousCandidates << '\t'
                     << s.trimIndividualCandidates << '\t'
                     << s.trimMultiPathCandidates << '\t'
                     << s.trimPathsVisited << '\t'
                     << s.trimPathsProjected << '\t'
                     << s.trimIndividualWrappedNoOps << '\t'
                     << s.projectedTrimmed << '\t'
                     << s.projectedTrimEmpty << '\t'
                     << s.projectedTrimFull << '\t'
                     << s.projectedTrimPartial << '\t'
                     << s.trimSplitCount << '\t'
                     << s.repeaterCandidates << '\t'
                     << s.repeaterCopiesVisited << '\t'
                     << s.repeaterCopiesProjected << '\t'
                     << s.repeaterVisibleCopies << '\t'
                     << s.repeaterHiddenCopies << '\t'
                     << s.repeaterStaticGeometryCopies << '\t'
                     << s.repeaterAnimatedGeometryCopies << '\t'
                     << s.repeaterTransformAnimatedCopies << '\t'
                     << s.repeaterOpacityAnimatedCopies << '\t'
                     << s.repeaterFillApplicationsProjected << '\t'
                     << s.repeaterStrokeApplicationsProjected << '\t'
                     << s.repeaterNestedPaintApplicationsProjected << '\t'
                     << s.repeaterAnimatedPaintApplicationsProjected << '\t'
                     << s.skippedRepeaterBinding << '\t'
                     << s.skippedRepeaterProperties << '\t'
                     << s.skippedRepeaterEvaluation << '\t'
                     << s.rejectedRepeaterInput << '\t'
                     << s.repeaterParityMismatches << '\t'
                     << s.skippedTrimBinding << '\t'
                     << s.skippedTrimProperties << '\t'
                     << s.skippedTrimEvaluation << '\t'
                     << s.rejectedTrimInput << '\t'
                     << s.skippedPrimitiveProperties << '\t'
                     << s.skippedPrimitiveEvaluation << '\t'
                     << s.skippedPolystarProperties << '\t'
                     << s.skippedPolystarEvaluation << '\t'
                     << s.rejectedPolystarInputs << '\t'
                     << s.nestedCompositionCandidates << '\t'
                     << s.skippedModified << '\t'
                     << s.skippedMultiplePaths << '\t'
                     << s.skippedTransform << '\t'
                     << s.parityMismatches << '\t'
                     << p.geometryUpdateCount << '\t'
                     << p.assetStaticGeometryCount << '\t'
                     << p.instanceGeometryCount << '\n';
            ++rows;
        }
    }
    std::cout << "Recorded " << rows << " source geometry samples\n";
    return EXIT_SUCCESS;
}
