/*
 * Trim-path compatibility implementation derived from the behaviour of
 * Samsung/Telegram rlottie VPathMesure, VDasher, VBezier and VLine.
 *
 * The original implementation is licensed under LGPL-2.1-or-later. This
 * adapted backend-neutral implementation is distributed with the same
 * notice; see LICENSES/LGPL-2.1.txt and NOTICE.md.
 */

#include "TrimPathGenerator.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace avemotion::render::detail {
namespace {

constexpr float kTelegramEpsilon = 0.000001F;
constexpr float kBezierLengthError = 0.01F;
constexpr std::size_t kMaximumBezierDepth = 64U;
constexpr std::size_t kMaximumLengthSearchIterations = 1024U;

template <typename T>
[[nodiscard]] bool reserveAndReport(
    std::vector<T>& values,
    std::size_t capacity) {
    const auto before = values.capacity();
    if (capacity > before) values.reserve(capacity);
    return values.capacity() != before;
}

[[nodiscard]] bool telegramCompare(float left, float right) noexcept {
    return std::fabs(left - right) < kTelegramEpsilon;
}

[[nodiscard]] bool telegramIsZero(float value) noexcept {
    return std::fabs(value) <= kTelegramEpsilon;
}

[[nodiscard]] bool finite(model::MotionVec2Value value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y);
}

[[nodiscard]] float lineLength(
    model::MotionVec2Value first,
    model::MotionVec2Value second) noexcept {
    float x = second.x - first.x;
    float y = second.y - first.y;
    x = x < 0.0F ? -x : x;
    y = y < 0.0F ? -y : y;
    return x > y ? x + 0.375F * y : y + 0.375F * x;
}

struct Line final {
    model::MotionVec2Value first;
    model::MotionVec2Value second;

    [[nodiscard]] float length() const noexcept {
        return lineLength(first, second);
    }

    [[nodiscard]] bool splitAtLength(
        float lengthAt,
        Line& left,
        Line& right) const noexcept {
        const float total = length();
        if (!std::isfinite(total) || total <= 0.0F
            || !std::isfinite(lengthAt)) {
            return false;
        }
        const float dx = ((second.x - first.x) / total) * lengthAt;
        const float dy = ((second.y - first.y) / total) * lengthAt;
        left = {first, {first.x + dx, first.y + dy}};
        right = {left.second, second};
        return finite(left.second);
    }
};

struct Bezier final {
    model::MotionVec2Value p1;
    model::MotionVec2Value p2;
    model::MotionVec2Value p3;
    model::MotionVec2Value p4;

    void split(Bezier& firstHalf, Bezier& secondHalf) const noexcept {
        float center = (p2.x + p3.x) * 0.5F;
        firstHalf.p2.x = (p1.x + p2.x) * 0.5F;
        secondHalf.p3.x = (p3.x + p4.x) * 0.5F;
        firstHalf.p1.x = p1.x;
        secondHalf.p4.x = p4.x;
        firstHalf.p3.x = (firstHalf.p2.x + center) * 0.5F;
        secondHalf.p2.x = (secondHalf.p3.x + center) * 0.5F;
        firstHalf.p4.x = secondHalf.p1.x =
            (firstHalf.p3.x + secondHalf.p2.x) * 0.5F;

        center = (p2.y + p3.y) * 0.5F;
        firstHalf.p2.y = (p1.y + p2.y) * 0.5F;
        secondHalf.p3.y = (p3.y + p4.y) * 0.5F;
        firstHalf.p1.y = p1.y;
        secondHalf.p4.y = p4.y;
        firstHalf.p3.y = (firstHalf.p2.y + center) * 0.5F;
        secondHalf.p2.y = (secondHalf.p3.y + center) * 0.5F;
        firstHalf.p4.y = secondHalf.p1.y =
            (firstHalf.p3.y + secondHalf.p2.y) * 0.5F;
    }

    void parameterSplitLeft(float t, Bezier& left) noexcept {
        left.p1 = p1;
        left.p2 = {
            p1.x + t * (p2.x - p1.x),
            p1.y + t * (p2.y - p1.y),
        };
        left.p3 = {
            p2.x + t * (p3.x - p2.x),
            p2.y + t * (p3.y - p2.y),
        };
        p3 = {
            p3.x + t * (p4.x - p3.x),
            p3.y + t * (p4.y - p3.y),
        };
        p2 = {
            left.p3.x + t * (p3.x - left.p3.x),
            left.p3.y + t * (p3.y - left.p3.y),
        };
        left.p3 = {
            left.p2.x + t * (left.p3.x - left.p2.x),
            left.p2.y + t * (left.p3.y - left.p2.y),
        };
        left.p4 = p1 = {
            left.p3.x + t * (p2.x - left.p3.x),
            left.p3.y + t * (p2.y - left.p3.y),
        };
    }

    [[nodiscard]] float length(std::size_t depth = 0U) const noexcept {
        float controlLength = 0.0F;
        controlLength += lineLength(p1, p2);
        controlLength += lineLength(p2, p3);
        controlLength += lineLength(p3, p4);
        const float chord = lineLength(p1, p4);
        if (!std::isfinite(controlLength) || !std::isfinite(chord)) {
            return std::numeric_limits<float>::quiet_NaN();
        }
        if ((controlLength - chord) > kBezierLengthError
            && depth < kMaximumBezierDepth) {
            Bezier left;
            Bezier right;
            split(left, right);
            return left.length(depth + 1U) + right.length(depth + 1U);
        }
        return controlLength;
    }

    [[nodiscard]] bool parameterAtLength(float requested, float& t) const noexcept {
        const float total = length();
        if (!std::isfinite(total) || !std::isfinite(requested)
            || requested < 0.0F) {
            return false;
        }
        t = 1.0F;
        if (requested > total || telegramCompare(requested, total)) {
            return true;
        }

        t *= 0.5F;
        float lastBigger = 1.0F;
        for (std::size_t iteration = 0U;
             iteration < kMaximumLengthSearchIterations; ++iteration) {
            Bezier right = *this;
            Bezier left;
            right.parameterSplitLeft(t, left);
            const float leftLength = left.length();
            if (!std::isfinite(leftLength)) return false;
            if (std::fabs(leftLength - requested) < kBezierLengthError) {
                return true;
            }
            if (leftLength < requested) {
                t += (lastBigger - t) * 0.5F;
            } else {
                lastBigger = t;
                t -= t * 0.5F;
            }
        }
        return false;
    }

    [[nodiscard]] bool splitAtLength(
        float requested,
        Bezier& left,
        Bezier& right) const noexcept {
        right = *this;
        float t = 0.0F;
        if (!right.parameterAtLength(requested, t)) return false;
        right.parameterSplitLeft(t, left);
        return finite(left.p1) && finite(left.p2) && finite(left.p3)
            && finite(left.p4) && finite(right.p1) && finite(right.p2)
            && finite(right.p3) && finite(right.p4);
    }
};

[[nodiscard]] bool validatePath(
    std::span<const runtime::PathVerb> verbs,
    std::span<const model::MotionVec2Value> points,
    std::size_t& contourCount) noexcept {
    std::size_t pointIndex = 0U;
    bool hasCurrent = false;
    contourCount = 0U;
    for (const auto verb : verbs) {
        switch (verb) {
        case runtime::PathVerb::MoveTo:
            if (pointIndex >= points.size() || !finite(points[pointIndex])) {
                return false;
            }
            ++pointIndex;
            ++contourCount;
            hasCurrent = true;
            break;
        case runtime::PathVerb::LineTo:
            if (!hasCurrent || pointIndex >= points.size()
                || !finite(points[pointIndex])) {
                return false;
            }
            ++pointIndex;
            break;
        case runtime::PathVerb::CubicTo:
            if (!hasCurrent || pointIndex + 3U > points.size()) return false;
            if (!finite(points[pointIndex]) || !finite(points[pointIndex + 1U])
                || !finite(points[pointIndex + 2U])) {
                return false;
            }
            pointIndex += 3U;
            break;
        case runtime::PathVerb::Close:
            if (!hasCurrent) return false;
            break;
        }
    }
    return pointIndex == points.size();
}

[[nodiscard]] float pathLength(
    std::span<const runtime::PathVerb> verbs,
    std::span<const model::MotionVec2Value> points) noexcept {
    std::size_t pointIndex = 0U;
    model::MotionVec2Value current;
    float result = 0.0F;
    for (const auto verb : verbs) {
        switch (verb) {
        case runtime::PathVerb::MoveTo:
            current = points[pointIndex++];
            break;
        case runtime::PathVerb::LineTo: {
            const auto next = points[pointIndex++];
            result += lineLength(current, next);
            current = next;
            break;
        }
        case runtime::PathVerb::CubicTo: {
            const Bezier bezier{
                current,
                points[pointIndex],
                points[pointIndex + 1U],
                points[pointIndex + 2U],
            };
            const float value = bezier.length();
            if (!std::isfinite(value)) {
                return std::numeric_limits<float>::quiet_NaN();
            }
            result += value;
            current = points[pointIndex + 2U];
            pointIndex += 3U;
            break;
        }
        case runtime::PathVerb::Close:
            // rlottie's VPath stores the closing segment in its point stream
            // when needed. VDasher intentionally ignores the Close element.
            break;
        }
        if (!std::isfinite(result)) {
            return std::numeric_limits<float>::quiet_NaN();
        }
    }
    return result;
}

class PathWriter final {
public:
    explicit PathWriter(TrimmedPath& output) : output_(output) {}

    void moveTo(model::MotionVec2Value point) {
        output_.verbs.push_back(runtime::PathVerb::MoveTo);
        output_.points.push_back(point);
    }

    void lineTo(model::MotionVec2Value point) {
        output_.verbs.push_back(runtime::PathVerb::LineTo);
        output_.points.push_back(point);
    }

    void cubicTo(
        model::MotionVec2Value control1,
        model::MotionVec2Value control2,
        model::MotionVec2Value end) {
        output_.verbs.push_back(runtime::PathVerb::CubicTo);
        output_.points.push_back(control1);
        output_.points.push_back(control2);
        output_.points.push_back(end);
    }

private:
    TrimmedPath& output_;
};

class Dasher final {
public:
    struct Dash final {
        float length = 0.0F;
        float gap = 0.0F;
    };

    Dasher(std::array<Dash, 2U> pattern, TrimmedPath& output)
        : pattern_(pattern), output_(output), writer_(output) {
        for (const auto& value : pattern_) {
            if (!telegramCompare(value.length, 0.0F)) noLength_ = false;
            if (!telegramCompare(value.gap, 0.0F)) noGap_ = false;
        }
    }

    [[nodiscard]] bool run(
        std::span<const runtime::PathVerb> verbs,
        std::span<const model::MotionVec2Value> points) {
        if (noLength_ || noGap_) return false;
        std::size_t pointIndex = 0U;
        for (const auto verb : verbs) {
            switch (verb) {
            case runtime::PathVerb::MoveTo:
                if (!moveTo(points[pointIndex++])) return false;
                break;
            case runtime::PathVerb::LineTo:
                if (!lineTo(points[pointIndex++])) return false;
                break;
            case runtime::PathVerb::CubicTo:
                if (!cubicTo(
                        points[pointIndex],
                        points[pointIndex + 1U],
                        points[pointIndex + 2U])) {
                    return false;
                }
                pointIndex += 3U;
                break;
            case runtime::PathVerb::Close:
                break;
            }
        }
        return pointIndex == points.size();
    }

private:
    [[nodiscard]] bool advanceActiveSegment() noexcept {
        startNewSegment_ = true;
        for (std::size_t guard = 0U; guard < pattern_.size() * 4U; ++guard) {
            if (discard_) {
                discard_ = false;
                index_ = (index_ + 1U) % pattern_.size();
                currentLength_ = pattern_[index_].length;
            } else {
                discard_ = true;
                currentLength_ = pattern_[index_].gap;
            }
            if (!telegramIsZero(currentLength_)) return true;
        }
        return false;
    }

    [[nodiscard]] bool moveTo(model::MotionVec2Value point) noexcept {
        discard_ = false;
        startNewSegment_ = true;
        current_ = point;
        index_ = 0U;
        currentLength_ = pattern_[index_].length;
        if (telegramIsZero(currentLength_) && !advanceActiveSegment()) {
            return false;
        }
        return true;
    }

    void addLine(model::MotionVec2Value point) {
        if (discard_) return;
        if (startNewSegment_) {
            writer_.moveTo(current_);
            startNewSegment_ = false;
        }
        writer_.lineTo(point);
    }

    void addCubic(
        model::MotionVec2Value control1,
        model::MotionVec2Value control2,
        model::MotionVec2Value end) {
        if (discard_) return;
        if (startNewSegment_) {
            writer_.moveTo(current_);
            startNewSegment_ = false;
        }
        writer_.cubicTo(control1, control2, end);
    }

    [[nodiscard]] bool lineTo(model::MotionVec2Value point) {
        Line line{current_, point};
        float length = line.length();
        if (!std::isfinite(length)) return false;

        if (length <= currentLength_) {
            currentLength_ -= length;
            addLine(point);
        } else {
            while (length > currentLength_) {
                length -= currentLength_;
                Line left;
                Line right;
                if (!line.splitAtLength(currentLength_, left, right)) return false;
                addLine(left.second);
                ++output_.splitCount;
                if (!advanceActiveSegment()) return false;
                line = right;
                current_ = line.first;
            }
            if (length > 1.0F) {
                currentLength_ -= length;
                addLine(line.second);
            }
        }

        if (currentLength_ < 1.0F && !advanceActiveSegment()) return false;
        current_ = point;
        return true;
    }

    [[nodiscard]] bool cubicTo(
        model::MotionVec2Value control1,
        model::MotionVec2Value control2,
        model::MotionVec2Value end) {
        Bezier bezier{current_, control1, control2, end};
        float length = bezier.length();
        if (!std::isfinite(length)) return false;

        if (length <= currentLength_) {
            currentLength_ -= length;
            addCubic(control1, control2, end);
        } else {
            while (length > currentLength_) {
                length -= currentLength_;
                Bezier left;
                Bezier right;
                if (!bezier.splitAtLength(currentLength_, left, right)) {
                    return false;
                }
                addCubic(left.p2, left.p3, left.p4);
                ++output_.splitCount;
                if (!advanceActiveSegment()) return false;
                bezier = right;
                current_ = bezier.p1;
            }
            if (length > 1.0F) {
                currentLength_ -= length;
                addCubic(bezier.p2, bezier.p3, bezier.p4);
            }
        }

        if (currentLength_ < 1.0F && !advanceActiveSegment()) return false;
        current_ = end;
        return true;
    }

    std::array<Dash, 2U> pattern_{};
    TrimmedPath& output_;
    PathWriter writer_;
    model::MotionVec2Value current_{};
    std::size_t index_ = 0U;
    float currentLength_ = 0.0F;
    bool discard_ = false;
    bool startNewSegment_ = true;
    bool noLength_ = true;
    bool noGap_ = true;
};

} // namespace

void TrimmedPath::prepare(
    std::size_t verbCapacity,
    std::size_t pointCapacity) {
    bool changed = reserveAndReport(verbs, verbCapacity);
    changed = reserveAndReport(points, pointCapacity) || changed;
    if (changed) ++storageGeneration;
}

void TrimmedPath::reset() noexcept {
    verbs.clear();
    points.clear();
    sourceLength = 0.0F;
    contourCount = 0U;
    splitCount = 0U;
    valid = false;
    empty = true;
    full = false;
}

void TrimmedPathCollection::prepare(
    std::size_t pathCapacity,
    std::size_t verbCapacity,
    std::size_t pointCapacity,
    std::size_t singlePathVerbCapacity,
    std::size_t singlePathPointCapacity) {
    bool changed = reserveAndReport(verbs, verbCapacity);
    changed = reserveAndReport(points, pointCapacity) || changed;
    changed = reserveAndReport(paths, pathCapacity) || changed;
    changed = reserveAndReport(sourceLengths, pathCapacity) || changed;
    const auto scratchGeneration = scratch.storageGeneration;
    scratch.prepare(singlePathVerbCapacity, singlePathPointCapacity);
    changed = scratch.storageGeneration != scratchGeneration || changed;
    if (changed) ++storageGeneration;
}

void TrimmedPathCollection::reset() noexcept {
    verbs.clear();
    points.clear();
    paths.clear();
    sourceLengths.clear();
    scratch.reset();
    splitCount = 0U;
    valid = false;
    empty = true;
    full = false;
    individualWrappedNoOp = false;
}

std::span<const runtime::PathVerb> TrimmedPathCollection::pathVerbs(
    std::size_t index) const noexcept {
    if (index >= paths.size()) return {};
    const auto& path = paths[index];
    if (path.firstVerb > verbs.size()
        || path.verbCount > verbs.size() - path.firstVerb) {
        return {};
    }
    return {verbs.data() + path.firstVerb, path.verbCount};
}

std::span<const model::MotionVec2Value> TrimmedPathCollection::pathPoints(
    std::size_t index) const noexcept {
    if (index >= paths.size()) return {};
    const auto& path = paths[index];
    if (path.firstPoint > points.size()
        || path.pointCount > points.size() - path.firstPoint) {
        return {};
    }
    return {points.data() + path.firstPoint, path.pointCount};
}

NormalizedTrimSegment normalizeTrimSegment(
    float startPercent,
    float endPercent,
    float offsetDegrees) noexcept {
    NormalizedTrimSegment result;
    if (!std::isfinite(startPercent) || !std::isfinite(endPercent)
        || !std::isfinite(offsetDegrees)) {
        return result;
    }

    float start = startPercent / 100.0F;
    float end = endPercent / 100.0F;
    const float offset = std::fmod(offsetDegrees, 360.0F) / 360.0F;
    const float difference = std::fabs(start - end);
    if (telegramCompare(difference, 0.0F)) {
        result = {0.0F, 0.0F, true, true, false};
        return result;
    }
    if (telegramCompare(difference, 1.0F)) {
        result = {0.0F, 1.0F, true, false, true};
        return result;
    }

    auto noLoop = [&result](float first, float second) noexcept {
        result.start = std::min(first, second);
        result.end = std::max(first, second);
    };
    auto loop = [&result](float first, float second) noexcept {
        result.start = std::max(first, second);
        result.end = std::min(first, second);
    };

    if (offset > 0.0F) {
        start += offset;
        end += offset;
        if (start <= 1.0F && end <= 1.0F) {
            noLoop(start, end);
        } else if (start > 1.0F && end > 1.0F) {
            noLoop(start - 1.0F, end - 1.0F);
        } else if (start > 1.0F) {
            loop(start - 1.0F, end);
        } else {
            loop(start, end - 1.0F);
        }
    } else {
        start += offset;
        end += offset;
        if (start >= 0.0F && end >= 0.0F) {
            noLoop(start, end);
        } else if (start < 0.0F && end < 0.0F) {
            noLoop(1.0F + start, 1.0F + end);
        } else if (start < 0.0F) {
            loop(1.0F + start, end);
        } else {
            loop(start, 1.0F + end);
        }
    }

    result.valid = std::isfinite(result.start) && std::isfinite(result.end)
        && result.start >= 0.0F && result.start <= 1.0F
        && result.end >= 0.0F && result.end <= 1.0F;
    result.empty = result.valid && telegramCompare(result.start, result.end);
    result.full = result.valid
        && telegramCompare(std::fabs(result.start - result.end), 1.0F);
    return result;
}

float measurePath(
    std::span<const runtime::PathVerb> verbs,
    std::span<const model::MotionVec2Value> points,
    bool& valid) noexcept {
    valid = false;
    std::size_t contourCount = 0U;
    if (!validatePath(verbs, points, contourCount)) {
        return std::numeric_limits<float>::quiet_NaN();
    }
    const float result = pathLength(verbs, points);
    valid = std::isfinite(result);
    return result;
}

bool trimPathInto(
    std::span<const runtime::PathVerb> verbs,
    std::span<const model::MotionVec2Value> points,
    const NormalizedTrimSegment& segment,
    TrimmedPath& result) {
    const auto generationBefore = result.storageGeneration;
    const auto verbCapacityBefore = result.verbs.capacity();
    const auto pointCapacityBefore = result.points.capacity();
    result.reset();
    if (!segment.valid) return false;
    if (!validatePath(verbs, points, result.contourCount)) return false;

    result.empty = segment.empty;
    result.full = segment.full;
    if (segment.empty) {
        result.valid = true;
        return true;
    }
    if (segment.full) {
        result.verbs.assign(verbs.begin(), verbs.end());
        result.points.assign(points.begin(), points.end());
        result.sourceLength = pathLength(verbs, points);
        result.valid = std::isfinite(result.sourceLength);
        result.empty = false;
        if (result.verbs.capacity() != verbCapacityBefore
            || result.points.capacity() != pointCapacityBefore) {
            result.storageGeneration = generationBefore + 1U;
        }
        return result.valid;
    }

    result.sourceLength = pathLength(verbs, points);
    if (!std::isfinite(result.sourceLength) || result.sourceLength <= 0.0F) {
        return false;
    }

    const float maximum = std::numeric_limits<float>::max();
    std::array<Dasher::Dash, 2U> pattern{};
    if (segment.start < segment.end) {
        pattern = {{
            {0.0F, result.sourceLength * segment.start},
            {(segment.end - segment.start) * result.sourceLength, maximum},
        }};
    } else {
        pattern = {{
            {result.sourceLength * segment.end,
             (segment.start - segment.end) * result.sourceLength},
            {(1.0F - segment.start) * result.sourceLength, maximum},
        }};
    }

    result.verbs.reserve(verbs.size() + 4U);
    result.points.reserve(points.size() + 8U);
    Dasher dasher{pattern, result};
    if (!dasher.run(verbs, points)) {
        result.verbs.clear();
        result.points.clear();
        if (result.verbs.capacity() != verbCapacityBefore
            || result.points.capacity() != pointCapacityBefore) {
            result.storageGeneration = generationBefore + 1U;
        }
        return false;
    }
    result.valid = true;
    result.empty = result.verbs.empty();
    result.full = false;
    if (result.verbs.capacity() != verbCapacityBefore
        || result.points.capacity() != pointCapacityBefore) {
        result.storageGeneration = generationBefore + 1U;
    }
    return true;
}

TrimmedPath trimPath(
    std::span<const runtime::PathVerb> verbs,
    std::span<const model::MotionVec2Value> points,
    const NormalizedTrimSegment& segment) {
    TrimmedPath result;
    (void)trimPathInto(verbs, points, segment, result);
    return result;
}

bool trimPathCollection(
    std::span<const TrimPathInput> inputPaths,
    const NormalizedTrimSegment& segment,
    TrimPathCollectionMode mode,
    TrimmedPathCollection& output) {
    const auto verbsCapacity = output.verbs.capacity();
    const auto pointsCapacity = output.points.capacity();
    const auto pathsCapacity = output.paths.capacity();
    const auto lengthsCapacity = output.sourceLengths.capacity();
    const auto scratchGeneration = output.scratch.storageGeneration;
    output.reset();
    if (!segment.valid) return false;

    output.paths.reserve(inputPaths.size());
    output.sourceLengths.reserve(inputPaths.size());
    float totalLength = 0.0F;
    for (const auto& input : inputPaths) {
        bool validLength = false;
        const float length = measurePath(input.verbs, input.points, validLength);
        if (!validLength || length < 0.0F) return false;
        output.sourceLengths.push_back(length);
        totalLength += length;
        if (!std::isfinite(totalLength)) return false;
    }

    auto appendResult = [&](const TrimmedPath& path) {
        const TrimmedPathSlice slice{
            output.verbs.size(),
            path.verbs.size(),
            output.points.size(),
            path.points.size(),
            path.sourceLength,
            path.empty,
            path.full,
        };
        output.verbs.insert(
            output.verbs.end(), path.verbs.begin(), path.verbs.end());
        output.points.insert(
            output.points.end(), path.points.begin(), path.points.end());
        output.paths.push_back(slice);
        output.splitCount += path.splitCount;
    };

    auto appendOriginal = [&](const TrimPathInput& input, float length) {
        const TrimmedPathSlice slice{
            output.verbs.size(),
            input.verbs.size(),
            output.points.size(),
            input.points.size(),
            length,
            input.verbs.empty(),
            true,
        };
        output.verbs.insert(
            output.verbs.end(), input.verbs.begin(), input.verbs.end());
        output.points.insert(
            output.points.end(), input.points.begin(), input.points.end());
        output.paths.push_back(slice);
    };

    auto appendEmpty = [&](float length) {
        output.paths.push_back({
            output.verbs.size(),
            0U,
            output.points.size(),
            0U,
            length,
            true,
            false,
        });
    };

    if (mode == TrimPathCollectionMode::Simultaneous) {
        for (const auto& input : inputPaths) {
            if (!trimPathInto(
                    input.verbs, input.points, segment, output.scratch)) {
                return false;
            }
            appendResult(output.scratch);
        }
    } else {
        if (segment.empty) {
            for (const float length : output.sourceLengths) appendEmpty(length);
        } else if (segment.full || segment.start > segment.end) {
            output.individualWrappedNoOp = segment.start > segment.end;
            for (std::size_t index = 0U; index < inputPaths.size(); ++index) {
                appendOriginal(inputPaths[index], output.sourceLengths[index]);
            }
        } else {
            const float start = totalLength * segment.start;
            const float end = totalLength * segment.end;
            if (!std::isfinite(start) || !std::isfinite(end)) return false;

            float currentLength = 0.0F;
            for (std::size_t index = 0U; index < inputPaths.size(); ++index) {
                const auto& input = inputPaths[index];
                const float length = output.sourceLengths[index];
                if (currentLength > end) {
                    appendEmpty(length);
                    continue;
                }

                if (currentLength < start
                    && currentLength + length < start) {
                    currentLength += length;
                    appendEmpty(length);
                    continue;
                }

                if (start <= currentLength
                    && end >= currentLength + length) {
                    currentLength += length;
                    appendOriginal(input, length);
                    continue;
                }

                if (length <= 0.0F) return false;
                float localStart = start > currentLength
                    ? start - currentLength : 0.0F;
                localStart /= length;
                float localEnd = currentLength + length < end
                    ? length : end - currentLength;
                localEnd /= length;
                const NormalizedTrimSegment local{
                    localStart,
                    localEnd,
                    true,
                    telegramCompare(localStart, localEnd),
                    telegramCompare(std::fabs(localStart - localEnd), 1.0F),
                };
                if (!trimPathInto(
                        input.verbs, input.points, local, output.scratch)) {
                    return false;
                }
                appendResult(output.scratch);
                currentLength += length;
            }
        }
    }

    output.valid = output.paths.size() == inputPaths.size();
    output.empty = output.valid;
    output.full = output.valid && !output.individualWrappedNoOp;
    for (const auto& path : output.paths) {
        output.empty = output.empty && path.empty;
        output.full = output.full && path.full;
    }

    if (output.verbs.capacity() != verbsCapacity
        || output.points.capacity() != pointsCapacity
        || output.paths.capacity() != pathsCapacity
        || output.sourceLengths.capacity() != lengthsCapacity
        || output.scratch.storageGeneration != scratchGeneration) {
        ++output.storageGeneration;
    }
    return output.valid;
}

} // namespace avemotion::render::detail
