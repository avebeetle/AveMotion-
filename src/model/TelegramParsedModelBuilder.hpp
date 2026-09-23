#pragma once

#include "AssetModelBuilder.hpp"
#include "avemotion/evaluation/PropertyEvaluator.hpp"

#include <memory>
#include <vector>
#include <string>
#include <string_view>

namespace avemotion::model::detail {

struct ParsedModelBuildResult final {
    std::shared_ptr<const MotionAssetModel> model;
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept {
        return model != nullptr && error.empty();
    }
};

// Builds immutable authored topology/property/track tables directly from the
// pinned Telegram LOTModel. Render-facing geometry/paint tables are still
// merged by the existing exact-evaluation oracle in Part 7.
[[nodiscard]] ParsedModelBuildResult buildTelegramParsedModel(
    std::string_view json,
    std::string_view cacheKey,
    const AssetModelDescriptor& descriptor);

struct TelegramPropertyOracleResult final {
    std::shared_ptr<const MotionAssetModel> model;
    std::vector<evaluation::MotionPropertyValue> properties;
    std::vector<evaluation::EvaluatedNodeTransform> nodeTransforms;
    std::vector<evaluation::EvaluatedShape> shapes;
    std::vector<MotionVec2Value> shapePoints;
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept {
        return model != nullptr && error.empty();
    }
};

// Test/reference seam: evaluates the pinned Telegram parsed model at an exact
// integer frame while preserving the canonical PropertyId/SourceNodeId order.
// Production AveMotion evaluation never depends on this function.
[[nodiscard]] TelegramPropertyOracleResult evaluateTelegramParsedProperties(
    std::string_view json,
    std::string_view cacheKey,
    const AssetModelDescriptor& descriptor,
    int frame);

} // namespace avemotion::model::detail
