#include "avemotion/formats/Tgs.hpp"

#include "MinizInflate.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <system_error>
#include <utility>
#include <vector>

namespace avemotion::formats {
namespace {

constexpr std::uint8_t kGzipId1 = 0x1FU;
constexpr std::uint8_t kGzipId2 = 0x8BU;
constexpr std::uint8_t kDeflateMethod = 8U;
constexpr std::uint8_t kFlagText = 0x01U;
constexpr std::uint8_t kFlagHeaderCrc = 0x02U;
constexpr std::uint8_t kFlagExtra = 0x04U;
constexpr std::uint8_t kFlagName = 0x08U;
constexpr std::uint8_t kFlagComment = 0x10U;
constexpr std::uint8_t kReservedFlags = 0xE0U;
constexpr std::size_t kBaseHeaderBytes = 10U;
constexpr std::size_t kTrailerBytes = 8U;

[[nodiscard]] std::uint8_t byteAt(
    std::span<const std::byte> bytes,
    std::size_t offset) noexcept {
    return std::to_integer<std::uint8_t>(bytes[offset]);
}

[[nodiscard]] std::uint16_t readLe16(
    std::span<const std::byte> bytes,
    std::size_t offset) noexcept {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(byteAt(bytes, offset))
        | (static_cast<std::uint16_t>(byteAt(bytes, offset + 1U)) << 8U));
}

[[nodiscard]] std::uint32_t readLe32(
    std::span<const std::byte> bytes,
    std::size_t offset) noexcept {
    return static_cast<std::uint32_t>(byteAt(bytes, offset))
        | (static_cast<std::uint32_t>(byteAt(bytes, offset + 1U)) << 8U)
        | (static_cast<std::uint32_t>(byteAt(bytes, offset + 2U)) << 16U)
        | (static_cast<std::uint32_t>(byteAt(bytes, offset + 3U)) << 24U);
}

[[nodiscard]] TgsDecodeResult failure(
    TgsErrorCode code,
    std::size_t offset,
    std::string message) {
    TgsDecodeResult result;
    result.error = {code, offset, std::move(message)};
    return result;
}

[[nodiscard]] bool validLimits(const TgsDecodeLimits& limits) noexcept {
    return limits.maxCompressedBytes >= kBaseHeaderBytes + kTrailerBytes
        && limits.maxJsonBytes != 0U
        && limits.maxHeaderFieldBytes != 0U
        && limits.maxExpansionRatio != 0U;
}

[[nodiscard]] bool expansionWithinLimit(
    std::size_t outputBytes,
    std::size_t compressedBytes,
    std::size_t ratio) noexcept {
    if (compressedBytes == 0U) return false;
    const auto quotient = outputBytes / compressedBytes;
    const auto remainder = outputBytes % compressedBytes;
    return quotient < ratio || (quotient == ratio && remainder == 0U);
}

[[nodiscard]] bool isJsonWhitespace(unsigned char value) noexcept {
    return value == 0x20U || value == 0x09U || value == 0x0AU || value == 0x0DU;
}

[[nodiscard]] bool hasJsonObjectEnvelope(std::string_view text) noexcept {
    if (text.size() >= 3U
        && static_cast<unsigned char>(text[0]) == 0xEFU
        && static_cast<unsigned char>(text[1]) == 0xBBU
        && static_cast<unsigned char>(text[2]) == 0xBFU) {
        return false;
    }

    std::size_t first = 0U;
    while (first < text.size()
           && isJsonWhitespace(static_cast<unsigned char>(text[first]))) {
        ++first;
    }
    if (first == text.size() || text[first] != '{') return false;

    auto last = text.size();
    while (last > first
           && isJsonWhitespace(static_cast<unsigned char>(text[last - 1U]))) {
        --last;
    }
    return last > first && text[last - 1U] == '}';
}

struct HeaderParseResult final {
    std::size_t payloadOffset = 0U;
    TgsMetadata metadata;
    TgsError error;
};

[[nodiscard]] HeaderParseResult parseHeader(
    std::span<const std::byte> bytes,
    const TgsDecodeLimits& limits,
    std::size_t trailerOffset) {
    HeaderParseResult result;
    if (bytes.size() < kBaseHeaderBytes + kTrailerBytes) {
        result.error = {
            TgsErrorCode::HeaderTruncated,
            bytes.size(),
            "the gzip member is shorter than the base header and trailer"};
        return result;
    }
    if (byteAt(bytes, 0U) != kGzipId1 || byteAt(bytes, 1U) != kGzipId2) {
        result.error = {TgsErrorCode::NotGzip, 0U, "the TGS input has no gzip magic"};
        return result;
    }
    if (byteAt(bytes, 2U) != kDeflateMethod) {
        result.error = {
            TgsErrorCode::UnsupportedCompressionMethod,
            2U,
            "the gzip member does not use the DEFLATE compression method"};
        return result;
    }

    const auto flags = byteAt(bytes, 3U);
    if ((flags & kReservedFlags) != 0U) {
        result.error = {
            TgsErrorCode::ReservedFlagsSet,
            3U,
            "the gzip header has reserved flag bits set"};
        return result;
    }

    result.metadata.modificationTime = readLe32(bytes, 4U);
    result.metadata.flags = flags;
    result.metadata.hasExtraField = (flags & kFlagExtra) != 0U;
    result.metadata.hasOriginalName = (flags & kFlagName) != 0U;
    result.metadata.hasComment = (flags & kFlagComment) != 0U;
    result.metadata.hasHeaderCrc = (flags & kFlagHeaderCrc) != 0U;
    static_cast<void>(kFlagText);

    std::size_t cursor = kBaseHeaderBytes;
    if ((flags & kFlagExtra) != 0U) {
        if (cursor + 2U > trailerOffset) {
            result.error = {
                TgsErrorCode::HeaderTruncated,
                cursor,
                "the gzip extra-field length is truncated"};
            return result;
        }
        const auto extraBytes = static_cast<std::size_t>(readLe16(bytes, cursor));
        cursor += 2U;
        if (extraBytes > limits.maxHeaderFieldBytes) {
            result.error = {
                TgsErrorCode::HeaderFieldLimitExceeded,
                cursor - 2U,
                "the gzip extra field exceeds the configured header-field limit"};
            return result;
        }
        if (extraBytes > trailerOffset - cursor) {
            result.error = {
                TgsErrorCode::HeaderTruncated,
                cursor,
                "the gzip extra field extends beyond the compressed member"};
            return result;
        }
        cursor += extraBytes;
    }

    const auto consumeZeroTerminatedField = [&](const char* label) -> TgsError {
        const auto start = cursor;
        while (cursor < trailerOffset && byteAt(bytes, cursor) != 0U) {
            if (cursor - start >= limits.maxHeaderFieldBytes) {
                return {
                    TgsErrorCode::HeaderFieldLimitExceeded,
                    start,
                    std::string{"the gzip "} + label
                        + " exceeds the configured header-field limit"};
            }
            ++cursor;
        }
        if (cursor >= trailerOffset) {
            return {
                TgsErrorCode::HeaderTruncated,
                start,
                std::string{"the gzip "} + label + " is not NUL-terminated"};
        }
        ++cursor;
        return {};
    };

    if ((flags & kFlagName) != 0U) {
        if (auto error = consumeZeroTerminatedField("original name"); error) {
            result.error = std::move(error);
            return result;
        }
    }
    if ((flags & kFlagComment) != 0U) {
        if (auto error = consumeZeroTerminatedField("comment"); error) {
            result.error = std::move(error);
            return result;
        }
    }

    if ((flags & kFlagHeaderCrc) != 0U) {
        if (cursor + 2U > trailerOffset) {
            result.error = {
                TgsErrorCode::HeaderTruncated,
                cursor,
                "the gzip header CRC is truncated"};
            return result;
        }
        const auto expected = readLe16(bytes, cursor);
        if (limits.validateHeaderCrc) {
            const auto actual = static_cast<std::uint16_t>(
                detail::crc32(bytes.first(cursor)) & 0xFFFFU);
            if (actual != expected) {
                result.error = {
                    TgsErrorCode::HeaderCrcMismatch,
                    cursor,
                    "the gzip header CRC does not match"};
                return result;
            }
        }
        cursor += 2U;
    }

    if (cursor >= trailerOffset) {
        result.error = {
            TgsErrorCode::MissingDeflatePayload,
            cursor,
            "the gzip member contains no DEFLATE payload"};
        return result;
    }
    result.payloadOffset = cursor;
    return result;
}

} // namespace

AssetFormat detectAssetFormat(std::span<const std::byte> bytes) noexcept {
    if (bytes.size() >= 2U
        && byteAt(bytes, 0U) == kGzipId1
        && byteAt(bytes, 1U) == kGzipId2) {
        return AssetFormat::TelegramTgs;
    }

    std::size_t cursor = 0U;
    while (cursor < bytes.size()
           && isJsonWhitespace(byteAt(bytes, cursor))) {
        ++cursor;
    }
    if (cursor < bytes.size() && byteAt(bytes, cursor) == static_cast<std::uint8_t>('{')) {
        return AssetFormat::LottieJson;
    }
    return AssetFormat::Unknown;
}

bool isValidUtf8(std::string_view text) noexcept {
    const auto* bytes = reinterpret_cast<const unsigned char*>(text.data());
    std::size_t index = 0U;
    while (index < text.size()) {
        const auto lead = bytes[index];
        if (lead <= 0x7FU) {
            ++index;
            continue;
        }

        std::size_t continuationCount = 0U;
        std::uint32_t codePoint = 0U;
        std::uint32_t minimum = 0U;
        if (lead >= 0xC2U && lead <= 0xDFU) {
            continuationCount = 1U;
            codePoint = lead & 0x1FU;
            minimum = 0x80U;
        } else if (lead >= 0xE0U && lead <= 0xEFU) {
            continuationCount = 2U;
            codePoint = lead & 0x0FU;
            minimum = 0x800U;
        } else if (lead >= 0xF0U && lead <= 0xF4U) {
            continuationCount = 3U;
            codePoint = lead & 0x07U;
            minimum = 0x10000U;
        } else {
            return false;
        }

        if (continuationCount > text.size() - index - 1U) return false;
        for (std::size_t continuation = 0U;
             continuation < continuationCount;
             ++continuation) {
            const auto value = bytes[index + continuation + 1U];
            if ((value & 0xC0U) != 0x80U) return false;
            codePoint = (codePoint << 6U) | (value & 0x3FU);
        }
        if (codePoint < minimum
            || codePoint > 0x10FFFFU
            || (codePoint >= 0xD800U && codePoint <= 0xDFFFU)) {
            return false;
        }
        index += continuationCount + 1U;
    }
    return true;
}

TgsDecodeResult decodeTgs(
    std::span<const std::byte> bytes,
    TgsDecodeLimits limits) {
    if (!validLimits(limits)) {
        return failure(TgsErrorCode::InvalidLimits, 0U,
            "the configured TGS limits are invalid");
    }
    if (bytes.empty()) {
        return failure(TgsErrorCode::EmptyInput, 0U, "the TGS input is empty");
    }
    if (bytes.size() > limits.maxCompressedBytes) {
        return failure(
            TgsErrorCode::CompressedSizeLimitExceeded,
            limits.maxCompressedBytes,
            "the compressed TGS input exceeds the configured size limit");
    }
    if (bytes.size() < kBaseHeaderBytes + kTrailerBytes) {
        return failure(
            TgsErrorCode::HeaderTruncated,
            bytes.size(),
            "the TGS input is too short to contain a gzip member");
    }

    const auto trailerOffset = bytes.size() - kTrailerBytes;
    auto header = parseHeader(bytes, limits, trailerOffset);
    if (header.error) {
        TgsDecodeResult result;
        result.error = std::move(header.error);
        return result;
    }

    const auto expectedCrc = readLe32(bytes, trailerOffset);
    const auto expectedJsonBytes32 = readLe32(bytes, trailerOffset + 4U);
    const auto expectedJsonBytes = static_cast<std::size_t>(expectedJsonBytes32);
    const auto deflateBytes = trailerOffset - header.payloadOffset;

    if (expectedJsonBytes == 0U) {
        return failure(TgsErrorCode::InvalidJsonEnvelope, trailerOffset + 4U,
            "the TGS trailer declares an empty JSON document");
    }
    if (expectedJsonBytes > limits.maxJsonBytes) {
        return failure(
            TgsErrorCode::JsonSizeLimitExceeded,
            trailerOffset + 4U,
            "the decompressed TGS JSON exceeds the configured size limit");
    }
    if (!expansionWithinLimit(
            expectedJsonBytes,
            deflateBytes,
            limits.maxExpansionRatio)) {
        return failure(
            TgsErrorCode::ExpansionRatioExceeded,
            header.payloadOffset,
            "the TGS expansion ratio exceeds the configured limit");
    }

    std::string json(expectedJsonBytes, '\0');
    const auto inflate = detail::inflateRawDeflate(
        bytes.subspan(header.payloadOffset, deflateBytes),
        std::span<std::byte>{
            reinterpret_cast<std::byte*>(json.data()), json.size()});

    switch (inflate.status) {
    case detail::RawInflateStatus::Done:
        break;
    case detail::RawInflateStatus::HasMoreOutput:
        return failure(
            TgsErrorCode::DeflateOutputTooSmall,
            header.payloadOffset + inflate.inputConsumed,
            "the DEFLATE stream expands beyond the size declared by the TGS trailer");
    case detail::RawInflateStatus::NeedsMoreInput:
        return failure(
            TgsErrorCode::DeflateFailed,
            header.payloadOffset + inflate.inputConsumed,
            "the DEFLATE stream is truncated");
    case detail::RawInflateStatus::BadParameter:
    case detail::RawInflateStatus::Failed:
        return failure(
            TgsErrorCode::DeflateFailed,
            header.payloadOffset + inflate.inputConsumed,
            "the TGS DEFLATE stream is corrupt");
    }

    if (limits.rejectTrailingDeflateData && inflate.inputConsumed != deflateBytes) {
        return failure(
            TgsErrorCode::TrailingCompressedData,
            header.payloadOffset + inflate.inputConsumed,
            "the gzip member contains trailing or concatenated compressed data");
    }
    if (inflate.outputWritten != expectedJsonBytes) {
        return failure(
            TgsErrorCode::SizeMismatch,
            trailerOffset + 4U,
            "the decompressed byte count does not match the TGS trailer");
    }

    const auto actualCrc = detail::crc32(std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(json.data()), json.size()});
    if (actualCrc != expectedCrc) {
        return failure(
            TgsErrorCode::CrcMismatch,
            trailerOffset,
            "the decompressed JSON CRC does not match the TGS trailer");
    }
    if (limits.requireUtf8 && !isValidUtf8(json)) {
        return failure(TgsErrorCode::InvalidUtf8, 0U,
            "the decompressed TGS payload is not valid UTF-8");
    }
    if (limits.requireJsonObject && !hasJsonObjectEnvelope(json)) {
        return failure(TgsErrorCode::InvalidJsonEnvelope, 0U,
            "the decompressed TGS payload is not a JSON object");
    }

    TgsDecodeResult result;
    result.json = std::move(json);
    result.metadata = header.metadata;
    result.metadata.compressedBytes = bytes.size();
    result.metadata.deflateBytes = deflateBytes;
    result.metadata.jsonBytes = expectedJsonBytes;
    result.metadata.crc32 = actualCrc;
    return result;
}

TgsDecodeResult decodeTgsFile(
    const std::filesystem::path& path,
    TgsDecodeLimits limits) {
    if (!validLimits(limits)) {
        return failure(TgsErrorCode::InvalidLimits, 0U,
            "the configured TGS limits are invalid");
    }

    std::error_code error;
    const auto fileBytes = std::filesystem::file_size(path, error);
    if (error) {
        return failure(TgsErrorCode::FileSizeFailed, 0U,
            "unable to determine the TGS file size: " + error.message());
    }
    if (fileBytes > limits.maxCompressedBytes
        || fileBytes > std::numeric_limits<std::size_t>::max()) {
        return failure(
            TgsErrorCode::CompressedSizeLimitExceeded,
            limits.maxCompressedBytes,
            "the compressed TGS file exceeds the configured size limit");
    }

    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        return failure(TgsErrorCode::FileOpenFailed, 0U,
            "unable to open the TGS file");
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(fileBytes));
    if (!bytes.empty()) {
        stream.read(
            reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    }
    if (!stream || stream.gcount() != static_cast<std::streamsize>(bytes.size())) {
        return failure(TgsErrorCode::FileReadFailed, 0U,
            "unable to read the complete TGS file");
    }
    return decodeTgs(bytes, limits);
}

const char* toString(TgsErrorCode code) noexcept {
    switch (code) {
    case TgsErrorCode::None: return "none";
    case TgsErrorCode::InvalidLimits: return "invalid-limits";
    case TgsErrorCode::EmptyInput: return "empty-input";
    case TgsErrorCode::CompressedSizeLimitExceeded: return "compressed-size-limit";
    case TgsErrorCode::NotGzip: return "not-gzip";
    case TgsErrorCode::HeaderTruncated: return "header-truncated";
    case TgsErrorCode::UnsupportedCompressionMethod: return "unsupported-method";
    case TgsErrorCode::ReservedFlagsSet: return "reserved-flags";
    case TgsErrorCode::HeaderFieldLimitExceeded: return "header-field-limit";
    case TgsErrorCode::HeaderCrcMismatch: return "header-crc-mismatch";
    case TgsErrorCode::MissingDeflatePayload: return "missing-deflate-payload";
    case TgsErrorCode::JsonSizeLimitExceeded: return "json-size-limit";
    case TgsErrorCode::ExpansionRatioExceeded: return "expansion-ratio-limit";
    case TgsErrorCode::DeflateFailed: return "deflate-failed";
    case TgsErrorCode::DeflateOutputTooSmall: return "deflate-output-too-small";
    case TgsErrorCode::TrailingCompressedData: return "trailing-compressed-data";
    case TgsErrorCode::SizeMismatch: return "size-mismatch";
    case TgsErrorCode::CrcMismatch: return "crc-mismatch";
    case TgsErrorCode::InvalidUtf8: return "invalid-utf8";
    case TgsErrorCode::InvalidJsonEnvelope: return "invalid-json-envelope";
    case TgsErrorCode::FileSizeFailed: return "file-size-failed";
    case TgsErrorCode::FileOpenFailed: return "file-open-failed";
    case TgsErrorCode::FileReadFailed: return "file-read-failed";
    }
    return "unknown";
}

} // namespace avemotion::formats
