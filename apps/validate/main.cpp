#include "avemotion/core/Hash.hpp"
#include "avemotion/formats/Tgs.hpp"
#include "avemotion/runtime/Runtime.hpp"
#include "avemotion/validation/AssetValidator.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace avemotion;

struct Options final {
    validation::ValidationProfile profile =
        validation::ValidationProfile::ApplicationAsset;
    bool loopIntentKnown = false;
    bool intendedLoop = true;
    bool tsv = false;
    std::optional<std::filesystem::path> output;
    std::vector<std::filesystem::path> inputs;
};

void printUsage() {
    std::cout
        << "Usage: avemotion_validate [options] asset.json|asset.tgs [...]\n"
        << "Options:\n"
        << "  --profile application|telegram|direct2d\n"
        << "  --loop | --no-loop       Supply host loop intent\n"
        << "  --tsv                    Write one summary row per asset\n"
        << "  --output <path>          Write report to a file\n"
        << "  --help\n";
}

std::optional<Options> parseOptions(int argc, char** argv) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--help" || argument == "-h") {
            printUsage();
            return std::nullopt;
        }
        if (argument == "--profile") {
            if (++index >= argc) {
                std::cerr << "--profile requires a value\n";
                return std::nullopt;
            }
            const std::string_view value{argv[index]};
            if (value == "application") {
                options.profile = validation::ValidationProfile::ApplicationAsset;
            } else if (value == "telegram") {
                options.profile = validation::ValidationProfile::TelegramSticker;
            } else if (value == "direct2d") {
                options.profile = validation::ValidationProfile::Direct2DNative;
            } else {
                std::cerr << "unknown profile: " << value << '\n';
                return std::nullopt;
            }
            continue;
        }
        if (argument == "--loop") {
            options.loopIntentKnown = true;
            options.intendedLoop = true;
            continue;
        }
        if (argument == "--no-loop") {
            options.loopIntentKnown = true;
            options.intendedLoop = false;
            continue;
        }
        if (argument == "--tsv") {
            options.tsv = true;
            continue;
        }
        if (argument == "--output") {
            if (++index >= argc) {
                std::cerr << "--output requires a path\n";
                return std::nullopt;
            }
            options.output = std::filesystem::path{argv[index]};
            continue;
        }
        if (!argument.empty() && argument.front() == '-') {
            std::cerr << "unknown option: " << argument << '\n';
            return std::nullopt;
        }
        options.inputs.emplace_back(argv[index]);
    }
    if (options.inputs.empty()) {
        printUsage();
        return std::nullopt;
    }
    return options;
}

std::optional<std::vector<std::byte>> readFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        return std::nullopt;
    }
    const auto end = stream.tellg();
    if (end < 0) {
        return std::nullopt;
    }
    const auto size = static_cast<std::size_t>(end);
    std::vector<std::byte> bytes(size);
    stream.seekg(0, std::ios::beg);
    if (size != 0U) {
        stream.read(reinterpret_cast<char*>(bytes.data()),
                    static_cast<std::streamsize>(size));
    }
    if (!stream && size != 0U) {
        return std::nullopt;
    }
    return bytes;
}

validation::AssetValidationOptions validationOptions(
    validation::ValidationProfile profile) {
    switch (profile) {
    case validation::ValidationProfile::ApplicationAsset:
        return validation::AssetValidationOptions::applicationAsset();
    case validation::ValidationProfile::TelegramSticker:
        return validation::AssetValidationOptions::telegramSticker();
    case validation::ValidationProfile::Direct2DNative:
        return validation::AssetValidationOptions::direct2DNative();
    }
    return validation::AssetValidationOptions::applicationAsset();
}

void writeTsvHeader(std::ostream& output) {
    output
        << "asset\tformat\tprofile\taccepted\tstructural\tnative_direct2d"
        << "\tfallback\tunsupported\tinfo\twarnings\terrors\tfatals"
        << "\tnodes\tproperties\ttracks\tsegments\tshape_points"
        << "\twork_units\tfingerprint\n";
}

void writeTsvRow(
    std::ostream& output,
    const std::filesystem::path& path,
    validation::AssetSourceFormat format,
    const model::MotionAssetModel& model,
    const validation::AssetValidationReport& report) {
    output
        << path.filename().string() << '\t'
        << validation::toString(format) << '\t'
        << validation::toString(report.profile) << '\t'
        << (report.accepted() ? 1 : 0) << '\t'
        << (report.structurallyValid ? 1 : 0) << '\t'
        << (report.nativeDirect2DReady ? 1 : 0) << '\t'
        << report.fallbackFeatureOccurrences << '\t'
        << report.unsupportedFeatureOccurrences << '\t'
        << report.infoCount << '\t'
        << report.warningCount << '\t'
        << report.errorCount << '\t'
        << report.fatalCount << '\t'
        << model.sourceNodes.size() << '\t'
        << model.properties.size() << '\t'
        << model.tracks.size() << '\t'
        << model.segments.size() << '\t'
        << report.complexity.shapePointCount << '\t'
        << report.complexity.estimatedWorkUnits << '\t'
        << core::formatHash(report.fingerprint) << '\n';
}

void writeTextReport(
    std::ostream& output,
    const std::filesystem::path& path,
    validation::AssetSourceFormat format,
    const runtime::AssetMetadata& metadata,
    const validation::AssetValidationReport& report) {
    output << "Asset: " << path.string() << '\n'
           << "Format: " << validation::toString(format) << '\n'
           << "Profile: " << validation::toString(report.profile) << '\n'
           << "Result: " << (report.accepted() ? "ACCEPT" : "REJECT") << '\n'
           << "Canvas: " << metadata.width << 'x' << metadata.height << '\n'
           << "Frame rate: " << metadata.frameRate << '\n'
           << "Duration: " << metadata.durationSeconds << " s\n"
           << "Frames: " << metadata.totalFrames << '\n'
           << "Structural: " << (report.structurallyValid ? "valid" : "invalid") << '\n'
           << "Direct2D native ready: "
           << (report.nativeDirect2DReady ? "yes" : "no") << '\n'
           << "Fallback occurrences: " << report.fallbackFeatureOccurrences << '\n'
           << "Unsupported occurrences: " << report.unsupportedFeatureOccurrences << '\n'
           << "Complexity work units: " << report.complexity.estimatedWorkUnits << '\n'
           << "Report fingerprint: " << core::formatHash(report.fingerprint) << '\n';

    output << "Features:\n";
    for (const auto& feature : report.features) {
        output << "  - " << validation::toString(feature.feature)
               << ": " << feature.occurrences
               << " [" << validation::toString(feature.support) << "]\n";
    }
    if (!report.issues.empty()) {
        output << "Issues:\n";
        for (const auto& issue : report.issues) {
            output << "  - [" << validation::toString(issue.severity) << "] "
                   << validation::toString(issue.code) << ": "
                   << issue.message;
            if (issue.node.valid()) {
                output << " (node " << issue.node.index() << ')';
            }
            if (issue.property.valid()) {
                output << " (property " << issue.property.index() << ')';
            }
            output << '\n';
        }
    }
    output << '\n';
}

} // namespace

int main(int argc, char** argv) {
    const auto parsed = parseOptions(argc, argv);
    if (!parsed.has_value()) {
        return argc > 1 ? 1 : 0;
    }
    const auto options = *parsed;

    std::ofstream fileOutput;
    std::ostream* output = &std::cout;
    if (options.output.has_value()) {
        fileOutput.open(*options.output, std::ios::binary | std::ios::trunc);
        if (!fileOutput) {
            std::cerr << "cannot open output file: " << options.output->string() << '\n';
            return 1;
        }
        output = &fileOutput;
    }
    if (options.tsv) {
        writeTsvHeader(*output);
    }

    runtime::Runtime runtime;
    const validation::AssetValidator validator;
    const auto validatorOptions = validationOptions(options.profile);
    bool loadFailure = false;
    bool validationFailure = false;

    for (const auto& path : options.inputs) {
        const auto bytes = readFile(path);
        if (!bytes.has_value()) {
            std::cerr << "cannot read asset: " << path.string() << '\n';
            loadFailure = true;
            continue;
        }

        const auto detected = formats::detectAssetFormat(*bytes);
        validation::AssetSourceFormat sourceFormat =
            validation::AssetSourceFormat::Unknown;
        validation::AssetValidationContext context;
        context.encodedBytes = bytes->size();
        context.loopIntentKnown = options.loopIntentKnown;
        context.intendedLoop = options.intendedLoop;

        runtime::AssetLoadResult loaded;
        if (detected == formats::AssetFormat::TelegramTgs) {
            sourceFormat = validation::AssetSourceFormat::TelegramTgs;
            const auto limits = options.profile == validation::ValidationProfile::TelegramSticker
                ? formats::TgsDecodeLimits::telegramSticker()
                : formats::TgsDecodeLimits::relaxedApplicationAsset();
            const auto decoded = formats::decodeTgs(*bytes, limits);
            if (!decoded) {
                std::cerr << path.string() << ": TGS decode failed: "
                          << decoded.error.message << '\n';
                loadFailure = true;
                continue;
            }
            context.jsonBytes = decoded.metadata.jsonBytes;
            loaded = runtime.loadLottieJson(decoded.json, path.filename().string());
        } else if (detected == formats::AssetFormat::LottieJson) {
            sourceFormat = validation::AssetSourceFormat::LottieJson;
            context.jsonBytes = bytes->size();
            const std::string json{
                reinterpret_cast<const char*>(bytes->data()), bytes->size()};
            loaded = runtime.loadLottieJson(json, path.filename().string());
        } else {
            std::cerr << path.string() << ": unsupported asset format\n";
            loadFailure = true;
            continue;
        }
        context.sourceFormat = sourceFormat;

        if (!loaded) {
            std::cerr << path.string() << ": asset load failed: "
                      << loaded.error.message << '\n';
            loadFailure = true;
            continue;
        }
        const auto prepared = loaded.asset->prepareModel();
        if (!prepared) {
            std::cerr << path.string() << ": model preparation failed: "
                      << prepared.error << '\n';
            loadFailure = true;
            continue;
        }

        const auto report = validator.validate(*prepared.model, context, validatorOptions);
        if (options.tsv) {
            writeTsvRow(*output, path, sourceFormat, *prepared.model, report);
        } else {
            writeTextReport(*output, path, sourceFormat, loaded.asset->metadata(), report);
        }
        validationFailure = validationFailure || !report.accepted();
    }

    if (loadFailure) {
        return 1;
    }
    return validationFailure ? 2 : 0;
}
