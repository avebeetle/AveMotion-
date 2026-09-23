#include "RlottieSceneBridge.hpp"

#include "avemotion/core/Hash.hpp"
#include "avemotion/runtime/RecordingBackend.hpp"

#include <rlottiecommon.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace avemotion::runtime::detail {
namespace {

constexpr std::size_t kMaximumLayerDepth = 256U;
constexpr std::size_t kMaximumLayers = 100000U;
constexpr std::size_t kMaximumDrawItems = 1000000U;
constexpr std::size_t kMaximumPathVerbs = 10000000U;
constexpr std::size_t kMaximumPathPoints = kMaximumPathVerbs * 3U;
constexpr std::size_t kMaximumMasks = 1000000U;
constexpr std::size_t kMaximumGradientStops = 1000000U;
constexpr std::size_t kMaximumDashValues = 1000000U;

enum class DeclaredPointUnit {
    Floats,
    Points,
};

void includePoint(RectF& bounds, Vec2 point) noexcept {
    if (!bounds.valid) {
        bounds = {true, point.x, point.y, point.x, point.y};
        return;
    }
    bounds.left = std::min(bounds.left, point.x);
    bounds.top = std::min(bounds.top, point.y);
    bounds.right = std::max(bounds.right, point.x);
    bounds.bottom = std::max(bounds.bottom, point.y);
}

void includeRect(RectF& destination, const RectF& source) noexcept {
    if (!source.valid) {
        return;
    }
    if (!destination.valid) {
        destination = source;
        return;
    }
    destination.left = std::min(destination.left, source.left);
    destination.top = std::min(destination.top, source.top);
    destination.right = std::max(destination.right, source.right);
    destination.bottom = std::max(destination.bottom, source.bottom);
}

std::size_t pointCountForVerb(PathVerb verb) noexcept {
    switch (verb) {
    case PathVerb::MoveTo:
    case PathVerb::LineTo:
        return 1U;
    case PathVerb::CubicTo:
        return 3U;
    case PathVerb::Close:
        return 0U;
    }
    return 0U;
}

bool decodeVerb(char raw, PathVerb& result) noexcept {
    switch (static_cast<unsigned char>(raw)) {
    case 0U:
        result = PathVerb::MoveTo;
        return true;
    case 1U:
        result = PathVerb::LineTo;
        return true;
    case 2U:
        result = PathVerb::CubicTo;
        return true;
    case 3U:
        result = PathVerb::Close;
        return true;
    default:
        return false;
    }
}

RuntimeError copyPath(
    const float* points,
    std::size_t declaredPointCount,
    const char* elements,
    std::size_t elementCount,
    DeclaredPointUnit pointUnit,
    EvaluatedPath& destination) {
    destination = {};
    if (elementCount == 0U) {
        return {};
    }
    if (elementCount > kMaximumPathVerbs || elements == nullptr) {
        return {RuntimeErrorCode::EvaluationFailed, "invalid or excessive path element array"};
    }

    destination.verbs.reserve(elementCount);
    std::size_t requiredPoints = 0U;
    for (std::size_t index = 0; index < elementCount; ++index) {
        PathVerb verb{};
        if (!decodeVerb(elements[index], verb)) {
            return {RuntimeErrorCode::EvaluationFailed, "unknown upstream path verb"};
        }
        destination.verbs.push_back(verb);
        const auto pointsForVerb = pointCountForVerb(verb);
        if (requiredPoints > kMaximumPathPoints - pointsForVerb) {
            return {RuntimeErrorCode::EvaluationFailed, "path point count exceeds the supported limit"};
        }
        requiredPoints += pointsForVerb;
    }

    const auto requiredFloats = requiredPoints * 2U;
    if (pointUnit == DeclaredPointUnit::Points
        && declaredPointCount > std::numeric_limits<std::size_t>::max() / 2U) {
        return {RuntimeErrorCode::EvaluationFailed, "path point count overflows its declared unit"};
    }
    const auto declaredFloats = pointUnit == DeclaredPointUnit::Floats
        ? declaredPointCount
        : declaredPointCount * 2U;
    if (requiredFloats != 0U && (points == nullptr || declaredFloats < requiredFloats)) {
        return {RuntimeErrorCode::EvaluationFailed, "upstream path point array is shorter than its verb stream"};
    }

    destination.points.reserve(requiredPoints);
    for (std::size_t index = 0; index < requiredPoints; ++index) {
        const Vec2 point{points[index * 2U], points[index * 2U + 1U]};
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
            return {RuntimeErrorCode::EvaluationFailed, "non-finite evaluated path point"};
        }
        destination.points.push_back(point);
        includePoint(destination.controlBounds, point);
    }

    core::Fnv1a64 hash;
    hash.appendU64(destination.verbs.size());
    hash.appendU64(destination.points.size());
    for (const auto verb : destination.verbs) {
        hash.appendU8(static_cast<std::uint8_t>(verb));
    }
    for (const auto point : destination.points) {
        hash.appendFloat(point.x);
        hash.appendFloat(point.y);
    }
    destination.hash = hash.value();
    return {};
}

LineCap mapCap(LOTCapStyle cap) noexcept {
    switch (cap) {
    case CapSquare:
        return LineCap::Square;
    case CapRound:
        return LineCap::Round;
    case CapFlat:
    default:
        return LineCap::Flat;
    }
}

LineJoin mapJoin(LOTJoinStyle join) noexcept {
    switch (join) {
    case JoinBevel:
        return LineJoin::Bevel;
    case JoinRound:
        return LineJoin::Round;
    case JoinMiter:
    default:
        return LineJoin::Miter;
    }
}

MaskMode mapMask(LOTMaskType mode) noexcept {
    switch (mode) {
    case MaskSubstract:
        return MaskMode::Subtract;
    case MaskIntersect:
        return MaskMode::Intersect;
    case MaskDifference:
        return MaskMode::Difference;
    case MaskAdd:
    default:
        return MaskMode::Add;
    }
}

MatteMode mapMatte(LOTMatteType matte) noexcept {
    switch (matte) {
    case MatteAlpha:
        return MatteMode::Alpha;
    case MatteAlphaInv:
        return MatteMode::AlphaInverted;
    case MatteLuma:
        return MatteMode::Luma;
    case MatteLumaInv:
        return MatteMode::LumaInverted;
    case MatteNone:
    default:
        return MatteMode::None;
    }
}

#if AVEMOTION_TELEGRAM_LOCAL_GEOMETRY_EXTENSIONS
RuntimeError copyAffineTransform(
    const LOTNode& source,
    AffineTransform& destination) noexcept {
    const std::array values{
        source.mAveLocalTransform.m11,
        source.mAveLocalTransform.m12,
        source.mAveLocalTransform.m13,
        source.mAveLocalTransform.m21,
        source.mAveLocalTransform.m22,
        source.mAveLocalTransform.m23,
        source.mAveLocalTransform.m31,
        source.mAveLocalTransform.m32,
        source.mAveLocalTransform.m33,
    };
    if (std::any_of(
            values.begin(), values.end(),
            [](float value) { return !std::isfinite(value); })) {
        return {
            RuntimeErrorCode::EvaluationFailed,
            "non-finite AveMotion local geometry transform"};
    }
    destination = {
        source.mAveLocalTransform.m11,
        source.mAveLocalTransform.m12,
        source.mAveLocalTransform.m21,
        source.mAveLocalTransform.m22,
        source.mAveLocalTransform.m31,
        source.mAveLocalTransform.m32,
    };
    return {};
}
#endif

class Builder final {
public:
    explicit Builder(EvaluatedScene scene)
        : scene_(std::move(scene)) {
    }

    SceneBuildOutcome build(const LOTLayerNode* root) {
        if (root == nullptr) {
            return {{}, {RuntimeErrorCode::EvaluationFailed, "rlottie returned a null render tree"}};
        }
#if AVEMOTION_TELEGRAM_LOCAL_GEOMETRY_EXTENSIONS
        scene_.modelLayerCount = root->mAveLayerCount;
        scene_.modelNodeCount = root->mAveNodeCount;
        scene_.modelGeometryCount = root->mAveGeometryCount;
        scene_.modelPaintCount = root->mAvePaintCount;
#endif
        const auto rootIndex = appendLayer(root, kInvalidSceneIndex, 0U);
        if (error_) {
            return {{}, std::move(error_)};
        }
        if (rootIndex != 0U) {
            return {{}, {RuntimeErrorCode::EvaluationFailed, "unexpected root layer index"}};
        }

        scene_.statistics.layerCount = scene_.layers.size();
        scene_.statistics.drawItemCount = scene_.drawItems.size();
        scene_.statistics.maskCount = scene_.masks.size();
        if (scene_.modelLayerCount == 0U) {
            scene_.modelLayerCount = static_cast<std::uint32_t>(scene_.layers.size());
        }
        if (scene_.modelNodeCount == 0U) {
            scene_.modelNodeCount = static_cast<std::uint32_t>(scene_.drawItems.size());
        }
        if (scene_.modelGeometryCount == 0U) {
            scene_.modelGeometryCount = static_cast<std::uint32_t>(scene_.drawItems.size());
        }
        if (scene_.modelPaintCount == 0U) {
            scene_.modelPaintCount = static_cast<std::uint32_t>(scene_.drawItems.size());
        }
        scene_.fingerprints = computeSceneFingerprints(scene_);
        return {std::move(scene_), {}};
    }

private:
    std::uint32_t appendLayer(
        const LOTLayerNode* source,
        std::uint32_t parent,
        std::size_t depth) {
        if (source == nullptr) {
            setError("null child layer pointer");
            return kInvalidSceneIndex;
        }
        if (depth > kMaximumLayerDepth) {
            setError("render tree exceeds maximum supported depth");
            return kInvalidSceneIndex;
        }
        if (scene_.layers.size() >= kMaximumLayers) {
            setError("render tree exceeds maximum supported layer count");
            return kInvalidSceneIndex;
        }
        if (!activeLayers_.insert(source).second) {
            setError("cycle detected in upstream render tree");
            return kInvalidSceneIndex;
        }

        const auto layerIndex = static_cast<std::uint32_t>(scene_.layers.size());
        scene_.layers.emplace_back();
        auto& layer = scene_.layers[layerIndex];
#if AVEMOTION_TELEGRAM_LOCAL_GEOMETRY_EXTENSIONS
        if (source->mAveLayerId != AveInvalidSourceId) {
            layer.modelLayer = model::LayerId{source->mAveLayerId};
        }
#endif
        if (!layer.modelLayer.valid()) {
            layer.modelLayer = model::LayerId{layerIndex};
        }
        layer.parentLayer = parent;
        layer.keyPath = source->keypath != nullptr ? source->keypath : std::string{};
        layer.visible = source->mVisible != 0;
        layer.opacity = static_cast<float>(source->mAlpha) / 255.0F;
        layer.matte = mapMatte(source->mMatte);
        if (layer.visible) {
            ++scene_.statistics.visibleLayerCount;
        }

        if (source->mClipPath.elmCount != 0U) {
            const auto error = copyPath(
                source->mClipPath.ptPtr,
                source->mClipPath.ptCount,
                source->mClipPath.elmPtr,
                source->mClipPath.elmCount,
                DeclaredPointUnit::Floats,
                layer.clipPath);
            if (error) {
                error_ = error;
                activeLayers_.erase(source);
                return kInvalidSceneIndex;
            }
            ++scene_.statistics.clipPathCount;
            addPathStatistics(layer.clipPath);
            includeRect(scene_.controlBounds, layer.clipPath.controlBounds);
        }

        layer.firstMask = static_cast<std::uint32_t>(scene_.masks.size());
        if (source->mMaskList.size > kMaximumMasks
            || scene_.masks.size() > kMaximumMasks - source->mMaskList.size) {
            setError("render tree exceeds maximum supported mask count");
            activeLayers_.erase(source);
            return kInvalidSceneIndex;
        }
        if (source->mMaskList.size != 0U && source->mMaskList.ptr == nullptr) {
            setError("mask list has a non-zero size and a null pointer");
            activeLayers_.erase(source);
            return kInvalidSceneIndex;
        }
        for (std::size_t index = 0; index < source->mMaskList.size; ++index) {
            const auto& upstreamMask = source->mMaskList.ptr[index];
            EvaluatedMask mask;
            mask.layerIndex = layerIndex;
            mask.mode = mapMask(upstreamMask.mMode);
            mask.opacity = static_cast<float>(upstreamMask.mAlpha) / 255.0F;
            const auto error = copyPath(
                upstreamMask.mPath.ptPtr,
                upstreamMask.mPath.ptCount,
                upstreamMask.mPath.elmPtr,
                upstreamMask.mPath.elmCount,
                DeclaredPointUnit::Points,
                mask.path);
            if (error) {
                error_ = error;
                activeLayers_.erase(source);
                return kInvalidSceneIndex;
            }
            addPathStatistics(mask.path);
            includeRect(scene_.controlBounds, mask.path.controlBounds);
            scene_.masks.push_back(std::move(mask));
        }
        layer.maskCount = static_cast<std::uint32_t>(
            scene_.masks.size() - layer.firstMask);

        layer.firstDrawItem = static_cast<std::uint32_t>(scene_.drawItems.size());
        if (source->mNodeList.size != 0U && source->mNodeList.ptr == nullptr) {
            setError("node list has a non-zero size and a null pointer");
            activeLayers_.erase(source);
            return kInvalidSceneIndex;
        }
        for (std::size_t index = 0; index < source->mNodeList.size; ++index) {
            if (scene_.drawItems.size() >= kMaximumDrawItems) {
                setError("render tree exceeds maximum supported draw-item count");
                activeLayers_.erase(source);
                return kInvalidSceneIndex;
            }
            const auto* upstreamNode = source->mNodeList.ptr[index];
            if (upstreamNode == nullptr) {
                setError("null draw node pointer");
                activeLayers_.erase(source);
                return kInvalidSceneIndex;
            }
            EvaluatedDrawItem item;
#if AVEMOTION_TELEGRAM_LOCAL_GEOMETRY_EXTENSIONS
            if (upstreamNode->mAveNodeId != AveInvalidSourceId) {
                item.modelNode = model::NodeId{upstreamNode->mAveNodeId};
                item.modelDrawItem = model::DrawItemId{upstreamNode->mAveNodeId};
            }
            if (upstreamNode->mAveGeometryId != AveInvalidSourceId) {
                item.modelGeometry = model::GeometryId{upstreamNode->mAveGeometryId};
            }
            if (upstreamNode->mAvePaintId != AveInvalidSourceId) {
                item.modelPaint = model::PaintId{upstreamNode->mAvePaintId};
            }
            if (upstreamNode->mAveSourcePathNodeId != AveInvalidSourceId) {
                item.sourcePathNode = model::SourceNodeId{
                    upstreamNode->mAveSourcePathNodeId};
            }
            if (upstreamNode->mAveSourcePaintNodeId != AveInvalidSourceId) {
                item.sourcePaintNode = model::SourceNodeId{
                    upstreamNode->mAveSourcePaintNodeId};
            }
            item.sourcePathCount = upstreamNode->mAveSourcePathCount;
            item.sourcePathModifierFree =
                (upstreamNode->mAveSourceFlags & AveSourceFlagModifierFree) != 0U;
#endif
            const auto fallbackSourceId = static_cast<std::uint32_t>(
                scene_.drawItems.size());
            if (!item.modelNode.valid()) {
                item.modelNode = model::NodeId{fallbackSourceId};
            }
            if (!item.modelDrawItem.valid()) {
                item.modelDrawItem = model::DrawItemId{fallbackSourceId};
            }
            if (!item.modelGeometry.valid()) {
                item.modelGeometry = model::GeometryId{fallbackSourceId};
            }
            if (!item.modelPaint.valid()) {
                item.modelPaint = model::PaintId{fallbackSourceId};
            }
            item.layerIndex = layerIndex;
            item.drawOrder = static_cast<std::uint32_t>(scene_.drawItems.size());
            if ((upstreamNode->mFlag & ChangeFlagPath) != 0) {
                item.upstreamChangeBits |= SceneChangeGeometry;
            }
            if ((upstreamNode->mFlag & ChangeFlagPaint) != 0) {
                item.upstreamChangeBits |= SceneChangePaint;
            }
            item.fillRule = upstreamNode->mFillRule == FillEvenOdd
                ? FillRule::EvenOdd
                : FillRule::Winding;
            item.stroke.enabled = upstreamNode->mStroke.enable != 0;
            item.stroke.width = upstreamNode->mStroke.width;
            item.stroke.miterLimit = upstreamNode->mStroke.miterLimit;
            if (!std::isfinite(item.stroke.width)
                || !std::isfinite(item.stroke.miterLimit)) {
                setError("non-finite evaluated stroke value");
                activeLayers_.erase(source);
                return kInvalidSceneIndex;
            }
            item.stroke.cap = mapCap(upstreamNode->mStroke.cap);
            item.stroke.join = mapJoin(upstreamNode->mStroke.join);
            if (upstreamNode->mStroke.dashArraySize < 0
                || static_cast<std::size_t>(upstreamNode->mStroke.dashArraySize)
                    > kMaximumDashValues) {
                setError("invalid or excessive evaluated dash array");
                activeLayers_.erase(source);
                return kInvalidSceneIndex;
            }
            if (upstreamNode->mStroke.dashArraySize > 0) {
                if (upstreamNode->mStroke.dashArray == nullptr) {
                    setError("dash array size is non-zero but the pointer is null");
                    activeLayers_.erase(source);
                    return kInvalidSceneIndex;
                }
                item.stroke.dashArray.assign(
                    upstreamNode->mStroke.dashArray,
                    upstreamNode->mStroke.dashArray
                        + upstreamNode->mStroke.dashArraySize);
                if (std::any_of(
                        item.stroke.dashArray.begin(),
                        item.stroke.dashArray.end(),
                        [](float value) { return !std::isfinite(value); })) {
                    setError("non-finite evaluated dash value");
                    activeLayers_.erase(source);
                    return kInvalidSceneIndex;
                }
            }

            const auto pathError = copyPath(
                upstreamNode->mPath.ptPtr,
                upstreamNode->mPath.ptCount,
                upstreamNode->mPath.elmPtr,
                upstreamNode->mPath.elmCount,
                DeclaredPointUnit::Floats,
                item.path);
            if (pathError) {
                error_ = pathError;
                activeLayers_.erase(source);
                return kInvalidSceneIndex;
            }
            addPathStatistics(item.path);
            includeRect(scene_.controlBounds, item.path.controlBounds);

#if AVEMOTION_TELEGRAM_LOCAL_GEOMETRY_EXTENSIONS
            if ((upstreamNode->mAveSourceFlags & AveSourceFlagLocalGeometry) != 0U) {
                const auto localPathError = copyPath(
                    upstreamNode->mAveLocalPath.ptPtr,
                    upstreamNode->mAveLocalPath.ptCount,
                    upstreamNode->mAveLocalPath.elmPtr,
                    upstreamNode->mAveLocalPath.elmCount,
                    DeclaredPointUnit::Floats,
                    item.localPath);
                if (localPathError) {
                    error_ = localPathError;
                    activeLayers_.erase(source);
                    return kInvalidSceneIndex;
                }
                const auto transformError = copyAffineTransform(
                    *upstreamNode, item.localToViewport);
                if (transformError) {
                    error_ = transformError;
                    activeLayers_.erase(source);
                    return kInvalidSceneIndex;
                }
                item.localGeometryAvailable = true;
                item.localGeometryStaticCandidate =
                    (upstreamNode->mAveSourceFlags
                        & AveSourceFlagGeometryStatic) != 0U;
            }
#endif

            if (upstreamNode->mImageInfo.data != nullptr
                && upstreamNode->mImageInfo.width != 0U
                && upstreamNode->mImageInfo.height != 0U) {
                item.paint.kind = PaintKind::Image;
                item.paint.image.present = true;
                item.paint.image.width = upstreamNode->mImageInfo.width;
                item.paint.image.height = upstreamNode->mImageInfo.height;
                const auto& matrix = upstreamNode->mImageInfo.mMatrix;
                item.paint.image.matrix[0] = matrix.m11;
                item.paint.image.matrix[1] = matrix.m12;
                item.paint.image.matrix[2] = matrix.m13;
                item.paint.image.matrix[3] = matrix.m21;
                item.paint.image.matrix[4] = matrix.m22;
                item.paint.image.matrix[5] = matrix.m23;
                item.paint.image.matrix[6] = matrix.m31;
                item.paint.image.matrix[7] = matrix.m32;
                item.paint.image.matrix[8] = matrix.m33;
                if (std::any_of(
                        std::begin(item.paint.image.matrix),
                        std::end(item.paint.image.matrix),
                        [](float value) { return !std::isfinite(value); })) {
                    setError("non-finite evaluated image transform");
                    activeLayers_.erase(source);
                    return kInvalidSceneIndex;
                }
                ++scene_.statistics.imagePaintCount;
            } else if (upstreamNode->mBrushType == BrushGradient) {
                item.paint.kind = PaintKind::Gradient;
                item.paint.gradient.kind = upstreamNode->mGradient.type == GradientRadial
                    ? GradientKind::Radial
                    : GradientKind::Linear;
                item.paint.gradient.start = {
                    upstreamNode->mGradient.start.x,
                    upstreamNode->mGradient.start.y};
                item.paint.gradient.end = {
                    upstreamNode->mGradient.end.x,
                    upstreamNode->mGradient.end.y};
                item.paint.gradient.center = {
                    upstreamNode->mGradient.center.x,
                    upstreamNode->mGradient.center.y};
                item.paint.gradient.focal = {
                    upstreamNode->mGradient.focal.x,
                    upstreamNode->mGradient.focal.y};
                item.paint.gradient.centerRadius = upstreamNode->mGradient.cradius;
                item.paint.gradient.focalRadius = upstreamNode->mGradient.fradius;
                const std::array gradientValues{
                    item.paint.gradient.start.x,
                    item.paint.gradient.start.y,
                    item.paint.gradient.end.x,
                    item.paint.gradient.end.y,
                    item.paint.gradient.center.x,
                    item.paint.gradient.center.y,
                    item.paint.gradient.focal.x,
                    item.paint.gradient.focal.y,
                    item.paint.gradient.centerRadius,
                    item.paint.gradient.focalRadius};
                if (std::any_of(
                        gradientValues.begin(),
                        gradientValues.end(),
                        [](float value) { return !std::isfinite(value); })) {
                    setError("non-finite evaluated gradient value");
                    activeLayers_.erase(source);
                    return kInvalidSceneIndex;
                }
                if (upstreamNode->mGradient.stopCount > kMaximumGradientStops) {
                    setError("evaluated gradient exceeds maximum supported stop count");
                    activeLayers_.erase(source);
                    return kInvalidSceneIndex;
                }
                if (upstreamNode->mGradient.stopCount != 0U
                    && upstreamNode->mGradient.stopPtr == nullptr) {
                    setError("gradient stop count is non-zero but the pointer is null");
                    activeLayers_.erase(source);
                    return kInvalidSceneIndex;
                }
                item.paint.gradient.stops.reserve(upstreamNode->mGradient.stopCount);
                for (std::size_t stopIndex = 0;
                     stopIndex < upstreamNode->mGradient.stopCount;
                     ++stopIndex) {
                    const auto& stop = upstreamNode->mGradient.stopPtr[stopIndex];
                    if (!std::isfinite(stop.pos)) {
                        setError("non-finite evaluated gradient stop position");
                        activeLayers_.erase(source);
                        return kInvalidSceneIndex;
                    }
                    item.paint.gradient.stops.push_back({
                        stop.pos,
                        {stop.r, stop.g, stop.b, stop.a}});
                }
                scene_.statistics.gradientStopCount +=
                    item.paint.gradient.stops.size();
                ++scene_.statistics.gradientPaintCount;
            } else {
                item.paint.kind = PaintKind::Solid;
                item.paint.solid = {
                    upstreamNode->mColor.r,
                    upstreamNode->mColor.g,
                    upstreamNode->mColor.b,
                    upstreamNode->mColor.a};
                ++scene_.statistics.solidPaintCount;
            }

#if AVEMOTION_TELEGRAM_LOCAL_GEOMETRY_EXTENSIONS
            if ((upstreamNode->mAveSourceFlags & AveSourceFlagLocalSolidPaint) != 0U) {
                item.localPaintAvailable = true;
                item.localPaintStaticCandidate =
                    (upstreamNode->mAveSourceFlags & AveSourceFlagPaintStatic) != 0U;
                item.localPaint.kind = PaintKind::Solid;
                item.localPaint.solid = {
                    upstreamNode->mAveLocalColor.r,
                    upstreamNode->mAveLocalColor.g,
                    upstreamNode->mAveLocalColor.b,
                    upstreamNode->mAveLocalColor.a};
            }
            if ((upstreamNode->mAveSourceFlags & AveSourceFlagOpacitySeparated) != 0U) {
                if (!std::isfinite(upstreamNode->mAveOpacity)) {
                    setError("non-finite AveMotion separated opacity");
                    activeLayers_.erase(source);
                    return kInvalidSceneIndex;
                }
                item.opacitySeparated = true;
                item.separatedOpacity = upstreamNode->mAveOpacity;
            }
#endif
            scene_.drawItems.push_back(std::move(item));
        }
        layer.drawItemCount = static_cast<std::uint32_t>(
            scene_.drawItems.size() - layer.firstDrawItem);

        std::vector<std::uint32_t> directChildren;
        if (source->mLayerList.size > kMaximumLayers) {
            setError("child layer list exceeds maximum supported count");
            activeLayers_.erase(source);
            return kInvalidSceneIndex;
        }
        directChildren.reserve(source->mLayerList.size);
        if (source->mLayerList.size != 0U && source->mLayerList.ptr == nullptr) {
            setError("child layer list has a non-zero size and a null pointer");
            activeLayers_.erase(source);
            return kInvalidSceneIndex;
        }
        for (std::size_t index = 0; index < source->mLayerList.size; ++index) {
            const auto child = appendLayer(source->mLayerList.ptr[index], layerIndex, depth + 1U);
            if (error_) {
                activeLayers_.erase(source);
                return kInvalidSceneIndex;
            }
            directChildren.push_back(child);
        }
        scene_.layers[layerIndex].firstChildReference = static_cast<std::uint32_t>(
            scene_.childLayerIndices.size());
        scene_.layers[layerIndex].childCount = static_cast<std::uint32_t>(directChildren.size());
        scene_.childLayerIndices.insert(
            scene_.childLayerIndices.end(), directChildren.begin(), directChildren.end());

        activeLayers_.erase(source);
        return layerIndex;
    }

    void addPathStatistics(const EvaluatedPath& path) noexcept {
        scene_.statistics.pathVerbCount += path.verbs.size();
        scene_.statistics.pathPointCount += path.points.size();
    }

    void setError(std::string message) {
        if (!error_) {
            error_ = {RuntimeErrorCode::EvaluationFailed, std::move(message)};
        }
    }

    EvaluatedScene scene_;
    RuntimeError error_;
    std::unordered_set<const LOTLayerNode*> activeLayers_;
};

} // namespace

SceneBuildOutcome buildSceneFromRlottieTree(
    const LOTLayerNode* root,
    std::uint64_t sourceAssetHash,
    std::uint64_t instanceId,
    std::uint64_t evaluationSequence,
    std::size_t frameIndex,
    std::size_t viewportWidth,
    std::size_t viewportHeight) {
    EvaluatedScene scene;
    scene.sourceAssetHash = sourceAssetHash;
    scene.instanceId = instanceId;
    scene.evaluationSequence = evaluationSequence;
    scene.frameIndex = frameIndex;
    scene.viewportWidth = viewportWidth;
    scene.viewportHeight = viewportHeight;
    return Builder{std::move(scene)}.build(root);
}

} // namespace avemotion::runtime::detail
