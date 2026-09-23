#include "avemotion/formats/Tgs.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;
using avemotion::formats::AssetFormat;
using avemotion::formats::TgsDecodeLimits;
using avemotion::formats::TgsErrorCode;

namespace {

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

void require(bool condition, const std::string& message) {
    if (!condition) fail(message);
}

std::vector<std::byte> readBytes(const fs::path& path) {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) fail("unable to open " + path.string());
    const std::string data{
        std::istreambuf_iterator<char>{stream},
        std::istreambuf_iterator<char>{}};
    std::vector<std::byte> bytes(data.size());
    for (std::size_t index = 0U; index < data.size(); ++index) {
        bytes[index] = static_cast<std::byte>(
            static_cast<unsigned char>(data[index]));
    }
    return bytes;
}

std::string readText(const fs::path& path) {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) fail("unable to open " + path.string());
    return {
        std::istreambuf_iterator<char>{stream},
        std::istreambuf_iterator<char>{}};
}

std::uint32_t testCrc32(std::span<const std::byte> bytes) noexcept {
    std::uint32_t crc = 0xFFFFFFFFU;
    for (const auto raw : bytes) {
        crc ^= std::to_integer<std::uint8_t>(raw);
        for (int bit = 0; bit < 8; ++bit) {
            const auto mask = static_cast<std::uint32_t>(
                -static_cast<std::int32_t>(crc & 1U));
            crc = (crc >> 1U) ^ (0xEDB88320U & mask);
        }
    }
    return ~crc;
}

void appendLe16(std::vector<std::byte>& output, std::uint16_t value) {
    output.push_back(static_cast<std::byte>(value & 0xFFU));
    output.push_back(static_cast<std::byte>((value >> 8U) & 0xFFU));
}

void appendLe32(std::vector<std::byte>& output, std::uint32_t value) {
    output.push_back(static_cast<std::byte>(value & 0xFFU));
    output.push_back(static_cast<std::byte>((value >> 8U) & 0xFFU));
    output.push_back(static_cast<std::byte>((value >> 16U) & 0xFFU));
    output.push_back(static_cast<std::byte>((value >> 24U) & 0xFFU));
}

void writeLe32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) {
    bytes[offset + 0U] = static_cast<std::byte>(value & 0xFFU);
    bytes[offset + 1U] = static_cast<std::byte>((value >> 8U) & 0xFFU);
    bytes[offset + 2U] = static_cast<std::byte>((value >> 16U) & 0xFFU);
    bytes[offset + 3U] = static_cast<std::byte>((value >> 24U) & 0xFFU);
}

struct GzipOptions final {
    bool extra = false;
    bool name = false;
    bool comment = false;
    bool headerCrc = false;
};

std::vector<std::byte> makeStoredGzip(
    std::string_view payload,
    GzipOptions options = {}) {
    require(payload.size() <= 65535U, "stored test payload is too large");
    std::uint8_t flags = 0U;
    if (options.extra) flags |= 0x04U;
    if (options.name) flags |= 0x08U;
    if (options.comment) flags |= 0x10U;
    if (options.headerCrc) flags |= 0x02U;

    std::vector<std::byte> output{
        std::byte{0x1F}, std::byte{0x8B}, std::byte{0x08},
        static_cast<std::byte>(flags),
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0xFF}};
    if (options.extra) {
        appendLe16(output, 3U);
        output.push_back(std::byte{0x41});
        output.push_back(std::byte{0x56});
        output.push_back(std::byte{0x4D});
    }
    if (options.name) {
        constexpr std::string_view name = "fixture.tgs";
        for (const auto value : name) {
            output.push_back(static_cast<std::byte>(static_cast<unsigned char>(value)));
        }
        output.push_back(std::byte{0});
    }
    if (options.comment) {
        constexpr std::string_view comment = "AveMotion TGS test";
        for (const auto value : comment) {
            output.push_back(static_cast<std::byte>(static_cast<unsigned char>(value)));
        }
        output.push_back(std::byte{0});
    }
    if (options.headerCrc) {
        appendLe16(output, static_cast<std::uint16_t>(testCrc32(output) & 0xFFFFU));
    }

    // One final uncompressed DEFLATE block. This exercises the production
    // inflater without depending on a compressor in the test executable.
    output.push_back(std::byte{0x01});
    const auto length = static_cast<std::uint16_t>(payload.size());
    appendLe16(output, length);
    appendLe16(output, static_cast<std::uint16_t>(~length));
    for (const auto value : payload) {
        output.push_back(static_cast<std::byte>(static_cast<unsigned char>(value)));
    }

    const auto payloadBytes = std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(payload.data()), payload.size()};
    appendLe32(output, testCrc32(payloadBytes));
    appendLe32(output, static_cast<std::uint32_t>(payload.size()));
    return output;
}

void requireError(
    const avemotion::formats::TgsDecodeResult& result,
    TgsErrorCode expected,
    std::string_view label) {
    require(!result, std::string{label} + " unexpectedly succeeded");
    require(result.error.code == expected,
        std::string{label} + " returned "
            + avemotion::formats::toString(result.error.code)
            + " instead of " + avemotion::formats::toString(expected));
}

} // namespace

int main() {
    const fs::path fixtureDir{AVEMOTION_TGS_FIXTURE_DIR};
    const fs::path sourceFixtureDir{AVEMOTION_FIXTURE_DIR};
    const auto fixturePath = fixtureDir / "repeater_content_group.tgs";
    const auto jsonPath = sourceFixtureDir / "repeater_content_group.json";
    const auto fixture = readBytes(fixturePath);
    const auto expectedJson = readText(jsonPath);

    require(avemotion::formats::detectAssetFormat(fixture)
                == AssetFormat::TelegramTgs,
        "TGS magic detection failed");
    const auto jsonBytes = std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(expectedJson.data()), expectedJson.size()};
    require(avemotion::formats::detectAssetFormat(jsonBytes)
                == AssetFormat::LottieJson,
        "plain JSON detection failed");
    constexpr std::array<std::byte, 3> unknown{
        std::byte{0x01}, std::byte{0x02}, std::byte{0x03}};
    require(avemotion::formats::detectAssetFormat(unknown)
                == AssetFormat::Unknown,
        "unknown input was misdetected");

    const auto decoded = avemotion::formats::decodeTgs(fixture);
    require(static_cast<bool>(decoded),
        "valid dynamic-Huffman fixture failed: " + decoded.error.message);
    require(decoded.json == expectedJson, "decoded TGS JSON differs from source JSON");
    require(decoded.metadata.compressedBytes == fixture.size(),
        "compressed byte diagnostics are wrong");
    require(decoded.metadata.jsonBytes == expectedJson.size(),
        "decompressed byte diagnostics are wrong");
    require(decoded.metadata.deflateBytes > 0U, "deflate byte diagnostics are empty");
    require(decoded.metadata.crc32 == testCrc32(jsonBytes),
        "TGS CRC diagnostics are wrong");

    const auto fromFile = avemotion::formats::decodeTgsFile(fixturePath);
    require(static_cast<bool>(fromFile) && fromFile.json == expectedJson,
        "TGS file loading failed");

    const auto optional = makeStoredGzip(
        R"({"v":"5.7.4","fr":60,"ip":0,"op":1,"w":1,"h":1,"layers":[]})",
        {.extra = true, .name = true, .comment = true, .headerCrc = true});
    const auto optionalDecoded = avemotion::formats::decodeTgs(optional);
    require(static_cast<bool>(optionalDecoded),
        "valid optional gzip fields were rejected: " + optionalDecoded.error.message);
    require(optionalDecoded.metadata.hasExtraField
            && optionalDecoded.metadata.hasOriginalName
            && optionalDecoded.metadata.hasComment
            && optionalDecoded.metadata.hasHeaderCrc,
        "optional gzip field metadata is incomplete");

    require(avemotion::formats::isValidUtf8("AveMotion"),
        "ASCII must be valid UTF-8");
    require(avemotion::formats::isValidUtf8("Рыба-пила"),
        "valid Cyrillic UTF-8 was rejected");
    const std::string invalidUtf8{"\xF0\x28\x8C\x28", 4};
    require(!avemotion::formats::isValidUtf8(invalidUtf8),
        "invalid UTF-8 was accepted");

    requireError(avemotion::formats::decodeTgs({}),
        TgsErrorCode::EmptyInput, "empty input");

    auto tinyLimits = TgsDecodeLimits::telegramSticker();
    tinyLimits.maxCompressedBytes = 32U;
    requireError(avemotion::formats::decodeTgs(fixture, tinyLimits),
        TgsErrorCode::CompressedSizeLimitExceeded, "compressed limit");

    auto wrongMagic = optional;
    wrongMagic[0] = std::byte{0};
    requireError(avemotion::formats::decodeTgs(wrongMagic),
        TgsErrorCode::NotGzip, "wrong magic");

    auto wrongMethod = optional;
    wrongMethod[2] = std::byte{0};
    requireError(avemotion::formats::decodeTgs(wrongMethod),
        TgsErrorCode::UnsupportedCompressionMethod, "wrong compression method");

    auto reservedFlags = optional;
    reservedFlags[3] |= std::byte{0x20};
    requireError(avemotion::formats::decodeTgs(reservedFlags),
        TgsErrorCode::ReservedFlagsSet, "reserved gzip flags");

    auto badHeaderCrc = optional;
    // The FHCRC is immediately before the raw DEFLATE block. Find the block by
    // scanning for the final stored-block marker after the known comment.
    std::size_t storedBlock = 10U;
    storedBlock += 2U + 3U;
    storedBlock += std::string_view{"fixture.tgs"}.size() + 1U;
    storedBlock += std::string_view{"AveMotion TGS test"}.size() + 1U;
    badHeaderCrc[storedBlock] ^= std::byte{0x01};
    requireError(avemotion::formats::decodeTgs(badHeaderCrc),
        TgsErrorCode::HeaderCrcMismatch, "header CRC");
    auto noHeaderCrcValidation = TgsDecodeLimits::telegramSticker();
    noHeaderCrcValidation.validateHeaderCrc = false;
    require(static_cast<bool>(avemotion::formats::decodeTgs(
                badHeaderCrc, noHeaderCrcValidation)),
        "disabled FHCRC validation did not accept an otherwise valid member");

    auto shortHeader = optional;
    shortHeader.resize(12U);
    requireError(avemotion::formats::decodeTgs(shortHeader),
        TgsErrorCode::HeaderTruncated, "truncated header");

    auto longName = makeStoredGzip("{}", {.name = true});
    auto headerLimits = TgsDecodeLimits::telegramSticker();
    headerLimits.maxHeaderFieldBytes = 4U;
    requireError(avemotion::formats::decodeTgs(longName, headerLimits),
        TgsErrorCode::HeaderFieldLimitExceeded, "header field limit");

    auto badDeflate = optional;
    badDeflate[storedBlock + 2U] = std::byte{0};
    requireError(avemotion::formats::decodeTgs(badDeflate),
        TgsErrorCode::DeflateFailed, "corrupt DEFLATE");

    auto trailing = makeStoredGzip("{}");
    trailing.insert(trailing.end() - 8, std::byte{0});
    requireError(avemotion::formats::decodeTgs(trailing),
        TgsErrorCode::TrailingCompressedData, "trailing compressed data");
    auto allowTrailing = TgsDecodeLimits::telegramSticker();
    allowTrailing.rejectTrailingDeflateData = false;
    require(static_cast<bool>(avemotion::formats::decodeTgs(
                trailing, allowTrailing)),
        "explicit trailing-data opt-out was ignored");

    auto concatenated = makeStoredGzip("{}");
    const auto secondMember = makeStoredGzip("{}");
    concatenated.insert(
        concatenated.end(), secondMember.begin(), secondMember.end());
    requireError(avemotion::formats::decodeTgs(concatenated),
        TgsErrorCode::TrailingCompressedData, "concatenated gzip members");

    auto smallSize = makeStoredGzip(R"({"a":1})");
    writeLe32(smallSize, smallSize.size() - 4U, 2U);
    requireError(avemotion::formats::decodeTgs(smallSize),
        TgsErrorCode::DeflateOutputTooSmall, "small ISIZE");

    auto largeSize = makeStoredGzip(R"({"a":1})");
    writeLe32(largeSize, largeSize.size() - 4U, 64U);
    requireError(avemotion::formats::decodeTgs(largeSize),
        TgsErrorCode::SizeMismatch, "large ISIZE");

    auto badCrc = makeStoredGzip(R"({"a":1})");
    badCrc[badCrc.size() - 8U] ^= std::byte{0x01};
    requireError(avemotion::formats::decodeTgs(badCrc),
        TgsErrorCode::CrcMismatch, "payload CRC");

    auto jsonLimits = TgsDecodeLimits::telegramSticker();
    jsonLimits.maxJsonBytes = 4U;
    requireError(avemotion::formats::decodeTgs(makeStoredGzip(R"({"a":1})"), jsonLimits),
        TgsErrorCode::JsonSizeLimitExceeded, "JSON size limit");

    auto ratioBomb = makeStoredGzip("{}");
    writeLe32(ratioBomb, ratioBomb.size() - 4U, 4096U);
    auto ratioLimits = TgsDecodeLimits::telegramSticker();
    ratioLimits.maxJsonBytes = 8192U;
    ratioLimits.maxExpansionRatio = 2U;
    requireError(avemotion::formats::decodeTgs(ratioBomb, ratioLimits),
        TgsErrorCode::ExpansionRatioExceeded, "expansion ratio");

    const std::string invalidUtf8Json{"{\"x\":\"\xF0\x28\x8C\x28\"}", 12};
    requireError(avemotion::formats::decodeTgs(makeStoredGzip(invalidUtf8Json)),
        TgsErrorCode::InvalidUtf8, "invalid UTF-8 payload");
    requireError(avemotion::formats::decodeTgs(makeStoredGzip("[]")),
        TgsErrorCode::InvalidJsonEnvelope, "non-object JSON");
    auto allowNonUtf8 = TgsDecodeLimits::telegramSticker();
    allowNonUtf8.requireUtf8 = false;
    require(static_cast<bool>(avemotion::formats::decodeTgs(
                makeStoredGzip(invalidUtf8Json), allowNonUtf8)),
        "explicit UTF-8 validation opt-out was ignored");

    auto allowAnyJsonEnvelope = TgsDecodeLimits::telegramSticker();
    allowAnyJsonEnvelope.requireJsonObject = false;
    require(static_cast<bool>(avemotion::formats::decodeTgs(
                makeStoredGzip("[]"), allowAnyJsonEnvelope)),
        "explicit JSON-object validation opt-out was ignored");

    auto invalidLimits = TgsDecodeLimits::telegramSticker();
    invalidLimits.maxExpansionRatio = 0U;
    requireError(avemotion::formats::decodeTgs(optional, invalidLimits),
        TgsErrorCode::InvalidLimits, "invalid limits");

    const auto missingFile = avemotion::formats::decodeTgsFile(
        fixtureDir / "does-not-exist.tgs");
    requireError(missingFile, TgsErrorCode::FileSizeFailed, "missing file");

    // Deterministic mutation smoke: every malformed input must return a typed
    // result and must never escape the configured output bounds.
    for (std::size_t index = 0U; index < fixture.size(); index += 7U) {
        auto mutated = fixture;
        mutated[index] ^= static_cast<std::byte>(0x5AU);
        const auto result = avemotion::formats::decodeTgs(mutated);
        if (result) {
            require(result.json.size()
                    <= TgsDecodeLimits::telegramSticker().maxJsonBytes,
                "mutated TGS escaped the JSON size bound");
            require(avemotion::formats::isValidUtf8(result.json),
                "successful mutated TGS produced invalid UTF-8");
        } else {
            require(result.error.code != TgsErrorCode::None,
                "failed mutated TGS has no typed error");
        }
    }

    std::cout << "AveMotion hardened TGS decoder tests passed\n";
    return EXIT_SUCCESS;
}
