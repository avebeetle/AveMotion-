#pragma once

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace avemotion::test {

struct NativeEllipseTestAsset final {
    std::string name;
    std::string json;
};

inline std::string replaceNativeOnce(std::string source, std::string_view from,
                                     std::string_view to) {
    const auto offset = source.find(from);
    if (offset == std::string::npos
        || source.find(from, offset + from.size()) != std::string::npos)
        throw std::runtime_error("native test replacement missing or ambiguous: "
                                 + std::string(from));
    source.replace(offset, from.size(), to);
    return source;
}

inline std::string replaceNativeEllipsePosition(const std::string& seed,
                                                std::string_view replacement) {
    const std::string_view first = "\"p\": {\n                \"a\": 1,";
    const std::string_view last = "\n              },\n              \"nm\": \"Animated Ellipse\"";
    const auto begin = seed.find(first);
    const auto end = seed.find(last, begin == std::string::npos ? 0 : begin + first.size());
    if (begin == std::string::npos || end == std::string::npos
        || seed.find(first, begin + first.size()) != std::string::npos
        || seed.find(last, end + last.size()) != std::string::npos)
        throw std::runtime_error("ellipse position replacement missing or ambiguous");
    std::string result = seed;
    result.replace(begin, end + last.size() - begin,
                   std::string(replacement) + ",\n              \"nm\": \"Animated Ellipse\"");
    return result;
}

inline std::vector<NativeEllipseTestAsset> nativeEllipseTestAssets(const std::string& seed) {
    auto staticPosition = [&](std::string_view value) {
        return replaceNativeEllipsePosition(seed, std::string("\"p\": {\"a\":0,\"k\":")
                                               + std::string(value) + "}");
    };
    auto endpoints = [&](std::string start, std::string end) {
        auto result = replaceNativeOnce(seed, "\"s\": [-76, 0]", "\"s\": " + start);
        result = replaceNativeOnce(std::move(result), "\"e\": [76, 0]", "\"e\": " + end);
        return replaceNativeOnce(std::move(result), "\"t\": 60, \"s\": [76, 0]",
                                 "\"t\": 60, \"s\": " + end);
    };
    std::vector<NativeEllipseTestAsset> cases;
    cases.push_back({"animated", seed});
    cases.push_back({"static-visible", staticPosition("[0,0]")});
    cases.push_back({"static-far", staticPosition("[-32768,32768]")});
    cases.push_back({"activity", replaceNativeOnce(seed,
        "\"ip\": 0,\n      \"op\": 61", "\"ip\": 10,\n      \"op\": 20")});
    cases.push_back({"fractional-rate", replaceNativeOnce(seed, "\"fr\": 60", "\"fr\": 59.94")});
    auto nonsquare = replaceNativeOnce(seed, "\"w\": 512", "\"w\": 640");
    cases.push_back({"nonsquare", replaceNativeOnce(std::move(nonsquare), "\"h\": 512", "\"h\": 360")});
    auto fractional = endpoints("[-75.5,0.25]", "[75.5,0.25]");
    cases.push_back({"fractional-coordinates", replaceNativeOnce(std::move(fractional),
        "[256, 256, 0]", "[255.25,256.75,0]")});
    cases.push_back({"color-boundaries", replaceNativeOnce(seed,
        "[0.08, 0.72, 0.95, 1]", "[0.5,0.25,0.75,1]")});
    auto linear = replaceNativeOnce(seed, "\"o\": {\"x\": 0.333, \"y\": 0}",
                                    "\"o\": {\"x\": 0, \"y\": 0}");
    cases.push_back({"linear-easing", replaceNativeOnce(std::move(linear),
        "\"i\": {\"x\": 0.667, \"y\": 1}", "\"i\": {\"x\": 1, \"y\": 1}")});
    auto nonlinear = replaceNativeOnce(seed, "\"o\": {\"x\": 0.333, \"y\": 0}",
                                       "\"o\": {\"x\": 0.125, \"y\": 0.875}");
    cases.push_back({"nonlinear-easing", replaceNativeOnce(std::move(nonlinear),
        "\"i\": {\"x\": 0.667, \"y\": 1}", "\"i\": {\"x\": 0.875, \"y\": 0.125}")});
    auto names = replaceNativeOnce(seed, "\"fr\": 60", "\"fr\": 6e1");
    names = replaceNativeOnce(std::move(names), "\"op\": 61,\n  \"w\"", "\"op\": 6.1e1,\n  \"w\"");
    names = replaceNativeOnce(std::move(names), "\"nm\": \"Moving Circle\"", "\"nm\": \"\\u004doving Circle\"");
    names = replaceNativeOnce(std::move(names), "\"nm\": \"Circle Group\"", "\"nm\": \"\"");
    names = replaceNativeOnce(std::move(names), "\"nm\": \"Animated Ellipse\"", "\"nm\": \"\"");
    names = replaceNativeOnce(std::move(names), "\"nm\": \"Fill\"", "\"nm\": \"\"");
    cases.push_back({"equivalent-names", std::move(names)});
    cases.push_back({"tiny-collapsing", replaceNativeOnce(seed, "[120, 120]", "[1e-20,1e-20]")});
    cases.push_back({"edge-crossing", endpoints("[-400,0]", "[400,0]")});
    cases.push_back({"tiny-surviving", replaceNativeOnce(staticPosition("[0,0]"),
        "[120, 120]", "[0.0001,0.0001]")});
    cases.push_back({"boundary-translation", replaceNativeOnce(staticPosition("[0,0]"),
        "[256, 256, 0]", "[0.25,256,0]")});
    if (cases.size() != 15) throw std::runtime_error("native matrix case count");
    return cases;
}

} // namespace avemotion::test
