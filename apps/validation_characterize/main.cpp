#include "avemotion/core/Hash.hpp"
#include "avemotion/formats/Tgs.hpp"
#include "avemotion/runtime/Runtime.hpp"
#include "avemotion/validation/AssetValidator.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace avemotion;

struct CorpusEntry final {
    std::filesystem::path json;
    std::filesystem::path tgs;
    std::size_t jsonBytes = 0;
    std::size_t tgsBytes = 0;
};

struct FeatureAggregate final {
    validation::FeatureSupport support = validation::FeatureSupport::Unsupported;
    std::size_t occurrences = 0;
    std::size_t assets = 0;
};

[[noreturn]] void fail(std::string_view message) {
    std::cerr << "AveMotion validation characterizer failed: " << message << '\n';
    std::exit(1);
}

void require(bool condition, std::string_view message) {
    if (!condition) {
        fail(message);
    }
}

std::vector<std::byte> readBytes(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    require(static_cast<bool>(stream), "cannot open input file");
    const auto end = stream.tellg();
    require(end >= 0, "cannot determine input size");
    std::vector<std::byte> bytes(static_cast<std::size_t>(end));
    stream.seekg(0, std::ios::beg);
    if (!bytes.empty()) {
        stream.read(reinterpret_cast<char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()));
    }
    require(static_cast<bool>(stream) || bytes.empty(), "cannot read input file");
    return bytes;
}

std::vector<CorpusEntry> readManifest(
    const std::filesystem::path& root,
    const std::filesystem::path& manifestPath) {
    std::ifstream stream(manifestPath);
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
        std::array<std::string, 5> fields;
        for (auto& field : fields) {
            require(static_cast<bool>(std::getline(row, field, '\t')),
                    "manifest row is incomplete");
        }
        entries.push_back({
            .json = root / fields[0],
            .tgs = root / fields[1],
            .jsonBytes = static_cast<std::size_t>(std::stoull(fields[2])),
            .tgsBytes = static_cast<std::size_t>(std::stoull(fields[3])),
        });
    }
    return entries;
}

std::string featureSummary(const validation::AssetValidationReport& report) {
    std::ostringstream stream;
    bool first = true;
    for (const auto& feature : report.features) {
        if (!first) {
            stream << ';';
        }
        first = false;
        stream << validation::toString(feature.feature) << '='
               << feature.occurrences << ':'
               << validation::toString(feature.support);
    }
    return stream.str();
}

std::string sanitize(std::string value) {
    std::replace(value.begin(), value.end(), '\t', ' ');
    std::replace(value.begin(), value.end(), '\r', ' ');
    std::replace(value.begin(), value.end(), '\n', ' ');
    return value;
}

} // namespace

int main(int argc, char** argv) {
    std::filesystem::path root;
    std::filesystem::path manifest;
    std::filesystem::path output;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--root" && index + 1 < argc) {
            root = argv[++index];
        } else if (argument == "--manifest" && index + 1 < argc) {
            manifest = argv[++index];
        } else if (argument == "--output" && index + 1 < argc) {
            output = argv[++index];
        } else {
            std::cerr << "usage: avemotion_validation_characterize "
                         "--root <source-root> --manifest <manifest.tsv> "
                         "--output <directory>\n";
            return 1;
        }
    }
    require(!root.empty() && !manifest.empty() && !output.empty(),
            "missing required command-line arguments");
    std::filesystem::create_directories(output);

    const auto entries = readManifest(root, manifest);
    runtime::Runtime runtime;
    validation::AssetValidator validator;
    std::map<validation::FeatureKind, FeatureAggregate> featureAggregates;
    std::size_t applicationAccepted = 0U;
    std::size_t nativeReadyAssets = 0U;
    std::size_t fallbackAssets = 0U;
    std::size_t unsupportedAssets = 0U;
    std::size_t telegramAccepted = 0U;
    std::size_t totalNodes = 0U;
    std::size_t totalProperties = 0U;
    std::size_t totalTracks = 0U;
    std::size_t totalSegments = 0U;
    std::size_t totalShapePoints = 0U;
    std::size_t totalWorkUnits = 0U;

    std::ofstream summary(output / "validation_manifest.tsv",
                          std::ios::binary | std::ios::trunc);
    std::ofstream issues(output / "validation_issues.tsv",
                         std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(summary) && static_cast<bool>(issues),
            "cannot create output files");

    summary
        << "asset\tasset_hash\tjson_bytes\ttgs_bytes\twidth\theight\tfps"
        << "\tframes\tduration_ms\tnodes\tproperties\ttracks\tsegments"
        << "\tshape_points\tapplication_accepted\tnative_direct2d"
        << "\tfallback_occurrences\tunsupported_occurrences"
        << "\tapplication_warnings\tapplication_errors"
        << "\tapplication_fingerprint\ttelegram_accepted"
        << "\ttelegram_warnings\ttelegram_errors\ttelegram_fatals"
        << "\ttelegram_fingerprint\twork_units\tfeatures\n";
    issues
        << "asset\tprofile\tseverity\tcode\tfeature\tnode\tproperty\tmessage\n";

    for (const auto& entry : entries) {
        const auto jsonBytes = readBytes(entry.json);
        const auto tgsBytes = readBytes(entry.tgs);
        require(jsonBytes.size() == entry.jsonBytes, "JSON size mismatch");
        require(tgsBytes.size() == entry.tgsBytes, "TGS size mismatch");
        const auto decoded = formats::decodeTgs(tgsBytes);
        require(static_cast<bool>(decoded), "TGS decode failed");
        require(decoded.json.size() == jsonBytes.size(), "decoded JSON size mismatch");
        require(std::equal(decoded.json.begin(), decoded.json.end(),
                           reinterpret_cast<const char*>(jsonBytes.data())),
                "decoded TGS does not match source JSON");

        const auto loaded = runtime.loadTgs(
            tgsBytes, entry.tgs.filename().string());
        require(static_cast<bool>(loaded), "runtime failed to load TGS");
        const auto prepared = loaded.asset->prepareModel();
        require(static_cast<bool>(prepared), "runtime failed to prepare model");

        validation::AssetValidationContext context;
        context.sourceFormat = validation::AssetSourceFormat::TelegramTgs;
        context.encodedBytes = tgsBytes.size();
        context.jsonBytes = jsonBytes.size();
        context.loopIntentKnown = true;
        context.intendedLoop = true;

        const auto application = validator.validate(
            *prepared.model,
            context,
            validation::AssetValidationOptions::applicationAsset());
        const auto telegram = validator.validate(
            *prepared.model,
            context,
            validation::AssetValidationOptions::telegramSticker());
        require(application.structurallyValid,
                "compatibility corpus produced a structurally invalid model");

        applicationAccepted += application.accepted() ? 1U : 0U;
        nativeReadyAssets += application.nativeDirect2DReady ? 1U : 0U;
        fallbackAssets += application.requiresReferenceFallback ? 1U : 0U;
        unsupportedAssets += application.containsUnsupportedFeatures ? 1U : 0U;
        telegramAccepted += telegram.accepted() ? 1U : 0U;
        totalNodes += prepared.model->sourceNodes.size();
        totalProperties += prepared.model->properties.size();
        totalTracks += prepared.model->tracks.size();
        totalSegments += prepared.model->segments.size();
        totalShapePoints += application.complexity.shapePointCount;
        totalWorkUnits += application.complexity.estimatedWorkUnits;
        for (const auto& feature : application.features) {
            auto& aggregate = featureAggregates[feature.feature];
            aggregate.support = feature.support;
            aggregate.occurrences += feature.occurrences;
            ++aggregate.assets;
        }

        const auto& metadata = loaded.asset->metadata();
        const auto durationMs = static_cast<std::uint64_t>(
            metadata.durationSeconds * 1000.0 + 0.5);
        summary
            << entry.tgs.stem().string() << '\t'
            << core::formatHash(metadata.sourceHash) << '\t'
            << jsonBytes.size() << '\t'
            << tgsBytes.size() << '\t'
            << metadata.width << '\t'
            << metadata.height << '\t'
            << metadata.frameRate << '\t'
            << metadata.totalFrames << '\t'
            << durationMs << '\t'
            << prepared.model->sourceNodes.size() << '\t'
            << prepared.model->properties.size() << '\t'
            << prepared.model->tracks.size() << '\t'
            << prepared.model->segments.size() << '\t'
            << application.complexity.shapePointCount << '\t'
            << (application.accepted() ? 1 : 0) << '\t'
            << (application.nativeDirect2DReady ? 1 : 0) << '\t'
            << application.fallbackFeatureOccurrences << '\t'
            << application.unsupportedFeatureOccurrences << '\t'
            << application.warningCount << '\t'
            << application.errorCount + application.fatalCount << '\t'
            << core::formatHash(application.fingerprint) << '\t'
            << (telegram.accepted() ? 1 : 0) << '\t'
            << telegram.warningCount << '\t'
            << telegram.errorCount << '\t'
            << telegram.fatalCount << '\t'
            << core::formatHash(telegram.fingerprint) << '\t'
            << application.complexity.estimatedWorkUnits << '\t'
            << featureSummary(application) << '\n';

        const auto writeIssues = [&](
            const validation::AssetValidationReport& report) {
            for (const auto& issue : report.issues) {
                issues
                    << entry.tgs.stem().string() << '\t'
                    << validation::toString(report.profile) << '\t'
                    << validation::toString(issue.severity) << '\t'
                    << validation::toString(issue.code) << '\t'
                    << validation::toString(issue.feature) << '\t';
                if (issue.node.valid()) {
                    issues << issue.node.index();
                }
                issues << '\t';
                if (issue.property.valid()) {
                    issues << issue.property.index();
                }
                issues << '\t' << sanitize(issue.message) << '\n';
            }
        };
        writeIssues(application);
        writeIssues(telegram);
    }

    std::ofstream featureOutput(
        output / "validation_features.tsv", std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(featureOutput), "cannot create feature-frequency output");
    featureOutput << "feature\tsupport\toccurrences\tassets\n";
    for (const auto& [feature, aggregate] : featureAggregates) {
        featureOutput << validation::toString(feature) << '\t'
                      << validation::toString(aggregate.support) << '\t'
                      << aggregate.occurrences << '\t'
                      << aggregate.assets << '\n';
    }

    std::ofstream summaryOutput(
        output / "validation_summary.txt", std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(summaryOutput), "cannot create validation summary");
    summaryOutput
        << "assets=" << entries.size() << '\n'
        << "applicationAccepted=" << applicationAccepted << '\n'
        << "nativeDirect2DReady=" << nativeReadyAssets << '\n'
        << "fallbackAssets=" << fallbackAssets << '\n'
        << "unsupportedAssets=" << unsupportedAssets << '\n'
        << "telegramAccepted=" << telegramAccepted << '\n'
        << "nodes=" << totalNodes << '\n'
        << "properties=" << totalProperties << '\n'
        << "tracks=" << totalTracks << '\n'
        << "segments=" << totalSegments << '\n'
        << "shapePoints=" << totalShapePoints << '\n'
        << "workUnits=" << totalWorkUnits << '\n';

    std::cout << "AveMotion validation characterization completed\n"
              << "assets=" << entries.size() << '\n'
              << "nativeDirect2DReady=" << nativeReadyAssets << '\n'
              << "telegramAccepted=" << telegramAccepted << '\n'
              << "output=" << output.string() << '\n';
    return 0;
}
