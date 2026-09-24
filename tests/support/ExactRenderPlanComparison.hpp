#pragma once

#include "ExactSceneComparison.hpp"
#include "avemotion/render/RenderPlan.hpp"

#include <bit>
#include <stdexcept>
#include <string>
#include <vector>

namespace avemotion::test {

class ExactRenderPlanComparison final {
public:
    [[nodiscard]] std::string difference(const render::MotionRenderPlan& expected,
                                         const render::MotionRenderPlan& actual) const {
        try {
            compare(expected, actual, "plan");
        } catch (const Mismatch& mismatch) {
            return mismatch.what();
        }
        return {};
    }

private:
    struct Mismatch final : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    template <typename T>
    static void compare(const T& expected, const T& actual, const std::string& where) {
        if (!(expected == actual)) throw Mismatch{where + " differs"};
    }

    static void compare(float expected, float actual, const std::string& where) {
        if (std::bit_cast<std::uint32_t>(expected) != std::bit_cast<std::uint32_t>(actual))
            throw Mismatch{where + " differs (float bits "
                + std::to_string(std::bit_cast<std::uint32_t>(expected)) + "/"
                + std::to_string(std::bit_cast<std::uint32_t>(actual)) + ")"};
    }

    template <typename T>
    static void compare(const std::vector<T>& expected, const std::vector<T>& actual,
                        const std::string& where) {
        compare(expected.size(), actual.size(), where + ".size");
        for (std::size_t index = 0; index < expected.size(); ++index)
            compare(expected[index], actual[index], where + "[" + std::to_string(index) + "]");
    }

    static void compare(const std::shared_ptr<const runtime::EvaluatedScene>& expected,
                        const std::shared_ptr<const runtime::EvaluatedScene>& actual,
                        const std::string& where) {
        compare(bool(expected), bool(actual), where + ".present");
        if (!expected) return;
        const auto sceneDifference = ExactSceneComparison{}.difference(*expected, *actual);
        if (!sceneDifference.empty()) throw Mismatch{where + "." + sceneDifference};
        // ExactSceneComparison intentionally excludes these scene identity and
        // history fields. They are observable through MotionRenderPlan::sourceScene.
        compare(expected->assetHandle, actual->assetHandle, where + ".assetHandle");
        compare(expected->instanceHandle, actual->instanceHandle, where + ".instanceHandle");
        compare(expected->instanceId, actual->instanceId, where + ".instanceId");
        compare(expected->evaluationSequence, actual->evaluationSequence,
                where + ".evaluationSequence");
        compare(expected->changes.firstEvaluation, actual->changes.firstEvaluation,
                where + ".changes.firstEvaluation");
        compare(expected->changes.topologyChanged, actual->changes.topologyChanged,
                where + ".changes.topologyChanged");
        compare(expected->changes.geometryChanged, actual->changes.geometryChanged,
                where + ".changes.geometryChanged");
        compare(expected->changes.paintChanged, actual->changes.paintChanged,
                where + ".changes.paintChanged");
        compare(expected->changes.visualChanged, actual->changes.visualChanged,
                where + ".changes.visualChanged");
        // upstreamChangeBits remains excluded: it is ordinary upstream
        // implementation history, not the private scene stream contract.
    }

#define AVE_COMPARE_TYPE(Type) static void compare(const Type& expected, const Type& actual, const std::string& where)
#define AVE_FIELD(Field) compare(expected.Field, actual.Field, where + "." #Field)
    AVE_COMPARE_TYPE(runtime::RectF) {
        AVE_FIELD(valid); AVE_FIELD(left); AVE_FIELD(top); AVE_FIELD(right); AVE_FIELD(bottom);
    }
    AVE_COMPARE_TYPE(render::Matrix3x2) {
        AVE_FIELD(m11); AVE_FIELD(m12); AVE_FIELD(m21); AVE_FIELD(m22); AVE_FIELD(dx); AVE_FIELD(dy);
    }
    AVE_COMPARE_TYPE(render::GeometryCacheKey) {
        AVE_FIELD(scope); AVE_FIELD(assetHash); AVE_FIELD(assetIdentity); AVE_FIELD(instanceId);
        AVE_FIELD(instanceIdentity); AVE_FIELD(sourceKey); AVE_FIELD(resourceId);
        AVE_FIELD(contentHash); AVE_FIELD(revision);
    }
    AVE_COMPARE_TYPE(render::PaintCacheKey) {
        AVE_FIELD(scope); AVE_FIELD(assetHash); AVE_FIELD(assetIdentity); AVE_FIELD(instanceId);
        AVE_FIELD(instanceIdentity); AVE_FIELD(sourceKey); AVE_FIELD(resourceId);
        AVE_FIELD(contentHash); AVE_FIELD(revision);
    }
    AVE_COMPARE_TYPE(render::RenderPlanStamp) {
        AVE_FIELD(assetHash); AVE_FIELD(assetIdentity); AVE_FIELD(instanceId);
        AVE_FIELD(instanceIdentity); AVE_FIELD(evaluationSequence); AVE_FIELD(planSequence);
        AVE_FIELD(layoutRevision);
    }
    AVE_COMPARE_TYPE(render::MotionDrawItem) {
        AVE_FIELD(drawItem); AVE_FIELD(node); AVE_FIELD(sourceItemKey); AVE_FIELD(sourceDrawItemIndex);
        AVE_FIELD(drawOrder); AVE_FIELD(geometry); AVE_FIELD(paint);
        AVE_FIELD(geometryTransform); AVE_FIELD(presentationTransform);
        AVE_FIELD(effectiveOpacity); AVE_FIELD(presentedBounds); AVE_FIELD(featureBits);
    }
    AVE_COMPARE_TYPE(render::MotionGeometryUpdate) {
        AVE_FIELD(planDrawItemIndex); AVE_FIELD(key); AVE_FIELD(sourceDrawItemIndex);
    }
    AVE_COMPARE_TYPE(render::MotionPaintUpdate) {
        AVE_FIELD(planDrawItemIndex); AVE_FIELD(key); AVE_FIELD(sourceDrawItemIndex);
    }
    AVE_COMPARE_TYPE(render::RenderPlanStatistics) {
        AVE_FIELD(sourceDrawItemCount); AVE_FIELD(visibleDrawItemCount);
        AVE_FIELD(geometryUpdateCount); AVE_FIELD(paintUpdateCount);
        AVE_FIELD(assetStaticGeometryCount); AVE_FIELD(instanceGeometryCount);
        AVE_FIELD(assetStaticPaintCount); AVE_FIELD(instancePaintCount);
        AVE_FIELD(unsupportedFeatureItemCount);
    }
    AVE_COMPARE_TYPE(render::RenderPlanFingerprints) {
        AVE_FIELD(plan); AVE_FIELD(topology); AVE_FIELD(geometryIdentity);
        AVE_FIELD(paintIdentity); AVE_FIELD(presentation);
    }
    AVE_COMPARE_TYPE(render::MotionRenderPlan) {
        AVE_FIELD(stamp); AVE_FIELD(sourceScene); AVE_FIELD(drawItems);
        AVE_FIELD(geometryUpdates); AVE_FIELD(paintUpdates); AVE_FIELD(presentedBounds);
        AVE_FIELD(dirtyRegion); AVE_FIELD(statistics); AVE_FIELD(fingerprints);
        AVE_FIELD(firstPlan); AVE_FIELD(visualChanged); AVE_FIELD(reusableForRepaint);
    }
#undef AVE_FIELD
#undef AVE_COMPARE_TYPE
};

} // namespace avemotion::test
