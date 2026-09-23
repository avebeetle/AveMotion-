#include "RlottieSceneBridge.hpp"
#include "support/ExactSceneComparison.hpp"
#ifdef AVEMOTION_RECORDING_telegram
#include "support/TelegramRecordingChecks.hpp"
#else
#include "support/SamsungRecordingChecks.hpp"
#endif
#include <rlottie.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <thread>

namespace {
namespace fs = std::filesystem;
using avemotion::runtime::EvaluatedScene;
void require(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }
std::string read(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    require(bool(in), "open " + path.string());
    return {std::istreambuf_iterator<char>(in), {}};
}
auto load(const std::string& json) {
    auto result = rlottie::Animation::loadFromData(json, "recording-test", "", false);
    require(bool(result), "load fixture");
    return result;
}
auto candidate(const std::string& json) {
    auto result = load(json);
    require(result->enableRecordingLifecycle(), "pristine candidate enable");
    return result;
}
const LOTLayerNode* sample(rlottie::Animation& animation, size_t frame, size_t w, size_t h) {
    return animation.renderTreeForRecording(frame, w, h);
}
EvaluatedScene copy(const LOTLayerNode* tree, size_t frame, size_t w, size_t h) {
    auto result = avemotion::runtime::detail::buildSceneFromRlottieTree(tree, 1, 1, 1, frame, w, h);
    require(bool(result), "bridge: " + result.error.message);
    return std::move(result.scene);
}
EvaluatedScene fresh(const std::string& json, size_t frame, size_t w, size_t h) {
    auto animation = load(json);
    return copy(animation->renderTree(frame, w, h), frame, w, h);
}
void same(const EvaluatedScene& expected, const EvaluatedScene& actual, const std::string& where) {
    const auto diff = avemotion::test::ExactSceneComparison{}.difference(expected, actual);
    require(diff.empty(), where + ": " + diff);
}

void guards(const std::string& json) {
    auto recording = load(json);
    size_t w = 0, h = 0;
    recording->size(w, h);
    require(w && h && recording->totalFrame() && recording->frameRate() > 0 && recording->duration() > 0, "metadata before enable");
    (void)recording->layers(); (void)recording->frameAtPos(0.5);
    require(recording->enableRecordingLifecycle(), "pristine enables recording");
    require(recording->enableRecordingLifecycle(), "enabling is idempotent");
    require(recording->renderTree(0, 128, 128) == nullptr, "wrong tree entry rejected");
    auto ordinary = load(json);
    require(ordinary->renderTreeForRecording(0, 128, 128) == nullptr, "ordinary is not recording");
    require(ordinary->enableRecordingLifecycle(), "wrong entry does not consume pristine");
    for (auto dimensions : std::array<std::array<size_t, 2>, 4>{{{0,128},{128,0},{size_t(std::numeric_limits<int>::max())+1,128},{128,std::numeric_limits<size_t>::max()}}}) {
        require(recording->renderTreeForRecording(0, dimensions[0], dimensions[1]) == nullptr, "invalid dimensions rejected");
        same(fresh(json, 0, 128, 128), copy(sample(*recording, 0, 128, 128), 0, 128, 128), "invalid viewport recovery");
    }
    std::vector<uint32_t> pixels(128*128, 0x12345678U);
    rlottie::Surface surface(pixels.data(), 128, 128, 128*4);
    int callbacks = 0;
    const auto forbidden = [&](auto operation) {
        bool threw = false;
        try { operation(); } catch (const std::logic_error&) { threw = true; }
        require(threw, "forbidden operation throws logic_error");
        require(std::all_of(pixels.begin(), pixels.end(), [](auto p) { return p == 0x12345678U; }), "sentinel pixels untouched");
        require(callbacks == 0, "no property callback evaluated");
    };
    forbidden([&] { recording->renderSync(0, surface); });
    forbidden([&] { (void)recording->render(0, surface); });
    forbidden([&] { recording->setValue<rlottie::Property::FillColor>("**", rlottie::Color(1,0,0)); });
    forbidden([&] { recording->setValue<rlottie::Property::FillOpacity>("", 50.0F); });
    forbidden([&] { recording->setValue<rlottie::Property::TrPosition>("**", rlottie::Point(1,2)); });
    forbidden([&] { recording->setValue<rlottie::Property::TrScale>("**", rlottie::Size(2,2)); });
    forbidden([&] { recording->setValue<rlottie::Property::FillColor>("**", [&](const rlottie::FrameInfo&) { ++callbacks; return rlottie::Color(1,0,0); }); });
    forbidden([&] { recording->setValue<rlottie::Property::FillOpacity>("**", [&](const rlottie::FrameInfo&) { ++callbacks; return 50.0F; }); });
    forbidden([&] { recording->setValue<rlottie::Property::TrPosition>("**", [&](const rlottie::FrameInfo&) { ++callbacks; return rlottie::Point(1,2); }); });
    forbidden([&] { recording->setValue<rlottie::Property::TrScale>("**", [&](const rlottie::FrameInfo&) { ++callbacks; return rlottie::Size(2,2); }); });
    std::weak_ptr<int> callbackOwner;
    {
        auto owner = std::make_shared<int>(0);
        callbackOwner = owner;
        forbidden([&] { recording->setValue<rlottie::Property::FillOpacity>("**", [owner](const rlottie::FrameInfo&) { ++*owner; return 50.0F; }); });
    }
    require(callbackOwner.expired(), "rejected callback releases its captured owner");
    for (int use = 0; use < 4; ++use) {
        auto usedOrdinary = load(json);
        if (use == 0) usedOrdinary->renderSync(0, surface);
        if (use == 1) { auto pending = usedOrdinary->render(0, surface); require(!usedOrdinary->enableRecordingLifecycle(), "async dispatch synchronously consumes pristine"); pending.get(); }
        if (use == 2) (void)usedOrdinary->renderTree(0, 128, 128);
        if (use == 3) usedOrdinary->setValue<rlottie::Property::FillOpacity>("", 50.0F);
        require(!usedOrdinary->enableRecordingLifecycle(), "ordinary use forbids mode switch");
    }
    const auto expected = fresh(json, 0, 128, 128);
    const auto* retainedRoot = sample(*recording, 0, 128, 128);
    auto preserved = copy(retainedRoot, 0, 128, 128);
    require(sample(*recording, 1, 96, 160) == retainedRoot, "recording retains root publication owner");
    recording.reset();
    same(expected, preserved, "owned scene survives next sample and destruction");
    auto cpu = load(json);
    cpu->renderSync(0, surface);
    const auto before = pixels;
    auto unrelated = candidate(json);
    (void)sample(*unrelated, 1, 96, 160);
    cpu->renderSync(0, surface);
    require(before == pixels, "ordinary CPU unchanged across recording sample");
    std::cout << "PASS guards, ownership, CPU isolation\n";
}

void freshNoBuildSentinel() {
    const std::string json = R"({"v":"5.7.4","w":128,"h":128,"fr":30,"ip":-1,"op":3,"layers":[]})";
#ifdef AVEMOTION_RECORDING_telegram
    auto ordinary = load(json);
    require(ordinary->renderTree(0,128,128) == nullptr, "Telegram fresh global minus-one does not build");
#endif
    auto recording = candidate(json);
    require(sample(*recording,0,128,128) == nullptr, "recording global minus-one sentinel");
    same(fresh(json,0,96,160), copy(sample(*recording,0,96,160),0,96,160), "sentinel with changed viewport");
    require(sample(*recording,0,128,128) == nullptr, "sentinel never republishes stale tree");
    same(fresh(json,0,96,160), copy(sample(*recording,0,96,160),0,96,160), "recovery after sentinel");
}

#ifdef AVEMOTION_RECORDING_samsung
void samsungInclusiveVisibility() {
    const std::string json = R"({"v":"5.7.4","w":128,"h":128,"fr":30,"ip":0,"op":3,"layers":[{"ty":3,"ind":1,"nm":"Boundary","ip":0,"op":1,"st":0,"ks":{"o":{"a":0,"k":100},"r":{"a":0,"k":0},"p":{"a":0,"k":[0,0,0]},"a":{"a":0,"k":[0,0,0]},"s":{"a":0,"k":[100,100,100]}}}]})";
    auto retained = candidate(json);
    for (size_t frame : {0U,1U,2U,1U,0U}) {
        auto ordinary = load(json);
        const auto *expected = ordinary->renderTree(frame,128,128);
        const auto *actual = sample(*retained,frame,128,128);
        require(expected && actual && expected->mLayerList.size == 1 && actual->mLayerList.size == 1,
                "Samsung boundary fixture retains its child");
        const int visible = frame <= 1 ? 1 : 0;
        require(expected->mLayerList.ptr[0]->mVisible == visible &&
                actual->mLayerList.ptr[0]->mVisible == visible,
                "Samsung out-frame visibility remains inclusive");
    }
}
#endif

void timeline(const fs::path& path) {
    const auto json = read(path);
    auto retained = candidate(json);
    const auto count = retained->totalFrame();
    require(count > 0, "nonempty timeline");
    if (path.filename() == "recording-image-brush.json") {
        const auto image = fresh(json,0,128,128);
        require(!image.drawItems.empty(), "image fixture must publish a draw item");
        require(image.drawItems[0].paint.image.present && image.drawItems[0].paint.image.width == 1 &&
                image.drawItems[0].paint.image.height == 1, "image fixture must decode its owned pixel");
    }
    size_t samples = 0;
    const auto check = [&](size_t f, size_t w, size_t h) {
        same(fresh(json, f, w, h), copy(sample(*retained, f, w, h), f, w, h),
             path.filename().string() + " sample=" + std::to_string(samples++) + " frame=" + std::to_string(f));
    };
    for (size_t f = 0; f < count; ++f) check(f, 128, 128);
    for (size_t f = count; f-- > 0;) check(f, 96, 160);
    for (auto f : std::array<size_t, 10>{count/2,count/2,0,count-1,1%count,0,count-1,count/3,count/3,0}) {
        check(f,128,128); check(f,128,128); check(f,96,160); check(f,96,160); check(f,128,128);
    }
    check(count,128,128); check(std::numeric_limits<size_t>::max(),96,160);
    std::cout << "PASS " << path.filename().string() << " frames=" << count << " samples=" << samples << '\n';
}
void threaded(const std::string& json) {
    std::array<std::exception_ptr, 2> errors;
    std::array<std::thread, 2> threads;
    for (size_t i = 0; i < threads.size(); ++i) threads[i] = std::thread([&, i] {
        try {
            for (int loop = 0; loop < 16; ++loop) {
                auto retained = candidate(json);
                for (size_t f : {0U,1U,0U,2U,1U})
                    same(fresh(json,f,96,160), copy(sample(*retained,f,96,160),f,96,160), "independent thread");
            }
        } catch (...) { errors[i] = std::current_exception(); }
    });
    for (auto& thread : threads) thread.join();
    for (const auto& error : errors) if (error) std::rethrow_exception(error);
    std::cout << "PASS independent host threads and repeated destruction\n";
}
void lateFailure(const fs::path& path) {
    const auto json = read(path);
    auto retained = candidate(json);
    for (size_t frame : {0U, 1U, 0U}) {
        auto ordinary = load(json);
        auto expected = avemotion::runtime::detail::buildSceneFromRlottieTree(
            ordinary->renderTree(frame, 128, 128), 1, 1, 1, frame, 128, 128);
        auto actual = avemotion::runtime::detail::buildSceneFromRlottieTree(
            sample(*retained, frame, 128, 128), 1, 1, 1, frame, 128, 128);
        require(bool(expected) == (frame == 0) && bool(actual) == bool(expected),
            "finite overflow fails only at frame 1 and recovers");
        require(expected.error.code == actual.error.code
                && expected.error.message == actual.error.message,
            "finite overflow ordinary/recording typed error parity");
        if (actual) same(expected.scene, actual.scene, "finite overflow full-scene recovery parity");
    }
    std::cout << "PASS late-overflow.json: ordinary typed error and complete recording recovery\n";
}
}
int main() {
    try {
        const fs::path fixtures(AVEMOTION_FIXTURE_DIR), corpus(AVEMOTION_CORPUS_DIR);
        std::vector<fs::path> paths;
        for (const auto& directory : {fixtures, corpus}) for (const auto& entry : fs::directory_iterator(directory))
            if (entry.path().extension() == ".json") paths.push_back(entry.path());
        require(paths.size() == 16, "top-level corpus remains sixteen assets");
        for (const auto& entry : fs::directory_iterator(fixtures / "reference_sessions"))
            if (entry.path().extension() == ".json") paths.push_back(entry.path());
        std::sort(paths.begin(), paths.end());
        size_t failures = 0;
        for (const auto& path : paths) try {
            if (path.filename() == "late-overflow.json") lateFailure(path);
            else timeline(path);
        }
            catch (const std::exception& e) { ++failures; std::cerr << "FAILED: " << e.what() << '\n'; }
        guards(read(fixtures / "reference_sessions/dashed_stroke_session.json"));
        freshNoBuildSentinel();
#ifdef AVEMOTION_RECORDING_telegram
        avemotion::test::verifyTelegramRecordingPublication();
#else
        avemotion::test::verifySamsungRecordingPaths();
        avemotion::test::verifySamsungRecordingPublication();
        samsungInclusiveVisibility();
        std::cout << "PASS Samsung empty/gapped paths, publication and inclusive visibility\n";
#endif
        threaded(read(corpus / "mask.json"));
        threaded(read(fixtures / "reference_sessions/recording-negative-skipped.json"));
        require(failures == 0, std::to_string(failures) + " fixture histories differ");
        std::cout << "PASS recording lifecycle\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAILED: " << e.what() << '\n'; return 1; }
}
