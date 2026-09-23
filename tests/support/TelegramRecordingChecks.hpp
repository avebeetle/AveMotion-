#pragma once

#include "lottieitem.h"
#include <stdexcept>

namespace avemotion::test {

// Public borrowed trees cannot expose the evaluator's undashed path. This
// Telegram-only check exercises the actual drawable publisher twice and checks
// the source values directly, without adding a test hook to production.
inline void verifyTelegramRecordingPublication() {
    const auto check = [](bool ok, const char* message) {
        if (!ok) throw std::runtime_error(message);
    };
    LOTDrawable drawable;
    drawable.resetForRecording();
    VPath path;
    path.moveTo(0, 0); path.lineTo(80, 0); path.lineTo(80, 60);
    drawable.setPath(path);
    drawable.setAveLocalGeometry(path, VMatrix{}, true);
    drawable.setStrokeInfo(CapStyle::Flat, JoinStyle::Bevel, 10, 4);
    std::vector<float> dash{7, 5};
    drawable.setDashInfo(dash);
    const auto sourceElements = drawable.mPath.elements();
    const auto sourcePoints = drawable.mPath.points();
    std::vector<float> publication;
    for (int repeat = 0; repeat < 2; ++repeat) {
        drawable.sync();
        check(drawable.mPath.elements() == sourceElements, "recording publication consumed source elements");
        check(drawable.mPath.points().size() == sourcePoints.size(), "recording publication consumed source points");
        for (size_t i = 0; i < sourcePoints.size(); ++i) {
            check(drawable.mPath.points()[i].x() == sourcePoints[i].x() &&
                  drawable.mPath.points()[i].y() == sourcePoints[i].y(), "recording publication altered source coordinates");
            check(drawable.mAveLocalPath.points()[i].x() == sourcePoints[i].x() &&
                  drawable.mAveLocalPath.points()[i].y() == sourcePoints[i].y(), "recording publication altered source-local coordinates");
        }
        check(bool(drawable.mFlag & VDrawable::DirtyState::Path), "recording publication consumed evaluator dirty state");
        const auto& published = drawable.mCNode->mPath;
        std::vector<float> points(published.ptPtr, published.ptPtr + published.ptCount);
        check(points.size() > sourcePoints.size()*2, "test dash must subdivide source path");
        if (repeat) check(points == publication, "repeated raw publication changes dashed output");
        publication = std::move(points);
    }
}
}
