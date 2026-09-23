#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace avemotion::formats::detail {

enum class RawInflateStatus : std::uint8_t {
    Done,
    NeedsMoreInput,
    HasMoreOutput,
    Failed,
    BadParameter,
};

struct RawInflateResult final {
    RawInflateStatus status = RawInflateStatus::Failed;
    std::size_t inputConsumed = 0;
    std::size_t outputWritten = 0;
};

[[nodiscard]] RawInflateResult inflateRawDeflate(
    std::span<const std::byte> input,
    std::span<std::byte> output) noexcept;

[[nodiscard]] std::uint32_t crc32(
    std::span<const std::byte> bytes) noexcept;

} // namespace avemotion::formats::detail
