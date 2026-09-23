#include "avemotion/evaluation/PropertyEvaluator.hpp"

#include "avemotion/core/Hash.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

namespace avemotion::evaluation {
namespace {

constexpr std::uint32_t kInvalidIndex = model::kInvalidModelId;
constexpr int kNewtonIterations = 4;
constexpr float kNewtonMinimumSlope = 0.02F;
constexpr float kSubdivisionPrecision = 0.0000001F;
constexpr int kSubdivisionMaximumIterations = 10;
constexpr std::size_t kSplineTableSize = 11U;
constexpr float kSplineStep = 1.0F / static_cast<float>(kSplineTableSize - 1U);
constexpr float kDegreesToRadians = 0.017453292519943295769F;
constexpr float kSpatialLengthTolerance = 0.01F;
constexpr int kSpatialMaximumLengthSearchIterations = 256;
constexpr float kTelegramPi = 3.141592F;

[[nodiscard]] float canonicalFloat(float value) noexcept {
    return value == 0.0F ? 0.0F : value;
}

[[nodiscard]] model::MotionVec2Value canonicalVec2(
    model::MotionVec2Value value) noexcept {
    value.x = canonicalFloat(value.x);
    value.y = canonicalFloat(value.y);
    return value;
}

[[nodiscard]] model::MotionColorValue canonicalColor(
    model::MotionColorValue value) noexcept {
    value.r = canonicalFloat(value.r);
    value.g = canonicalFloat(value.g);
    value.b = canonicalFloat(value.b);
    value.a = canonicalFloat(value.a);
    return value;
}

[[nodiscard]] model::MotionMatrix3x2Value canonicalMatrix(
    model::MotionMatrix3x2Value value) noexcept {
    value.m11 = canonicalFloat(value.m11);
    value.m12 = canonicalFloat(value.m12);
    value.m21 = canonicalFloat(value.m21);
    value.m22 = canonicalFloat(value.m22);
    value.dx = canonicalFloat(value.dx);
    value.dy = canonicalFloat(value.dy);
    return value;
}

struct CubicBezierRuntime final {
    float x1 = 0.0F;
    float y1 = 0.0F;
    float x2 = 1.0F;
    float y2 = 1.0F;
    std::array<float, kSplineTableSize> samples{};
    bool linear = true;

    [[nodiscard]] static float coefficientA(float first, float second) noexcept {
        return 1.0F - 3.0F * second + 3.0F * first;
    }

    [[nodiscard]] static float coefficientB(float first, float second) noexcept {
        return 3.0F * second - 6.0F * first;
    }

    [[nodiscard]] static float coefficientC(float first) noexcept {
        return 3.0F * first;
    }

    [[nodiscard]] static float bezier(
        float t,
        float first,
        float second) noexcept {
        return ((coefficientA(first, second) * t
                    + coefficientB(first, second)) * t
                    + coefficientC(first)) * t;
    }

    [[nodiscard]] static float slope(
        float t,
        float first,
        float second) noexcept {
        return 3.0F * coefficientA(first, second) * t * t
            + 2.0F * coefficientB(first, second) * t
            + coefficientC(first);
    }

    void initialize(model::MotionVec2Value first, model::MotionVec2Value second) noexcept {
        x1 = first.x;
        y1 = first.y;
        x2 = second.x;
        y2 = second.y;
        linear = x1 == y1 && x2 == y2;
        if (linear) return;
        for (std::size_t index = 0; index < samples.size(); ++index) {
            samples[index] = bezier(
                static_cast<float>(index) * kSplineStep,
                x1,
                x2);
        }
    }

    [[nodiscard]] float binarySubdivide(
        float x,
        float first,
        float second) const noexcept {
        float currentX = 0.0F;
        float currentT = first;
        int iteration = 0;
        do {
            currentT = first + (second - first) / 2.0F;
            currentX = bezier(currentT, x1, x2) - x;
            if (currentX > 0.0F) {
                second = currentT;
            } else {
                first = currentT;
            }
        } while (std::fabs(currentX) > kSubdivisionPrecision
                 && ++iteration < kSubdivisionMaximumIterations);
        return currentT;
    }

    [[nodiscard]] float newton(float x, float guess) const noexcept {
        for (int iteration = 0; iteration < kNewtonIterations; ++iteration) {
            const float currentX = bezier(guess, x1, x2) - x;
            const float currentSlope = slope(guess, x1, x2);
            if (currentSlope == 0.0F) return guess;
            guess -= currentX / currentSlope;
        }
        return guess;
    }

    [[nodiscard]] float parameterForX(float x) const noexcept {
        float intervalStart = 0.0F;
        std::size_t sample = 1U;
        while (sample + 1U < samples.size() && samples[sample] <= x) {
            ++sample;
            intervalStart += kSplineStep;
        }
        const std::size_t lowerIndex = sample - 1U;
        const float lower = samples[lowerIndex];
        const float upper = samples[lowerIndex + 1U];
        const float denominator = upper - lower;
        const float distance = denominator != 0.0F
            ? (x - lower) / denominator
            : 0.0F;
        const float guess = intervalStart + distance * kSplineStep;
        const float initialSlope = slope(guess, x1, x2);
        if (initialSlope >= kNewtonMinimumSlope) {
            return newton(x, guess);
        }
        if (initialSlope == 0.0F) return guess;
        return binarySubdivide(x, intervalStart, intervalStart + kSplineStep);
    }

    [[nodiscard]] float value(float x) const noexcept {
        if (linear) return x;
        if (x <= 0.0F) return 0.0F;
        if (x >= 1.0F) return 1.0F;
        return bezier(parameterForX(x), y1, y2);
    }
};

// Mirrors the mature rlottie spatial-keyframe behavior: the temporal easing
// produces a normalized distance, then a cubic path is sampled at that arc
// length. rlottie intentionally uses its inexpensive alpha-max/beta-min line
// metric and recursive subdivision; preserving those choices gives us exact
// behavioral parity instead of a visually similar approximation.
struct SpatialBezierRuntime final {
    model::MotionVec2Value p0;
    model::MotionVec2Value p1;
    model::MotionVec2Value p2;
    model::MotionVec2Value p3;
    float totalLength = 0.0F;
    bool valid = false;

    [[nodiscard]] static float lineLength(
        model::MotionVec2Value first,
        model::MotionVec2Value second) noexcept {
        float x = second.x - first.x;
        float y = second.y - first.y;
        x = x < 0.0F ? -x : x;
        y = y < 0.0F ? -y : y;
        return x > y ? x + 0.375F * y : y + 0.375F * x;
    }

    [[nodiscard]] model::MotionVec2Value pointAt(float t) const noexcept {
        const float mt = 1.0F - t;

        float ax = p0.x * mt + p1.x * t;
        float bx = p1.x * mt + p2.x * t;
        float cx = p2.x * mt + p3.x * t;
        ax = ax * mt + bx * t;
        bx = bx * mt + cx * t;
        const float x = ax * mt + bx * t;

        float ay = p0.y * mt + p1.y * t;
        float by = p1.y * mt + p2.y * t;
        float cy = p2.y * mt + p3.y * t;
        ay = ay * mt + by * t;
        by = by * mt + cy * t;
        const float y = ay * mt + by * t;

        return canonicalVec2({x, y});
    }

    [[nodiscard]] model::MotionVec2Value derivative(float t) const noexcept {
        const float mt = 1.0F - t;
        const float d = t * t;
        const float a = -mt * mt;
        const float b = 1.0F - 4.0F * t + 3.0F * d;
        const float c = 2.0F * t - 3.0F * d;
        return {
            3.0F * (a * p0.x + b * p1.x + c * p2.x + d * p3.x),
            3.0F * (a * p0.y + b * p1.y + c * p2.y + d * p3.y),
        };
    }

    void split(
        SpatialBezierRuntime& first,
        SpatialBezierRuntime& second) const noexcept {
        const float cx = (p1.x + p2.x) * 0.5F;
        first.p1.x = (p0.x + p1.x) * 0.5F;
        second.p2.x = (p2.x + p3.x) * 0.5F;
        first.p0.x = p0.x;
        second.p3.x = p3.x;
        first.p2.x = (first.p1.x + cx) * 0.5F;
        second.p1.x = (second.p2.x + cx) * 0.5F;
        first.p3.x = second.p0.x =
            (first.p2.x + second.p1.x) * 0.5F;

        const float cy = (p1.y + p2.y) * 0.5F;
        first.p1.y = (p0.y + p1.y) * 0.5F;
        second.p2.y = (p2.y + p3.y) * 0.5F;
        first.p0.y = p0.y;
        second.p3.y = p3.y;
        first.p2.y = (first.p1.y + cy) * 0.5F;
        second.p1.y = (second.p2.y + cy) * 0.5F;
        first.p3.y = second.p0.y =
            (first.p2.y + second.p1.y) * 0.5F;
    }

    // Equivalent to rlottie's in-place parameterSplitLeft().
    void splitLeft(float t, SpatialBezierRuntime& left) noexcept {
        left.p0 = p0;
        left.p1 = {
            p0.x + t * (p1.x - p0.x),
            p0.y + t * (p1.y - p0.y),
        };
        left.p2 = {
            p1.x + t * (p2.x - p1.x),
            p1.y + t * (p2.y - p1.y),
        };

        p2 = {
            p2.x + t * (p3.x - p2.x),
            p2.y + t * (p3.y - p2.y),
        };
        p1 = {
            left.p2.x + t * (p2.x - left.p2.x),
            left.p2.y + t * (p2.y - left.p2.y),
        };
        left.p2 = {
            left.p1.x + t * (left.p2.x - left.p1.x),
            left.p1.y + t * (left.p2.y - left.p1.y),
        };
        left.p3 = p0 = {
            left.p2.x + t * (p1.x - left.p2.x),
            left.p2.y + t * (p1.y - left.p2.y),
        };
    }

    [[nodiscard]] float length() const noexcept {
        const float polygon = lineLength(p0, p1)
            + lineLength(p1, p2)
            + lineLength(p2, p3);
        const float chord = lineLength(p0, p3);
        if ((polygon - chord) > kSpatialLengthTolerance) {
            SpatialBezierRuntime left;
            SpatialBezierRuntime right;
            split(left, right);
            return left.length() + right.length();
        }
        return polygon;
    }

    struct LengthParameter final {
        float value = 0.0F;
        std::uint32_t iterations = 0U;
        bool converged = false;
    };

    [[nodiscard]] LengthParameter parameterAtLength(
        float requestedLength) const noexcept {
        // Preserve rlottie's endpoint and zero-length behavior. The original
        // implementation has an unbounded loop; AveMotion keeps its exact
        // update rule but adds a high hard limit and fails explicitly instead
        // of publishing an approximate value. A committed Telegram asset needs
        // 89 iterations near an endpoint, so small defensive limits are invalid.
        float t = 1.0F;
        if (requestedLength > totalLength
            || std::fabs(requestedLength - totalLength) < 0.000001F) {
            return {t, 0U, true};
        }

        t *= 0.5F;
        float lastBigger = 1.0F;
        for (std::uint32_t iteration = 0U;
             iteration < static_cast<std::uint32_t>(
                 kSpatialMaximumLengthSearchIterations);
             ++iteration) {
            auto right = *this;
            SpatialBezierRuntime left;
            right.splitLeft(t, left);
            const float leftLength = left.length();
            if (std::fabs(leftLength - requestedLength)
                < kSpatialLengthTolerance) {
                return {t, iteration + 1U, true};
            }
            if (leftLength < requestedLength) {
                t += (lastBigger - t) * 0.5F;
            } else {
                lastBigger = t;
                t -= t * 0.5F;
            }
        }
        return {std::clamp(t, 0.0F, 1.0F),
                static_cast<std::uint32_t>(
                    kSpatialMaximumLengthSearchIterations),
                false};
    }

    struct Sample final {
        model::MotionVec2Value value;
        float angleDegrees = 0.0F;
        std::uint32_t searchIterations = 0U;
        bool converged = false;
    };

    [[nodiscard]] Sample sample(float progress) const noexcept {
        if (!valid) return {};
        const auto parameter = parameterAtLength(
            std::clamp(progress, 0.0F, 1.0F) * totalLength);
        if (!parameter.converged) {
            return {{}, 0.0F, parameter.iterations, false};
        }
        const auto direction = derivative(parameter.value);
        return {
            pointAt(parameter.value),
            canonicalFloat(
                std::atan2(direction.y, direction.x)
                * 180.0F / kTelegramPi),
            parameter.iterations,
            true,
        };
    }

    void initialize(
        model::MotionVec2Value start,
        model::MotionVec2Value end,
        model::MotionVec2Value inTangent,
        model::MotionVec2Value outTangent) noexcept {
        p0 = start;
        p1 = {start.x + outTangent.x, start.y + outTangent.y};
        p2 = {end.x + inTangent.x, end.y + inTangent.y};
        p3 = end;
        totalLength = length();
        valid = std::isfinite(totalLength);
    }
};

[[nodiscard]] model::MotionMatrix3x2Value identityMatrix() noexcept {
    return {};
}

// rlottie uses row-vector affine composition: local * parent. Keep the exact
// multiply order so parented layers and shape-group transforms match the
// pinned Telegram evaluator.
[[nodiscard]] model::MotionMatrix3x2Value multiply(
    const model::MotionMatrix3x2Value& left,
    const model::MotionMatrix3x2Value& right) noexcept {
    return canonicalMatrix({
        left.m11 * right.m11 + left.m12 * right.m21,
        left.m11 * right.m12 + left.m12 * right.m22,
        left.m21 * right.m11 + left.m22 * right.m21,
        left.m21 * right.m12 + left.m22 * right.m22,
        left.dx * right.m11 + left.dy * right.m21 + right.dx,
        left.dx * right.m12 + left.dy * right.m22 + right.dy,
    });
}

struct MatrixBuilder final {
    model::MotionMatrix3x2Value value{};

    MatrixBuilder() noexcept {
        value.m11 = 1.0F;
        value.m22 = 1.0F;
    }

    void translate(float x, float y) noexcept {
        value.dx += x * value.m11 + y * value.m21;
        value.dy += y * value.m22 + x * value.m12;
    }

    void scale(float x, float y) noexcept {
        value.m12 *= x;
        value.m21 *= y;
        value.m11 *= x;
        value.m22 *= y;
    }

    void rotate(float degrees) noexcept {
        if (degrees == 0.0F) return;
        float sine = 0.0F;
        float cosine = 0.0F;
        if (degrees == 90.0F || degrees == -270.0F) {
            sine = 1.0F;
        } else if (degrees == 270.0F || degrees == -90.0F) {
            sine = -1.0F;
        } else if (degrees == 180.0F || degrees == -180.0F) {
            cosine = -1.0F;
        } else {
            const float radians = kDegreesToRadians * degrees;
            sine = std::sin(radians);
            cosine = std::cos(radians);
        }
        const float m11 = cosine * value.m11 + sine * value.m21;
        const float m12 = cosine * value.m12 + sine * value.m22;
        const float m21 = -sine * value.m11 + cosine * value.m21;
        const float m22 = -sine * value.m12 + cosine * value.m22;
        value.m11 = m11;
        value.m12 = m12;
        value.m21 = m21;
        value.m22 = m22;
    }
};

[[nodiscard]] MotionPropertyValue invalidValue() noexcept {
    return {};
}

[[nodiscard]] MotionPropertyValue unsupportedValue(
    model::PropertyValueType type) noexcept {
    MotionPropertyValue result;
    result.type = type;
    result.storage = PropertyStorageKind::Unsupported;
    return result;
}

[[nodiscard]] MotionPropertyValue materializeValue(
    const model::MotionAssetModel& modelValue,
    model::MotionValueRef reference) noexcept {
    MotionPropertyValue result;
    result.type = reference.type;
    if (!reference.valid()) return result;
    switch (reference.type) {
    case model::PropertyValueType::Scalar:
        if (reference.index >= modelValue.scalarValues.size()) return result;
        result.storage = PropertyStorageKind::Materialized;
        result.scalar = canonicalFloat(modelValue.scalarValues[reference.index]);
        return result;
    case model::PropertyValueType::Vec2:
        if (reference.index >= modelValue.vec2Values.size()) return result;
        result.storage = PropertyStorageKind::Materialized;
        result.vec2 = canonicalVec2(modelValue.vec2Values[reference.index]);
        return result;
    case model::PropertyValueType::Color:
        if (reference.index >= modelValue.colorValues.size()) return result;
        result.storage = PropertyStorageKind::Materialized;
        result.color = canonicalColor(modelValue.colorValues[reference.index]);
        return result;
    case model::PropertyValueType::Matrix3x2:
        if (reference.index >= modelValue.matrixValues.size()) return result;
        result.storage = PropertyStorageKind::Materialized;
        result.matrix = canonicalMatrix(modelValue.matrixValues[reference.index]);
        return result;
    case model::PropertyValueType::Shape:
        if (reference.index >= modelValue.shapeValues.size()) return result;
        result.storage = PropertyStorageKind::AssetReference;
        result.assetReference = reference;
        return result;
    case model::PropertyValueType::Gradient:
        if (reference.index >= modelValue.gradientValues.size()) return result;
        result.storage = PropertyStorageKind::AssetReference;
        result.assetReference = reference;
        return result;
    case model::PropertyValueType::None:
        return result;
    }
    return result;
}

[[nodiscard]] MotionPropertyValue zeroValue(
    model::PropertyValueType type) noexcept {
    MotionPropertyValue result;
    result.type = type;
    switch (type) {
    case model::PropertyValueType::Scalar:
    case model::PropertyValueType::Vec2:
    case model::PropertyValueType::Color:
    case model::PropertyValueType::Matrix3x2:
        result.storage = PropertyStorageKind::Materialized;
        if (type == model::PropertyValueType::Color) result.color.a = 0.0F;
        if (type == model::PropertyValueType::Matrix3x2) {
            result.matrix.m11 = 1.0F;
            result.matrix.m22 = 1.0F;
        }
        return result;
    case model::PropertyValueType::Shape:
    case model::PropertyValueType::Gradient:
        return unsupportedValue(type);
    case model::PropertyValueType::None:
        return result;
    }
    return result;
}

[[nodiscard]] MotionPropertyValue interpolate(
    const MotionPropertyValue& first,
    const MotionPropertyValue& second,
    float progress) noexcept {
    if (!first.materialized() || !second.materialized()
        || first.type != second.type) {
        return unsupportedValue(first.type);
    }
    MotionPropertyValue result;
    result.type = first.type;
    result.storage = PropertyStorageKind::Materialized;
    result.assetReference = {};
    switch (first.type) {
    case model::PropertyValueType::Scalar:
        result.scalar = canonicalFloat(
            first.scalar + progress * (second.scalar - first.scalar));
        break;
    case model::PropertyValueType::Vec2:
        result.vec2 = canonicalVec2({
            first.vec2.x + progress * (second.vec2.x - first.vec2.x),
            first.vec2.y + progress * (second.vec2.y - first.vec2.y),
        });
        break;
    case model::PropertyValueType::Color:
        result.color = canonicalColor({
            first.color.r + progress * (second.color.r - first.color.r),
            first.color.g + progress * (second.color.g - first.color.g),
            first.color.b + progress * (second.color.b - first.color.b),
            first.color.a + progress * (second.color.a - first.color.a),
        });
        break;
    case model::PropertyValueType::Matrix3x2:
        result.matrix = canonicalMatrix({
            first.matrix.m11 + progress * (second.matrix.m11 - first.matrix.m11),
            first.matrix.m12 + progress * (second.matrix.m12 - first.matrix.m12),
            first.matrix.m21 + progress * (second.matrix.m21 - first.matrix.m21),
            first.matrix.m22 + progress * (second.matrix.m22 - first.matrix.m22),
            first.matrix.dx + progress * (second.matrix.dx - first.matrix.dx),
            first.matrix.dy + progress * (second.matrix.dy - first.matrix.dy),
        });
        break;
    case model::PropertyValueType::Shape:
    case model::PropertyValueType::Gradient:
    case model::PropertyValueType::None:
        return unsupportedValue(first.type);
    }
    return result;
}

[[nodiscard]] bool containsFrame(
    const model::MotionSegmentRecord& segment,
    double frame) noexcept {
    return frame >= segment.firstFrame && frame < segment.endFrame;
}

struct SegmentSelection final {
    enum class Position : std::uint8_t {
        BeforeFirst,
        Active,
        Gap,
        AfterLast,
    } position = Position::Gap;
    std::uint32_t index = kInvalidIndex;
};

[[nodiscard]] bool validRange(
    model::IndexRange range,
    std::size_t size) noexcept {
    return static_cast<std::size_t>(range.first) <= size
        && static_cast<std::size_t>(range.count)
            <= size - static_cast<std::size_t>(range.first);
}

[[nodiscard]] bool finiteVec2(model::MotionVec2Value value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y);
}

[[nodiscard]] bool finiteColor(model::MotionColorValue value) noexcept {
    return std::isfinite(value.r) && std::isfinite(value.g)
        && std::isfinite(value.b) && std::isfinite(value.a);
}

[[nodiscard]] bool finiteMatrix(model::MotionMatrix3x2Value value) noexcept {
    return std::isfinite(value.m11) && std::isfinite(value.m12)
        && std::isfinite(value.m21) && std::isfinite(value.m22)
        && std::isfinite(value.dx) && std::isfinite(value.dy);
}

[[nodiscard]] bool validValueReference(
    const model::MotionAssetModel& modelValue,
    model::MotionValueRef reference,
    model::PropertyValueType expectedType) noexcept {
    if (!reference.valid() || reference.type != expectedType) return false;
    switch (reference.type) {
    case model::PropertyValueType::Scalar:
        return reference.index < modelValue.scalarValues.size()
            && std::isfinite(modelValue.scalarValues[reference.index]);
    case model::PropertyValueType::Vec2:
        return reference.index < modelValue.vec2Values.size()
            && finiteVec2(modelValue.vec2Values[reference.index]);
    case model::PropertyValueType::Color:
        return reference.index < modelValue.colorValues.size()
            && finiteColor(modelValue.colorValues[reference.index]);
    case model::PropertyValueType::Matrix3x2:
        return reference.index < modelValue.matrixValues.size()
            && finiteMatrix(modelValue.matrixValues[reference.index]);
    case model::PropertyValueType::Shape: {
        if (reference.index >= modelValue.shapeValues.size()) return false;
        const auto& shape = modelValue.shapeValues[reference.index];
        if (!validRange(shape.points, modelValue.shapePoints.size())
            || (shape.points.count != 0U
                && (shape.points.count < 1U
                    || ((shape.points.count - 1U) % 3U) != 0U))) {
            return false;
        }
        for (std::uint32_t offset = 0; offset < shape.points.count; ++offset) {
            const auto pointIndex = shape.points.first + offset;
            if (!finiteVec2(modelValue.shapePoints[pointIndex])) return false;
        }
        return true;
    }
    case model::PropertyValueType::Gradient:
        if (reference.index >= modelValue.gradientValues.size()) return false;
        if (!validRange(
                modelValue.gradientValues[reference.index].values,
                modelValue.gradientFloats.size())) return false;
        for (std::uint32_t offset = 0;
             offset < modelValue.gradientValues[reference.index].values.count;
             ++offset) {
            const auto valueIndex =
                modelValue.gradientValues[reference.index].values.first + offset;
            if (!std::isfinite(modelValue.gradientFloats[valueIndex])) return false;
        }
        return true;
    case model::PropertyValueType::None:
        return false;
    }
    return false;
}

[[nodiscard]] SegmentSelection locateSegment(
    const model::MotionAssetModel& modelValue,
    const model::MotionTrackRecord& track,
    double frame,
    MotionTrackCursor& cursor,
    PropertyEvaluationStatistics& statistics) noexcept {
    const auto firstIndex = track.segments.first;
    const auto count = track.segments.count;
    const auto lastIndex = firstIndex + count - 1U;
    const auto& first = modelValue.segments[firstIndex];
    const auto& last = modelValue.segments[lastIndex];
    if (frame <= first.firstFrame) {
        ++statistics.beforeFirstSamples;
        cursor = {first.id, frame, true};
        return {SegmentSelection::Position::BeforeFirst, firstIndex};
    }
    if (frame >= last.endFrame) {
        ++statistics.afterLastSamples;
        cursor = {last.id, frame, true};
        return {SegmentSelection::Position::AfterLast, lastIndex};
    }

    if (cursor.valid
        && cursor.segment.valid()
        && cursor.segment.index() >= firstIndex
        && cursor.segment.index() <= lastIndex) {
        const auto currentIndex = cursor.segment.index();
        const auto& current = modelValue.segments[currentIndex];
        if (containsFrame(current, frame)) {
            ++statistics.cursorHits;
            cursor.lastFrame = frame;
            return {SegmentSelection::Position::Active, currentIndex};
        }
        if (frame >= current.endFrame) {
            for (std::uint32_t index = currentIndex + 1U;
                 index <= lastIndex;
                 ++index) {
                const auto& candidate = modelValue.segments[index];
                if (containsFrame(candidate, frame)) {
                    ++statistics.adjacentCursorMoves;
                    cursor = {candidate.id, frame, true};
                    return {SegmentSelection::Position::Active, index};
                }
                if (frame < candidate.firstFrame) break;
            }
        } else {
            for (std::uint32_t index = currentIndex; index > firstIndex; --index) {
                const auto candidateIndex = index - 1U;
                const auto& candidate = modelValue.segments[candidateIndex];
                if (containsFrame(candidate, frame)) {
                    ++statistics.adjacentCursorMoves;
                    cursor = {candidate.id, frame, true};
                    return {SegmentSelection::Position::Active, candidateIndex};
                }
                if (frame >= candidate.endFrame) break;
            }
        }
    }

    ++statistics.binarySearches;
    std::uint32_t lower = firstIndex;
    std::uint32_t upper = firstIndex + count;
    while (lower < upper) {
        const auto middle = lower + (upper - lower) / 2U;
        if (modelValue.segments[middle].firstFrame <= frame) {
            lower = middle + 1U;
        } else {
            upper = middle;
        }
    }
    if (lower > firstIndex) {
        const auto candidateIndex = lower - 1U;
        const auto& candidate = modelValue.segments[candidateIndex];
        if (containsFrame(candidate, frame)) {
            cursor = {candidate.id, frame, true};
            return {SegmentSelection::Position::Active, candidateIndex};
        }
    }
    ++statistics.gapSamples;
    cursor = {{}, frame, false};
    return {SegmentSelection::Position::Gap, kInvalidIndex};
}

[[nodiscard]] bool hasProperty(
    const model::MotionAssetModel& modelValue,
    const model::MotionSourceNodeRecord& node,
    model::PropertySemantic semantic) noexcept {
    for (std::uint32_t offset = 0; offset < node.properties.count; ++offset) {
        const auto propertyId = modelValue.sourcePropertyIds[
            node.properties.first + offset];
        const auto* property = modelValue.property(propertyId);
        if (property != nullptr && property->semantic == semantic) return true;
    }
    return false;
}

[[nodiscard]] const EvaluatedProperty* findEvaluatedProperty(
    const model::MotionAssetModel& modelValue,
    const model::MotionSourceNodeRecord& node,
    model::PropertySemantic semantic,
    std::span<const EvaluatedProperty> values) noexcept {
    for (std::uint32_t offset = 0; offset < node.properties.count; ++offset) {
        const auto propertyId = modelValue.sourcePropertyIds[
            node.properties.first + offset];
        if (!propertyId.valid() || propertyId.index() >= values.size()) continue;
        const auto& value = values[propertyId.index()];
        if (value.semantic == semantic) return &value;
    }
    return nullptr;
}

[[nodiscard]] bool valueScalar(
    const EvaluatedProperty* property,
    float& output) noexcept {
    if (property == nullptr || !property->value.materialized()
        || property->value.type != model::PropertyValueType::Scalar) {
        return false;
    }
    output = property->value.scalar;
    return true;
}

[[nodiscard]] bool valueVec2(
    const EvaluatedProperty* property,
    model::MotionVec2Value& output) noexcept {
    if (property == nullptr || !property->value.materialized()
        || property->value.type != model::PropertyValueType::Vec2) {
        return false;
    }
    output = property->value.vec2;
    return true;
}

[[nodiscard]] EvaluatedNodeTransform evaluateNodeTransform(
    const model::MotionAssetModel& modelValue,
    const model::MotionSourceNodeRecord& node,
    std::span<const EvaluatedProperty> properties) noexcept {
    EvaluatedNodeTransform result;
    result.node = node.id;
    const auto* matrixProperty = findEvaluatedProperty(
        modelValue, node, model::PropertySemantic::TransformMatrix, properties);
    const auto* opacityProperty = findEvaluatedProperty(
        modelValue, node, model::PropertySemantic::TransformOpacity, properties);
    if (matrixProperty != nullptr) {
        if (!matrixProperty->value.materialized()
            || matrixProperty->value.type != model::PropertyValueType::Matrix3x2) {
            result.state = NodeTransformState::Unsupported;
            return result;
        }
        result.localMatrix = matrixProperty->value.matrix;
        float opacityPercent = 100.0F;
        if (opacityProperty != nullptr && !valueScalar(opacityProperty, opacityPercent)) {
            result.state = NodeTransformState::Unsupported;
            return result;
        }
        result.localOpacity = canonicalFloat(opacityPercent / 100.0F);
        result.state = NodeTransformState::StaticMatrix;
        return result;
    }

    const bool hasTransform = hasProperty(
        modelValue, node, model::PropertySemantic::TransformPosition)
        || hasProperty(modelValue, node, model::PropertySemantic::TransformPositionX)
        || hasProperty(modelValue, node, model::PropertySemantic::TransformPositionY)
        || hasProperty(modelValue, node, model::PropertySemantic::TransformScale)
        || hasProperty(modelValue, node, model::PropertySemantic::TransformRotation)
        || hasProperty(modelValue, node, model::PropertySemantic::TransformAnchor)
        || opacityProperty != nullptr;
    if (!hasTransform) return result;
    if (hasProperty(modelValue, node, model::PropertySemantic::TransformRotationX)
        || hasProperty(modelValue, node, model::PropertySemantic::TransformRotationY)
        || hasProperty(modelValue, node, model::PropertySemantic::TransformRotationZ)) {
        result.state = NodeTransformState::Unsupported;
        return result;
    }

    model::MotionVec2Value position{};
    const auto* positionProperty = findEvaluatedProperty(
        modelValue, node, model::PropertySemantic::TransformPosition, properties);
    float autoOrientAngle = 0.0F;
    if (positionProperty != nullptr) {
        if (!valueVec2(positionProperty, position)) {
            result.state = NodeTransformState::Unsupported;
            return result;
        }
        if (node.autoOrient && positionProperty->spatialAngleValid) {
            autoOrientAngle = positionProperty->spatialAngleDegrees;
        }
    } else {
        float x = 0.0F;
        float y = 0.0F;
        const auto* xProperty = findEvaluatedProperty(
            modelValue, node, model::PropertySemantic::TransformPositionX, properties);
        const auto* yProperty = findEvaluatedProperty(
            modelValue, node, model::PropertySemantic::TransformPositionY, properties);
        if ((xProperty != nullptr && !valueScalar(xProperty, x))
            || (yProperty != nullptr && !valueScalar(yProperty, y))) {
            result.state = NodeTransformState::Unsupported;
            return result;
        }
        position = {x, y};
    }

    model::MotionVec2Value scale{100.0F, 100.0F};
    model::MotionVec2Value anchor{};
    float rotation = 0.0F;
    float opacity = 100.0F;
    const auto* scaleProperty = findEvaluatedProperty(
        modelValue, node, model::PropertySemantic::TransformScale, properties);
    const auto* anchorProperty = findEvaluatedProperty(
        modelValue, node, model::PropertySemantic::TransformAnchor, properties);
    const auto* rotationProperty = findEvaluatedProperty(
        modelValue, node, model::PropertySemantic::TransformRotation, properties);
    if ((scaleProperty != nullptr && !valueVec2(scaleProperty, scale))
        || (anchorProperty != nullptr && !valueVec2(anchorProperty, anchor))
        || (rotationProperty != nullptr && !valueScalar(rotationProperty, rotation))
        || (opacityProperty != nullptr && !valueScalar(opacityProperty, opacity))) {
        result.state = NodeTransformState::Unsupported;
        return result;
    }

    MatrixBuilder matrix;
    matrix.translate(position.x, position.y);
    matrix.rotate(rotation + autoOrientAngle);
    matrix.scale(scale.x / 100.0F, scale.y / 100.0F);
    matrix.translate(-anchor.x, -anchor.y);
    result.localMatrix = canonicalMatrix(matrix.value);
    result.localOpacity = canonicalFloat(opacity / 100.0F);
    result.state = NodeTransformState::Evaluated2D;
    return result;
}

[[nodiscard]] model::SourceNodeId matrixParent(
    const model::MotionSourceNodeRecord& node) noexcept {
    return node.transformParent.valid() ? node.transformParent : node.parent;
}

[[nodiscard]] bool buildWorldOrder(
    const model::MotionAssetModel& modelValue,
    std::vector<model::SourceNodeId>& output,
    std::string& error) {
    const std::size_t count = modelValue.sourceNodes.size();
    output.clear();
    output.reserve(count);
    std::vector<std::vector<std::uint32_t>> dependents(count);
    std::vector<std::uint32_t> indegree(count, 0U);

    const auto addDependency = [&](std::size_t nodeIndex,
                                   model::SourceNodeId dependency) -> bool {
        if (!dependency.valid()) return true;
        if (dependency.index() >= count || dependency.index() == nodeIndex) {
            error = "property evaluator found an invalid transform hierarchy edge";
            return false;
        }
        const auto& node = modelValue.sourceNodes[nodeIndex];
        const auto& parent = modelValue.sourceNodes[dependency.index()];
        if (!parent.present || parent.composition != node.composition) {
            error = "property evaluator found a cross-composition hierarchy edge";
            return false;
        }
        auto& list = dependents[dependency.index()];
        const auto child = static_cast<std::uint32_t>(nodeIndex);
        if (std::find(list.begin(), list.end(), child) == list.end()) {
            list.push_back(child);
            ++indegree[nodeIndex];
        }
        return true;
    };

    for (std::size_t index = 0; index < count; ++index) {
        const auto& node = modelValue.sourceNodes[index];
        if (!node.present || node.id != model::makeId<model::SourceNodeId>(index)) {
            error = "property evaluator found a corrupt source-node hierarchy";
            return false;
        }
        if (!addDependency(index, node.parent)) return false;
        if (node.transformParent.valid()
            && node.transformParent != node.parent
            && !addDependency(index, node.transformParent)) {
            return false;
        }
    }

    struct GreaterIndex final {
        bool operator()(std::uint32_t left, std::uint32_t right) const noexcept {
            return left > right;
        }
    };
    std::priority_queue<
        std::uint32_t,
        std::vector<std::uint32_t>,
        GreaterIndex> ready;
    for (std::uint32_t index = 0; index < indegree.size(); ++index) {
        if (indegree[index] == 0U) ready.push(index);
    }
    while (!ready.empty()) {
        const auto current = ready.top();
        ready.pop();
        output.push_back(model::SourceNodeId{current});
        for (const auto child : dependents[current]) {
            if (--indegree[child] == 0U) ready.push(child);
        }
    }
    if (output.size() != count) {
        error = "property evaluator found a cycle in the transform hierarchy";
        output.clear();
        return false;
    }
    return true;
}

[[nodiscard]] bool staticWorldState(
    NodeTransformState localState,
    NodeTransformState matrixParentState,
    NodeTransformState opacityParentState) noexcept {
    const auto locallyStatic = localState == NodeTransformState::NotPresent
        || localState == NodeTransformState::StaticMatrix;
    const auto matrixStatic = matrixParentState == NodeTransformState::NotPresent
        || matrixParentState == NodeTransformState::StaticMatrix;
    const auto opacityStatic = opacityParentState == NodeTransformState::NotPresent
        || opacityParentState == NodeTransformState::StaticMatrix;
    return locallyStatic && matrixStatic && opacityStatic;
}

enum class ShapeSampleMode : std::uint8_t {
    Empty,
    Endpoint,
    Interpolate,
};

[[nodiscard]] bool validShapeEncoding(
    const model::MotionShapeValueRecord& shape) noexcept {
    return shape.points.count == 0U
        || (shape.points.count >= 1U
            && ((shape.points.count - 1U) % 3U) == 0U);
}

[[nodiscard]] const model::MotionShapeValueRecord* shapeRecord(
    const model::MotionAssetModel& modelValue,
    model::MotionValueRef reference) noexcept {
    if (!reference.valid()
        || reference.type != model::PropertyValueType::Shape
        || reference.index >= modelValue.shapeValues.size()) {
        return nullptr;
    }
    const auto& shape = modelValue.shapeValues[reference.index];
    if (!validShapeEncoding(shape)
        || !validRange(shape.points, modelValue.shapePoints.size())) {
        return nullptr;
    }
    return &shape;
}

[[nodiscard]] std::span<const model::MotionVec2Value> sourceShapePoints(
    const model::MotionAssetModel& modelValue,
    const model::MotionShapeValueRecord& shape) noexcept {
    if (shape.points.count == 0U) return {};
    return std::span<const model::MotionVec2Value>{
        modelValue.shapePoints.data() + shape.points.first,
        shape.points.count};
}

[[nodiscard]] std::uint64_t shapeHash(
    std::span<const model::MotionVec2Value> points,
    bool closed,
    bool topologyTruncated) noexcept {
    core::Fnv1a64 hash;
    hash.appendU8(closed ? 1U : 0U);
    hash.appendU8(topologyTruncated ? 1U : 0U);
    hash.appendU32(static_cast<std::uint32_t>(points.size()));
    for (const auto point : points) {
        hash.appendFloat(canonicalFloat(point.x));
        hash.appendFloat(canonicalFloat(point.y));
    }
    return hash.value();
}

[[nodiscard]] bool materializeShapeSample(
    const model::MotionAssetModel& modelValue,
    model::MotionValueRef firstReference,
    model::MotionValueRef secondReference,
    ShapeSampleMode mode,
    float progress,
    std::uint32_t shapeSlot,
    std::span<EvaluatedShape> shapes,
    std::span<model::MotionVec2Value> pointStorage,
    bool hasHistory,
    PropertyEvaluationStatistics& statistics,
    MotionPropertyValue& result) noexcept {
    if (shapeSlot >= shapes.size()) return false;
    auto& output = shapes[shapeSlot];
    if (static_cast<std::size_t>(output.firstPoint) + output.pointCapacity
        > pointStorage.size()) {
        return false;
    }

    const model::MotionShapeValueRecord* first = nullptr;
    const model::MotionShapeValueRecord* second = nullptr;
    if (mode != ShapeSampleMode::Empty) {
        first = shapeRecord(modelValue, firstReference);
        if (first == nullptr) return false;
    }
    if (mode == ShapeSampleMode::Interpolate) {
        second = shapeRecord(modelValue, secondReference);
        if (second == nullptr) return false;
    }

    std::span<const model::MotionVec2Value> firstPoints;
    std::span<const model::MotionVec2Value> secondPoints;
    std::uint32_t pointCount = 0U;
    bool closed = false;
    bool topologyTruncated = false;
    if (mode == ShapeSampleMode::Endpoint) {
        firstPoints = sourceShapePoints(modelValue, *first);
        pointCount = static_cast<std::uint32_t>(firstPoints.size());
        closed = first->closed;
    } else if (mode == ShapeSampleMode::Interpolate) {
        firstPoints = sourceShapePoints(modelValue, *first);
        secondPoints = sourceShapePoints(modelValue, *second);
        pointCount = static_cast<std::uint32_t>(
            std::min(firstPoints.size(), secondPoints.size()));
        // Match the pinned Telegram LottieShapeData::lerp semantics. The old
        // implementation truncates mismatched paths and does not copy the
        // closed flag into an interpolated value. The truncation is surfaced
        // diagnostically so a future validator can reject it explicitly.
        topologyTruncated = firstPoints.size() != secondPoints.size();
        closed = false;
    }
    if (pointCount > output.pointCapacity) return false;

    bool changed = !hasHistory
        || output.pointCount != pointCount
        || output.closed != closed
        || output.topologyTruncated != topologyTruncated;
    auto destination = pointStorage.subspan(output.firstPoint, pointCount);
    for (std::uint32_t index = 0U; index < pointCount; ++index) {
        model::MotionVec2Value next;
        if (mode == ShapeSampleMode::Interpolate) {
            next = canonicalVec2({
                firstPoints[index].x
                    + progress * (secondPoints[index].x - firstPoints[index].x),
                firstPoints[index].y
                    + progress * (secondPoints[index].y - firstPoints[index].y),
            });
            ++statistics.shapePointInterpolations;
        } else {
            next = canonicalVec2(firstPoints[index]);
        }
        if (!hasHistory || destination[index] != next) changed = true;
        destination[index] = next;
    }

    const auto contentHash = shapeHash(destination, closed, topologyTruncated);
    output.changed = changed;
    output.pointCount = pointCount;
    output.closed = closed;
    output.topologyTruncated = topologyTruncated;
    output.contentHash = contentHash;
    if (changed) {
        ++output.revision;
        ++statistics.changedShapes;
    }
    if (topologyTruncated) ++statistics.shapeTopologyTruncations;

    result = {};
    result.type = model::PropertyValueType::Shape;
    result.storage = PropertyStorageKind::Materialized;
    result.shapeSlot = shapeSlot;
    return true;
}

} // namespace

struct PropertyEvaluationWorkspace::Impl final {
    std::uint64_t modelFingerprint = 0;
    std::vector<MotionTrackCursor> cursors;
    std::vector<EvaluatedProperty> properties;
    std::vector<EvaluatedNodeTransform> nodeTransforms;
    std::vector<EvaluatedShape> shapes;
    std::vector<model::MotionVec2Value> shapePoints;
    std::uint64_t sequence = 0;
    std::uint64_t storageGeneration = 0;
    bool hasHistory = false;
};

struct PropertyEvaluator::Impl final {
    struct ShapeLayout final {
        model::PropertyId property;
        std::uint32_t firstPoint = 0;
        std::uint32_t pointCapacity = 0;
    };

    std::shared_ptr<const model::MotionAssetModel> model;
    std::vector<CubicBezierRuntime> easing;
    std::vector<SpatialBezierRuntime> spatial;
    std::vector<model::SourceNodeId> worldOrder;
    std::vector<ShapeLayout> shapeLayouts;
    std::vector<std::uint32_t> propertyShapeSlots;
    std::size_t shapePointCapacity = 0;
    bool valid = false;
    std::string error;
};

PropertyEvaluationWorkspace::PropertyEvaluationWorkspace()
    : impl_(std::make_unique<Impl>()) {
}

PropertyEvaluationWorkspace::PropertyEvaluationWorkspace(
    PropertyEvaluationWorkspace&&) noexcept = default;
PropertyEvaluationWorkspace& PropertyEvaluationWorkspace::operator=(
    PropertyEvaluationWorkspace&&) noexcept = default;
PropertyEvaluationWorkspace::~PropertyEvaluationWorkspace() = default;

void PropertyEvaluationWorkspace::resetHistory() noexcept {
    if (!impl_) return;
    impl_->hasHistory = false;
    impl_->sequence = 0;
    for (auto& cursor : impl_->cursors) cursor = {};
    for (auto& value : impl_->properties) {
        value.value = {};
        value.activeSegment = {};
        value.spatialAngleDegrees = 0.0F;
        value.spatialAngleValid = false;
        value.revision = 0;
        value.changed = false;
    }
    for (auto& value : impl_->shapes) {
        value.pointCount = 0U;
        value.closed = false;
        value.topologyTruncated = false;
        value.contentHash = 0U;
        value.revision = 0U;
        value.changed = false;
    }
    std::fill(
        impl_->shapePoints.begin(),
        impl_->shapePoints.end(),
        model::MotionVec2Value{});
    for (auto& value : impl_->nodeTransforms) {
        value.localMatrix = {};
        value.localMatrix.m11 = 1.0F;
        value.localMatrix.m22 = 1.0F;
        value.localOpacity = 1.0F;
        value.state = NodeTransformState::NotPresent;
        value.revision = 0;
        value.changed = false;
        value.worldMatrix = identityMatrix();
        value.worldOpacity = 1.0F;
        value.worldState = NodeTransformState::NotPresent;
        value.worldRevision = 0;
        value.worldChanged = false;
    }
}

std::size_t PropertyEvaluationWorkspace::propertyCount() const noexcept {
    return impl_ ? impl_->properties.size() : 0U;
}

std::size_t PropertyEvaluationWorkspace::trackCursorCount() const noexcept {
    return impl_ ? impl_->cursors.size() : 0U;
}

std::size_t PropertyEvaluationWorkspace::nodeTransformCount() const noexcept {
    return impl_ ? impl_->nodeTransforms.size() : 0U;
}

std::size_t PropertyEvaluationWorkspace::shapeCount() const noexcept {
    return impl_ ? impl_->shapes.size() : 0U;
}

std::size_t PropertyEvaluationWorkspace::shapePointCapacity() const noexcept {
    return impl_ ? impl_->shapePoints.size() : 0U;
}

std::size_t PropertyEvaluationWorkspace::retainedBytes() const noexcept {
    if (!impl_) return 0U;
    return impl_->cursors.capacity() * sizeof(MotionTrackCursor)
        + impl_->properties.capacity() * sizeof(EvaluatedProperty)
        + impl_->nodeTransforms.capacity() * sizeof(EvaluatedNodeTransform)
        + impl_->shapes.capacity() * sizeof(EvaluatedShape)
        + impl_->shapePoints.capacity() * sizeof(model::MotionVec2Value);
}

std::uint64_t PropertyEvaluationWorkspace::storageGeneration() const noexcept {
    return impl_ ? impl_->storageGeneration : 0U;
}

PropertyEvaluator::PropertyEvaluator(
    std::shared_ptr<const model::MotionAssetModel> modelValue)
    : impl_(std::make_unique<Impl>()) {
    impl_->model = std::move(modelValue);
    if (!impl_->model) {
        impl_->error = "property evaluator received a null asset model";
        return;
    }
    const auto& modelRef = *impl_->model;
    if (!modelRef.statistics.directParsedModel
        || modelRef.schemaVersion != model::MotionAssetModel::kSchemaVersion) {
        impl_->error = "property evaluator requires a direct parsed canonical model";
        return;
    }
    for (std::size_t index = 0; index < modelRef.sourceNodes.size(); ++index) {
        const auto& node = modelRef.sourceNodes[index];
        if (!node.present
            || node.id != model::makeId<model::SourceNodeId>(index)
            || !validRange(node.properties, modelRef.sourcePropertyIds.size())) {
            impl_->error = "property evaluator found a corrupt source-node table";
            return;
        }
        for (std::uint32_t offset = 0; offset < node.properties.count; ++offset) {
            const auto propertyId = modelRef.sourcePropertyIds[
                node.properties.first + offset];
            if (!propertyId.valid() || propertyId.index() >= modelRef.properties.size()) {
                impl_->error = "property evaluator found an invalid source-property reference";
                return;
            }
        }
    }

    for (std::size_t index = 0; index < modelRef.properties.size(); ++index) {
        const auto& property = modelRef.properties[index];
        const bool isStatic = (property.flags & model::PropertyFlagStatic) != 0U;
        const bool isAnimated = (property.flags & model::PropertyFlagAnimated) != 0U;
        if (!property.present
            || property.id != model::makeId<model::PropertyId>(index)
            || !property.owner.valid()
            || property.owner.index() >= modelRef.sourceNodes.size()
            || property.valueType == model::PropertyValueType::None
            || isStatic == isAnimated) {
            impl_->error = "property evaluator found a corrupt property table";
            return;
        }
        if (isStatic) {
            if (!validValueReference(
                    modelRef, property.staticValue, property.valueType)) {
                impl_->error = "property evaluator found an invalid static value";
                return;
            }
        } else {
            if (!property.track.valid()
                || property.track.index() >= modelRef.tracks.size()) {
                impl_->error = "property evaluator found an invalid property track";
                return;
            }
        }
    }

    impl_->easing.resize(modelRef.segments.size());
    impl_->spatial.resize(modelRef.segments.size());
    for (std::size_t index = 0; index < modelRef.segments.size(); ++index) {
        const auto& segment = modelRef.segments[index];
        if (!segment.present
            || segment.id != model::makeId<model::SegmentId>(index)
            || !segment.track.valid()
            || segment.track.index() >= modelRef.tracks.size()
            || !std::isfinite(segment.firstFrame)
            || !std::isfinite(segment.endFrame)
            || segment.endFrame < segment.firstFrame
            || !finiteVec2(segment.temporalControl1)
            || !finiteVec2(segment.temporalControl2)
            || !finiteVec2(segment.spatialInTangent)
            || !finiteVec2(segment.spatialOutTangent)) {
            impl_->error = "property evaluator found a corrupt segment table";
            return;
        }
        const auto& track = modelRef.tracks[segment.track.index()];
        if (!track.property.valid()
            || track.property.index() >= modelRef.properties.size()) {
            impl_->error = "property evaluator found a segment with an invalid track";
            return;
        }
        const auto expectedType = modelRef.properties[track.property.index()].valueType;
        if (!validValueReference(modelRef, segment.startValue, expectedType)
            || !validValueReference(modelRef, segment.endValue, expectedType)) {
            impl_->error = "property evaluator found an invalid segment value";
            return;
        }
        impl_->easing[index].initialize(
            segment.temporalControl1,
            segment.temporalControl2);
        if (segment.spatialInterpolation
            == model::SpatialInterpolation::CubicBezier) {
            if (expectedType != model::PropertyValueType::Vec2
                || segment.startValue.type != model::PropertyValueType::Vec2
                || segment.endValue.type != model::PropertyValueType::Vec2) {
                impl_->error = "spatial interpolation requires Vec2 keyframes";
                return;
            }
            impl_->spatial[index].initialize(
                modelRef.vec2Values[segment.startValue.index],
                modelRef.vec2Values[segment.endValue.index],
                segment.spatialInTangent,
                segment.spatialOutTangent);
            if (!impl_->spatial[index].valid) {
                impl_->error = "property evaluator found an invalid spatial curve";
                return;
            }
        }
    }

    for (std::size_t index = 0; index < modelRef.tracks.size(); ++index) {
        const auto& track = modelRef.tracks[index];
        if (!track.present
            || track.id != model::makeId<model::TrackId>(index)
            || !track.property.valid()
            || track.property.index() >= modelRef.properties.size()
            || modelRef.properties[track.property.index()].track != track.id
            || !validRange(track.segments, modelRef.segments.size())
            || track.segments.empty()
            || !std::isfinite(track.firstFrame)
            || !std::isfinite(track.endFrame)
            || track.endFrame < track.firstFrame) {
            impl_->error = "property evaluator found a corrupt track table";
            return;
        }
        double previousEnd = 0.0;
        bool havePrevious = false;
        for (std::uint32_t offset = 0; offset < track.segments.count; ++offset) {
            const auto segmentIndex = track.segments.first + offset;
            const auto& segment = modelRef.segments[segmentIndex];
            if (segment.track != track.id
                || (havePrevious && segment.firstFrame != previousEnd)) {
                impl_->error = "property evaluator requires contiguous ordered segments";
                return;
            }
            previousEnd = segment.endFrame;
            havePrevious = true;
        }
    }

    impl_->propertyShapeSlots.assign(
        modelRef.properties.size(), model::kInvalidModelId);
    for (const auto& property : modelRef.properties) {
        const bool isAnimated =
            (property.flags & model::PropertyFlagAnimated) != 0U;
        if (!isAnimated || property.valueType != model::PropertyValueType::Shape) {
            continue;
        }
        const auto& track = modelRef.tracks[property.track.index()];
        std::uint32_t pointCapacity = 0U;
        for (std::uint32_t offset = 0U; offset < track.segments.count; ++offset) {
            const auto& segment = modelRef.segments[track.segments.first + offset];
            const auto* first = shapeRecord(modelRef, segment.startValue);
            const auto* second = shapeRecord(modelRef, segment.endValue);
            if (first == nullptr || second == nullptr) {
                impl_->error = "property evaluator found corrupt shape storage";
                return;
            }
            pointCapacity = std::max(
                pointCapacity,
                std::max(first->points.count, second->points.count));
        }
        if (impl_->shapePointCapacity
            > std::numeric_limits<std::size_t>::max() - pointCapacity) {
            impl_->error = "property evaluator shape storage overflows size_t";
            return;
        }
        const auto nextShapePointCapacity =
            impl_->shapePointCapacity + pointCapacity;
        if (nextShapePointCapacity > model::kInvalidModelId
            || impl_->shapeLayouts.size() >= model::kInvalidModelId) {
            impl_->error = "property evaluator shape storage exceeds model limits";
            return;
        }
        const auto shapeSlot = static_cast<std::uint32_t>(
            impl_->shapeLayouts.size());
        impl_->propertyShapeSlots[property.id.index()] = shapeSlot;
        impl_->shapeLayouts.push_back({
            .property = property.id,
            .firstPoint = static_cast<std::uint32_t>(impl_->shapePointCapacity),
            .pointCapacity = pointCapacity,
        });
        impl_->shapePointCapacity = nextShapePointCapacity;
    }

    if (!buildWorldOrder(modelRef, impl_->worldOrder, impl_->error)) return;
    impl_->valid = true;
}

PropertyEvaluator::PropertyEvaluator(PropertyEvaluator&&) noexcept = default;
PropertyEvaluator& PropertyEvaluator::operator=(PropertyEvaluator&&) noexcept = default;
PropertyEvaluator::~PropertyEvaluator() = default;

bool PropertyEvaluator::valid() const noexcept {
    return impl_ && impl_->valid;
}

std::string_view PropertyEvaluator::errorMessage() const noexcept {
    return impl_ ? std::string_view{impl_->error} : std::string_view{};
}

const std::shared_ptr<const model::MotionAssetModel>& PropertyEvaluator::model() const noexcept {
    static const std::shared_ptr<const model::MotionAssetModel> empty;
    return impl_ ? impl_->model : empty;
}

void PropertyEvaluator::prepare(PropertyEvaluationWorkspace& workspace) const {
    if (!workspace.impl_) workspace.impl_ = std::make_unique<PropertyEvaluationWorkspace::Impl>();
    auto& target = *workspace.impl_;
    if (!impl_ || !impl_->valid || !impl_->model) {
        const bool hadStorage = !target.properties.empty()
            || !target.cursors.empty()
            || !target.nodeTransforms.empty()
            || !target.shapes.empty()
            || !target.shapePoints.empty();
        target.properties.clear();
        target.cursors.clear();
        target.nodeTransforms.clear();
        target.shapes.clear();
        target.shapePoints.clear();
        target.modelFingerprint = 0;
        target.sequence = 0;
        target.hasHistory = false;
        if (hadStorage) ++target.storageGeneration;
        return;
    }
    const auto& modelRef = *impl_->model;
    const bool resized = target.properties.size() != modelRef.properties.size()
        || target.cursors.size() != modelRef.tracks.size()
        || target.nodeTransforms.size() != modelRef.sourceNodes.size()
        || target.shapes.size() != impl_->shapeLayouts.size()
        || target.shapePoints.size() != impl_->shapePointCapacity;
    target.properties.resize(modelRef.properties.size());
    target.cursors.resize(modelRef.tracks.size());
    target.nodeTransforms.resize(modelRef.sourceNodes.size());
    target.shapes.resize(impl_->shapeLayouts.size());
    target.shapePoints.resize(impl_->shapePointCapacity);
    target.modelFingerprint = modelRef.parsedModelFingerprint;
    for (std::size_t index = 0; index < modelRef.properties.size(); ++index) {
        const auto& source = modelRef.properties[index];
        auto& destination = target.properties[index];
        destination.id = source.id;
        destination.owner = source.owner;
        destination.semantic = source.semantic;
        destination.semanticIndex = source.semanticIndex;
    }
    for (std::size_t index = 0; index < modelRef.sourceNodes.size(); ++index) {
        target.nodeTransforms[index].node = modelRef.sourceNodes[index].id;
    }
    for (std::size_t index = 0; index < impl_->shapeLayouts.size(); ++index) {
        const auto& layout = impl_->shapeLayouts[index];
        auto& shape = target.shapes[index];
        shape.property = layout.property;
        shape.firstPoint = layout.firstPoint;
        shape.pointCapacity = layout.pointCapacity;
    }
    if (resized) ++target.storageGeneration;
    workspace.resetHistory();
}

PropertyEvaluationView PropertyEvaluator::evaluate(
    double assetFrame,
    PropertyEvaluationWorkspace& workspace) const noexcept {
    PropertyEvaluationView view;
    view.requestedFrame = assetFrame;
    if (!impl_ || !impl_->valid || !impl_->model) {
        view.error = PropertyEvaluationErrorCode::InvalidModel;
        return view;
    }
    if (!std::isfinite(assetFrame)) {
        view.error = PropertyEvaluationErrorCode::NonFiniteFrame;
        return view;
    }
    if (!workspace.impl_
        || workspace.impl_->modelFingerprint != impl_->model->parsedModelFingerprint
        || workspace.impl_->properties.size() != impl_->model->properties.size()
        || workspace.impl_->cursors.size() != impl_->model->tracks.size()
        || workspace.impl_->nodeTransforms.size() != impl_->model->sourceNodes.size()
        || workspace.impl_->shapes.size() != impl_->shapeLayouts.size()
        || workspace.impl_->shapePoints.size() != impl_->shapePointCapacity) {
        view.error = PropertyEvaluationErrorCode::WorkspaceNotPrepared;
        return view;
    }

    auto& state = *workspace.impl_;
    const auto& modelRef = *impl_->model;
    PropertyEvaluationStatistics statistics;
    for (std::size_t index = 0; index < modelRef.properties.size(); ++index) {
        const auto& property = modelRef.properties[index];
        auto& output = state.properties[index];
        ++statistics.propertiesVisited;
        MotionPropertyValue nextValue = invalidValue();
        model::SegmentId nextSegment;
        float nextSpatialAngle = 0.0F;
        bool nextSpatialAngleValid = false;
        bool nextShapeChanged = false;
        const bool isStatic = (property.flags & model::PropertyFlagStatic) != 0U;
        const bool isAnimated = (property.flags & model::PropertyFlagAnimated) != 0U;
        if (isStatic) {
            ++statistics.staticProperties;
            nextValue = materializeValue(modelRef, property.staticValue);
            if (property.valueType == model::PropertyValueType::Shape
                && nextValue.storage == PropertyStorageKind::AssetReference) {
                ++statistics.staticShapeReferences;
            }
        } else if (isAnimated) {
            ++statistics.animatedProperties;
            if (!property.track.valid()
                || property.track.index() >= modelRef.tracks.size()) {
                view.error = PropertyEvaluationErrorCode::CorruptProperty;
                return view;
            }
            const auto& track = modelRef.tracks[property.track.index()];
            if (property.valueType == model::PropertyValueType::Gradient) {
                nextValue = unsupportedValue(property.valueType);
            } else {
                auto& cursor = state.cursors[track.id.index()];
                const auto selection = locateSegment(
                    modelRef, track, assetFrame, cursor, statistics);

                if (property.valueType == model::PropertyValueType::Shape) {
                    if (property.id.index() >= impl_->propertyShapeSlots.size()) {
                        view.error = PropertyEvaluationErrorCode::CorruptShapeStorage;
                        return view;
                    }
                    const auto shapeSlot =
                        impl_->propertyShapeSlots[property.id.index()];
                    if (shapeSlot == model::kInvalidModelId
                        || shapeSlot >= state.shapes.size()) {
                        view.error = PropertyEvaluationErrorCode::CorruptShapeStorage;
                        return view;
                    }

                    ShapeSampleMode mode = ShapeSampleMode::Empty;
                    model::MotionValueRef firstReference;
                    model::MotionValueRef secondReference;
                    float progress = 0.0F;
                    if (selection.index != kInvalidIndex) {
                        const auto& segment = modelRef.segments[selection.index];
                        nextSegment = segment.id;
                        if (selection.position
                            == SegmentSelection::Position::BeforeFirst) {
                            mode = ShapeSampleMode::Endpoint;
                            firstReference = segment.startValue;
                        } else if (selection.position
                                   == SegmentSelection::Position::AfterLast) {
                            mode = ShapeSampleMode::Endpoint;
                            firstReference = segment.endValue;
                        } else if (selection.position
                                   == SegmentSelection::Position::Active) {
                            if (segment.spatialInterpolation
                                != model::SpatialInterpolation::None) {
                                view.error = PropertyEvaluationErrorCode::CorruptSegment;
                                return view;
                            }
                            firstReference = segment.startValue;
                            secondReference = segment.endValue;
                            mode = ShapeSampleMode::Interpolate;
                            if (segment.interpolation
                                == model::SegmentInterpolation::Hold) {
                                ++statistics.holdSamples;
                                progress = 0.0F;
                            } else {
                                const float startFrame =
                                    static_cast<float>(segment.firstFrame);
                                const float endFrame =
                                    static_cast<float>(segment.endFrame);
                                const float requestedFrame =
                                    static_cast<float>(assetFrame);
                                const float denominator = endFrame - startFrame;
                                progress = denominator != 0.0F
                                    ? (requestedFrame - startFrame) / denominator
                                    : 0.0F;
                                progress = std::clamp(progress, 0.0F, 1.0F);
                                if (segment.interpolation
                                    == model::SegmentInterpolation::CubicBezier) {
                                    ++statistics.cubicSamples;
                                    progress =
                                        impl_->easing[selection.index].value(progress);
                                } else {
                                    ++statistics.linearSamples;
                                }
                            }
                        }
                    }
                    if (!materializeShapeSample(
                            modelRef,
                            firstReference,
                            secondReference,
                            mode,
                            progress,
                            shapeSlot,
                            state.shapes,
                            state.shapePoints,
                            state.hasHistory,
                            statistics,
                            nextValue)) {
                        view.error = PropertyEvaluationErrorCode::CorruptShapeStorage;
                        return view;
                    }
                    nextShapeChanged = state.shapes[shapeSlot].changed;
                    ++statistics.animatedShapeSamples;
                } else if (selection.index == kInvalidIndex) {
                    nextValue = zeroValue(property.valueType);
                } else {
                    const auto& segment = modelRef.segments[selection.index];
                    nextSegment = segment.id;
                    if (selection.position == SegmentSelection::Position::BeforeFirst) {
                        nextValue = materializeValue(modelRef, segment.startValue);
                    } else if (selection.position
                               == SegmentSelection::Position::AfterLast) {
                        nextValue = materializeValue(modelRef, segment.endValue);
                    } else if (selection.position
                               == SegmentSelection::Position::Active) {
                        const auto first =
                            materializeValue(modelRef, segment.startValue);
                        const auto second =
                            materializeValue(modelRef, segment.endValue);
                        if (segment.interpolation
                            == model::SegmentInterpolation::Hold) {
                            ++statistics.holdSamples;
                            nextValue = first;
                        } else {
                            const float startFrame =
                                static_cast<float>(segment.firstFrame);
                            const float endFrame =
                                static_cast<float>(segment.endFrame);
                            const float requestedFrame =
                                static_cast<float>(assetFrame);
                            const float denominator = endFrame - startFrame;
                            float progress = denominator != 0.0F
                                ? (requestedFrame - startFrame) / denominator
                                : 0.0F;
                            progress = std::clamp(progress, 0.0F, 1.0F);
                            if (segment.interpolation
                                == model::SegmentInterpolation::CubicBezier) {
                                ++statistics.cubicSamples;
                                progress =
                                    impl_->easing[selection.index].value(progress);
                            } else {
                                ++statistics.linearSamples;
                            }
                            if (segment.spatialInterpolation
                                == model::SpatialInterpolation::CubicBezier) {
                                if (property.valueType
                                        != model::PropertyValueType::Vec2
                                    || !impl_->spatial[selection.index].valid) {
                                    view.error = PropertyEvaluationErrorCode::CorruptSegment;
                                    return view;
                                }
                                ++statistics.spatialSamples;
                                const auto spatialSample =
                                    impl_->spatial[selection.index].sample(progress);
                                statistics.spatialLengthSearchIterations +=
                                    spatialSample.searchIterations;
                                statistics.spatialLengthSearchMaximum = std::max(
                                    statistics.spatialLengthSearchMaximum,
                                    static_cast<std::size_t>(
                                        spatialSample.searchIterations));
                                if (!spatialSample.converged) {
                                    view.error = PropertyEvaluationErrorCode::
                                        SpatialSearchDidNotConverge;
                                    return view;
                                }
                                nextValue.type = model::PropertyValueType::Vec2;
                                nextValue.storage = PropertyStorageKind::Materialized;
                                nextValue.vec2 = spatialSample.value;
                                nextSpatialAngle = spatialSample.angleDegrees;
                                nextSpatialAngleValid = true;
                                ++statistics.spatialAngleSamples;
                            } else {
                                nextValue = interpolate(first, second, progress);
                            }
                        }
                    }
                }
            }
        } else {
            view.error = PropertyEvaluationErrorCode::CorruptProperty;
            return view;
        }

        if (nextValue.storage == PropertyStorageKind::Materialized) {
            ++statistics.materializedProperties;
        } else if (nextValue.storage == PropertyStorageKind::AssetReference) {
            ++statistics.assetReferences;
        } else if (nextValue.storage == PropertyStorageKind::Unsupported) {
            ++statistics.unsupportedProperties;
        }
        const bool changed = !state.hasHistory
            || output.value != nextValue
            || output.spatialAngleDegrees != nextSpatialAngle
            || output.spatialAngleValid != nextSpatialAngleValid
            || nextShapeChanged;
        output.changed = changed;
        output.activeSegment = nextSegment;
        if (changed) {
            output.value = nextValue;
            output.spatialAngleDegrees = nextSpatialAngle;
            output.spatialAngleValid = nextSpatialAngleValid;
            ++output.revision;
            ++statistics.changedProperties;
        }
    }

    for (std::size_t index = 0; index < modelRef.sourceNodes.size(); ++index) {
        auto next = evaluateNodeTransform(
            modelRef,
            modelRef.sourceNodes[index],
            state.properties);
        auto& output = state.nodeTransforms[index];
        ++statistics.transformsVisited;
        const bool changed = !state.hasHistory
            || output.state != next.state
            || output.localMatrix != next.localMatrix
            || output.localOpacity != next.localOpacity;
        output.changed = changed;
        output.state = next.state;
        output.localMatrix = next.localMatrix;
        output.localOpacity = next.localOpacity;
        if (changed) {
            ++output.revision;
            ++statistics.transformsChanged;
        }
        if (output.supported()) ++statistics.transformsEvaluated;
        else if (output.state == NodeTransformState::Unsupported) {
            ++statistics.transformsUnsupported;
        }
    }

    for (const auto nodeId : impl_->worldOrder) {
        if (!nodeId.valid() || nodeId.index() >= modelRef.sourceNodes.size()) {
            view.error = PropertyEvaluationErrorCode::CorruptHierarchy;
            return view;
        }
        const auto& node = modelRef.sourceNodes[nodeId.index()];
        auto& output = state.nodeTransforms[nodeId.index()];
        const auto matrixParentId = matrixParent(node);
        const auto opacityParentId = node.parent;

        const EvaluatedNodeTransform* matrixParentValue = nullptr;
        const EvaluatedNodeTransform* opacityParentValue = nullptr;
        if (matrixParentId.valid()) {
            matrixParentValue = &state.nodeTransforms[matrixParentId.index()];
            if (node.transformParent.valid()) ++statistics.transformParentEdges;
            else ++statistics.structuralParentEdges;
        }
        if (opacityParentId.valid()) {
            opacityParentValue = &state.nodeTransforms[opacityParentId.index()];
            if (!matrixParentId.valid() || opacityParentId != matrixParentId) {
                ++statistics.structuralParentEdges;
            }
        }

        NodeTransformState nextWorldState = NodeTransformState::Unsupported;
        model::MotionMatrix3x2Value nextWorldMatrix = identityMatrix();
        float nextWorldOpacity = 1.0F;
        const bool localSupported = output.state != NodeTransformState::Unsupported;
        const bool matrixParentSupported = matrixParentValue == nullptr
            || matrixParentValue->worldSupported();
        const bool opacityParentSupported = opacityParentValue == nullptr
            || opacityParentValue->worldSupported();
        if (localSupported && matrixParentSupported && opacityParentSupported) {
            const auto& local = output.state == NodeTransformState::NotPresent
                ? identityMatrix() : output.localMatrix;
            const auto& parentMatrix = matrixParentValue == nullptr
                ? identityMatrix() : matrixParentValue->worldMatrix;
            nextWorldMatrix = multiply(local, parentMatrix);
            const float parentOpacity = opacityParentValue == nullptr
                ? 1.0F : opacityParentValue->worldOpacity;
            nextWorldOpacity = canonicalFloat(parentOpacity * output.localOpacity);

            const auto matrixParentState = matrixParentValue == nullptr
                ? NodeTransformState::NotPresent : matrixParentValue->worldState;
            const auto opacityParentState = opacityParentValue == nullptr
                ? NodeTransformState::NotPresent : opacityParentValue->worldState;
            nextWorldState = staticWorldState(
                output.state,
                matrixParentState,
                opacityParentState)
                ? NodeTransformState::StaticMatrix
                : NodeTransformState::Evaluated2D;
        }

        const bool worldChanged = !state.hasHistory
            || output.worldState != nextWorldState
            || output.worldMatrix != nextWorldMatrix
            || output.worldOpacity != nextWorldOpacity;
        output.worldChanged = worldChanged;
        output.worldState = nextWorldState;
        output.worldMatrix = nextWorldMatrix;
        output.worldOpacity = nextWorldOpacity;
        if (worldChanged) {
            ++output.worldRevision;
            ++statistics.worldTransformsChanged;
        }
        if (output.worldSupported()) ++statistics.worldTransformsEvaluated;
        else ++statistics.worldTransformsUnsupported;
    }

    state.hasHistory = true;
    ++state.sequence;
    view.sequence = state.sequence;
    view.properties = state.properties;
    view.nodeTransforms = state.nodeTransforms;
    view.shapes = state.shapes;
    view.shapePoints = state.shapePoints;
    view.statistics = statistics;
    return view;
}

} // namespace avemotion::evaluation
