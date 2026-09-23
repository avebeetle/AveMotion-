#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace avemotion::formats {

enum class AssetFormat : std::uint8_t {
    Unknown,
    LottieJson,
    TelegramTgs,
};

struct TgsDecodeLimits final {
    // Telegram animated stickers are intentionally small. These defaults are
    // strict enough for untrusted UI assets while remaining configurable for
    // private applications that intentionally use larger Lottie documents.
    std::size_t maxCompressedBytes = 64U * 1024U;
    std::size_t maxJsonBytes = 2U * 1024U * 1024U;
    std::size_t maxHeaderFieldBytes = 4U * 1024U;
    std::size_t maxExpansionRatio = 128U;
    bool validateHeaderCrc = true;
    bool requireUtf8 = true;
    bool requireJsonObject = true;
    bool rejectTrailingDeflateData = true;

    [[nodiscard]] static constexpr TgsDecodeLimits telegramSticker() noexcept {
        return {};
    }

    [[nodiscard]] static constexpr TgsDecodeLimits relaxedApplicationAsset()
        noexcept {
        TgsDecodeLimits value;
        value.maxCompressedBytes = 1024U * 1024U;
        value.maxJsonBytes = 16U * 1024U * 1024U;
        value.maxHeaderFieldBytes = 16U * 1024U;
        value.maxExpansionRatio = 256U;
        return value;
    }
};

enum class TgsErrorCode : std::uint8_t {
    None,
    InvalidLimits,
    EmptyInput,
    CompressedSizeLimitExceeded,
    NotGzip,
    HeaderTruncated,
    UnsupportedCompressionMethod,
    ReservedFlagsSet,
    HeaderFieldLimitExceeded,
    HeaderCrcMismatch,
    MissingDeflatePayload,
    JsonSizeLimitExceeded,
    ExpansionRatioExceeded,
    DeflateFailed,
    DeflateOutputTooSmall,
    TrailingCompressedData,
    SizeMismatch,
    CrcMismatch,
    InvalidUtf8,
    InvalidJsonEnvelope,
    FileSizeFailed,
    FileOpenFailed,
    FileReadFailed,
};

struct TgsError final {
    TgsErrorCode code = TgsErrorCode::None;
    std::size_t offset = 0;
    std::string message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return code != TgsErrorCode::None;
    }
};

struct TgsMetadata final {
    std::size_t compressedBytes = 0;
    std::size_t deflateBytes = 0;
    std::size_t jsonBytes = 0;
    std::uint32_t crc32 = 0;
    std::uint32_t modificationTime = 0;
    std::uint8_t flags = 0;
    bool hasExtraField = false;
    bool hasOriginalName = false;
    bool hasComment = false;
    bool hasHeaderCrc = false;
};

struct TgsDecodeResult final {
    std::string json;
    TgsMetadata metadata;
    TgsError error;

    [[nodiscard]] explicit operator bool() const noexcept {
        return !error && !json.empty();
    }
};

[[nodiscard]] AssetFormat detectAssetFormat(
    std::span<const std::byte> bytes) noexcept;

[[nodiscard]] bool isValidUtf8(std::string_view text) noexcept;

[[nodiscard]] TgsDecodeResult decodeTgs(
    std::span<const std::byte> bytes,
    TgsDecodeLimits limits = TgsDecodeLimits::telegramSticker());

[[nodiscard]] TgsDecodeResult decodeTgsFile(
    const std::filesystem::path& path,
    TgsDecodeLimits limits = TgsDecodeLimits::telegramSticker());

[[nodiscard]] const char* toString(TgsErrorCode code) noexcept;

} // namespace avemotion::formats
