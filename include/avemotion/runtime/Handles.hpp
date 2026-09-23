#pragma once

#include <cstdint>
#include <limits>

namespace avemotion::runtime {

inline constexpr std::uint32_t kInvalidHandleIndex =
    std::numeric_limits<std::uint32_t>::max();

struct AssetHandle final {
    std::uint32_t index = kInvalidHandleIndex;
    std::uint32_t generation = 0;

    [[nodiscard]] constexpr bool valid() const noexcept {
        return index != kInvalidHandleIndex && generation != 0U;
    }

    [[nodiscard]] constexpr std::uint64_t packed() const noexcept {
        return (static_cast<std::uint64_t>(generation) << 32U)
            | static_cast<std::uint64_t>(index);
    }

    [[nodiscard]] friend constexpr bool operator==(
        const AssetHandle&,
        const AssetHandle&) noexcept = default;
};

struct InstanceHandle final {
    std::uint32_t index = kInvalidHandleIndex;
    std::uint32_t generation = 0;

    [[nodiscard]] constexpr bool valid() const noexcept {
        return index != kInvalidHandleIndex && generation != 0U;
    }

    [[nodiscard]] constexpr std::uint64_t packed() const noexcept {
        return (static_cast<std::uint64_t>(generation) << 32U)
            | static_cast<std::uint64_t>(index);
    }

    [[nodiscard]] friend constexpr bool operator==(
        const InstanceHandle&,
        const InstanceHandle&) noexcept = default;
};

} // namespace avemotion::runtime
