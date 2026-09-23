#include "avemotion/reference/UpstreamInfo.hpp"
#include "avemotion/runtime/Runtime.hpp"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;

namespace {
[[noreturn]] void fail(const std::string& message) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
}
void require(bool condition, const std::string& message) {
    if (!condition) fail(message);
}
std::string readText(const fs::path& path) {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) fail("unable to open " + path.string());
    return {std::istreambuf_iterator<char>{stream},
            std::istreambuf_iterator<char>{}};
}
bool near(double a, double b, double tolerance = 1.0e-7) {
    return std::abs(a - b) <= tolerance;
}
} // namespace

int main() {
    using namespace avemotion;
    const auto upstream = reference::selectedUpstream();
    const auto json = readText(fs::path{AVEMOTION_CORPUS_DIR} / "StickAndBall.json");

    runtime::Runtime runtime;
    auto loaded = runtime.loadLottieJson(json, "playback");
    require(static_cast<bool>(loaded), "asset load failed");
    auto created = runtime.createInstance(loaded.asset);
    require(static_cast<bool>(created), "instance creation failed");
    auto& instance = *created.instance;

    const auto zero = runtime::MotionTime::fromNanoseconds(0);
    const auto duration = runtime::MotionTime::fromSeconds(
        loaded.asset->metadata().durationSeconds);
    const auto quarter = runtime::MotionTime::fromNanoseconds(
        duration.nanoseconds / 4);
    const auto half = runtime::MotionTime::fromNanoseconds(
        duration.nanoseconds / 2);
    const auto threeQuarter = runtime::MotionTime::fromNanoseconds(
        (duration.nanoseconds * 3) / 4);

    auto snapshot = instance.playbackSnapshot(zero);
    require(snapshot.status == runtime::PlaybackStatus::Stopped
                && near(snapshot.normalizedPosition, 0.0),
            "default playback state is wrong");

    instance.play(zero);
    snapshot = instance.playbackSnapshot(quarter);
    require(snapshot.status == runtime::PlaybackStatus::Playing
                && near(snapshot.normalizedPosition, 0.25, 1.0e-6),
            "absolute-time forward playback is wrong");

    instance.pause(quarter);
    const auto paused = instance.playbackSnapshot(threeQuarter);
    require(paused.status == runtime::PlaybackStatus::Paused
                && near(paused.normalizedPosition, 0.25, 1.0e-6),
            "pause did not freeze logical position");

    instance.resume(half);
    const auto resumed = instance.playbackSnapshot(threeQuarter);
    require(near(resumed.normalizedPosition, 0.5, 1.0e-6),
            "resume accumulated paused wall time");

    instance.seekNormalized(0.6, threeQuarter);
    require(near(instance.playbackSnapshot(threeQuarter).normalizedPosition, 0.6),
            "seek did not re-anchor playback");
    instance.setDirection(runtime::PlaybackDirection::Reverse, threeQuarter);
    require(near(instance.playbackSnapshot(threeQuarter).normalizedPosition, 0.6),
            "direction change caused a visual jump");
    const auto later = runtime::MotionTime::fromNanoseconds(
        threeQuarter.nanoseconds + duration.nanoseconds / 10);
    require(near(instance.playbackSnapshot(later).normalizedPosition, 0.5, 1.0e-6),
            "reverse playback mapping is wrong");

    instance.stop();
    instance.setDirection(runtime::PlaybackDirection::Reverse, zero);
    instance.play(zero);
    const auto reverseStart = instance.playbackSnapshot(zero);
    require(near(reverseStart.normalizedPosition, 1.0),
            "reverse playback did not begin at the terminal sample: "
                + std::to_string(reverseStart.normalizedPosition));

    instance.setControlledProgress(0.375);
    snapshot = instance.playbackSnapshot(duration);
    require(snapshot.status == runtime::PlaybackStatus::Controlled
                && near(snapshot.normalizedPosition, 0.375),
            "controlled progress depends on presentation time");
    instance.resume(zero);
    require(near(instance.playbackSnapshot(zero).normalizedPosition, 0.375),
            "controlled-to-playing transition jumped");

    instance.seekNormalized(0.0, zero);
    instance.setDirection(runtime::PlaybackDirection::Forward, zero);
    require(instance.setPlaybackRate(2.0, zero),
            "valid playback rate was rejected");
    require(!instance.setPlaybackRate(0.0, zero),
            "zero playback rate was accepted");
    instance.play(zero);
    require(near(instance.playbackSnapshot(quarter).normalizedPosition, 0.5, 1.0e-6),
            "playback rate mapping is wrong");

    instance.stop();
    instance.setLoopMode(runtime::PlaybackLoopMode::Once);
    instance.setDirection(runtime::PlaybackDirection::Forward, zero);
    require(instance.setPlaybackRate(1.0, zero), "normal playback rate rejected");
    instance.play(zero);
    const auto beyond = runtime::MotionTime::fromNanoseconds(
        duration.nanoseconds + duration.nanoseconds / 5);
    const auto completed = instance.playbackSnapshot(beyond);
    require(completed.completed && near(completed.normalizedPosition, 1.0),
            "once playback did not report completion");

    if (upstream.variant == "telegram") {
        auto evaluated = instance.evaluateAt(beyond, 128U, 128U);
        require(static_cast<bool>(evaluated),
                "host-driven playback evaluation failed: " + evaluated.error.message);
        require(evaluated.scene.assetModelApplied,
                "playback evaluation bypassed immutable model");
        const auto held = instance.playbackSnapshot(beyond);
        require(held.status == runtime::PlaybackStatus::Holding
                    && held.completed,
                "completed playback did not enter holding state");

        instance.seekNormalized(0.5, beyond);
        auto directA = instance.evaluateAt(beyond, 128U, 128U);
        auto directB = instance.evaluateAt(beyond, 128U, 128U);
        require(directA && directB,
                "repeated exact-time evaluation failed");
        require(directA.scene.fingerprints.scene == directB.scene.fingerprints.scene,
                "repeated exact-time evaluation is history dependent");
    }

    std::cout << "AveMotion playback tests passed using " << upstream.variant << '\n';
    return EXIT_SUCCESS;
}
