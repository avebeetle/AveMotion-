#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "Win32Graphics.hpp"

#include "avemotion/backends/direct2d/Direct2DBackend.hpp"
#include "avemotion/evaluation/PropertyEvaluator.hpp"
#include "avemotion/player/Player.hpp"
#include "avemotion/render/RenderPlanner.hpp"
#include "avemotion/render/SourceGeometryProjector.hpp"
#include "avemotion/runtime/Runtime.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>

namespace avemotion::preview {

struct PreviewOptions final {
    std::filesystem::path assetPath;
    std::filesystem::path artifactDirectory;
    bool forceWarp = false;
    bool selfTest = false;
    bool hidden = false;
    bool showHelp = false;
};

class PreviewApplication final {
public:
    explicit PreviewApplication(PreviewOptions options);
    PreviewApplication(const PreviewApplication&) = delete;
    PreviewApplication& operator=(const PreviewApplication&) = delete;
    ~PreviewApplication();

    [[nodiscard]] int run(HINSTANCE instance, int showCommand);

private:
    struct Pipeline;

    [[nodiscard]] bool createWindow(HINSTANCE instance, int showCommand);
    [[nodiscard]] bool initializeGraphics();
    [[nodiscard]] bool loadAsset(const std::filesystem::path& path);
    [[nodiscard]] bool renderAt(runtime::MotionTime time, bool present);
    [[nodiscard]] bool recoverGraphics(std::wstring_view reason);
    [[nodiscard]] bool resizeGraphics(std::uint32_t width, std::uint32_t height);
    [[nodiscard]] bool resizeGraphics(
        std::uint32_t width,
        std::uint32_t height,
        float dpi);
    [[nodiscard]] bool runSelfTest();

    void handleKey(WPARAM key);
    void togglePause();
    void toggleDirection();
    void toggleLoop();
    void seekRelative(double delta);
    void changeRate(double multiplier);
    void updateTitle(runtime::MotionTime now);
    void requestFrame() noexcept;
    void scheduleFrameWakeup(runtime::MotionTime deadline) noexcept;
    void cancelFrameTimer() noexcept;
    void onFrameTimer() noexcept;

    static void playerRequestFrame(void* context) noexcept;
    static void playerScheduleWakeup(
        void* context,
        runtime::MotionTime deadline) noexcept;
    static void playerCancelWakeup(void* context) noexcept;

    [[nodiscard]] runtime::MotionTime now() const noexcept;
    [[nodiscard]] float currentDpi() const noexcept;
    [[nodiscard]] std::filesystem::path defaultAssetPath() const;
    [[nodiscard]] std::filesystem::path executableDirectory() const;

    void showFatal(std::wstring_view title, std::wstring_view message) const;
    void rememberError(std::wstring message);

    static LRESULT CALLBACK windowProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    PreviewOptions options_;
    HINSTANCE applicationInstance_ = nullptr;
    HWND window_ = nullptr;
    HANDLE frameTimer_ = nullptr;
    LARGE_INTEGER qpcFrequency_{};
    bool running_ = false;
    bool minimized_ = false;
    bool frameTimerArmed_ = false;
    bool fatalError_ = false;
    std::wstring lastError_;

    Win32Graphics graphics_;
    backends::direct2d::Backend backend_;
    runtime::Runtime runtime_;
    player::Player player_;
    std::unique_ptr<Pipeline> pipeline_;

    std::uint64_t framesRendered_ = 0;
    std::uint64_t resizeCount_ = 0;
    std::uint64_t deviceRecreationCount_ = 0;
    std::size_t lastFrameIndex_ = 0;
    std::uint64_t lastTitleUpdateNanoseconds_ = 0;
};

[[nodiscard]] PreviewOptions parsePreviewOptions();
void showPreviewUsage();

} // namespace avemotion::preview
