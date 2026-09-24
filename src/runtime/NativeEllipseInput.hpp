#pragma once

#include "NativeEllipseAdmission.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace avemotion::runtime::detail {

struct NativeEllipseDecimalPower final {
    bool negative = false;
    std::string magnitude = "0";
    bool operator==(const NativeEllipseDecimalPower&) const = default;
};

struct NativeEllipseDecimal final {
    bool negative = false;
    std::string digits = "0";
    NativeEllipseDecimalPower power;
    bool operator==(const NativeEllipseDecimal&) const = default;
};

using NativeEllipseVec2 = std::array<NativeEllipseDecimal, 2>;

struct NativeEllipseStaticPosition final {
    NativeEllipseVec2 value;
    bool operator==(const NativeEllipseStaticPosition&) const = default;
};

struct NativeEllipseAnimatedPosition final {
    std::uint32_t firstFrame = 0;
    std::uint32_t lastFrame = 0;
    NativeEllipseVec2 start, end, incoming, outgoing;
    bool operator==(const NativeEllipseAnimatedPosition&) const = default;
};

using NativeEllipsePosition = std::variant<NativeEllipseStaticPosition, NativeEllipseAnimatedPosition>;

struct NativeEllipseInput final {
    std::uint32_t width = 0, height = 0, endFrame = 0;
    NativeEllipseDecimal frameRate;
    std::int32_t layerId = 0;
    std::uint32_t layerInFrame = 0, layerOutFrame = 0;
    NativeEllipseVec2 layerTranslation, size;
    NativeEllipsePosition position;
    std::array<NativeEllipseDecimal, 4> fillColor;
    std::optional<std::string> version, name, layerName, groupName, ellipseName,
        fillName, transformName;
    bool operator==(const NativeEllipseInput&) const = default;
};

struct NativeEllipseInputResult final {
    NativeEllipseAdmission admission;
    std::shared_ptr<const NativeEllipseInput> input;

    [[nodiscard]] explicit operator bool() const noexcept {
        return admission.accepted() && input != nullptr;
    }
};

[[nodiscard]] NativeEllipseInputResult decodeNativeEllipseInput(std::string_view json);

} // namespace avemotion::runtime::detail
