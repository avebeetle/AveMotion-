#include "TelegramParsedModelBuilder.hpp"

#include "avemotion/evaluation/PropertyEvaluator.hpp"
#include "avemotion/reference/UpstreamInfo.hpp"
#include "avemotion/runtime/Runtime.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
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
    if (!condition) fail(message);
}

std::string readText(const fs::path& path) {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) fail("unable to open " + path.string());
    return {std::istreambuf_iterator<char>{stream},
            std::istreambuf_iterator<char>{}};
}

std::vector<fs::path> corpus() {
    std::vector<fs::path> result;
    for (const auto& entry : fs::directory_iterator{AVEMOTION_CORPUS_DIR}) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            result.push_back(entry.path());
        }
    }
    const auto autoOrientFixture =
        fs::path{AVEMOTION_CORPUS_DIR}.parent_path()
        / "fixtures" / "auto_orient_spatial.json";
    if (fs::exists(autoOrientFixture)) result.push_back(autoOrientFixture);
    std::sort(result.begin(), result.end());
    return result;
}

bool sameFloat(float left, float right) {
    if (left == 0.0F && right == 0.0F) return true;
    return left == right;
}

bool sameValue(
    const avemotion::evaluation::MotionPropertyValue& left,
    const avemotion::evaluation::MotionPropertyValue& right) {
    using avemotion::model::PropertyValueType;
    if (left.type != right.type || left.storage != right.storage) return false;
    if (left.storage == avemotion::evaluation::PropertyStorageKind::AssetReference) {
        return left.assetReference.type == right.assetReference.type
            && left.assetReference.index == right.assetReference.index;
    }
    if (left.storage != avemotion::evaluation::PropertyStorageKind::Materialized) {
        return true;
    }
    switch (left.type) {
    case PropertyValueType::Scalar:
        return sameFloat(left.scalar, right.scalar);
    case PropertyValueType::Vec2:
        return sameFloat(left.vec2.x, right.vec2.x)
            && sameFloat(left.vec2.y, right.vec2.y);
    case PropertyValueType::Color:
        return sameFloat(left.color.r, right.color.r)
            && sameFloat(left.color.g, right.color.g)
            && sameFloat(left.color.b, right.color.b)
            && sameFloat(left.color.a, right.color.a);
    case PropertyValueType::Matrix3x2:
        return sameFloat(left.matrix.m11, right.matrix.m11)
            && sameFloat(left.matrix.m12, right.matrix.m12)
            && sameFloat(left.matrix.m21, right.matrix.m21)
            && sameFloat(left.matrix.m22, right.matrix.m22)
            && sameFloat(left.matrix.dx, right.matrix.dx)
            && sameFloat(left.matrix.dy, right.matrix.dy);
    case PropertyValueType::Shape:
    case PropertyValueType::Gradient:
    case PropertyValueType::None:
        return true;
    }
    return false;
}

struct ShapeValueView final {
    std::span<const avemotion::model::MotionVec2Value> points;
    bool closed = false;
    bool topologyTruncated = false;
    bool valid = false;
};

ShapeValueView shapeValue(
    const avemotion::model::MotionAssetModel& model,
    const avemotion::evaluation::MotionPropertyValue& value,
    std::span<const avemotion::evaluation::EvaluatedShape> shapes,
    std::span<const avemotion::model::MotionVec2Value> shapePoints) {
    using avemotion::evaluation::PropertyStorageKind;
    using avemotion::model::PropertyValueType;
    if (value.type != PropertyValueType::Shape) return {};
    if (value.storage == PropertyStorageKind::AssetReference) {
        if (!value.assetReference.valid()
            || value.assetReference.index >= model.shapeValues.size()) {
            return {};
        }
        const auto& shape = model.shapeValues[value.assetReference.index];
        if (static_cast<std::size_t>(shape.points.first) + shape.points.count
            > model.shapePoints.size()) {
            return {};
        }
        return {
            shape.points.count == 0U
                ? std::span<const avemotion::model::MotionVec2Value>{}
                : std::span<const avemotion::model::MotionVec2Value>{
                    model.shapePoints.data() + shape.points.first,
                    shape.points.count},
            shape.closed,
            false,
            true,
        };
    }
    if (value.storage != PropertyStorageKind::Materialized
        || value.shapeSlot >= shapes.size()) {
        return {};
    }
    const auto& shape = shapes[value.shapeSlot];
    if (static_cast<std::size_t>(shape.firstPoint) + shape.pointCount
        > shapePoints.size()) {
        return {};
    }
    return {
        shapePoints.subspan(shape.firstPoint, shape.pointCount),
        shape.closed,
        shape.topologyTruncated,
        true,
    };
}

bool near(float left, float right, float epsilon = 2.0e-5F) {
    return std::fabs(left - right) <= epsilon;
}

bool sameTransform(
    const avemotion::evaluation::EvaluatedNodeTransform& left,
    const avemotion::evaluation::EvaluatedNodeTransform& right) {
    return left.supported() && right.supported()
        && near(left.localMatrix.m11, right.localMatrix.m11)
        && near(left.localMatrix.m12, right.localMatrix.m12)
        && near(left.localMatrix.m21, right.localMatrix.m21)
        && near(left.localMatrix.m22, right.localMatrix.m22)
        && near(left.localMatrix.dx, right.localMatrix.dx)
        && near(left.localMatrix.dy, right.localMatrix.dy)
        && near(left.localOpacity, right.localOpacity);
}

bool sameWorldTransform(
    const avemotion::evaluation::EvaluatedNodeTransform& left,
    const avemotion::evaluation::EvaluatedNodeTransform& right) {
    return left.worldSupported() && right.worldSupported()
        && near(left.worldMatrix.m11, right.worldMatrix.m11)
        && near(left.worldMatrix.m12, right.worldMatrix.m12)
        && near(left.worldMatrix.m21, right.worldMatrix.m21)
        && near(left.worldMatrix.m22, right.worldMatrix.m22)
        && near(left.worldMatrix.dx, right.worldMatrix.dx)
        && near(left.worldMatrix.dy, right.worldMatrix.dy)
        && near(left.worldOpacity, right.worldOpacity);
}

std::vector<std::size_t> sampleFrames(
    const avemotion::model::MotionAssetModel& model,
    std::size_t totalFrames) {
    std::vector<std::size_t> result;
    const auto add = [&](double frame) {
        if (!std::isfinite(frame)) return;
        if (totalFrames == 0U) {
            result.push_back(0U);
            return;
        }
        const double maximum = static_cast<double>(totalFrames - 1U);
        const double clamped = std::clamp(frame, 0.0, maximum);
        result.push_back(static_cast<std::size_t>(std::llround(clamped)));
    };

    add(0.0);
    if (totalFrames > 1U) add(1.0);
    if (totalFrames > 0U) {
        const double last = static_cast<double>(totalFrames - 1U);
        add(last * 0.25);
        add(last * 0.5);
        add(last * 0.75);
        add(last);
    }

    for (const auto& segment : model.segments) {
        if (!segment.present) continue;
        add(std::floor(segment.firstFrame));
        add(std::ceil(segment.firstFrame));
        add((segment.firstFrame + segment.endFrame) * 0.5);
        add(std::floor(segment.endFrame));
        if (segment.endFrame > segment.firstFrame) {
            add(std::ceil(segment.endFrame) - 1.0);
        }
    }

    std::uint64_t random = model.sourceAssetHash ^ 0x9e3779b97f4a7c15ULL;
    for (int index = 0; index < 12; ++index) {
        random ^= random << 13U;
        random ^= random >> 7U;
        random ^= random << 17U;
        if (totalFrames > 0U) {
            result.push_back(static_cast<std::size_t>(
                random % static_cast<std::uint64_t>(totalFrames)));
        }
    }

    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

struct OracleSample final {
    std::size_t frame = 0U;
    std::vector<avemotion::evaluation::MotionPropertyValue> properties;
    std::vector<avemotion::evaluation::EvaluatedNodeTransform> transforms;
    std::vector<avemotion::evaluation::EvaluatedShape> shapes;
    std::vector<avemotion::model::MotionVec2Value> shapePoints;
};

struct ComparisonCounters final {
    std::size_t properties = 0U;
    std::size_t spatialProperties = 0U;
    std::size_t transforms = 0U;
    std::size_t worldTransforms = 0U;
    std::size_t shapes = 0U;
    std::size_t shapePoints = 0U;
    std::size_t skippedComplex = 0U;
    std::size_t skippedTransforms = 0U;
    std::size_t skippedWorldTransforms = 0U;
};

void compareEvaluation(
    const fs::path& path,
    std::size_t frame,
    const avemotion::model::MotionAssetModel& model,
    const avemotion::evaluation::PropertyEvaluationView& evaluated,
    const OracleSample& oracle,
    ComparisonCounters& counters,
    bool countUnsupported) {
    require(static_cast<bool>(evaluated),
            path.filename().string() + ": evaluation failed");
    require(evaluated.properties.size() == oracle.properties.size(),
            path.filename().string() + ": property count differs");
    for (std::size_t index = 0; index < evaluated.properties.size(); ++index) {
        const auto& property = model.properties[index];
        const auto& actual = evaluated.properties[index].value;
        const auto& expected = oracle.properties[index];
        if ((property.flags & avemotion::model::PropertyFlagAnimated) != 0U
            && (property.flags & avemotion::model::PropertyFlagSpatial) != 0U) {
            require(actual.materialized()
                        && actual.type == avemotion::model::PropertyValueType::Vec2,
                    path.filename().string()
                        + ": spatial property was not materialized at property "
                        + std::to_string(index));
            if (!sameValue(actual, expected)) {
                std::cerr << path.filename().string()
                          << ": spatial parity mismatch at frame " << frame
                          << ", property " << index
                          << ", actual=(" << actual.vec2.x << ',' << actual.vec2.y
                          << "), expected=(" << expected.vec2.x << ','
                          << expected.vec2.y << ")\n";
                fail("spatial property parity mismatch");
            }
            ++counters.properties;
            if (countUnsupported) ++counters.spatialProperties;
            continue;
        }
        if ((property.flags & avemotion::model::PropertyFlagAnimated) != 0U
            && property.valueType == avemotion::model::PropertyValueType::Shape) {
            const auto actualShape = shapeValue(
                model, actual, evaluated.shapes, evaluated.shapePoints);
            const auto expectedShape = shapeValue(
                model, expected, oracle.shapes, oracle.shapePoints);
            require(actualShape.valid && expectedShape.valid,
                    path.filename().string()
                        + ": animated shape was not materialized at property "
                        + std::to_string(index));
            require(actualShape.closed == expectedShape.closed,
                    path.filename().string()
                        + ": animated shape closed flag differs at frame "
                        + std::to_string(frame) + ", property "
                        + std::to_string(index));
            require(actualShape.points.size() == expectedShape.points.size(),
                    path.filename().string()
                        + ": animated shape point count differs at frame "
                        + std::to_string(frame) + ", property "
                        + std::to_string(index));
            for (std::size_t point = 0; point < actualShape.points.size(); ++point) {
                require(
                    sameFloat(actualShape.points[point].x, expectedShape.points[point].x)
                        && sameFloat(
                            actualShape.points[point].y, expectedShape.points[point].y),
                    path.filename().string()
                        + ": animated shape point differs at frame "
                        + std::to_string(frame) + ", property "
                        + std::to_string(index) + ", point "
                        + std::to_string(point));
            }
            ++counters.properties;
            if (countUnsupported) {
                ++counters.shapes;
                counters.shapePoints += actualShape.points.size();
            }
            continue;
        }
        if ((property.flags & avemotion::model::PropertyFlagAnimated) != 0U
            && property.valueType == avemotion::model::PropertyValueType::Gradient) {
            if (countUnsupported) ++counters.skippedComplex;
            require(actual.storage
                        == avemotion::evaluation::PropertyStorageKind::Unsupported,
                    path.filename().string()
                        + ": animated gradient was approximated at property "
                        + std::to_string(index));
            continue;
        }
        require(sameValue(actual, expected),
                path.filename().string() + ": property parity mismatch at frame "
                    + std::to_string(frame) + ", property "
                    + std::to_string(index));
        ++counters.properties;
    }

    require(evaluated.nodeTransforms.size() == oracle.transforms.size(),
            path.filename().string() + ": transform count differs");
    for (std::size_t index = 0; index < evaluated.nodeTransforms.size(); ++index) {
        const auto& actual = evaluated.nodeTransforms[index];
        const auto& expected = oracle.transforms[index];
        if (actual.state == avemotion::evaluation::NodeTransformState::Unsupported) {
            ++counters.skippedTransforms;
            continue;
        }
        require(actual.state == expected.state,
                path.filename().string() + ": transform support/state mismatch at frame "
                    + std::to_string(frame) + ", node "
                    + std::to_string(index));
        if (!actual.supported()) continue;
        require(sameTransform(actual, expected),
                path.filename().string() + ": transform parity mismatch at frame "
                    + std::to_string(frame) + ", node "
                    + std::to_string(index));
        ++counters.transforms;
        if (!actual.worldSupported() || !expected.worldSupported()) {
            ++counters.skippedWorldTransforms;
            continue;
        }
        require(actual.worldState == expected.worldState,
                path.filename().string()
                    + ": world-transform state mismatch at frame "
                    + std::to_string(frame) + ", node "
                    + std::to_string(index));
        require(sameWorldTransform(actual, expected),
                path.filename().string()
                    + ": world-transform parity mismatch at frame "
                    + std::to_string(frame) + ", node "
                    + std::to_string(index));
        ++counters.worldTransforms;
    }
}

} // namespace

int main() {
    require(avemotion::reference::selectedUpstream().variant == "telegram",
            "property parity requires Telegram rlottie");
    avemotion::runtime::Runtime runtime;
    ComparisonCounters counters;
    std::size_t assetsVisited = 0U;
    std::size_t uniqueSamples = 0U;
    std::size_t directBinarySearches = 0U;
    std::size_t forwardCursorFastPaths = 0U;
    std::size_t reverseCursorFastPaths = 0U;
    std::size_t spatialLengthSearchIterations = 0U;
    std::size_t spatialLengthSearchMaximum = 0U;

    for (const auto& path : corpus()) {
        const auto json = readText(path);
        auto loaded = runtime.loadLottieJson(json, path.filename().string());
        require(static_cast<bool>(loaded),
                path.filename().string() + ": load failed");
        const auto prepared = loaded.asset->prepareModel();
        require(static_cast<bool>(prepared),
                path.filename().string() + ": model preparation failed");
        avemotion::evaluation::PropertyEvaluator evaluator{prepared.model};
        require(evaluator.valid(), std::string{evaluator.errorMessage()});

        const auto frames = sampleFrames(
            *prepared.model, loaded.asset->metadata().totalFrames);
        require(!frames.empty(), path.filename().string() + ": no sample frames");
        uniqueSamples += frames.size();
        ++assetsVisited;

        const std::string debugName = path.filename().string();
        const avemotion::model::detail::AssetModelDescriptor descriptor{
            .assetHandle = loaded.asset->handle(),
            .sourceAssetHash = loaded.asset->metadata().sourceHash,
            .logicalWidth = loaded.asset->metadata().width,
            .logicalHeight = loaded.asset->metadata().height,
            .frameRate = loaded.asset->metadata().frameRate,
            .totalFrames = loaded.asset->metadata().totalFrames,
            .debugName = debugName.c_str(),
        };

        std::vector<OracleSample> oracleSamples;
        oracleSamples.reserve(frames.size());
        for (const auto frame : frames) {
            const auto oracle =
                avemotion::model::detail::evaluateTelegramParsedProperties(
                    json,
                    debugName + "-property-oracle",
                    descriptor,
                    static_cast<int>(frame));
            require(static_cast<bool>(oracle),
                    debugName + ": oracle failed: " + oracle.error);
            require(oracle.model->parsedModelFingerprint
                        == prepared.model->parsedModelFingerprint,
                    debugName + ": oracle model differs");
            oracleSamples.push_back({
                frame,
                oracle.properties,
                oracle.nodeTransforms,
                oracle.shapes,
                oracle.shapePoints,
            });
        }

        avemotion::evaluation::PropertyEvaluationWorkspace forward;
        evaluator.prepare(forward);
        const auto forwardStorage = forward.storageGeneration();
        for (std::size_t index = 0; index < frames.size(); ++index) {
            const auto evaluated = evaluator.evaluate(
                static_cast<double>(frames[index]), forward);
            compareEvaluation(
                path,
                frames[index],
                *prepared.model,
                evaluated,
                oracleSamples[index],
                counters,
                true);
            forwardCursorFastPaths += evaluated.statistics.cursorHits
                + evaluated.statistics.adjacentCursorMoves;
            spatialLengthSearchIterations +=
                evaluated.statistics.spatialLengthSearchIterations;
            spatialLengthSearchMaximum = std::max(
                spatialLengthSearchMaximum,
                evaluated.statistics.spatialLengthSearchMaximum);
            require(forward.storageGeneration() == forwardStorage,
                    debugName + ": forward traversal resized workspace storage");
        }
        const auto repeated = evaluator.evaluate(
            static_cast<double>(frames.back()), forward);
        require(static_cast<bool>(repeated), debugName + ": repeat failed");
        require(repeated.statistics.changedProperties == 0U
                    && repeated.statistics.changedShapes == 0U
                    && repeated.statistics.transformsChanged == 0U
                    && repeated.statistics.worldTransformsChanged == 0U,
                debugName + ": repeated exact time was not stable");

        avemotion::evaluation::PropertyEvaluationWorkspace reverse;
        evaluator.prepare(reverse);
        const auto reverseStorage = reverse.storageGeneration();
        for (std::size_t offset = frames.size(); offset > 0U; --offset) {
            const auto index = offset - 1U;
            const auto evaluated = evaluator.evaluate(
                static_cast<double>(frames[index]), reverse);
            compareEvaluation(
                path,
                frames[index],
                *prepared.model,
                evaluated,
                oracleSamples[index],
                counters,
                false);
            reverseCursorFastPaths += evaluated.statistics.cursorHits
                + evaluated.statistics.adjacentCursorMoves;
            require(reverse.storageGeneration() == reverseStorage,
                    debugName + ": reverse traversal resized workspace storage");
        }

        avemotion::evaluation::PropertyEvaluationWorkspace direct;
        evaluator.prepare(direct);
        const auto directStorage = direct.storageGeneration();
        for (std::size_t index = 0; index < frames.size(); ++index) {
            direct.resetHistory();
            const auto evaluated = evaluator.evaluate(
                static_cast<double>(frames[index]), direct);
            compareEvaluation(
                path,
                frames[index],
                *prepared.model,
                evaluated,
                oracleSamples[index],
                counters,
                false);
            directBinarySearches += evaluated.statistics.binarySearches;
            require(direct.storageGeneration() == directStorage,
                    debugName + ": direct seek resized workspace storage");
        }
    }

    require(assetsVisited == 9U, "property parity did not visit corpus and fixtures");
    require(uniqueSamples > 100U, "property parity sampled too few unique times");
    require(counters.properties > 8'000U,
            "property parity corpus compared too few values");
    require(counters.spatialProperties > 0U,
            "spatial parity corpus compared no spatial values");
    require(counters.shapes > 0U && counters.shapePoints > 0U,
            "shape parity corpus compared no animated path values");
    require(counters.transforms > 1'000U,
            "transform parity corpus compared too few values");
    require(counters.worldTransforms > 1'000U,
            "world-transform parity corpus compared too few values");
    require(spatialLengthSearchMaximum > 80U,
            "spatial parity did not exercise the long Telegram length search");
    require(directBinarySearches > 0U,
            "direct random seek never used binary-search fallback");
    require(forwardCursorFastPaths > 0U,
            "forward traversal never used track cursors");
    require(reverseCursorFastPaths > 0U,
            "reverse traversal never used track cursors");

    std::cout << "Telegram parity passed: assets=" << assetsVisited
              << ", samples=" << uniqueSamples
              << ", properties=" << counters.properties
              << ", spatialProperties=" << counters.spatialProperties
              << ", shapes=" << counters.shapes
              << ", shapePoints=" << counters.shapePoints
              << ", transforms=" << counters.transforms
              << ", worldTransforms=" << counters.worldTransforms
              << ", skippedComplex=" << counters.skippedComplex
              << ", skippedTransforms=" << counters.skippedTransforms
              << ", skippedWorldTransforms=" << counters.skippedWorldTransforms
              << ", directBinarySearches=" << directBinarySearches
              << ", forwardCursorFastPaths=" << forwardCursorFastPaths
              << ", reverseCursorFastPaths=" << reverseCursorFastPaths
              << ", spatialLengthIterations=" << spatialLengthSearchIterations
              << ", spatialLengthMaximum=" << spatialLengthSearchMaximum << '\n';
    return EXIT_SUCCESS;
}
