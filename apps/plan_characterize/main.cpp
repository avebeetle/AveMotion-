#include "avemotion/core/Hash.hpp"
#include "avemotion/reference/UpstreamInfo.hpp"
#include "avemotion/render/HeadlessBackend.hpp"
#include "avemotion/render/RenderPlanner.hpp"
#include "avemotion/runtime/Runtime.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {
std::string readText(const fs::path& path) {
    std::ifstream stream{path, std::ios::binary};
    return {
        std::istreambuf_iterator<char>{stream},
        std::istreambuf_iterator<char>{}};
}

void usage(const char* executable) {
    std::cerr << "Usage: " << executable
              << " --output DIR [--size PIXELS] ASSET.json...\n";
}

void writeHeader(std::ostream& out) {
    out << "variant\tasset\tasset_fnv64\tsample\tframe\tviewport"
           "\tplan\ttopology\tgeometry_identity\tpaint_identity"
           "\tpresentation\tsource_items\tvisible_items\tgeometry_updates"
           "\tpaint_updates\tasset_static_geometry\tinstance_geometry"
           "\tasset_static_paint\tinstance_paint\tunsupported_items"
           "\tdirty_valid\n";
}

void writeRow(
    std::ostream& out,
    std::string_view variant,
    std::string_view asset,
    std::uint64_t assetHash,
    std::string_view sample,
    const avemotion::runtime::EvaluatedScene& scene,
    const avemotion::render::MotionRenderPlan& plan) {
    const auto& fp = plan.fingerprints;
    const auto& stats = plan.statistics;
    out << variant << '\t'
        << asset << '\t'
        << avemotion::core::formatHash(assetHash) << '\t'
        << sample << '\t'
        << scene.frameIndex << '\t'
        << scene.viewportWidth << 'x' << scene.viewportHeight << '\t'
        << avemotion::core::formatHash(fp.plan) << '\t'
        << avemotion::core::formatHash(fp.topology) << '\t'
        << avemotion::core::formatHash(fp.geometryIdentity) << '\t'
        << avemotion::core::formatHash(fp.paintIdentity) << '\t'
        << avemotion::core::formatHash(fp.presentation) << '\t'
        << stats.sourceDrawItemCount << '\t'
        << stats.visibleDrawItemCount << '\t'
        << stats.geometryUpdateCount << '\t'
        << stats.paintUpdateCount << '\t'
        << stats.assetStaticGeometryCount << '\t'
        << stats.instanceGeometryCount << '\t'
        << stats.assetStaticPaintCount << '\t'
        << stats.instancePaintCount << '\t'
        << stats.unsupportedFeatureItemCount << '\t'
        << (plan.dirtyRegion.valid ? 1 : 0) << '\n';
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

    fs::create_directories(output / "plans");
    std::ofstream manifest{output / "plan_manifest.tsv", std::ios::binary};
    if (!manifest) {
        std::cerr << "Unable to create plan manifest\n";
        return EXIT_FAILURE;
    }
    writeHeader(manifest);

    const auto upstream = avemotion::reference::selectedUpstream();
    avemotion::runtime::Runtime runtime;
    avemotion::render::HeadlessPlanBackend headless;
    std::size_t planCount = 0U;

    for (const auto& assetPath : assets) {
        auto loaded = runtime.loadLottieJson(
            readText(assetPath), assetPath.filename().string());
        if (!loaded) {
            std::cerr << "Unable to load " << assetPath << ": "
                      << loaded.error.message << '\n';
            return EXIT_FAILURE;
        }
        auto created = runtime.createInstance(loaded.asset);
        if (!created) {
            std::cerr << "Unable to create instance for " << assetPath << '\n';
            return EXIT_FAILURE;
        }
        avemotion::render::MotionRenderPlanner planner;
        const auto& metadata = loaded.asset->metadata();
        const std::vector<std::pair<std::string_view, std::size_t>> samples = {
            {"p000", 0U},
            {"p025", metadata.totalFrames / 4U},
            {"p050", metadata.totalFrames / 2U},
            {"p075", (metadata.totalFrames * 3U) / 4U},
            {"p100", metadata.totalFrames - 1U},
        };
        for (const auto& [sample, frame] : samples) {
            auto evaluated = created.instance->evaluateFrame(
                frame, renderSize, renderSize);
            if (!evaluated) {
                std::cerr << "Unable to evaluate " << assetPath << '\n';
                return EXIT_FAILURE;
            }
            const auto frameIndex = evaluated.scene.frameIndex;
            const auto viewportWidth = evaluated.scene.viewportWidth;
            const auto viewportHeight = evaluated.scene.viewportHeight;
            auto planned = planner.build(std::move(evaluated.scene));
            if (!planned) {
                std::cerr << "Unable to plan " << assetPath << ": "
                          << planned.error.message << '\n';
                return EXIT_FAILURE;
            }
            avemotion::runtime::EvaluatedScene rowScene;
            rowScene.frameIndex = frameIndex;
            rowScene.viewportWidth = viewportWidth;
            rowScene.viewportHeight = viewportHeight;
            writeRow(
                manifest,
                upstream.variant,
                assetPath.filename().string(),
                metadata.sourceHash,
                sample,
                rowScene,
                planned.plan);
            const auto planFile = output / "plans" /
                (assetPath.stem().string() + "_" + std::string{sample} + ".txt");
            std::ofstream planStream{planFile, std::ios::binary};
            planStream << headless.describe(planned.plan);
            ++planCount;
        }
    }

    std::cout << "Recorded " << planCount << " render plans using "
              << upstream.variant << " rlottie\n";
    return EXIT_SUCCESS;
}
