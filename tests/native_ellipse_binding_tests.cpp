#include "NativeEllipseBinding.hpp"
#include "avemotion/runtime/Runtime.hpp"

#include <cstdlib>
#include <array>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <functional>

using namespace avemotion::runtime::detail;
namespace model = avemotion::model;

namespace {
void require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string{message});
}

std::string readFixture(std::string_view name) {
    std::ifstream input{std::filesystem::path{AVEMOTION_FIXTURE_DIR} / name, std::ios::binary};
    require(static_cast<bool>(input), "cannot open fixture");
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

std::string replaceOnce(std::string source, std::string_view from, std::string_view to) {
    const auto offset = source.find(from);
    require(offset != std::string::npos, std::string{"replacement text missing: "} + std::string{from});
    require(source.find(from, offset + from.size()) == std::string::npos, "replacement not unique");
    source.replace(offset, from.size(), to);
    return source;
}

std::string replaceBetween(std::string source, std::string_view first,
                           std::string_view last, std::string_view replacement) {
    const auto begin = source.find(first);
    const auto end = source.find(last, begin + first.size());
    require(begin != std::string::npos && end != std::string::npos, "replacement range missing");
    source.replace(begin, end + last.size() - begin, replacement);
    return source;
}

struct Prepared {
    std::shared_ptr<const NativeEllipseInput> input;
    std::shared_ptr<const model::MotionAssetModel> model;
    NativeEllipseModelBinding binding;
};

Prepared prepare(std::string_view json) {
    auto input = decodeNativeEllipseInput(json);
    avemotion::runtime::Runtime runtime;
    auto asset = runtime.loadLottieJson(json);
    require(input && asset, "variant prerequisites");
    auto model = asset.asset->prepareModel();
    require(static_cast<bool>(model), "variant model");
    auto result = bindNativeEllipseModel(*input.input, *model.model);
    require(static_cast<bool>(result), "authored variant bound");
    return {input.input, model.model, *result.binding};
}

void rejectChanged(const Prepared& baseline,
                   const std::function<void(model::MotionAssetModel&)>& change,
                   std::string_view label) {
    auto changed = *baseline.model;
    change(changed);
    const auto result = bindNativeEllipseModel(*baseline.input, changed);
    require(!result && !result.binding, label);
}

void testBaseline(const std::string& seed) {
    const auto prepared = prepare(seed);
    const auto& b = prepared.binding;
    const auto& m = *prepared.model;
    require(m.sourceNodes[b.layer.index()].parent == b.root
        && m.sourceNodes[b.group.index()].parent == b.layer
        && m.sourceNodes[b.ellipse.index()].parent == b.group
        && m.sourceNodes[b.fill.index()].parent == b.group, "semantic lineage");
    require(m.properties[b.position.index()].semantic == model::PropertySemantic::EllipsePosition
        && m.properties[b.position.index()].owner == b.ellipse
        && m.properties[b.position.index()].flags == model::PropertyFlagAnimated,
        "animated position resolved");
    const auto& track = m.tracks[m.properties[b.position.index()].track.index()];
    const auto& segment = m.segments[track.segments.first];
    require(track.firstFrame == 0 && track.endFrame == 60 && track.segments.count == 1,
        "one half-open track");
    require(segment.temporalControl1.x == static_cast<float>(0.333)
        && segment.temporalControl2.x == static_cast<float>(0.667)
        && segment.interpolation == model::SegmentInterpolation::CubicBezier,
        "outgoing/incoming controls");
    require(m.sourceNodes[b.layer.index()].inFrame == 0
        && m.sourceNodes[b.layer.index()].outFrame == 61, "baseline layer interval");
}

void testVariants(const std::string& seed) {
    auto activity = replaceOnce(seed, "\"ip\": 0,\n      \"op\": 61",
        "\"ip\": 10,\n      \"op\": 20");
    activity = replaceOnce(activity, "\"ind\": 1", "\"ind\": 2147483647");
    activity = replaceOnce(activity, "[256, 256, 0]", "[-32768, 32768, 0]");
    activity = replaceOnce(activity, "[120, 120]", "[16384, 0.5]");
    activity = replaceOnce(activity, "[0.08, 0.72, 0.95, 1]", "[0, 1, 0.4, 1]");
    const auto active = prepare(activity);
    require(active.model->sourceNodes[active.binding.layer.index()].inFrame == 10
        && active.model->sourceNodes[active.binding.layer.index()].outFrame == 20,
        "activity interval preserved");
    require(active.model->tracks.front().firstFrame == 0
        && active.model->tracks.front().endFrame == 60,
        "activity does not truncate track");

    const auto staticJson = replaceBetween(seed, "\"p\": {\n                \"a\": 1,",
        "\n              },\n              \"nm\": \"Animated Ellipse\"",
        "\"p\": {\"a\":0,\"k\":[-32768,32768]},\n              \"nm\": \"Animated Ellipse\"");
    const auto fixed = prepare(staticJson);
    require(fixed.model->tracks.empty() && fixed.model->segments.empty()
        && fixed.model->properties[fixed.binding.position.index()].flags == model::PropertyFlagStatic,
        "static position has no track");
    rejectChanged(fixed, [&](auto& m) {
        m.properties[fixed.binding.position.index()].track = model::TrackId{0};
    }, "static position with dangling track");

    const auto fraction = prepare(replaceOnce(seed, "\"fr\": 60", "\"fr\": 59.94"));
    require(fraction.model->frameRate == static_cast<double>(static_cast<float>(59.94))
        && fraction.model->compositions.front().frameRate
            == static_cast<double>(static_cast<float>(59.94)),
        "fractional rate uses pinned float storage");
    const auto equivalent = prepare(replaceOnce(seed, "\"fr\": 60", "\"fr\": 6000e-2"));
    require(equivalent.model->frameRate == 60.0, "equivalent decimal binding");

    auto missing = replaceOnce(seed, "      \"nm\": \"Moving Circle\",\n", "");
    missing = replaceOnce(missing, "          \"nm\": \"Circle Group\",\n", "");
    missing = replaceOnce(missing, ",\n              \"nm\": \"Animated Ellipse\"", "");
    missing = replaceOnce(missing, ",\n              \"nm\": \"Fill\"", "");
    const auto absent = prepare(missing);
    require(absent.model->sourceNodes[absent.binding.layer.index()].debugName == "layer:1"
        && absent.model->sourceNodes[absent.binding.group.index()].debugName == "group"
        && absent.model->sourceNodes[absent.binding.ellipse.index()].debugName == "source-node",
        "effective fallback names");
    auto empty = replaceOnce(seed, "Moving Circle", "");
    empty = replaceOnce(empty, "Circle Group", "");
    empty = replaceOnce(empty, "Animated Ellipse", "");
    empty = replaceOnce(empty, "\"nm\": \"Fill\"", "\"nm\": \"\"");
    prepare(empty);
    prepare(replaceOnce(seed, "Moving Circle", "Moving\\u0020Circle \\u2603"));

    auto linear = replaceOnce(seed, "\"o\": {\"x\": 0.333, \"y\": 0}",
        "\"o\": {\"x\": 0, \"y\": 0}");
    linear = replaceOnce(linear, "\"i\": {\"x\": 0.667, \"y\": 1}",
        "\"i\": {\"x\": 1, \"y\": 1}");
    const auto straight = prepare(linear);
    require(straight.model->segments.front().interpolation == model::SegmentInterpolation::Linear,
        "linear controls classified");
}

void testNumeric(const std::string& seed) {
    const auto baseline = prepare(seed);
    const auto check = [&](std::string json, std::string_view label) {
        auto decoded = decodeNativeEllipseInput(json);
        require(static_cast<bool>(decoded), "raw underflow still admitted");
        const auto result = bindNativeEllipseModel(*decoded.input, *baseline.model);
        require(!result && !result.binding
            && result.code == NativeEllipseBindingCode::UnsupportedNumericConversion, label);
    };
    check(replaceOnce(seed, "\"fr\": 60", "\"fr\": 1e-9999"), "rate underflow");
    check(replaceOnce(seed, "[120, 120]", "[1e-9999, 120]"), "size underflow");
    auto forged = *baseline.input;
    forged.frameRate = {false, "1", {false, "9999"}};
    require(bindNativeEllipseModel(forged, *baseline.model).code
        == NativeEllipseBindingCode::UnsupportedNumericConversion, "private overflow");
    forged = *baseline.input;
    forged.frameRate.digits.clear();
    require(bindNativeEllipseModel(forged, *baseline.model).code
        == NativeEllipseBindingCode::UnsupportedNumericConversion, "malformed private decimal");
    require(!decodeNativeEllipseInput(replaceOnce(seed, "\"fr\": 60", "\"fr\": 1e9999")),
        "raw overflow remains rejected by admission");
    const auto tiny = prepare(replaceOnce(seed, "[120, 120]", "[1e-45, 120]"));
    require(tiny.model->vec2Values[tiny.model->properties[tiny.binding.size.index()].staticValue.index].x
        == std::numeric_limits<float>::denorm_min(), "nonzero subnormal preserved");
}

void testReindexed(const std::string& seed) {
    const auto p = prepare(seed);
    auto changed = *p.model;
    constexpr std::array<unsigned, 5> nodeMap{4, 2, 0, 3, 1};
    constexpr std::array<unsigned, 8> propertyMap{5, 2, 7, 1, 6, 0, 4, 3};
    const auto oldNodes = changed.sourceNodes;
    const auto oldProperties = changed.properties;
    for (std::size_t old = 0; old < nodeMap.size(); ++old) {
        auto node = oldNodes[old];
        node.id = model::SourceNodeId{nodeMap[old]};
        if (node.parent.valid()) node.parent = model::SourceNodeId{nodeMap[node.parent.index()]};
        if (node.transformParent.valid()) {
            node.transformParent = model::SourceNodeId{nodeMap[node.transformParent.index()]};
        }
        changed.sourceNodes[nodeMap[old]] = node;
    }
    changed.compositions.front().rootNode = model::SourceNodeId{
        nodeMap[changed.compositions.front().rootNode.index()]};
    for (auto& child : changed.sourceChildIds) child = model::SourceNodeId{nodeMap[child.index()]};
    for (std::size_t old = 0; old < propertyMap.size(); ++old) {
        auto property = oldProperties[old];
        property.id = model::PropertyId{propertyMap[old]};
        property.owner = model::SourceNodeId{nodeMap[property.owner.index()]};
        changed.properties[propertyMap[old]] = property;
    }
    for (auto& property : changed.sourcePropertyIds) {
        property = model::PropertyId{propertyMap[property.index()]};
    }
    for (auto& track : changed.tracks) track.property = model::PropertyId{propertyMap[track.property.index()]};
    const auto rebound = bindNativeEllipseModel(*p.input, changed);
    require(static_cast<bool>(rebound), "reindexed valid graph binds");
    require(rebound.binding->root.index() == nodeMap[p.binding.root.index()]
        && rebound.binding->position.index() == propertyMap[p.binding.position.index()],
        "semantic IDs followed reindexing");
}

void testMutations(const std::string& seed) {
    const auto p = prepare(seed);
    const auto& b = p.binding;
    rejectChanged(p, [&](auto& m) { m.properties[b.position.index()].owner = b.fill; }, "wrong owner");
    rejectChanged(p, [&](auto& m) { m.sourceNodes.push_back(m.sourceNodes.back()); }, "extra node");
    rejectChanged(p, [&](auto& m) { m.sourceNodes[b.group.index()].children.first = UINT32_MAX; }, "range overflow");
    rejectChanged(p, [&](auto& m) { m.sourceChildIds[m.sourceNodes[b.group.index()].children.first] = {}; }, "invalid child ID");
    rejectChanged(p, [&](auto& m) { m.sourcePropertyIds[m.sourceNodes[b.ellipse.index()].properties.first] = {}; }, "invalid property ID");
    rejectChanged(p, [&](auto& m) { m.sourceNodes[b.ellipse.index()].parent = b.layer; }, "parent mismatch");
    rejectChanged(p, [&](auto& m) { std::swap(m.sourceNodes[b.ellipse.index()].kind, m.sourceNodes[b.fill.index()].kind); }, "wrong source kinds");
    rejectChanged(p, [&](auto& m) { m.sourceNodes[b.ellipse.index()].composition = {}; }, "composition mismatch");
    rejectChanged(p, [&](auto& m) { m.sourceNodes[b.layer.index()].nameHash++; }, "name hash mismatch");
    rejectChanged(p, [&](auto& m) { m.sourceNodes[b.layer.index()].debugName = "other"; }, "name mismatch");
    rejectChanged(p, [&](auto& m) { m.sourceNodes[b.layer.index()].authoredLayerId++; }, "layer ID mismatch");
    rejectChanged(p, [&](auto& m) { m.sourceNodes[b.group.index()].dependencyBits = 0; }, "dependency mismatch");
    rejectChanged(p, [&](auto& m) { m.sourceNodes[b.fill.index()].enabled = false; }, "fill disabled");
    rejectChanged(p, [&](auto& m) { m.properties[b.size.index()].semantic = model::PropertySemantic::EllipsePosition; }, "semantic duplicate");
    rejectChanged(p, [&](auto& m) { m.properties[b.size.index()].flags = model::PropertyFlagAnimated; }, "wrong flags");
    rejectChanged(p, [&](auto& m) { m.properties.push_back(m.properties.back()); }, "extra authored property");
    rejectChanged(p, [&](auto& m) { m.properties[b.size.index()].valueType = model::PropertyValueType::Scalar; }, "wrong value type");
    rejectChanged(p, [&](auto& m) { m.properties[b.size.index()].staticValue.index = UINT32_MAX; }, "invalid value index");
    rejectChanged(p, [&](auto& m) {
        m.properties[b.fillOpacity.index()].staticValue =
            m.properties[b.layerOpacity.index()].staticValue;
    }, "aliased scalar leaves unused authored value");
    rejectChanged(p, [&](auto& m) { m.vec2Values[m.properties[b.size.index()].staticValue.index].x++; }, "wrong size");
    rejectChanged(p, [&](auto& m) { m.matrixValues[m.properties[b.groupTransform.index()].staticValue.index].dx++; }, "wrong group matrix");
    rejectChanged(p, [&](auto& m) { m.tracks.push_back(m.tracks.back()); }, "extra track");
    rejectChanged(p, [&](auto& m) { m.segments.push_back(m.segments.back()); }, "extra segment");
    rejectChanged(p, [&](auto& m) { m.segments.front().temporalControl1.x = 0.5F; }, "wrong control");
    rejectChanged(p, [&](auto& m) { m.segments.front().firstFrame = 1; }, "wrong segment time");
    rejectChanged(p, [&](auto& m) { m.segments.front().spatialOutTangent.x = 1; }, "spatial tangent");
    rejectChanged(p, [&](auto& m) { m.tracks.front().property = b.size; }, "wrong track property");
    rejectChanged(p, [&](auto& m) { m.segments.front().interpolation = model::SegmentInterpolation::Linear; }, "wrong interpolation");
    rejectChanged(p, [&](auto& m) { m.segments.front().spatialInterpolation = model::SpatialInterpolation::CubicBezier; }, "spatial interpolation");
    rejectChanged(p, [&](auto& m) { m.segments.front().endFrame = 59; }, "wrong segment end");
    rejectChanged(p, [&](auto& m) { m.segments.front().startValue.type = model::PropertyValueType::Scalar; }, "wrong start type");
    rejectChanged(p, [&](auto& m) { m.segments.front().startValue.index = UINT32_MAX; }, "wrong start index");
    rejectChanged(p, [&](auto& m) { m.sourcePropertyIds[m.sourceNodes[b.ellipse.index()].properties.first + 1] = b.position; }, "duplicate edge");
}
}

int main() {
    try {
        const auto json = readFixture("telegram_sticker_basic.json");
        testBaseline(json);
        testVariants(json);
        testNumeric(json);
        testReindexed(json);
        testMutations(json);
        std::cout << "native ellipse binding tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
