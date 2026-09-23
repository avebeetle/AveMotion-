#include "TelegramParsedModelBuilder.hpp"

#include "avemotion/core/Hash.hpp"
#include "avemotion/evaluation/PropertyEvaluator.hpp"
#include "avemotion/reference/UpstreamInfo.hpp"
#include "avemotion/runtime/Runtime.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::string readText(const fs::path& path) {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) return {};
    return {std::istreambuf_iterator<char>{stream},
            std::istreambuf_iterator<char>{}};
}

void appendValue(
    avemotion::core::Fnv1a64& hash,
    const avemotion::evaluation::MotionPropertyValue& value) {
    using avemotion::model::PropertyValueType;
    hash.appendU8(static_cast<std::uint8_t>(value.type));
    hash.appendU8(static_cast<std::uint8_t>(value.storage));
    hash.appendU8(static_cast<std::uint8_t>(value.assetReference.type));
    hash.appendU32(value.assetReference.index);
    switch (value.type) {
    case PropertyValueType::Scalar:
        hash.appendFloat(value.scalar);
        break;
    case PropertyValueType::Vec2:
        hash.appendFloat(value.vec2.x);
        hash.appendFloat(value.vec2.y);
        break;
    case PropertyValueType::Color:
        hash.appendFloat(value.color.r);
        hash.appendFloat(value.color.g);
        hash.appendFloat(value.color.b);
        hash.appendFloat(value.color.a);
        break;
    case PropertyValueType::Matrix3x2:
        hash.appendFloat(value.matrix.m11);
        hash.appendFloat(value.matrix.m12);
        hash.appendFloat(value.matrix.m21);
        hash.appendFloat(value.matrix.m22);
        hash.appendFloat(value.matrix.dx);
        hash.appendFloat(value.matrix.dy);
        break;
    case PropertyValueType::Shape:
    case PropertyValueType::Gradient:
    case PropertyValueType::None:
        break;
    }
}

void appendTransform(
    avemotion::core::Fnv1a64& hash,
    const avemotion::evaluation::EvaluatedNodeTransform& value) {
    hash.appendU32(value.node.value);
    hash.appendU8(static_cast<std::uint8_t>(value.state));
    hash.appendFloat(value.localMatrix.m11);
    hash.appendFloat(value.localMatrix.m12);
    hash.appendFloat(value.localMatrix.m21);
    hash.appendFloat(value.localMatrix.m22);
    hash.appendFloat(value.localMatrix.dx);
    hash.appendFloat(value.localMatrix.dy);
    hash.appendFloat(value.localOpacity);
}

void appendWorldTransform(
    avemotion::core::Fnv1a64& hash,
    const avemotion::evaluation::EvaluatedNodeTransform& value) {
    hash.appendU32(value.node.value);
    hash.appendU8(static_cast<std::uint8_t>(value.worldState));
    hash.appendFloat(value.worldMatrix.m11);
    hash.appendFloat(value.worldMatrix.m12);
    hash.appendFloat(value.worldMatrix.m21);
    hash.appendFloat(value.worldMatrix.m22);
    hash.appendFloat(value.worldMatrix.dx);
    hash.appendFloat(value.worldMatrix.dy);
    hash.appendFloat(value.worldOpacity);
}

bool near(float left, float right, float epsilon = 2.0e-5F) {
    return std::fabs(left - right) <= epsilon;
}

bool sameTransform(
    const avemotion::evaluation::EvaluatedNodeTransform& left,
    const avemotion::evaluation::EvaluatedNodeTransform& right) {
    return left.supported() && right.supported()
        && left.state == right.state
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
        && left.worldState == right.worldState
        && near(left.worldMatrix.m11, right.worldMatrix.m11)
        && near(left.worldMatrix.m12, right.worldMatrix.m12)
        && near(left.worldMatrix.m21, right.worldMatrix.m21)
        && near(left.worldMatrix.m22, right.worldMatrix.m22)
        && near(left.worldMatrix.dx, right.worldMatrix.dx)
        && near(left.worldMatrix.dy, right.worldMatrix.dy)
        && near(left.worldOpacity, right.worldOpacity);
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
    if (value.type != avemotion::model::PropertyValueType::Shape) return {};
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

void appendShape(
    avemotion::core::Fnv1a64& hash,
    const ShapeValueView& shape) {
    hash.appendU8(shape.valid ? 1U : 0U);
    if (!shape.valid) return;
    hash.appendU8(shape.closed ? 1U : 0U);
    hash.appendU8(shape.topologyTruncated ? 1U : 0U);
    hash.appendU32(static_cast<std::uint32_t>(shape.points.size()));
    for (const auto point : shape.points) {
        hash.appendFloat(point.x);
        hash.appendFloat(point.y);
    }
}

bool sameShape(const ShapeValueView& left, const ShapeValueView& right) {
    if (!left.valid || !right.valid
        || left.closed != right.closed
        || left.points.size() != right.points.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.points.size(); ++index) {
        if (!sameFloat(left.points[index].x, right.points[index].x)
            || !sameFloat(left.points[index].y, right.points[index].y)) {
            return false;
        }
    }
    return true;
}

void usage(const char* executable) {
    std::cerr << "Usage: " << executable
              << " --output <directory> <asset.json>...\n";
}

} // namespace

int main(int argc, char** argv) {
    fs::path output;
    std::vector<fs::path> assets;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--output" && index + 1 < argc) {
            output = argv[++index];
        } else if (!argument.empty() && argument.front() == '-') {
            usage(argv[0]);
            return EXIT_FAILURE;
        } else {
            assets.emplace_back(argument);
        }
    }
    if (output.empty() || assets.empty()) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    fs::create_directories(output);
    std::ofstream manifest{output / "property_manifest.tsv", std::ios::binary};
    if (!manifest) {
        std::cerr << "Unable to create property manifest\n";
        return EXIT_FAILURE;
    }
    manifest << "variant\tasset\tsample\tframe\tproperties\tcompared"
                "\tunsupported\tspatial_properties\tshapes\tshape_points"
                "\tshape_truncations\tchanged_shapes\ttransforms"
                "\tworld_transforms\tproperty_hash\toracle_hash"
                "\ttransform_hash\toracle_transform_hash"
                "\tworld_transform_hash\toracle_world_transform_hash"
                "\tcursor_hits\tadjacent_moves\tbinary_searches"
                "\tcubic_samples\tspatial_samples\tspatial_angle_samples"
                "\tspatial_length_iterations\tspatial_length_max"
                "\tworld_transform_changes\n";

    const auto upstream = avemotion::reference::selectedUpstream();
    if (upstream.variant != "telegram") {
        std::cerr << "Property characterizer requires Telegram rlottie\n";
        return EXIT_FAILURE;
    }
    avemotion::runtime::Runtime runtime;
    constexpr std::array<double, 5> positions{0.0, 0.25, 0.5, 0.75, 1.0};
    for (const auto& path : assets) {
        const auto json = readText(path);
        auto loaded = runtime.loadLottieJson(json, path.filename().string());
        if (!loaded) {
            std::cerr << "Unable to load " << path << ": "
                      << loaded.error.message << '\n';
            return EXIT_FAILURE;
        }
        const auto prepared = loaded.asset->prepareModel();
        if (!prepared) {
            std::cerr << "Unable to prepare model for " << path << ": "
                      << prepared.error << '\n';
            return EXIT_FAILURE;
        }
        avemotion::evaluation::PropertyEvaluator evaluator{prepared.model};
        if (!evaluator.valid()) {
            std::cerr << "Unable to create evaluator: "
                      << evaluator.errorMessage() << '\n';
            return EXIT_FAILURE;
        }
        avemotion::evaluation::PropertyEvaluationWorkspace workspace;
        evaluator.prepare(workspace);
        const auto total = loaded.asset->metadata().totalFrames;
        for (std::size_t sample = 0; sample < positions.size(); ++sample) {
            const auto position = positions[sample];
            const auto frame = total == 0U
                ? 0U
                : static_cast<std::size_t>(
                    std::llround(position * static_cast<double>(total - 1U)));
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
            const auto oracle =
                avemotion::model::detail::evaluateTelegramParsedProperties(
                    json,
                    debugName + "-property-characterizer",
                    descriptor,
                    static_cast<int>(frame));
            if (!oracle) {
                std::cerr << "Oracle failed for " << path << ": "
                          << oracle.error << '\n';
                return EXIT_FAILURE;
            }
            const auto evaluated = evaluator.evaluate(
                static_cast<double>(frame), workspace);
            if (!evaluated) {
                std::cerr << "Evaluation failed for " << path << '\n';
                return EXIT_FAILURE;
            }

            avemotion::core::Fnv1a64 propertyHash;
            avemotion::core::Fnv1a64 oracleHash;
            avemotion::core::Fnv1a64 transformHash;
            avemotion::core::Fnv1a64 oracleTransformHash;
            avemotion::core::Fnv1a64 worldTransformHash;
            avemotion::core::Fnv1a64 oracleWorldTransformHash;
            std::size_t compared = 0U;
            std::size_t unsupported = 0U;
            std::size_t spatialProperties = 0U;
            std::size_t shapeProperties = 0U;
            std::size_t shapePointCount = 0U;
            for (std::size_t index = 0; index < evaluated.properties.size(); ++index) {
                const auto& property = prepared.model->properties[index];
                const auto& actual = evaluated.properties[index].value;
                const auto& expected = oracle.properties[index];
                appendValue(propertyHash, actual);
                appendValue(oracleHash, expected);
                if ((property.flags & avemotion::model::PropertyFlagAnimated) != 0U
                    && property.valueType
                        == avemotion::model::PropertyValueType::Shape) {
                    const auto actualShape = shapeValue(
                        *prepared.model, actual, evaluated.shapes, evaluated.shapePoints);
                    const auto expectedShape = shapeValue(
                        *prepared.model, expected, oracle.shapes, oracle.shapePoints);
                    appendShape(propertyHash, actualShape);
                    appendShape(oracleHash, expectedShape);
                    if (!sameShape(actualShape, expectedShape)) {
                        std::cerr << "Shape mismatch: "
                                  << path.filename().string() << " frame=" << frame
                                  << " property=" << index << '\n';
                        return EXIT_FAILURE;
                    }
                    ++compared;
                    ++shapeProperties;
                    shapePointCount += actualShape.points.size();
                    continue;
                }
                const bool complexUnsupported =
                    (property.flags & avemotion::model::PropertyFlagAnimated) != 0U
                    && property.valueType
                        == avemotion::model::PropertyValueType::Gradient;
                if (complexUnsupported) {
                    ++unsupported;
                    continue;
                }
                if ((property.flags & avemotion::model::PropertyFlagSpatial) != 0U) {
                    ++spatialProperties;
                }
                if (!sameValue(actual, expected)) {
                    std::cerr << "Property mismatch: " << path.filename().string()
                              << " frame=" << frame << " property=" << index
                              << " semantic=" << static_cast<int>(property.semantic)
                              << " flags=" << property.flags << '\n';
                    return EXIT_FAILURE;
                }
                ++compared;
            }
            std::size_t transforms = 0U;
            std::size_t worldTransforms = 0U;
            for (std::size_t index = 0; index < evaluated.nodeTransforms.size(); ++index) {
                const auto& actual = evaluated.nodeTransforms[index];
                const auto& expected = oracle.nodeTransforms[index];
                if (actual.supported() && expected.supported()) {
                    if (!sameTransform(actual, expected)) {
                        std::cerr << "Local transform mismatch: "
                                  << path.filename().string() << " frame=" << frame
                                  << " node=" << index << '\n';
                        return EXIT_FAILURE;
                    }
                    appendTransform(transformHash, actual);
                    appendTransform(oracleTransformHash, expected);
                    ++transforms;
                }
                if (actual.worldSupported() && expected.worldSupported()) {
                    if (!sameWorldTransform(actual, expected)) {
                        std::cerr << "World transform mismatch: "
                                  << path.filename().string() << " frame=" << frame
                                  << " node=" << index << '\n';
                        return EXIT_FAILURE;
                    }
                    appendWorldTransform(worldTransformHash, actual);
                    appendWorldTransform(oracleWorldTransformHash, expected);
                    ++worldTransforms;
                }
            }
            manifest << upstream.variant << '\t'
                     << path.filename().string() << '\t'
                     << sample << '\t' << frame << '\t'
                     << evaluated.properties.size() << '\t'
                     << compared << '\t' << unsupported << '\t'
                     << spatialProperties << '\t'
                     << shapeProperties << '\t'
                     << shapePointCount << '\t'
                     << evaluated.statistics.shapeTopologyTruncations << '\t'
                     << evaluated.statistics.changedShapes << '\t'
                     << transforms << '\t' << worldTransforms << '\t'
                     << avemotion::core::formatHash(propertyHash.value()) << '\t'
                     << avemotion::core::formatHash(oracleHash.value()) << '\t'
                     << avemotion::core::formatHash(transformHash.value()) << '\t'
                     << avemotion::core::formatHash(oracleTransformHash.value()) << '\t'
                     << avemotion::core::formatHash(worldTransformHash.value()) << '\t'
                     << avemotion::core::formatHash(oracleWorldTransformHash.value()) << '\t'
                     << evaluated.statistics.cursorHits << '\t'
                     << evaluated.statistics.adjacentCursorMoves << '\t'
                     << evaluated.statistics.binarySearches << '\t'
                     << evaluated.statistics.cubicSamples << '\t'
                     << evaluated.statistics.spatialSamples << '\t'
                     << evaluated.statistics.spatialAngleSamples << '\t'
                     << evaluated.statistics.spatialLengthSearchIterations << '\t'
                     << evaluated.statistics.spatialLengthSearchMaximum << '\t'
                     << evaluated.statistics.worldTransformsChanged << '\n';
        }
    }
    std::cout << "Recorded canonical property evaluation for " << assets.size()
              << " assets\n";
    return EXIT_SUCCESS;
}
