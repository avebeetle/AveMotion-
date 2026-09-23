#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace avemotion::core {

[[nodiscard]] std::uint64_t fnv1a64(std::span<const std::byte> bytes) noexcept;
[[nodiscard]] std::string formatHash(std::uint64_t hash);

class Fnv1a64 final {
public:
    void appendBytes(std::span<const std::byte> bytes) noexcept;
    void appendString(std::string_view value) noexcept;
    void appendU8(std::uint8_t value) noexcept;
    void appendU32(std::uint32_t value) noexcept;
    void appendU64(std::uint64_t value) noexcept;
    void appendI32(std::int32_t value) noexcept;
    void appendFloat(float value) noexcept;
    void appendDouble(double value) noexcept;

    [[nodiscard]] std::uint64_t value() const noexcept { return value_; }

private:
    std::uint64_t value_ = 14695981039346656037ULL;
};

} // namespace avemotion::core
