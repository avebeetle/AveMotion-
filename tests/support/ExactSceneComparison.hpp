#pragma once

#include "avemotion/model/AssetModel.hpp"
#include "avemotion/runtime/RecordingBackend.hpp"

#include <stdexcept>
#include <string>
#include <unordered_map>

namespace avemotion::test {

// Field-by-field audit of EvaluatedScene.hpp (and its pointed-to values).
// Deliberate exclusions: asset/instance handles and instance identity,
// evaluationSequence, changes, and drawItem.upstreamChangeBits. The caller
// checks sequence and repeated-frame changes separately. No epsilon, struct
// memcmp, pointer-address equality across scenes, or fingerprint shortcut.
class ExactSceneComparison final {
public:
    [[nodiscard]] std::string difference(
        const runtime::EvaluatedScene& expected,
        const runtime::EvaluatedScene& actual) {
        expectedAliases_.clear();
        actualAliases_.clear();
        try {
            compare(expected, actual, "scene");
            const runtime::RecordingBackend recorder;
            compare(recorder.record(expected), recorder.record(actual), "record");
        } catch (const Mismatch& mismatch) {
            return mismatch.what();
        }
        return {};
    }

private:
    struct Mismatch final : std::runtime_error {
        using std::runtime_error::runtime_error;
    };
    std::unordered_map<const void*, const void*> expectedAliases_;
    std::unordered_map<const void*, const void*> actualAliases_;

    template <typename T>
    void compare(const T& expected, const T& actual, const std::string& where) {
        if (!(expected == actual)) throw Mismatch{where + " differs"};
    }

    template <typename T>
    void compare(const std::vector<T>& expected, const std::vector<T>& actual,
                 const std::string& where) {
        compare(expected.size(), actual.size(), where + ".size");
        for (std::size_t index = 0; index < expected.size(); ++index) {
            compare(expected[index], actual[index],
                    where + "[" + std::to_string(index) + "]");
        }
    }

    template <typename T, std::size_t N>
    void compare(const T (&expected)[N], const T (&actual)[N], const std::string& where) {
        for (std::size_t index = 0; index < N; ++index) {
            compare(expected[index], actual[index],
                    where + "[" + std::to_string(index) + "]");
        }
    }

    template <typename T>
    void compare(const std::optional<T>& expected, const std::optional<T>& actual,
                 const std::string& where) {
        compare(expected.has_value(), actual.has_value(), where + ".present");
        if (expected) compare(*expected, *actual, where);
    }

    template <typename T>
    void compare(const std::shared_ptr<const T>& expected,
                 const std::shared_ptr<const T>& actual, const std::string& where) {
        compare(static_cast<bool>(expected), static_cast<bool>(actual), where + ".present");
        if (!expected) return;
        // Bijection of object identities *within* the two scenes: equal values
        // alone must not hide a lost alias or an unintended newly shared object.
        const auto [forward, insertedForward] = expectedAliases_.emplace(expected.get(), actual.get());
        const auto [reverse, insertedReverse] = actualAliases_.emplace(actual.get(), expected.get());
        if ((!insertedForward && forward->second != actual.get())
            || (!insertedReverse && reverse->second != expected.get())) {
            throw Mismatch{where + ".alias relationship differs"};
        }
        if (insertedForward) compare(*expected, *actual, where);
    }

// Keep the field names explicit so additions to the public structs can be
// audited mechanically. Each call descends even into currently inactive paint
// variants: their stored values are still observable through the public API.
#define AVE_COMPARE_TYPE(Type) void compare(const Type& expected, const Type& actual, const std::string& where)
#define AVE_FIELD(Field) compare(expected.Field, actual.Field, where + "." #Field)

    AVE_COMPARE_TYPE(runtime::Vec2) { AVE_FIELD(x); AVE_FIELD(y); }
    AVE_COMPARE_TYPE(runtime::RectF) {
        AVE_FIELD(valid); AVE_FIELD(left); AVE_FIELD(top); AVE_FIELD(right); AVE_FIELD(bottom);
    }
    AVE_COMPARE_TYPE(runtime::AffineTransform) {
        AVE_FIELD(m11); AVE_FIELD(m12); AVE_FIELD(m21); AVE_FIELD(m22); AVE_FIELD(dx); AVE_FIELD(dy);
    }
    AVE_COMPARE_TYPE(runtime::EvaluatedPath) {
        AVE_FIELD(verbs); AVE_FIELD(points); AVE_FIELD(controlBounds); AVE_FIELD(hash);
    }
    AVE_COMPARE_TYPE(runtime::Color8) { AVE_FIELD(r); AVE_FIELD(g); AVE_FIELD(b); AVE_FIELD(a); }
    AVE_COMPARE_TYPE(runtime::EvaluatedGradientStop) { AVE_FIELD(position); AVE_FIELD(color); }
    AVE_COMPARE_TYPE(runtime::EvaluatedGradient) {
        AVE_FIELD(kind); AVE_FIELD(start); AVE_FIELD(end); AVE_FIELD(center); AVE_FIELD(focal);
        AVE_FIELD(centerRadius); AVE_FIELD(focalRadius); AVE_FIELD(stops);
    }
    AVE_COMPARE_TYPE(runtime::EvaluatedStroke) {
        AVE_FIELD(enabled); AVE_FIELD(width); AVE_FIELD(miterLimit); AVE_FIELD(cap);
        AVE_FIELD(join); AVE_FIELD(dashArray);
    }
    AVE_COMPARE_TYPE(runtime::EvaluatedImage) {
        AVE_FIELD(present); AVE_FIELD(width); AVE_FIELD(height); AVE_FIELD(matrix);
    }
    AVE_COMPARE_TYPE(runtime::EvaluatedPaint) {
        AVE_FIELD(kind); AVE_FIELD(solid); AVE_FIELD(gradient); AVE_FIELD(image);
    }
    AVE_COMPARE_TYPE(runtime::CanonicalGeometry) {
        AVE_FIELD(sourceKey); AVE_FIELD(contentHash); AVE_FIELD(fillRule); AVE_FIELD(path);
    }
    AVE_COMPARE_TYPE(runtime::CanonicalPaint) {
        AVE_FIELD(sourceKey); AVE_FIELD(contentHash); AVE_FIELD(stroke); AVE_FIELD(paint);
    }
    AVE_COMPARE_TYPE(runtime::EvaluatedMask) {
        AVE_FIELD(layerIndex); AVE_FIELD(mode); AVE_FIELD(opacity); AVE_FIELD(path);
    }
    AVE_COMPARE_TYPE(runtime::EvaluatedDrawItem) {
        AVE_FIELD(modelDrawItem); AVE_FIELD(modelNode); AVE_FIELD(modelGeometry); AVE_FIELD(modelPaint);
        AVE_FIELD(sourceGeometryId); AVE_FIELD(sourcePaintId); AVE_FIELD(sourcePathNode);
        AVE_FIELD(sourcePaintNode); AVE_FIELD(sourcePathCount); AVE_FIELD(sourcePathModifierFree);
        AVE_FIELD(sourceGeometryProjected); AVE_FIELD(sourceRepeaterProjected); AVE_FIELD(sourceRepeaterNode);
        AVE_FIELD(sourceRepeaterCopyIndex); AVE_FIELD(sourceRepeaterVisibleCopies);
        AVE_FIELD(sourceRepeaterMaximumCopies); AVE_FIELD(sourceRepeaterTransformRevision);
        AVE_FIELD(sourceRepeaterOpacityRevision); AVE_FIELD(sourceGeometryRevision);
        AVE_FIELD(sourceGeometryRevisionAuthoritative); AVE_FIELD(geometryOrigin); AVE_FIELD(paintOrigin);
        AVE_FIELD(layerIndex); AVE_FIELD(drawOrder);
        // upstreamChangeBits is intentionally history-dependent.
        AVE_FIELD(fillRule); AVE_FIELD(stroke); AVE_FIELD(paint); AVE_FIELD(path);
        AVE_FIELD(localGeometryAvailable); AVE_FIELD(localGeometryStaticCandidate);
        AVE_FIELD(localToViewport); AVE_FIELD(localPath); AVE_FIELD(localPaintAvailable);
        AVE_FIELD(localPaintStaticCandidate); AVE_FIELD(localStroke); AVE_FIELD(localPaint);
        AVE_FIELD(opacitySeparated); AVE_FIELD(separatedOpacity);
        AVE_FIELD(canonicalGeometry); AVE_FIELD(canonicalPaint);
    }
    AVE_COMPARE_TYPE(runtime::EvaluatedLayer) {
        AVE_FIELD(modelLayer); AVE_FIELD(parentLayer); AVE_FIELD(firstChildReference); AVE_FIELD(childCount);
        AVE_FIELD(firstDrawItem); AVE_FIELD(drawItemCount); AVE_FIELD(firstMask); AVE_FIELD(maskCount);
        AVE_FIELD(keyPath); AVE_FIELD(visible); AVE_FIELD(opacity); AVE_FIELD(matte); AVE_FIELD(clipPath);
    }
    AVE_COMPARE_TYPE(runtime::SceneStatistics) {
        AVE_FIELD(layerCount); AVE_FIELD(visibleLayerCount); AVE_FIELD(drawItemCount); AVE_FIELD(maskCount);
        AVE_FIELD(clipPathCount); AVE_FIELD(pathVerbCount); AVE_FIELD(pathPointCount);
        AVE_FIELD(gradientStopCount); AVE_FIELD(solidPaintCount); AVE_FIELD(gradientPaintCount);
        AVE_FIELD(imagePaintCount);
    }
    AVE_COMPARE_TYPE(runtime::SceneFingerprints) {
        AVE_FIELD(scene); AVE_FIELD(topology); AVE_FIELD(geometry); AVE_FIELD(paint);
    }
    AVE_COMPARE_TYPE(runtime::SceneRecord) {
        AVE_FIELD(fingerprints); AVE_FIELD(statistics); AVE_FIELD(controlBounds);
    }
    AVE_COMPARE_TYPE(runtime::EvaluatedScene) {
        AVE_FIELD(sourceAssetHash);
        // instanceId, assetHandle, instanceHandle, evaluationSequence excluded.
        AVE_FIELD(frameIndex); AVE_FIELD(viewportWidth); AVE_FIELD(viewportHeight);
        AVE_FIELD(layers); AVE_FIELD(childLayerIndices); AVE_FIELD(drawItems); AVE_FIELD(masks);
        AVE_FIELD(modelLayerCount); AVE_FIELD(modelNodeCount); AVE_FIELD(modelGeometryCount);
        AVE_FIELD(modelPaintCount); AVE_FIELD(assetModel); AVE_FIELD(assetModelApplied);
        AVE_FIELD(controlBounds); AVE_FIELD(statistics); AVE_FIELD(fingerprints);
        // changes excluded.
    }

    // assetModel is null on evaluateFrame today. Still compare pointed-to model
    // values exhaustively if present, rather than silently ignoring that field.
    AVE_COMPARE_TYPE(model::IndexRange) { AVE_FIELD(first); AVE_FIELD(count); }
    AVE_COMPARE_TYPE(model::MotionLayerRecord) {
        AVE_FIELD(present); AVE_FIELD(id); AVE_FIELD(parent); AVE_FIELD(children); AVE_FIELD(nodes);
        AVE_FIELD(masks); AVE_FIELD(nameHash); AVE_FIELD(debugName); AVE_FIELD(dependencyBits); AVE_FIELD(matte);
    }
    AVE_COMPARE_TYPE(model::MotionGeometryRecord) {
        AVE_FIELD(present); AVE_FIELD(id); AVE_FIELD(contentHash); AVE_FIELD(resourceClass); AVE_FIELD(staticValue);
    }
    AVE_COMPARE_TYPE(model::MotionPaintRecord) {
        AVE_FIELD(present); AVE_FIELD(id); AVE_FIELD(contentHash); AVE_FIELD(resourceClass); AVE_FIELD(staticValue);
    }
    AVE_COMPARE_TYPE(model::MotionNodeRecord) {
        AVE_FIELD(present); AVE_FIELD(id); AVE_FIELD(drawItem); AVE_FIELD(layer); AVE_FIELD(geometry);
        AVE_FIELD(paint); AVE_FIELD(drawOrder); AVE_FIELD(dependencyBits);
    }
    AVE_COMPARE_TYPE(model::MotionClipRecord) {
        AVE_FIELD(present); AVE_FIELD(id); AVE_FIELD(debugName); AVE_FIELD(firstFrame);
        AVE_FIELD(endFrame); AVE_FIELD(defaultLoop);
    }
    AVE_COMPARE_TYPE(model::MotionVec2Value) { AVE_FIELD(x); AVE_FIELD(y); }
    AVE_COMPARE_TYPE(model::MotionColorValue) { AVE_FIELD(r); AVE_FIELD(g); AVE_FIELD(b); AVE_FIELD(a); }
    AVE_COMPARE_TYPE(model::MotionMatrix3x2Value) {
        AVE_FIELD(m11); AVE_FIELD(m12); AVE_FIELD(m21); AVE_FIELD(m22); AVE_FIELD(dx); AVE_FIELD(dy);
    }
    AVE_COMPARE_TYPE(model::MotionValueRef) { AVE_FIELD(type); AVE_FIELD(index); }
    AVE_COMPARE_TYPE(model::MotionShapeValueRecord) { AVE_FIELD(points); AVE_FIELD(closed); }
    AVE_COMPARE_TYPE(model::MotionGradientValueRecord) { AVE_FIELD(values); }
    AVE_COMPARE_TYPE(model::MotionCompositionRecord) {
        AVE_FIELD(present); AVE_FIELD(id); AVE_FIELD(rootNode); AVE_FIELD(debugName); AVE_FIELD(logicalWidth);
        AVE_FIELD(logicalHeight); AVE_FIELD(firstFrame); AVE_FIELD(endFrame); AVE_FIELD(frameRate);
    }
    AVE_COMPARE_TYPE(model::MotionSourceNodeRecord) {
        AVE_FIELD(present); AVE_FIELD(id); AVE_FIELD(parent); AVE_FIELD(transformParent); AVE_FIELD(composition);
        AVE_FIELD(referencedComposition); AVE_FIELD(kind); AVE_FIELD(layerKind); AVE_FIELD(children);
        AVE_FIELD(properties); AVE_FIELD(nameHash); AVE_FIELD(debugName); AVE_FIELD(hidden);
        AVE_FIELD(authoredStatic); AVE_FIELD(autoOrient); AVE_FIELD(authoredLayerId); AVE_FIELD(authoredParentLayerId);
        AVE_FIELD(inFrame); AVE_FIELD(outFrame); AVE_FIELD(startFrame); AVE_FIELD(timeStretch); AVE_FIELD(dependencyBits);
        AVE_FIELD(fillRule); AVE_FIELD(strokeCap); AVE_FIELD(strokeJoin); AVE_FIELD(gradientType);
        AVE_FIELD(maskMode); AVE_FIELD(matteMode); AVE_FIELD(blendMode); AVE_FIELD(pathDirection);
        AVE_FIELD(polystarType); AVE_FIELD(trimMode); AVE_FIELD(miterLimit); AVE_FIELD(repeaterMaximumCopies);
        AVE_FIELD(gradientColorPointCount); AVE_FIELD(layerWidth); AVE_FIELD(layerHeight); AVE_FIELD(solidColor);
        AVE_FIELD(sourceAssetRefHash); AVE_FIELD(enabled); AVE_FIELD(maskInverted);
    }
    AVE_COMPARE_TYPE(model::MotionPropertyRecord) {
        AVE_FIELD(present); AVE_FIELD(id); AVE_FIELD(owner); AVE_FIELD(semantic); AVE_FIELD(semanticIndex);
        AVE_FIELD(valueType); AVE_FIELD(flags); AVE_FIELD(staticValue); AVE_FIELD(track);
    }
    AVE_COMPARE_TYPE(model::MotionTrackRecord) {
        AVE_FIELD(present); AVE_FIELD(id); AVE_FIELD(property); AVE_FIELD(segments); AVE_FIELD(firstFrame); AVE_FIELD(endFrame);
    }
    AVE_COMPARE_TYPE(model::MotionSegmentRecord) {
        AVE_FIELD(present); AVE_FIELD(id); AVE_FIELD(track); AVE_FIELD(firstFrame); AVE_FIELD(endFrame);
        AVE_FIELD(interpolation); AVE_FIELD(spatialInterpolation); AVE_FIELD(startValue); AVE_FIELD(endValue);
        AVE_FIELD(temporalControl1); AVE_FIELD(temporalControl2); AVE_FIELD(spatialInTangent); AVE_FIELD(spatialOutTangent);
    }
    AVE_COMPARE_TYPE(model::MotionAssetModelStatistics) {
        AVE_FIELD(declaredLayerCount); AVE_FIELD(declaredNodeCount); AVE_FIELD(declaredGeometryCount); AVE_FIELD(declaredPaintCount);
        AVE_FIELD(observedLayerCount); AVE_FIELD(observedNodeCount); AVE_FIELD(observedGeometryCount); AVE_FIELD(observedPaintCount);
        AVE_FIELD(assetStaticGeometryCount); AVE_FIELD(assetStaticPaintCount); AVE_FIELD(maskCount); AVE_FIELD(clipCount);
        AVE_FIELD(directParsedModel); AVE_FIELD(compositionCount); AVE_FIELD(sourceNodeCount); AVE_FIELD(propertyCount);
        AVE_FIELD(staticPropertyCount); AVE_FIELD(animatedPropertyCount); AVE_FIELD(trackCount); AVE_FIELD(segmentCount);
        AVE_FIELD(scalarValueCount); AVE_FIELD(vec2ValueCount); AVE_FIELD(colorValueCount); AVE_FIELD(matrixValueCount);
        AVE_FIELD(shapeValueCount); AVE_FIELD(gradientValueCount);
    }
    AVE_COMPARE_TYPE(model::MotionAssetModel) {
        AVE_FIELD(schemaVersion); AVE_FIELD(revision);
        // assetHandle is identity, excluded consistently with EvaluatedScene.
        AVE_FIELD(sourceAssetHash); AVE_FIELD(logicalWidth); AVE_FIELD(logicalHeight); AVE_FIELD(frameRate);
        AVE_FIELD(totalFrames); AVE_FIELD(debugName); AVE_FIELD(layers); AVE_FIELD(childLayerIds); AVE_FIELD(layerNodeIds);
        AVE_FIELD(nodes); AVE_FIELD(geometries); AVE_FIELD(paints); AVE_FIELD(drawOrder); AVE_FIELD(clips);
        AVE_FIELD(compositions); AVE_FIELD(sourceNodes); AVE_FIELD(sourceChildIds); AVE_FIELD(sourcePropertyIds);
        AVE_FIELD(properties); AVE_FIELD(tracks); AVE_FIELD(segments); AVE_FIELD(scalarValues); AVE_FIELD(vec2Values);
        AVE_FIELD(colorValues); AVE_FIELD(matrixValues); AVE_FIELD(shapeValues); AVE_FIELD(shapePoints);
        AVE_FIELD(gradientValues); AVE_FIELD(gradientFloats); AVE_FIELD(statistics); AVE_FIELD(parsedModelFingerprint);
        AVE_FIELD(topologyFingerprint); AVE_FIELD(resourceFingerprint); AVE_FIELD(fingerprint);
    }

#undef AVE_FIELD
#undef AVE_COMPARE_TYPE
};

} // namespace avemotion::test
