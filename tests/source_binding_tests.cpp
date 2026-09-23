#include "TelegramParsedModelBuilder.hpp"
#include "RlottieSceneBridge.hpp"
#include "support/ExactSceneComparison.hpp"
#include "avemotionanimationaccess.h"
#include "lottiemodel.h"
#include <rlottie.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <array>
#include <atomic>
#include <thread>

namespace {
using avemotion::runtime::EvaluatedScene;
using namespace avemotion::model::detail;
void require(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }
std::string read(const std::string& name) {
    std::ifstream in(std::filesystem::path(AVEMOTION_FIXTURE_DIR) / "reference_sessions" / name, std::ios::binary);
    require(bool(in), "open fixture " + name);
    return {std::istreambuf_iterator<char>(in), {}};
}
auto load(const std::string& json, const std::string& key) {
    auto a = rlottie::Animation::loadFromData(json, key, {}, true);
    require(bool(a), "load"); return a;
}
AssetModelDescriptor descriptor() {
    AssetModelDescriptor d{};
    d.sourceAssetHash = 1; d.logicalWidth = 128; d.logicalHeight = 128;
    d.frameRate = 30; d.totalFrames = 4; d.debugName = "source-binding";
    return d;
}
EvaluatedScene copy(const LOTLayerNode* tree, size_t frame) {
    auto result = avemotion::runtime::detail::buildSceneFromRlottieTree(tree, 1, 1, 1, frame, 128, 128);
    require(bool(result), "bridge: " + result.error.message); return std::move(result.scene);
}
void same(const EvaluatedScene& expected, const EvaluatedScene& actual) {
    auto diff = avemotion::test::ExactSceneComparison{}.difference(expected, actual);
    require(diff.empty(), "full scene mismatch: " + diff);
}
bool liveIds(const EvaluatedScene& scene) {
    for (const auto& item : scene.drawItems)
        if (item.sourcePathNode.valid() && item.sourcePaintNode.valid()) return true;
    return false;
}
std::string shaped(const std::string& path, const std::string& paint, bool multi = false, bool hidden = false) {
    return R"({"v":"5.7.4","w":128,"h":128,"fr":30,"ip":0,"op":4,"layers":[{"ty":4,"ind":1,"ip":)"
        + std::string(hidden ? "1" : "0") + R"(,"op":4,"st":0,"ks":{"o":{"k":100},"p":{"k":[0,0]},"a":{"k":[0,0]},"s":{"k":[100,100]},"r":{"k":0}},"shapes":[)"
        + path + (multi ? "," + path : "") + "," + paint + "]}]}";
}
const std::array<std::string,4> paths{{
    R"({"ty":"rc","d":1,"p":{"k":[64,64]},"s":{"k":[40,40]},"r":{"k":0}})",
    R"({"ty":"el","d":1,"p":{"k":[64,64]},"s":{"k":[40,40]}})",
    R"({"ty":"sh","ks":{"k":{"i":[[0,0],[0,0],[0,0]],"o":[[0,0],[0,0],[0,0]],"v":[[40,40],[80,40],[64,80]],"c":true}}})",
    R"({"ty":"sr","sy":1,"d":1,"p":{"k":[64,64]},"pt":{"k":5},"r":{"k":0},"or":{"k":30},"os":{"k":0},"ir":{"k":15},"is":{"k":0}})"
}};
const std::array<std::string,4> paints{{
    R"({"ty":"fl","c":{"k":[1,0,0,1]},"o":{"k":100},"r":1})",
    R"({"ty":"st","c":{"k":[0,1,0,1]},"o":{"k":100},"w":{"k":3},"lc":1,"lj":1,"ml":4})",
    R"({"ty":"gf","t":1,"s":{"k":[40,40]},"e":{"k":[80,80]},"o":{"k":100},"r":1,"g":{"p":2,"k":{"k":[0,1,0,0,1,0,0,1]}}})",
    R"({"ty":"gs","t":1,"s":{"k":[40,40]},"e":{"k":[80,80]},"o":{"k":100},"w":{"k":3},"lc":1,"lj":1,"ml":4,"g":{"p":2,"k":{"k":[0,1,0,0,1,0,0,1]}}})"
}};

// Catches a factory that reloads or aliases evaluator state instead of the exact source.
void sourceContract() {
    using Access = rlottie::AveMotionAnimationAccess;
    require(!Access::fromModel({}), "null source rejected");
    require(!Access::fromModel(std::make_shared<LOTModel>()), "null root rejected");
    require(!buildTelegramParsedModel(std::shared_ptr<LOTModel>{}, descriptor()), "null builder source rejected");
    auto a = load(shaped(paths[0],paints[0]),"part25d-contract");
    auto source = Access::model(*a);
    require(bool(source), "source lease");
    auto b = Access::fromModel(source);
    require(bool(b) && Access::model(*b) == source, "factory shares exact source");
    size_t aw=0,ah=0,bw=0,bh=0;
    a->size(aw,ah); b->size(bw,bh);
    require(aw==bw && ah==bh && a->totalFrame()==b->totalFrame() && a->frameRate()==b->frameRate() && a->duration()==b->duration(), "metadata parity");
    same(copy(a->renderTree(1,128,128),1),copy(b->renderTree(1,128,128),1));
    std::vector<uint32_t> ap(128*128),bp(128*128);
    rlottie::Surface as(ap.data(),128,128,128*4),bs(bp.data(),128,128,128*4);
    a->renderSync(1,as); b->renderSync(1,bs);
    require(ap==bp, "ordinary CPU factory pixels");
    a.reset(); b.reset();
    auto survivor=Access::fromModel(source);
    require(bool(survivor), "lease keeps authored source alive");
}

// Every path/paint owner must refresh, including invisible descendants and multi-path paints.
void ownerCoverage() {
    using Access = rlottie::AveMotionAnimationAccess;
    for (size_t p=0;p<paths.size();++p) for (size_t q=0;q<paints.size();++q) {
        for (bool multi : {false,true}) {
            auto json=shaped(paths[p],paints[q],multi,true);
            auto a=load(json,"part25d-owners-"+std::to_string(p)+"-"+std::to_string(q)+(multi?"-multi":""));
            auto source=Access::model(*a);
            require(a->enableRecordingLifecycle(), "owners enable");
            const auto hidden=copy(a->renderTreeForRecording(0,128,128),0);
            auto parsed=buildTelegramParsedModel(source,descriptor());
            require(bool(parsed),parsed.error);
            for (size_t frame : {0U,1U,3U,1U}) {
                auto ordinary=Access::fromModel(source);
                const auto expected=copy(ordinary->renderTree(frame,128,128),frame);
                const auto actual=copy(a->renderTreeForRecording(frame,128,128),frame);
                same(expected,actual);
                if (frame) {
                    require(!expected.drawItems.empty(),"visible owner geometry");
                    require(multi ? !liveIds(expected) : liveIds(expected),"applicable single-path IDs / intentional multi-path invalidity");
                    if (multi) for (const auto& item:expected.drawItems)
                        require(!item.sourcePathNode.valid() && item.sourcePaintNode.valid() && item.sourcePathCount==2,"multi-path count and paint binding");
                }
            }
            auto ordinary=Access::fromModel(source);
            same(hidden,copy(ordinary->renderTree(0,128,128),0));
        }
    }
}

// A failed extractor stamps some IDs; suppressing its epoch would leave retained scenes stale.
void failurePublication() {
    using Access = rlottie::AveMotionAnimationAccess;
    auto json=read("recording-negative-active-control.json");
    const auto child=json.find("\"ind\":2");
    require(child!=std::string::npos,"duplicate child fixture");
    json.replace(child,7,"\"ind\":1");
    auto a=load(json,"part25d-failure");
    auto source=Access::model(*a);
    require(a->enableRecordingLifecycle(),"failure enable");
    const auto before=copy(a->renderTreeForRecording(2,128,128),2);
    require(!liveIds(before),"failed source cold");
    const auto epoch=source->mAveMotionBindingEpoch.load();
    auto parsed=buildTelegramParsedModel(source,descriptor());
    require(!parsed && !parsed.model && parsed.error=="parsed composition contains duplicate layer IDs","existing duplicate rejection and no partial final model");
    require(source->mAveMotionBindingEpoch.load()!=epoch,"failure publishes epoch");
    auto ordinary=Access::fromModel(source);
    const auto expected=copy(ordinary->renderTree(2,128,128),2);
    require(liveIds(expected),"failed extraction really stamped visible bindings");
    same(expected,copy(a->renderTreeForRecording(2,128,128),2));
    require(!liveIds(before),"failed refresh preserves old copy");
}

void propertyPublication() {
    auto json=shaped(paths[2],paints[2]);
    const std::string key="part25d-property";
    auto a=load(json,key);
    auto source=rlottie::AveMotionAnimationAccess::model(*a);
    require(a->enableRecordingLifecycle(),"property enable");
    const auto before=copy(a->renderTreeForRecording(1,128,128),1);
    const auto epoch=source->mAveMotionBindingEpoch.load();
    auto oracle=evaluateTelegramParsedProperties(json,key,descriptor(),1);
    require(bool(oracle),oracle.error);
    require(source->mAveMotionBindingEpoch.load()!=epoch,"property oracle publishes epoch");
    auto ordinary=load(json,key);
    const auto expected=copy(ordinary->renderTree(1,128,128),1);
    require(liveIds(expected),"property bindings applicable");
    same(expected,copy(a->renderTreeForRecording(1,128,128),1));
    require(!liveIds(before),"property refresh preserves cold copy");
}

void descendantHistories() {
    using Access=rlottie::AveMotionAnimationAccess;
    for (const std::string name : {"recording-inner-paint-control.json", "recording-nested-repeater.json", "recording-outer-trim.json"}) {
        for (bool initialSample : {false,true}) {
            const auto json=read(name)+(initialSample ? "\n  " : "\n   ");
            auto a=load(json,"part25d-history-"+name+(initialSample ? "-sampled" : "-pristine"));
            auto source=Access::model(*a);
            require(a->enableRecordingLifecycle(),"history enable");
            EvaluatedScene before;
            if(initialSample) before=copy(a->renderTreeForRecording(0,128,128),0);
            auto parsed=buildTelegramParsedModel(source,descriptor()); require(bool(parsed),parsed.error);
            bool livePaint=false;
            const LOTLayerNode* root=nullptr;
            for(size_t frame : {0U,1U,2U,3U,0U,1U}) {
                auto ordinary=Access::fromModel(source);
                const auto expected=copy(ordinary->renderTree(frame,128,128),frame);
                const auto* tree=a->renderTreeForRecording(frame,128,128);
                if(root) require(root==tree,"refresh retains publication root"); else root=tree;
                same(expected,copy(tree,frame));
                for(const auto& item:expected.drawItems) livePaint|=item.sourcePaintNode.valid();
                if(name=="recording-inner-paint-control.json" && frame%2)
                    require(liveIds(expected),"repeater descendants have live applicable IDs");
            }
            require(livePaint,"history exercises bound visible paint");
            if(initialSample) require(!liveIds(before),"old hidden copied scene remains cold");
        }
    }
}

// Separate mutable sessions may construct/sample while two descriptor-specific extractors stamp one source.
void concurrentPublication() {
    using Access=rlottie::AveMotionAnimationAccess;
    auto initial=load(shaped(paths[0],paints[0]),"part25d-concurrent");
    auto source=Access::model(*initial);
    std::atomic<bool> start{false};
    std::array<std::exception_ptr,4> failures{};
    std::array<std::shared_ptr<const avemotion::model::MotionAssetModel>,2> results;
    std::array<EvaluatedScene,2> scenes;
    std::array<std::unique_ptr<rlottie::Animation>,2> retained;
    for(auto& a:retained) { a=Access::fromModel(source); require(a->enableRecordingLifecycle(),"pre-publication concurrent enable"); }
    std::vector<std::thread> workers;
    for (size_t i=0;i<4;++i) workers.emplace_back([&,i] {
        try {
            while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
            for (int run=0;run<40;++run) {
                if(i<2) {
                    auto d=descriptor(); d.assetHandle={static_cast<uint32_t>(i+1),1}; d.debugName=i ? "second" : "first";
                    auto parsed=buildTelegramParsedModel(source,d);
                    require(bool(parsed),parsed.error); results[i]=parsed.model;
                } else {
                    auto a=Access::fromModel(source);
                    require(a->enableRecordingLifecycle(),"concurrent enable");
                    for (size_t frame : {0U,3U,1U}) scenes[i-2]=copy(a->renderTreeForRecording(frame,128,128),frame);
                    (void)copy(retained[i-2]->renderTreeForRecording(1,128,128),1);
                    auto ordinary=Access::fromModel(source);
                    (void)copy(ordinary->renderTree(1,128,128),1);
                }
            }
        } catch (...) { failures[i]=std::current_exception(); }
    });
    start.store(true,std::memory_order_release);
    for(auto& worker:workers) worker.join();
    for(auto failure:failures) if(failure) std::rethrow_exception(failure);
    require(results[0]!=results[1] && results[0]->assetHandle==avemotion::runtime::AssetHandle{1,1} && results[1]->assetHandle==avemotion::runtime::AssetHandle{2,1} && results[0]->debugName=="first" && results[1]->debugName=="second","descriptor-specific model ownership");
    auto ordinary=Access::fromModel(source);
    const auto expected=copy(ordinary->renderTree(1,128,128),1);
    require(liveIds(expected),"concurrent final bindings valid");
    // After joining, the original cold sessions must observe completed publication.
    for(size_t i=0;i<retained.size();++i) {
        scenes[i]=copy(retained[i]->renderTreeForRecording(1,128,128),1); same(expected,scenes[i]);
    }
}
// Catches retained constructor snapshots failing to observe later parsed stamping.
void lateBinding() {
    auto json = read("recording-inner-paint-control.json") + "\n \n";
    const std::string key = "part25d-cold-late-binding";
    auto retained = load(json, key);
    require(retained->enableRecordingLifecycle(), "recording enable");
    auto before = copy(retained->renderTreeForRecording(1,128,128),1);
    require(!liveIds(before), "cold source starts unbound");
    const auto saved = before;
    auto parsed = buildTelegramParsedModel(json,key,descriptor());
    require(bool(parsed), parsed.error);
    auto ordinary = load(json,key);
    auto expected = copy(ordinary->renderTree(1,128,128),1);
    require(liveIds(expected), "oracle has applicable authored IDs");
    same(expected,copy(retained->renderTreeForRecording(1,128,128),1));
    same(saved,before);
}
}
int main() {
    try {
        lateBinding(); sourceContract(); ownerCoverage(); failurePublication(); propertyPublication(); descendantHistories(); concurrentPublication();
        std::cout << "PASS source bindings: cold refresh, lease/factory/CPU, 32 owner combinations, failed extraction, property oracle, hidden/repeater/trim histories, concurrent publication\n";
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
