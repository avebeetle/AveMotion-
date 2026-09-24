#pragma once

#include "NativeEllipseCertificate.hpp"
#include "avemotion/evaluation/PropertyEvaluator.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace avemotion::render::detail {

class NativeEllipseStream;

enum class NativeEllipseCreateCode {
    Ready, InvalidCertificate, InvalidIdentity, EvaluationPreparationFailed
};

struct NativeEllipseCreateResult final {
    NativeEllipseCreateCode code = NativeEllipseCreateCode::InvalidCertificate;
    std::string message;
    std::unique_ptr<NativeEllipseStream> stream;
    [[nodiscard]] explicit operator bool() const noexcept {
        return code == NativeEllipseCreateCode::Ready && stream != nullptr;
    }
};

enum class NativeEllipseFrameCode {
    Emitted, InvalidViewport, EvaluationFailed, UnsupportedNumericOutput,
    ModelApplicationFailed
};

struct NativeEllipseFrameResult final {
    NativeEllipseFrameCode code = NativeEllipseFrameCode::EvaluationFailed;
    std::string message;
    std::optional<runtime::EvaluatedScene> scene;
    [[nodiscard]] explicit operator bool() const noexcept {
        return code == NativeEllipseFrameCode::Emitted && scene.has_value();
    }
};

class NativeEllipseStream final {
public:
    [[nodiscard]] static NativeEllipseCreateResult create(
        std::shared_ptr<const runtime::detail::NativeEllipseCertificate> certificate,
        std::uint64_t instanceId);
    [[nodiscard]] NativeEllipseFrameResult emit(
        std::size_t frame, std::size_t width, std::size_t height);
    NativeEllipseStream(const NativeEllipseStream&) = delete;
    NativeEllipseStream& operator=(const NativeEllipseStream&) = delete;
    ~NativeEllipseStream();

private:
    NativeEllipseStream(
        std::shared_ptr<const runtime::detail::NativeEllipseCertificate> certificate,
        std::uint64_t instanceId);
    std::shared_ptr<const runtime::detail::NativeEllipseCertificate> certificate_;
    std::uint64_t instanceId_ = 0;
    evaluation::PropertyEvaluator evaluator_;
    evaluation::PropertyEvaluationWorkspace workspace_;
    std::uint64_t attemptSequence_ = 0;
    runtime::SceneFingerprints previous_;
    bool hasPrevious_ = false;
};

} // namespace avemotion::render::detail
