#include "avemotion/model/AssetModel.hpp"
#include "avemotion/render/RenderPlanner.hpp"
#include "avemotion/runtime/Runtime.hpp"

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
} // namespace

int main() {
    using namespace avemotion;
    const auto assetPath = fs::path{AVEMOTION_CORPUS_DIR}
        / "ModernPictogramsForLottie_LoudMute.json";

    runtime::Runtime runtime;
    auto loaded = runtime.loadLottieJson(readText(assetPath), assetPath.filename().string());
    require(static_cast<bool>(loaded), "asset load failed");
    const auto prepared = loaded.asset->prepareModel();
    require(static_cast<bool>(prepared), "asset model preparation failed");

    auto first = runtime.createInstance(loaded.asset);
    auto second = runtime.createInstance(loaded.asset);
    require(first && second, "instance creation failed");

    auto firstScene = first.instance->evaluateModelPosition(0.5, 128U, 128U);
    auto secondScene = second.instance->evaluateModelPosition(0.5, 128U, 128U);
    require(firstScene && secondScene, "model-aware evaluation failed");

    render::MotionRenderPlanner firstPlanner;
    render::MotionRenderPlanner secondPlanner;
    auto firstPlan = firstPlanner.build(std::move(firstScene.scene));
    auto secondPlan = secondPlanner.build(std::move(secondScene.scene));
    require(firstPlan && secondPlan, "model-aware plan construction failed");
    require(firstPlan.plan.drawItems.size() == secondPlan.plan.drawItems.size(),
            "two instances produced different draw-item topology");

    bool sharedStaticGeometry = false;
    bool sharedStaticPaint = false;
    bool distinctInstanceGeometry = false;
    bool distinctInstancePaint = false;
    for (std::size_t index = 0; index < firstPlan.plan.drawItems.size(); ++index) {
        const auto& a = firstPlan.plan.drawItems[index];
        const auto& b = secondPlan.plan.drawItems[index];
        require(a.node.valid() && a.drawItem.valid(),
                "model-aware plan lost typed node identity");
        require(a.geometry.resourceId.valid() && a.paint.resourceId.valid(),
                "model-aware plan lost typed resource identity");
        require(a.node == b.node && a.drawItem == b.drawItem
                    && a.geometry.resourceId == b.geometry.resourceId
                    && a.paint.resourceId == b.paint.resourceId,
                "typed identity differs between instances of one asset");
        require(a.sourceItemKey == static_cast<std::uint64_t>(a.node.value) + 1U,
                "model-aware item key is not derived from stable NodeId");

        if (a.geometry.scope == render::ResourceIdentityScope::Asset) {
            require(a.geometry == b.geometry,
                    "asset-static geometry key differs between instances");
            sharedStaticGeometry = true;
        } else {
            require(a.geometry.instanceIdentity != b.geometry.instanceIdentity,
                    "instance geometry key aliases another instance");
            distinctInstanceGeometry = true;
        }
        if (a.paint.scope == render::ResourceIdentityScope::Asset) {
            require(a.paint == b.paint,
                    "asset-static paint key differs between instances");
            sharedStaticPaint = true;
        } else {
            require(a.paint.instanceIdentity != b.paint.instanceIdentity,
                    "instance paint key aliases another instance");
            distinctInstancePaint = true;
        }
    }
    require(sharedStaticGeometry, "corpus sample exposed no shared static geometry");
    require(sharedStaticPaint, "corpus sample exposed no shared static paint");
    require(distinctInstanceGeometry || distinctInstancePaint,
            "corpus sample exposed no instance-scoped resource identity");

    // Replanning the exact same state for the same instance must not emit
    // redundant native-resource updates.
    auto repeatedScene = first.instance->evaluateModelPosition(0.5, 128U, 128U);
    require(static_cast<bool>(repeatedScene), "repeated model evaluation failed");
    auto repeatedPlan = firstPlanner.build(std::move(repeatedScene.scene));
    require(static_cast<bool>(repeatedPlan), "repeated plan construction failed");
    require(repeatedPlan.plan.geometryUpdates.empty()
                && repeatedPlan.plan.paintUpdates.empty()
                && !repeatedPlan.plan.visualChanged,
            "repeated model-aware plan emitted spurious resource work");

    std::cout << "AveMotion typed render-plan tests passed\n";
    return EXIT_SUCCESS;
}
