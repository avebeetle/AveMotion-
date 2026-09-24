#include "OwnJsonReader.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace avemotion::formats::detail {
namespace {
constexpr std::size_t MaxInput = 1'048'576;
constexpr std::size_t MaxValues = 4'096;
constexpr std::uint32_t MaxDepth = 32;
static_assert(MaxInput + 1 <= std::numeric_limits<OwnJsonNodeId>::max());

enum class State : std::uint8_t {
    ObjectFirstKey, ObjectKey, ObjectColon, ObjectValue, ObjectComma,
    ArrayFirstValue, ArrayValue, ArrayComma
};
struct Frame final {
    OwnJsonNodeId node = OwnJsonNoNode;
    OwnJsonNodeId lastChild = OwnJsonNoNode;
    State state = State::ObjectFirstKey;
    std::uint32_t keyOffset = 0;
    std::uint32_t keyLength = 0;
};
struct QueueEntry final {
    OwnJsonNodeId node = OwnJsonNoNode;
    std::uint32_t parent = 0;
    std::uint32_t depth = 0;
    std::uint32_t arrayIndex = 0;
};
template <typename T> void growForOne(std::vector<T>& values, std::size_t limit) {
    if (values.size() < values.capacity()) return;
    const auto old = values.capacity();
    // Both arenas start at eight entries, double, and stop at input bytes + 1.
    // The input gate and the assertion above make index/span casts representable.
    values.reserve(old == 0 ? std::min<std::size_t>(8, limit)
        : old >= limit / 2 ? limit : old * 2);
}
bool digit(char c) noexcept { return c >= '0' && c <= '9'; }
bool white(char c) noexcept { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
int hex(char c) noexcept {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
void appendScalar(std::string& out, std::uint32_t scalar) {
    if (scalar < 0x80) out.push_back(static_cast<char>(scalar));
    else if (scalar < 0x800) {
        out.push_back(static_cast<char>(0xC0U | (scalar >> 6U)));
        out.push_back(static_cast<char>(0x80U | (scalar & 0x3FU)));
    } else if (scalar < 0x10000) {
        out.push_back(static_cast<char>(0xE0U | (scalar >> 12U)));
        out.push_back(static_cast<char>(0x80U | ((scalar >> 6U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (scalar & 0x3FU)));
    } else {
        out.push_back(static_cast<char>(0xF0U | (scalar >> 18U)));
        out.push_back(static_cast<char>(0x80U | ((scalar >> 12U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | ((scalar >> 6U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (scalar & 0x3FU)));
    }
}
void appendPointerPart(std::string& path, std::string_view part) {
    path.push_back('/');
    for (char c : part) {
        if (c == '~') path += "~0";
        else if (c == '/') path += "~1";
        else path.push_back(c);
    }
}
} // namespace

std::span<const OwnJsonNode> OwnJsonDocument::nodes() const noexcept { return nodes_; }
const OwnJsonNode* OwnJsonDocument::node(OwnJsonNodeId id) const noexcept {
    return id < nodes_.size() ? &nodes_[id] : nullptr;
}
std::string_view OwnJsonDocument::source() const noexcept { return source_; }
std::optional<std::string_view> OwnJsonDocument::valueBytes(OwnJsonNodeId id) const noexcept {
    const auto* value = node(id);
    if (value == nullptr) return std::nullopt;
    if (value->kind == OwnJsonKind::Number)
        return std::string_view{source_}.substr(value->dataOffset, value->dataLength);
    if (value->kind == OwnJsonKind::String)
        return std::string_view{decoded_}.substr(value->dataOffset, value->dataLength);
    return std::nullopt;
}
std::optional<std::string_view> OwnJsonDocument::memberName(OwnJsonNodeId id) const noexcept {
    const auto* value = node(id);
    if (value == nullptr || !value->hasKey) return std::nullopt;
    return std::string_view{decoded_}.substr(value->keyOffset, value->keyLength);
}

class OwnJsonBuilder final {
public:
    explicit OwnJsonBuilder(std::string_view source) : document_(std::make_shared<OwnJsonDocument>()) {
        document_->source_.assign(source);
        result_.statistics.inputBytes = source.size();
        result_.statistics.sourceBytes = source.size();
        result_.statistics.nodeSizeBytes = sizeof(OwnJsonNode);
        result_.statistics.frameSizeBytes = sizeof(Frame);
    }
    OwnJsonReadResult run() {
        if (!parse()) return finish(OwnJsonReadCode::InvalidJson, "/");
        if (auto failure = inspectResources()) return finish(failure->first, std::move(failure->second));
        captureStats();
        result_.code = OwnJsonReadCode::Parsed;
        result_.document = std::move(document_);
        return std::move(result_);
    }
private:
    void captureStats() {
        auto& stats = result_.statistics;
        stats.decodedBytes = document_->decoded_.size();
        stats.nodeCount = document_->nodes_.size();
        stats.nodeCapacity = document_->nodes_.capacity();
        stats.frameCapacity = frames_.capacity();
    }
    OwnJsonReadResult finish(OwnJsonReadCode code, std::string path) {
        captureStats();
        result_.code = code;
        result_.path = std::move(path);
        return std::move(result_);
    }
    void skipWhite() {
        const auto& source = document_->source_;
        while (position_ < source.size() && white(source[position_])) ++position_;
    }
    bool literal(std::string_view word) {
        if (std::string_view{document_->source_}.substr(position_, word.size()) != word) return false;
        position_ += word.size();
        return true;
    }
    bool fourHex(std::uint32_t& output) {
        const auto& source = document_->source_;
        if (source.size() - position_ < 4) return false;
        output = 0;
        for (int i = 0; i < 4; ++i) {
            const auto value = hex(source[position_++]);
            if (value < 0) return false;
            output = (output << 4U) | static_cast<std::uint32_t>(value);
        }
        return true;
    }
    bool string(std::uint32_t& offset, std::uint32_t& length) {
        const auto& source = document_->source_;
        if (position_ >= source.size() || source[position_++] != '"') return false;
        auto& decoded = document_->decoded_;
        const auto start = decoded.size();
        while (position_ < source.size()) {
            const auto raw = static_cast<unsigned char>(source[position_++]);
            if (raw == '"') {
                offset = static_cast<std::uint32_t>(start);
                length = static_cast<std::uint32_t>(decoded.size() - start);
                return true;
            }
            if (raw < 0x20) return false;
            if (raw == '\\') {
                if (position_ == source.size()) return false;
                const char escaped = source[position_++];
                switch (escaped) {
                case '"': case '\\': case '/': decoded.push_back(escaped); break;
                case 'b': decoded.push_back('\b'); break;
                case 'f': decoded.push_back('\f'); break;
                case 'n': decoded.push_back('\n'); break;
                case 'r': decoded.push_back('\r'); break;
                case 't': decoded.push_back('\t'); break;
                case 'u': {
                    std::uint32_t scalar = 0;
                    if (!fourHex(scalar)) return false;
                    if (scalar >= 0xDC00 && scalar <= 0xDFFF) return false;
                    if (scalar >= 0xD800 && scalar <= 0xDBFF) {
                        if (source.size() - position_ < 6 || source[position_] != '\\'
                            || source[position_ + 1] != 'u') return false;
                        position_ += 2;
                        std::uint32_t low = 0;
                        if (!fourHex(low) || low < 0xDC00 || low > 0xDFFF) return false;
                        scalar = 0x10000 + ((scalar - 0xD800) << 10U) + low - 0xDC00;
                    }
                    appendScalar(decoded, scalar);
                    break;
                }
                default: return false;
                }
                continue;
            }
            if (raw < 0x80) { decoded.push_back(static_cast<char>(raw)); continue; }
            std::uint32_t scalar = 0;
            std::size_t tail = 0;
            std::uint32_t minimum = 0;
            if (raw >= 0xC2 && raw <= 0xDF) { scalar = raw & 0x1FU; tail = 1; minimum = 0x80; }
            else if (raw >= 0xE0 && raw <= 0xEF) { scalar = raw & 0x0FU; tail = 2; minimum = 0x800; }
            else if (raw >= 0xF0 && raw <= 0xF4) { scalar = raw & 0x07U; tail = 3; minimum = 0x10000; }
            else return false;
            if (source.size() - position_ < tail) return false;
            const auto rawStart = position_ - 1;
            for (std::size_t i = 0; i < tail; ++i) {
                const auto continuation = static_cast<unsigned char>(source[position_++]);
                if ((continuation & 0xC0U) != 0x80U) return false;
                scalar = (scalar << 6U) | (continuation & 0x3FU);
            }
            if (scalar < minimum || (scalar >= 0xD800 && scalar <= 0xDFFF)
                || scalar > 0x10FFFF) return false;
            decoded.append(source, rawStart, tail + 1);
        }
        return false;
    }
    bool number(std::uint32_t& offset, std::uint32_t& length) {
        const auto& source = document_->source_;
        const auto start = position_;
        if (source[position_] == '-') ++position_;
        if (position_ == source.size()) return false;
        if (source[position_] == '0') ++position_;
        else {
            if (source[position_] < '1' || source[position_] > '9') return false;
            do { ++position_; } while (position_ < source.size() && digit(source[position_]));
        }
        if (position_ < source.size() && source[position_] == '.') {
            ++position_;
            if (position_ == source.size() || !digit(source[position_])) return false;
            do { ++position_; } while (position_ < source.size() && digit(source[position_]));
        }
        if (position_ < source.size() && (source[position_] == 'e' || source[position_] == 'E')) {
            ++position_;
            if (position_ < source.size() && (source[position_] == '+' || source[position_] == '-')) ++position_;
            if (position_ == source.size() || !digit(source[position_])) return false;
            do { ++position_; } while (position_ < source.size() && digit(source[position_]));
        }
        offset = static_cast<std::uint32_t>(start);
        length = static_cast<std::uint32_t>(position_ - start);
        return true;
    }
    bool value() {
        const auto& source = document_->source_;
        if (position_ >= source.size()) return false;
        const auto token = source[position_];
        OwnJsonNode node;
        bool container = false;
        if (token == '{') { node.kind = OwnJsonKind::Object; container = true; ++position_; }
        else if (token == '[') { node.kind = OwnJsonKind::Array; container = true; ++position_; }
        else if (token == '"') {
            node.kind = OwnJsonKind::String;
            if (!string(node.dataOffset, node.dataLength)) return false;
        } else if (token == 't') { node.kind = OwnJsonKind::Boolean; node.boolean = true; if (!literal("true")) return false; }
        else if (token == 'f') { node.kind = OwnJsonKind::Boolean; if (!literal("false")) return false; }
        else if (token == 'n') { if (!literal("null")) return false; }
        else if (token == '-' || digit(token)) {
            node.kind = OwnJsonKind::Number;
            if (!number(node.dataOffset, node.dataLength)) return false;
        } else return false;
        if (!frames_.empty()) {
            auto& parent = frames_.back();
            if (parent.state == State::ObjectValue) {
                node.hasKey = true;
                node.keyOffset = parent.keyOffset;
                node.keyLength = parent.keyLength;
                parent.state = State::ObjectComma;
            } else parent.state = State::ArrayComma;
        }
        growForOne(document_->nodes_, source.size() + 1);
        const auto id = static_cast<OwnJsonNodeId>(document_->nodes_.size());
        document_->nodes_.push_back(node);
        if (!frames_.empty()) {
            auto& parent = frames_.back();
            auto& parentNode = document_->nodes_[parent.node];
            if (parent.lastChild == OwnJsonNoNode) parentNode.firstChild = id;
            else document_->nodes_[parent.lastChild].nextSibling = id;
            ++parentNode.childCount;
            parent.lastChild = id;
        }
        if (container) {
            growForOne(frames_, source.size() + 1);
            frames_.push_back(Frame{id, OwnJsonNoNode,
                node.kind == OwnJsonKind::Object ? State::ObjectFirstKey : State::ArrayFirstValue});
            result_.statistics.framePeak = std::max(result_.statistics.framePeak, frames_.size());
        }
        return true;
    }
    bool parse() {
        skipWhite();
        if (!value()) return false;
        while (!frames_.empty()) {
            skipWhite();
            if (position_ == document_->source_.size()) return false;
            const char token = document_->source_[position_];
            switch (frames_.back().state) {
            case State::ObjectFirstKey:
                if (token == '}') { ++position_; frames_.pop_back(); break; }
                [[fallthrough]];
            case State::ObjectKey:
                if (token != '"' || !string(frames_.back().keyOffset, frames_.back().keyLength)) return false;
                frames_.back().state = State::ObjectColon;
                break;
            case State::ObjectColon:
                if (token != ':') return false;
                ++position_;
                frames_.back().state = State::ObjectValue;
                break;
            case State::ObjectValue:
                if (!value()) return false;
                break;
            case State::ObjectComma:
                if (token == '}') { ++position_; frames_.pop_back(); }
                else if (token == ',') { ++position_; frames_.back().state = State::ObjectKey; }
                else return false;
                break;
            case State::ArrayFirstValue:
                if (token == ']') { ++position_; frames_.pop_back(); break; }
                [[fallthrough]];
            case State::ArrayValue:
                if (!value()) return false;
                break;
            case State::ArrayComma:
                if (token == ']') { ++position_; frames_.pop_back(); }
                else if (token == ',') { ++position_; frames_.back().state = State::ArrayValue; }
                else return false;
                break;
            }
        }
        skipWhite();
        return position_ == document_->source_.size();
    }
    std::string pathFor(const std::vector<QueueEntry>& queue, std::uint32_t parent,
        OwnJsonNodeId child, std::uint32_t arrayIndex) const {
        std::vector<std::pair<OwnJsonNodeId, std::uint32_t>> parts;
        if (child != OwnJsonNoNode) parts.emplace_back(child, arrayIndex);
        while (parent != 0) {
            const auto& entry = queue[parent];
            parts.emplace_back(entry.node, entry.arrayIndex);
            parent = entry.parent;
        }
        if (parts.empty()) return "/";
        std::string path;
        for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
            const auto& part = document_->nodes_[it->first];
            if (part.hasKey) appendPointerPart(path, *document_->memberName(it->first));
            else { path.push_back('/'); path += std::to_string(it->second); }
        }
        return path;
    }
    std::optional<std::pair<OwnJsonReadCode, std::string>> inspectResources() {
        std::vector<QueueEntry> queue;
        queue.reserve(MaxValues);
        queue.push_back(QueueEntry{0, 0, 1, 0});
        result_.statistics.resourceQueueCount = 1;
        for (std::size_t cursor = 0; cursor < queue.size(); ++cursor) {
            const auto current = queue[cursor];
            const auto& node = document_->nodes_[current.node];
            const bool container = node.kind == OwnJsonKind::Object || node.kind == OwnJsonKind::Array;
            if (!container) continue;
            if (current.depth > MaxDepth) {
                return std::pair{OwnJsonReadCode::ResourceLimit,
                    pathFor(queue, static_cast<std::uint32_t>(cursor), OwnJsonNoNode, 0)};
            }
            OwnJsonNodeId child = node.firstChild;
            std::uint32_t arrayIndex = 0;
            while (child != OwnJsonNoNode) {
                if (node.kind == OwnJsonKind::Object) {
                    for (auto previous = node.firstChild; previous != child;
                         previous = document_->nodes_[previous].nextSibling) {
                        if (document_->memberName(previous) == document_->memberName(child)) {
                            return std::pair{OwnJsonReadCode::InvalidJson,
                                pathFor(queue, static_cast<std::uint32_t>(cursor), child, arrayIndex)};
                        }
                    }
                }
                if (queue.size() == MaxValues) {
                    return std::pair{OwnJsonReadCode::ResourceLimit,
                        pathFor(queue, static_cast<std::uint32_t>(cursor), child, arrayIndex)};
                }
                const bool childContainer = document_->nodes_[child].kind == OwnJsonKind::Object
                    || document_->nodes_[child].kind == OwnJsonKind::Array;
                queue.push_back(QueueEntry{child, static_cast<std::uint32_t>(cursor),
                    current.depth + static_cast<std::uint32_t>(childContainer), arrayIndex});
                result_.statistics.resourceQueueCount = queue.size();
                child = document_->nodes_[child].nextSibling;
                ++arrayIndex;
            }
        }
        return std::nullopt;
    }
    std::shared_ptr<OwnJsonDocument> document_;
    std::vector<Frame> frames_;
    std::size_t position_ = 0;
    OwnJsonReadResult result_;
};

OwnJsonReadResult readOwnJson(std::string_view bytes) {
    if (bytes.size() > MaxInput || bytes.empty() || bytes.find('\0') != std::string_view::npos) {
        OwnJsonReadResult result;
        result.code = bytes.size() > MaxInput ? OwnJsonReadCode::ResourceLimit : OwnJsonReadCode::InvalidJson;
        result.path = "/";
        result.statistics.inputBytes = bytes.size();
        result.statistics.nodeSizeBytes = sizeof(OwnJsonNode);
        result.statistics.frameSizeBytes = sizeof(Frame);
        return result;
    }
    return OwnJsonBuilder{bytes}.run();
}
} // namespace avemotion::formats::detail
