#include "avemotion/reference/ReferenceRuntime.hpp"
#include "avemotion/reference/UpstreamInfo.hpp"
#include "avemotion/runtime/Runtime.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;
using avemotion::runtime::Runtime;

void require(bool condition, const std::string& message) {
    if (!condition) { std::cerr << "FAILED: " << message << '\n'; std::exit(EXIT_FAILURE); }
}

std::string readText(const fs::path& path) {
    std::ifstream stream{path, std::ios::binary};
    require(bool(stream), "unable to open " + path.string());
    return {std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}

std::size_t oracleFrame(const avemotion::reference::ReferenceAnimation& oracle,
                        double position) {
    const auto normalized = std::isfinite(position) ? position : 0.0;
    const auto count = oracle.metadata().totalFrames;
    return std::min(oracle.frameAtPosition(normalized), count - 1U);
}

void comparePositions(const std::string& json, const std::string& label,
                      const std::vector<double>& positions) {
    Runtime runtime;
    avemotion::reference::ReferenceRuntime reference;
    auto loaded = runtime.loadLottieJson(json, label);
    auto independent = reference.loadJson(json, label + "-oracle", false);
    require(loaded && independent, label + ": independent loads failed");
    const auto& actual = loaded.asset->metadata();
    const auto& expected = independent.animation->metadata();
    require(actual.width == expected.width && actual.height == expected.height
                && actual.frameRate == expected.frameRate
                && actual.durationSeconds == expected.durationSeconds
                && actual.totalFrames == expected.totalFrames,
            label + ": metadata differs from independent reference");
    runtime.resetDiagnostics();
    auto created = runtime.createInstance(loaded.asset);
    require(bool(created), label + ": ordinary instance creation failed");
    auto second = runtime.createInstance(loaded.asset);
    require(bool(second), label + ": independent instance creation failed");
    avemotion::runtime::Instance moved = std::move(*created.instance);
    for (const auto position : positions) {
        const auto frame = oracleFrame(*independent.animation, position);
        require(moved.frameAtPosition(position) == frame, label + ": moved mapping differs");
        require(second.instance->frameAtPosition(position) == frame,
                label + ": second mapping differs");
    }
    moved.setControlledProgress(0.5);
    static_cast<void>(moved.playbackSnapshot({}));
    runtime.resetDiagnostics();
    for (int repeat = 0; repeat < 100; ++repeat) {
        for (const auto position : positions) {
            static_cast<void>(moved.frameAtPosition(position));
            static_cast<void>(second.instance->frameAtPosition(position));
        }
        static_cast<void>(moved.playbackSnapshot({}));
    }
    const auto quiet = runtime.diagnostics();
    require(quiet.referenceMetadataSessionsCreated == 0U
                && quiet.referenceSceneSessionsCreated == 0U
                && quiet.referenceModelSessionsCreated == 0U
                && quiet.referenceCpuSessionsCreated == 0U
                && quiet.referenceSceneSamples == 0U
                && quiet.referenceModelSamples == 0U,
            label + ": mapping or playback constructed or sampled a reference session");
}

std::vector<double> positions(std::size_t count) {
    std::vector<double> result;
    result.reserve(1100U + count * 6U);
    for (int i = 0; i <= 1000; ++i) result.push_back(i / 1000.0);
    for (std::size_t frame = 0; frame < count && count > 1U && count <= 10000U; ++frame) {
        const double threshold = static_cast<double>(frame) / static_cast<double>(count - 1U);
        result.push_back(threshold);
        result.push_back(std::nextafter(threshold, 0.0));
        result.push_back(std::nextafter(threshold, 1.0));
        if (frame + 1U < count) {
            const double half = (static_cast<double>(frame) + 0.5)
                / static_cast<double>(count - 1U);
            result.push_back(half);
            result.push_back(std::nextafter(half, 0.0));
            result.push_back(std::nextafter(half, 1.0));
        }
    }
    result.insert(result.end(), {-1.0, 2.0,
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()});
    std::reverse(result.begin(), result.end());
    return result;
}

std::string minimal(double ip, double op) {
    return "{\"v\":\"5.7.4\",\"w\":64,\"h\":64,\"fr\":60,\"ip\":"
        + std::to_string(ip) + ",\"op\":" + std::to_string(op) + ",\"layers\":[]}";
}
} // namespace

int main() {
    const auto variant = avemotion::reference::selectedUpstream().variant;
    require(variant == "telegram" || variant == "samsung", "unsupported test variant");
    std::vector<fs::path> files;
    for (const auto& root : {fs::path{AVEMOTION_CORPUS_DIR}, fs::path{AVEMOTION_FIXTURE_DIR}}) {
        for (const auto& entry : fs::directory_iterator{root}) {
            if (entry.is_regular_file() && entry.path().extension() == ".json")
                files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    require(files.size() == 16U, "expected all 16 top-level JSON files");
    for (const auto& file : files) {
        Runtime runtime;
        auto loaded = runtime.loadLottieJson(readText(file), file.filename().string());
        require(bool(loaded), "unable to load " + file.string());
        comparePositions(readText(file), file.filename().string(),
                         positions(loaded.asset->metadata().totalFrames));
    }
    struct EndpointCase { double ip; double op; std::size_t telegramCount; std::size_t samsungCount; };
    for (const auto& fixture : std::array<EndpointCase, 4>{{
             {0.0, 10.0, 10U, 11U}, {100.0, 110.0, 10U, 11U},
             {-10.0, 10.0, 20U, 21U}, {0.25, 10.75, 10U, 12U}}}) {
        const auto json = minimal(fixture.ip, fixture.op);
        Runtime runtime;
        auto loaded = runtime.loadLottieJson(json, "minimal-count");
        require(bool(loaded), "minimal ordered fixture rejected");
        require(loaded.asset->metadata().totalFrames
                    == (variant == "telegram" ? fixture.telegramCount : fixture.samsungCount),
                "root endpoint frame count changed");
        comparePositions(json, "minimal-" + std::to_string(fixture.ip),
                         positions(loaded.asset->metadata().totalFrames));
    }
    const auto oneFrame = minimal(0.0, variant == "telegram" ? 1.0 : 0.0);
    comparePositions(oneFrame, "one-frame", positions(1U));
    {
        Runtime runtime;
        auto loaded = runtime.loadLottieJson(minimal(0.0, 10.0), "rounding");
        require(bool(loaded), "rounding fixture rejected");
        auto created = runtime.createInstance(loaded.asset);
        require(bool(created), "rounding fixture instance rejected");
        require(created.instance->frameAtPosition(0.05) == (variant == "telegram" ? 0U : 1U),
                "p=.05 must distinguish pinned rounding semantics");
    }
    {
        Runtime runtime;
        auto loaded = runtime.loadLottieJson(minimal(2.0, 1.0), "reversed");
        if (variant == "telegram") {
            require(bool(loaded), "Telegram reversed root range must remain accepted");
            require(loaded.asset->metadata().totalFrames
                        > static_cast<std::size_t>(std::numeric_limits<long>::max()),
                    "reversed range must select legacy mapping");
            runtime.resetDiagnostics();
            auto created = runtime.createInstance(loaded.asset);
            require(bool(created), "Telegram reversed instance must be accepted");
            require(runtime.diagnostics().referenceSceneSessionsCreated == 1U,
                    "reversed instance must retain its legacy Scene session");
            require(created.instance->frameAtPosition(0.0) == 0U,
                    "reversed zero position changed");
        } else {
            require(!loaded, "Samsung reversed root range must remain rejected");
        }
    }
    {
        Runtime left;
        Runtime right;
        require(left.createInstance({}).error.code
                    == avemotion::runtime::RuntimeErrorCode::InvalidArgument,
                "null asset error changed");
        auto loaded = left.loadLottieJson(minimal(0.0, 10.0), "foreign");
        require(bool(loaded), "foreign fixture load failed");
        require(right.createInstance(loaded.asset).error.code
                    == avemotion::runtime::RuntimeErrorCode::InvalidArgument,
                "foreign asset error changed");
    }
    Runtime runtime;
    auto loaded = runtime.loadLottieJson(minimal(0.0, 10.0), "zero-construction");
    require(bool(loaded), "zero-construction asset failed to load");
    runtime.resetDiagnostics();
    auto instance = runtime.createInstance(loaded.asset);
    require(bool(instance), "ordinary instance creation failed");
    require(runtime.diagnostics().referenceSceneSessionsCreated == 0U,
            "ordinary instance construction must not create a mapping tree");
    std::cout << "frame mapping parity and lifecycle passed\n";
}
