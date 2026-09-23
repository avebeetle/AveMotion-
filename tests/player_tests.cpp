#include "avemotion/player/Player.hpp"
#include "avemotion/runtime/Runtime.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

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

bool near(double left, double right, double tolerance = 1.0e-6) {
    return std::abs(left - right) <= tolerance;
}

avemotion::runtime::MotionTime add(
    avemotion::runtime::MotionTime time,
    std::int64_t nanoseconds) {
    return avemotion::runtime::MotionTime::fromNanoseconds(
        time.nanoseconds + nanoseconds);
}

struct HostRecorder final {
    std::uint64_t frameRequests = 0U;
    std::uint64_t schedules = 0U;
    std::uint64_t cancels = 0U;
    std::optional<avemotion::runtime::MotionTime> deadline;

    static void requestFrame(void* context) noexcept {
        auto& self = *static_cast<HostRecorder*>(context);
        ++self.frameRequests;
    }

    static void scheduleWakeup(
        void* context,
        avemotion::runtime::MotionTime value) noexcept {
        auto& self = *static_cast<HostRecorder*>(context);
        ++self.schedules;
        self.deadline = value;
    }

    static void cancelWakeup(void* context) noexcept {
        auto& self = *static_cast<HostRecorder*>(context);
        ++self.cancels;
        self.deadline.reset();
    }
};

std::shared_ptr<avemotion::runtime::Instance> createSharedInstance(
    avemotion::runtime::Runtime& runtime,
    const std::shared_ptr<const avemotion::runtime::Asset>& asset) {
    auto created = runtime.createInstance(asset);
    require(static_cast<bool>(created),
        "instance creation failed: " + created.error.message);
    return std::shared_ptr<avemotion::runtime::Instance>{
        std::move(created.instance)};
}

const avemotion::player::ScheduledFrame* findFrame(
    std::span<const avemotion::player::ScheduledFrame> frames,
    avemotion::player::PlayerHandle handle) {
    const auto iterator = std::find_if(
        frames.begin(),
        frames.end(),
        [handle](const auto& frame) { return frame.handle == handle; });
    return iterator == frames.end() ? nullptr : &*iterator;
}

} // namespace

int main() {
    using namespace avemotion;

    runtime::Runtime runtime;
    const auto json = readText(
        fs::path{AVEMOTION_CORPUS_DIR} / "StickAndBall.json");
    auto loaded = runtime.loadLottieJson(json, "player-scheduler");
    require(static_cast<bool>(loaded),
        "asset load failed: " + loaded.error.message);

    auto first = createSharedInstance(runtime, loaded.asset);
    auto second = createSharedInstance(runtime, loaded.asset);
    auto frozen = createSharedInstance(runtime, loaded.asset);

    HostRecorder host;
    player::Player player{{
        .userData = &host,
        .requestFrame = &HostRecorder::requestFrame,
        .scheduleWakeup = &HostRecorder::scheduleWakeup,
        .cancelWakeup = &HostRecorder::cancelWakeup,
    }};

    const auto zero = runtime::MotionTime::fromNanoseconds(0);
    const auto duration = runtime::MotionTime::fromSeconds(
        loaded.asset->metadata().durationSeconds);
    require(duration.nanoseconds > 0, "asset duration is invalid");

    const auto firstAdded = player.addInstance(first, zero, {
        .maximumPresentationRate = 60.0,
        .visible = true,
        .hiddenTimePolicy = player::HiddenTimePolicy::KeepUp,
    });
    const auto secondAdded = player.addInstance(second, zero, {
        .maximumPresentationRate = 60.0,
        .visible = true,
        .hiddenTimePolicy = player::HiddenTimePolicy::KeepUp,
    });
    const auto frozenAdded = player.addInstance(frozen, zero, {
        .maximumPresentationRate = 60.0,
        .visible = true,
        .hiddenTimePolicy = player::HiddenTimePolicy::Freeze,
    });
    require(firstAdded && secondAdded && frozenAdded,
        "player registration failed");
    require(host.frameRequests == 1U,
        "multiple registrations did not coalesce into one frame request");

    auto tick = player.tick(zero);
    require(tick.frames.size() == 3U,
        "first tick did not return all visible first frames");
    for (const auto& frame : tick.frames) {
        require(player::hasReason(frame.reasons, player::FrameReason::FirstFrame),
            "first frame reason is missing");
    }
    require(!tick.nextDeadline.has_value(),
        "stopped instances unexpectedly scheduled a wakeup");

    require(player.setPlaybackRate(secondAdded.handle, 0.5, zero),
        "second playback rate update failed");
    require(player.play(firstAdded.handle, zero), "first play failed");
    require(player.play(secondAdded.handle, zero), "second play failed");
    require(player.play(frozenAdded.handle, zero), "frozen play failed");
    require(host.frameRequests == 2U,
        "play mutations were not coalesced into one request");

    tick = player.tick(zero);
    require(tick.frames.size() == 3U,
        "play tick did not return all changed entries");
    require(tick.nextDeadline.has_value(),
        "playing instances did not schedule a shared deadline");
    const auto firstDeadline = *tick.nextDeadline;
    require(firstDeadline.nanoseconds > zero.nanoseconds,
        "first deadline is not in the future");
    require(host.deadline == tick.nextDeadline,
        "host did not receive the nearest deadline");

    tick = player.tick(add(firstDeadline, -1));
    require(tick.frames.empty(),
        "scheduler returned a frame before the nearest deadline");

    tick = player.tick(firstDeadline);
    require(tick.frames.size() >= 1U,
        "nearest deadline did not produce a frame");
    const auto* firstDue = findFrame(tick.frames, firstAdded.handle);
    require(firstDue != nullptr
            && player::hasReason(
                firstDue->reasons,
                player::FrameReason::TimelineAdvanced),
        "first instance was not selected at its cadence deadline");
    require(findFrame(tick.frames, secondAdded.handle) == nullptr,
        "half-rate instance advanced at the full-rate deadline");

    const auto sourceRate = loaded.asset->metadata().frameRate;
    require(sourceRate > 0.0, "asset frame rate is invalid");
    const auto interval = static_cast<std::int64_t>(std::llround(
        1'000'000'000.0 / sourceRate));
    const auto late = add(firstDeadline, interval * 12);
    tick = player.tick(late);
    require(tick.frames.size() <= 3U,
        "late update accumulated an unbounded frame queue");
    require(tick.skippedDeadlines > 0U,
        "late update did not account for skipped deadlines");

    require(player.pause(firstAdded.handle, late), "pause failed");
    tick = player.tick(late);
    firstDue = findFrame(tick.frames, firstAdded.handle);
    require(firstDue != nullptr
            && player::hasReason(
                firstDue->reasons,
                player::FrameReason::PlaybackChanged),
        "pause did not produce one state-change frame");

    // Direct runtime mutations are still detected through the playback
    // revision, even when a host bypasses the Player convenience wrappers.
    second->seekNormalized(0.42, late);
    tick = player.tick(late);
    const auto* secondDue = findFrame(tick.frames, secondAdded.handle);
    require(secondDue != nullptr
            && player::hasReason(
                secondDue->reasons,
                player::FrameReason::PlaybackChanged),
        "direct instance mutation was not detected");

    const auto keepUpBefore = second->playbackSnapshot(late).normalizedPosition;
    require(player.setVisible(secondAdded.handle, false, late),
        "hiding keep-up instance failed");
    tick = player.tick(late);
    secondDue = findFrame(tick.frames, secondAdded.handle);
    require(secondDue != nullptr && !secondDue->visible
            && player::hasReason(
                secondDue->reasons,
                player::FrameReason::VisibilityChanged),
        "hidden entry did not produce one clear-presentation event");

    const auto muchLater = add(late, duration.nanoseconds / 3);
    require(player.setVisible(secondAdded.handle, true, muchLater),
        "showing keep-up instance failed");
    const auto keepUpAfter = second->playbackSnapshot(muchLater).normalizedPosition;
    require(!near(keepUpBefore, keepUpAfter),
        "KeepUp visibility policy froze logical time");
    tick = player.tick(muchLater);
    secondDue = findFrame(tick.frames, secondAdded.handle);
    require(secondDue != nullptr && secondDue->visible,
        "shown keep-up entry did not request an immediate frame");

    const auto freezeAt = add(muchLater, duration.nanoseconds / 10);
    const auto frozenBefore = frozen->playbackSnapshot(freezeAt).normalizedPosition;
    require(player.setVisible(frozenAdded.handle, false, freezeAt),
        "hiding freeze-policy instance failed");
    require(frozen->playbackSnapshot(freezeAt).status
            == runtime::PlaybackStatus::Paused,
        "Freeze policy did not automatically pause a playing instance");
    const auto freezeLater = add(freezeAt, duration.nanoseconds / 2);
    require(near(
        frozen->playbackSnapshot(freezeLater).normalizedPosition,
        frozenBefore),
        "Freeze policy allowed hidden logical time to advance");
    require(player.setVisible(frozenAdded.handle, true, freezeLater),
        "showing freeze-policy instance failed");
    require(frozen->playbackSnapshot(freezeLater).status
            == runtime::PlaybackStatus::Playing,
        "Freeze policy did not resume an automatically paused instance");
    require(near(
        frozen->playbackSnapshot(freezeLater).normalizedPosition,
        frozenBefore),
        "Freeze resume caused a visual jump");
    tick = player.tick(freezeLater);
    const auto* frozenDue = findFrame(tick.frames, frozenAdded.handle);
    require(frozenDue != nullptr && frozenDue->visible,
        "shown freeze-policy entry did not request an immediate frame");

    // Explicit invalidations are coalesced until the next tick.
    const auto requestsBeforeInvalidation = host.frameRequests;
    require(player.invalidate(firstAdded.handle), "first invalidate failed");
    require(player.invalidate(firstAdded.handle), "second invalidate failed");
    require(host.frameRequests == requestsBeforeInvalidation + 1U,
        "explicit invalidations were not coalesced");
    tick = player.tick(freezeLater);
    firstDue = findFrame(tick.frames, firstAdded.handle);
    require(firstDue != nullptr
            && player::hasReason(
                firstDue->reasons,
                player::FrameReason::ExplicitInvalidation),
        "explicit invalidation reason is missing");

    // Once playback schedules one terminal frame and then leaves no periodic
    // wakeup for that entry.
    require(player.setLoopMode(
        firstAdded.handle,
        runtime::PlaybackLoopMode::Once,
        freezeLater),
        "once mode failed");
    require(player.seekNormalized(firstAdded.handle, 0.0, freezeLater),
        "once seek failed");
    require(player.setDirection(
        firstAdded.handle,
        runtime::PlaybackDirection::Forward,
        freezeLater),
        "once direction failed");
    require(player.play(firstAdded.handle, freezeLater), "once play failed");
    tick = player.tick(freezeLater);
    const auto afterCompletion = add(freezeLater, duration.nanoseconds * 2);
    tick = player.tick(afterCompletion);
    firstDue = findFrame(tick.frames, firstAdded.handle);
    require(firstDue != nullptr
            && player::hasReason(firstDue->reasons, player::FrameReason::Completion)
            && firstDue->playback.completed,
        "once playback did not produce one completion frame");

    // Host repaint mode returns all visible entries without perturbing their
    // cadence or resource identities.
    tick = player.tick(afterCompletion, player::FrameSelection::AllVisible);
    require(tick.frames.size() == 3U,
        "AllVisible repaint did not return every visible registration");
    for (const auto& frame : tick.frames) {
        require(player::hasReason(frame.reasons, player::FrameReason::HostRepaint),
            "host repaint reason is missing");
    }

    const auto oldHandle = firstAdded.handle;
    require(player.removeInstance(oldHandle, afterCompletion),
        "instance removal failed");
    require(player.instance(oldHandle) == nullptr,
        "stale player handle still resolves after removal");
    const auto replacement = player.addInstance(first, afterCompletion);
    require(static_cast<bool>(replacement), "replacement registration failed");
    require(replacement.handle.index == oldHandle.index
            && replacement.handle.generation != oldHandle.generation,
        "player handle generation did not advance on slot reuse");

    auto hiddenPreplaying = createSharedInstance(runtime, loaded.asset);
    hiddenPreplaying->play(zero);
    const auto hiddenAdded = player.addInstance(hiddenPreplaying, zero, {
        .maximumPresentationRate = 60.0,
        .visible = false,
        .hiddenTimePolicy = player::HiddenTimePolicy::Freeze,
    });
    require(static_cast<bool>(hiddenAdded),
        "hidden pre-playing registration failed");
    require(hiddenPreplaying->playbackSnapshot(zero).status
            == runtime::PlaybackStatus::Paused,
        "hidden Freeze registration did not contain active playback");
    require(player.removeInstance(hiddenAdded.handle, zero),
        "hidden pre-playing removal failed");

    const auto stableStorage = player.diagnostics().storageGeneration;
    for (int index = 0; index != 1000; ++index) {
        const auto sample = add(afterCompletion, index * interval);
        static_cast<void>(player.tick(sample));
    }
    require(player.diagnostics().storageGeneration == stableStorage,
        "steady scheduler ticks grew retained storage");

    // Replacing host callbacks must preserve a coalesced repaint request even
    // when the entry that caused it has already been removed. The host still
    // needs one frame to clear the retired presentation bounds.
    {
        player::Player callbackPlayer;
        auto callbackInstance = createSharedInstance(runtime, loaded.asset);
        const auto callbackAdded = callbackPlayer.addInstance(
            callbackInstance,
            zero);
        require(static_cast<bool>(callbackAdded),
            "callback-replacement registration failed");
        static_cast<void>(callbackPlayer.tick(zero));
        require(callbackPlayer.removeInstance(callbackAdded.handle, zero),
            "callback-replacement removal failed");
        HostRecorder replacementHost;
        callbackPlayer.setHostCallbacks({
            .userData = &replacementHost,
            .requestFrame = &HostRecorder::requestFrame,
            .scheduleWakeup = &HostRecorder::scheduleWakeup,
            .cancelWakeup = &HostRecorder::cancelWakeup,
        }, zero);
        require(replacementHost.frameRequests == 1U,
            "callback replacement lost the pending clear-presentation request");
    }

    // An isolated Once registration emits a terminal frame and then removes
    // itself from the periodic deadline schedule.
    {
        player::Player oncePlayer;
        auto onceInstance = createSharedInstance(runtime, loaded.asset);
        const auto onceAdded = oncePlayer.addInstance(onceInstance, zero);
        require(static_cast<bool>(onceAdded),
            "isolated once registration failed");
        require(oncePlayer.setLoopMode(
            onceAdded.handle,
            runtime::PlaybackLoopMode::Once,
            zero),
            "isolated once loop mode failed");
        require(oncePlayer.play(onceAdded.handle, zero),
            "isolated once play failed");
        auto onceTick = oncePlayer.tick(zero);
        require(onceTick.nextDeadline.has_value(),
            "isolated once playback did not schedule its first deadline");
        onceTick = oncePlayer.tick(add(zero, duration.nanoseconds * 2));
        const auto* completion = findFrame(onceTick.frames, onceAdded.handle);
        require(completion != nullptr
                && completion->playback.completed
                && player::hasReason(
                    completion->reasons,
                    player::FrameReason::Completion),
            "isolated once playback did not emit its completion frame");
        require(!onceTick.nextDeadline.has_value(),
            "completed once playback retained a periodic deadline");
    }

    // Replacing callbacks also retires the old platform deadline before the
    // same absolute deadline is published to the new host. Destruction
    // cancels the final outstanding wakeup.
    HostRecorder retiredHost;
    HostRecorder activeHost;
    {
        player::Player callbackPlayer{{
            .userData = &retiredHost,
            .requestFrame = &HostRecorder::requestFrame,
            .scheduleWakeup = &HostRecorder::scheduleWakeup,
            .cancelWakeup = &HostRecorder::cancelWakeup,
        }};
        auto callbackInstance = createSharedInstance(runtime, loaded.asset);
        const auto callbackAdded = callbackPlayer.addInstance(
            callbackInstance,
            zero);
        require(static_cast<bool>(callbackAdded),
            "deadline callback registration failed");
        require(callbackPlayer.play(callbackAdded.handle, zero),
            "deadline callback play failed");
        const auto callbackTick = callbackPlayer.tick(zero);
        require(callbackTick.nextDeadline.has_value()
                && retiredHost.deadline == callbackTick.nextDeadline,
            "old host did not receive the active deadline");
        callbackPlayer.setHostCallbacks({
            .userData = &activeHost,
            .requestFrame = &HostRecorder::requestFrame,
            .scheduleWakeup = &HostRecorder::scheduleWakeup,
            .cancelWakeup = &HostRecorder::cancelWakeup,
        }, zero);
        require(retiredHost.cancels == 1U,
            "callback replacement did not retire the old host wakeup");
        require(activeHost.deadline == callbackTick.nextDeadline,
            "callback replacement did not publish the deadline to the new host");
    }
    require(activeHost.cancels == 1U,
        "player destruction did not cancel the final host wakeup");

    const auto diagnostics = player.diagnostics();
    require(diagnostics.registrations == 5U,
        "registration diagnostics are incorrect");
    require(diagnostics.removals == 2U,
        "removal diagnostics are incorrect");
    require(diagnostics.skippedDeadlines > 0U,
        "skipped-deadline diagnostics are empty");
    require(diagnostics.automaticPauses == 2U
            && diagnostics.automaticResumes == 1U,
        "visibility policy diagnostics are incorrect");

    std::cout << "AveMotion centralized player/scheduler tests passed\n";
    return EXIT_SUCCESS;
}
