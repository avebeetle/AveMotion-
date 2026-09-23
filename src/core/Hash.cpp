#include "avemotion/core/Hash.hpp"

#include <array>
#include <bit>
#include <iomanip>
#include <sstream>
#include <string_view>

namespace avemotion::core {
namespace {
constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;
}

std::uint64_t fnv1a64(std::span<const std::byte> bytes) noexcept {
    auto hash = kFnvOffset;
    for (const auto byte : bytes) {
        hash ^= std::to_integer<std::uint8_t>(byte);
        hash *= kFnvPrime;
    }
    return hash;
}

std::string formatHash(std::uint64_t hash) {
    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(16) << hash;
    return stream.str();
}

void Fnv1a64::appendBytes(std::span<const std::byte> bytes) noexcept {
    for (const auto byte : bytes) {
        value_ ^= std::to_integer<std::uint8_t>(byte);
        value_ *= kFnvPrime;
    }
}

void Fnv1a64::appendString(std::string_view value) noexcept {
    appendU64(static_cast<std::uint64_t>(value.size()));
    appendBytes({reinterpret_cast<const std::byte*>(value.data()), value.size()});
}

void Fnv1a64::appendU8(std::uint8_t value) noexcept {
    const std::array bytes{static_cast<std::byte>(value)};
    appendBytes(bytes);
}

void Fnv1a64::appendU32(std::uint32_t value) noexcept {
    std::array<std::byte, sizeof(value)> bytes{};
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::byte>((value >> (index * 8U)) & 0xFFU);
    }
    appendBytes(bytes);
}

void Fnv1a64::appendU64(std::uint64_t value) noexcept {
    std::array<std::byte, sizeof(value)> bytes{};
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::byte>((value >> (index * 8U)) & 0xFFU);
    }
    appendBytes(bytes);
}

void Fnv1a64::appendI32(std::int32_t value) noexcept {
    appendU32(std::bit_cast<std::uint32_t>(value));
}

void Fnv1a64::appendFloat(float value) noexcept {
    if (value == 0.0F) {
        value = 0.0F;
    }
    appendU32(std::bit_cast<std::uint32_t>(value));
}

void Fnv1a64::appendDouble(double value) noexcept {
    if (value == 0.0) {
        value = 0.0;
    }
    appendU64(std::bit_cast<std::uint64_t>(value));
}

} // namespace avemotion::core
