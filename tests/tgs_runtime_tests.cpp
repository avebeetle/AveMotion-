#include "avemotion/runtime/RecordingBackend.hpp"
#include "avemotion/runtime/Runtime.hpp"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace {

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

void require(bool condition, const std::string& message) {
    if (!condition) fail(message);
}

std::string readText(const fs::path& path) {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) fail("unable to open " + path.string());
    return {
        std::istreambuf_iterator<char>{stream},
        std::istreambuf_iterator<char>{}};
}

std::vector<std::byte> readBytes(const fs::path& path) {
    const auto text = readText(path);
    std::vector<std::byte> bytes(text.size());
    for (std::size_t index = 0U; index < text.size(); ++index) {
        bytes[index] = static_cast<std::byte>(
            static_cast<unsigned char>(text[index]));
    }
    return bytes;
}

} // namespace

int main() {
    using namespace avemotion;
    const fs::path fixtureDir{AVEMOTION_FIXTURE_DIR};
    const fs::path tgsDir{AVEMOTION_TGS_FIXTURE_DIR};
    const auto json = readText(fixtureDir / "repeater_content_group.json");
    const auto tgs = readBytes(tgsDir / "repeater_content_group.tgs");

    runtime::Runtime runtime;

    const std::array<std::byte, 3U> invalid{
        std::byte{0x1F}, std::byte{0x8B}, std::byte{0x08}};
    const auto rejected = runtime.loadTgs(invalid, "truncated.tgs");
    require(!rejected, "truncated TGS unexpectedly loaded");
    require(rejected.error.code == runtime::RuntimeErrorCode::ContainerDecodeFailed,
        "truncated TGS did not report ContainerDecodeFailed");

    const auto tgsSpan = std::span<const std::byte>{tgs.data(), tgs.size()};
    if (!runtime::Runtime::compiledWithReferenceEngine()) {
        const auto offline = runtime.loadTgs(tgsSpan, "offline.tgs");
        require(!offline, "offline runtime unexpectedly loaded TGS");
        require(offline.error.code == runtime::RuntimeErrorCode::ReferenceUnavailable,
            "offline TGS load must report ReferenceUnavailable after decoding");

        const auto diagnostics = runtime.diagnostics();
        require(diagnostics.tgsDecodeAttempts == 2U,
            "offline TGS decode attempt diagnostics are wrong");
        require(diagnostics.tgsDecodesSucceeded == 1U,
            "offline successful TGS decode diagnostics are wrong");
        require(diagnostics.tgsDecodesFailed == 1U,
            "offline failed TGS decode diagnostics are wrong");
        require(diagnostics.tgsCompressedBytes == tgs.size(),
            "offline compressed byte diagnostics are wrong");
        require(diagnostics.tgsJsonBytes == json.size(),
            "offline JSON byte diagnostics are wrong");
        std::cout << "AveMotion offline TGS runtime seam tests passed\n";
        return EXIT_SUCCESS;
    }

    auto jsonLoaded = runtime.loadLottieJson(json, "source.json");
    auto tgsLoaded = runtime.loadTgs(tgsSpan, "source.tgs");
    auto fileLoaded = runtime.loadTgsFile(
        tgsDir / "repeater_content_group.tgs");
    auto autoTgs = runtime.loadAssetData(tgsSpan, "auto.tgs");
    const auto jsonBytes = std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(json.data()), json.size()};
    auto autoJson = runtime.loadAssetData(jsonBytes, "auto.json");

    require(jsonLoaded && tgsLoaded && fileLoaded && autoTgs && autoJson,
        "JSON/TGS runtime loading did not produce five valid assets");
    const auto& expected = jsonLoaded.asset->metadata();
    for (const auto* loaded : {
             &tgsLoaded, &fileLoaded, &autoTgs, &autoJson}) {
        const auto& metadata = loaded->asset->metadata();
        require(metadata.width == expected.width
                && metadata.height == expected.height
                && metadata.totalFrames == expected.totalFrames
                && metadata.frameRate == expected.frameRate
                && metadata.durationSeconds == expected.durationSeconds,
            "TGS and JSON metadata differ");
        require(metadata.sourceHash == expected.sourceHash,
            "TGS must retain decoded JSON source identity");
    }

    // The direct parsed-model builder is intentionally Telegram-specific at
    // this stage. Samsung remains a CPU/reference comparison variant, so TGS
    // transport parity there is proven through metadata, evaluated scenes and
    // reference pixels without requiring a canonical model that the variant
    // does not expose.
    if (std::string_view{AVEMOTION_TEST_REFERENCE_VARIANT} == "telegram") {
        const auto jsonModel = jsonLoaded.asset->prepareModel();
        const auto tgsModel = tgsLoaded.asset->prepareModel();
        require(jsonModel && tgsModel,
            "canonical model preparation failed after TGS loading");
        require(jsonModel.model->fingerprint == tgsModel.model->fingerprint
                && jsonModel.model->topologyFingerprint
                    == tgsModel.model->topologyFingerprint
                && jsonModel.model->resourceFingerprint
                    == tgsModel.model->resourceFingerprint,
            "TGS and JSON canonical models differ");
    }

    auto jsonInstance = runtime.createInstance(jsonLoaded.asset);
    auto tgsInstance = runtime.createInstance(tgsLoaded.asset);
    require(jsonInstance && tgsInstance, "TGS/JSON instance creation failed");

    runtime::RecordingBackend recorder;
    const auto middle = expected.totalFrames / 2U;
    auto jsonScene = jsonInstance.instance->evaluateFrame(middle, 128U, 128U);
    auto tgsScene = tgsInstance.instance->evaluateFrame(middle, 128U, 128U);
    require(jsonScene && tgsScene, "TGS/JSON scene evaluation failed");
    require(recorder.record(jsonScene.scene).fingerprints.scene
            == recorder.record(tgsScene.scene).fingerprints.scene,
        "TGS and JSON evaluated scenes differ");

    auto jsonCpu = jsonInstance.instance->renderCpuFrame(middle, 96U, 96U);
    auto tgsCpu = tgsInstance.instance->renderCpuFrame(middle, 96U, 96U);
    require(jsonCpu && tgsCpu, "TGS/JSON CPU reference render failed");
    require(jsonCpu.frame.fnv1a64 == tgsCpu.frame.fnv1a64,
        "TGS and JSON CPU reference pixels differ");

    const std::array<std::byte, 4U> unknown{
        std::byte{0x00}, std::byte{0x01}, std::byte{0x02}, std::byte{0x03}};
    const auto unknownLoad = runtime.loadAssetData(unknown, "unknown.bin");
    require(!unknownLoad
            && unknownLoad.error.code == runtime::RuntimeErrorCode::InvalidAsset,
        "auto-detect must reject unknown asset bytes");

    const auto diagnostics = runtime.diagnostics();
    require(diagnostics.tgsDecodeAttempts == 4U,
        "TGS decode attempt diagnostics are wrong");
    require(diagnostics.tgsDecodesSucceeded == 3U,
        "TGS successful decode diagnostics are wrong");
    require(diagnostics.tgsDecodesFailed == 1U,
        "TGS failed decode diagnostics are wrong");
    require(diagnostics.tgsCompressedBytes == tgs.size() * 3U,
        "TGS compressed-byte diagnostics are wrong");
    require(diagnostics.tgsJsonBytes == json.size() * 3U,
        "TGS JSON-byte diagnostics are wrong");

    std::cout << "AveMotion TGS runtime integration tests passed\n";
    return EXIT_SUCCESS;
}
