#include "avemotion/core/Hash.hpp"
#include "avemotion/validation/AssetValidator.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string_view>

namespace {

using namespace avemotion;

[[noreturn]] void fail(std::string_view message) {
    std::cerr << "AveMotion asset validator test failed: " << message << '\n';
    std::exit(1);
}

void require(bool condition, std::string_view message) {
    if (!condition) {
        fail(message);
    }
}


model::MotionSourceNodeRecord makeSourceNode(
    model::SourceNodeId id,
    model::SourceNodeId parent,
    model::SourceNodeKind kind,
    std::string debugName = {}) {
    model::MotionSourceNodeRecord node;
    node.present = true;
    node.id = id;
    node.parent = parent;
    node.composition = model::CompositionId{0};
    node.kind = kind;
    node.debugName = std::move(debugName);
    return node;
}

model::MotionPropertyRecord makeStaticVec2(
    model::PropertyId id,
    model::SourceNodeId owner,
    model::PropertySemantic semantic,
    std::uint32_t valueIndex) {
    return {
        .present = true,
        .id = id,
        .owner = owner,
        .semantic = semantic,
        .valueType = model::PropertyValueType::Vec2,
        .flags = model::PropertyFlagStatic,
        .staticValue = {model::PropertyValueType::Vec2, valueIndex},
        .track = {},
    };
}

model::MotionPropertyRecord makeStaticScalar(
    model::PropertyId id,
    model::SourceNodeId owner,
    model::PropertySemantic semantic,
    std::uint32_t valueIndex) {
    return {
        .present = true,
        .id = id,
        .owner = owner,
        .semantic = semantic,
        .valueType = model::PropertyValueType::Scalar,
        .flags = model::PropertyFlagStatic,
        .staticValue = {model::PropertyValueType::Scalar, valueIndex},
        .track = {},
    };
}

model::MotionPropertyRecord makeStaticColor(
    model::PropertyId id,
    model::SourceNodeId owner,
    model::PropertySemantic semantic,
    std::uint32_t valueIndex) {
    return {
        .present = true,
        .id = id,
        .owner = owner,
        .semantic = semantic,
        .valueType = model::PropertyValueType::Color,
        .flags = model::PropertyFlagStatic,
        .staticValue = {model::PropertyValueType::Color, valueIndex},
        .track = {},
    };
}

model::MotionAssetModel makeValidAsset() {
    model::MotionAssetModel asset;
    asset.logicalWidth = 512;
    asset.logicalHeight = 512;
    asset.frameRate = 60.0;
    asset.totalFrames = 120;
    asset.debugName = "validator-fixture";
    asset.statistics.directParsedModel = true;

    asset.compositions.push_back({
        .present = true,
        .id = model::CompositionId{0},
        .rootNode = model::SourceNodeId{0},
        .debugName = "root",
        .logicalWidth = 512,
        .logicalHeight = 512,
        .firstFrame = 0.0,
        .endFrame = 120.0,
        .frameRate = 60.0,
    });

    asset.sourceChildIds = {
        model::SourceNodeId{1},
        model::SourceNodeId{2},
        model::SourceNodeId{3},
        model::SourceNodeId{4},
    };
    asset.sourcePropertyIds = {
        model::PropertyId{0}, model::PropertyId{1}, model::PropertyId{2},
        model::PropertyId{3}, model::PropertyId{4},
    };

    asset.sourceNodes.resize(5);

    asset.sourceNodes[0] = makeSourceNode(
        model::SourceNodeId{0}, {}, model::SourceNodeKind::Composition, "composition");
    asset.sourceNodes[0].children = {0, 1};

    asset.sourceNodes[1] = makeSourceNode(
        model::SourceNodeId{1}, model::SourceNodeId{0},
        model::SourceNodeKind::Layer, "shape-layer");
    asset.sourceNodes[1].layerKind = model::SourceLayerKind::Shape;
    asset.sourceNodes[1].children = {1, 1};
    asset.sourceNodes[1].inFrame = 0.0;
    asset.sourceNodes[1].outFrame = 120.0;
    asset.sourceNodes[1].timeStretch = 1.0F;

    asset.sourceNodes[2] = makeSourceNode(
        model::SourceNodeId{2}, model::SourceNodeId{1},
        model::SourceNodeKind::ShapeGroup, "group");
    asset.sourceNodes[2].children = {2, 2};

    asset.sourceNodes[3] = makeSourceNode(
        model::SourceNodeId{3}, model::SourceNodeId{2},
        model::SourceNodeKind::Rectangle, "rect");
    asset.sourceNodes[3].properties = {0, 3};
    asset.sourceNodes[3].pathDirection = model::SourcePathDirection::Clockwise;

    asset.sourceNodes[4] = makeSourceNode(
        model::SourceNodeId{4}, model::SourceNodeId{2},
        model::SourceNodeKind::Fill, "fill");
    asset.sourceNodes[4].properties = {3, 2};
    asset.sourceNodes[4].fillRule = model::SourceFillRule::Winding;

    asset.vec2Values = {{256.0F, 256.0F}, {120.0F, 80.0F}};
    asset.scalarValues = {16.0F, 100.0F};
    asset.colorValues = {{0.2F, 0.6F, 0.9F, 1.0F}};
    asset.properties = {
        makeStaticVec2(model::PropertyId{0}, model::SourceNodeId{3},
                       model::PropertySemantic::RectanglePosition, 0),
        makeStaticVec2(model::PropertyId{1}, model::SourceNodeId{3},
                       model::PropertySemantic::RectangleSize, 1),
        makeStaticScalar(model::PropertyId{2}, model::SourceNodeId{3},
                         model::PropertySemantic::RectangleRoundness, 0),
        makeStaticColor(model::PropertyId{3}, model::SourceNodeId{4},
                        model::PropertySemantic::FillColor, 0),
        makeStaticScalar(model::PropertyId{4}, model::SourceNodeId{4},
                         model::PropertySemantic::FillOpacity, 1),
    };

    asset.statistics.compositionCount = asset.compositions.size();
    asset.statistics.sourceNodeCount = asset.sourceNodes.size();
    asset.statistics.propertyCount = asset.properties.size();
    asset.statistics.staticPropertyCount = asset.properties.size();
    asset.statistics.vec2ValueCount = asset.vec2Values.size();
    asset.statistics.scalarValueCount = asset.scalarValues.size();
    asset.statistics.colorValueCount = asset.colorValues.size();
    return asset;
}

bool hasIssue(
    const validation::AssetValidationReport& report,
    validation::ValidationIssueCode code) {
    for (const auto& issue : report.issues) {
        if (issue.code == code) {
            return true;
        }
    }
    return false;
}

void testValidApplicationAsset() {
    const auto asset = makeValidAsset();
    const validation::AssetValidator validator;
    const auto report = validator.validate(asset);
    require(report.accepted(), "valid application asset was rejected");
    require(report.structurallyValid, "valid asset is not structurally valid");
    require(report.nativeDirect2DReady, "simple native asset is not Direct2D ready");
    require(!report.requiresReferenceFallback, "simple asset unexpectedly needs fallback");
    require(report.fingerprint != 0U, "report fingerprint was not generated");

    const auto repeated = validator.validate(asset);
    require(repeated.fingerprint == report.fingerprint,
            "validation report fingerprint is not deterministic");
}

void testTelegramStickerProfile() {
    const auto asset = makeValidAsset();
    validation::AssetValidationContext context;
    context.sourceFormat = validation::AssetSourceFormat::TelegramTgs;
    context.encodedBytes = 4096;
    context.jsonBytes = 12000;
    context.loopIntentKnown = true;
    context.intendedLoop = true;

    const validation::AssetValidator validator;
    const auto report = validator.validate(
        asset, context, validation::AssetValidationOptions::telegramSticker());
    require(report.accepted(), "compliant Telegram sticker profile was rejected");
    require(report.errorCount == 0U && report.fatalCount == 0U,
            "compliant Telegram profile produced errors");
    require(hasIssue(report, validation::ValidationIssueCode::TelegramBoundsNotProven),
            "Telegram bounds caveat was not reported");
}



void testTelegramEnvelopeAndPlaybackPolicy() {
    validation::AssetValidator validator;
    validation::AssetValidationContext context;
    context.sourceFormat = validation::AssetSourceFormat::TelegramTgs;
    context.encodedBytes = 64U * 1024U;
    context.loopIntentKnown = true;
    context.intendedLoop = true;

    auto asset = makeValidAsset();
    auto report = validator.validate(
        asset, context, validation::AssetValidationOptions::telegramSticker());
    require(!hasIssue(report, validation::ValidationIssueCode::TelegramCompressedSizeExceeded),
            "exact 64-KiB Telegram container limit was rejected");

    context.encodedBytes += 1U;
    report = validator.validate(
        asset, context, validation::AssetValidationOptions::telegramSticker());
    require(hasIssue(report, validation::ValidationIssueCode::TelegramCompressedSizeExceeded),
            "oversized Telegram container was accepted");

    context.encodedBytes = 4096U;
    context.sourceFormat = validation::AssetSourceFormat::LottieJson;
    report = validator.validate(
        asset, context, validation::AssetValidationOptions::telegramSticker());
    require(hasIssue(report, validation::ValidationIssueCode::TelegramSourceMustBeTgs),
            "plain JSON was accepted as Telegram TGS profile input");

    context.sourceFormat = validation::AssetSourceFormat::TelegramTgs;
    context.loopIntentKnown = false;
    report = validator.validate(
        asset, context, validation::AssetValidationOptions::telegramSticker());
    require(report.accepted(), "unknown loop intent should be a warning, not an error");
    require(hasIssue(report, validation::ValidationIssueCode::TelegramLoopUnknown),
            "unknown Telegram loop intent was not diagnosed");

    context.loopIntentKnown = true;
    context.intendedLoop = false;
    report = validator.validate(
        asset, context, validation::AssetValidationOptions::telegramSticker());
    require(hasIssue(report, validation::ValidationIssueCode::TelegramLoopRequired),
            "non-looping Telegram sticker intent was accepted");

    context.intendedLoop = true;
    asset.logicalWidth = 511U;
    report = validator.validate(
        asset, context, validation::AssetValidationOptions::telegramSticker());
    require(hasIssue(report, validation::ValidationIssueCode::TelegramCanvasMismatch),
            "non-512 Telegram canvas was accepted");

    asset.logicalWidth = 512U;
    asset.frameRate = 29.0;
    asset.totalFrames = 88U;
    report = validator.validate(
        asset, context, validation::AssetValidationOptions::telegramSticker());
    require(hasIssue(report, validation::ValidationIssueCode::TelegramFrameRateOutOfRange),
            "below-30-FPS Telegram asset was accepted");

    asset.frameRate = 30.0;
    asset.totalFrames = 91U; // (91 - 1) / 30 = 3 seconds.
    report = validator.validate(
        asset, context, validation::AssetValidationOptions::telegramSticker());
    require(report.accepted(), "30-FPS Telegram profile should be accepted");
    require(hasIssue(report, validation::ValidationIssueCode::TelegramFrameRateNotPreferred),
            "non-preferred 30-FPS Telegram rate was not reported");
}

void testTelegramDurationEndpointSemantics() {
    auto asset = makeValidAsset();
    asset.totalFrames = 181; // Telegram rlottie duration: (181 - 1) / 60 = 3 s.

    validation::AssetValidationContext context;
    context.sourceFormat = validation::AssetSourceFormat::TelegramTgs;
    context.encodedBytes = 4096;
    context.loopIntentKnown = true;
    context.intendedLoop = true;

    const validation::AssetValidator validator;
    const auto exactLimit = validator.validate(
        asset, context, validation::AssetValidationOptions::telegramSticker());
    require(!hasIssue(exactLimit, validation::ValidationIssueCode::TelegramDurationExceeded),
            "exact three-second Telegram duration was rejected");

    asset.totalFrames = 182;
    const auto overLimit = validator.validate(
        asset, context, validation::AssetValidationOptions::telegramSticker());
    require(hasIssue(overLimit, validation::ValidationIssueCode::TelegramDurationExceeded),
            "over-three-second Telegram duration was accepted");
}

void testTelegramForbiddenFeatures() {
    auto asset = makeValidAsset();
    asset.sourceNodes[3].kind = model::SourceNodeKind::Polystar;
    asset.sourceNodes[3].polystarType = model::SourcePolystarType::Star;
    auto repeater = makeSourceNode(
        model::SourceNodeId{5}, model::SourceNodeId{2},
        model::SourceNodeKind::Repeater, "repeater");
    repeater.repeaterMaximumCopies = 4.0F;
    asset.sourceNodes.push_back(std::move(repeater));
    asset.sourceChildIds.push_back(model::SourceNodeId{5});
    asset.sourceNodes[2].children.count = 3;

    validation::AssetValidationContext context;
    context.sourceFormat = validation::AssetSourceFormat::TelegramTgs;
    context.encodedBytes = 1024;
    context.loopIntentKnown = true;
    context.intendedLoop = true;

    const validation::AssetValidator validator;
    const auto report = validator.validate(
        asset, context, validation::AssetValidationOptions::telegramSticker());
    require(!report.accepted(), "forbidden Telegram features were accepted");
    require(hasIssue(report, validation::ValidationIssueCode::TelegramForbiddenFeature),
            "Telegram forbidden feature was not diagnosed");
}

void testStructuralCorruption() {
    auto asset = makeValidAsset();
    asset.sourceNodes[2].children = {9999, 1};
    const validation::AssetValidator validator;
    const auto report = validator.validate(asset);
    require(!report.structurallyValid, "invalid child range was accepted");
    require(hasIssue(report, validation::ValidationIssueCode::InvalidRange),
            "invalid child range was not diagnosed");
}

void testCycleAndUnsupportedFeature() {
    auto asset = makeValidAsset();
    asset.sourceNodes[1].parent = model::SourceNodeId{2};
    asset.sourceNodes[2].parent = model::SourceNodeId{1};
    asset.sourceNodes[4].kind = model::SourceNodeKind::Layer;
    asset.sourceNodes[4].layerKind = model::SourceLayerKind::Image;

    const validation::AssetValidator validator;
    const auto report = validator.validate(asset);
    require(!report.structurallyValid, "parent cycle was accepted");
    require(hasIssue(report, validation::ValidationIssueCode::HierarchyCycle),
            "parent cycle was not diagnosed");
    require(hasIssue(report, validation::ValidationIssueCode::UnsupportedFeature),
            "unsupported image layer was not diagnosed");
}

void testDirect2DNativeProfile() {
    auto asset = makeValidAsset();
    asset.sourceNodes.push_back(makeSourceNode(
        model::SourceNodeId{5}, model::SourceNodeId{2},
        model::SourceNodeKind::GradientFill, "gradient-fill"));
    asset.sourceChildIds.push_back(model::SourceNodeId{5});
    asset.sourceNodes[2].children.count = 3;

    const validation::AssetValidator validator;
    const auto report = validator.validate(
        asset, {}, validation::AssetValidationOptions::direct2DNative());
    require(!report.accepted(), "Direct2D-native profile accepted fallback feature");
    require(report.requiresReferenceFallback,
            "gradient fill did not mark reference fallback requirement");
    require(hasIssue(report, validation::ValidationIssueCode::ReferenceFallbackRequired),
            "fallback requirement was not reported");
}

} // namespace

int main() {
    testValidApplicationAsset();
    testTelegramStickerProfile();
    testTelegramEnvelopeAndPlaybackPolicy();
    testTelegramDurationEndpointSemantics();
    testTelegramForbiddenFeatures();
    testStructuralCorruption();
    testCycleAndUnsupportedFeature();
    testDirect2DNativeProfile();
    std::cout << "AveMotion asset validator tests passed\n";
    return 0;
}
