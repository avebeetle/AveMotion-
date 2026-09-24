#include "OwnJsonReader.hpp"
#include "avemotion/formats/Tgs.hpp"

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
#include <thread>
#include <vector>

using namespace avemotion::formats::detail;

namespace {
namespace fs = std::filesystem;
int checks = 0;
void require(bool condition, const char* message) {
    ++checks;
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void expect(std::string_view input, OwnJsonReadCode code, std::string_view path) {
    const auto result = readOwnJson(input);
    if (result.code != code || result.path != path ||
        static_cast<bool>(result.document) != (code == OwnJsonReadCode::Parsed)) {
        std::cerr << "FAILED: expected code " << static_cast<int>(code)
                  << " path length " << path.size() << ", got code "
                  << static_cast<int>(result.code) << " path length "
                  << result.path.size() << '\n';
        std::exit(EXIT_FAILURE);
    }
    require(result.statistics.resourceQueueCount <= 4096, "bounded resource queue");
    require(result.statistics.sourceBytes <= input.size(), "bounded owned source");
    require(result.statistics.decodedBytes <= input.size(), "bounded decoded bytes");
    require(result.statistics.nodeCount <= input.size() + 1, "bounded logical nodes");
    require(result.statistics.framePeak <= input.size() + 1, "bounded logical frames");
    require(result.statistics.nodeCapacity <= input.size() + 1, "bounded node capacity");
    require(result.statistics.frameCapacity <= input.size() + 1, "bounded frame capacity");
}

std::string nestedArrays(std::size_t count) {
    return std::string(count, '[') + '0' + std::string(count, ']');
}

std::string repeatPath(std::size_t count) {
    std::string path;
    for (std::size_t i = 0; i < count; ++i) path += "/0";
    return path;
}

std::string arrayOfNulls(std::size_t count) {
    std::string result = "[";
    for (std::size_t i = 0; i < count; ++i) {
        if (i != 0) result.push_back(',');
        result += "null";
    }
    result.push_back(']');
    return result;
}

void checkArena(const OwnJsonDocument& doc, const OwnJsonReadStatistics& stats) {
    const auto nodes = doc.nodes();
    require(!nodes.empty(), "nonempty arena");
    require(stats.nodeCount == nodes.size(), "reported node count");
    require(doc.node(OwnJsonNoNode) == nullptr, "sentinel node rejected");
    require(doc.node(static_cast<OwnJsonNodeId>(nodes.size())) == nullptr, "past-end node rejected");
    require(!doc.valueBytes(OwnJsonNoNode).has_value(), "bad value lookup rejected");
    require(!doc.memberName(OwnJsonNoNode).has_value(), "bad key lookup rejected");
    std::vector<unsigned> parents(nodes.size());
    for (std::size_t parent = 0; parent < nodes.size(); ++parent) {
        const auto& node = nodes[parent];
        auto child = node.firstChild;
        std::size_t count = 0;
        while (child != OwnJsonNoNode) {
            require(child < nodes.size(), "child index in arena");
            require(++count <= nodes.size(), "sibling chain acyclic");
            ++parents[child];
            require(parents[child] == 1, "one parent per child");
            require(nodes[child].hasKey == (node.kind == OwnJsonKind::Object), "member key presence");
            child = nodes[child].nextSibling;
        }
        require(count == node.childCount, "exact child count");
        if (node.kind != OwnJsonKind::Array && node.kind != OwnJsonKind::Object)
            require(count == 0, "scalar has no children");
        if (node.kind == OwnJsonKind::String || node.kind == OwnJsonKind::Number)
            require(doc.valueBytes(static_cast<OwnJsonNodeId>(parent)).has_value(), "value span exists");
        else require(!doc.valueBytes(static_cast<OwnJsonNodeId>(parent)).has_value(), "nontext has no value span");
    }
    require(parents[0] == 0, "root has no parent");
    require(!nodes[0].hasKey && nodes[0].nextSibling == OwnJsonNoNode, "root unlinked");
    for (std::size_t i = 1; i < nodes.size(); ++i) require(parents[i] == 1, "no lost node");
}

void stress(std::string_view label, const std::string& input,
    OwnJsonReadCode code, std::string_view path) {
    const auto result = readOwnJson(input);
    if (result.code != code || result.path != path ||
        static_cast<bool>(result.document) != (code == OwnJsonReadCode::Parsed)) {
        const auto raw = fs::path{AVEMOTION_TEST_RAW_DIR} / (std::string{label} + ".raw");
        std::ofstream stream{raw, std::ios::binary};
        stream.write(input.data(), static_cast<std::streamsize>(input.size()));
        std::cerr << "FAILED stress " << label << ": code " << static_cast<int>(result.code)
                  << " path length " << result.path.size() << " raw " << raw << '\n';
        std::exit(EXIT_FAILURE);
    }
    const auto& s = result.statistics;
    require(s.inputBytes == input.size(), "stress input bytes");
    require(s.sourceBytes <= input.size() && s.decodedBytes <= input.size(), "stress byte bounds");
    require(s.nodeCount <= input.size() + 1 && s.framePeak <= input.size() + 1, "stress logical bounds");
    require(s.nodeCapacity <= input.size() + 1 && s.frameCapacity <= input.size() + 1, "stress capacity bounds");
    require(s.resourceQueueCount <= 4096, "stress queue bound");
    if (code == OwnJsonReadCode::Parsed && (label == "cap-string" || label == "cap-exponent")) {
        require(result.document->valueBytes(0).has_value(), "cap scalar span exists");
        if (label == "cap-exponent") require(result.document->valueBytes(0) == input, "cap exponent exact");
        else require(result.document->valueBytes(0)->size() == input.size() - 2, "cap string exact length");
    }
    std::cout << "STRESS " << label << " input=" << s.inputBytes << " source=" << s.sourceBytes
              << " decoded=" << s.decodedBytes << " nodes=" << s.nodeCount << '/' << s.nodeCapacity
              << " frames=" << s.framePeak << '/' << s.frameCapacity
              << " queue=" << s.resourceQueueCount << " nodeSize=" << s.nodeSizeBytes
              << " frameSize=" << s.frameSizeBytes << '\n';
}

std::string readText(const fs::path& path) {
    std::ifstream stream{path, std::ios::binary};
    require(static_cast<bool>(stream), "fixture opened");
    return {std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}

void checkSameDocument(const OwnJsonDocument& left, const OwnJsonDocument& right) {
    require(left.source() == right.source(), "fixture source bytes equal");
    require(left.nodes().size() == right.nodes().size(), "fixture node counts equal");
    for (std::size_t i = 0; i < left.nodes().size(); ++i) {
        const auto a = left.nodes()[i];
        const auto b = right.nodes()[i];
        require(a.kind == b.kind && a.boolean == b.boolean && a.hasKey == b.hasKey,
            "fixture scalar kinds equal");
        require(a.firstChild == b.firstChild && a.nextSibling == b.nextSibling
            && a.childCount == b.childCount, "fixture order equal");
        require(left.valueBytes(static_cast<OwnJsonNodeId>(i))
            == right.valueBytes(static_cast<OwnJsonNodeId>(i)), "fixture value bytes equal");
        require(left.memberName(static_cast<OwnJsonNodeId>(i))
            == right.memberName(static_cast<OwnJsonNodeId>(i)), "fixture member bytes equal");
    }
}

void scalarAndSyntax() {
    const std::string sample = R"({"":null,"b":true,"c":false,"d":-0.00e+12,"e":"\"\/\\\b\f\n\r\t\u0000\u00e9\ud83d\ude00","f":[],"g":{}})";
    const auto parsed = readOwnJson(sample);
    require(static_cast<bool>(parsed), "all kinds parsed");
    checkArena(*parsed.document, parsed.statistics);
    const auto& d = *parsed.document;
    require(d.nodes().size() == 8, "all-kind node count");
    require(d.node(0)->kind == OwnJsonKind::Object && d.node(0)->childCount == 7, "all-kind object");
    require(d.memberName(1) == std::string_view{}, "empty key distinct from absent");
    require(d.node(1)->kind == OwnJsonKind::Null && !d.valueBytes(1), "null child");
    require(d.node(2)->kind == OwnJsonKind::Boolean && d.node(2)->boolean, "true child");
    require(d.node(3)->kind == OwnJsonKind::Boolean && !d.node(3)->boolean, "false child");
    require(d.valueBytes(4) == "-0.00e+12", "exact signed number lexeme");
    const std::string decoded = std::string{"\"/\\\b\f\n\r\t", 8} + std::string(1, '\0')
        + "\xC3\xA9\xF0\x9F\x98\x80";
    require(d.valueBytes(5) == decoded, "all escapes and surrogate pair decoded");
    require(d.node(6)->kind == OwnJsonKind::Array && d.node(6)->childCount == 0, "empty array");
    require(d.node(7)->kind == OwnJsonKind::Object && d.node(7)->childCount == 0, "empty object");
    require(!d.valueBytes(2) && !d.memberName(0), "absent span checks");

    for (const std::string input : {"0e309", "1e-999999999999999999999999", "-0.00e+00012"}) {
        auto result = readOwnJson(input);
        require(static_cast<bool>(result), "lexical exponent accepted");
        require(result.document->valueBytes(0) == input, "lexical exponent exact");
    }
    for (const std::string input : {"\"A\"", "\"\xC3\xA9\"", "\"\xE2\x82\xAC\"",
         "\"\xF0\x9F\x98\x80\"", "\"\u0061\""}) {
        auto result = readOwnJson(input);
        require(static_cast<bool>(result), "valid scalar string");
        checkArena(*result.document, result.statistics);
    }
    auto rawKey = readOwnJson("{\"\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80\":\"ok\"}");
    require(static_cast<bool>(rawKey), "raw UTF8 scalar key parsed");
    require(rawKey.document->memberName(1) == "\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80", "raw UTF8 key exact");
    const std::string nulKey = "{\"a\\u0000b\":1,\"a\\u0000b\":2}";
    expect(nulKey, OwnJsonReadCode::InvalidJson, std::string{"/a\0b", 4});
    expect("{\"\":0,\"\":1}", OwnJsonReadCode::InvalidJson, "/");

    const std::vector<std::string> invalid = {
        "", " ", "true false", "nullx", "tru", "False", "NaN", "Infinity", "+1", "01", "-01", "-", "1.", "1e", "1E+", ".1",
        "[", "[1", "[1,", "[1,]", "[1 2]", "[1:2]", "{", "{\"a\"", "{\"a\":", "{\"a\":1", "{\"a\":1,", "{\"a\":1,}",
        "{a:1}", "{\"a\" 1}", "{\"a\":1 \"b\":2}", "{\"a\":1:2}", "{}{}", "/*x*/0", "\xEF\xBB\xBF{}", "\v0",
        "\"raw\nline\"", "\"raw\t-tab\"", "\"raw\x1F-control\"",
        "\"\\x\"", "\"\\u123\"", "\"\\u12x4\"", "\"\\uD800\"", "\"\\uDC00\"", "\"\\uD800\\u0041\"",
        "\"\xC0\xAF\"", "\"\xE2\x82\"", "\"\xED\xA0\x80\"", "\"\xED\xB0\x80\"", "\"\xF4\x90\x80\x80\"", "\"\xFF\""
    };
    for (const auto& input : invalid) expect(input, OwnJsonReadCode::InvalidJson, "/");
    for (const auto& raw : {std::string{"\xC0\xAF", 2}, std::string{"\xE2\x82", 2},
        std::string{"\xED\xA0\x80", 3}, std::string{"\xED\xB0\x80", 3},
        std::string{"\xF4\x90\x80\x80", 4}, std::string{"\xFF", 1}}) {
        expect("{\"" + raw + "\":0}", OwnJsonReadCode::InvalidJson, "/");
    }
    expect(std::string{"[0\0]", 4}, OwnJsonReadCode::InvalidJson, "/");
    expect(" \t\r\n[true,false,null]\n", OwnJsonReadCode::Parsed, "");
}

void resourcesAndStress() {
    expect("{\"x\":{\"a\":1,\"a\":2},\"x\":3}", OwnJsonReadCode::InvalidJson, "/x");
    expect("{\"x\":{\"a\":1,\"a\":2},\"y\":3}", OwnJsonReadCode::InvalidJson, "/x/a");
    expect("{\"x\":{\"a\":1,\"a\":2}} {", OwnJsonReadCode::InvalidJson, "/");
    expect(nestedArrays(33) + "x", OwnJsonReadCode::InvalidJson, "/");
    std::string fullQueue = "[";
    for (int i = 0; i < 4093; ++i) fullQueue += i == 0 ? "null" : ",null";
    expect(fullQueue + ",{\"k\":1,\"k\":2}]", OwnJsonReadCode::InvalidJson, "/4093/k");
    expect(fullQueue + ",{\"k\":1,\"z\":2}]", OwnJsonReadCode::ResourceLimit, "/4093/z");
    const auto atCap = std::size_t{1'048'576};
    stress("cap-whitespace", "0" + std::string(atCap - 1, ' '), OwnJsonReadCode::Parsed, "");
    stress("cap-broad", "[" + std::string(atCap - 2, '0') + "]", OwnJsonReadCode::InvalidJson, "/");
    std::string broad = "[";
    for (std::size_t i = 0; i < 524287; ++i) {
        if (i != 0) broad.push_back(',');
        broad.push_back('0');
    }
    broad += "] ";
    require(broad.size() == atCap, "broad cap size");
    stress("cap-broad-values", broad, OwnJsonReadCode::ResourceLimit, "/4095");
    auto brokenBroad = broad;
    brokenBroad[brokenBroad.size() - 2] = '{';
    stress("cap-broad-malformed", brokenBroad, OwnJsonReadCode::InvalidJson, "/");
    auto deep = nestedArrays(524287) + " ";
    require(deep.size() == atCap, "deep cap size");
    stress("cap-deep", deep, OwnJsonReadCode::ResourceLimit, repeatPath(32));
    deep[deep.size() - 2] = '{';
    stress("cap-deep-malformed", deep, OwnJsonReadCode::InvalidJson, "/");
    stress("cap-string", "\"" + std::string(atCap - 2, 'a') + "\"", OwnJsonReadCode::Parsed, "");
    stress("cap-exponent", "1e" + std::string(atCap - 2, '9'), OwnJsonReadCode::Parsed, "");
    auto badString = "\"" + std::string(atCap - 2, 'a') + "\"";
    badString.back() = '\\';
    stress("cap-string-malformed", badString, OwnJsonReadCode::InvalidJson, "/");
    auto badExponent = "1e" + std::string(atCap - 2, '9');
    badExponent.back() = 'x';
    stress("cap-exponent-malformed", badExponent, OwnJsonReadCode::InvalidJson, "/");
    stress("over-cap-invalid", std::string(atCap + 1, '{'), OwnJsonReadCode::ResourceLimit, "/");
}

void ownershipAndFixtures() {
    std::string source = "{\"k\":\"held\",\"n\":1e-999999999999999999}";
    auto parsed = readOwnJson(source);
    require(static_cast<bool>(parsed), "ownership parse");
    auto held = parsed.document;
    source.assign(source.size(), 'X');
    parsed.document.reset();
    require(held->source() == "{\"k\":\"held\",\"n\":1e-999999999999999999}", "owned source survives overwrite");
    require(held->memberName(1) == "k" && held->valueBytes(1) == "held", "owned decoded bytes survive");
    require(held->valueBytes(2) == "1e-999999999999999999", "owned number survives");

    std::string growing = "[";
    for (int i = 0; i < 1500; ++i) growing += i == 0 ? "{\"k\":\"v\"}" : ",{\"k\":\"v\"}";
    growing += "]";
    const auto many = readOwnJson(growing);
    require(static_cast<bool>(many), "arena growth parsed");
    checkArena(*many.document, many.statistics);
    require(many.document->node(0)->childCount == 1500, "arena growth child count");
    require(many.document->memberName(2) == "k" && many.document->valueBytes(2) == "v", "early index survives growth");
    require(many.document->memberName(static_cast<OwnJsonNodeId>(many.document->nodes().size() - 1)) == "k", "late index survives growth");

    const auto fixtures = fs::path{AVEMOTION_FIXTURE_DIR};
    for (const auto& name : {"telegram_sticker_basic.json", "repeater_content_group.json"}) {
        auto fixture = readText(fixtures / name);
        auto result = readOwnJson(fixture);
        require(static_cast<bool>(result), "committed fixture parsed");
        checkArena(*result.document, result.statistics);
        require(result.document->source() == fixture, "fixture source exact");
    }
    auto tgs = readText(fixtures / "tgs" / "repeater_content_group.tgs");
    const auto bytes = std::span<const std::byte>{reinterpret_cast<const std::byte*>(tgs.data()), tgs.size()};
    auto decoded = avemotion::formats::decodeTgs(bytes);
    require(static_cast<bool>(decoded), "committed TGS decoded");
    auto expected = readOwnJson(readText(fixtures / "repeater_content_group.json"));
    auto actual = readOwnJson(decoded.json);
    require(static_cast<bool>(expected) && static_cast<bool>(actual), "JSON and TGS payloads parsed");
    checkSameDocument(*expected.document, *actual.document);

    bool threadGood[2] = {true, true};
    auto task = [&](int slot) {
        for (int i = 0; i < 128; ++i) {
            auto text = std::string{"{\"v\":\"own\",\"n\":"} + std::to_string(i) + "}";
            auto result = readOwnJson(text);
            if (!result || result.document->nodes().size() != 3
                || result.document->node(0)->kind != OwnJsonKind::Object
                || result.document->node(0)->firstChild != 1
                || result.document->node(1)->kind != OwnJsonKind::String
                || result.document->node(1)->nextSibling != 2
                || result.document->node(2)->kind != OwnJsonKind::Number
                || result.document->node(2)->nextSibling != OwnJsonNoNode
                || result.document->memberName(1) != "v"
                || result.document->valueBytes(1) != "own"
                || result.document->memberName(2) != "n"
                || result.document->valueBytes(2) != std::to_string(i)
                || result.document->source() != text) threadGood[slot] = false;
        }
    };
    std::thread one{task, 0};
    std::thread two{task, 1};
    one.join(); two.join();
    require(threadGood[0] && threadGood[1], "two independent readers stable");
    task(0);
    require(threadGood[0], "serial reader repeats thread results");
}
}

int main() {
    const std::string input = R"({"a":[true,null,-0.00e+12],"s":"A\u00e9"})";
    auto parsed = readOwnJson(input);
    require(static_cast<bool>(parsed), "own JSON document parsed");
    require(parsed.path.empty(), "success path empty");
    require(parsed.document->node(0)->kind == OwnJsonKind::Object, "object root");
    require(parsed.document->source() == input, "exact owned source");

    expect("{\"a\":1,\"\\u0061\":2}", OwnJsonReadCode::InvalidJson, "/a");
    expect("{\"a/b~c\":1,\"a\\/b~c\":2}", OwnJsonReadCode::InvalidJson, "/a~1b~0c");
    expect("{\"a\":1,\"a\":2} {", OwnJsonReadCode::InvalidJson, "/");
    expect(nestedArrays(32), OwnJsonReadCode::Parsed, "");
    expect(nestedArrays(33), OwnJsonReadCode::ResourceLimit, repeatPath(32));
    expect(arrayOfNulls(4095), OwnJsonReadCode::Parsed, "");
    expect(arrayOfNulls(4096), OwnJsonReadCode::ResourceLimit, "/4095");
    expect(nestedArrays(33) + "{", OwnJsonReadCode::InvalidJson, "/");
    scalarAndSyntax();
    ownershipAndFixtures();
    resourcesAndStress();
    std::cout << "PASS own JSON reader checks=" << checks << '\n';
}
