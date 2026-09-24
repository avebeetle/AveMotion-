#pragma once

#include "NativeEllipseBinding.hpp"
#include "avemotion/runtime/Runtime.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace avemotion::runtime::detail {

enum class NativeEllipseScanCode {
    Complete, Identity, Timeline, LayerLayout, DrawMultiplicity,
    SourceBinding, ResourceBinding, UnsupportedSlot, IncompleteScan
};

struct NativeEllipseLayerSlot final {
    model::LayerId id;
    std::uint32_t parentIndex = kInvalidSceneIndex;
    std::string keyPath;
    std::uint32_t firstChildReference = 0, childCount = 0;
    std::uint32_t firstDrawItem = 0;
};

struct NativeEllipseDrawSlot final {
    model::DrawItemId draw;
    model::NodeId node;
    model::GeometryId geometry;
    model::PaintId paint;
    std::uint32_t layerIndex = kInvalidSceneIndex, drawOrder = 0;
    model::SourceNodeId sourcePath, sourcePaint;
    std::uint32_t sourcePathCount = 0;
    bool sourcePathModifierFree = false;
    bool localGeometryAvailable = false, localGeometryStaticCandidate = false;
    bool localPaintAvailable = false, localPaintStaticCandidate = false;
    FillRule fillRule = FillRule::Winding;
    Color8 solid, localSolid;
    float strokeWidth = 0.0F, strokeMiterLimit = 0.0F;
    LineCap strokeCap = LineCap::Flat;
    LineJoin strokeJoin = LineJoin::Miter;
    float localStrokeWidth = 0.0F, localStrokeMiterLimit = 0.0F;
    LineCap localStrokeCap = LineCap::Flat;
    LineJoin localStrokeJoin = LineJoin::Miter;
    bool opacitySeparated = false;
    float separatedOpacity = 1.0F;
};

struct NativeEllipseSlotMetadata final {
    std::size_t width = 0, height = 0, totalFrames = 0;
    std::size_t activeFirstFrame = 0, activeEndFrame = 0;
    double frameRate = 0.0;
    AssetHandle assetHandle;
    std::uint64_t sourceHash = 0;
    NativeEllipseModelBinding binding;
    NativeEllipseLayerSlot root, shape;
    NativeEllipseDrawSlot draw;
    std::uint32_t modelLayerCount = 0, modelNodeCount = 0;
    std::uint32_t modelGeometryCount = 0, modelPaintCount = 0;
    std::size_t observedFrames = 0, observedActiveFrames = 0;
};

class NativeEllipseScanAudit final {
public:
    NativeEllipseScanAudit(const NativeEllipseInput& input,
                           const NativeEllipseModelBinding& binding,
                           const model::MotionAssetModel& frozenModel,
                           AssetHandle expectedHandle, std::uint64_t expectedHash);
    [[nodiscard]] bool observe(std::size_t frame, const EvaluatedScene& scene);
    [[nodiscard]] std::optional<NativeEllipseSlotMetadata> finish();
    [[nodiscard]] NativeEllipseScanCode code() const noexcept { return code_; }

private:
    [[nodiscard]] bool fail(NativeEllipseScanCode code) noexcept;
    const NativeEllipseInput& input_;
    const NativeEllipseModelBinding& binding_;
    const model::MotionAssetModel& model_;
    NativeEllipseSlotMetadata slot_;
    NativeEllipseScanCode code_ = NativeEllipseScanCode::IncompleteScan;
    bool poisoned_ = false, finished_ = false, hasActive_ = false;
};

struct NativeEllipseCertificateResult;

class NativeEllipseCertificate final {
public:
    const std::string exactJson;
    const std::shared_ptr<const NativeEllipseInput> input;
    const std::shared_ptr<const Asset> asset;
    const std::shared_ptr<const model::MotionAssetModel> model;
    const NativeEllipseSlotMetadata slot;

    [[nodiscard]] bool matchesAsset(const std::shared_ptr<const Asset>& candidate) const noexcept;

private:
    NativeEllipseCertificate(std::string json,
                             std::shared_ptr<const NativeEllipseInput> admitted,
                             std::shared_ptr<const Asset> lease,
                             std::shared_ptr<const model::MotionAssetModel> frozen,
                             NativeEllipseSlotMetadata metadata);
    friend NativeEllipseCertificateResult prepareNativeEllipseCertificate(
        Runtime& runtime, std::string_view json);
};

enum class NativeEllipseCertificateCode {
    Certified, AdmissionRejected, ReferenceError, BindingRejected, ScanRejected
};

struct NativeEllipseCertificateResult final {
    NativeEllipseCertificateCode code = NativeEllipseCertificateCode::ReferenceError;
    NativeEllipseAdmission admission;
    NativeEllipseBindingCode bindingCode = NativeEllipseBindingCode::InvalidModelTable;
    NativeEllipseScanCode scanCode = NativeEllipseScanCode::IncompleteScan;
    RuntimeError referenceError;
    std::shared_ptr<const NativeEllipseCertificate> certificate;
    DiagnosticsSnapshot diagnosticsBefore, diagnosticsAfter;
    [[nodiscard]] explicit operator bool() const noexcept {
        return code == NativeEllipseCertificateCode::Certified && certificate != nullptr;
    }
};

[[nodiscard]] NativeEllipseCertificateResult prepareNativeEllipseCertificate(std::string_view json);
[[nodiscard]] NativeEllipseCertificateResult prepareNativeEllipseCertificate(Runtime& runtime,
                                                                              std::string_view json);

} // namespace avemotion::runtime::detail
