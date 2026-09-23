#include "NativeEllipseAdmission.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using avemotion::runtime::detail::NativeEllipseAdmissionCode;
using avemotion::runtime::detail::auditNativeEllipseInput;

void require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string{message});
}

std::string readFixture(std::string_view name) {
    const auto path = std::filesystem::path{AVEMOTION_FIXTURE_DIR} / name;
    std::ifstream input{path, std::ios::binary};
    require(static_cast<bool>(input), "cannot open fixture");
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

std::string replaceOnce(std::string source, std::string_view from, std::string_view to) {
    const auto position = source.find(from);
    require(position != std::string::npos, "mutation source fragment is absent");
    source.replace(position, from.size(), to);
    return source;
}

std::string replaceBetween(std::string source, std::string_view first,
                           std::string_view last, std::string_view replacement) {
    const auto begin = source.find(first);
    require(begin != std::string::npos, "range mutation start is absent");
    const auto end = source.find(last, begin + first.size());
    require(end != std::string::npos, "range mutation end is absent");
    source.replace(begin, end + last.size() - begin, replacement);
    return source;
}

void expectAccepted(std::string_view json, std::string_view name) {
    const auto result = auditNativeEllipseInput(json);
    require(result.accepted(), std::string{name} + ": expected Accepted, got code "
        + std::to_string(static_cast<int>(result.code)) + " at " + result.path);
    require(result.path.empty(), std::string{name} + ": accepted path must be empty");
}

void expectRejected(std::string_view json, NativeEllipseAdmissionCode code,
                    std::string_view name) {
    const auto result = auditNativeEllipseInput(json);
    require(!result.accepted(), std::string{name} + ": unexpectedly accepted");
    require(result.code == code, std::string{name} + ": wrong code "
        + std::to_string(static_cast<int>(result.code)) + " at " + result.path);
    require(!result.path.empty() && result.path.front() == '/',
        std::string{name} + ": rejection needs an owned pointer path");
}

void testBaseline() {
    const auto baseline = readFixture("telegram_sticker_basic.json");
    const auto accepted = auditNativeEllipseInput(baseline);
    require(accepted.accepted(), "baseline should satisfy the specified input grammar");
    require(accepted.path.empty(), "accepted result has no error path");
    const auto withUnknown = replaceOnce(baseline, "\"ty\": \"el\"",
        "\"unhandled\": 1, \"ty\": \"el\"");
    const auto rejected = auditNativeEllipseInput(withUnknown);
    require(!rejected.accepted(), "unknown ellipse field must reject");
    require(rejected.code == NativeEllipseAdmissionCode::UnsupportedField,
        "unknown field classification");
    require(!rejected.path.empty() && rejected.path.front() == '/', "owned error path");
}

void testAcceptedVariants(const std::string& baseline) {
    expectAccepted(replaceOnce(baseline, "\"ip\": 0,\n      \"op\": 61",
        "\"ip\": 10,\n      \"op\": 20"), "short layer range");
    expectAccepted(replaceBetween(baseline, "\"p\": {\n                \"a\": 1,",
        "\n              },\n              \"nm\": \"Animated Ellipse\"",
        "\"p\": {\"a\": 0, \"k\": [32768, -32768]},\n              \"nm\": \"Animated Ellipse\""),
        "static boundary ellipse position");
    auto varied = replaceOnce(baseline, "[120, 120]", "[16384, 0.5]");
    varied = replaceOnce(varied, "[0.08, 0.72, 0.95, 1]", "[0, 1, 0.4, 1]");
    varied = replaceOnce(varied, "[256, 256, 0]", "[-32768, 32768, 0]");
    varied = replaceOnce(varied, "Moving Circle", "Different layer");
    varied = replaceOnce(varied, "\"fr\": 60", "\"fr\": 240");
    varied = replaceOnce(varied, "\"w\": 512", "\"w\": 8192");
    varied = replaceOnce(varied, "\"h\": 512", "\"h\": 1");
    varied = replaceOnce(varied, "\"ind\": 1", "\"ind\": 2147483647");
    expectAccepted(varied, "valid maximums and variations");
    expectAccepted(replaceOnce(baseline, "\"v\": \"5.7.4\",\n  \"fr\": 60,",
        "\"fr\": 60,\n  \"v\": \"5.7.4\","), "reordered root keys");
    auto noNames = replaceOnce(baseline, "  \"v\": \"5.7.4\",\n", "");
    noNames = replaceOnce(noNames, "  \"nm\": \"AveMotion Telegram sticker profile fixture\",\n", "");
    noNames = replaceOnce(noNames, "      \"nm\": \"Moving Circle\",\n", "");
    noNames = replaceOnce(noNames, "          \"nm\": \"Circle Group\",\n", "");
    noNames = replaceOnce(noNames, ",\n              \"nm\": \"Animated Ellipse\"", "");
    noNames = replaceOnce(noNames, ",\n              \"nm\": \"Fill\"", "");
    noNames = replaceOnce(noNames, ",\n              \"nm\": \"Transform\"", "");
    expectAccepted(noNames, "optional names and version omitted");
    expectAccepted(replaceOnce(baseline, "Moving Circle", "Moving\\u0020Circle \\u2603"),
        "escaped inert UTF-8 name");
    expectAccepted(replaceOnce(baseline, "\"fr\": 60", "\"fr\": 59.5"),
        "fractional frame rate is a finite number");
    auto decimals = replaceOnce(baseline, "\"op\": 61,\n  \"w\"", "\"op\": 61.0,\n  \"w\"");
    decimals = replaceOnce(decimals, "\"ind\": 1", "\"ind\": 1e0");
    decimals = replaceOnce(decimals, "{\"t\": 60, \"s\": [76, 0]}",
        "{\"t\": 60.0, \"s\": [76, 0]}");
    expectAccepted(decimals, "integral decimal JSON numbers");
    expectAccepted(std::string{" \r\n\t"} + baseline + " \n\t", "JSON whitespace");
    auto maximumTimeline = replaceOnce(baseline, "\"op\": 61,\n  \"w\"",
        "\"op\": 10000,\n  \"w\"");
    maximumTimeline = replaceOnce(maximumTimeline, "\"op\": 61,\n      \"st\"",
        "\"op\": 10000,\n      \"st\"");
    maximumTimeline = replaceOnce(maximumTimeline, "{\"t\": 60, \"s\": [76, 0]}",
        "{\"t\": 9999, \"s\": [76, 0]}");
    expectAccepted(maximumTimeline, "maximum root timeline and matching final keyframe");
    expectAccepted(replaceOnce(baseline, "Moving Circle", std::string(256, 'N')),
        "256-byte inert name");
    require(baseline.size() < 1'048'576, "fixture must fit byte cap");
    expectAccepted(baseline + std::string(1'048'576 - baseline.size(), ' '),
        "exact maximum input byte count");
}

struct Mutation {
    std::string_view name;
    std::string_view from;
    std::string_view to;
    NativeEllipseAdmissionCode code;
};

void testGrammarRejections(const std::string& baseline) {
    using C = NativeEllipseAdmissionCode;
    const std::vector<Mutation> mutations{
        {"root unknown", "\"fr\": 60", "\"other\": 1, \"fr\": 60", C::UnsupportedField},
        {"layer unknown", "\"ind\": 1", "\"other\": 1, \"ind\": 1", C::UnsupportedField},
        {"layer transform unknown", "\"ks\": {", "\"ks\": {\"other\": 1,", C::UnsupportedField},
        {"group unknown", "\"ty\": \"gr\"", "\"other\": 1, \"ty\": \"gr\"", C::UnsupportedField},
        {"ellipse unknown", "\"ty\": \"el\"", "\"other\": 1, \"ty\": \"el\"", C::UnsupportedField},
        {"fill unknown", "\"ty\": \"fl\"", "\"other\": 1, \"ty\": \"fl\"", C::UnsupportedField},
        {"group transform unknown", "\"ty\": \"tr\"", "\"other\": 1, \"ty\": \"tr\"", C::UnsupportedField},
        {"static property unknown", "\"s\": {\"a\": 0, \"k\": [120, 120]}",
            "\"s\": {\"extra\": 0, \"a\": 0, \"k\": [120, 120]}", C::UnsupportedField},
        {"animated property unknown", "\"a\": 1,\n                \"k\": [",
            "\"extra\": 0, \"a\": 1,\n                \"k\": [", C::UnsupportedField},
        {"first keyframe unknown", "\"i\": {\"x\": 0.667", "\"extra\": 0, \"i\": {\"x\": 0.667", C::UnsupportedField},
        {"final keyframe unknown", "{\"t\": 60, \"s\": [76, 0]}",
            "{\"extra\": 0, \"t\": 60, \"s\": [76, 0]}", C::UnsupportedField},
        {"easing unknown", "\"x\": 0.667", "\"extra\": 0, \"x\": 0.667", C::UnsupportedField},
        {"root required", "\"fr\": 60,", "", C::UnsupportedStructure},
        {"layer required", "\"ind\": 1,", "", C::UnsupportedStructure},
        {"layer transform required", "\"ks\": {\n        \"o\": {\"a\": 0, \"k\": 100},",
            "\"ks\": {", C::UnsupportedStructure},
        {"group required", "\"ty\": \"gr\",", "", C::UnsupportedStructure},
        {"ellipse required", "\"d\": 1,", "", C::UnsupportedStructure},
        {"fill required", "\"c\": {\"a\": 0, \"k\": [0.08, 0.72, 0.95, 1]},", "", C::UnsupportedStructure},
        {"group transform required", "\"sk\": {\"a\": 0, \"k\": 0},", "", C::UnsupportedStructure},
        {"static required", "\"s\": {\"a\": 0, \"k\": [120, 120]}",
            "\"s\": {\"a\": 0}", C::UnsupportedStructure},
        {"animated required", "\"a\": 1,\n                \"k\": [",
            "\"k\": [", C::UnsupportedStructure},
        {"first keyframe required", ",\n                    \"e\": [76, 0]", "", C::UnsupportedStructure},
        {"final keyframe required", "{\"t\": 60, \"s\": [76, 0]}", "{\"t\": 60}", C::UnsupportedStructure},
        {"easing required", "\"i\": {\"x\": 0.667, \"y\": 1}",
            "\"i\": {\"x\": 0.667}", C::UnsupportedStructure},
        {"fr type", "\"fr\": 60", "\"fr\": true", C::InvalidType},
        {"fr zero", "\"fr\": 60", "\"fr\": 0", C::UnsupportedValue},
        {"fr high", "\"fr\": 60", "\"fr\": 241", C::UnsupportedValue},
        {"width fraction", "\"w\": 512", "\"w\": 512.1", C::UnsupportedValue},
        {"width low", "\"w\": 512", "\"w\": 0", C::UnsupportedValue},
        {"height high", "\"h\": 512", "\"h\": 8193", C::UnsupportedValue},
        {"root ip", "\"ip\": 0,\n  \"op\": 61", "\"ip\": 1,\n  \"op\": 61", C::UnsupportedValue},
        {"root op low", "\"op\": 61,\n  \"w\"", "\"op\": 1,\n  \"w\"", C::UnsupportedValue},
        {"root op high", "\"op\": 61,\n  \"w\"", "\"op\": 10001,\n  \"w\"", C::UnsupportedValue},
        {"root ddd", "\"ddd\": 0,\n  \"assets\"", "\"ddd\": 1,\n  \"assets\"", C::UnsupportedValue},
        {"assets nonempty", "\"assets\": []", "\"assets\": [1]", C::UnsupportedStructure},
        {"assets type", "\"assets\": []", "\"assets\": {}", C::InvalidType},
        {"markers nonempty", "\"markers\": []", "\"markers\": [1]", C::UnsupportedStructure},
        {"layer index", "\"ind\": 1", "\"ind\": 0", C::UnsupportedValue},
        {"layer index high", "\"ind\": 1", "\"ind\": 2147483648", C::UnsupportedValue},
        {"layer type", "\"ty\": 4", "\"ty\": 5", C::UnsupportedValue},
        {"layer sr", "\"sr\": 1", "\"sr\": 2", C::UnsupportedValue},
        {"layer ao", "\"ao\": 0", "\"ao\": 1", C::UnsupportedValue},
        {"layer ip negative", "\"ip\": 0,\n      \"op\": 61", "\"ip\": -1,\n      \"op\": 61", C::UnsupportedValue},
        {"layer zero range", "\"ip\": 0,\n      \"op\": 61", "\"ip\": 61,\n      \"op\": 61", C::UnsupportedValue},
        {"layer beyond root", "\"ip\": 0,\n      \"op\": 61", "\"ip\": 0,\n      \"op\": 62", C::UnsupportedValue},
        {"layer st", "\"st\": 0", "\"st\": 1", C::UnsupportedValue},
        {"layer bm", "\"bm\": 0", "\"bm\": 1", C::UnsupportedValue},
        {"ellipse direction", "\"d\": 1", "\"d\": 2", C::UnsupportedValue},
        {"ellipse size zero", "[120, 120]", "[0, 120]", C::UnsupportedValue},
        {"ellipse size high", "[120, 120]", "[16385, 120]", C::UnsupportedValue},
        {"ellipse size arity", "[120, 120]", "[120, 120, 1]", C::UnsupportedStructure},
        {"ellipse size type", "[120, 120]", "[120, null]", C::InvalidType},
        {"ellipse size object", "[120, 120]", "{\"x\":120,\"y\":120}", C::InvalidType},
        {"ellipse size animated", "\"s\": {\"a\": 0, \"k\": [120, 120]}",
            "\"s\": {\"a\": 1, \"k\": [120, 120]}", C::UnsupportedValue},
        {"ellipse size k type", "[120, 120]", "42", C::InvalidType},
        {"position high", "[-76, 0]", "[-32769, 0]", C::UnsupportedValue},
        {"position short", "[-76, 0]", "[-76]", C::UnsupportedStructure},
        {"position element type", "[-76, 0]", "[-76, \"0\"]", C::InvalidType},
        {"position expression", "\"p\": {\n                \"a\": 1,",
            "\"p\": {\"x\": \"time\",\n                \"a\": 1,", C::UnsupportedField},
        {"first time", "\"t\": 0,", "\"t\": 1,", C::UnsupportedValue},
        {"final time", "{\"t\": 60, \"s\": [76, 0]}",
            "{\"t\": 59, \"s\": [76, 0]}", C::UnsupportedValue},
        {"final continuity", "{\"t\": 60, \"s\": [76, 0]}",
            "{\"t\": 60, \"s\": [75, 0]}", C::UnsupportedValue},
        {"first hold", "\"t\": 0,", "\"h\": 1, \"t\": 0,", C::UnsupportedField},
        {"first spatial tangent", "\"t\": 0,", "\"to\": [0, 0], \"t\": 0,", C::UnsupportedField},
        {"easing array", "\"x\": 0.667", "\"x\": [0.667]", C::InvalidType},
        {"easing high", "\"x\": 0.667", "\"x\": 1.1", C::UnsupportedValue},
        {"extra keyframe", "{\"t\": 60, \"s\": [76, 0]}",
            "{\"t\": 60, \"s\": [76, 0]}, {}", C::UnsupportedStructure},
        {"color red low", "[0.08, 0.72, 0.95, 1]", "[-0.01, 0.72, 0.95, 1]", C::UnsupportedValue},
        {"color alpha", "[0.08, 0.72, 0.95, 1]", "[0.08, 0.72, 0.95, 0.9]", C::UnsupportedValue},
        {"color arity", "[0.08, 0.72, 0.95, 1]", "[0.08, 0.72, 0.95]", C::UnsupportedStructure},
        {"color bool", "[0.08, 0.72, 0.95, 1]", "[0.08, true, 0.95, 1]", C::InvalidType},
        {"color blue high", "[0.08, 0.72, 0.95, 1]", "[0.08, 0.72, 1.1, 1]", C::UnsupportedValue},
        {"fill opacity", "\"o\": {\"a\": 0, \"k\": 100}",
            "\"o\": {\"a\": 0, \"k\": 99}", C::UnsupportedValue},
        {"fill color animated", "\"c\": {\"a\": 0, \"k\": [0.08, 0.72, 0.95, 1]}",
            "\"c\": {\"a\": 1, \"k\": [0.08, 0.72, 0.95, 1]}", C::UnsupportedValue},
        {"fill rule", "\"r\": 1,\n              \"nm\": \"Fill\"",
            "\"r\": 2,\n              \"nm\": \"Fill\"", C::UnsupportedValue},
        {"layer translation z", "[256, 256, 0]", "[256, 256, 1]", C::UnsupportedValue},
        {"layer translation arity", "[256, 256, 0]", "[256, 256]", C::UnsupportedStructure},
        {"layer translation range", "[256, 256, 0]", "[32769, 256, 0]", C::UnsupportedValue},
        {"layer opacity", "\"o\": {\"a\": 0, \"k\": 100},\n        \"r\"",
            "\"o\": {\"a\": 0, \"k\": 99},\n        \"r\"", C::UnsupportedValue},
        {"layer anchor", "[0, 0, 0]", "[1, 0, 0]", C::UnsupportedValue},
        {"layer scale", "[100, 100, 100]", "[99, 100, 100]", C::UnsupportedValue},
        {"layer rotation", "\"r\": {\"a\": 0, \"k\": 0}",
            "\"r\": {\"a\": 0, \"k\": 1}", C::UnsupportedValue},
        {"group anchor", "\"a\": {\"a\": 0, \"k\": [0, 0]}",
            "\"a\": {\"a\": 0, \"k\": [0, 1]}", C::UnsupportedValue},
        {"group translation", "\"p\": {\"a\": 0, \"k\": [0, 0]}",
            "\"p\": {\"a\": 0, \"k\": [1, 0]}", C::UnsupportedValue},
        {"group translation arity", "\"p\": {\"a\": 0, \"k\": [0, 0]}",
            "\"p\": {\"a\": 0, \"k\": [0, 0, 0]}", C::UnsupportedStructure},
        {"group scale", "\"s\": {\"a\": 0, \"k\": [100, 100]}",
            "\"s\": {\"a\": 0, \"k\": [99, 100]}", C::UnsupportedValue},
        {"group skew", "\"sk\": {\"a\": 0, \"k\": 0}",
            "\"sk\": {\"a\": 0, \"k\": 1}", C::UnsupportedValue},
        {"group skew axis", "\"sa\": {\"a\": 0, \"k\": 0}",
            "\"sa\": {\"a\": 0, \"k\": 1}", C::UnsupportedValue},
        {"group opacity", "\"o\": {\"a\": 0, \"k\": 100},\n              \"sk\"",
            "\"o\": {\"a\": 0, \"k\": 99},\n              \"sk\"", C::UnsupportedValue},
        {"group transform animated", "\"sk\": {\"a\": 0, \"k\": 0}",
            "\"sk\": {\"a\": 1, \"k\": 0}", C::UnsupportedValue},
        {"unknown operator", "\"ty\": \"el\"", "\"ty\": \"sr\"", C::UnsupportedValue},
        {"hidden operator", "\"ty\": \"el\"", "\"ty\": \"el\", \"hd\": true", C::UnsupportedField},
        {"merge operator", "\"ty\": \"el\"", "\"ty\": \"mm\"", C::UnsupportedValue},
        {"extra layer", "\"layers\": [", "\"layers\": [{},", C::UnsupportedStructure},
        {"extra group", "\"shapes\": [", "\"shapes\": [{},", C::UnsupportedStructure},
        {"extra fill", "\"it\": [", "\"it\": [{\"ty\":\"fl\"},", C::UnsupportedStructure},
        {"extra path", "\"it\": [", "\"it\": [{\"ty\":\"sh\"},", C::UnsupportedStructure},
    };
    for (const auto& mutation : mutations) {
        expectRejected(replaceOnce(baseline, mutation.from, mutation.to),
            mutation.code, mutation.name);
    }
    expectRejected(replaceOnce(baseline, "Moving Circle", std::string(257, 'N')),
        C::UnsupportedValue, "257-byte inert name");
    expectRejected(replaceBetween(baseline, "\"p\": {\n                \"a\": 1,",
        "\n              },\n              \"nm\": \"Animated Ellipse\"",
        "\"p\": {\"a\": 1, \"k\": 1},\n              \"nm\": \"Animated Ellipse\""),
        C::InvalidType, "animated position k type");
}

void testDocumentRejections(const std::string& baseline) {
    using C = NativeEllipseAdmissionCode;
    expectRejected("", C::InvalidJson, "empty");
    expectRejected("   ", C::InvalidJson, "whitespace only");
    expectRejected(baseline.substr(0, baseline.size() - 3), C::InvalidJson, "truncated");
    expectRejected(baseline + "{}", C::InvalidJson, "trailing object");
    expectRejected(baseline + "/* comment */", C::InvalidJson, "comment");
    expectRejected(replaceOnce(baseline, "\"markers\": []", "\"markers\": [],"),
        C::InvalidJson, "trailing comma");
    auto nulSuffix = baseline;
    nulSuffix.push_back('\0');
    nulSuffix += "ignored";
    expectRejected(nulSuffix, C::InvalidJson, "literal NUL suffix");
    auto invalidUtf8 = baseline;
    invalidUtf8.insert(invalidUtf8.find("Moving Circle"), 1, static_cast<char>(0xFF));
    expectRejected(invalidUtf8, C::InvalidJson, "invalid UTF-8");
    expectRejected(replaceOnce(baseline, "Moving Circle", "Moving\\u0000Circle"),
        C::UnsupportedValue, "decoded NUL in name");
    expectRejected(replaceOnce(baseline, "\"fr\": 60", "\"fr\": 60, \"fr\": 60"),
        C::InvalidJson, "duplicate root key");
    expectRejected(replaceOnce(baseline, "\"ty\": \"el\"",
        "\"ty\": \"el\", \"\\u0074y\": \"el\""),
        C::InvalidJson, "escaped nested duplicate");
    expectRejected(replaceOnce(baseline, "\"fr\": 60", "\"fr\": null"),
        C::InvalidType, "null numeric");
    expectRejected(replaceOnce(baseline, "\"fr\": 60", "\"fr\": \"60\""),
        C::InvalidType, "string numeric");
    for (const auto token : {"NaN", "Infinity", "1e9999"}) {
        expectRejected(replaceOnce(baseline, "\"fr\": 60", std::string{"\"fr\": "} + token),
            C::InvalidJson, token);
    }
    expectRejected(std::string(1'048'577, ' '), C::ResourceLimit, "byte limit first");
    const auto nested = std::string(33, '[') + "0" + std::string(33, ']');
    expectRejected(replaceOnce(baseline, "\"markers\": []",
        std::string{"\"markers\": [], \"deep\": "} + nested),
        C::ResourceLimit, "33 nested containers");
    std::string many = "\"markers\": [], \"many\": [";
    for (int index = 0; index < 4096; ++index) {
        if (index) many += ',';
        many += "null";
    }
    many += ']';
    expectRejected(replaceOnce(baseline, "\"markers\": []", many),
        C::ResourceLimit, "value count over 4096");
    const auto owned = [&baseline] {
        auto temporary = replaceOnce(baseline, "\"ty\": \"el\"",
            "\"a/b~c\": 1, \"ty\": \"el\"");
        return auditNativeEllipseInput(temporary);
    }();
    require(owned.code == C::UnsupportedField && owned.path.find("a~1b~0c") != std::string::npos,
        "owned escaped JSON pointer after source destruction");
    expectAccepted(baseline, "subsequent audit independent");
}

} // namespace

int main() {
    try {
        testBaseline();
        const auto baseline = readFixture("telegram_sticker_basic.json");
        testAcceptedVariants(baseline);
        testGrammarRejections(baseline);
        testDocumentRejections(baseline);
        std::cout << "native ellipse admission tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
