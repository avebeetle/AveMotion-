#pragma once
#include "lottieitem.h"
#include <stdexcept>

namespace avemotion::test {
namespace samsungModel = rlottie::internal::model;
inline void verifySamsungRecordingPaths() {
    using namespace rlottie::internal;
    const auto check = [](bool ok, const char* message) {
        if (!ok) throw std::runtime_error(message);
    };
    // Construct a deliberately gapped model: the parser normally joins keyframe
    // intervals. These real model/evaluator cases pin the output-untouched path
    // contract, including the empty-animation branch, without parser assumptions.
    const auto populate = [](samsungModel::Property<samsungModel::PathData>& property) {
        auto& frames = property.animation().frames_;
        frames.resize(2);
        frames[0].start_ = 0; frames[0].end_ = 1;
        frames[1].start_ = 3; frames[1].end_ = 4;
        for (auto& frame : frames) {
            frame.value_.start_.mPoints = {{0,0},{10,0},{20,0},{30,0}};
            frame.value_.end_ = frame.value_.start_;
        }
    };
    samsungModel::Path data;
    data.setStatic(false);
    populate(data.mShape);
    renderer::Path shape(&data);
    samsungModel::Mask maskData;
    populate(maskData.mShape);
    renderer::Mask mask(&maskData);
    for (int scenario = 0; scenario < 2; ++scenario) {
        shape.update(0, VMatrix{}, 1, renderer::DirtyFlagBit::All);
        mask.update(0, VMatrix{}, 1, renderer::DirtyFlagBit::All);
        check(!shape.localPath().empty() && !mask.mFinalPath.empty(), "path regression must seed geometry");
        if (scenario) {
            data.mShape.animation().frames_.clear();
            maskData.mShape.animation().frames_.clear();
        }
        shape.resetForRecording();
        mask.resetForRecording();
        shape.update(2, VMatrix{}, 1, renderer::DirtyFlagBit::All);
        mask.update(2, VMatrix{}, 1, renderer::DirtyFlagBit::All);
        renderer::Path freshShape(&data);
        renderer::Mask freshMask(&maskData);
        freshShape.update(2, VMatrix{}, 1, renderer::DirtyFlagBit::All);
        freshMask.update(2, VMatrix{}, 1, renderer::DirtyFlagBit::All);
        check(freshShape.localPath().empty() && freshMask.mFinalPath.empty(), "fresh gap/empty path is empty");
        check(shape.localPath().empty(), scenario ? "empty PathData clears retained shape" : "gapped PathData clears retained shape");
        check(mask.mFinalPath.empty(), scenario ? "empty PathData clears retained mask" : "gapped PathData clears retained mask");
    }
}

inline void verifySamsungRecordingPublication() {
    using namespace rlottie::internal;
    const auto check = [](bool ok, const char* message) {
        if (!ok) throw std::runtime_error(message);
    };
    VPath path;
    path.moveTo(0, 0); path.lineTo(80, 0); path.lineTo(80, 60);
    for (auto dash : {std::vector<float>{7, 5}, std::vector<float>{0, 0}}) {
        renderer::Drawable drawable;
        drawable.setType(VDrawable::Type::StrokeWithDash);
        auto *payload = drawable.mStrokeInfo;
        drawable.resetForRecording();
        drawable.setPath(path);
        drawable.setStrokeInfo(CapStyle::Flat, JoinStyle::Bevel, 10, 4);
        drawable.setDashInfo(dash);
        std::vector<float> publication;
        for (int repeat = 0; repeat < 2; ++repeat) {
            drawable.sync();
            check(drawable.mPath.elements() == path.elements(), "Samsung publication consumed source elements");
            check(drawable.mPath.points().size() == path.points().size(), "Samsung publication consumed source points");
            for (size_t i = 0; i < path.points().size(); ++i)
                check(drawable.mPath.points()[i].x() == path.points()[i].x() &&
                      drawable.mPath.points()[i].y() == path.points()[i].y(), "Samsung publication altered source coordinates");
            check(bool(drawable.mFlag & VDrawable::DirtyState::Path), "Samsung publication consumed evaluator dirty state");
            const auto &published = drawable.mCNode->mPath;
            std::vector<float> points(published.ptPtr, published.ptPtr + published.ptCount);
            if (dash[0] == 0) {
                check(published.elmCount == 3 && published.ptCount == 6,
                      "Samsung zero dash retains original path through value-returning overload");
            } else {
                check(points.size() > path.points().size()*2, "Samsung test dash must subdivide path");
            }
            if (repeat) check(points == publication, "Samsung repeated publication changes dash output");
            publication = std::move(points);
        }
        drawable.resetForRecording();
        check(drawable.mStrokeInfo == payload, "Samsung reset retains typed stroke allocation");
        check(payload->width == 0 && payload->miterLimit == 10 &&
              payload->cap == CapStyle::Flat && payload->join == JoinStyle::Bevel &&
              static_cast<VDrawable::StrokeWithDashInfo*>(payload)->mDash.empty(),
              "Samsung reset restores typed stroke defaults");
    }
}
}
