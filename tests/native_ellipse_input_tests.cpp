#include "NativeEllipseInput.hpp"
#include "avemotion/formats/Tgs.hpp"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <variant>

namespace {
using namespace avemotion::runtime::detail;
static_assert(std::is_same_v<decltype(NativeEllipseInputResult{}.input),
    std::shared_ptr<const NativeEllipseInput>>);

void require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string{message});
}

std::string readFixture(std::string_view name) {
    std::ifstream input{std::filesystem::path{AVEMOTION_FIXTURE_DIR} / name, std::ios::binary};
    require(static_cast<bool>(input), "cannot open fixture");
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

NativeEllipseDecimal decimal(bool negative, std::string digits,
                             bool negativePower = false, std::string power = "0") {
    return {negative, std::move(digits), {negativePower, std::move(power)}};
}

std::string replaceOnce(std::string source, std::string_view from, std::string_view to) {
    const auto position = source.find(from);
    require(position != std::string::npos, "mutation source fragment is absent");
    require(source.find(from, position + from.size()) == std::string::npos,
        "mutation source fragment is not unique");
    source.replace(position, from.size(), to);
    return source;
}

std::string replaceBetween(std::string source, std::string_view first,
                           std::string_view last, std::string_view replacement) {
    const auto begin = source.find(first);
    require(begin != std::string::npos && source.find(first, begin + first.size()) == std::string::npos,
        "range mutation start must be unique");
    const auto end = source.find(last, begin + first.size());
    require(end != std::string::npos && source.find(last, end + last.size()) == std::string::npos,
        "range mutation end must be unique");
    source.replace(begin, end + last.size() - begin, replacement);
    return source;
}

NativeEllipseInputResult accepted(std::string_view json, std::string_view label) {
    auto result = decodeNativeEllipseInput(json);
    require(static_cast<bool>(result) && result.admission.path.empty(), label);
    return result;
}

void rejected(std::string_view json, std::string_view label) {
    const auto old = auditNativeEllipseInput(json);
    const auto result = decodeNativeEllipseInput(json);
    require(!old.accepted() && !result && !result.input
        && result.admission.code == old.code && result.admission.path == old.path, label);
}

void testBaseline() {
    const auto result = decodeNativeEllipseInput(readFixture("telegram_sticker_basic.json"));
    require(result.admission.accepted(), "fixture remains admitted");
    require(result.input != nullptr, "accepted input publishes owned descriptor");
    require(static_cast<bool>(result) && result.admission.path.empty(), "accepted result");
    const auto& input = *result.input;
    require(input.width == 512 && input.height == 512, "dimensions");
    require(input.endFrame == 61 && input.layerId == 1, "timeline/id");
    require(input.frameRate == decimal(false, "6", false, "1"), "exact rate");
    require(input.layerInFrame == 0 && input.layerOutFrame == 61, "interval");
    require(input.layerTranslation == NativeEllipseVec2{decimal(false, "256"), decimal(false, "256")}, "translation");
    require(input.size == NativeEllipseVec2{decimal(false, "12", false, "1"), decimal(false, "12", false, "1")}, "size");
    const auto& motion = std::get<NativeEllipseAnimatedPosition>(input.position);
    require(motion.firstFrame == 0 && motion.lastFrame == 60, "keyframe times");
    require(motion.start == NativeEllipseVec2{decimal(true, "76"), decimal(false, "0")}, "start");
    require(motion.end == NativeEllipseVec2{decimal(false, "76"), decimal(false, "0")}, "end");
    require(motion.incoming == NativeEllipseVec2{decimal(false, "667", true, "3"), decimal(false, "1")}, "incoming");
    require(motion.outgoing == NativeEllipseVec2{decimal(false, "333", true, "3"), decimal(false, "0")}, "outgoing");
    require(input.fillColor == std::array<NativeEllipseDecimal, 4>{decimal(false, "8", true, "2"), decimal(false, "72", true, "2"), decimal(false, "95", true, "2"), decimal(false, "1")}, "fill");
    require(input.version == "5.7.4" && input.name == "AveMotion Telegram sticker profile fixture"
        && input.layerName == "Moving Circle" && input.groupName == "Circle Group"
        && input.ellipseName == "Animated Ellipse" && input.fillName == "Fill"
        && input.transformName == "Transform", "all names/version");
}

void testNumbers(const std::string& seed) {
    const auto baseline = accepted(seed, "baseline");
    const auto tiny = accepted(replaceOnce(seed, "\"fr\": 60", "\"fr\": 1e-9999"),
        "positive underflow remains admitted");
    require(tiny.input->frameRate == decimal(false, "1", true, "9999"),
        "positive underflow retained exactly");
    const auto tinySize = accepted(replaceOnce(seed, "[120, 120]", "[1e-9999, 120]"),
        "positive underflow size remains admitted");
    require(tinySize.input->size[0] == decimal(false, "1", true, "9999"),
        "tiny size retained exactly");
    const auto longExponent = accepted(replaceOnce(seed, "\"fr\": 60",
        "\"fr\": 1e-999999999999999999999999"), "long negative exponent admitted");
    require(longExponent.input->frameRate
        == decimal(false, "1", true, "999999999999999999999999"),
        "long exponent retained without expansion");
    const auto equivalent = accepted(replaceOnce(seed, "\"fr\": 60", "\"fr\": 6000e-2"),
        "equivalent decimal admitted");
    require(*equivalent.input == *baseline.input, "equivalent decimal descriptor");
    const auto negativeZero = accepted(replaceOnce(seed, "\"fr\": 60", "\"fr\": 60e-0"),
        "zero exponent admitted");
    require(*negativeZero.input == *baseline.input, "zero exponent canonicalized");
    const auto negativeZeroValue = accepted(replaceOnce(seed, "[-76, 0]", "[-76, -0]"),
        "negative zero value admitted");
    require(*negativeZeroValue.input == *baseline.input, "negative zero canonicalized");
    const auto integerSpelling = accepted(replaceOnce(seed,
        "\"op\": 61,\n  \"w\"", "\"op\": 6100000000000000000000000000e-26,\n  \"w\""),
        "structural equivalent integer admitted");
    require(integerSpelling.input->endFrame == 61
        && *integerSpelling.input == *baseline.input, "structural exact integer");
    rejected(replaceOnce(seed, "\"fr\": 60", "\"fr\": 240.00000000000001"),
        "near frame-rate bound classification/path");
    rejected(replaceOnce(seed, "\"e\": [76, 0]", "\"e\": [76.00000000000001, 0]"),
        "near continuity mismatch classification/path");
}

void testVariants(const std::string& seed) {
    auto variant = replaceOnce(seed, "\"ip\": 0,\n      \"op\": 61",
        "\"ip\": 10,\n      \"op\": 20");
    variant = replaceOnce(variant, "\"ind\": 1", "\"ind\": 2147483647");
    variant = replaceOnce(variant, "[256, 256, 0]", "[-32768, 32768, 0]");
    variant = replaceOnce(variant, "[120, 120]", "[16384, 0.5]");
    variant = replaceOnce(variant, "[0.08, 0.72, 0.95, 1]", "[0, 1, 0.4, 1]");
    const auto varied = accepted(variant, "nondefault variable fields admitted");
    require(varied.input->layerInFrame == 10 && varied.input->layerOutFrame == 20
        && varied.input->layerId == 2147483647, "active interval and ID retained");
    require(varied.input->layerTranslation == NativeEllipseVec2{decimal(true, "32768"), decimal(false, "32768")},
        "translation boundaries retained");
    require(varied.input->size == NativeEllipseVec2{decimal(false, "16384"), decimal(false, "5", true, "1")},
        "size boundaries retained");
    require(varied.input->fillColor == std::array<NativeEllipseDecimal, 4>{
        decimal(false, "0"), decimal(false, "1"), decimal(false, "4", true, "1"), decimal(false, "1")},
        "fill boundaries retained");

    const auto staticJson = replaceBetween(seed, "\"p\": {\n                \"a\": 1,",
        "\n              },\n              \"nm\": \"Animated Ellipse\"",
        "\"p\": {\"a\":0,\"k\":[-32768,32768]},\n              \"nm\": \"Animated Ellipse\"");
    const auto fixed = accepted(staticJson, "static position admitted");
    require(std::get<NativeEllipseStaticPosition>(fixed.input->position).value
        == NativeEllipseVec2{decimal(true, "32768"), decimal(false, "32768")},
        "static position retained");
}

void testNamesAndOwnership(const std::string& seed) {
    auto noNames = replaceOnce(seed, "  \"v\": \"5.7.4\",\n", "");
    noNames = replaceOnce(noNames, "  \"nm\": \"AveMotion Telegram sticker profile fixture\",\n", "");
    noNames = replaceOnce(noNames, "      \"nm\": \"Moving Circle\",\n", "");
    noNames = replaceOnce(noNames, "          \"nm\": \"Circle Group\",\n", "");
    noNames = replaceOnce(noNames, ",\n              \"nm\": \"Animated Ellipse\"", "");
    noNames = replaceOnce(noNames, ",\n              \"nm\": \"Fill\"", "");
    noNames = replaceOnce(noNames, ",\n              \"nm\": \"Transform\"", "");
    const auto absent = accepted(noNames, "missing optional names admitted");
    require(!absent.input->version && !absent.input->name && !absent.input->layerName
        && !absent.input->groupName && !absent.input->ellipseName
        && !absent.input->fillName && !absent.input->transformName,
        "all optional absences retained");
    auto emptyNames = replaceOnce(seed, "\"v\": \"5.7.4\"", "\"v\": \"\"");
    emptyNames = replaceOnce(emptyNames, "AveMotion Telegram sticker profile fixture", "");
    emptyNames = replaceOnce(emptyNames, "Moving Circle", "");
    emptyNames = replaceOnce(emptyNames, "Circle Group", "");
    emptyNames = replaceOnce(emptyNames, "Animated Ellipse", "");
    emptyNames = replaceOnce(emptyNames, "\"nm\": \"Fill\"", "\"nm\": \"\"");
    emptyNames = replaceOnce(emptyNames, "\"nm\": \"Transform\"", "\"nm\": \"\"");
    const auto empty = accepted(emptyNames, "empty names admitted");
    require(empty.input->version && empty.input->version->empty()
        && empty.input->name && empty.input->name->empty()
        && empty.input->layerName && empty.input->layerName->empty()
        && empty.input->groupName && empty.input->groupName->empty()
        && empty.input->ellipseName && empty.input->ellipseName->empty()
        && empty.input->fillName && empty.input->fillName->empty()
        && empty.input->transformName && empty.input->transformName->empty(),
        "present empty names distinguished from absence");
    const auto utf8 = accepted(replaceOnce(seed, "Moving Circle", "Moving\\u0020Circle \\u2603"),
        "escaped UTF-8 name admitted");
    require(utf8.input->layerName == "Moving Circle \xE2\x98\x83", "decoded UTF-8 retained");
    const auto reordered = accepted(replaceOnce(seed, "\"v\": \"5.7.4\",\n  \"fr\": 60,",
        "\"fr\": 60,\n  \"v\": \"5.7.4\","), "reordered keys admitted");
    require(*reordered.input == *accepted(seed, "baseline").input, "key order independent");
    std::shared_ptr<const NativeEllipseInput> retained;
    {
        auto buffer = seed;
        retained = accepted(buffer, "owned buffer admitted").input;
        buffer.assign(buffer.size(), 'x');
    }
    require(*retained == *accepted(seed, "baseline after buffer destruction").input,
        "source overwrite/destruction cannot change descriptor");
}

void testRejectionsAndTgs(const std::string& seed) {
    rejected(replaceOnce(seed, "\"fr\": 60", "\"other\": 1, \"fr\": 60"), "unknown field");
    rejected(replaceOnce(seed, "\"fr\": 60", "\"fr\": 60, \"fr\": 60"), "duplicate field");
    auto badUtf8 = seed;
    badUtf8.insert(badUtf8.find("Moving Circle"), 1, static_cast<char>(0xFF));
    rejected(badUtf8, "invalid UTF-8");
    rejected(std::string(1'048'577, ' '), "oversized input");
    const auto nested = std::string(33, '[') + "0" + std::string(33, ']');
    rejected(replaceOnce(seed, "\"markers\": []",
        std::string{"\"markers\": [], \"deep\": "} + nested), "deep input");
    std::string many = "\"markers\": [], \"many\": [";
    for (int index = 0; index < 4096; ++index) {
        if (index) many += ',';
        many += "null";
    }
    many += ']';
    rejected(replaceOnce(seed, "\"markers\": []", many), "node limit");
    const auto decodedTgs = avemotion::formats::decodeTgsFile(
        std::filesystem::path{AVEMOTION_TGS_DIR} / "telegram_sticker_basic.tgs");
    require(static_cast<bool>(decodedTgs), "owned TGS transport decoded");
    const auto tgsInput = accepted(decodedTgs.json, "TGS JSON admitted");
    require(*tgsInput.input == *accepted(seed, "JSON seed admitted").input,
        "TGS and JSON descriptors equal");
}

void testConcurrentIsolation(const std::string& seed) {
    const auto baseline = accepted(seed, "baseline before threads").input;
    std::array<std::shared_ptr<const NativeEllipseInput>, 2> outputs;
    std::array<std::exception_ptr, 2> errors;
    std::array<std::thread, 2> threads;
    for (std::size_t index = 0; index < threads.size(); ++index) {
        threads[index] = std::thread([&, index] {
            try {
                auto json = replaceOnce(seed, "Moving Circle", index == 0 ? "Thread A" : "Thread B");
                json = replaceOnce(json, "[256, 256, 0]", index == 0
                    ? "[11, 22, 0]" : "[33, 44, 0]");
                outputs[index] = accepted(json, "concurrent descriptor").input;
            } catch (...) { errors[index] = std::current_exception(); }
        });
    }
    for (auto& thread : threads) thread.join();
    for (const auto& error : errors) if (error) std::rethrow_exception(error);
    require(outputs[0]->layerName == "Thread A" && outputs[1]->layerName == "Thread B",
        "concurrent names isolated");
    require(outputs[0]->layerTranslation == NativeEllipseVec2{decimal(false, "11"), decimal(false, "22")}
        && outputs[1]->layerTranslation == NativeEllipseVec2{decimal(false, "33"), decimal(false, "44")},
        "concurrent translations isolated");
    require(*baseline == *accepted(seed, "baseline after threads").input,
        "prior descriptor unchanged after concurrent decodes");
}
} // namespace

int main() {
    try {
        testBaseline();
        const auto seed = readFixture("telegram_sticker_basic.json");
        testNumbers(seed);
        testVariants(seed);
        testNamesAndOwnership(seed);
        testRejectionsAndTgs(seed);
        testConcurrentIsolation(seed);
        std::cout << "native ellipse input tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
