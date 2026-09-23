#include "support/GoldenVariance.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

[[noreturn]] void fail(const char* message) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

void require(bool condition, const char* message) {
    if (!condition) fail(message);
}

} // namespace

int main() {
    using avemotion::testsupport::GoldenManifestKind;
    using avemotion::testsupport::compareGoldenRows;

    const std::string sceneExpected =
        "telegram\tpolystar_line_clockwise_trim.json\t8d8cd4aacce0760d\t"
        "p100\t149\t128x128\t0cca4a4d4395c248\tcf63ab9896bc758f\t"
        "c52135375f4b4fd9\t069461bce8601f2c\t2\t2\t1\t0\t0\t33\t32\t0";
    const std::string sceneMsvc =
        "telegram\tpolystar_line_clockwise_trim.json\t8d8cd4aacce0760d\t"
        "p100\t149\t128x128\td16b938d33790784\tcf63ab9896bc758f\t"
        "4c0fb2275b3068ab\t069461bce8601f2c\t2\t2\t1\t0\t0\t33\t32\t0";
    require(compareGoldenRows(
        GoldenManifestKind::Scene, sceneExpected, sceneExpected, false).matches,
        "exact scene row must pass");
    const auto sceneAccepted = compareGoldenRows(
        GoldenManifestKind::Scene, sceneExpected, sceneMsvc, true);
    require(sceneAccepted.matches && sceneAccepted.acceptedKnownVariance,
        "known MSVC scene variance must pass");
    require(!compareGoldenRows(
        GoldenManifestKind::Scene, sceneExpected, sceneMsvc, false).matches,
        "known variance must fail when policy is disabled");

    auto unknownSceneFingerprint = sceneMsvc;
    const auto capturedSceneHash = unknownSceneFingerprint.find("d16b938d33790784");
    require(capturedSceneHash != std::string::npos,
        "captured scene hash must exist in the fixture");
    unknownSceneFingerprint.replace(
        capturedSceneHash, 16U, "ffffffffffffffff");
    require(!compareGoldenRows(
        GoldenManifestKind::Scene,
        sceneExpected,
        unknownSceneFingerprint,
        true).matches,
        "an uncaptured hash must not be waived even in an approved field");

    auto badScene = sceneMsvc;
    badScene.replace(badScene.rfind("\t32\t0"), 3U, "\t31");
    require(!compareGoldenRows(
        GoldenManifestKind::Scene, sceneExpected, badScene, true).matches,
        "scene statistics must never be waived");

    const std::string planExpected =
        "telegram\tpolystar_line_clockwise_trim.json\t8d8cd4aacce0760d\t"
        "p100\t149\t128x128\t4e7835d97cd1de98\teb44aa0694aad3ba\t"
        "f392889bad747440\t75f45d666045d802\t83bafa6dc131d21b\t"
        "1\t1\t1\t0\t0\t1\t0\t1\t0\t1";
    const std::string planMsvc =
        "telegram\tpolystar_line_clockwise_trim.json\t8d8cd4aacce0760d\t"
        "p100\t149\t128x128\t43cc77e5cb56f0e1\teb44aa0694aad3ba\t"
        "4992a725d8c4bd2c\t75f45d666045d802\t38cbdf9a188f397b\t"
        "1\t1\t1\t0\t0\t1\t0\t1\t0\t1";
    const auto planAccepted = compareGoldenRows(
        GoldenManifestKind::Plan, planExpected, planMsvc, true);
    require(planAccepted.matches && planAccepted.acceptedKnownVariance,
        "known MSVC plan variance must pass");

    auto unknownPlanFingerprint = planMsvc;
    const auto capturedPlanHash = unknownPlanFingerprint.find("43cc77e5cb56f0e1");
    require(capturedPlanHash != std::string::npos,
        "captured plan hash must exist in the fixture");
    unknownPlanFingerprint.replace(
        capturedPlanHash, 16U, "eeeeeeeeeeeeeeee");
    require(!compareGoldenRows(
        GoldenManifestKind::Plan,
        planExpected,
        unknownPlanFingerprint,
        true).matches,
        "an uncaptured plan hash must not be waived");

    std::cout << "AveMotion reference golden variance tests passed\n";
    return EXIT_SUCCESS;
}
