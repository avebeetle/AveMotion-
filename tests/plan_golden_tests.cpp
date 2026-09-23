#include "avemotion/core/Hash.hpp"
#include "avemotion/reference/UpstreamInfo.hpp"
#include "avemotion/render/RenderPlanner.hpp"
#include "avemotion/runtime/Runtime.hpp"
#include "support/GoldenVariance.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {
[[noreturn]] void fail(const std::string& message) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

std::string readText(const fs::path& path) {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        fail("unable to open " + path.string());
    }
    return {
        std::istreambuf_iterator<char>{stream},
        std::istreambuf_iterator<char>{}};
}

std::string row(
    std::string_view variant,
    std::string_view asset,
    std::uint64_t assetHash,
    std::string_view sample,
    std::size_t frame,
    std::size_t viewportWidth,
    std::size_t viewportHeight,
    const avemotion::render::MotionRenderPlan& plan) {
    const auto& fp = plan.fingerprints;
    const auto& stats = plan.statistics;
    std::ostringstream out;
    out << variant << '\t'
        << asset << '\t'
        << avemotion::core::formatHash(assetHash) << '\t'
        << sample << '\t'
        << frame << '\t'
        << viewportWidth << 'x' << viewportHeight << '\t'
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
        << (plan.dirtyRegion.valid ? 1 : 0);
    return out.str();
}
} // namespace

int main() {
    const auto upstream = avemotion::reference::selectedUpstream();
    std::ifstream expectedStream{AVEMOTION_PLAN_GOLDEN_FILE, std::ios::binary};
    if (!expectedStream) {
        fail("unable to open plan golden file");
    }
    std::vector<std::string> expected;
    std::string line;
    std::getline(expectedStream, line);
    while (std::getline(expectedStream, line)) {
        if (!line.empty()) {
            expected.push_back(line);
        }
    }

    std::vector<fs::path> assets;
    for (const auto& entry : fs::directory_iterator{AVEMOTION_CORPUS_DIR}) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            assets.push_back(entry.path());
        }
    }
    std::sort(assets.begin(), assets.end());

    avemotion::runtime::Runtime runtime;
    std::vector<std::string> actual;
    for (const auto& assetPath : assets) {
        auto loaded = runtime.loadLottieJson(
            readText(assetPath), assetPath.filename().string());
        if (!loaded) {
            fail("unable to load " + assetPath.string());
        }
        auto created = runtime.createInstance(loaded.asset);
        if (!created) {
            fail("unable to create instance for " + assetPath.string());
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
            auto evaluated = created.instance->evaluateFrame(frame, 128U, 128U);
            if (!evaluated) {
                fail("unable to evaluate " + assetPath.string());
            }
            const auto selectedFrame = evaluated.scene.frameIndex;
            const auto width = evaluated.scene.viewportWidth;
            const auto height = evaluated.scene.viewportHeight;
            auto planned = planner.build(std::move(evaluated.scene));
            if (!planned) {
                fail("unable to plan " + assetPath.string());
            }
            actual.push_back(row(
                upstream.variant,
                assetPath.filename().string(),
                metadata.sourceHash,
                sample,
                selectedFrame,
                width,
                height,
                planned.plan));
        }
    }

    if (actual.size() != expected.size()) {
        std::cerr << "Plan golden mismatch: expected " << expected.size()
                  << " rows, got " << actual.size() << '\n';
        return EXIT_FAILURE;
    }

#if defined(_MSC_VER)
    constexpr bool kAllowKnownMsvcFloatVariance = true;
#else
    constexpr bool kAllowKnownMsvcFloatVariance = false;
#endif
    std::size_t acceptedVariances = 0U;
    for (std::size_t index = 0U; index < actual.size(); ++index) {
        const auto comparison = avemotion::testsupport::compareGoldenRows(
            avemotion::testsupport::GoldenManifestKind::Plan,
            expected[index],
            actual[index],
            kAllowKnownMsvcFloatVariance);
        if (!comparison.matches) {
            std::cerr << "Plan golden mismatch at row " << index
                      << ": " << comparison.message
                      << "\nexpected: " << expected[index]
                      << "\nactual:   " << actual[index] << '\n';
            return EXIT_FAILURE;
        }
        if (comparison.acceptedKnownVariance) {
            ++acceptedVariances;
            std::cout << "Accepted known MSVC rlottie float variance at plan row "
                      << index << '\n';
        }
    }

    std::cout << "AveMotion plan golden tests passed for " << upstream.variant
              << " with " << actual.size() << " rows";
    if (acceptedVariances != 0U) {
        std::cout << " (" << acceptedVariances
                  << " approved upstream MSVC float variance)";
    }
    std::cout << '\n';
    return EXIT_SUCCESS;
}
