#include "avemotion/core/Hash.hpp"
#include "avemotion/reference/UpstreamInfo.hpp"
#include "avemotion/runtime/RecordingBackend.hpp"
#include "avemotion/runtime/Runtime.hpp"

#include <algorithm>
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

void writeManifestHeader(std::ostream& out) {
    out << "variant\tasset\tasset_fnv64\tsample\tframe\tviewport"
           "\tscene\ttopology\tgeometry\tpaint\tlayers\tvisible_layers"
           "\tdraw_items\tmasks\tclip_paths\tpath_verbs\tpath_points"
           "\tgradient_stops\n";
}

void writeManifestRow(
    std::ostream& out,
    std::string_view variant,
    std::string_view asset,
    std::uint64_t assetHash,
    std::string_view sample,
    const avemotion::runtime::EvaluatedScene& scene) {
    const auto& stats = scene.statistics;
    const auto& fp = scene.fingerprints;
    out << variant << '\t'
        << asset << '\t'
        << avemotion::core::formatHash(assetHash) << '\t'
        << sample << '\t'
        << scene.frameIndex << '\t'
        << scene.viewportWidth << 'x' << scene.viewportHeight << '\t'
        << avemotion::core::formatHash(fp.scene) << '\t'
        << avemotion::core::formatHash(fp.topology) << '\t'
        << avemotion::core::formatHash(fp.geometry) << '\t'
        << avemotion::core::formatHash(fp.paint) << '\t'
        << stats.layerCount << '\t'
        << stats.visibleLayerCount << '\t'
        << stats.drawItemCount << '\t'
        << stats.maskCount << '\t'
        << stats.clipPathCount << '\t'
        << stats.pathVerbCount << '\t'
        << stats.pathPointCount << '\t'
        << stats.gradientStopCount << '\n';
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

    fs::create_directories(output / "scenes");
    std::ofstream manifest{output / "scene_manifest.tsv", std::ios::binary};
    if (!manifest) {
        std::cerr << "Unable to create scene manifest\n";
        return EXIT_FAILURE;
    }
    writeManifestHeader(manifest);

    const auto upstream = avemotion::reference::selectedUpstream();
    avemotion::runtime::Runtime runtime;
    avemotion::runtime::RecordingBackend recorder;
    std::size_t sceneCount = 0U;

    for (const auto& assetPath : assets) {
        const auto json = readText(assetPath);
        auto loaded = runtime.loadLottieJson(json, assetPath.filename().string());
        if (!loaded) {
            std::cerr << "Unable to load " << assetPath << ": "
                      << loaded.error.message << '\n';
            return EXIT_FAILURE;
        }
        auto created = runtime.createInstance(loaded.asset);
        if (!created) {
            std::cerr << "Unable to create instance for " << assetPath << ": "
                      << created.error.message << '\n';
            return EXIT_FAILURE;
        }

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
                std::cerr << "Unable to evaluate " << assetPath << ": "
                          << evaluated.error.message << '\n';
                return EXIT_FAILURE;
            }
            writeManifestRow(
                manifest,
                upstream.variant,
                assetPath.filename().string(),
                metadata.sourceHash,
                sample,
                evaluated.scene);
            const auto sceneFile = output / "scenes" /
                (assetPath.stem().string() + "_" + std::string{sample} + ".txt");
            std::ofstream sceneStream{sceneFile, std::ios::binary};
            sceneStream << recorder.describe(evaluated.scene);
            ++sceneCount;
        }
    }

    const auto diagnostics = runtime.diagnostics();
    std::cout << "Recorded " << sceneCount << " evaluated scenes using "
              << upstream.variant << " rlottie\n"
              << "layers=" << diagnostics.evaluatedLayers
              << " draw-items=" << diagnostics.evaluatedDrawItems
              << " masks=" << diagnostics.evaluatedMasks
              << " copied-points=" << diagnostics.copiedPathPoints << '\n';
    return EXIT_SUCCESS;
}
