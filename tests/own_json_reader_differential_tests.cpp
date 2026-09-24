#include "support/OwnJsonLegacyOracle.hpp"
#include "avemotion/formats/Tgs.hpp"

#include <algorithm>
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

namespace {
namespace fs = std::filesystem;
using namespace avemotion::formats::detail;
using namespace avemotion::test;
int checks = 0, samePolicy = 0, differentPolicy = 0;
void require(bool ok, std::string_view reason) {
    ++checks;
    if (!ok) { std::cerr << "FAILED: " << reason << '\n'; std::exit(EXIT_FAILURE); }
}
std::string hex(std::string_view bytes) {
    constexpr char alphabet[] = "0123456789ABCDEF";
    std::string out;
    for (unsigned char byte : bytes) {
        out += alphabet[byte >> 4];
        out += alphabet[byte & 15];
    }
    return out;
}
[[noreturn]] void mismatch(std::string_view label, std::string_view bytes,
                           const OwnJsonObservation& own, const OwnJsonObservation& old) {
    fs::path dir = fs::path{AVEMOTION_TEST_RAW_DIR} / "failures";
    fs::create_directories(dir);
    std::string file{label};
    std::replace_if(file.begin(), file.end(),
        [](char c) { return c == '/' || c == '\\' || c == ':'; }, '_');
    std::ofstream raw{dir / (file + ".raw"), std::ios::binary};
    raw.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    std::ofstream dump{dir / (file + ".hex")};
    dump << hex(bytes) << '\n';
    std::cerr << "DIFFERENCE " << label << " own=" << static_cast<int>(own.code)
              << "/" << hex(own.path) << "/" << own.values.size()
              << " legacy=" << static_cast<int>(old.code)
              << "/" << hex(old.path) << "/" << old.values.size();
    for (std::size_t i = 0; i < std::min(own.values.size(), old.values.size()); ++i) {
        const auto& a = own.values[i]; const auto& b = old.values[i];
        if (a == b) continue;
        std::cerr << " firstRow=" << i << " field=";
        if (a.kind != b.kind) std::cerr << "kind";
        else if (a.boolean != b.boolean) std::cerr << "boolean";
        else if (a.hasKey != b.hasKey) std::cerr << "hasKey";
        else if (a.key != b.key) std::cerr << "key";
        else if (a.valueBytes != b.valueBytes) std::cerr << "valueBytes";
        else if (a.parentOrdinal != b.parentOrdinal) std::cerr << "parentOrdinal";
        else std::cerr << "childCount";
        break;
    }
    std::cerr << " raw=" << (dir / (file + ".raw")) << '\n';
    std::exit(EXIT_FAILURE);
}
bool arenaReachable(std::span<const OwnJsonNode> nodes) {
    if (nodes.empty()) return false;
    std::vector<unsigned> parents(nodes.size());
    for (const auto& node : nodes) {
        std::size_t count = 0;
        for (auto child = node.firstChild; child != OwnJsonNoNode;) {
            if (child >= nodes.size() || ++count > nodes.size() || ++parents[child] != 1) return false;
            child = nodes[child].nextSibling;
        }
        if (count != node.childCount) return false;
    }
    if (parents[0] != 0) return false;
    for (std::size_t i = 1; i < nodes.size(); ++i) if (parents[i] != 1) return false;
    std::vector<bool> seen(nodes.size());
    std::vector<OwnJsonNodeId> pending{0};
    std::size_t visited = 0;
    while (!pending.empty()) {
        const auto id = pending.back();
        pending.pop_back();
        if (id >= nodes.size() || seen[id] || ++visited > nodes.size()) return false;
        seen[id] = true;
        std::size_t siblings = 0;
        for (auto child = nodes[id].firstChild; child != OwnJsonNoNode;) {
            if (child >= nodes.size() || ++siblings > nodes.size()) return false;
            pending.push_back(child);
            child = nodes[child].nextSibling;
        }
    }
    return visited == nodes.size();
}
void reachabilityWitness() {
    std::vector<OwnJsonNode> valid(2);
    valid[0].kind = OwnJsonKind::Array;
    valid[0].firstChild = 1;
    valid[0].childCount = 1;
    require(arenaReachable(valid), "synthetic root tree reachable");
    auto disconnected = valid;
    disconnected.resize(4);
    disconnected[2].kind = OwnJsonKind::Array;
    disconnected[2].firstChild = 3;
    disconnected[2].childCount = 1;
    disconnected[3].kind = OwnJsonKind::Array;
    disconnected[3].firstChild = 2;
    disconnected[3].childCount = 1;
    require(!arenaReachable(disconnected), "disconnected two-node cycle rejected");
    auto outOfRange = valid;
    outOfRange[0].firstChild = 2;
    require(!arenaReachable(outOfRange), "out-of-range child rejected");
    auto repeated = valid;
    repeated[0].childCount = 2;
    repeated[1].nextSibling = 1;
    require(!arenaReachable(repeated), "repeated sibling rejected without hanging");
}
void indexes(const OwnJsonReadResult& result, std::string_view input) {
    const auto& s = result.statistics;
    require(s.inputBytes == input.size(), "input byte count");
    require(s.sourceBytes <= input.size() && s.decodedBytes <= input.size(), "owned byte bounds");
    require(s.nodeCount <= input.size() + 1 && s.nodeCapacity <= input.size() + 1, "node bounds");
    require(s.framePeak <= input.size() + 1 && s.frameCapacity <= input.size() + 1, "frame bounds");
    require(s.resourceQueueCount <= 4096, "queue bound");
    if (result.code != OwnJsonReadCode::Parsed) {
        require(!result.document, "rejection has no published document");
        return;
    }
    require(result.document && result.path.empty(), "parsed document and path");
    const auto& doc = *result.document;
    const auto nodes = doc.nodes();
    require(doc.source() == input && nodes.size() == s.nodeCount && !nodes.empty(), "source and arena");
    require(!doc.node(OwnJsonNoNode) && !doc.node(static_cast<OwnJsonNodeId>(nodes.size())),
        "invalid index lookups");
    std::vector<unsigned> parents(nodes.size());
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const auto& node = nodes[i];
        require(doc.node(static_cast<OwnJsonNodeId>(i)) == &node, "node lookup");
        const auto key = doc.memberName(static_cast<OwnJsonNodeId>(i));
        require(key.has_value() == node.hasKey, "key presence");
        if (key) require(key->size() == node.keyLength, "key span length");
        const auto value = doc.valueBytes(static_cast<OwnJsonNodeId>(i));
        const bool text = node.kind == OwnJsonKind::String || node.kind == OwnJsonKind::Number;
        require(value.has_value() == text, "value span presence");
        if (value) require(value->size() == node.dataLength, "value span length");
        std::size_t count = 0;
        for (auto child = node.firstChild; child != OwnJsonNoNode; child = nodes[child].nextSibling) {
            require(child < nodes.size() && ++count <= nodes.size(), "finite sibling index");
            require(++parents[child] == 1, "one parent");
            require(nodes[child].hasKey == (node.kind == OwnJsonKind::Object), "child key kind");
        }
        require(count == node.childCount, "child count");
        if (node.kind != OwnJsonKind::Object && node.kind != OwnJsonKind::Array)
            require(count == 0, "scalar child count");
    }
    require(parents[0] == 0 && nodes[0].nextSibling == OwnJsonNoNode, "root links");
    for (std::size_t i = 1; i < nodes.size(); ++i) require(parents[i] == 1, "all nodes reachable");
    require(arenaReachable(nodes), "arena traversal visits every node");
}
void compare(std::string_view label, std::string_view bytes) {
    const auto result = readOwnJson(bytes);
    indexes(result, bytes);
    const auto own = observeOwnJson(result);
    const auto old = observeLegacyJson(bytes);
    if (!(own == old)) mismatch(label, bytes, own, old);
    ++samePolicy;
}
void policy(std::string_view label, std::string_view bytes,
            OwnJsonReadCode ownCode, std::string_view ownPath,
            OwnJsonReadCode oldCode, std::string_view oldPath,
            std::string_view token = {}) {
    const auto result = readOwnJson(bytes);
    indexes(result, bytes);
    const auto own = observeOwnJson(result);
    const auto old = observeLegacyJson(bytes);
    require(own.code == ownCode && own.path == ownPath, std::string{label} + " own code/path");
    require(old.code == oldCode && old.path == oldPath, std::string{label} + " legacy code/path");
    if (ownCode == OwnJsonReadCode::Parsed && !token.empty())
        require(!own.values.empty() && own.values.back().kind == OwnJsonKind::Number
            && own.values.back().valueBytes == token, std::string{label} + " numeric spelling");
    if (ownCode == oldCode && ownPath == oldPath) {
        if (!(own == old)) mismatch(label, bytes, own, old);
        ++samePolicy;
    } else {
        require(!(own == old), std::string{label} + " visible policy difference");
        ++differentPolicy;
    }
}
void observationMutations() {
    const std::string json = R"({"k":[true,12.00e-1,"\u00e9"]})";
    const auto own = observeOwnJson(readOwnJson(json));
    const auto old = observeLegacyJson(json);
    require(own == old && own.values.size() == 5, "complete five-row ordinary observation");
    require(own.values[0].kind == OwnJsonKind::Object && own.values[0].childCount == 1
        && own.values[0].parentOrdinal == UINT32_MAX, "literal root row");
    require(own.values[1].kind == OwnJsonKind::Array && own.values[1].hasKey
        && own.values[1].key == "k" && own.values[1].parentOrdinal == 0
        && own.values[1].childCount == 3, "literal array row");
    require(own.values[2].kind == OwnJsonKind::Boolean && own.values[2].boolean
        && own.values[2].parentOrdinal == 1, "literal boolean row");
    require(own.values[3].kind == OwnJsonKind::Number
        && own.values[3].valueBytes == "12.00e-1", "literal number row");
    require(own.values[4].kind == OwnJsonKind::String
        && own.values[4].valueBytes == "\xC3\xA9", "literal decoded string row");
    auto changed = own; changed.values[1].key = "other";
    require(!(changed == old), "key mutation");
    changed = own; changed.values.back().valueBytes = "different";
    require(!(changed == old), "string mutation");
    changed = own; changed.values[3].valueBytes = "12e-1";
    require(!(changed == old), "numeric lexeme mutation");
    changed = own; changed.values[2].kind = OwnJsonKind::Null;
    require(!(changed == old), "kind mutation");
    changed = own; changed.values[2].boolean = false;
    require(!(changed == old), "boolean mutation");
    changed = own; changed.values[1].hasKey = false;
    require(!(changed == old), "key presence mutation");
    changed = own; changed.values[3].parentOrdinal = 0;
    require(!(changed == old), "parent mutation");
    changed = own; std::swap(changed.values[2], changed.values[3]);
    require(!(changed == old), "order mutation");
    changed = own; changed.values[1].childCount = 2;
    require(!(changed == old), "count mutation");
    changed = own; changed.code = OwnJsonReadCode::InvalidJson;
    require(!(changed == old), "code mutation");
    changed = own; changed.path = "/k";
    require(!(changed == old), "path mutation");
    changed = own; changed.values.pop_back();
    require(!(changed == old), "missing value mutation");
    ++samePolicy;
}
std::string zeros(std::size_t n) { return std::string(n, '0'); }
void numericPolicies() {
    const std::vector<std::string> tokens = {
        "0.0e309", "-0.0e309", "1.0e309", "0.00e310", "0.00e311",
        "0." + zeros(400) + "1e401", "0." + zeros(400) + "1e402",
        "1" + zeros(309) + "e-309", "1" + zeros(309) + "e-616",
        "1" + zeros(309) + "e-617", "-1" + zeros(309) + "e-617",
        "1" + zeros(308) + "e-308", "1.7976931348623158e308",
        "-1.7976931348623158e308", "17976931348623158e292",
        "1.79769313486231580e308"
    };
    const bool oldParsed[] = {true,true,false,true,false,true,true,false,
        false,true,true,true,false,false,false,true};
    require(tokens.size() == 16, "sixteen numeric discriminator tokens");
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        for (bool member : {false, true}) {
            const auto json = member ? "{\"x\":" + tokens[i] + "}" : tokens[i];
            policy("numeric-" + std::to_string(i + 1) + (member ? "-member" : "-scalar"),
                json, OwnJsonReadCode::Parsed, "",
                oldParsed[i] ? OwnJsonReadCode::Parsed : OwnJsonReadCode::InvalidJson,
                oldParsed[i] ? "" : "/", tokens[i]);
        }
    }
    policy("zero-integer-overflow", "0e309", OwnJsonReadCode::Parsed, "",
        OwnJsonReadCode::InvalidJson, "/", "0e309");
    policy("zero-integer-edge", "0e308", OwnJsonReadCode::Parsed, "",
        OwnJsonReadCode::Parsed, "", "0e308");
    const std::string tiny = "1e-999999999999999999999999";
    policy("huge-negative-exponent", tiny, OwnJsonReadCode::Parsed, "",
        OwnJsonReadCode::Parsed, "", tiny);
}
void unicodePolicies() {
    policy("low-value", "\"\\uDC00\"", OwnJsonReadCode::InvalidJson, "/",
        OwnJsonReadCode::Parsed, "");
    policy("low-key", "{\"\\uDC00\":0}", OwnJsonReadCode::InvalidJson, "/",
        OwnJsonReadCode::Parsed, "");
    policy("duplicate-low", "{\"\\uDC00\":0,\"\\uDC00\":1}",
        OwnJsonReadCode::InvalidJson, "/", OwnJsonReadCode::InvalidJson,
        std::string{"/\xED\xB0\x80", 4});
    policy("raw-low", std::string{"\"\xED\xB0\x80\"", 5},
        OwnJsonReadCode::InvalidJson, "/", OwnJsonReadCode::InvalidJson, "/");
    compare("valid-pair", "\"\\uD83D\\uDE00\"");
    const auto smile = observeOwnJson(readOwnJson("\"\\uD83D\\uDE00\""));
    require(smile.values.size() == 1 && smile.values[0].valueBytes == "\xF0\x9F\x98\x80",
        "literal decoded surrogate pair");
    compare("malformed-suffix", "{\"x\":1} ?");
    const auto eighth = "1" + zeros(309) + "e-309";
    const auto tenth = "1" + zeros(309) + "e-617";
    policy("duplicate-overflow", "{\"x\":0,\"x\":" + eighth + "}",
        OwnJsonReadCode::InvalidJson, "/x", OwnJsonReadCode::InvalidJson, "/");
    policy("duplicate-convertible", "{\"x\":0,\"x\":" + tenth + "}",
        OwnJsonReadCode::InvalidJson, "/x", OwnJsonReadCode::InvalidJson, "/x");
}
void resourceBoundaries() {
    const auto nested = [](std::size_t n) { return std::string(n, '[') + "0" + std::string(n, ']'); };
    const auto broad = [](std::size_t n) {
        std::string json = "[";
        for (std::size_t i = 0; i < n; ++i) json += i ? ",null" : "null";
        return json + "]";
    };
    policy("depth-32", nested(32), OwnJsonReadCode::Parsed, "", OwnJsonReadCode::Parsed, "");
    const std::string deepPath = [] {
        std::string path;
        for (int i = 0; i < 32; ++i) path += "/0";
        return path;
    }();
    constexpr std::string_view emptyDepthOwnPath = "/"
        "/0/0/0/0/0/0/0/0/0/0" "/0/0/0/0/0/0/0/0/0/0"
        "/0/0/0/0/0/0/0/0/0/0" "/0";
    constexpr std::string_view emptyDepthOldPath =
        "/0/0/0/0/0/0/0/0/0/0" "/0/0/0/0/0/0/0/0/0/0"
        "/0/0/0/0/0/0/0/0/0/0" "/0";
    policy("empty-ancestor-duplicate", "{\"\":{\"a\":1,\"a\":2}}",
        OwnJsonReadCode::InvalidJson, "//a", OwnJsonReadCode::InvalidJson, "/a");
    policy("empty-ancestor-empty-duplicate", "{\"\":{\"\":1,\"\":2}}",
        OwnJsonReadCode::InvalidJson, "//", OwnJsonReadCode::InvalidJson, "/");
    policy("nonempty-prefix-empty-ancestor", "{\"outer\":{\"\":{\"a\":1,\"a\":2}}}",
        OwnJsonReadCode::InvalidJson, "/outer//a", OwnJsonReadCode::InvalidJson, "/outer//a");
    policy("empty-ancestor-depth", "{\"\":" + nested(32) + "}",
        OwnJsonReadCode::ResourceLimit, emptyDepthOwnPath,
        OwnJsonReadCode::ResourceLimit, emptyDepthOldPath);
    policy("empty-ancestor-count", "{\"\":" + broad(4095) + "}",
        OwnJsonReadCode::ResourceLimit, "//4094", OwnJsonReadCode::ResourceLimit, "/4094");
    policy("empty-ancestor-duplicate-malformed", "{\"\":{\"a\":1,\"a\":2}} ?",
        OwnJsonReadCode::InvalidJson, "/", OwnJsonReadCode::InvalidJson, "/");
    policy("empty-ancestor-depth-malformed", "{\"\":" + nested(32) + "} ?",
        OwnJsonReadCode::InvalidJson, "/", OwnJsonReadCode::InvalidJson, "/");
    policy("empty-ancestor-count-malformed", "{\"\":" + broad(4095) + "} ?",
        OwnJsonReadCode::InvalidJson, "/", OwnJsonReadCode::InvalidJson, "/");
    policy("depth-33", nested(33), OwnJsonReadCode::ResourceLimit, deepPath,
        OwnJsonReadCode::ResourceLimit, deepPath);
    policy("children-4095", broad(4095), OwnJsonReadCode::Parsed, "", OwnJsonReadCode::Parsed, "");
    policy("children-4096", broad(4096), OwnJsonReadCode::ResourceLimit, "/4095",
        OwnJsonReadCode::ResourceLimit, "/4095");
    policy("empty-duplicate", "{\"\":0,\"\":1}", OwnJsonReadCode::InvalidJson, "/",
        OwnJsonReadCode::InvalidJson, "/");
    const std::string nulPath{"/a\0b", 4};
    policy("nul-duplicate", "{\"a\\u0000b\":1,\"a\\u0000b\":2}",
        OwnJsonReadCode::InvalidJson, nulPath, OwnJsonReadCode::InvalidJson, nulPath);
    policy("escaped-duplicate", "{\"a\":1,\"\\u0061\":2}",
        OwnJsonReadCode::InvalidJson, "/a", OwnJsonReadCode::InvalidJson, "/a");
    policy("source-duplicates", "{\"z\":{\"b\":1,\"b\":2},\"z\":0}",
        OwnJsonReadCode::InvalidJson, "/z", OwnJsonReadCode::InvalidJson, "/z");
    policy("breadth-duplicates", "{\"a\":{\"x\":0,\"x\":1},\"b\":{\"y\":0,\"y\":1}}",
        OwnJsonReadCode::InvalidJson, "/a/x", OwnJsonReadCode::InvalidJson, "/a/x");
    policy("deep-malformed", nested(33) + " ?", OwnJsonReadCode::InvalidJson, "/",
        OwnJsonReadCode::InvalidJson, "/");
    policy("broad-malformed", broad(4096) + " ?", OwnJsonReadCode::InvalidJson, "/",
        OwnJsonReadCode::InvalidJson, "/");
    policy("nul-input", std::string{"[0\0]", 4}, OwnJsonReadCode::InvalidJson, "/",
        OwnJsonReadCode::InvalidJson, "/");
    policy("empty-input", "", OwnJsonReadCode::InvalidJson, "/",
        OwnJsonReadCode::InvalidJson, "/");
}
struct Xorshift32 {
    std::uint32_t state = 0x26E2026U;
    std::uint32_t next() { state ^= state << 13; state ^= state >> 17; state ^= state << 5; return state; }
    std::uint32_t choose(std::uint32_t limit) { return next() % limit; }
};
std::string makeString(Xorshift32& random) {
    const std::string choices[] = {"\"\"", "\"plain\"", "\"\\u00e9\"",
        "\"\\uD83D\\uDE00\"", "\"\\n\\t\\u0000\"", "\"a\\/b~c\""};
    return choices[random.choose(6)];
}
std::string makeValue(Xorshift32& random, int depth, int maxDepth, int& budget, int force = -1) {
    --budget;
    int kind = force < 0 ? static_cast<int>(random.choose(6)) : force;
    if (depth >= maxDepth || budget <= 0) kind = static_cast<int>(random.choose(4));
    if (kind == 0) return "null";
    if (kind == 1) return random.choose(2) ? "true" : "false";
    if (kind == 2) {
        std::string number = std::to_string(static_cast<int>(random.choose(20001)) - 10000);
        if (random.choose(2)) number += "." + std::to_string(random.choose(100));
        if (random.choose(2)) number += "e" + std::to_string(static_cast<int>(random.choose(17)) - 8);
        return number;
    }
    if (kind == 3) return makeString(random);
    const int count = std::min<int>(random.choose(6), budget);
    std::vector<int> order;
    for (int i = 0; i < count; ++i) order.push_back(i);
    for (int i = count - 1; i > 0; --i) std::swap(order[i], order[random.choose(i + 1)]);
    std::string json = kind == 4 ? "{" : "[";
    for (int i = 0; i < count; ++i) {
        if (i) json += ',';
        if (kind == 4) json += "\"k" + std::to_string(order[i]) + "\":";
        int allowance = std::max(1, budget / (count - i));
        int childBudget = allowance;
        json += makeValue(random, depth + 1, maxDepth, childBudget);
        budget -= allowance - childBudget;
    }
    return json + (kind == 4 ? "}" : "]");
}
void generated() {
    Xorshift32 random;
    int valid = 0, mutations = 0;
    for (int i = 0; i < 512; ++i) {
        int budget = 96;
        const auto json = makeValue(random, 0, i % 7, budget, i % 6);
        require(json.size() <= 8192, "generated length bound");
        const auto label = "seed-26E2026-case-" + std::to_string(i);
        compare(label + "-valid", json); ++valid;
        auto deletion = json;
        deletion.erase(random.choose(static_cast<std::uint32_t>(deletion.size())), 1);
        if (i == 184) {
            const std::string witness = R"({"k2":"\uD83D\uDE00","k0":"uD83D\uDE00","k1":"plain","k3":true})";
            require(deletion == witness, "case 184 exact byte-deletion witness");
            policy(label + "-delete", deletion, OwnJsonReadCode::InvalidJson, "/",
                OwnJsonReadCode::Parsed, "");
            const OwnJsonObservation literal{OwnJsonReadCode::Parsed, "", {
                {OwnJsonKind::Object, false, false, "", "", UINT32_MAX, 4},
                {OwnJsonKind::String, false, true, "k2", "\xF0\x9F\x98\x80", 0, 0},
                {OwnJsonKind::String, false, true, "k0", "uD83D\xED\xB8\x80", 0, 0},
                {OwnJsonKind::String, false, true, "k1", "plain", 0, 0},
                {OwnJsonKind::Boolean, true, true, "k3", "", 0, 0}
            }};
            require(observeLegacyJson(deletion) == literal, "case 184 complete legacy rows");
        } else compare(label + "-delete", deletion);
        ++mutations;
        auto replacement = json;
        const auto delimiter = replacement.find_first_of("{}[]:,");
        if (delimiter == std::string::npos) replacement.push_back('?');
        else replacement[delimiter] = '?';
        compare(label + "-replace", replacement); ++mutations;
    }
    require(valid == 512 && mutations == 1024, "generated and mutation counts");
    std::cout << "GENERATED seed=0x26E2026 valid=" << valid << " mutations=" << mutations << '\n';
}
std::string readFile(const fs::path& path) {
    std::ifstream input{path, std::ios::binary};
    require(static_cast<bool>(input), "committed fixture opened");
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}
void fixtures() {
    const fs::path root{AVEMOTION_FIXTURE_DIR};
    for (const auto* name : {"telegram_sticker_basic.json", "repeater_content_group.json",
             "primitive_geometry.json", "trim_path_geometry.json"})
        compare(name, readFile(root / name));
    const auto tgs = readFile(root / "tgs" / "repeater_content_group.tgs");
    const std::span<const std::byte> bytes{reinterpret_cast<const std::byte*>(tgs.data()), tgs.size()};
    const auto decoded = avemotion::formats::decodeTgs(bytes);
    require(static_cast<bool>(decoded), "repeater TGS decoded");
    compare("decoded-repeater-tgs", decoded.json);
}
}
int main() {
    reachabilityWitness();
    observationMutations();
    numericPolicies();
    unicodePolicies();
    resourceBoundaries();
    generated();
    fixtures();
    std::cout << "PASS own JSON differential checks=" << checks
              << " same-policy=" << samePolicy << " intentional-difference=" << differentPolicy << '\n';
}
