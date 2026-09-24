#include "NativeEllipseStream.hpp"
#include "support/ExactSceneComparison.hpp"
#include "support/NativeEllipseOracle.hpp"
#include "support/NativeEllipseTestAssets.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
void require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}

std::string floatBits(float value) {
    return std::to_string(std::bit_cast<std::uint32_t>(value));
}

void compareFrame(avemotion::test::NativeEllipseOracle& oracle,
                  avemotion::render::detail::NativeEllipseStream& stream,
                  const avemotion::runtime::detail::NativeEllipseCertificate& certificate,
                  const std::string& caseName, const std::string& order,
                  std::size_t frame, std::size_t width, std::size_t height,
                  std::size_t ordinal) {
    auto expected = oracle.freshScene(frame, width, height);
    require(expected.assetHandle == avemotion::runtime::AssetHandle{700001, 1}
        && expected.assetModel.get() == oracle.model().get(),
        caseName + " independent ordinary model owner");
    auto actual = stream.emit(frame, width, height);
    require(static_cast<bool>(actual), caseName + " " + order + " emit failed frame="
        + std::to_string(frame) + " viewport=" + std::to_string(width) + "x"
        + std::to_string(height) + " ordinal=" + std::to_string(ordinal)
        + " status=" + std::to_string(static_cast<int>(actual.code)) + " " + actual.message);
    require(actual.scene->assetHandle == certificate.asset->handle()
        && !actual.scene->instanceHandle.valid() && actual.scene->instanceId == 41
        && actual.scene->evaluationSequence == ordinal
        && actual.scene->assetModel.get() == certificate.model.get(),
        caseName + " private scene identity/sequence");
    require(actual.scene->changes.firstEvaluation == expected.changes.firstEvaluation
        && actual.scene->changes.topologyChanged == expected.changes.topologyChanged
        && actual.scene->changes.geometryChanged == expected.changes.geometryChanged
        && actual.scene->changes.paintChanged == expected.changes.paintChanged
        && actual.scene->changes.visualChanged == expected.changes.visualChanged,
        caseName + " scene history differs frame=" + std::to_string(frame)
            + " ordinal=" + std::to_string(ordinal));
    for (const auto& item : actual.scene->drawItems) {
        if (item.canonicalGeometry) {
            const auto* record = actual.scene->assetModel->geometry(item.modelGeometry);
            require(record && record->staticValue
                && item.canonicalGeometry.get() == &*record->staticValue
                && !item.canonicalGeometry.owner_before(actual.scene->assetModel)
                && !actual.scene->assetModel.owner_before(item.canonicalGeometry),
                caseName + " canonical geometry aliases frozen model");
        }
        if (item.canonicalPaint) {
            const auto* record = actual.scene->assetModel->paint(item.modelPaint);
            require(record && record->staticValue
                && item.canonicalPaint.get() == &*record->staticValue
                && !item.canonicalPaint.owner_before(actual.scene->assetModel)
                && !actual.scene->assetModel.owner_before(item.canonicalPaint),
                caseName + " canonical paint aliases frozen model");
        }
    }
    avemotion::test::normalizeNativeOracleIdentity(expected, certificate, 41, ordinal);
    const auto difference = avemotion::test::ExactSceneComparison{}.difference(expected, *actual.scene);
    if (!difference.empty()) {
        std::ostringstream message;
        message << caseName << " " << order << " frame=" << frame << " viewport="
                << width << 'x' << height << " ordinal=" << ordinal << " " << difference;
        if (!expected.drawItems.empty() && !actual.scene->drawItems.empty()) {
            const auto& e = expected.drawItems.front();
            const auto& a = actual.scene->drawItems.front();
            message << " transformBits=["
                << floatBits(e.localToViewport.m11) << '/' << floatBits(a.localToViewport.m11) << ','
                << floatBits(e.localToViewport.m12) << '/' << floatBits(a.localToViewport.m12) << ','
                << floatBits(e.localToViewport.m21) << '/' << floatBits(a.localToViewport.m21) << ','
                << floatBits(e.localToViewport.m22) << '/' << floatBits(a.localToViewport.m22) << ','
                << floatBits(e.localToViewport.dx) << '/' << floatBits(a.localToViewport.dx) << ','
                << floatBits(e.localToViewport.dy) << '/' << floatBits(a.localToViewport.dy) << ']';
            if (!e.path.points.empty() && !a.path.points.empty()) {
                message << " finalFirstPointBits=" << floatBits(e.path.points.front().x)
                    << '/' << floatBits(a.path.points.front().x) << ','
                    << floatBits(e.path.points.front().y) << '/'
                    << floatBits(a.path.points.front().y);
            }
            if (!e.localPath.points.empty() && !a.localPath.points.empty()) {
                message << " localFirstPointBits=" << floatBits(e.localPath.points.front().x)
                    << '/' << floatBits(a.localPath.points.front().x) << ','
                    << floatBits(e.localPath.points.front().y) << '/'
                    << floatBits(a.localPath.points.front().y);
            }
        }
        throw std::runtime_error(message.str());
    }
    for (const auto& item : actual.scene->drawItems) {
        const auto finite = [](float value) { return std::isfinite(value); };
        require(finite(item.localToViewport.m11) && finite(item.localToViewport.m12)
            && finite(item.localToViewport.m21) && finite(item.localToViewport.m22)
            && finite(item.localToViewport.dx) && finite(item.localToViewport.dy),
            caseName + " nonfinite transform");
        for (auto point : item.path.points)
            require(finite(point.x) && finite(point.y), caseName + " nonfinite path");
        for (auto point : item.localPath.points)
            require(finite(point.x) && finite(point.y), caseName + " nonfinite local path");
    }
}
}

int main() {
    try {
        std::ifstream input(std::string(AVEMOTION_FIXTURE_DIR) + "/telegram_sticker_basic.json");
        require(bool(input), "baseline fixture readable");
        const std::string json(std::istreambuf_iterator<char>{input}, {});
        auto prepared = avemotion::runtime::detail::prepareNativeEllipseCertificate(json);
        require(static_cast<bool>(prepared), "baseline certificate prerequisite");
        auto created = avemotion::render::detail::NativeEllipseStream::create(
            prepared.certificate, 41);
        require(static_cast<bool>(created), "native stream created");
        auto emitted = created.stream->emit(30, 512, 512);
        require(static_cast<bool>(emitted), "native baseline scene emitted");
        require(emitted.scene->assetModelApplied, "native scene applies frozen model");
        avemotion::test::NativeEllipseOracle baselineOracle(json);
        require(baselineOracle.model().get() != prepared.certificate->model.get(),
                "independent ordinary frozen model");
        auto baselineStream = avemotion::render::detail::NativeEllipseStream::create(
            prepared.certificate, 41);
        require(static_cast<bool>(baselineStream), "baseline comparison stream");
        compareFrame(baselineOracle, *baselineStream.stream, *prepared.certificate,
                     "animated", "baseline", 30, 512, 512, 1);
        require(!avemotion::render::detail::NativeEllipseStream::create(nullptr, 41),
                "null certificate rejected");
        require(!avemotion::render::detail::NativeEllipseStream::create(prepared.certificate, 0),
                "zero identity rejected");
        auto boundary = avemotion::render::detail::NativeEllipseStream::create(
            prepared.certificate, 41);
        require(static_cast<bool>(boundary), "boundary stream created");
        auto invalid = boundary.stream->emit(0, 0, 512);
        require(!invalid && !invalid.scene
            && invalid.code == avemotion::render::detail::NativeEllipseFrameCode::InvalidViewport,
            "failed viewport has no scene");
        auto valid = boundary.stream->emit(std::numeric_limits<std::size_t>::max(), 512, 512);
        require(valid && valid.scene->frameIndex == 60
            && valid.scene->evaluationSequence == 2
            && valid.scene->changes.firstEvaluation,
            "clamp, attempted sequence and unpublished failure history");
        auto failedHeight = boundary.stream->emit(60, 512, 0);
        require(!failedHeight && !failedHeight.scene
            && failedHeight.code == avemotion::render::detail::NativeEllipseFrameCode::InvalidViewport,
            "zero height rejected without scene");
        auto repeat = boundary.stream->emit(60, 512, 512);
        require(repeat && repeat.scene->evaluationSequence == 4
            && !repeat.scene->changes.firstEvaluation
            && !repeat.scene->changes.geometryChanged
            && !repeat.scene->changes.visualChanged,
            "failed attempt skips geometry history and valid repeat is unchanged");
        avemotion::runtime::Runtime liveRuntime;
        auto measured = avemotion::runtime::detail::prepareNativeEllipseCertificate(liveRuntime, json);
        require(static_cast<bool>(measured), "measured certificate prerequisite");
        const auto countersBefore = liveRuntime.diagnostics();
        auto measuredStream = avemotion::render::detail::NativeEllipseStream::create(
            measured.certificate, 99);
        require(static_cast<bool>(measuredStream), "measured native stream");
        for (std::size_t frame = 0; frame <= 60; ++frame)
            require(static_cast<bool>(measuredStream.stream->emit(frame, 512, 512)),
                    "measured native-only emission");
        const auto countersAfter = liveRuntime.diagnostics();
        require(countersBefore.referenceMetadataSessionsCreated == countersAfter.referenceMetadataSessionsCreated
            && countersBefore.referenceSceneSessionsCreated == countersAfter.referenceSceneSessionsCreated
            && countersBefore.referenceModelSessionsCreated == countersAfter.referenceModelSessionsCreated
            && countersBefore.referenceCpuSessionsCreated == countersAfter.referenceCpuSessionsCreated
            && countersBefore.referenceSceneSamples == countersAfter.referenceSceneSamples
            && countersBefore.referenceModelSamples == countersAfter.referenceModelSamples
            && countersBefore.sceneEvaluations == countersAfter.sceneEvaluations
            && countersBefore.modelEvaluations == countersAfter.modelEvaluations
            && countersBefore.cpuFramesRendered == countersAfter.cpuFramesRendered,
            "native-only emission leaves live Runtime reference counters unchanged");
        const std::array<std::pair<std::size_t, std::size_t>, 4> viewports{{
            {512, 512}, {256, 256}, {384, 256}, {256, 384}}};
        std::size_t comparisons = 0, directSamples = 0, directParses = 0;
        for (const auto& testAsset : avemotion::test::nativeEllipseTestAssets(json)) {
            auto certified = avemotion::runtime::detail::prepareNativeEllipseCertificate(testAsset.json);
            require(static_cast<bool>(certified), testAsset.name + " C certification failed code="
                + std::to_string(static_cast<int>(certified.code)) + " admission="
                + std::to_string(static_cast<int>(certified.admission.code)) + " binding="
                + std::to_string(static_cast<int>(certified.bindingCode)) + " scan="
                + std::to_string(static_cast<int>(certified.scanCode)));
            std::size_t caseComparisons = 0;
            for (auto [width, height] : viewports) {
                avemotion::test::NativeEllipseOracle oracle(testAsset.json);
                require(oracle.model().get() != certified.certificate->model.get(),
                    testAsset.name + " independent model pointer");
                auto candidate = avemotion::render::detail::NativeEllipseStream::create(
                    certified.certificate, 41);
                require(static_cast<bool>(candidate), testAsset.name + " stream creation");
                std::size_t ordinal = 0;
                for (std::size_t frame = 0; frame <= 60; ++frame) {
                    compareFrame(oracle, *candidate.stream, *certified.certificate,
                        testAsset.name, "forward", frame, width, height, ++ordinal);
                    ++caseComparisons;
                }
                std::cout << "ORDER " << testAsset.name << ' ' << width << 'x' << height
                          << " forward planned=61 executed=61\n";
                for (std::size_t frame = 61; frame-- > 0;) {
                    compareFrame(oracle, *candidate.stream, *certified.certificate,
                        testAsset.name, "reverse", frame, width, height, ++ordinal);
                    ++caseComparisons;
                }
                std::cout << "ORDER " << testAsset.name << ' ' << width << 'x' << height
                          << " reverse planned=61 executed=61\n";
                for (auto frame : {0U, 0U, 60U, 60U, 9U, 10U, 19U, 20U}) {
                    compareFrame(oracle, *candidate.stream, *certified.certificate,
                        testAsset.name, "boundary", frame, width, height, ++ordinal);
                    ++caseComparisons;
                }
                std::cout << "ORDER " << testAsset.name << ' ' << width << 'x' << height
                          << " boundary planned=8 executed=8\n";
                require(ordinal == 130, testAsset.name + " viewport planned count");
                directSamples += oracle.directSampleCount();
                directParses += oracle.directParseCount();
            }
            avemotion::test::NativeEllipseOracle alternatingOracle(testAsset.json);
            auto alternating = avemotion::render::detail::NativeEllipseStream::create(
                certified.certificate, 41);
            require(static_cast<bool>(alternating), testAsset.name + " alternating stream");
            for (std::size_t index = 0; index < 8; ++index) {
                const auto [width, height] = viewports[index % viewports.size()];
                compareFrame(alternatingOracle, *alternating.stream, *certified.certificate,
                    testAsset.name, "alternating", 30, width, height, index + 1);
                ++caseComparisons;
            }
            std::cout << "ORDER " << testAsset.name
                      << " alternating planned=8 executed=8\n";
            directSamples += alternatingOracle.directSampleCount();
            directParses += alternatingOracle.directParseCount();
            require(caseComparisons == 528, testAsset.name + " planned case count");
            comparisons += caseComparisons;
            std::cout << "CASE " << testAsset.name << " certified=1 planned=528 executed="
                      << caseComparisons << '\n';
        }
        require(comparisons == 7920, "full matrix comparison count");
        std::cout << "MATRIX planned=7920 executed=" << comparisons
                  << " ordinaryDirectSamples=" << directSamples
                  << " oracleDirectParses=" << directParses << '\n';
        std::cout << "native ellipse stream tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
