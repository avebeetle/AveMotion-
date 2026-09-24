#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace avemotion::formats::detail {

using OwnJsonNodeId = std::uint32_t;
inline constexpr OwnJsonNodeId OwnJsonNoNode = UINT32_MAX;

enum class OwnJsonKind : std::uint8_t { Null, Boolean, Number, String, Object, Array };

struct OwnJsonNode final {
    OwnJsonKind kind = OwnJsonKind::Null;
    bool boolean = false;
    bool hasKey = false;
    OwnJsonNodeId firstChild = OwnJsonNoNode;
    OwnJsonNodeId nextSibling = OwnJsonNoNode;
    std::uint32_t childCount = 0;
    std::uint32_t keyOffset = 0;
    std::uint32_t keyLength = 0;
    std::uint32_t dataOffset = 0;
    std::uint32_t dataLength = 0;
};

struct OwnJsonReadStatistics final {
    std::size_t inputBytes = 0;
    std::size_t sourceBytes = 0;
    std::size_t decodedBytes = 0;
    std::size_t nodeCount = 0;
    std::size_t nodeCapacity = 0;
    std::size_t framePeak = 0;
    std::size_t frameCapacity = 0;
    std::size_t nodeSizeBytes = 0;
    std::size_t frameSizeBytes = 0;
    std::size_t resourceQueueCount = 0;
};

class OwnJsonBuilder;

class OwnJsonDocument final {
public:
    [[nodiscard]] std::span<const OwnJsonNode> nodes() const noexcept;
    [[nodiscard]] const OwnJsonNode* node(OwnJsonNodeId id) const noexcept;
    [[nodiscard]] std::string_view source() const noexcept;
    [[nodiscard]] std::optional<std::string_view> valueBytes(OwnJsonNodeId id) const noexcept;
    [[nodiscard]] std::optional<std::string_view> memberName(OwnJsonNodeId id) const noexcept;

private:
    friend class OwnJsonBuilder;
    std::string source_;
    std::string decoded_;
    std::vector<OwnJsonNode> nodes_;
};

enum class OwnJsonReadCode { Parsed, InvalidJson, ResourceLimit };

struct OwnJsonReadResult final {
    OwnJsonReadCode code = OwnJsonReadCode::InvalidJson;
    std::string path;
    std::shared_ptr<const OwnJsonDocument> document;
    OwnJsonReadStatistics statistics;

    [[nodiscard]] explicit operator bool() const noexcept {
        return code == OwnJsonReadCode::Parsed && static_cast<bool>(document);
    }
};

[[nodiscard]] OwnJsonReadResult readOwnJson(std::string_view bytes);

} // namespace avemotion::formats::detail
