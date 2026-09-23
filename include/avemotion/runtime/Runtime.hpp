#pragma once

#include "avemotion/formats/Tgs.hpp"
#include "avemotion/model/AssetModel.hpp"
#include "avemotion/runtime/Diagnostics.hpp"
#include "avemotion/runtime/EvaluatedScene.hpp"
#include "avemotion/runtime/Handles.hpp"
#include "avemotion/runtime/Playback.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace avemotion::runtime {
namespace detail {
struct RuntimeState;
struct AssetData;
struct InstanceData;
}

enum class RuntimeErrorCode : std::uint8_t {
    None,
    ReferenceUnavailable,
    InvalidArgument,
    InvalidAsset,
    InstanceCreationFailed,
    EvaluationFailed,
    CpuRenderFailed,
    ContainerDecodeFailed,
    AssetModelPreparationFailed,
    InvalidPlaybackState,
};

struct RuntimeError final {
    RuntimeErrorCode code = RuntimeErrorCode::None;
    std::string message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return code != RuntimeErrorCode::None;
    }
};

struct AssetMetadata final {
    std::size_t width = 0;
    std::size_t height = 0;
    double frameRate = 0.0;
    double durationSeconds = 0.0;
    std::size_t totalFrames = 0;
    std::uint64_t sourceHash = 0;
    std::string debugName;
};

struct CpuFrame final {
    std::size_t frameIndex = 0;
    std::size_t width = 0;
    std::size_t height = 0;
    std::size_t strideBytes = 0;
    std::vector<std::uint32_t> argbPremultiplied;
    std::uint64_t fnv1a64 = 0;
    std::uint64_t alphaSum = 0;
    std::size_t nonTransparentPixels = 0;
    RectF nonTransparentBounds;
};

class Asset final {
public:
    Asset(const Asset&) = delete;
    Asset& operator=(const Asset&) = delete;
    ~Asset();

    [[nodiscard]] const AssetMetadata& metadata() const noexcept;
    [[nodiscard]] AssetHandle handle() const noexcept;
    [[nodiscard]] std::size_t canonicalGeometryCount() const noexcept;
    [[nodiscard]] std::size_t canonicalPaintCount() const noexcept;
    [[nodiscard]] model::AssetModelResult prepareModel() const;
    [[nodiscard]] std::shared_ptr<const model::MotionAssetModel> model() const;

private:
    explicit Asset(std::shared_ptr<const detail::AssetData> data) noexcept;
    std::shared_ptr<const detail::AssetData> data_;
    friend class Runtime;
    friend class Instance;
};

struct AssetLoadResult final {
    std::shared_ptr<const Asset> asset;
    RuntimeError error;

    [[nodiscard]] explicit operator bool() const noexcept {
        return asset != nullptr && !error;
    }
};

class Instance final {
public:
    Instance(Instance&&) noexcept;
    Instance& operator=(Instance&&) noexcept;
    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;
    ~Instance();

    [[nodiscard]] const AssetMetadata& assetMetadata() const noexcept;
    [[nodiscard]] std::uint64_t id() const noexcept;
    [[nodiscard]] InstanceHandle handle() const noexcept;
    [[nodiscard]] std::size_t frameAtPosition(double normalizedPosition) const noexcept;

    void play(MotionTime presentationTime) noexcept;
    void pause(MotionTime presentationTime) noexcept;
    void resume(MotionTime presentationTime) noexcept;
    void stop() noexcept;
    void seekNormalized(double normalizedPosition, MotionTime presentationTime) noexcept;
    void setControlledProgress(double normalizedPosition) noexcept;
    void setDirection(PlaybackDirection direction, MotionTime presentationTime) noexcept;
    [[nodiscard]] bool setPlaybackRate(
        double playbackRate,
        MotionTime presentationTime) noexcept;
    void setLoopMode(PlaybackLoopMode loopMode) noexcept;
    [[nodiscard]] PlaybackSnapshot playbackSnapshot(
        MotionTime presentationTime) const noexcept;

    struct SceneResult final {
        EvaluatedScene scene;
        RuntimeError error;
        [[nodiscard]] explicit operator bool() const noexcept { return !error; }
    };

    struct CpuFrameResult final {
        CpuFrame frame;
        RuntimeError error;
        [[nodiscard]] explicit operator bool() const noexcept { return !error; }
    };

    [[nodiscard]] SceneResult evaluateFrame(
        std::size_t frameIndex,
        std::size_t viewportWidth,
        std::size_t viewportHeight);

    [[nodiscard]] SceneResult evaluatePosition(
        double normalizedPosition,
        std::size_t viewportWidth,
        std::size_t viewportHeight);

    [[nodiscard]] SceneResult evaluateModelFrame(
        std::size_t frameIndex,
        std::size_t viewportWidth,
        std::size_t viewportHeight);

    [[nodiscard]] SceneResult evaluateModelPosition(
        double normalizedPosition,
        std::size_t viewportWidth,
        std::size_t viewportHeight);

    [[nodiscard]] SceneResult evaluateAt(
        MotionTime presentationTime,
        std::size_t viewportWidth,
        std::size_t viewportHeight);

    [[nodiscard]] CpuFrameResult renderCpuFrame(
        std::size_t frameIndex,
        std::size_t width,
        std::size_t height,
        bool keepAspectRatio = true);

private:
    explicit Instance(std::unique_ptr<detail::InstanceData> data) noexcept;
    std::unique_ptr<detail::InstanceData> data_;
    friend class Runtime;
};

struct InstanceCreateResult final {
    std::unique_ptr<Instance> instance;
    RuntimeError error;

    [[nodiscard]] explicit operator bool() const noexcept {
        return instance != nullptr && !error;
    }
};

class Runtime final {
public:
    Runtime();
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    Runtime(Runtime&&) noexcept;
    Runtime& operator=(Runtime&&) noexcept;
    ~Runtime();

    [[nodiscard]] static bool compiledWithReferenceEngine() noexcept;

    [[nodiscard]] AssetLoadResult loadLottieJson(
        std::string_view json,
        std::string_view debugName = {});

    [[nodiscard]] AssetLoadResult loadTgs(
        std::span<const std::byte> bytes,
        std::string_view debugName = {},
        formats::TgsDecodeLimits limits =
            formats::TgsDecodeLimits::telegramSticker());

    [[nodiscard]] AssetLoadResult loadTgsFile(
        const std::filesystem::path& path,
        std::string_view debugName = {},
        formats::TgsDecodeLimits limits =
            formats::TgsDecodeLimits::telegramSticker());

    [[nodiscard]] AssetLoadResult loadAssetData(
        std::span<const std::byte> bytes,
        std::string_view debugName = {},
        formats::TgsDecodeLimits tgsLimits =
            formats::TgsDecodeLimits::telegramSticker());

    [[nodiscard]] InstanceCreateResult createInstance(
        std::shared_ptr<const Asset> asset) const;

    [[nodiscard]] DiagnosticsSnapshot diagnostics() const noexcept;
    void resetDiagnostics() noexcept;

private:
    std::shared_ptr<detail::RuntimeState> state_;
};

} // namespace avemotion::runtime
