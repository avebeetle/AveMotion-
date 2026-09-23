#pragma once

#include "avemotion/render/RenderPlan.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace avemotion::render {

enum class RenderPlanErrorCode : std::uint8_t {
    None,
    InvalidArgument,
    InvalidScene,
    StaleSnapshot,
};

struct RenderPlanError final {
    RenderPlanErrorCode code = RenderPlanErrorCode::None;
    std::string message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return code != RenderPlanErrorCode::None;
    }
};

struct RenderPlanBuildResult final {
    MotionRenderPlan plan;
    RenderPlanError error;

    [[nodiscard]] explicit operator bool() const noexcept { return !error; }
};

struct RenderPlannerDiagnostics final {
    std::uint64_t plansBuilt = 0;
    std::uint64_t plansRejected = 0;
    std::uint64_t drawItemsPlanned = 0;
    std::uint64_t geometryUpdates = 0;
    std::uint64_t paintUpdates = 0;
    std::uint64_t unchangedPlans = 0;
    std::uint64_t forgottenInstances = 0;
};

class MotionRenderPlanner final {
public:
    MotionRenderPlanner();
    MotionRenderPlanner(MotionRenderPlanner&&) noexcept;
    MotionRenderPlanner& operator=(MotionRenderPlanner&&) noexcept;
    MotionRenderPlanner(const MotionRenderPlanner&) = delete;
    MotionRenderPlanner& operator=(const MotionRenderPlanner&) = delete;
    ~MotionRenderPlanner();

    [[nodiscard]] RenderPlanBuildResult build(
        std::shared_ptr<const runtime::EvaluatedScene> scene,
        const PresentationState& presentation = {});

    [[nodiscard]] RenderPlanBuildResult build(
        runtime::EvaluatedScene scene,
        const PresentationState& presentation = {});

    void forgetInstance(std::uint64_t instanceId) noexcept;
    void forgetInstance(runtime::InstanceHandle instance) noexcept;
    void reset() noexcept;

    [[nodiscard]] RenderPlannerDiagnostics diagnostics() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace avemotion::render
