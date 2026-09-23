#include "PrimitivePathGenerator.hpp"

#include <cmath>

namespace avemotion::render::detail {
namespace {

constexpr float kPathKappa = 0.5522847498F;
constexpr float kPi = 3.141592F;
constexpr float kTelegramCompareEpsilon = 0.000001F;
constexpr float kPolystarMagicNumber = 0.47829F / 0.28F;
constexpr float kPolygonMagicNumber = 0.25F;
constexpr float kDegreesToRadians = 0.017453292519943295769F;

struct TelegramRectF final {
    TelegramRectF(double x, double y, double width, double height) noexcept
        : x1(static_cast<float>(x)),
          y1(static_cast<float>(y)),
          x2(static_cast<float>(x + width)),
          y2(static_cast<float>(y + height)) {
    }

    [[nodiscard]] bool empty() const noexcept {
        return x1 >= x2 || y1 >= y2;
    }
    [[nodiscard]] float x() const noexcept { return x1; }
    [[nodiscard]] float y() const noexcept { return y1; }
    [[nodiscard]] float width() const noexcept { return x2 - x1; }
    [[nodiscard]] float height() const noexcept { return y2 - y1; }
    [[nodiscard]] model::MotionVec2Value center() const noexcept {
        return {x1 + (x2 - x1) / 2.0F, y1 + (y2 - y1) / 2.0F};
    }

    float x1 = 0.0F;
    float y1 = 0.0F;
    float x2 = 0.0F;
    float y2 = 0.0F;
};

[[nodiscard]] bool telegramCompare(float left, float right) noexcept {
    return std::fabs(left - right) < kTelegramCompareEpsilon;
}

[[nodiscard]] bool telegramIsZero(float value) noexcept {
    return std::fabs(value) <= kTelegramCompareEpsilon;
}

class PathWriter final {
public:
    explicit PathWriter(PrimitivePath& path) noexcept
        : path_(path) {
    }

    void moveTo(model::MotionVec2Value point) noexcept {
        appendVerb(runtime::PathVerb::MoveTo);
        appendPoint(point);
        start_ = point;
        last_ = point;
        hasPoint_ = true;
    }

    void moveTo(float x, float y) noexcept {
        moveTo({x, y});
    }

    void lineTo(model::MotionVec2Value point) noexcept {
        appendVerb(runtime::PathVerb::LineTo);
        appendPoint(point);
        last_ = point;
        hasPoint_ = true;
    }

    void lineTo(float x, float y) noexcept {
        lineTo({x, y});
    }

    void cubicTo(
        model::MotionVec2Value control1,
        model::MotionVec2Value control2,
        model::MotionVec2Value end) noexcept {
        appendVerb(runtime::PathVerb::CubicTo);
        appendPoint(control1);
        appendPoint(control2);
        appendPoint(end);
        last_ = end;
        hasPoint_ = true;
    }

    void cubicTo(
        float c1x,
        float c1y,
        float c2x,
        float c2y,
        float ex,
        float ey) noexcept {
        cubicTo({c1x, c1y}, {c2x, c2y}, {ex, ey});
    }

    void close() noexcept {
        if (!path_.valid || !hasPoint_) return;
        if (!telegramCompare(start_.x, last_.x)
            || !telegramCompare(start_.y, last_.y)) {
            lineTo(start_);
        }
        appendVerb(runtime::PathVerb::Close);
    }

private:
    void appendVerb(runtime::PathVerb verb) noexcept {
        if (path_.verbCount < path_.verbs.size()) {
            path_.verbs[path_.verbCount++] = verb;
        } else {
            path_.valid = false;
        }
    }

    void appendPoint(model::MotionVec2Value point) noexcept {
        if (path_.pointCount < path_.points.size()) {
            path_.points[path_.pointCount++] = point;
        } else {
            path_.valid = false;
        }
    }

    PrimitivePath& path_;
    model::MotionVec2Value start_{};
    model::MotionVec2Value last_{};
    bool hasPoint_ = false;
};

[[nodiscard]] bool finite(model::MotionVec2Value value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y);
}

[[nodiscard]] bool validAuthoredPointCount(float value) noexcept {
    return std::isfinite(value) && value >= 1.0F
        && value <= static_cast<float>(PrimitivePath::kMaximumAuthoredPointCount);
}

void telegramSinCos(
    float angleDegrees,
    float& sine,
    float& cosine) noexcept {
    sine = 0.0F;
    cosine = 0.0F;
    if (angleDegrees == 90.0F || angleDegrees == -270.0F) {
        sine = 1.0F;
    } else if (angleDegrees == 270.0F || angleDegrees == -90.0F) {
        sine = -1.0F;
    } else if (angleDegrees == 180.0F) {
        cosine = -1.0F;
    } else {
        const float radians = kDegreesToRadians * angleDegrees;
        sine = std::sin(radians);
        cosine = std::cos(radians);
    }
}

void applyTelegramPolystarTransform(
    PrimitivePath& path,
    model::MotionVec2Value position,
    float rotationDegrees) noexcept {
    if (!path.valid) return;

    // LOTPolystarItem::updatePath() in the pinned Telegram baseline applies
    // translate(position), then rotate(rotation), then rotate(rotation) again.
    // Reproduce the exact float operation order instead of silently fixing the
    // historical double rotation.
    float m11 = 1.0F;
    float m12 = 0.0F;
    float m21 = 0.0F;
    float m22 = 1.0F;
    if (rotationDegrees != 0.0F) {
        float sine = 0.0F;
        float cosine = 0.0F;
        telegramSinCos(rotationDegrees, sine, cosine);
        m11 = cosine;
        m12 = sine;
        m21 = -sine;
        m22 = cosine;

        const float next11 = cosine * m11 + sine * m21;
        const float next12 = cosine * m12 + sine * m22;
        const float next21 = -sine * m11 + cosine * m21;
        const float next22 = -sine * m12 + cosine * m22;
        m11 = next11;
        m12 = next12;
        m21 = next21;
        m22 = next22;
    }

    for (std::size_t index = 0; index < path.pointCount; ++index) {
        const auto point = path.points[index];
        path.points[index] = {
            m11 * point.x + m21 * point.y + position.x,
            m12 * point.x + m22 * point.y + position.y,
        };
        if (!finite(path.points[index])) {
            path.valid = false;
            return;
        }
    }
}

void bezierCoefficients(
    float t,
    float& a,
    float& b,
    float& c,
    float& d) noexcept {
    const float inverse = 1.0F - t;
    b = inverse * inverse;
    c = t * t;
    d = c * t;
    a = b * inverse;
    b *= 3.0F * t;
    c *= 3.0F * inverse;
}

[[nodiscard]] float parameterForArcAngle(float angle) noexcept {
    if (telegramCompare(angle, 0.0F)) return 0.0F;
    if (telegramCompare(angle, 90.0F)) return 1.0F;

    const float radians = (angle / 180.0F) * kPi;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    float tc = angle / 90.0F;
    for (int iteration = 0; iteration != 2; ++iteration) {
        tc -= ((((2.0F - 3.0F * kPathKappa) * tc
                     + 3.0F * (kPathKappa - 1.0F))
                    * tc)
                   * tc
               + 1.0F - cosine)
            / (((6.0F - 9.0F * kPathKappa) * tc
                   + 6.0F * (kPathKappa - 1.0F))
                * tc);
    }

    float ts = tc;
    for (int iteration = 0; iteration != 2; ++iteration) {
        ts -= ((((3.0F * kPathKappa - 2.0F) * ts
                     - 6.0F * kPathKappa + 3.0F)
                    * ts
                + 3.0F * kPathKappa)
                   * ts
               - sine)
            / (((9.0F * kPathKappa - 6.0F) * ts
                   + 12.0F * kPathKappa - 6.0F)
                  * ts
               + 3.0F * kPathKappa);
    }
    return 0.5F * (tc + ts);
}

[[nodiscard]] model::MotionVec2Value ellipsePoint(
    const TelegramRectF& rect,
    float angle) noexcept {
    float theta = angle - 360.0F * std::floor(angle / 360.0F);
    float t = theta / 90.0F;
    const int quadrant = static_cast<int>(t);
    t -= static_cast<float>(quadrant);
    t = parameterForArcAngle(90.0F * t);
    if ((quadrant & 1) != 0) t = 1.0F - t;

    float a = 0.0F;
    float b = 0.0F;
    float c = 0.0F;
    float d = 0.0F;
    bezierCoefficients(t, a, b, c, d);
    model::MotionVec2Value point{
        a + b + c * kPathKappa,
        d + c + b * kPathKappa,
    };
    if (quadrant == 1 || quadrant == 2) point.x = -point.x;
    if (quadrant == 0 || quadrant == 1) point.y = -point.y;

    const auto center = rect.center();
    const float halfWidth = rect.width() / 2.0F;
    const float halfHeight = rect.height() / 2.0F;
    return {
        center.x + halfWidth * point.x,
        center.y + halfHeight * point.y,
    };
}

void appendQuarterArc(
    PathWriter& writer,
    const TelegramRectF& rect,
    float startAngle,
    float sweepLength) noexcept {
    // VPath::curvesForArc() returns an empty start point and no curves for an
    // empty arc rectangle; VPath::arcTo() still appends that start point.
    // Preserve the legacy behavior for unusual negative authored roundness.
    if (rect.empty()) {
        writer.lineTo(0.0F, 0.0F);
        return;
    }
    const float x = rect.x();
    const float y = rect.y();
    const float width = rect.width();
    const float halfWidth = width / 2.0F;
    const float widthKappa = halfWidth * kPathKappa;
    const float height = rect.height();
    const float halfHeight = height / 2.0F;
    const float heightKappa = halfHeight * kPathKappa;

    const model::MotionVec2Value points[13]{
        {x + width, y + halfHeight},
        {x + width, y + halfHeight + heightKappa},
        {x + halfWidth + widthKappa, y + height},
        {x + halfWidth, y + height},
        {x + halfWidth - widthKappa, y + height},
        {x, y + halfHeight + heightKappa},
        {x, y + halfHeight},
        {x, y + halfHeight - heightKappa},
        {x + halfWidth - widthKappa, y},
        {x + halfWidth, y},
        {x + halfWidth + widthKappa, y},
        {x + width, y + halfHeight - heightKappa},
        {x + width, y + halfHeight},
    };

    int startSegment = static_cast<int>(std::floor(startAngle / 90.0F));
    const int endSegment = static_cast<int>(
        std::floor((startAngle + sweepLength) / 90.0F));
    float startT = (startAngle - static_cast<float>(startSegment * 90)) / 90.0F;
    float endT = (startAngle + sweepLength
        - static_cast<float>(endSegment * 90)) / 90.0F;
    const int delta = sweepLength > 0.0F ? 1 : -1;
    if (delta < 0) {
        startT = 1.0F - startT;
        endT = 1.0F - endT;
    }
    if (telegramCompare(startT - 1.0F, 0.0F)) {
        startT = 0.0F;
        startSegment += delta;
    }

    // AveMotion only calls this helper for exact quarter arcs. Preserve the
    // original Telegram segment selection and endpoint arithmetic, which is
    // what controls whether VPath::close() appends an explicit final line.
    const int quadrant = 3 - ((startSegment % 4) + 4) % 4;
    const int offset = 3 * quadrant;
    const auto startPoint = ellipsePoint(rect, startAngle);
    const auto endPoint = ellipsePoint(rect, startAngle + sweepLength);
    writer.lineTo(startPoint);
    if (delta > 0) {
        writer.cubicTo(points[offset + 2], points[offset + 1], endPoint);
    } else {
        writer.cubicTo(points[offset + 1], points[offset + 2], endPoint);
    }
    (void)endT;
}

void addSharpRectangle(
    PrimitivePath& path,
    const TelegramRectF& rect,
    model::SourcePathDirection direction) noexcept {
    PathWriter writer{path};
    const float x = rect.x();
    const float y = rect.y();
    const float width = rect.width();
    const float height = rect.height();
    if (direction == model::SourcePathDirection::Clockwise) {
        writer.moveTo(x + width, y);
        writer.lineTo(x + width, y + height);
        writer.lineTo(x, y + height);
        writer.lineTo(x, y);
    } else {
        writer.moveTo(x + width, y);
        writer.lineTo(x, y);
        writer.lineTo(x, y + height);
        writer.lineTo(x + width, y + height);
    }
    writer.close();
}

void addRoundedRectangle(
    PrimitivePath& path,
    const TelegramRectF& rect,
    float radiusX,
    float radiusY,
    model::SourcePathDirection direction) noexcept {
    PathWriter writer{path};
    path.rounded = true;
    const float x = rect.x();
    const float y = rect.y();
    const float width = rect.width();
    const float height = rect.height();

    if (direction == model::SourcePathDirection::Clockwise) {
        writer.moveTo(x + width, y + radiusY / 2.0F);
        appendQuarterArc(writer, TelegramRectF{
            x + width - radiusX, y + height - radiusY, radiusX, radiusY},
            0.0F, -90.0F);
        appendQuarterArc(writer, TelegramRectF{
            x, y + height - radiusY, radiusX, radiusY},
            -90.0F, -90.0F);
        appendQuarterArc(writer, TelegramRectF{x, y, radiusX, radiusY},
            -180.0F, -90.0F);
        appendQuarterArc(writer, TelegramRectF{
            x + width - radiusX, y, radiusX, radiusY},
            -270.0F, -90.0F);
    } else {
        writer.moveTo(x + width, y + radiusY / 2.0F);
        appendQuarterArc(writer, TelegramRectF{
            x + width - radiusX, y, radiusX, radiusY},
            0.0F, 90.0F);
        appendQuarterArc(writer, TelegramRectF{x, y, radiusX, radiusY},
            90.0F, 90.0F);
        appendQuarterArc(writer, TelegramRectF{
            x, y + height - radiusY, radiusX, radiusY},
            180.0F, 90.0F);
        appendQuarterArc(writer, TelegramRectF{
            x + width - radiusX, y + height - radiusY, radiusX, radiusY},
            270.0F, 90.0F);
    }
    writer.close();
}

} // namespace

PrimitivePath generateRectanglePath(
    model::MotionVec2Value position,
    model::MotionVec2Value size,
    float roundness,
    model::SourcePathDirection direction) noexcept {
    const float x = position.x - size.x / 2.0F;
    const float y = position.y - size.y / 2.0F;
    const TelegramRectF rect{x, y, size.x, size.y};
    PrimitivePath result;
    if (rect.empty()) return result;

    if (2.0F * roundness > rect.width()) roundness = rect.width() / 2.0F;
    if (2.0F * roundness > rect.height()) roundness = rect.height() / 2.0F;
    if (telegramCompare(roundness, 0.0F)) {
        addSharpRectangle(result, rect, direction);
        return result;
    }

    float radiusX = 2.0F * roundness;
    float radiusY = 2.0F * roundness;
    if (radiusX > rect.width()) radiusX = rect.width();
    if (radiusY > rect.height()) radiusY = rect.height();
    addRoundedRectangle(result, rect, radiusX, radiusY, direction);
    return result;
}

PrimitivePath generateEllipsePath(
    model::MotionVec2Value position,
    model::MotionVec2Value size,
    model::SourcePathDirection direction) noexcept {
    const float x = position.x - size.x / 2.0F;
    const float y = position.y - size.y / 2.0F;
    const TelegramRectF rect{x, y, size.x, size.y};
    PrimitivePath result;
    if (rect.empty()) return result;

    const float width = rect.width();
    const float halfWidth = width / 2.0F;
    const float widthKappa = halfWidth * kPathKappa;
    const float height = rect.height();
    const float halfHeight = height / 2.0F;
    const float heightKappa = halfHeight * kPathKappa;
    const float left = rect.x();
    const float top = rect.y();
    PathWriter writer{result};

    writer.moveTo(left + halfWidth, top);
    if (direction == model::SourcePathDirection::Clockwise) {
        writer.cubicTo(
            left + halfWidth + widthKappa, top,
            left + width, top + halfHeight - heightKappa,
            left + width, top + halfHeight);
        writer.cubicTo(
            left + width, top + halfHeight + heightKappa,
            left + halfWidth + widthKappa, top + height,
            left + halfWidth, top + height);
        writer.cubicTo(
            left + halfWidth - widthKappa, top + height,
            left, top + halfHeight + heightKappa,
            left, top + halfHeight);
        writer.cubicTo(
            left, top + halfHeight - heightKappa,
            left + halfWidth - widthKappa, top,
            left + halfWidth, top);
    } else {
        writer.cubicTo(
            left + halfWidth - widthKappa, top,
            left, top + halfHeight - heightKappa,
            left, top + halfHeight);
        writer.cubicTo(
            left, top + halfHeight + heightKappa,
            left + halfWidth - widthKappa, top + height,
            left + halfWidth, top + height);
        writer.cubicTo(
            left + halfWidth + widthKappa, top + height,
            left + width, top + halfHeight + heightKappa,
            left + width, top + halfHeight);
        writer.cubicTo(
            left + width, top + halfHeight - heightKappa,
            left + halfWidth + widthKappa, top,
            left + halfWidth, top);
    }
    writer.close();
    return result;
}

PrimitivePath generatePolystarPath(
    model::MotionVec2Value position,
    float pointCount,
    float innerRadius,
    float outerRadius,
    float innerRoundness,
    float outerRoundness,
    float rotationDegrees,
    model::SourcePathDirection direction) noexcept {
    PrimitivePath result;
    if (!finite(position) || !validAuthoredPointCount(pointCount)
        || !std::isfinite(innerRadius) || !std::isfinite(outerRadius)
        || !std::isfinite(innerRoundness) || !std::isfinite(outerRoundness)
        || !std::isfinite(rotationDegrees)) {
        result.valid = false;
        return result;
    }

    // This is an intentionally close, independently namespaced translation of
    // the pinned Telegram VPath::addPolystar() behavior. Operation order and
    // float constants are preserved because they are part of the established
    // pixel/evaluated-scene oracle for this extraction laboratory.
    float currentAngle = (-90.0F) * kPi / 180.0F;
    float x = 0.0F;
    float y = 0.0F;
    float partialPointRadius = 0.0F;
    const float anglePerPoint = 2.0F * kPi / pointCount;
    const float halfAnglePerPoint = anglePerPoint / 2.0F;
    const float partialPointAmount = pointCount - std::floor(pointCount);
    bool longSegment = false;
    const auto numPoints = static_cast<std::size_t>(
        std::ceil(pointCount) * 2.0F);
    const float angleDirection =
        direction == model::SourcePathDirection::Clockwise ? 1.0F : -1.0F;

    innerRoundness /= 100.0F;
    outerRoundness /= 100.0F;
    const bool hasRoundness =
        !telegramIsZero(innerRoundness) || !telegramIsZero(outerRoundness);
    result.rounded = hasRoundness;

    if (!telegramCompare(partialPointAmount, 0.0F)) {
        currentAngle += halfAnglePerPoint
            * (1.0F - partialPointAmount) * angleDirection;
    }

    if (!telegramCompare(partialPointAmount, 0.0F)) {
        partialPointRadius = innerRadius
            + partialPointAmount * (outerRadius - innerRadius);
        x = partialPointRadius * std::cos(currentAngle);
        y = partialPointRadius * std::sin(currentAngle);
        currentAngle += anglePerPoint * partialPointAmount
            / 2.0F * angleDirection;
    } else {
        x = outerRadius * std::cos(currentAngle);
        y = outerRadius * std::sin(currentAngle);
        currentAngle += halfAnglePerPoint * angleDirection;
    }

    PathWriter writer{result};
    writer.moveTo(x, y);

    for (std::size_t index = 0; index < numPoints && result.valid; ++index) {
        float radius = longSegment ? outerRadius : innerRadius;
        float deltaTheta = halfAnglePerPoint;
        if (!telegramIsZero(partialPointRadius) && index == numPoints - 2U) {
            deltaTheta = anglePerPoint * partialPointAmount / 2.0F;
        }
        if (!telegramIsZero(partialPointRadius) && index == numPoints - 1U) {
            radius = partialPointRadius;
        }

        const float previousX = x;
        const float previousY = y;
        x = radius * std::cos(currentAngle);
        y = radius * std::sin(currentAngle);

        if (hasRoundness) {
            const float control1Theta =
                std::atan2(previousY, previousX)
                - kPi / 2.0F * angleDirection;
            const float control1Dx = std::cos(control1Theta);
            const float control1Dy = std::sin(control1Theta);
            const float control2Theta =
                std::atan2(y, x) - kPi / 2.0F * angleDirection;
            const float control2Dx = std::cos(control2Theta);
            const float control2Dy = std::sin(control2Theta);

            const float control1Roundness =
                longSegment ? innerRoundness : outerRoundness;
            const float control2Roundness =
                longSegment ? outerRoundness : innerRoundness;
            const float control1Radius =
                longSegment ? innerRadius : outerRadius;
            const float control2Radius =
                longSegment ? outerRadius : innerRadius;

            float control1X = control1Radius * control1Roundness
                * kPolystarMagicNumber * control1Dx / pointCount;
            float control1Y = control1Radius * control1Roundness
                * kPolystarMagicNumber * control1Dy / pointCount;
            float control2X = control2Radius * control2Roundness
                * kPolystarMagicNumber * control2Dx / pointCount;
            float control2Y = control2Radius * control2Roundness
                * kPolystarMagicNumber * control2Dy / pointCount;

            if (!telegramIsZero(partialPointAmount)
                && (index == 0U || index == numPoints - 1U)) {
                control1X *= partialPointAmount;
                control1Y *= partialPointAmount;
                control2X *= partialPointAmount;
                control2Y *= partialPointAmount;
            }

            writer.cubicTo(
                previousX - control1X,
                previousY - control1Y,
                x + control2X,
                y + control2Y,
                x,
                y);
        } else {
            writer.lineTo(x, y);
        }

        currentAngle += deltaTheta * angleDirection;
        longSegment = !longSegment;
    }

    writer.close();
    applyTelegramPolystarTransform(result, position, rotationDegrees);
    return result;
}

PrimitivePath generatePolygonPath(
    model::MotionVec2Value position,
    float pointCount,
    float outerRadius,
    float outerRoundness,
    float rotationDegrees,
    model::SourcePathDirection direction) noexcept {
    PrimitivePath result;
    if (!finite(position) || !validAuthoredPointCount(pointCount)
        || !std::isfinite(outerRadius) || !std::isfinite(outerRoundness)
        || !std::isfinite(rotationDegrees)) {
        result.valid = false;
        return result;
    }

    const float integralPoints = std::floor(pointCount);
    if (integralPoints < 1.0F
        || integralPoints
            > static_cast<float>(PrimitivePath::kMaximumAuthoredPointCount)) {
        result.valid = false;
        return result;
    }

    // Preserve the pinned Telegram VPath::addPolygon() arithmetic exactly,
    // including its historical second -90-degree/radian conversion.
    float currentAngle = (-90.0F) * kPi / 180.0F;
    const float anglePerPoint = 2.0F * kPi / integralPoints;
    const auto numPoints = static_cast<std::size_t>(integralPoints);
    const float angleDirection =
        direction == model::SourcePathDirection::Clockwise ? 1.0F : -1.0F;
    outerRoundness /= 100.0F;
    const bool hasRoundness = !telegramIsZero(outerRoundness);
    result.rounded = hasRoundness;

    currentAngle = (currentAngle - 90.0F) * kPi / 180.0F;
    float x = outerRadius * std::cos(currentAngle);
    float y = outerRadius * std::sin(currentAngle);
    currentAngle += anglePerPoint * angleDirection;

    PathWriter writer{result};
    writer.moveTo(x, y);

    for (std::size_t index = 0; index < numPoints && result.valid; ++index) {
        const float previousX = x;
        const float previousY = y;
        x = outerRadius * std::cos(currentAngle);
        y = outerRadius * std::sin(currentAngle);

        if (hasRoundness) {
            const float control1Theta =
                std::atan2(previousY, previousX)
                - kPi / 2.0F * angleDirection;
            const float control1Dx = std::cos(control1Theta);
            const float control1Dy = std::sin(control1Theta);
            const float control2Theta =
                std::atan2(y, x) - kPi / 2.0F * angleDirection;
            const float control2Dx = std::cos(control2Theta);
            const float control2Dy = std::sin(control2Theta);

            const float control1X = outerRadius * outerRoundness
                * kPolygonMagicNumber * control1Dx;
            const float control1Y = outerRadius * outerRoundness
                * kPolygonMagicNumber * control1Dy;
            const float control2X = outerRadius * outerRoundness
                * kPolygonMagicNumber * control2Dx;
            const float control2Y = outerRadius * outerRoundness
                * kPolygonMagicNumber * control2Dy;

            writer.cubicTo(
                previousX - control1X,
                previousY - control1Y,
                x + control2X,
                y + control2Y,
                x,
                y);
        } else {
            writer.lineTo(x, y);
        }

        currentAngle += anglePerPoint * angleDirection;
    }

    writer.close();
    applyTelegramPolystarTransform(result, position, rotationDegrees);
    return result;
}

} // namespace avemotion::render::detail
