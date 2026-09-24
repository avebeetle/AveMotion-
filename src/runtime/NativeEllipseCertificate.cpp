#include "NativeEllipseCertificate.hpp"

#include <utility>

namespace avemotion::runtime::detail {

NativeEllipseCertificate::NativeEllipseCertificate(
    std::string json, std::shared_ptr<const NativeEllipseInput> admitted,
    std::shared_ptr<const Asset> lease,
    std::shared_ptr<const model::MotionAssetModel> frozen,
    NativeEllipseSlotMetadata metadata)
    : exactJson(std::move(json)), input(std::move(admitted)), asset(std::move(lease)),
      model(std::move(frozen)), slot(std::move(metadata)) {}

bool NativeEllipseCertificate::matchesAsset(
    const std::shared_ptr<const Asset>& candidate) const noexcept {
    return candidate && candidate.get() == asset.get()
        && candidate->handle() == slot.assetHandle
        && candidate->metadata().sourceHash == slot.sourceHash
        && candidate->model().get() == model.get()
        && model->assetHandle == slot.assetHandle
        && model->sourceAssetHash == slot.sourceHash;
}

NativeEllipseCertificateResult prepareNativeEllipseCertificate(std::string_view json) {
    Runtime runtime;
    return prepareNativeEllipseCertificate(runtime, json);
}

NativeEllipseCertificateResult prepareNativeEllipseCertificate(Runtime& runtime,
                                                               std::string_view json) {
    NativeEllipseCertificateResult result;
    result.diagnosticsBefore = runtime.diagnostics();
    const auto finish = [&]() {
        result.diagnosticsAfter = runtime.diagnostics();
        return result;
    };

    // The parser's byte guard runs before any owned copy or reference load.
    auto decoded = decodeNativeEllipseInput(json);
    result.admission = decoded.admission;
    if (!decoded) {
        result.code = NativeEllipseCertificateCode::AdmissionRejected;
        return finish();
    }
    std::string ownedJson{json};
    auto loaded = runtime.loadLottieJson(ownedJson);
    if (!loaded) {
        result.referenceError = loaded.error;
        return finish();
    }
    auto prepared = loaded.asset->prepareModel();
    if (!prepared) {
        result.referenceError = {RuntimeErrorCode::AssetModelPreparationFailed, prepared.error};
        return finish();
    }
    const auto& metadata = loaded.asset->metadata();
    if (!loaded.asset->handle().valid()
        || prepared.model.get() != loaded.asset->model().get()
        || prepared.model->assetHandle != loaded.asset->handle()
        || prepared.model->sourceAssetHash != metadata.sourceHash
        || prepared.model->logicalWidth != metadata.width
        || prepared.model->logicalHeight != metadata.height
        || prepared.model->frameRate != metadata.frameRate
        || prepared.model->totalFrames != metadata.totalFrames) {
        result.code = NativeEllipseCertificateCode::ScanRejected;
        result.scanCode = NativeEllipseScanCode::Identity;
        return finish();
    }
    auto bound = bindNativeEllipseModel(*decoded.input, *prepared.model);
    result.bindingCode = bound.code;
    if (!bound) {
        result.code = NativeEllipseCertificateCode::BindingRejected;
        return finish();
    }
    auto created = runtime.createInstance(loaded.asset);
    if (!created) {
        result.referenceError = created.error;
        return finish();
    }
    NativeEllipseScanAudit audit{*decoded.input, *bound.binding, *prepared.model,
                                 loaded.asset->handle(), metadata.sourceHash};
    for (std::size_t frame = 0; frame < decoded.input->endFrame; ++frame) {
        auto evaluated = created.instance->evaluateFrame(frame, decoded.input->width,
                                                          decoded.input->height);
        if (!evaluated) {
            result.referenceError = evaluated.error;
            return finish();
        }
        if (!audit.observe(frame, evaluated.scene)) {
            result.code = NativeEllipseCertificateCode::ScanRejected;
            result.scanCode = audit.code();
            return finish();
        }
    }
    auto slot = audit.finish();
    if (!slot) {
        result.code = NativeEllipseCertificateCode::ScanRejected;
        result.scanCode = audit.code();
        return finish();
    }
    result.certificate = std::shared_ptr<const NativeEllipseCertificate>(
        new NativeEllipseCertificate(std::move(ownedJson), std::move(decoded.input),
                                     std::move(loaded.asset), std::move(prepared.model),
                                     std::move(*slot)));
    result.code = NativeEllipseCertificateCode::Certified;
    result.scanCode = NativeEllipseScanCode::Complete;
    return finish();
}

} // namespace avemotion::runtime::detail
