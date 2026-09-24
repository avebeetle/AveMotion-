#pragma once

#include "OwnJsonReader.hpp"

#include <rapidjson/document.h>
#include <rapidjson/memorystream.h>
#include <rapidjson/reader.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace avemotion::test {

struct OwnJsonObservedValue final {
    formats::detail::OwnJsonKind kind = formats::detail::OwnJsonKind::Null;
    bool boolean = false;
    bool hasKey = false;
    std::string key;
    std::string valueBytes;
    std::uint32_t parentOrdinal = UINT32_MAX;
    std::uint32_t childCount = 0;

    bool operator==(const OwnJsonObservedValue&) const = default;
};

struct OwnJsonObservation final {
    formats::detail::OwnJsonReadCode code = formats::detail::OwnJsonReadCode::InvalidJson;
    std::string path;
    std::vector<OwnJsonObservedValue> values;

    bool operator==(const OwnJsonObservation&) const = default;
};

inline OwnJsonObservation observeOwnJson(const formats::detail::OwnJsonReadResult& result) {
    OwnJsonObservation observation{result.code, result.path, {}};
    if (!result.document) return observation;
    const auto& document = *result.document;
    struct Pending { formats::detail::OwnJsonNodeId id; std::uint32_t parent; };
    std::vector<Pending> pending{{0, UINT32_MAX}};
    while (!pending.empty()) {
        const auto [id, parent] = pending.back();
        pending.pop_back();
        const auto* node = document.node(id);
        if (!node) return {formats::detail::OwnJsonReadCode::InvalidJson, "invalid candidate index", {}};
        OwnJsonObservedValue row;
        row.kind = node->kind;
        row.boolean = node->boolean;
        row.hasKey = node->hasKey;
        if (const auto key = document.memberName(id)) row.key.assign(*key);
        if (const auto value = document.valueBytes(id)) row.valueBytes.assign(*value);
        row.parentOrdinal = parent;
        row.childCount = node->childCount;
        const auto ordinal = static_cast<std::uint32_t>(observation.values.size());
        observation.values.push_back(std::move(row));
        std::vector<formats::detail::OwnJsonNodeId> children;
        for (auto child = node->firstChild; child != formats::detail::OwnJsonNoNode;) {
            if (children.size() >= document.nodes().size() || !document.node(child))
                return {formats::detail::OwnJsonReadCode::InvalidJson, "invalid candidate chain", {}};
            children.push_back(child);
            child = document.node(child)->nextSibling;
        }
        for (auto it = children.rbegin(); it != children.rend(); ++it) pending.push_back({*it, ordinal});
    }
    return observation;
}

namespace legacy_oracle_detail {
using Value = rapidjson::Value;
using Code = formats::detail::OwnJsonReadCode;
using Kind = formats::detail::OwnJsonKind;

inline std::string addSegment(const std::string& base, std::string_view segment) {
    std::string out = base == "/" ? "" : base;
    out += '/';
    for (const char byte : segment) {
        switch (byte) {
        case '/': out += "~1"; break;
        case '~': out += "~0"; break;
        default: out += byte; break;
        }
    }
    return out;
}

inline Kind valueKind(const Value& value) {
    if (value.IsNull()) return Kind::Null;
    if (value.IsBool()) return Kind::Boolean;
    if (value.IsNumber()) return Kind::Number;
    if (value.IsString()) return Kind::String;
    if (value.IsObject()) return Kind::Object;
    return Kind::Array;
}

struct NumberEvents final : rapidjson::BaseReaderHandler<rapidjson::UTF8<>, NumberEvents> {
    std::vector<std::string> numbers;
    bool RawNumber(const char* digits, rapidjson::SizeType size, bool) {
        numbers.emplace_back(digits, size);
        return true;
    }
};

inline OwnJsonObservation inspect(std::string_view bytes) {
    if (bytes.size() > 1'048'576) return {Code::ResourceLimit, "/", {}};
    if (bytes.empty() || bytes.find('\0') != std::string_view::npos)
        return {Code::InvalidJson, "/", {}};
    rapidjson::Document root;
    root.Parse<rapidjson::kParseIterativeFlag | rapidjson::kParseValidateEncodingFlag>(
        bytes.data(), bytes.size());
    if (root.HasParseError()) return {Code::InvalidJson, "/", {}};

    // Eligibility is breadth first. Each queued child owns its path, so pointer
    // bytes remain length aware even when a decoded member name contains NUL.
    struct Entry { const Value* value; std::string path; std::uint32_t depth; };
    std::vector<Entry> queue{{&root, "/", 1}};
    for (std::size_t cursor = 0; cursor < queue.size(); ++cursor) {
        const auto entry = queue[cursor];
        if (!(entry.value->IsObject() || entry.value->IsArray())) continue;
        if (entry.depth > 32) return {Code::ResourceLimit, entry.path, {}};
        if (entry.value->IsObject()) {
            std::vector<std::string_view> seen;
            for (auto member = entry.value->MemberBegin(); member != entry.value->MemberEnd(); ++member) {
                const std::string_view key{member->name.GetString(), member->name.GetStringLength()};
                const auto childPath = addSegment(entry.path, key);
                for (const auto previous : seen)
                    if (previous == key) return {Code::InvalidJson, childPath, {}};
                seen.push_back(key);
                if (queue.size() == 4096) return {Code::ResourceLimit, childPath, {}};
                const auto childKind = valueKind(member->value);
                queue.push_back({&member->value, childPath,
                    entry.depth + static_cast<std::uint32_t>(childKind == Kind::Object || childKind == Kind::Array)});
            }
        } else {
            for (rapidjson::SizeType index = 0; index < entry.value->Size(); ++index) {
                const auto& child = (*entry.value)[index];
                const auto childPath = addSegment(entry.path, std::to_string(index));
                if (queue.size() == 4096) return {Code::ResourceLimit, childPath, {}};
                const auto childKind = valueKind(child);
                queue.push_back({&child, childPath,
                    entry.depth + static_cast<std::uint32_t>(childKind == Kind::Object || childKind == Kind::Array)});
            }
        }
    }

    rapidjson::MemoryStream stream{bytes.data(), bytes.size()};
    rapidjson::Reader reader;
    NumberEvents tokens;
    reader.Parse<rapidjson::kParseIterativeFlag | rapidjson::kParseValidateEncodingFlag |
        rapidjson::kParseNumbersAsStringsFlag>(stream, tokens);
    if (reader.HasParseError()) return {Code::InvalidJson, "/", {}};

    OwnJsonObservation observation{Code::Parsed, "", {}};
    std::size_t numberIndex = 0;
    const auto descend = [&](const auto& self, const Value& value, std::uint32_t parent,
                             bool hasKey, std::string_view key) -> void {
        OwnJsonObservedValue row;
        row.kind = valueKind(value);
        row.boolean = value.IsBool() && value.GetBool();
        row.hasKey = hasKey;
        row.key.assign(key);
        row.parentOrdinal = parent;
        if (value.IsNumber()) {
            if (numberIndex >= tokens.numbers.size()) {
                observation.code = Code::InvalidJson;
                observation.path = "oracle number alignment";
                return;
            }
            row.valueBytes = tokens.numbers[numberIndex++];
        }
        if (value.IsString()) row.valueBytes.assign(value.GetString(), value.GetStringLength());
        if (value.IsObject()) row.childCount = value.MemberCount();
        if (value.IsArray()) row.childCount = value.Size();
        const auto ordinal = static_cast<std::uint32_t>(observation.values.size());
        observation.values.push_back(std::move(row));
        if (value.IsObject()) {
            for (auto member = value.MemberBegin(); member != value.MemberEnd(); ++member)
                self(self, member->value, ordinal, true,
                    std::string_view{member->name.GetString(), member->name.GetStringLength()});
        } else if (value.IsArray()) {
            for (const auto& child : value.GetArray()) self(self, child, ordinal, false, {});
        }
    };
    descend(descend, root, UINT32_MAX, false, {});
    if (numberIndex != tokens.numbers.size()) return {Code::InvalidJson, "oracle number alignment", {}};
    return observation;
}
} // namespace legacy_oracle_detail

inline OwnJsonObservation observeLegacyJson(std::string_view bytes) {
    return legacy_oracle_detail::inspect(bytes);
}

} // namespace avemotion::test
