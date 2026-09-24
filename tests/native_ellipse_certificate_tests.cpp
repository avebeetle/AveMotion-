#include "NativeEllipseCertificate.hpp"

#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

using namespace avemotion::runtime::detail;
namespace runtime = avemotion::runtime;
namespace model = avemotion::model;

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string{message});
}

std::string readFixture() {
    std::ifstream input{std::filesystem::path{AVEMOTION_FIXTURE_DIR}
                        / "telegram_sticker_basic.json", std::ios::binary};
    require(static_cast<bool>(input), "cannot open baseline fixture");
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

std::string replaceOnce(std::string source, std::string_view from, std::string_view to) {
    const auto offset = source.find(from);
    require(offset != std::string::npos, "replacement text missing");
    require(source.find(from, offset + from.size()) == std::string::npos,
            "replacement text not unique");
    source.replace(offset, from.size(), to);
    return source;
}

std::string replaceBetween(std::string source, std::string_view first,
                           std::string_view last, std::string_view replacement) {
    const auto begin = source.find(first);
    const auto end = source.find(last, begin + first.size());
    require(begin != std::string::npos && end != std::string::npos,
            "replacement range missing");
    source.replace(begin, end + last.size() - begin, replacement);
    return source;
}

std::string activityJson(const std::string& seed) {
    return replaceOnce(seed, "\"ip\": 0,\n      \"op\": 61",
                       "\"ip\": 10,\n      \"op\": 20");
}

std::string staticJson(const std::string& seed) {
    return replaceBetween(seed, "\"p\": {\n                \"a\": 1,",
        "\n              },\n              \"nm\": \"Animated Ellipse\"",
        "\"p\": {\"a\":0,\"k\":[-32768,32768]},\n              \"nm\": \"Animated Ellipse\"");
}

struct ScanHarness {
    std::string json;
    runtime::Runtime runtime;
    std::shared_ptr<const NativeEllipseInput> input;
    std::shared_ptr<const runtime::Asset> asset;
    std::shared_ptr<const model::MotionAssetModel> frozen;
    NativeEllipseModelBinding binding;
    std::unique_ptr<runtime::Instance> instance;

    explicit ScanHarness(std::string source) : json(std::move(source)) {
        auto decoded = decodeNativeEllipseInput(json);
        require(static_cast<bool>(decoded), "scan input admitted");
        input = decoded.input;
        auto loaded = runtime.loadLottieJson(json);
        require(static_cast<bool>(loaded), "scan asset loaded");
        asset = loaded.asset;
        auto prepared = asset->prepareModel();
        require(static_cast<bool>(prepared), "scan model prepared");
        frozen = prepared.model;
        auto bound = bindNativeEllipseModel(*input, *frozen);
        require(static_cast<bool>(bound), "scan authored binding");
        binding = *bound.binding;
        auto created = runtime.createInstance(asset);
        require(static_cast<bool>(created), "scan instance created");
        instance = std::move(created.instance);
    }

    NativeEllipseScanAudit audit() const {
        return {*input, binding, *frozen, asset->handle(), asset->metadata().sourceHash};
    }

    runtime::EvaluatedScene frame(std::size_t index) {
        auto result = instance->evaluateFrame(index, input->width, input->height);
        require(static_cast<bool>(result), "reference frame evaluated");
        return std::move(result.scene);
    }

    void observeBefore(NativeEllipseScanAudit& scan, std::size_t target) {
        for (std::size_t frameIndex = 0; frameIndex < target; ++frameIndex)
            require(scan.observe(frameIndex, frame(frameIndex)), "ordered prefix accepted");
    }
};

void checkSlot(const NativeEllipseCertificate& certificate,
               std::size_t activeFirst, std::size_t activeEnd, bool staticGeometry) {
    const auto& slot = certificate.slot;
    require(certificate.exactJson.size() > 0 && certificate.input && certificate.asset
        && certificate.model && certificate.matchesAsset(certificate.asset),
        "certificate owns exact bytes, input, model and matching lease");
    require(slot.width == 512 && slot.height == 512 && slot.totalFrames == 61
        && slot.frameRate == 60.0 && slot.activeFirstFrame == activeFirst
        && slot.activeEndFrame == activeEnd && slot.observedFrames == 61
        && slot.observedActiveFrames == activeEnd - activeFirst,
        "descriptor and complete interval stored");
    require(slot.assetHandle == certificate.asset->handle()
        && slot.sourceHash == certificate.asset->metadata().sourceHash
        && slot.sourceHash == certificate.model->sourceAssetHash,
        "asset and model identity stored");
    require(slot.modelLayerCount == 2 && slot.modelNodeCount == 1
        && slot.modelGeometryCount == 1 && slot.modelPaintCount == 1,
        "declared render table sizes stored");
    require(slot.root.id.valid() && slot.shape.id.valid() && slot.root.id != slot.shape.id
        && slot.root.parentIndex == runtime::kInvalidSceneIndex
        && slot.shape.parentIndex == 0 && slot.root.firstChildReference == 0
        && slot.root.childCount == 1 && slot.shape.childCount == 0
        && slot.root.firstDrawItem == 0 && slot.shape.firstDrawItem == 0
        && slot.root.keyPath == "__" && slot.shape.keyPath == "Moving Circle",
        "fixed two-layer layout stored");
    const auto& draw = slot.draw;
    require(draw.draw.valid() && draw.node.valid() && draw.geometry.valid()
        && draw.paint.valid() && draw.layerIndex == 1 && draw.drawOrder == 0
        && draw.sourcePath == slot.binding.ellipse
        && draw.sourcePaint == slot.binding.fill
        && draw.sourcePathCount == 1 && draw.sourcePathModifierFree
        && draw.localGeometryAvailable && draw.localPaintAvailable
        && draw.localGeometryStaticCandidate == staticGeometry
        && draw.localPaintStaticCandidate && draw.fillRule == runtime::FillRule::Winding
        && draw.opacitySeparated && draw.separatedOpacity == 1.0F,
        "unique source-bound draw and local flags stored");
    require(draw.strokeWidth == 0.0F && draw.strokeMiterLimit == 0.0F
        && draw.strokeCap == runtime::LineCap::Flat
        && draw.strokeJoin == runtime::LineJoin::Miter
        && draw.localStrokeWidth == 0.0F && draw.localStrokeMiterLimit == 0.0F
        && draw.localStrokeCap == runtime::LineCap::Flat
        && draw.localStrokeJoin == runtime::LineJoin::Miter,
        "disabled stroke scalar defaults stored");
    const auto* paint = certificate.model->paint(draw.paint);
    const auto* geometry = certificate.model->geometry(draw.geometry);
    require(paint && geometry && paint->resourceClass == model::ResourceClass::AssetStatic
        && paint->staticValue && paint->staticValue->paint.solid.r == draw.localSolid.r
        && paint->staticValue->paint.solid.g == draw.localSolid.g
        && paint->staticValue->paint.solid.b == draw.localSolid.b
        && paint->staticValue->paint.solid.a == draw.localSolid.a
        && geometry->resourceClass == (staticGeometry
            ? model::ResourceClass::AssetStatic : model::ResourceClass::InstanceEvaluated),
        "frozen resource classes and static paint bytes agree");
}

void testFactoryAndOwnership(const std::string& seed) {
    auto local = prepareNativeEllipseCertificate(seed);
    require(static_cast<bool>(local), "local-Runtime baseline certificate published");
    require(local.certificate->exactJson == seed, "retained byte identity");
    checkSlot(*local.certificate, 0, 61, false);
    require(local.diagnosticsAfter.referenceModelSamples
            - local.diagnosticsBefore.referenceModelSamples == 61
        && local.diagnosticsAfter.referenceSceneSamples
            - local.diagnosticsBefore.referenceSceneSamples == 61
        && local.diagnosticsAfter.cpuFramesRendered
            == local.diagnosticsBefore.cpuFramesRendered,
        "cold factory uses N model and N extra scene samples without CPU render");

    runtime::Runtime runtime;
    auto existing = runtime.loadLottieJson(seed);
    require(static_cast<bool>(existing), "supplied Runtime preexisting load");
    auto existingInstance = runtime.createInstance(existing.asset);
    require(static_cast<bool>(existingInstance), "supplied Runtime preexisting instance");
    require(static_cast<bool>(existingInstance.instance->evaluateFrame(0, 512, 512)),
            "supplied Runtime preexisting evaluation");
    const auto before = runtime.diagnostics();
    auto supplied = prepareNativeEllipseCertificate(runtime, seed);
    require(static_cast<bool>(supplied), "supplied-Runtime baseline certificate published");
    require(supplied.diagnosticsBefore.referenceSceneSamples == before.referenceSceneSamples
        && supplied.diagnosticsBefore.assetLoadsSucceeded == before.assetLoadsSucceeded
        && supplied.diagnosticsAfter.referenceModelSamples - before.referenceModelSamples == 61
        && supplied.diagnosticsAfter.referenceSceneSamples - before.referenceSceneSamples == 61
        && supplied.diagnosticsAfter.cpuFramesRendered == before.cpuFramesRendered,
        "nonempty Runtime counter deltas are separate and exact");
    require(!supplied.certificate->matchesAsset(existing.asset),
            "same bytes loaded earlier are a distinct asset lease");
    auto another = runtime.loadLottieJson(seed);
    require(static_cast<bool>(another), "same bytes load independently");
    require(!supplied.certificate->matchesAsset(another.asset),
            "same hash and bytes cannot replace matching asset pointer");
    require(static_cast<bool>(existingInstance.instance->evaluateFrame(10, 512, 512)),
            "supplied Runtime remains usable after factory");
    auto retained = local.certificate;
    local = {};
    require(retained->matchesAsset(retained->asset) && retained->model == retained->asset->model(),
            "certificate survives factory Runtime and Instance destruction");
}

void testVariants(const std::string& seed) {
    auto activity = prepareNativeEllipseCertificate(activityJson(seed));
    require(static_cast<bool>(activity), "[10,20) activity certificate published");
    checkSlot(*activity.certificate, 10, 20, false);
    ScanHarness active{activityJson(seed)};
    auto scan = active.audit();
    for (std::size_t frame = 0; frame < 61; ++frame) {
        const auto scene = active.frame(frame);
        const bool visible = frame >= 10 && frame < 20;
        require(scene.layers.size() == 2 && scene.layers[1].visible == visible
            && scene.drawItems.size() == (visible ? 1U : 0U)
            && scan.observe(frame, scene), "every activity frame follows half-open interval");
    }
    require(scan.finish().has_value(), "ordered activity scan finishes");
    auto fixed = prepareNativeEllipseCertificate(staticJson(seed));
    require(static_cast<bool>(fixed), "static-position certificate published: "
        + std::to_string(static_cast<int>(fixed.code)) + "/"
        + std::to_string(static_cast<int>(fixed.scanCode)));
    checkSlot(*fixed.certificate, 0, 61, true);
}

void testMutations(const std::string& seed) {
    ScanHarness baseline{seed};
    const auto reject = [&](std::size_t target,
                            const std::function<void(runtime::EvaluatedScene&)>& mutate,
                            NativeEllipseScanCode expected, std::string_view label) {
        auto scan = baseline.audit();
        baseline.observeBefore(scan, target);
        auto scene = baseline.frame(target);
        mutate(scene);
        require(!scan.observe(target, scene) && scan.code() == expected, label);
        require(!scan.finish() && !scan.observe(target, baseline.frame(target)),
                "failed scan stays poisoned");
    };
    reject(10, [](auto& scene) { scene.drawItems.push_back(scene.drawItems.front()); },
           NativeEllipseScanCode::DrawMultiplicity, "duplicate node draw rejected");
    reject(10, [](auto& scene) { scene.drawItems.clear(); },
           NativeEllipseScanCode::DrawMultiplicity, "missing active draw rejected");
    reject(10, [](auto& scene) { scene.layers.push_back(scene.layers.back()); },
           NativeEllipseScanCode::LayerLayout, "extra render layer rejected");
    reject(10, [](auto& scene) { scene.drawItems.front().sourcePathNode = {}; },
           NativeEllipseScanCode::SourceBinding, "wrong ellipse source rejected");
    reject(10, [](auto& scene) { scene.drawItems.front().sourcePaintNode = {}; },
           NativeEllipseScanCode::SourceBinding, "wrong fill source rejected");
    reject(10, [](auto& scene) { scene.drawItems.front().sourcePathCount = 2; },
           NativeEllipseScanCode::SourceBinding, "extra authored path binding rejected");
    reject(10, [](auto& scene) { scene.drawItems.front().sourcePathModifierFree = false; },
           NativeEllipseScanCode::SourceBinding, "path modifier flag rejected");
    reject(10, [](auto& scene) { scene.drawItems.front().modelNode = {}; },
           NativeEllipseScanCode::ResourceBinding, "invalid render ID rejected");
    reject(10, [](auto& scene) { scene.drawItems.front().drawOrder = 1; },
           NativeEllipseScanCode::ResourceBinding, "draw order drift rejected");
    reject(10, [](auto& scene) { scene.drawItems.front().localPaintAvailable = false; },
           NativeEllipseScanCode::UnsupportedSlot, "missing local paint rejected");
    reject(10, [](auto& scene) { scene.drawItems.front().localGeometryAvailable = false; },
           NativeEllipseScanCode::UnsupportedSlot, "missing local geometry rejected");
    reject(10, [](auto& scene) { ++scene.drawItems.front().localPaint.solid.r; },
           NativeEllipseScanCode::ResourceBinding, "local paint byte drift rejected");
    reject(10, [](auto& scene) { ++scene.drawItems.front().stroke.width; },
           NativeEllipseScanCode::UnsupportedSlot, "disabled stroke byte drift rejected");
    reject(10, [](auto& scene) { ++scene.drawItems.front().paint.gradient.end.x; },
           NativeEllipseScanCode::UnsupportedSlot, "inactive gradient default rejected");
    reject(10, [](auto& scene) { scene.drawItems.front().fillRule = runtime::FillRule::EvenOdd; },
           NativeEllipseScanCode::UnsupportedSlot, "fill rule drift rejected");
    reject(10, [](auto& scene) { scene.drawItems.front().localPaintStaticCandidate = false; },
           NativeEllipseScanCode::UnsupportedSlot, "paint static candidate loss rejected");
    reject(10, [](auto& scene) { scene.modelPaintCount = 2; },
           NativeEllipseScanCode::ResourceBinding, "declared paint count mismatch rejected");
    reject(10, [](auto& scene) { scene.sourceAssetHash ^= 1; },
           NativeEllipseScanCode::Identity, "scene hash mismatch rejected");
    reject(10, [](auto& scene) { ++scene.assetHandle.generation; },
           NativeEllipseScanCode::Identity, "scene handle mismatch rejected");
    reject(10, [](auto& scene) { scene.layers[1].visible = false; },
           NativeEllipseScanCode::LayerLayout, "active boundary visibility rejected");
    reject(10, [](auto& scene) { scene.layers[0].childCount = 0; },
           NativeEllipseScanCode::LayerLayout, "child layout drift rejected");

    ScanHarness activity{activityJson(seed)};
    for (std::size_t target : {9U, 10U, 19U, 20U}) {
        auto scan = activity.audit();
        activity.observeBefore(scan, target);
        auto scene = activity.frame(target);
        scene.layers[1].visible = !scene.layers[1].visible;
        require(!scan.observe(target, scene) && scan.code() == NativeEllipseScanCode::LayerLayout,
                "each visibility boundary rejected");
    }
    {
        auto scan = activity.audit();
        activity.observeBefore(scan, 20);
        auto scene = activity.frame(20);
        scene.drawItems.push_back(activity.frame(19).drawItems.front());
        require(!scan.observe(20, scene)
            && scan.code() == NativeEllipseScanCode::DrawMultiplicity,
            "draw outside interval rejected");
    }
    {
        auto scan = baseline.audit();
        require(!scan.finish() && scan.code() == NativeEllipseScanCode::IncompleteScan,
                "early finish poisons audit");
        require(!scan.observe(0, baseline.frame(0)), "early-finished audit remains poisoned");
    }
    {
        auto scan = baseline.audit();
        require(!scan.observe(1, baseline.frame(1))
            && scan.code() == NativeEllipseScanCode::Timeline,
            "skipped first frame rejected");
    }
    {
        auto scan = baseline.audit();
        require(scan.observe(0, baseline.frame(0)), "first frame accepted");
        require(!scan.observe(0, baseline.frame(0))
            && scan.code() == NativeEllipseScanCode::Timeline,
            "repeated frame rejected");
    }
    {
        auto changed = *baseline.frozen;
        changed.paints[0].resourceClass = model::ResourceClass::InstanceEvaluated;
        NativeEllipseScanAudit scan{*baseline.input, baseline.binding, changed,
                                    baseline.asset->handle(), baseline.asset->metadata().sourceHash};
        baseline.observeBefore(scan, 61);
        require(!scan.finish() && scan.code() == NativeEllipseScanCode::ResourceBinding,
                "final paint class demotion rejected");
    }
    {
        auto changed = *baseline.frozen;
        changed.nodes[0].layer = changed.layers[0].id;
        NativeEllipseScanAudit scan{*baseline.input, baseline.binding, changed,
                                    baseline.asset->handle(), baseline.asset->metadata().sourceHash};
        baseline.observeBefore(scan, 61);
        require(!scan.finish() && scan.code() == NativeEllipseScanCode::ResourceBinding,
            "final node layer disagreement rejected");
    }
    {
        auto changed = *baseline.frozen;
        ++changed.paints[0].staticValue->paint.solid.r;
        NativeEllipseScanAudit scan{*baseline.input, baseline.binding, changed,
                                    baseline.asset->handle(), baseline.asset->metadata().sourceHash};
        baseline.observeBefore(scan, 61);
        require(!scan.finish() && scan.code() == NativeEllipseScanCode::ResourceBinding,
                "final static paint byte disagreement rejected");
    }
    {
        auto otherHandle = baseline.asset->handle();
        ++otherHandle.generation;
        NativeEllipseScanAudit scan{*baseline.input, baseline.binding, *baseline.frozen,
                                    otherHandle, baseline.asset->metadata().sourceHash};
        require(!scan.observe(0, baseline.frame(0))
            && scan.code() == NativeEllipseScanCode::Identity,
            "wrong expected handle poisons audit");
    }
    {
        auto changed = *baseline.frozen;
        auto root = changed.layers[0];
        auto shape = changed.layers[1];
        root.id = model::LayerId{2};
        shape.id = model::LayerId{5};
        shape.parent = root.id;
        changed.layers.assign(6, {});
        changed.layers[2] = root;
        changed.layers[5] = shape;
        changed.childLayerIds[0] = shape.id;
        auto node = changed.nodes[0];
        node.id = model::NodeId{8};
        node.drawItem = model::DrawItemId{8};
        node.layer = shape.id;
        node.geometry = model::GeometryId{3};
        node.paint = model::PaintId{4};
        changed.nodes.assign(9, {});
        changed.nodes[8] = node;
        changed.layerNodeIds[0] = node.id;
        changed.drawOrder[0] = node.id;
        auto geometry = changed.geometries[0];
        geometry.id = node.geometry;
        changed.geometries.assign(4, {});
        changed.geometries[3] = geometry;
        auto paint = changed.paints[0];
        paint.id = node.paint;
        paint.staticValue->sourceKey = 5;
        changed.paints.assign(5, {});
        changed.paints[4] = paint;
        changed.statistics.declaredLayerCount = changed.layers.size();
        changed.statistics.declaredNodeCount = changed.nodes.size();
        changed.statistics.declaredGeometryCount = changed.geometries.size();
        changed.statistics.declaredPaintCount = changed.paints.size();
        NativeEllipseScanAudit scan{*baseline.input, baseline.binding, changed,
                                    baseline.asset->handle(), baseline.asset->metadata().sourceHash};
        for (std::size_t frameIndex = 0; frameIndex < 61; ++frameIndex) {
            auto scene = baseline.frame(frameIndex);
            scene.layers[0].modelLayer = root.id;
            scene.layers[1].modelLayer = shape.id;
            scene.modelLayerCount = 6;
            scene.modelNodeCount = 9;
            scene.modelGeometryCount = 4;
            scene.modelPaintCount = 5;
            if (!scene.drawItems.empty()) {
                auto& draw = scene.drawItems.front();
                draw.modelNode = node.id;
                draw.modelDrawItem = node.drawItem;
                draw.modelGeometry = node.geometry;
                draw.modelPaint = node.paint;
            }
            require(scan.observe(frameIndex, scene), "reindexed render scene accepted");
        }
        require(scan.finish().has_value(), "coherently reindexed render tables certified");
    }
}

void testFailuresAndConcurrency(const std::string& seed) {
    runtime::Runtime runtime;
    const auto oversized = prepareNativeEllipseCertificate(
        runtime, std::string(1'048'577, 'x'));
    require(!oversized && oversized.code == NativeEllipseCertificateCode::AdmissionRejected
        && oversized.admission.code == NativeEllipseAdmissionCode::ResourceLimit
        && oversized.admission.path == "/"
        && oversized.diagnosticsAfter.assetLoadAttempts
            == oversized.diagnosticsBefore.assetLoadAttempts,
        "byte limit rejects before copy/reference load");
    const auto ineligibleJson = replaceOnce(seed, "\"ty\": \"el\"", "\"ty\": \"rc\"");
    const auto unsupported = prepareNativeEllipseCertificate(runtime, ineligibleJson);
    require(!unsupported && unsupported.code == NativeEllipseCertificateCode::AdmissionRejected,
        "unsupported raw grammar yields no certificate");
    auto ineligibleOrdinary = runtime.loadLottieJson(ineligibleJson);
    require(static_cast<bool>(ineligibleOrdinary), "valid ineligible source still loads normally");
    auto ineligibleInstance = runtime.createInstance(ineligibleOrdinary.asset);
    require(static_cast<bool>(ineligibleInstance)
        && static_cast<bool>(ineligibleInstance.instance->evaluateFrame(10, 512, 512)),
        "valid ineligible source still evaluates normally");
    const auto underflowJson = replaceOnce(seed, "[120, 120]", "[1e-9999, 120]");
    auto numeric = prepareNativeEllipseCertificate(runtime, underflowJson);
    require(!numeric && numeric.code == NativeEllipseCertificateCode::BindingRejected
        && numeric.bindingCode == NativeEllipseBindingCode::UnsupportedNumericConversion,
        "admitted decimal underflow is separately ineligible for binding");
    require(numeric.diagnosticsAfter.referenceModelSamples
            - numeric.diagnosticsBefore.referenceModelSamples == 61
        && numeric.diagnosticsAfter.referenceSceneSamples
            == numeric.diagnosticsBefore.referenceSceneSamples,
        "binding rejection stops before extra scene scan");
    auto numericOrdinary = runtime.loadLottieJson(underflowJson);
    require(static_cast<bool>(numericOrdinary), "numeric ineligible source loads normally");
    auto numericInstance = runtime.createInstance(numericOrdinary.asset);
    require(static_cast<bool>(numericInstance)
        && static_cast<bool>(numericInstance.instance->evaluateFrame(10, 512, 512)),
        "numeric ineligible source evaluates normally");
    auto ordinary = runtime.loadLottieJson(seed);
    require(static_cast<bool>(ordinary), "ordinary baseline load still available");
    auto instance = runtime.createInstance(ordinary.asset);
    require(static_cast<bool>(instance)
        && static_cast<bool>(instance.instance->evaluateFrame(10, 512, 512)),
        "ordinary evaluation remains available after rejection");

    auto first = std::async(std::launch::async, [&] {
        return prepareNativeEllipseCertificate(seed);
    });
    auto second = std::async(std::launch::async, [&] {
        return prepareNativeEllipseCertificate(seed);
    });
    auto a = first.get();
    auto b = second.get();
    require(a && b && a.certificate != b.certificate
        && a.certificate->asset != b.certificate->asset
        && !a.certificate->matchesAsset(b.certificate->asset),
        "independent concurrent factories publish separate leases");
}

} // namespace

int main() {
    try {
        const auto seed = readFixture();
        testFactoryAndOwnership(seed);
        testVariants(seed);
        testMutations(seed);
        testFailuresAndConcurrency(seed);
        std::cout << "native ellipse certificate tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
