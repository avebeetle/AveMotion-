#include "NativeEllipseStream.hpp"
#include "support/ExactRenderPlanComparison.hpp"
#include "support/NativeEllipseOracle.hpp"
#include "support/NativeEllipseTestAssets.hpp"

#include "avemotion/core/Hash.hpp"
#include "avemotion/render/RenderPlanner.hpp"
#include "avemotionanimationaccess.h"

#include <rlottie.h>

#include <algorithm>
#include <array>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
namespace runtime = avemotion::runtime;
namespace render = avemotion::render;
namespace test = avemotion::test;
namespace detail = avemotion::render::detail;

void require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}

void requireAliases(const runtime::EvaluatedScene& scene) {
    require(bool(scene.assetModel), "scene retains frozen model");
    for (const auto& draw : scene.drawItems) {
        if (draw.canonicalGeometry) {
            const auto* record = scene.assetModel->geometry(draw.modelGeometry);
            require(record && record->staticValue
                && draw.canonicalGeometry.get() == &*record->staticValue,
                "geometry alias points into its own frozen record");
            require(!scene.assetModel.owner_before(draw.canonicalGeometry)
                && !draw.canonicalGeometry.owner_before(scene.assetModel),
                "geometry alias shares model control block");
        }
        if (draw.canonicalPaint) {
            const auto* record = scene.assetModel->paint(draw.modelPaint);
            require(record && record->staticValue
                && draw.canonicalPaint.get() == &*record->staticValue,
                "paint alias points into its own frozen record");
            require(!scene.assetModel.owner_before(draw.canonicalPaint)
                && !draw.canonicalPaint.owner_before(scene.assetModel),
                "paint alias shares model control block");
        }
    }
}

void comparatorMutationMatrix(const render::MotionRenderPlan& baseline) {
    test::ExactRenderPlanComparison compare;
    require(compare.difference(baseline, baseline).empty(), "identical plans agree");
    require(!baseline.drawItems.empty() && !baseline.geometryUpdates.empty()
        && !baseline.paintUpdates.empty() && baseline.sourceScene,
        "mutation baseline is a populated real plan");
    auto mutate = [&](const char* name, auto edit) {
        auto changed = baseline;
        edit(changed);
        const auto difference = compare.difference(baseline, changed);
        require(!difference.empty(), std::string("comparator missed ") + name);
        std::cout << "MUTATION " << name << ' ' << difference << '\n';
    };
    mutate("stamp", [](auto& plan) { ++plan.stamp.evaluationSequence; });
    mutate("sourceScene", [](auto& plan) {
        auto source = std::make_shared<runtime::EvaluatedScene>(*plan.sourceScene);
        ++source->frameIndex;
        plan.sourceScene = std::move(source);
    });
    mutate("sourceHistory", [](auto& plan) {
        auto source = std::make_shared<runtime::EvaluatedScene>(*plan.sourceScene);
        source->changes.geometryChanged = !source->changes.geometryChanged;
        plan.sourceScene = std::move(source);
    });
    mutate("draw", [](auto& plan) { plan.drawItems.front().featureBits ^= render::RenderFeatureStroke; });
    mutate("geometryKey", [](auto& plan) { ++plan.drawItems.front().geometry.revision; });
    mutate("paintKey", [](auto& plan) { ++plan.drawItems.front().paint.contentHash; });
    mutate("geometryUpdate", [](auto& plan) { plan.geometryUpdates.front().sourceDrawItemIndex ^= 1U; });
    mutate("paintUpdate", [](auto& plan) { plan.paintUpdates.front().sourceDrawItemIndex ^= 1U; });
    mutate("history", [](auto& plan) { plan.firstPlan = !plan.firstPlan; });
    mutate("presentedBounds", [](auto& plan) { plan.presentedBounds.left += 1.0F; });
    mutate("dirtyRegion", [](auto& plan) { plan.dirtyRegion.left += 1.0F; });
    mutate("statistics", [](auto& plan) { ++plan.statistics.geometryUpdateCount; });
    mutate("fingerprints", [](auto& plan) { ++plan.fingerprints.presentation; });
    mutate("repaint", [](auto& plan) { plan.reusableForRepaint = !plan.reusableForRepaint; });
}

render::PresentationState presentation(std::size_t index) {
    render::PresentationState state;
    switch (index) {
    case 1: state.transform.dx = 3.25F; state.transform.dy = -2.5F; break;
    case 2: state.opacity = 0.5F; break;
    case 3: state.visible = false; break;
    case 4: state.visible = true; break;
    case 5: state.layoutRevision = 1; break;
    case 6: state.layoutRevision = 2; break;
    default: break;
    }
    return state;
}

void planHistory(const std::string& name, const std::string& json) {
    auto certified = runtime::detail::prepareNativeEllipseCertificate(json);
    require(bool(certified), name + " C certificate");
    test::NativeEllipseOracle oracle(json);
    require(oracle.model().get() != certified.certificate->model.get(),
            name + " independent frozen models");
    auto made = detail::NativeEllipseStream::create(certified.certificate, 91);
    require(bool(made), name + " stream creation");
    runtime::Runtime ordinaryRuntime;
    auto ordinaryAsset = ordinaryRuntime.loadLottieJson(json, name);
    require(bool(ordinaryAsset), name + " sustained ordinary asset");
    auto ordinary = ordinaryRuntime.createInstance(ordinaryAsset.asset);
    require(bool(ordinary), name + " sustained ordinary instance");
    render::MotionRenderPlanner nativePlanner, oraclePlanner;
    test::ExactRenderPlanComparison compare;
    constexpr std::array<std::size_t, 11> frames{0, 0, 10, 19, 20, 20, 60, 30, 10, 9, 0};
    constexpr std::array<std::pair<std::size_t, std::size_t>, 4> viewports{{
        {512, 512}, {256, 256}, {384, 256}, {256, 384}}};
    std::size_t scenes = 0, plans = 0;
    runtime::SceneFingerprints previous;
    bool hasPrevious = false;
    std::shared_ptr<const runtime::EvaluatedScene> older;
    for (auto frame : frames) {
        const auto run = [&](std::size_t width, std::size_t height) {
            auto expected = oracle.freshScene(frame, width, height);
            auto actual = made.stream->emit(frame, width, height);
            require(bool(actual), name + " native emit frame=" + std::to_string(frame));
            auto sustained = ordinary.instance->evaluateModelFrame(frame, width, height);
            require(bool(sustained), name + " sustained ordinary evaluate");
            const auto& changes = actual.scene->changes;
            const auto& fp = actual.scene->fingerprints;
            require(changes.firstEvaluation == !hasPrevious
                && changes.topologyChanged == (!hasPrevious || fp.topology != previous.topology)
                && changes.geometryChanged == (!hasPrevious || fp.geometry != previous.geometry)
                && changes.paintChanged == (!hasPrevious || fp.paint != previous.paint)
                && changes.visualChanged == (!hasPrevious || fp.scene != previous.scene),
                name + " independent fingerprint history");
            require(changes.firstEvaluation == sustained.scene.changes.firstEvaluation
                && changes.topologyChanged == sustained.scene.changes.topologyChanged
                && changes.geometryChanged == sustained.scene.changes.geometryChanged
                && changes.paintChanged == sustained.scene.changes.paintChanged
                && changes.visualChanged == sustained.scene.changes.visualChanged,
                name + " sustained ordinary history");
            previous = fp;
            hasPrevious = true;
            requireAliases(*actual.scene);
            requireAliases(expected);
            require(actual.scene->assetModel.get() != expected.assetModel.get(),
                    name + " scene model owners remain distinct");
            test::normalizeNativeOracleIdentity(expected, *certified.certificate, 91,
                                                actual.scene->evaluationSequence);
            auto actualSource = std::make_shared<const runtime::EvaluatedScene>(std::move(*actual.scene));
            auto expectedSource = std::make_shared<const runtime::EvaluatedScene>(std::move(expected));
            if (!older) older = actualSource;
            for (std::size_t index = 0; index < 7; ++index) {
                const auto state = presentation(index);
                auto nativePlan = nativePlanner.build(actualSource, state);
                auto expectedPlan = oraclePlanner.build(expectedSource, state);
                require(nativePlan && expectedPlan, name + " corresponding plans built");
                const auto difference = compare.difference(expectedPlan.plan, nativePlan.plan);
                require(difference.empty(), name + " plan frame=" + std::to_string(frame)
                    + " viewport=" + std::to_string(width) + "x" + std::to_string(height)
                    + " ordinal=" + std::to_string(scenes + 1) + " presentation="
                    + std::to_string(index) + " " + difference);
                ++plans;
            }
            ++scenes;
        };
        run(512, 512);
        if (frame == 30) {
            for (std::size_t index = 1; index < viewports.size(); ++index)
                run(viewports[index].first, viewports[index].second);
            run(512, 512);
        }
    }
    auto stale = nativePlanner.build(older);
    require(!stale && stale.error.code == render::RenderPlanErrorCode::StaleSnapshot,
            name + " older retained scene rejected as stale");
    auto invalid = made.stream->emit(30, 0, 512);
    require(!invalid && !invalid.scene && invalid.code == detail::NativeEllipseFrameCode::InvalidViewport,
            name + " invalid viewport between successes");
    auto clamped = made.stream->emit(9999, 512, 512);
    require(clamped && clamped.scene->frameIndex == 60 && clamped.scene->evaluationSequence == scenes + 2,
            name + " clamp and attempted sequence");
    require(clamped.scene->changes.firstEvaluation == false
        && clamped.scene->changes.visualChanged == (clamped.scene->fingerprints.scene != previous.scene),
        name + " failed viewport did not become scene history");
    auto clampedPlan = nativePlanner.build(
        std::make_shared<const runtime::EvaluatedScene>(*clamped.scene));
    require(bool(clampedPlan), name + " plan after failed viewport and clamp");
    auto expectedClamp = oracle.freshScene(9999, 512, 512);
    test::normalizeNativeOracleIdentity(expectedClamp, *certified.certificate, 91,
                                        clamped.scene->evaluationSequence);
    auto expectedClampPlan = oraclePlanner.build(
        std::make_shared<const runtime::EvaluatedScene>(std::move(expectedClamp)));
    require(bool(expectedClampPlan), name + " ordinary clamp plan");
    require(compare.difference(expectedClampPlan.plan, clampedPlan.plan).empty(),
            name + " full plan parity after invalid viewport and clamp");
    std::cout << "HISTORY " << name << " scenes=" << scenes << " plans=" << plans
              << " oracleDirectSamples=" << oracle.directSampleCount()
              << " oracleDirectParses=" << oracle.directParseCount() << '\n';
}

render::MotionRenderPlan makePlan(render::MotionRenderPlanner& planner,
                                  runtime::EvaluatedScene scene) {
    auto built = planner.build(std::make_shared<const runtime::EvaluatedScene>(std::move(scene)));
    require(bool(built), "native plan built");
    return std::move(built.plan);
}

struct OwnedPlanSnapshot final {
    runtime::EvaluatedScene scene;
    render::MotionRenderPlan plan;
    std::shared_ptr<avemotion::model::MotionAssetModel> model;
};

OwnedPlanSnapshot ownPlanValues(const runtime::EvaluatedScene& scene,
                                const render::MotionRenderPlan& plan) {
    require(bool(scene.assetModel) && bool(plan.sourceScene), "snapshot source model and plan");
    OwnedPlanSnapshot snapshot;
    snapshot.model = std::make_shared<avemotion::model::MotionAssetModel>(*scene.assetModel);
    snapshot.scene = scene;
    snapshot.scene.assetModel = snapshot.model;
    for (auto& draw : snapshot.scene.drawItems) {
        if (draw.canonicalGeometry) {
            const auto* record = snapshot.model->geometry(draw.modelGeometry);
            require(record && record->staticValue, "snapshot geometry static record");
            draw.canonicalGeometry = std::shared_ptr<const runtime::CanonicalGeometry>(
                snapshot.model, &*record->staticValue);
        }
        if (draw.canonicalPaint) {
            const auto* record = snapshot.model->paint(draw.modelPaint);
            require(record && record->staticValue, "snapshot paint static record");
            draw.canonicalPaint = std::shared_ptr<const runtime::CanonicalPaint>(
                snapshot.model, &*record->staticValue);
        }
    }
    requireAliases(snapshot.scene);
    snapshot.plan = plan;
    snapshot.plan.sourceScene = std::make_shared<const runtime::EvaluatedScene>(snapshot.scene);
    require(test::ExactSceneComparison{}.difference(snapshot.scene, scene).empty(),
            "value-owned snapshot matches original scene before advance");
    require(test::ExactRenderPlanComparison{}.difference(snapshot.plan, plan).empty(),
            "value-owned snapshot matches original plan before advance");
    return snapshot;
}

void twoStreamIdentity(const std::string& animatedJson, const std::string& staticJson) {
    test::ExactRenderPlanComparison compare;
    auto animated = runtime::detail::prepareNativeEllipseCertificate(animatedJson);
    require(bool(animated), "two-stream animated certificate");
    auto first = detail::NativeEllipseStream::create(animated.certificate, 101);
    auto second = detail::NativeEllipseStream::create(animated.certificate, 102);
    require(first && second, "two private streams created");
    render::MotionRenderPlanner firstPlanner, secondPlanner;
    auto firstScene = first.stream->emit(30, 512, 512);
    auto secondScene = second.stream->emit(60, 256, 256);
    require(firstScene && secondScene, "interleaved first scenes");
    requireAliases(*firstScene.scene);
    requireAliases(*secondScene.scene);
    auto firstPlan = makePlan(firstPlanner, *firstScene.scene);
    auto secondPlan = makePlan(secondPlanner, *secondScene.scene);
    require(firstPlan.drawItems.size() == 1 && secondPlan.drawItems.size() == 1,
            "animated plans each retain draw");
    const auto& firstDraw = firstPlan.drawItems.front();
    const auto& secondDraw = secondPlan.drawItems.front();
    require(firstDraw.geometry.scope == render::ResourceIdentityScope::Instance
        && secondDraw.geometry.scope == render::ResourceIdentityScope::Instance
        && firstDraw.geometry.instanceIdentity != secondDraw.geometry.instanceIdentity,
        "animated geometry keys have separate instance identities");
    require(firstDraw.paint.scope == render::ResourceIdentityScope::Asset
        && firstDraw.paint == secondDraw.paint,
        "static paint keys match across streams");
    const auto retainedFirst = ownPlanValues(*firstScene.scene, firstPlan);
    const auto retainedSecond = ownPlanValues(*secondScene.scene, secondPlan);
    // Corrupt only a separate test-owned clone. Its shallow twin shares the
    // pointee and misses the change; the independent baseline catches it.
    auto corrupted = ownPlanValues(*firstScene.scene, firstPlan);
    const auto shallowTwin = corrupted.plan;
    ++corrupted.model->logicalWidth;
    require(compare.difference(shallowTwin, corrupted.plan).empty(),
            "shallow twin demonstrates shared-model blind spot");
    require(!compare.difference(retainedFirst.plan, corrupted.plan).empty(),
            "independent retained snapshot detects model pointee corruption");
    auto canonicalCorrupted = ownPlanValues(*firstScene.scene, firstPlan);
    const auto canonicalShallowTwin = canonicalCorrupted.plan;
    const auto paintId = canonicalCorrupted.scene.drawItems.front().modelPaint;
    auto* paintRecord = &canonicalCorrupted.model->paints.at(paintId.index());
    require(paintRecord->staticValue.has_value(), "test-owned canonical paint exists");
    paintRecord->staticValue->paint.solid.r ^= 1U;
    require(compare.difference(canonicalShallowTwin, canonicalCorrupted.plan).empty(),
            "shallow twin demonstrates shared-canonical blind spot");
    require(!compare.difference(retainedFirst.plan, canonicalCorrupted.plan).empty()
        && !test::ExactSceneComparison{}.difference(
            retainedFirst.scene, *canonicalCorrupted.plan.sourceScene).empty(),
            "independent retained snapshot detects canonical pointee corruption");
    auto firstRepeat = first.stream->emit(30, 512, 512);
    auto secondAdvance = second.stream->emit(59, 384, 256);
    require(firstRepeat && secondAdvance, "interleaved repeat and reverse");
    auto repeatPlan = makePlan(firstPlanner, *firstRepeat.scene);
    auto advancePlan = makePlan(secondPlanner, *secondAdvance.scene);
    require(repeatPlan.geometryUpdates.empty() && repeatPlan.paintUpdates.empty()
        && !firstRepeat.scene->changes.visualChanged,
        "repeated frame has no resource updates or scene visual change");
    require(compare.difference(retainedFirst.plan, firstPlan).empty()
        && compare.difference(retainedSecond.plan, secondPlan).empty()
        && test::ExactSceneComparison{}.difference(retainedFirst.scene, *firstScene.scene).empty()
        && test::ExactSceneComparison{}.difference(retainedSecond.scene, *secondScene.scene).empty(),
        "value-owned retained scenes, canonical values and plans remain immutable");
    require(!advancePlan.drawItems.empty(), "reverse stream remains independently active");

    auto staticCertificate = runtime::detail::prepareNativeEllipseCertificate(staticJson);
    require(bool(staticCertificate), "static visible certificate");
    auto staticA = detail::NativeEllipseStream::create(staticCertificate.certificate, 101);
    auto staticB = detail::NativeEllipseStream::create(staticCertificate.certificate, 102);
    require(staticA && staticB, "static streams created");
    render::MotionRenderPlanner staticPlannerA, staticPlannerB;
    auto staticSceneA = staticA.stream->emit(30, 512, 512);
    auto staticSceneB = staticB.stream->emit(30, 512, 512);
    require(staticSceneA && staticSceneB, "static scenes emitted");
    auto staticPlanA = makePlan(staticPlannerA, *staticSceneA.scene);
    auto staticPlanB = makePlan(staticPlannerB, *staticSceneB.scene);
    require(staticPlanA.drawItems.size() == 1 && staticPlanB.drawItems.size() == 1,
            "static plans have draw");
    require(staticPlanA.drawItems.front().geometry.scope == render::ResourceIdentityScope::Asset
        && staticPlanA.drawItems.front().geometry == staticPlanB.drawItems.front().geometry,
        "static visible geometry keys share asset scope");
    auto staticOwnedA = ownPlanValues(*staticSceneA.scene, staticPlanA);
    auto staticOwnedB = ownPlanValues(*staticSceneB.scene, staticPlanB);
    auto geometryCorrupted = ownPlanValues(*staticSceneA.scene, staticPlanA);
    const auto geometryShallowTwin = geometryCorrupted.plan;
    const auto geometryId = geometryCorrupted.scene.drawItems.front().modelGeometry;
    auto* geometryRecord = &geometryCorrupted.model->geometries.at(geometryId.index());
    require(geometryRecord->staticValue && !geometryRecord->staticValue->path.points.empty(),
            "test-owned canonical geometry has a real point");
    geometryRecord->staticValue->path.points.front().x += 1.0F;
    require(compare.difference(geometryShallowTwin, geometryCorrupted.plan).empty(),
            "shallow twin demonstrates shared-geometry blind spot");
    require(!compare.difference(staticOwnedA.plan, geometryCorrupted.plan).empty()
        && !test::ExactSceneComparison{}.difference(
            staticOwnedA.scene, *geometryCorrupted.plan.sourceScene).empty(),
            "independent retained snapshot detects canonical geometry corruption");
    auto staticAdvanceA = staticA.stream->emit(31, 512, 512);
    auto staticAdvanceB = staticB.stream->emit(32, 384, 256);
    require(staticAdvanceA && staticAdvanceB, "static streams advance independently");
    auto advancedStaticPlanA = makePlan(staticPlannerA, *staticAdvanceA.scene);
    auto advancedStaticPlanB = makePlan(staticPlannerB, *staticAdvanceB.scene);
    require(bool(advancedStaticPlanA.sourceScene) && bool(advancedStaticPlanB.sourceScene),
            "advanced static plans retain source scenes");
    require(compare.difference(staticOwnedA.plan, staticPlanA).empty()
        && compare.difference(staticOwnedB.plan, staticPlanB).empty()
        && test::ExactSceneComparison{}.difference(staticOwnedA.scene, *staticSceneA.scene).empty()
        && test::ExactSceneComparison{}.difference(staticOwnedB.scene, *staticSceneB.scene).empty(),
            "static retained scene, canonical geometry and plan values remain immutable");
    std::cout << "TWO_STREAM interleaved=8 animatedGeometryDistinct=1 staticPaintShared=1"
              << " staticGeometryShared=1 valueOwnedRetainedPlansStable=4"
              << " corruptionDetected=model,paint,geometry\n";
}

using Plans = std::vector<render::MotionRenderPlan>;

Plans runOwnedSequence(const std::shared_ptr<const runtime::detail::NativeEllipseCertificate>& certificate,
                       std::uint64_t id, bool reverse) {
    auto made = detail::NativeEllipseStream::create(certificate, id);
    require(bool(made), "owned worker stream created");
    render::MotionRenderPlanner planner;
    Plans plans;
    plans.reserve(128);
    constexpr std::array<std::pair<std::size_t, std::size_t>, 4> viewports{{
        {512, 512}, {256, 256}, {384, 256}, {256, 384}}};
    for (std::size_t index = 0; index < 128; ++index) {
        const auto frame = reverse ? (60 - (index % 61)) : (index % 61);
        const auto [width, height] = viewports[index % viewports.size()];
        auto scene = made.stream->emit(frame, width, height);
        require(bool(scene), "owned worker emission");
        plans.push_back(makePlan(planner, std::move(*scene.scene)));
    }
    return plans;
}

void concurrentStreams(const std::string& json) {
    auto certified = runtime::detail::prepareNativeEllipseCertificate(json);
    require(bool(certified), "concurrency certificate");
    Plans actualA, actualB;
    std::exception_ptr errorA, errorB;
    std::thread first([&] {
        try { actualA = runOwnedSequence(certified.certificate, 101, false); }
        catch (...) { errorA = std::current_exception(); }
    });
    std::thread second([&] {
        try { actualB = runOwnedSequence(certified.certificate, 102, true); }
        catch (...) { errorB = std::current_exception(); }
    });
    first.join();
    second.join();
    if (errorA) std::rethrow_exception(errorA);
    if (errorB) std::rethrow_exception(errorB);
    const auto expectedA = runOwnedSequence(certified.certificate, 101, false);
    const auto expectedB = runOwnedSequence(certified.certificate, 102, true);
    require(actualA.size() == 128 && actualB.size() == 128, "two bounded worker outputs");
    test::ExactRenderPlanComparison compare;
    for (std::size_t index = 0; index < 128; ++index) {
        const auto firstDifference = compare.difference(expectedA[index], actualA[index]);
        const auto secondDifference = compare.difference(expectedB[index], actualB[index]);
        require(firstDifference.empty(), "concurrent first worker ordinal="
            + std::to_string(index) + " " + firstDifference);
        require(secondDifference.empty(), "concurrent second worker ordinal="
            + std::to_string(index) + " " + secondDifference);
    }
    std::cout << "CONCURRENT workerRequests=128+128 sequentialComparison=256\n";
}

void lifetime(const std::string& json) {
    std::weak_ptr<const runtime::detail::NativeEllipseCertificate> weakCertificate;
    std::weak_ptr<const runtime::Asset> weakAsset;
    std::weak_ptr<const avemotion::model::MotionAssetModel> weakModel;
    std::unique_ptr<detail::NativeEllipseStream> stream;
    {
        auto prepared = runtime::detail::prepareNativeEllipseCertificate(json);
        require(bool(prepared), "one-argument factory certificate");
        weakCertificate = prepared.certificate;
        weakAsset = prepared.certificate->asset;
        weakModel = prepared.certificate->model;
        auto made = detail::NativeEllipseStream::create(prepared.certificate, 103);
        require(bool(made), "lifetime stream created");
        stream = std::move(made.stream);
    }
    require(!weakCertificate.expired() && !weakAsset.expired() && !weakModel.expired(),
            "stream retains certificate and model after factory Runtime destruction");
    auto emitted = stream->emit(30, 512, 512);
    require(bool(emitted), "emit after factory Runtime and external certificate reset");
    requireAliases(*emitted.scene);
    auto scene = std::move(*emitted.scene);
    emitted.scene.reset();
    std::shared_ptr<const runtime::CanonicalPaint> paintAlias;
    std::shared_ptr<const runtime::CanonicalGeometry> geometryAlias;
    for (const auto& draw : scene.drawItems) {
        if (draw.canonicalPaint) paintAlias = draw.canonicalPaint;
        if (draw.canonicalGeometry) geometryAlias = draw.canonicalGeometry;
    }
    require(bool(paintAlias), "retained canonical paint alias");
    render::MotionRenderPlanner planner;
    auto plan = makePlan(planner, scene);
    stream.reset();
    require(weakCertificate.expired() && weakAsset.expired() && !weakModel.expired(),
            "certificate and Asset expire after stream, model stays with outputs");
    requireAliases(scene);
    requireAliases(*plan.sourceScene);
    scene = {};
    plan = {};
    require(!weakModel.expired(), "canonical alias retains model after scenes and plans");
    geometryAlias.reset();
    paintAlias.reset();
    require(weakModel.expired(), "model expires after final canonical alias");
    std::cout << "LIFETIME runtimeDestroyed=1 externalCertificateGone=1"
              << " streamDestroyed=1 retainedOutputsValid=1 finalWeakExpiry=1\n";
}

void referenceCounters(const std::string& json) {
    runtime::Runtime liveRuntime;
    auto prepared = runtime::detail::prepareNativeEllipseCertificate(liveRuntime, json);
    require(bool(prepared), "live Runtime certificate prepared");
    const auto count = prepared.certificate->slot.totalFrames;
    require(prepared.diagnosticsAfter.referenceModelSamples
                - prepared.diagnosticsBefore.referenceModelSamples == count
        && prepared.diagnosticsAfter.referenceSceneSamples
                - prepared.diagnosticsBefore.referenceSceneSamples == count,
        "preparation performed N model plus N scene samples");
    require(prepared.diagnosticsAfter.referenceCpuSessionsCreated
                == prepared.diagnosticsBefore.referenceCpuSessionsCreated,
        "cold preparation opened no CPU session");
    const auto before = liveRuntime.diagnostics();
    auto made = detail::NativeEllipseStream::create(prepared.certificate, 104);
    require(bool(made), "measured stream created");
    for (std::size_t frame = 0; frame < count; ++frame)
        require(bool(made.stream->emit(frame, 512, 512)), "measured native-only emission");
    const auto after = liveRuntime.diagnostics();
#define CHECK_COUNTER(Field) do { \
    const auto preparationDelta = prepared.diagnosticsAfter.Field - prepared.diagnosticsBefore.Field; \
    const auto nativeDelta = after.Field - before.Field; \
    std::cout << "COUNTER " #Field " preparation=" << preparationDelta \
              << " native=" << nativeDelta << '\n'; \
    require(after.Field == before.Field, "native-only changed " #Field); \
} while (false)
    CHECK_COUNTER(assetLoadAttempts);
    CHECK_COUNTER(assetLoadsSucceeded);
    CHECK_COUNTER(assetLoadsFailed);
    CHECK_COUNTER(tgsDecodeAttempts);
    CHECK_COUNTER(tgsDecodesSucceeded);
    CHECK_COUNTER(tgsDecodesFailed);
    CHECK_COUNTER(tgsCompressedBytes);
    CHECK_COUNTER(tgsJsonBytes);
    CHECK_COUNTER(instancesCreated);
    CHECK_COUNTER(referenceMetadataSessionsCreated);
    CHECK_COUNTER(referenceSceneSessionsCreated);
    CHECK_COUNTER(referenceModelSessionsCreated);
    CHECK_COUNTER(referenceCpuSessionsCreated);
    CHECK_COUNTER(referenceSceneSamples);
    CHECK_COUNTER(referenceModelSamples);
    CHECK_COUNTER(sceneEvaluations);
    CHECK_COUNTER(sceneEvaluationFailures);
    CHECK_COUNTER(cpuFramesRendered);
    CHECK_COUNTER(evaluatedLayers);
    CHECK_COUNTER(evaluatedDrawItems);
    CHECK_COUNTER(evaluatedMasks);
    CHECK_COUNTER(copiedPathPoints);
    CHECK_COUNTER(canonicalGeometriesCreated);
    CHECK_COUNTER(canonicalPaintsCreated);
    CHECK_COUNTER(canonicalResourceConflicts);
    CHECK_COUNTER(sceneEvaluationNanoseconds);
    CHECK_COUNTER(cpuRenderNanoseconds);
    CHECK_COUNTER(assetModelBuildAttempts);
    CHECK_COUNTER(assetModelBuildsSucceeded);
    CHECK_COUNTER(assetModelBuildsFailed);
    CHECK_COUNTER(assetModelLayers);
    CHECK_COUNTER(assetModelNodes);
    CHECK_COUNTER(assetModelStaticGeometries);
    CHECK_COUNTER(assetModelStaticPaints);
    CHECK_COUNTER(modelEvaluations);
    CHECK_COUNTER(playbackEvaluations);
#undef CHECK_COUNTER
    std::cout << "LIVE_COUNTER nativeEmissions=" << count << " allFieldsUnchanged=1\n";
}

void compareCpu(const std::vector<std::uint32_t>& expectedPixels, std::size_t frame,
                const runtime::CpuFrame& actual, const std::string& context) {
    require(actual.frameIndex == frame && actual.width == 512 && actual.height == 512
        && actual.strideBytes == 512 * sizeof(std::uint32_t)
        && expectedPixels == actual.argbPremultiplied,
        context + " ordinary CPU pixels/dimensions/stride");
}

void cpuIsolation(const std::string& json) {
    const auto sourceHash = avemotion::core::fnv1a64(
        std::as_bytes(std::span{json.data(), json.size()}));
    const auto key = "avemotion-asset-" + avemotion::core::formatHash(sourceHash);
    auto primaryObserver = rlottie::Animation::loadFromData(json, key, {}, true);
    require(bool(primaryObserver), "live primary cached source observer");
    const auto primarySource = rlottie::AveMotionAnimationAccess::model(*primaryObserver);
    require(bool(primarySource), "primary cached source lease");
    runtime::Runtime runtimeA;
    auto assetA = runtimeA.loadLottieJson(json, "cpu-primary");
    require(assetA && assetA.asset->metadata().sourceHash == sourceHash,
            "primary ordinary CPU asset uses observed source key");
    auto primary = runtimeA.createInstance(assetA.asset);
    require(bool(primary), "primary ordinary CPU instance");
    auto primaryFollowup = rlottie::Animation::loadFromData(json, key, {}, true);
    require(primaryFollowup
        && rlottie::AveMotionAnimationAccess::model(*primaryFollowup).get() == primarySource.get(),
        "retained cached source identity stable across primary Runtime load");
    auto oracleSeed = rlottie::Animation::loadFromData(
        json, "cpu-isolation-cache-disabled-seed", {}, false);
    require(bool(oracleSeed), "cache-disabled ordinary seed parse");
    const auto oracleSource = rlottie::AveMotionAnimationAccess::model(*oracleSeed);
    require(primarySource && oracleSource && primarySource.get() != oracleSource.get(),
            "CPU oracle parsed source distinct from primary cached source");
    auto oracleAnimation = rlottie::AveMotionAnimationAccess::fromModel(oracleSource);
    require(oracleAnimation && oracleAnimation.get() != primaryObserver.get()
        && rlottie::AveMotionAnimationAccess::model(*oracleAnimation).get() == oracleSource.get(),
        "CPU oracle animation owns distinct retained parsed source");
    constexpr std::array<std::size_t, 3> frames{0, 30, 60};
    std::array<std::vector<std::uint32_t>, 3> expectedPixels;
    std::array<runtime::CpuFrame, 3> beforeFrames;
    for (std::size_t index = 0; index < frames.size(); ++index) {
        auto& pixels = expectedPixels[index];
        pixels.resize(512 * 512);
        rlottie::Surface surface(pixels.data(), 512, 512, 512 * sizeof(std::uint32_t));
        oracleAnimation->renderSync(frames[index], surface);
        auto before = primary.instance->renderCpuFrame(frames[index], 512, 512);
        require(bool(before), "ordinary CPU before all native emissions");
        compareCpu(pixels, frames[index], before.frame,
                   "before frame=" + std::to_string(frames[index]));
        beforeFrames[index] = std::move(before.frame);
    }
    auto prepared = runtime::detail::prepareNativeEllipseCertificate(json);
    require(bool(prepared), "CPU isolation certificate");
    auto made = detail::NativeEllipseStream::create(prepared.certificate, 105);
    require(bool(made), "CPU isolation stream");
    for (std::size_t frameIndex = 0; frameIndex < frames.size(); ++frameIndex) {
        const auto frame = frames[frameIndex];
        for (std::size_t index = 0; index < 8; ++index)
            require(bool(made.stream->emit((frame + index) % 61, 512, 512)),
                    "interleaved native emission during CPU isolation");
        auto after = primary.instance->renderCpuFrame(frame, 512, 512);
        require(bool(after), "ordinary CPU after native emission");
        compareCpu(expectedPixels[frameIndex], frame, after.frame,
                   "after frame=" + std::to_string(frame));
        compareCpu(beforeFrames[frameIndex].argbPremultiplied, frame, after.frame,
                   "retained primary before/after frame=" + std::to_string(frame));
    }
    std::cout << "CPU_ISOLATION independentCacheDisabledSource=1 frames=0,30,60"
              << " nativeInterleaves=24 oraclePixelComparisons=6 retainedBeforeAfter=3"
              << " dimensionsAndStride=9\n";
}
}

int main() {
    try {
        std::ifstream input(std::string(AVEMOTION_FIXTURE_DIR) + "/telegram_sticker_basic.json");
        require(bool(input), "fixture readable");
        const std::string json(std::istreambuf_iterator<char>{input}, {});
        auto prepared = avemotion::runtime::detail::prepareNativeEllipseCertificate(json);
        require(bool(prepared), "certificate prepared");
        auto stream = avemotion::render::detail::NativeEllipseStream::create(prepared.certificate, 91);
        require(bool(stream), "stream created");
        auto emitted = stream.stream->emit(30, 512, 512);
        require(bool(emitted), "scene emitted");
        avemotion::render::MotionRenderPlanner planner;
        auto built = planner.build(std::make_shared<const avemotion::runtime::EvaluatedScene>(*emitted.scene));
        require(bool(built), "real plan built");
        comparatorMutationMatrix(built.plan);
        const auto assets = test::nativeEllipseTestAssets(json);
        for (const auto& name : {"animated", "static-visible", "activity", "edge-crossing"}) {
            const auto found = std::find_if(assets.begin(), assets.end(), [name](const auto& value) {
                return value.name == name;
            });
            require(found != assets.end(), std::string(name) + " asset available");
            planHistory(found->name, found->json);
        }
        const auto staticFound = std::find_if(assets.begin(), assets.end(), [](const auto& value) {
            return value.name == "static-visible";
        });
        require(staticFound != assets.end(), "static visible asset for two streams");
        twoStreamIdentity(json, staticFound->json);
        concurrentStreams(json);
        lifetime(json);
        referenceCounters(json);
        cpuIsolation(json);
        std::cout << "native ellipse stream lifecycle tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
