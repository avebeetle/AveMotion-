#include "avemotion/model/AssetModel.hpp"
#include "avemotion/reference/UpstreamInfo.hpp"
#include "avemotion/runtime/Runtime.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <unordered_set>
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

std::vector<fs::path> corpus() {
    std::vector<fs::path> result;
    for (const auto& entry : fs::directory_iterator{AVEMOTION_CORPUS_DIR}) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            result.push_back(entry.path());
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

bool validRange(avemotion::model::IndexRange range, std::size_t size) {
    return static_cast<std::size_t>(range.first) <= size
        && static_cast<std::size_t>(range.count)
            <= size - static_cast<std::size_t>(range.first);
}

bool validValueRef(
    const avemotion::model::MotionAssetModel& model,
    avemotion::model::MotionValueRef ref) {
    using avemotion::model::PropertyValueType;
    if (!ref.valid()) return false;
    switch (ref.type) {
    case PropertyValueType::Scalar:
        return ref.index < model.scalarValues.size();
    case PropertyValueType::Vec2:
        return ref.index < model.vec2Values.size();
    case PropertyValueType::Color:
        return ref.index < model.colorValues.size();
    case PropertyValueType::Matrix3x2:
        return ref.index < model.matrixValues.size();
    case PropertyValueType::Shape:
        if (ref.index >= model.shapeValues.size()) return false;
        return validRange(model.shapeValues[ref.index].points, model.shapePoints.size());
    case PropertyValueType::Gradient:
        if (ref.index >= model.gradientValues.size()) return false;
        return validRange(
            model.gradientValues[ref.index].values,
            model.gradientFloats.size());
    case PropertyValueType::None:
        return false;
    }
    return false;
}

void validateParsedModel(
    const avemotion::model::MotionAssetModel& model,
    const std::string& assetName) {
    using namespace avemotion::model;
    const auto context = [&assetName](const std::string& message) {
        return assetName + ": " + message;
    };

    require(model.statistics.directParsedModel,
            context("direct parsed-model flag is not set"));
    require(model.parsedModelFingerprint != 0U,
            context("parsed-model fingerprint is zero"));
    require(!model.compositions.empty(), context("composition table is empty"));
    require(!model.sourceNodes.empty(), context("source node table is empty"));
    require(!model.properties.empty(), context("property table is empty"));

    const auto& stats = model.statistics;
    require(stats.compositionCount == model.compositions.size(),
            context("composition statistics differ"));
    require(stats.sourceNodeCount == model.sourceNodes.size(),
            context("source-node statistics differ"));
    require(stats.propertyCount == model.properties.size(),
            context("property statistics differ"));
    require(stats.staticPropertyCount + stats.animatedPropertyCount
                == stats.propertyCount,
            context("static/animated property statistics do not partition table"));
    require(stats.trackCount == model.tracks.size(),
            context("track statistics differ"));
    require(stats.segmentCount == model.segments.size(),
            context("segment statistics differ"));
    require(stats.scalarValueCount == model.scalarValues.size(),
            context("scalar-value statistics differ"));
    require(stats.vec2ValueCount == model.vec2Values.size(),
            context("Vec2-value statistics differ"));
    require(stats.colorValueCount == model.colorValues.size(),
            context("color-value statistics differ"));
    require(stats.matrixValueCount == model.matrixValues.size(),
            context("matrix-value statistics differ"));
    require(stats.shapeValueCount == model.shapeValues.size(),
            context("shape-value statistics differ"));
    require(stats.gradientValueCount == model.gradientValues.size(),
            context("gradient-value statistics differ"));

    for (std::size_t index = 0; index < model.compositions.size(); ++index) {
        const auto id = makeId<CompositionId>(index);
        const auto* composition = model.composition(id);
        require(composition != nullptr && composition->id == id,
                context("composition table is not dense"));
        require(model.sourceNode(composition->rootNode) != nullptr,
                context("composition root is invalid"));
        require(std::isfinite(composition->firstFrame)
                    && std::isfinite(composition->endFrame)
                    && std::isfinite(composition->frameRate)
                    && composition->endFrame >= composition->firstFrame
                    && composition->frameRate > 0.0
                    && composition->logicalWidth > 0U
                    && composition->logicalHeight > 0U,
                context("composition metadata is invalid"));
        const auto* root = model.sourceNode(composition->rootNode);
        require(root != nullptr
                    && root->kind == SourceNodeKind::Composition
                    && root->composition == composition->id
                    && !root->parent.valid(),
                context("composition root is inconsistent"));
    }

    std::size_t layerNodeCount = 0U;
    for (std::size_t index = 0; index < model.sourceNodes.size(); ++index) {
        const auto id = makeId<SourceNodeId>(index);
        const auto* node = model.sourceNode(id);
        require(node != nullptr && node->id == id,
                context("source node table is not dense"));
        require(model.composition(node->composition) != nullptr,
                context("source node composition is invalid"));
        require(validRange(node->children, model.sourceChildIds.size()),
                context("source child range is invalid"));
        require(validRange(node->properties, model.sourcePropertyIds.size()),
                context("source property range is invalid"));
        if (node->parent.valid()) {
            const auto* parent = model.sourceNode(node->parent);
            require(parent != nullptr && parent->composition == node->composition,
                    context("source structural parent is invalid"));
        }
        if (node->transformParent.valid()) {
            const auto* parent = model.sourceNode(node->transformParent);
            require(parent != nullptr && parent->kind == SourceNodeKind::Layer,
                    context("transform parent is not a layer"));
            require(parent->authoredLayerId == node->authoredParentLayerId,
                    context("transform parent authored ID differs"));
        } else if (node->kind == SourceNodeKind::Layer) {
            require(node->authoredParentLayerId < 0,
                    context("layer parent ID was not resolved"));
        }
        for (std::uint32_t offset = 0; offset < node->children.count; ++offset) {
            const auto childId = model.sourceChildIds[node->children.first + offset];
            const auto* child = model.sourceNode(childId);
            require(child != nullptr && child->parent == node->id,
                    context("source child range is inconsistent"));
        }
        std::uint16_t expectedDashIndex = 0U;
        for (std::uint32_t offset = 0; offset < node->properties.count; ++offset) {
            const auto propertyId =
                model.sourcePropertyIds[node->properties.first + offset];
            const auto* property = model.property(propertyId);
            require(property != nullptr && property->owner == node->id,
                    context("source property range is inconsistent"));
            if (property->semantic == PropertySemantic::StrokeDashValue) {
                require(property->semanticIndex == expectedDashIndex++,
                        context("dash property component indices are inconsistent"));
            } else {
                require(property->semanticIndex == 0U,
                        context("non-repeated property has a component index"));
            }
        }
        if (node->layerKind == SourceLayerKind::Precomposition) {
            require(model.composition(node->referencedComposition) != nullptr
                        && node->sourceAssetRefHash != 0U,
                    context("precomposition reference is invalid"));
        } else {
            require(!node->referencedComposition.valid(),
                    context("non-precomposition node has a composition reference"));
        }
        require(std::isfinite(node->miterLimit)
                    && std::isfinite(node->repeaterMaximumCopies)
                    && std::isfinite(node->solidColor.r)
                    && std::isfinite(node->solidColor.g)
                    && std::isfinite(node->solidColor.b)
                    && std::isfinite(node->solidColor.a)
                    && node->layerWidth >= 0
                    && node->layerHeight >= 0
                    && node->gradientColorPointCount >= 0,
                context("source node authored metadata is invalid"));
        if (node->kind == SourceNodeKind::Layer) {
            ++layerNodeCount;
            require(std::isfinite(node->inFrame)
                        && std::isfinite(node->outFrame)
                        && std::isfinite(node->startFrame)
                        && std::isfinite(node->timeStretch)
                        && node->outFrame >= node->inFrame,
                    context("layer timeline metadata is invalid"));
        }
    }
    require(layerNodeCount > 0U, context("direct source graph contains no layers"));

    for (std::size_t index = 0; index < model.properties.size(); ++index) {
        const auto id = makeId<PropertyId>(index);
        const auto* property = model.property(id);
        require(property != nullptr && property->id == id,
                context("property table is not dense"));
        require(model.sourceNode(property->owner) != nullptr,
                context("property owner is invalid"));
        require(property->valueType != PropertyValueType::None,
                context("property has no value type"));
        const bool isStatic = (property->flags & PropertyFlagStatic) != 0U;
        const bool isAnimated = (property->flags & PropertyFlagAnimated) != 0U;
        if ((property->flags & PropertyFlagSpatial) != 0U) {
            require(property->valueType == PropertyValueType::Vec2 && isAnimated,
                    context("spatial property is not an animated Vec2"));
        }
        require(isStatic != isAnimated,
                context("property must be exactly static or animated"));
        if (isStatic) {
            require(validValueRef(model, property->staticValue),
                    context("static property value is invalid"));
            require(property->staticValue.type == property->valueType,
                    context("static property value type differs"));
            require(!property->track.valid(),
                    context("static property unexpectedly references a track"));
        } else {
            const auto* track = model.track(property->track);
            require(track != nullptr && track->property == property->id,
                    context("animated property track is invalid"));
            require(!property->staticValue.valid(),
                    context("animated property unexpectedly stores a static value"));
        }
    }

    for (std::size_t index = 0; index < model.tracks.size(); ++index) {
        const auto id = makeId<TrackId>(index);
        const auto* track = model.track(id);
        require(track != nullptr && track->id == id,
                context("track table is not dense"));
        const auto* property = model.property(track->property);
        require(property != nullptr && property->track == track->id,
                context("track/property back-reference is invalid"));
        require(validRange(track->segments, model.segments.size())
                    && track->segments.count > 0U,
                context("track segment range is invalid"));
        require(std::isfinite(track->firstFrame)
                    && std::isfinite(track->endFrame)
                    && track->endFrame >= track->firstFrame,
                context("track timeline range is invalid"));

        double previousEnd = -std::numeric_limits<double>::infinity();
        bool hasSpatialSegment = false;
        for (std::uint32_t offset = 0; offset < track->segments.count; ++offset) {
            const auto segmentId = makeId<SegmentId>(track->segments.first + offset);
            const auto* segment = model.segment(segmentId);
            require(segment != nullptr && segment->id == segmentId
                        && segment->track == track->id,
                    context("segment identity/back-reference is invalid"));
            require(std::isfinite(segment->firstFrame)
                        && std::isfinite(segment->endFrame)
                        && segment->firstFrame >= previousEnd
                        && segment->endFrame >= segment->firstFrame,
                    context("segment times are invalid or overlapping"));
            previousEnd = segment->endFrame;
            require(validValueRef(model, segment->startValue)
                        && validValueRef(model, segment->endValue),
                    context("segment value reference is invalid"));
            require(segment->startValue.type == property->valueType
                        && segment->endValue.type == property->valueType,
                    context("segment value type differs from property"));
            if (segment->spatialInterpolation != SpatialInterpolation::None) {
                hasSpatialSegment = true;
                require(property->valueType == PropertyValueType::Vec2
                            && (property->flags & PropertyFlagSpatial) != 0U,
                        context("spatial segment belongs to a non-spatial property"));
            }
            require(std::isfinite(segment->temporalControl1.x)
                        && std::isfinite(segment->temporalControl1.y)
                        && std::isfinite(segment->temporalControl2.x)
                        && std::isfinite(segment->temporalControl2.y)
                        && std::isfinite(segment->spatialInTangent.x)
                        && std::isfinite(segment->spatialInTangent.y)
                        && std::isfinite(segment->spatialOutTangent.x)
                        && std::isfinite(segment->spatialOutTangent.y),
                    context("segment easing/spatial data is non-finite"));
        }
        require(hasSpatialSegment
                    == ((property->flags & PropertyFlagSpatial) != 0U),
                context("property spatial flag differs from its segments"));
    }

    for (const auto value : model.scalarValues) {
        require(std::isfinite(value), context("scalar value is non-finite"));
    }
    for (const auto value : model.vec2Values) {
        require(std::isfinite(value.x) && std::isfinite(value.y),
                context("Vec2 value is non-finite"));
    }
    for (const auto value : model.colorValues) {
        require(std::isfinite(value.r) && std::isfinite(value.g)
                    && std::isfinite(value.b) && std::isfinite(value.a),
                context("color value is non-finite"));
    }
    for (const auto value : model.matrixValues) {
        require(std::isfinite(value.m11) && std::isfinite(value.m12)
                    && std::isfinite(value.m21) && std::isfinite(value.m22)
                    && std::isfinite(value.dx) && std::isfinite(value.dy),
                context("matrix value is non-finite"));
    }
    for (const auto value : model.shapePoints) {
        require(std::isfinite(value.x) && std::isfinite(value.y),
                context("shape point is non-finite"));
    }
    for (const auto value : model.gradientFloats) {
        require(std::isfinite(value), context("gradient component is non-finite"));
    }
}

} // namespace

int main() {
    using namespace avemotion;
    require(reference::selectedUpstream().variant == "telegram",
            "parsed-model tests must run against Telegram baseline");

    runtime::Runtime runtime;
    const auto assets = corpus();
    require(!assets.empty(), "characterization corpus is empty");

    std::size_t totalStaticProperties = 0U;
    std::size_t totalAnimatedProperties = 0U;
    std::size_t totalTracks = 0U;
    std::size_t totalSegments = 0U;
    std::size_t totalTransformParents = 0U;
    std::size_t totalCompositions = 0U;
    std::size_t totalPrecompositionReferences = 0U;

    for (const auto& path : assets) {
        const auto json = readText(path);
        auto loaded = runtime.loadLottieJson(json, path.filename().string());
        require(static_cast<bool>(loaded), "asset load failed for " + path.string());
        const auto prepared = loaded.asset->prepareModel();
        require(static_cast<bool>(prepared),
                "parsed-model preparation failed for " + path.string()
                    + ": " + prepared.error);
        validateParsedModel(*prepared.model, path.filename().string());

        auto duplicate = runtime.loadLottieJson(json, path.filename().string());
        require(static_cast<bool>(duplicate), "duplicate asset load failed");
        const auto duplicateModel = duplicate.asset->prepareModel();
        require(static_cast<bool>(duplicateModel), "duplicate model preparation failed");
        require(duplicateModel.model->parsedModelFingerprint
                    == prepared.model->parsedModelFingerprint,
                "direct parsed-model fingerprint is not deterministic");

        auto historyAsset = runtime.loadLottieJson(
            json, path.filename().string() + "#history");
        require(static_cast<bool>(historyAsset), "history asset load failed");
        auto historyInstance = runtime.createInstance(historyAsset.asset);
        require(static_cast<bool>(historyInstance), "history instance creation failed");
        for (const auto position : {0.75, 0.125, 0.5, 0.0, 1.0}) {
            const auto scene = historyInstance.instance->evaluatePosition(
                position,
                historyAsset.asset->metadata().width,
                historyAsset.asset->metadata().height);
            require(static_cast<bool>(scene), "history evaluation failed");
        }
        const auto historyModel = historyAsset.asset->prepareModel();
        require(static_cast<bool>(historyModel), "history model preparation failed");
        require(historyModel.model->parsedModelFingerprint
                    == prepared.model->parsedModelFingerprint,
                "parsed model depends on prior evaluation history");
        require(historyModel.model->compositions.size()
                    == prepared.model->compositions.size()
                    && historyModel.model->sourceNodes.size()
                        == prepared.model->sourceNodes.size()
                    && historyModel.model->properties.size()
                        == prepared.model->properties.size()
                    && historyModel.model->tracks.size()
                        == prepared.model->tracks.size()
                    && historyModel.model->segments.size()
                        == prepared.model->segments.size(),
                "parsed model table sizes depend on evaluation history");

        totalStaticProperties += prepared.model->statistics.staticPropertyCount;
        totalAnimatedProperties += prepared.model->statistics.animatedPropertyCount;
        totalTracks += prepared.model->tracks.size();
        totalSegments += prepared.model->segments.size();
        totalCompositions += prepared.model->compositions.size();
        totalPrecompositionReferences += static_cast<std::size_t>(std::count_if(
            prepared.model->sourceNodes.begin(),
            prepared.model->sourceNodes.end(),
            [](const auto& node) { return node.referencedComposition.valid(); }));
        totalTransformParents += static_cast<std::size_t>(std::count_if(
            prepared.model->sourceNodes.begin(),
            prepared.model->sourceNodes.end(),
            [](const auto& node) { return node.transformParent.valid(); }));
    }

    require(totalStaticProperties > 0U, "corpus exposed no static properties");
    require(totalAnimatedProperties > 0U, "corpus exposed no animated properties");
    require(totalTracks > 0U && totalSegments >= totalTracks,
            "corpus exposed no usable tracks/segments");
    require(totalTransformParents > 0U,
            "corpus exposed no resolved layer transform-parent relation");
    require(totalCompositions > assets.size(),
            "corpus precompositions were not extracted as reusable tables");
    require(totalPrecompositionReferences > 0U,
            "corpus exposed no precomposition references");

    std::cout << "AveMotion direct parsed-model tests passed for " << assets.size()
              << " assets static-properties=" << totalStaticProperties
              << " animated-properties=" << totalAnimatedProperties
              << " tracks=" << totalTracks
              << " segments=" << totalSegments
              << " compositions=" << totalCompositions
              << " precomposition-refs=" << totalPrecompositionReferences
              << " transform-parents=" << totalTransformParents << '\n';
    return EXIT_SUCCESS;
}
