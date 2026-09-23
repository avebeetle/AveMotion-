#include "CorpusAnalysis.hpp"

#include "avemotion/core/Hash.hpp"
#include "avemotion/evaluation/PropertyEvaluator.hpp"
#include "avemotion/formats/Tgs.hpp"
#include "avemotion/render/RenderPlanner.hpp"
#include "avemotion/render/SourceGeometryProjector.hpp"
#include "avemotion/runtime/Runtime.hpp"
#include "avemotion/validation/AssetValidator.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using namespace avemotion;
using Clock = std::chrono::steady_clock;

struct Options final {
    std::filesystem::path input;
    std::filesystem::path output;
    bool recursive = true;
    bool includeNames = false;
    bool strict = false;
    std::size_t samples = 60U;
    std::size_t loadRepeats = 3U;
    std::size_t cpuRepeats = 2U;
    std::size_t renderSize = 128U;
    std::size_t maximumFiles = 1000U;
    std::uintmax_t maximumTotalBytes = 64U * 1024U * 1024U;
};

struct AssetResult final {
    std::string alias;
    std::string sourceName;
    std::string format;
    std::string status;
    std::string error;
    std::uint64_t rawHash = 0U;
    std::uint64_t sourceHash = 0U;
    std::size_t encodedBytes = 0U;
    std::size_t jsonBytes = 0U;
    std::size_t width = 0U;
    std::size_t height = 0U;
    double frameRate = 0.0;
    std::size_t totalFrames = 0U;
    std::uint64_t durationMs = 0U;
    bool applicationAccepted = false;
    bool direct2DNative = false;
    bool requiresFallback = false;
    bool unsupported = false;
    bool telegramAccepted = false;
    std::size_t warnings = 0U;
    std::size_t errors = 0U;
    std::size_t workUnits = 0U;
    std::size_t sourceNodes = 0U;
    std::size_t properties = 0U;
    std::size_t tracks = 0U;
    std::size_t segments = 0U;
    std::size_t shapePoints = 0U;
    std::size_t gradientValues = 0U;
    std::size_t projectedItems = 0U;
    std::size_t sourceDrawItems = 0U;
    std::size_t unsupportedDrawItems = 0U;
    std::int64_t loadModelMedianUs = 0;
    std::int64_t firstPipelineUs = 0;
    std::int64_t steadyPipelineAverageUs = 0;
    std::int64_t cpuRenderMedianUs = 0;
    std::size_t modelBytes = 0U;
    std::size_t evaluatorWorkspaceBytes = 0U;
    std::size_t projectorWorkspaceBytes = 0U;
    std::size_t sceneBytes = 0U;
    std::size_t planBytes = 0U;
    std::size_t perInstanceBytes = 0U;
    std::vector<validation::FeatureUsage> features;
    std::vector<std::pair<validation::ValidationProfile, validation::ValidationIssue>> issues;
};

struct FeatureAggregate final {
    validation::FeatureSupport support = validation::FeatureSupport::Native;
    std::size_t occurrences = 0U;
    std::set<std::string> assets;
};

[[noreturn]] void fail(std::string_view message) {
    std::cerr << "AveMotion corpus lab failed: " << message << '\n';
    std::exit(1);
}

void require(bool condition, std::string_view message) {
    if (!condition) fail(message);
}

[[nodiscard]] std::string pathUtf8(const std::filesystem::path& value) {
    const auto text = value.generic_u8string();
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}

[[nodiscard]] std::string sanitize(std::string value) {
    std::replace(value.begin(), value.end(), '\t', ' ');
    std::replace(value.begin(), value.end(), '\r', ' ');
    std::replace(value.begin(), value.end(), '\n', ' ');
    return value;
}

[[nodiscard]] std::optional<std::size_t> parseSize(std::string_view value) {
    try {
        std::size_t consumed = 0U;
        const auto parsed = std::stoull(std::string{value}, &consumed, 10);
        if (consumed != value.size()
            || parsed > std::numeric_limits<std::size_t>::max()) {
            return std::nullopt;
        }
        return static_cast<std::size_t>(parsed);
    } catch (...) {
        return std::nullopt;
    }
}

[[nodiscard]] Options parseOptions(int argc, char** argv) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        const auto takeValue = [&](std::string_view name) -> std::string_view {
            if (index + 1 >= argc) fail(std::string{name} + " requires a value");
            return argv[++index];
        };
        if (argument == "--input") {
            options.input = std::filesystem::path{takeValue(argument)};
        } else if (argument == "--output") {
            options.output = std::filesystem::path{takeValue(argument)};
        } else if (argument == "--no-recursive") {
            options.recursive = false;
        } else if (argument == "--include-names") {
            options.includeNames = true;
        } else if (argument == "--strict") {
            options.strict = true;
        } else if (argument == "--samples") {
            const auto value = parseSize(takeValue(argument));
            require(value && *value >= 1U && *value <= 10000U,
                    "--samples must be in [1, 10000]");
            options.samples = *value;
        } else if (argument == "--load-repeats") {
            const auto value = parseSize(takeValue(argument));
            require(value && *value >= 1U && *value <= 100U,
                    "--load-repeats must be in [1, 100]");
            options.loadRepeats = *value;
        } else if (argument == "--cpu-repeats") {
            const auto value = parseSize(takeValue(argument));
            require(value && *value <= 100U,
                    "--cpu-repeats must be in [0, 100]");
            options.cpuRepeats = *value;
        } else if (argument == "--render-size") {
            const auto value = parseSize(takeValue(argument));
            require(value && *value >= 16U && *value <= 4096U,
                    "--render-size must be in [16, 4096]");
            options.renderSize = *value;
        } else if (argument == "--max-files") {
            const auto value = parseSize(takeValue(argument));
            require(value && *value >= 1U && *value <= 100000U,
                    "--max-files must be in [1, 100000]");
            options.maximumFiles = *value;
        } else if (argument == "--max-total-mib") {
            const auto value = parseSize(takeValue(argument));
            require(value && *value >= 1U && *value <= 4096U,
                    "--max-total-mib must be in [1, 4096]");
            options.maximumTotalBytes = static_cast<std::uintmax_t>(*value)
                * 1024U * 1024U;
        } else {
            fail("unknown argument: " + std::string{argument});
        }
    }
    require(!options.input.empty(), "--input is required");
    require(!options.output.empty(), "--output is required");
    return options;
}

[[nodiscard]] bool candidateExtension(const std::filesystem::path& path) {
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return extension == ".tgs" || extension == ".json";
}

[[nodiscard]] std::vector<std::filesystem::path> enumerateFiles(
    const Options& options) {
    require(std::filesystem::exists(options.input), "input path does not exist");
    std::vector<std::filesystem::path> files;
    if (std::filesystem::is_regular_file(options.input)) {
        if (candidateExtension(options.input)) files.push_back(options.input);
    } else {
        const auto inspect = [&](const auto& entry) {
            std::error_code error;
            if (entry.is_regular_file(error) && !error
                && candidateExtension(entry.path())) {
                files.push_back(entry.path());
            }
        };
        if (options.recursive) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(
                     options.input,
                     std::filesystem::directory_options::skip_permission_denied)) {
                inspect(entry);
                require(files.size() <= options.maximumFiles,
                        "input exceeds --max-files");
            }
        } else {
            for (const auto& entry : std::filesystem::directory_iterator(options.input)) {
                inspect(entry);
                require(files.size() <= options.maximumFiles,
                        "input exceeds --max-files");
            }
        }
    }
    std::sort(files.begin(), files.end(), [](const auto& left, const auto& right) {
        return pathUtf8(left) < pathUtf8(right);
    });
    require(!files.empty(), "input contains no .tgs or .json files");
    return files;
}

[[nodiscard]] std::vector<std::byte> readBytes(
    const std::filesystem::path& path,
    std::uintmax_t& totalBytes,
    std::uintmax_t maximumTotalBytes) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) return {};
    const auto end = stream.tellg();
    if (end < 0) return {};
    const auto count = static_cast<std::uintmax_t>(end);
    require(count <= maximumTotalBytes - totalBytes,
            "input exceeds --max-total-mib");
    totalBytes += count;
    std::vector<std::byte> bytes(static_cast<std::size_t>(count));
    stream.seekg(0, std::ios::beg);
    if (!bytes.empty()) {
        stream.read(reinterpret_cast<char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()));
    }
    if (!stream && !bytes.empty()) return {};
    return bytes;
}

[[nodiscard]] std::int64_t microseconds(Clock::duration duration) noexcept {
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

[[nodiscard]] std::int64_t median(std::vector<std::int64_t> values) {
    if (values.empty()) return 0;
    std::sort(values.begin(), values.end());
    const auto middle = values.size() / 2U;
    if ((values.size() & 1U) != 0U) return values[middle];
    return (values[middle - 1U] + values[middle]) / 2;
}

[[nodiscard]] std::size_t saturatingAdd(
    std::size_t left,
    std::size_t right) noexcept {
    const auto maximum = std::numeric_limits<std::size_t>::max();
    return left > maximum - right ? maximum : left + right;
}

[[nodiscard]] std::size_t saturatingMultiply(
    std::size_t left,
    std::size_t right) noexcept {
    if (left == 0U || right == 0U) return 0U;
    const auto maximum = std::numeric_limits<std::size_t>::max();
    return left > maximum / right ? maximum : left * right;
}

[[nodiscard]] std::size_t sceneBytes(const runtime::EvaluatedScene& scene) noexcept {
    std::size_t total = sizeof(runtime::EvaluatedScene)
        + scene.layers.capacity() * sizeof(runtime::EvaluatedLayer)
        + scene.childLayerIndices.capacity() * sizeof(std::uint32_t)
        + scene.drawItems.capacity() * sizeof(runtime::EvaluatedDrawItem)
        + scene.masks.capacity() * sizeof(runtime::EvaluatedMask);
    const auto addPath = [&total](const runtime::EvaluatedPath& path) {
        total += path.verbs.capacity() * sizeof(runtime::PathVerb);
        total += path.points.capacity() * sizeof(runtime::Vec2);
    };
    for (const auto& layer : scene.layers) {
        total += layer.keyPath.capacity();
        addPath(layer.clipPath);
    }
    for (const auto& item : scene.drawItems) {
        addPath(item.path);
        addPath(item.localPath);
        total += item.stroke.dashArray.capacity() * sizeof(float);
        total += item.localStroke.dashArray.capacity() * sizeof(float);
        total += item.paint.gradient.stops.capacity()
            * sizeof(runtime::EvaluatedGradientStop);
        total += item.localPaint.gradient.stops.capacity()
            * sizeof(runtime::EvaluatedGradientStop);
    }
    for (const auto& mask : scene.masks) addPath(mask.path);
    return total;
}

[[nodiscard]] std::size_t planBytes(const render::MotionRenderPlan& plan) noexcept {
    return sizeof(render::MotionRenderPlan)
        + plan.drawItems.capacity() * sizeof(render::MotionDrawItem)
        + plan.geometryUpdates.capacity() * sizeof(render::MotionGeometryUpdate)
        + plan.paintUpdates.capacity() * sizeof(render::MotionPaintUpdate);
}

[[nodiscard]] std::string featureSummary(
    std::span<const validation::FeatureUsage> features) {
    std::ostringstream stream;
    bool first = true;
    for (const auto& feature : features) {
        if (!first) stream << ';';
        first = false;
        stream << validation::toString(feature.feature) << '='
               << feature.occurrences << ':'
               << validation::toString(feature.support);
    }
    return stream.str();
}

void appendIssues(
    AssetResult& result,
    validation::ValidationProfile profile,
    const validation::AssetValidationReport& report) {
    for (const auto& issue : report.issues) {
        result.issues.emplace_back(profile, issue);
    }
}

[[nodiscard]] runtime::AssetLoadResult loadAsset(
    runtime::Runtime& runtimeValue,
    std::span<const std::byte> bytes,
    formats::AssetFormat format,
    std::string_view name,
    std::size_t& jsonBytes) {
    if (format == formats::AssetFormat::TelegramTgs) {
        const auto limits = formats::TgsDecodeLimits::relaxedApplicationAsset();
        const auto decoded = formats::decodeTgs(bytes, limits);
        if (!decoded) {
            runtime::AssetLoadResult result;
            result.error.code = runtime::RuntimeErrorCode::ContainerDecodeFailed;
            result.error.message = decoded.error.message;
            return result;
        }
        jsonBytes = decoded.json.size();
        return runtimeValue.loadTgs(bytes, name, limits);
    }
    if (format == formats::AssetFormat::LottieJson) {
        jsonBytes = bytes.size();
        const std::string json{
            reinterpret_cast<const char*>(bytes.data()), bytes.size()};
        return runtimeValue.loadLottieJson(json, name);
    }
    runtime::AssetLoadResult result;
    result.error.code = runtime::RuntimeErrorCode::InvalidArgument;
    result.error.message = "unknown asset format";
    return result;
}

AssetResult analyzeAsset(
    const Options& options,
    const std::filesystem::path& path,
    std::span<const std::byte> bytes,
    std::size_t collisionOrdinal) {
    AssetResult result;
    result.rawHash = core::fnv1a64(bytes);
    result.alias = lab::stableAssetAlias(result.rawHash, collisionOrdinal);
    result.sourceName = options.includeNames ? pathUtf8(path.filename()) : "redacted";
    result.encodedBytes = bytes.size();
    const auto format = formats::detectAssetFormat(bytes);
    result.format = format == formats::AssetFormat::TelegramTgs
        ? "tgs"
        : format == formats::AssetFormat::LottieJson ? "json" : "unknown";
    if (format == formats::AssetFormat::Unknown) {
        result.status = "load-failed";
        result.error = "unknown asset format";
        return result;
    }

    std::vector<std::int64_t> loadTimes;
    loadTimes.reserve(options.loadRepeats);
    for (std::size_t repeat = 0U; repeat < options.loadRepeats; ++repeat) {
        runtime::Runtime timingRuntime;
        std::size_t jsonBytes = 0U;
        const auto start = Clock::now();
        const auto loaded = loadAsset(
            timingRuntime, bytes, format,
            result.alias + "-load-" + std::to_string(repeat), jsonBytes);
        if (!loaded) {
            result.status = "load-failed";
            result.error = loaded.error.message;
            return result;
        }
        const auto prepared = loaded.asset->prepareModel();
        if (!prepared) {
            result.status = "model-failed";
            result.error = prepared.error;
            return result;
        }
        loadTimes.push_back(microseconds(Clock::now() - start));
        result.jsonBytes = jsonBytes;
    }
    result.loadModelMedianUs = median(loadTimes);

    runtime::Runtime runtimeValue;
    std::size_t jsonBytes = 0U;
    const auto loaded = loadAsset(runtimeValue, bytes, format, result.alias, jsonBytes);
    if (!loaded) {
        result.status = "load-failed";
        result.error = loaded.error.message;
        return result;
    }
    result.jsonBytes = jsonBytes;
    const auto prepared = loaded.asset->prepareModel();
    if (!prepared) {
        result.status = "model-failed";
        result.error = prepared.error;
        return result;
    }
    const auto& metadata = loaded.asset->metadata();
    result.sourceHash = metadata.sourceHash;
    result.width = metadata.width;
    result.height = metadata.height;
    result.frameRate = metadata.frameRate;
    result.totalFrames = metadata.totalFrames;
    result.durationMs = static_cast<std::uint64_t>(
        std::max(0.0, metadata.durationSeconds) * 1000.0 + 0.5);
    result.sourceNodes = prepared.model->sourceNodes.size();
    result.properties = prepared.model->properties.size();
    result.tracks = prepared.model->tracks.size();
    result.segments = prepared.model->segments.size();
    result.modelBytes = lab::estimateCanonicalModelBytes(*prepared.model);

    validation::AssetValidationContext context;
    context.sourceFormat = format == formats::AssetFormat::TelegramTgs
        ? validation::AssetSourceFormat::TelegramTgs
        : validation::AssetSourceFormat::LottieJson;
    context.encodedBytes = bytes.size();
    context.jsonBytes = jsonBytes;
    context.loopIntentKnown = format == formats::AssetFormat::TelegramTgs;
    context.intendedLoop = context.loopIntentKnown;
    validation::AssetValidator validator;
    const auto application = validator.validate(
        *prepared.model, context,
        validation::AssetValidationOptions::applicationAsset());
    const auto direct2d = validator.validate(
        *prepared.model, context,
        validation::AssetValidationOptions::direct2DNative());
    const auto telegram = validator.validate(
        *prepared.model, context,
        validation::AssetValidationOptions::telegramSticker());
    result.applicationAccepted = application.accepted();
    result.direct2DNative = direct2d.accepted();
    result.requiresFallback = application.requiresReferenceFallback;
    result.unsupported = application.containsUnsupportedFeatures;
    result.telegramAccepted = telegram.accepted();
    result.warnings = application.warningCount;
    result.errors = application.errorCount + application.fatalCount;
    result.workUnits = application.complexity.estimatedWorkUnits;
    result.shapePoints = application.complexity.shapePointCount;
    result.gradientValues = application.complexity.gradientValueCount;
    result.features = application.features;
    appendIssues(result, validation::ValidationProfile::ApplicationAsset, application);
    appendIssues(result, validation::ValidationProfile::Direct2DNative, direct2d);
    appendIssues(result, validation::ValidationProfile::TelegramSticker, telegram);
    if (!application.structurallyValid) {
        result.status = "invalid-model";
        result.error = "canonical model failed structural validation";
        return result;
    }

    auto created = runtimeValue.createInstance(loaded.asset);
    if (!created) {
        result.status = "instance-failed";
        result.error = created.error.message;
        return result;
    }
    auto& instance = *created.instance;
    evaluation::PropertyEvaluator evaluator{prepared.model};
    if (!evaluator.valid()) {
        result.status = "evaluator-failed";
        result.error = std::string{evaluator.errorMessage()};
        return result;
    }
    evaluation::PropertyEvaluationWorkspace evaluationWorkspace;
    evaluator.prepare(evaluationWorkspace);
    render::SourceGeometryProjector projector{prepared.model};
    if (!projector.valid()) {
        result.status = "projector-failed";
        result.error = std::string{projector.errorMessage()};
        return result;
    }
    render::SourceGeometryProjectionWorkspace projectionWorkspace;
    if (!projector.prepare(projectionWorkspace)) {
        result.status = "projector-failed";
        result.error = "projector workspace preparation failed";
        return result;
    }
    result.evaluatorWorkspaceBytes = evaluationWorkspace.retainedBytes();
    result.projectorWorkspaceBytes = projectionWorkspace.retainedBytes();

    render::MotionRenderPlanner planner;
    auto runPipeline = [&](double progress, bool collect) -> bool {
        const auto frame = instance.frameAtPosition(progress);
        const auto evaluated = evaluator.evaluate(
            static_cast<double>(frame), evaluationWorkspace);
        if (!evaluated) return false;
        auto scene = instance.evaluateModelFrame(
            frame, options.renderSize, options.renderSize);
        if (!scene) return false;
        const auto projected = projector.project(
            scene.scene, evaluated, projectionWorkspace);
        if (!projected) return false;
        const auto built = planner.build(std::move(scene.scene));
        if (!built) return false;
        if (collect) {
            result.projectedItems += projected.statistics.projected;
            result.sourceDrawItems += built.plan.statistics.sourceDrawItemCount;
            result.unsupportedDrawItems +=
                built.plan.statistics.unsupportedFeatureItemCount;
            if (built.plan.sourceScene) {
                result.sceneBytes = std::max(
                    result.sceneBytes, sceneBytes(*built.plan.sourceScene));
            }
            result.planBytes = std::max(result.planBytes, planBytes(built.plan));
        }
        return true;
    };

    const auto firstStart = Clock::now();
    if (!runPipeline(0.0, true)) {
        result.status = "pipeline-failed";
        result.error = "first native pipeline sample failed";
        return result;
    }
    result.firstPipelineUs = microseconds(Clock::now() - firstStart);

    const auto steadyStart = Clock::now();
    for (std::size_t sample = 0U; sample < options.samples; ++sample) {
        const double progress = options.samples == 1U
            ? 0.5
            : static_cast<double>(sample)
                / static_cast<double>(options.samples - 1U);
        if (!runPipeline(progress, true)) {
            result.status = "pipeline-failed";
            result.error = "steady native pipeline sample failed";
            return result;
        }
    }
    result.steadyPipelineAverageUs = microseconds(Clock::now() - steadyStart)
        / static_cast<std::int64_t>(options.samples);

    if (options.cpuRepeats != 0U) {
        std::vector<std::int64_t> cpuTimes;
        cpuTimes.reserve(options.cpuRepeats);
        const auto frame = instance.frameAtPosition(0.5);
        for (std::size_t repeat = 0U; repeat < options.cpuRepeats; ++repeat) {
            const auto start = Clock::now();
            const auto cpu = instance.renderCpuFrame(
                frame, options.renderSize, options.renderSize, true);
            if (!cpu) {
                result.status = "cpu-render-failed";
                result.error = cpu.error.message;
                return result;
            }
            cpuTimes.push_back(microseconds(Clock::now() - start));
        }
        result.cpuRenderMedianUs = median(cpuTimes);
    }

    result.perInstanceBytes = result.evaluatorWorkspaceBytes
        + result.projectorWorkspaceBytes + result.sceneBytes + result.planBytes;
    result.status = "ok";
    return result;
}

void writeReports(const Options& options, std::vector<AssetResult> results) {
    std::filesystem::create_directories(options.output);
    std::sort(results.begin(), results.end(), [](const auto& left, const auto& right) {
        return left.alias < right.alias;
    });

    std::map<validation::FeatureKind, FeatureAggregate> featureAggregates;
    std::size_t loaded = 0U;
    std::size_t failed = 0U;
    std::size_t native = 0U;
    std::size_t fallback = 0U;
    std::size_t unsupported = 0U;
    std::size_t telegram = 0U;
    for (const auto& result : results) {
        if (result.status == "ok") {
            ++loaded;
            native += result.direct2DNative ? 1U : 0U;
            fallback += result.requiresFallback ? 1U : 0U;
            unsupported += result.unsupported ? 1U : 0U;
            telegram += result.telegramAccepted ? 1U : 0U;
            for (const auto& feature : result.features) {
                auto& aggregate = featureAggregates[feature.feature];
                aggregate.support = feature.support;
                aggregate.occurrences += feature.occurrences;
                aggregate.assets.insert(result.alias);
            }
        } else {
            ++failed;
        }
    }

    std::vector<lab::FeatureObservation> observations;
    observations.reserve(featureAggregates.size());
    for (const auto& [feature, aggregate] : featureAggregates) {
        observations.push_back({feature, aggregate.support,
                                aggregate.occurrences,
                                {aggregate.assets.begin(), aggregate.assets.end()}});
    }
    const auto priorities = lab::rankFeaturePriorities(observations);

    std::ofstream manifest(options.output / "corpus_manifest.tsv",
                           std::ios::binary | std::ios::trunc);
    std::ofstream issues(options.output / "corpus_issues.tsv",
                         std::ios::binary | std::ios::trunc);
    std::ofstream features(options.output / "corpus_features.tsv",
                           std::ios::binary | std::ios::trunc);
    std::ofstream benchmarks(options.output / "corpus_benchmarks.tsv",
                             std::ios::binary | std::ios::trunc);
    std::ofstream decisions(options.output / "corpus_decision.tsv",
                            std::ios::binary | std::ios::trunc);
    std::ofstream summary(options.output / "corpus_summary.txt",
                          std::ios::binary | std::ios::trunc);
    require(manifest && issues && features && benchmarks && decisions && summary,
            "cannot create corpus reports");

    manifest
        << "asset\tsource\tformat\tstatus\terror\traw_hash\tsource_hash"
        << "\tencoded_bytes\tjson_bytes\twidth\theight\tfps\tframes\tduration_ms"
        << "\tapplication_accepted\tdirect2d_native\tfallback\tunsupported"
        << "\ttelegram_accepted\twarnings\terrors\twork_units\tsource_nodes"
        << "\tproperties\ttracks\tsegments\tshape_points\tgradient_values"
        << "\tfeatures\n";
    benchmarks
        << "asset\tload_model_median_us\tfirst_pipeline_us"
        << "\tsteady_pipeline_average_us\tcpu_render_median_us"
        << "\tprojected_items\tsource_draw_items\tunsupported_draw_items"
        << "\tmodel_bytes\tevaluator_workspace_bytes\tprojector_workspace_bytes"
        << "\tscene_bytes\tplan_bytes\tper_instance_bytes"
        << "\tinstances_1_bytes\tinstances_16_bytes\tinstances_64_bytes\n";
    issues << "asset\tprofile\tseverity\tcode\tfeature\tnode\tproperty\tmessage\n";

    for (const auto& result : results) {
        manifest
            << result.alias << '\t' << sanitize(result.sourceName) << '\t'
            << result.format << '\t' << result.status << '\t'
            << sanitize(result.error) << '\t'
            << core::formatHash(result.rawHash) << '\t'
            << core::formatHash(result.sourceHash) << '\t'
            << result.encodedBytes << '\t' << result.jsonBytes << '\t'
            << result.width << '\t' << result.height << '\t'
            << std::setprecision(12) << result.frameRate << '\t'
            << result.totalFrames << '\t' << result.durationMs << '\t'
            << (result.applicationAccepted ? 1 : 0) << '\t'
            << (result.direct2DNative ? 1 : 0) << '\t'
            << (result.requiresFallback ? 1 : 0) << '\t'
            << (result.unsupported ? 1 : 0) << '\t'
            << (result.telegramAccepted ? 1 : 0) << '\t'
            << result.warnings << '\t' << result.errors << '\t'
            << result.workUnits << '\t' << result.sourceNodes << '\t'
            << result.properties << '\t' << result.tracks << '\t'
            << result.segments << '\t' << result.shapePoints << '\t'
            << result.gradientValues << '\t'
            << featureSummary(result.features) << '\n';
        benchmarks
            << result.alias << '\t' << result.loadModelMedianUs << '\t'
            << result.firstPipelineUs << '\t'
            << result.steadyPipelineAverageUs << '\t'
            << result.cpuRenderMedianUs << '\t'
            << result.projectedItems << '\t' << result.sourceDrawItems << '\t'
            << result.unsupportedDrawItems << '\t' << result.modelBytes << '\t'
            << result.evaluatorWorkspaceBytes << '\t'
            << result.projectorWorkspaceBytes << '\t'
            << result.sceneBytes << '\t' << result.planBytes << '\t'
            << result.perInstanceBytes << '\t'
            << saturatingAdd(result.modelBytes, result.perInstanceBytes) << '\t'
            << saturatingAdd(result.modelBytes,
                             saturatingMultiply(result.perInstanceBytes, 16U)) << '\t'
            << saturatingAdd(result.modelBytes,
                             saturatingMultiply(result.perInstanceBytes, 64U)) << '\n';
        for (const auto& [profile, issue] : result.issues) {
            issues << result.alias << '\t' << validation::toString(profile) << '\t'
                   << validation::toString(issue.severity) << '\t'
                   << validation::toString(issue.code) << '\t'
                   << validation::toString(issue.feature) << '\t'
                   << (issue.node.valid() ? std::to_string(issue.node.index()) : "-") << '\t'
                   << (issue.property.valid() ? std::to_string(issue.property.index()) : "-") << '\t'
                   << sanitize(issue.message) << '\n';
        }
    }

    features << "feature\tsupport\toccurrences\tassets\n";
    for (const auto& [feature, aggregate] : featureAggregates) {
        features << validation::toString(feature) << '\t'
                 << validation::toString(aggregate.support) << '\t'
                 << aggregate.occurrences << '\t' << aggregate.assets.size() << '\n';
    }

    decisions << "rank\tfamily\tscore\tblocked_assets\tassets\toccurrences\teffort\trationale\n";
    for (std::size_t index = 0U; index < priorities.size(); ++index) {
        const auto& priority = priorities[index];
        decisions << (index + 1U) << '\t' << lab::toString(priority.family) << '\t'
                  << priority.score << '\t' << priority.blockedAssets << '\t'
                  << priority.assets << '\t' << priority.occurrences << '\t'
                  << priority.effort << '\t' << sanitize(priority.rationale) << '\n';
    }

    summary
        << "AveMotion private corpus laboratory\n"
        << "schema=1\n"
        << "privacy=" << (options.includeNames ? "names-included" : "hashed-aliases") << '\n'
        << "files=" << results.size() << '\n'
        << "loaded=" << loaded << '\n'
        << "failed=" << failed << '\n'
        << "nativeDirect2DReady=" << native << '\n'
        << "fallbackAssets=" << fallback << '\n'
        << "unsupportedAssets=" << unsupported << '\n'
        << "telegramProfileAccepted=" << telegram << '\n'
        << "samplesPerAsset=" << options.samples << '\n'
        << "renderSize=" << options.renderSize << '\n'
        << "rankingFormula=blockedAssets*1000+occurrences*25+supportWeight-effort*50\n"
        << "rankingStatus=preliminary-until-representative-private-corpus\n";
    if (!priorities.empty()) {
        summary << "topCandidate=" << lab::toString(priorities.front().family) << '\n'
                << "topCandidateScore=" << priorities.front().score << '\n';
    } else {
        summary << "topCandidate=none\n";
    }
    summary << "sourceAssetsCopied=0\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        const auto options = parseOptions(argc, argv);
        const auto files = enumerateFiles(options);
        std::filesystem::remove_all(options.output);
        std::filesystem::create_directories(options.output);

        std::uintmax_t totalBytes = 0U;
        std::map<std::uint64_t, std::size_t> aliasCollisions;
        std::vector<AssetResult> results;
        results.reserve(files.size());
        for (const auto& path : files) {
            const auto bytes = readBytes(path, totalBytes, options.maximumTotalBytes);
            if (bytes.empty() && std::filesystem::file_size(path) != 0U) {
                AssetResult failed;
                failed.alias = "asset-unreadable-" + std::to_string(results.size());
                failed.sourceName = options.includeNames ? pathUtf8(path.filename()) : "redacted";
                failed.status = "read-failed";
                failed.error = "cannot read file";
                results.push_back(std::move(failed));
                continue;
            }
            const auto rawHash = core::fnv1a64(bytes);
            const auto ordinal = aliasCollisions[rawHash]++;
            results.push_back(analyzeAsset(options, path, bytes, ordinal));
            std::cout << '[' << results.size() << '/' << files.size() << "] "
                      << results.back().alias << ' ' << results.back().status << '\n';
        }
        writeReports(options, results);
        const bool failed = std::any_of(results.begin(), results.end(), [](const auto& result) {
            return result.status != "ok";
        });
        std::cout << "AveMotion corpus laboratory completed\n"
                  << "assets=" << results.size() << '\n'
                  << "output=" << pathUtf8(options.output) << '\n';
        return options.strict && failed ? 2 : 0;
    } catch (const std::exception& exception) {
        std::cerr << "AveMotion corpus lab exception: " << exception.what() << '\n';
        return 1;
    }
}
