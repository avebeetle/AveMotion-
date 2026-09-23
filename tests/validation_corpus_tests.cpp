#include "avemotion/formats/Tgs.hpp"
#include "avemotion/runtime/Runtime.hpp"
#include "avemotion/validation/AssetValidator.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifndef AVEMOTION_SOURCE_ROOT
#error AVEMOTION_SOURCE_ROOT is required
#endif

namespace {

using namespace avemotion;

struct CorpusEntry final {
    std::filesystem::path json;
    std::filesystem::path tgs;
    std::size_t jsonBytes = 0;
    std::size_t tgsBytes = 0;
};

[[noreturn]] void fail(std::string_view message) {
    std::cerr << "AveMotion validation corpus test failed: " << message << '\n';
    std::exit(1);
}

void require(bool condition, std::string_view message) {
    if (!condition) {
        fail(message);
    }
}

std::vector<std::byte> readBytes(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    require(static_cast<bool>(stream), "cannot open corpus file");
    const auto end = stream.tellg();
    require(end >= 0, "cannot determine corpus file size");
    std::vector<std::byte> bytes(static_cast<std::size_t>(end));
    stream.seekg(0, std::ios::beg);
    if (!bytes.empty()) {
        stream.read(reinterpret_cast<char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()));
    }
    require(static_cast<bool>(stream) || bytes.empty(), "cannot read corpus file");
    return bytes;
}

std::vector<CorpusEntry> readManifest(const std::filesystem::path& root) {
    const auto path = root / "tests/compatibility/manifest.tsv";
    std::ifstream stream(path);
    require(static_cast<bool>(stream), "cannot open compatibility manifest");
    std::string line;
    require(static_cast<bool>(std::getline(stream, line)), "manifest is empty");
    require(line == "source_json\ttgs\tjson_bytes\ttgs_bytes\tsha256",
            "manifest header mismatch");
    std::vector<CorpusEntry> entries;
    while (std::getline(stream, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream row(line);
        std::string json;
        std::string tgs;
        std::string jsonBytes;
        std::string tgsBytes;
        std::string sha;
        require(static_cast<bool>(std::getline(row, json, '\t')),
                "manifest JSON column missing");
        require(static_cast<bool>(std::getline(row, tgs, '\t')),
                "manifest TGS column missing");
        require(static_cast<bool>(std::getline(row, jsonBytes, '\t')),
                "manifest JSON size column missing");
        require(static_cast<bool>(std::getline(row, tgsBytes, '\t')),
                "manifest TGS size column missing");
        require(static_cast<bool>(std::getline(row, sha, '\t')),
                "manifest hash column missing");
        entries.push_back({
            .json = root / json,
            .tgs = root / tgs,
            .jsonBytes = static_cast<std::size_t>(std::stoull(jsonBytes)),
            .tgsBytes = static_cast<std::size_t>(std::stoull(tgsBytes)),
        });
    }
    return entries;
}

} // namespace

int main() {
    const std::filesystem::path root{AVEMOTION_SOURCE_ROOT};
    const auto entries = readManifest(root);
    require(entries.size() == 16U, "unexpected compatibility corpus size");

    runtime::Runtime runtime;
    validation::AssetValidator validator;
    std::size_t nativeReady = 0U;
    std::size_t fallbackAssets = 0U;
    std::size_t unsupportedAssets = 0U;
    std::size_t telegramCompliant = 0U;

    for (const auto& entry : entries) {
        const auto jsonBytes = readBytes(entry.json);
        const auto tgsBytes = readBytes(entry.tgs);
        require(jsonBytes.size() == entry.jsonBytes, "JSON size mismatch");
        require(tgsBytes.size() == entry.tgsBytes, "TGS size mismatch");

        const auto decoded = formats::decodeTgs(
            tgsBytes, formats::TgsDecodeLimits::telegramSticker());
        require(static_cast<bool>(decoded), "TGS decode failed");
        require(decoded.json.size() == jsonBytes.size(), "decoded JSON size mismatch");
        require(std::equal(decoded.json.begin(), decoded.json.end(),
                           reinterpret_cast<const char*>(jsonBytes.data())),
                "TGS transport changed JSON bytes");

        const std::string json{
            reinterpret_cast<const char*>(jsonBytes.data()), jsonBytes.size()};
        const auto jsonAsset = runtime.loadLottieJson(json, entry.json.filename().string());
        const auto tgsAsset = runtime.loadTgs(
            tgsBytes,
            entry.tgs.filename().string(),
            formats::TgsDecodeLimits::telegramSticker());
        require(static_cast<bool>(jsonAsset), "JSON asset load failed");
        require(static_cast<bool>(tgsAsset), "TGS asset load failed");
        const auto jsonModel = jsonAsset.asset->prepareModel();
        const auto tgsModel = tgsAsset.asset->prepareModel();
        require(static_cast<bool>(jsonModel), "JSON model preparation failed");
        require(static_cast<bool>(tgsModel), "TGS model preparation failed");
        require(jsonModel.model->fingerprint == tgsModel.model->fingerprint,
                "JSON and TGS canonical model fingerprints differ");

        validation::AssetValidationContext jsonContext;
        jsonContext.sourceFormat = validation::AssetSourceFormat::LottieJson;
        jsonContext.encodedBytes = jsonBytes.size();
        jsonContext.jsonBytes = jsonBytes.size();
        jsonContext.loopIntentKnown = true;
        jsonContext.intendedLoop = true;
        validation::AssetValidationContext tgsContext = jsonContext;
        tgsContext.sourceFormat = validation::AssetSourceFormat::TelegramTgs;
        tgsContext.encodedBytes = tgsBytes.size();

        const auto jsonReport = validator.validate(
            *jsonModel.model,
            jsonContext,
            validation::AssetValidationOptions::applicationAsset());
        const auto tgsReport = validator.validate(
            *tgsModel.model,
            tgsContext,
            validation::AssetValidationOptions::applicationAsset());
        require(jsonReport.structurallyValid, "JSON report is structurally invalid");
        require(tgsReport.structurallyValid, "TGS report is structurally invalid");
        require(jsonReport.fingerprint == tgsReport.fingerprint,
                "application validation depends on transport format");

        const auto telegramReport = validator.validate(
            *tgsModel.model,
            tgsContext,
            validation::AssetValidationOptions::telegramSticker());

        nativeReady += jsonReport.nativeDirect2DReady ? 1U : 0U;
        fallbackAssets += jsonReport.requiresReferenceFallback ? 1U : 0U;
        unsupportedAssets += jsonReport.containsUnsupportedFeatures ? 1U : 0U;
        telegramCompliant += telegramReport.accepted() ? 1U : 0U;
    }

    require(nativeReady != 0U, "corpus contains no Direct2D-native asset");
    require(fallbackAssets != 0U, "corpus contains no fallback feature coverage");
    require(unsupportedAssets == 0U, "existing compatibility corpus contains unsupported features");
    require(telegramCompliant > 0U,
            "corpus contains no Telegram-profile-compliant asset");
    require(telegramCompliant < entries.size(),
            "synthetic corpus unexpectedly all satisfies Telegram sticker profile");

    std::cout << "AveMotion validation compatibility corpus passed\n"
              << "assets=" << entries.size() << '\n'
              << "nativeReady=" << nativeReady << '\n'
              << "fallbackAssets=" << fallbackAssets << '\n'
              << "telegramCompliant=" << telegramCompliant << '\n';
    return 0;
}
