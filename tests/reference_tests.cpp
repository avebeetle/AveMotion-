#include "avemotion/reference/ReferenceRuntime.hpp"
#include "avemotion/reference/UpstreamInfo.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {
[[noreturn]] void fail(const std::string& message) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

void require(bool condition, const std::string& message) {
    if (!condition) {
        fail(message);
    }
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

std::uint64_t loadAndRenderMid(const fs::path& path) {
    avemotion::reference::ReferenceRuntime runtime;
    auto loaded = runtime.loadJson(readText(path), path.filename().string(), false);
    if (!loaded) {
        fail("parallel load failed for " + path.string());
    }
    const auto total = loaded.animation->metadata().totalFrames;
    return loaded.animation->renderFrame(total / 2U, 64U, 64U).fnv1a64;
}
} // namespace

int main() {
    require(avemotion::reference::ReferenceRuntime::compiledWithRlottie(),
            "Part 2 reference tests require a selected rlottie variant");
    require(avemotion::reference::manifestMatchesSelectedUpstream(),
            "UPSTREAM.json must contain the selected repository and commit");

    avemotion::reference::ReferenceRuntime runtime;
    const auto invalid = runtime.loadJson("{}", "invalid", false);
    require(!invalid, "empty JSON object must be rejected");
    require(!invalid.error.empty(), "invalid JSON must include an error message");

    std::vector<fs::path> assets;
    for (const auto& entry : fs::directory_iterator{AVEMOTION_CORPUS_DIR}) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            assets.push_back(entry.path());
        }
    }
    std::sort(assets.begin(), assets.end());
    require(assets.size() >= 8U, "the characterization corpus is incomplete");

    for (const auto& path : assets) {
        const auto json = readText(path);
        auto first = runtime.loadJson(json, path.filename().string() + "-a", false);
        auto second = runtime.loadJson(json, path.filename().string() + "-b", false);
        require(first && second, "asset load failed for " + path.string());

        const auto& metadata = first.animation->metadata();
        require(metadata.loaded, "metadata must report a loaded asset");
        require(metadata.width > 0U && metadata.height > 0U,
                "asset dimensions must be positive");
        require(metadata.totalFrames > 0U, "asset must contain at least one frame");
        require(metadata.frameRate > 0.0, "asset frame rate must be positive");

        const auto middle = metadata.totalFrames / 2U;
        const auto frame0 = first.animation->renderFrame(0U, 96U, 96U);
        const auto middleA = first.animation->renderFrame(middle, 96U, 96U);
        const auto last = first.animation->renderFrame(
            metadata.totalFrames - 1U, 96U, 96U);
        const auto middleB = first.animation->renderFrame(middle, 96U, 96U);
        const auto middleIndependent = second.animation->renderFrame(middle, 96U, 96U);
        const auto frame0AfterSeek = first.animation->renderFrame(0U, 96U, 96U);

        require(frame0.fnv1a64 == frame0AfterSeek.fnv1a64,
                "random seek changed frame zero for " + path.string());
        require(middleA.fnv1a64 == middleB.fnv1a64,
                "repeated same-frame render is not deterministic for " + path.string());
        require(middleA.fnv1a64 == middleIndependent.fnv1a64,
                "independent instances disagree for " + path.string());
        require(frame0.argbPremultiplied.size() == 96U * 96U,
                "unexpected rendered surface size");
        require(last.argbPremultiplied.size() == 96U * 96U,
                "unexpected last-frame surface size");
    }

    std::vector<std::future<std::uint64_t>> futures;
    for (const auto& path : assets) {
        futures.push_back(std::async(std::launch::async, loadAndRenderMid, path));
    }
    for (auto& future : futures) {
        require(future.get() != 0U, "parallel independent render returned an invalid hash");
    }

    const auto upstream = avemotion::reference::selectedUpstream();
    std::cout << "AveMotion reference tests passed for " << upstream.variant
              << " @ " << upstream.commit << '\n';
    return EXIT_SUCCESS;
}
