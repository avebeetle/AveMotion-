#include "PreviewApp.hpp"

#include <d2d1helper.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <string_view>
#include <utility>

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

namespace avemotion::preview {
namespace {

constexpr wchar_t kWindowClassName[] = L"AveMotionWin32PreviewWindow";
constexpr wchar_t kWindowTitle[] = L"AveMotion Win32 Preview";

std::wstring utf8ToWide(std::string_view value) {
    if (value.empty()) return {};
    const auto required = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0);
    if (required <= 0) return L"<invalid UTF-8>";
    std::wstring result(static_cast<std::size_t>(required), L'\0');
    const auto converted = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        result.data(),
        required);
    return converted == required ? result : L"<invalid UTF-8>";
}

std::string wideToUtf8(std::wstring_view value) {
    if (value.empty()) return {};
    const auto required = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (required <= 0) return "<invalid UTF-16>";
    std::string result(static_cast<std::size_t>(required), '\0');
    const auto converted = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        result.data(),
        required,
        nullptr,
        nullptr);
    return converted == required ? result : "<invalid UTF-16>";
}

std::string pathUtf8(const std::filesystem::path& path) {
    return wideToUtf8(path.wstring());
}

std::vector<std::byte> readBytes(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return {};
    const std::string data{
        std::istreambuf_iterator<char>{stream},
        std::istreambuf_iterator<char>{},
    };
    std::vector<std::byte> bytes(data.size());
    for (std::size_t index = 0U; index < data.size(); ++index) {
        bytes[index] = static_cast<std::byte>(
            static_cast<unsigned char>(data[index]));
    }
    return bytes;
}

std::wstring playbackStatusName(runtime::PlaybackStatus value) {
    switch (value) {
    case runtime::PlaybackStatus::Stopped: return L"Stopped";
    case runtime::PlaybackStatus::Playing: return L"Playing";
    case runtime::PlaybackStatus::Paused: return L"Paused";
    case runtime::PlaybackStatus::Controlled: return L"Controlled";
    case runtime::PlaybackStatus::Holding: return L"Holding";
    }
    return L"Unknown";
}

std::wstring directionName(runtime::PlaybackDirection value) {
    return value == runtime::PlaybackDirection::Forward ? L"Forward" : L"Reverse";
}

std::wstring loopName(runtime::PlaybackLoopMode value) {
    return value == runtime::PlaybackLoopMode::Loop ? L"Loop" : L"Once";
}

std::uint32_t dipDimension(std::uint32_t pixels, float dpi) noexcept {
    if (!std::isfinite(dpi) || dpi <= 0.0F || pixels == 0U) return 0U;
    const auto value = std::llround(
        static_cast<long double>(pixels) * 96.0L / dpi);
    return static_cast<std::uint32_t>(std::max<std::int64_t>(1, value));
}

std::wstring formatGraphicsError(const GraphicsError& error) {
    std::wostringstream stream;
    stream << error.message;
    if (FAILED(error.nativeCode)) {
        stream << L" (HRESULT 0x" << std::hex << std::uppercase
               << static_cast<std::uint32_t>(error.nativeCode) << L')';
    }
    return stream.str();
}

std::wstring formatBackendError(
    const backends::direct2d::BackendError& error) {
    std::wostringstream stream;
    stream << utf8ToWide(error.message);
    if (FAILED(error.nativeCode)) {
        stream << L" (HRESULT 0x" << std::hex << std::uppercase
               << static_cast<std::uint32_t>(error.nativeCode) << L')';
    }
    return stream.str();
}

} // namespace

struct PreviewApplication::Pipeline final {
    std::shared_ptr<const runtime::Asset> asset;
    std::shared_ptr<runtime::Instance> instance;
    player::PlayerHandle playerHandle;
    std::shared_ptr<const model::MotionAssetModel> model;
    std::unique_ptr<evaluation::PropertyEvaluator> evaluator;
    evaluation::PropertyEvaluationWorkspace propertyWorkspace;
    std::unique_ptr<render::SourceGeometryProjector> projector;
    render::SourceGeometryProjectionWorkspace projectionWorkspace;
    render::MotionRenderPlanner planner;
    std::filesystem::path assetPath;
    std::uint64_t propertyStorageGeneration = 0;
    std::uint64_t projectionStorageGeneration = 0;
};

PreviewApplication::PreviewApplication(PreviewOptions options)
    : options_(std::move(options)),
      player_({
          .userData = this,
          .requestFrame = &PreviewApplication::playerRequestFrame,
          .scheduleWakeup = &PreviewApplication::playerScheduleWakeup,
          .cancelWakeup = &PreviewApplication::playerCancelWakeup,
      }) {
    QueryPerformanceFrequency(&qpcFrequency_);
}

PreviewApplication::~PreviewApplication() {
    cancelFrameTimer();
    if (frameTimer_ != nullptr) {
        CloseHandle(frameTimer_);
        frameTimer_ = nullptr;
    }
}

int PreviewApplication::run(HINSTANCE instance, int showCommand) {
    applicationInstance_ = instance;
    if (qpcFrequency_.QuadPart <= 0) {
        showFatal(L"AveMotion", L"QueryPerformanceFrequency is unavailable.");
        return EXIT_FAILURE;
    }

    frameTimer_ = CreateWaitableTimerExW(
        nullptr,
        nullptr,
        CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
        TIMER_ALL_ACCESS);
    if (frameTimer_ == nullptr) {
        frameTimer_ = CreateWaitableTimerExW(
            nullptr,
            nullptr,
            0U,
            TIMER_ALL_ACCESS);
    }
    if (frameTimer_ == nullptr) {
        showFatal(L"AveMotion", L"Unable to create the frame waitable timer.");
        return EXIT_FAILURE;
    }

    if (!createWindow(instance, showCommand) || !initializeGraphics()) {
        return EXIT_FAILURE;
    }

    auto assetPath = options_.assetPath;
    if (assetPath.empty()) assetPath = defaultAssetPath();
    if (!loadAsset(assetPath)) {
        return EXIT_FAILURE;
    }

    if (options_.selfTest) {
        const auto passed = runSelfTest();
        if (window_ != nullptr) DestroyWindow(window_);
        return passed ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (!options_.hidden) {
        ShowWindow(window_, showCommand == 0 ? SW_SHOWNORMAL : showCommand);
        UpdateWindow(window_);
    }
    running_ = true;
    requestFrame();

    MSG message{};
    while (running_) {
        const auto waitResult = MsgWaitForMultipleObjectsEx(
            1U,
            &frameTimer_,
            INFINITE,
            QS_ALLINPUT,
            MWMO_INPUTAVAILABLE);
        if (waitResult == WAIT_OBJECT_0) {
            onFrameTimer();
        } else if (waitResult == WAIT_FAILED) {
            rememberError(L"MsgWaitForMultipleObjectsEx failed.");
            break;
        }

        while (PeekMessageW(&message, nullptr, 0U, 0U, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                running_ = false;
                break;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    return fatalError_ ? EXIT_FAILURE : static_cast<int>(message.wParam);
}

bool PreviewApplication::createWindow(HINSTANCE instance, int showCommand) {
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = &PreviewApplication::windowProcedure;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hbrBackground = nullptr;
    windowClass.lpszClassName = kWindowClassName;
    if (RegisterClassExW(&windowClass) == 0U
        && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        showFatal(L"AveMotion", L"Unable to register the Win32 preview class.");
        return false;
    }

    const auto dpi = GetDpiForSystem();
    RECT rectangle{0, 0, 960, 640};
    AdjustWindowRectExForDpi(
        &rectangle,
        WS_OVERLAPPEDWINDOW,
        FALSE,
        0U,
        dpi);
    window_ = CreateWindowExW(
        0U,
        kWindowClassName,
        kWindowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rectangle.right - rectangle.left,
        rectangle.bottom - rectangle.top,
        nullptr,
        nullptr,
        instance,
        this);
    if (window_ == nullptr) {
        showFatal(L"AveMotion", L"Unable to create the Win32 preview window.");
        return false;
    }
    if (options_.hidden || options_.selfTest) {
        ShowWindow(window_, SW_HIDE);
    } else if (showCommand != 0) {
        ShowWindow(window_, showCommand);
    }
    return true;
}

bool PreviewApplication::initializeGraphics() {
    RECT client{};
    GetClientRect(window_, &client);
    const auto width = static_cast<std::uint32_t>(std::max<LONG>(1, client.right));
    const auto height = static_cast<std::uint32_t>(std::max<LONG>(1, client.bottom));
    GraphicsError error;
    if (!graphics_.initialize(
            window_,
            options_.forceWarp || options_.selfTest,
            width,
            height,
            currentDpi(),
            error)) {
        showFatal(L"AveMotion graphics initialization", formatGraphicsError(error));
        return false;
    }
    return true;
}

bool PreviewApplication::loadAsset(const std::filesystem::path& path) {
    const auto bytes = readBytes(path);
    if (bytes.empty()) {
        showFatal(L"AveMotion asset", L"Unable to read Lottie JSON or TGS asset:\n" + path.wstring());
        return false;
    }

    auto loaded = runtime_.loadAssetData(bytes, pathUtf8(path.filename()));
    if (!loaded) {
        showFatal(L"AveMotion asset", utf8ToWide(loaded.error.message));
        return false;
    }
    const auto prepared = loaded.asset->prepareModel();
    if (!prepared) {
        showFatal(L"AveMotion canonical model", utf8ToWide(prepared.error));
        return false;
    }
    auto created = runtime_.createInstance(loaded.asset);
    if (!created) {
        showFatal(L"AveMotion instance", utf8ToWide(created.error.message));
        return false;
    }

    auto next = std::make_unique<Pipeline>();
    next->asset = std::move(loaded.asset);
    next->instance = std::shared_ptr<runtime::Instance>{
        std::move(created.instance)};
    next->model = prepared.model;
    next->evaluator = std::make_unique<evaluation::PropertyEvaluator>(next->model);
    if (!next->evaluator->valid()) {
        showFatal(L"AveMotion evaluator", utf8ToWide(next->evaluator->errorMessage()));
        return false;
    }
    next->evaluator->prepare(next->propertyWorkspace);
    next->propertyStorageGeneration = next->propertyWorkspace.storageGeneration();
    next->projector = std::make_unique<render::SourceGeometryProjector>(next->model);
    if (!next->projector->valid()) {
        showFatal(L"AveMotion geometry projector", utf8ToWide(next->projector->errorMessage()));
        return false;
    }
    if (!next->projector->prepare(next->projectionWorkspace)) {
        showFatal(L"AveMotion geometry projector", L"Unable to prepare retained geometry workspace.");
        return false;
    }
    next->projectionStorageGeneration = next->projectionWorkspace.storageGeneration();
    next->assetPath = path;

    const auto time = now();
    if (pipeline_ && pipeline_->playerHandle.valid()) {
        static_cast<void>(player_.removeInstance(pipeline_->playerHandle, time));
    }
    const auto registered = player_.addInstance(next->instance, time, {
        .maximumPresentationRate = 60.0,
        .visible = !minimized_,
        .hiddenTimePolicy = player::HiddenTimePolicy::Freeze,
    });
    if (!registered) {
        showFatal(L"AveMotion player", utf8ToWide(registered.error.message));
        return false;
    }
    next->playerHandle = registered.handle;
    static_cast<void>(player_.setLoopMode(
        next->playerHandle,
        runtime::PlaybackLoopMode::Loop,
        time));
    static_cast<void>(player_.play(next->playerHandle, time));

    pipeline_ = std::move(next);
    backend_.clear();
    framesRendered_ = 0U;
    lastFrameIndex_ = 0U;
    updateTitle(time);
    requestFrame();
    return true;
}

bool PreviewApplication::renderAt(runtime::MotionTime time, bool present) {
    if (!pipeline_ || minimized_ || !graphics_.ready()) return true;

    const auto snapshot = pipeline_->instance->playbackSnapshot(time);
    lastFrameIndex_ = snapshot.frameIndex;
    const auto properties = pipeline_->evaluator->evaluate(
        static_cast<double>(snapshot.frameIndex),
        pipeline_->propertyWorkspace);
    if (!properties) {
        rememberError(L"Property evaluation failed.");
        return false;
    }

    const auto logicalWidth = dipDimension(graphics_.pixelWidth(), graphics_.dpi());
    const auto logicalHeight = dipDimension(graphics_.pixelHeight(), graphics_.dpi());
    auto evaluated = pipeline_->instance->evaluateAt(
        time,
        logicalWidth,
        logicalHeight);
    if (!evaluated) {
        rememberError(L"Scene evaluation failed: " + utf8ToWide(evaluated.error.message));
        return false;
    }

    const auto projected = pipeline_->projector->project(
        evaluated.scene,
        properties,
        pipeline_->projectionWorkspace);
    if (!projected) {
        rememberError(L"Source geometry projection failed.");
        return false;
    }
    if (pipeline_->propertyWorkspace.storageGeneration()
            != pipeline_->propertyStorageGeneration
        || pipeline_->projectionWorkspace.storageGeneration()
            != pipeline_->projectionStorageGeneration) {
        rememberError(L"A retained evaluator/projector workspace grew after prepare().");
        return false;
    }

    auto planned = pipeline_->planner.build(std::move(evaluated.scene));
    if (!planned) {
        rememberError(L"Render-plan construction failed: " + utf8ToWide(planned.error.message));
        return false;
    }

    GraphicsError graphicsError;
    if (!graphics_.beginFrame(D2D1::ColorF(0x10141D), graphicsError)) {
        if (Win32Graphics::isDeviceLoss(graphicsError.nativeCode)) {
            return recoverGraphics(formatGraphicsError(graphicsError));
        }
        rememberError(formatGraphicsError(graphicsError));
        return false;
    }

    const auto drawResult = backend_.draw(planned.plan, {
        .deviceContext = graphics_.deviceContext(),
        .graphicsDomainId = graphics_.graphicsDomainId(),
        .graphicsGeneration = graphics_.graphicsGeneration(),
    });
    if (!drawResult) {
        // EndDraw is still required after a backend error so the host-owned
        // device context is returned to a defined state.
        GraphicsError endError;
        static_cast<void>(graphics_.endFrame(endError));
        if (drawResult.error.code
                == backends::direct2d::BackendErrorCode::DeviceLost
            || Win32Graphics::isDeviceLoss(
                static_cast<HRESULT>(drawResult.error.nativeCode))) {
            return recoverGraphics(formatBackendError(drawResult.error));
        }
        rememberError(formatBackendError(drawResult.error));
        return false;
    }

    if (!graphics_.endFrame(graphicsError)) {
        if (Win32Graphics::isDeviceLoss(graphicsError.nativeCode)) {
            return recoverGraphics(formatGraphicsError(graphicsError));
        }
        rememberError(formatGraphicsError(graphicsError));
        return false;
    }
    if (present && !graphics_.present(graphicsError)) {
        if (Win32Graphics::isDeviceLoss(graphicsError.nativeCode)) {
            return recoverGraphics(formatGraphicsError(graphicsError));
        }
        rememberError(formatGraphicsError(graphicsError));
        return false;
    }

    ++framesRendered_;
    updateTitle(time);
    return true;
}

bool PreviewApplication::recoverGraphics(std::wstring_view reason) {
    backend_.clear();
    GraphicsError error;
    if (!graphics_.recreate(error)) {
        rememberError(L"Graphics recreation failed after: " + std::wstring{reason}
            + L"\n" + formatGraphicsError(error));
        return false;
    }
    ++deviceRecreationCount_;
    requestFrame();
    return true;
}

bool PreviewApplication::resizeGraphics(std::uint32_t width, std::uint32_t height) {
    return resizeGraphics(width, height, currentDpi());
}

bool PreviewApplication::resizeGraphics(
    std::uint32_t width,
    std::uint32_t height,
    float dpi) {
    if (width == 0U || height == 0U) return true;
    GraphicsError error;
    if (!graphics_.resize(width, height, dpi, error)) {
        if (Win32Graphics::isDeviceLoss(error.nativeCode)) {
            return recoverGraphics(formatGraphicsError(error));
        }
        rememberError(formatGraphicsError(error));
        return false;
    }
    ++resizeCount_;
    requestFrame();
    return true;
}

bool PreviewApplication::runSelfTest() {
    if (!pipeline_) return false;
    cancelFrameTimer();
    const auto selfTestZero = runtime::MotionTime{};
    static_cast<void>(player_.stop(pipeline_->playerHandle, selfTestZero));
    static_cast<void>(player_.setDirection(
        pipeline_->playerHandle,
        runtime::PlaybackDirection::Forward,
        selfTestZero));
    static_cast<void>(player_.setLoopMode(
        pipeline_->playerHandle,
        runtime::PlaybackLoopMode::Loop,
        selfTestZero));
    static_cast<void>(player_.setPlaybackRate(
        pipeline_->playerHandle,
        1.0,
        selfTestZero));
    static_cast<void>(player_.play(pipeline_->playerHandle, selfTestZero));
    static_cast<void>(player_.tick(
        selfTestZero,
        player::FrameSelection::AllVisible));

    constexpr std::size_t frameCount = 150U;
    std::unordered_set<std::size_t> uniqueFrames;
    std::optional<std::size_t> pausedFrame;
    std::optional<std::size_t> laterPausedFrame;
    bool passed = true;
    std::wstring failure;

    for (std::size_t index = 0U; index < frameCount; ++index) {
        const auto time = runtime::MotionTime::fromSeconds(
            static_cast<double>(index) / 60.0);
        if (index == 24U) {
            static_cast<void>(player_.pause(pipeline_->playerHandle, time));
            pausedFrame = pipeline_->instance->playbackSnapshot(time).frameIndex;
        } else if (index == 30U) {
            laterPausedFrame = pipeline_->instance->playbackSnapshot(time).frameIndex;
        } else if (index == 36U) {
            static_cast<void>(player_.resume(pipeline_->playerHandle, time));
        } else if (index == 54U) {
            static_cast<void>(player_.setDirection(
                pipeline_->playerHandle,
                runtime::PlaybackDirection::Reverse,
                time));
        } else if (index == 72U) {
            static_cast<void>(player_.setDirection(
                pipeline_->playerHandle,
                runtime::PlaybackDirection::Forward,
                time));
        } else if (index == 84U) {
            static_cast<void>(player_.seekNormalized(
                pipeline_->playerHandle,
                0.25,
                time));
        } else if (index == 96U) {
            if (!resizeGraphics(800U, 450U, 144.0F)) {
                passed = false;
                failure = L"resize lifecycle failed";
                break;
            }
        } else if (index == 114U) {
            if (!recoverGraphics(L"forced Part 20 self-test recreation")) {
                passed = false;
                failure = L"device recreation lifecycle failed";
                break;
            }
        } else if (index == 126U) {
            static_cast<void>(player_.setPlaybackRate(
                pipeline_->playerHandle,
                1.5,
                time));
        }

        const auto scheduled = player_.tick(
            time,
            player::FrameSelection::AllVisible);
        const auto scheduledEntry = std::find_if(
            scheduled.frames.begin(),
            scheduled.frames.end(),
            [this](const auto& frame) {
                return frame.handle == pipeline_->playerHandle;
            });
        if (scheduledEntry == scheduled.frames.end()) {
            passed = false;
            failure = L"central player omitted the visible preview instance";
            break;
        }

        const auto snapshot = pipeline_->instance->playbackSnapshot(time);
        uniqueFrames.insert(snapshot.frameIndex);
        if (!renderAt(time, true)) {
            passed = false;
            failure = L"live host render failed";
            break;
        }
    }

    if (passed && (!pausedFrame || !laterPausedFrame
        || *pausedFrame != *laterPausedFrame)) {
        passed = false;
        failure = L"pause did not hold the exact frame";
    }
    if (passed && uniqueFrames.size() < 10U) {
        passed = false;
        failure = L"absolute-time playback visited too few unique frames";
    }

    const auto finalTime = runtime::MotionTime::fromSeconds(2.75);
    if (passed && !renderAt(finalTime, false)) {
        passed = false;
        failure = L"final non-presented capture frame failed";
    }

    const auto artifactDirectory = options_.artifactDirectory.empty()
        ? executableDirectory() / L"win32-preview-artifacts"
        : options_.artifactDirectory;
    GraphicsError captureError;
    if (passed && !graphics_.saveBackBufferPpm(
            artifactDirectory / L"avemotion-win32-preview-selftest.ppm",
            captureError)) {
        passed = false;
        failure = L"preview capture failed: " + formatGraphicsError(captureError);
    }

    const auto backendDiagnostics = backend_.diagnostics();
    const auto playerDiagnostics = player_.diagnostics();
    if (passed && (framesRendered_ < frameCount
        || backendDiagnostics.drawCalls < frameCount
        || backendDiagnostics.geometryCacheHits == 0U
        || backendDiagnostics.geometryCacheMisses == 0U
        || backendDiagnostics.resourceDomainResets < 2U
        || playerDiagnostics.ticks < frameCount
        || playerDiagnostics.framesReturned < frameCount
        || playerDiagnostics.wakeupsScheduled == 0U
        || resizeCount_ == 0U
        || deviceRecreationCount_ == 0U)) {
        passed = false;
        failure = L"preview lifecycle diagnostics did not exercise all required paths";
    }

    std::error_code filesystemError;
    std::filesystem::create_directories(artifactDirectory, filesystemError);
    if (filesystemError) {
        std::wcerr << L"Unable to create the preview artifact directory: "
                   << utf8ToWide(filesystemError.message()) << L'\n';
        return false;
    }
    std::ofstream report(
        artifactDirectory / L"avemotion-win32-preview-selftest.txt",
        std::ios::binary);
    if (!report) {
        std::wcerr << L"Unable to create the preview self-test report.\n";
        return false;
    }
    report << "status=" << (passed ? "pass" : "fail") << '\n'
           << "asset=" << pathUtf8(pipeline_->assetPath) << '\n'
           << "framesRendered=" << framesRendered_ << '\n'
           << "uniqueFrames=" << uniqueFrames.size() << '\n'
           << "resizes=" << resizeCount_ << '\n'
           << "deviceRecreations=" << deviceRecreationCount_ << '\n'
           << "finalDpi=" << graphics_.dpi() << '\n'
           << "finalPixelWidth=" << graphics_.pixelWidth() << '\n'
           << "finalPixelHeight=" << graphics_.pixelHeight() << '\n'
           << "drawCalls=" << backendDiagnostics.drawCalls << '\n'
           << "geometryCacheHits=" << backendDiagnostics.geometryCacheHits << '\n'
           << "geometryCacheMisses=" << backendDiagnostics.geometryCacheMisses << '\n'
           << "geometryResourcesCreated="
           << backendDiagnostics.geometryResourcesCreated << '\n'
           << "resourceDomainResets="
           << backendDiagnostics.resourceDomainResets << '\n'
           << "playerTicks=" << playerDiagnostics.ticks << '\n'
           << "playerFramesReturned=" << playerDiagnostics.framesReturned << '\n'
           << "playerWakeupsScheduled=" << playerDiagnostics.wakeupsScheduled << '\n'
           << "playerSkippedDeadlines=" << playerDiagnostics.skippedDeadlines << '\n'
           << "usingWarp=" << (graphics_.usingWarp() ? 1 : 0) << '\n';
    if (!passed) {
        report << "reason=";
        const auto utf8 = wideToUtf8(failure);
        report.write(
            utf8.data(),
            static_cast<std::streamsize>(utf8.size()));
        report << '\n';
    }
    report.close();
    if (!report) {
        std::wcerr << L"Unable to finish writing the preview self-test report.\n";
        return false;
    }
    return passed;
}

void PreviewApplication::handleKey(WPARAM key) {
    switch (key) {
    case VK_SPACE: togglePause(); break;
    case 'R': toggleDirection(); break;
    case 'L': toggleLoop(); break;
    case VK_LEFT: seekRelative(-0.05); break;
    case VK_RIGHT: seekRelative(0.05); break;
    case VK_HOME:
        if (pipeline_) {
            static_cast<void>(player_.seekNormalized(
                pipeline_->playerHandle,
                0.0,
                now()));
        }
        break;
    case VK_END:
        if (pipeline_) {
            static_cast<void>(player_.seekNormalized(
                pipeline_->playerHandle,
                1.0,
                now()));
        }
        break;
    case VK_OEM_PLUS:
    case VK_ADD: changeRate(1.25); break;
    case VK_OEM_MINUS:
    case VK_SUBTRACT: changeRate(0.8); break;
    case VK_F5:
        static_cast<void>(recoverGraphics(L"manual F5 recreation"));
        break;
    case VK_ESCAPE:
        DestroyWindow(window_);
        break;
    default:
        break;
    }
    updateTitle(now());
}

void PreviewApplication::togglePause() {
    if (!pipeline_) return;
    const auto time = now();
    const auto snapshot = pipeline_->instance->playbackSnapshot(time);
    if (snapshot.status == runtime::PlaybackStatus::Playing) {
        static_cast<void>(player_.pause(pipeline_->playerHandle, time));
    } else {
        static_cast<void>(player_.resume(pipeline_->playerHandle, time));
    }
}

void PreviewApplication::toggleDirection() {
    if (!pipeline_) return;
    const auto time = now();
    const auto snapshot = pipeline_->instance->playbackSnapshot(time);
    static_cast<void>(player_.setDirection(
        pipeline_->playerHandle,
        snapshot.direction == runtime::PlaybackDirection::Forward
            ? runtime::PlaybackDirection::Reverse
            : runtime::PlaybackDirection::Forward,
        time));
}

void PreviewApplication::toggleLoop() {
    if (!pipeline_) return;
    const auto time = now();
    const auto snapshot = pipeline_->instance->playbackSnapshot(time);
    static_cast<void>(player_.setLoopMode(
        pipeline_->playerHandle,
        snapshot.loopMode == runtime::PlaybackLoopMode::Loop
            ? runtime::PlaybackLoopMode::Once
            : runtime::PlaybackLoopMode::Loop,
        time));
}

void PreviewApplication::seekRelative(double delta) {
    if (!pipeline_) return;
    const auto time = now();
    const auto snapshot = pipeline_->instance->playbackSnapshot(time);
    static_cast<void>(player_.seekNormalized(
        pipeline_->playerHandle,
        snapshot.normalizedPosition + delta,
        time));
}

void PreviewApplication::changeRate(double multiplier) {
    if (!pipeline_) return;
    const auto time = now();
    const auto snapshot = pipeline_->instance->playbackSnapshot(time);
    const auto rate = std::clamp(snapshot.playbackRate * multiplier, 0.125, 8.0);
    static_cast<void>(player_.setPlaybackRate(
        pipeline_->playerHandle,
        rate,
        time));
}

void PreviewApplication::updateTitle(runtime::MotionTime time) {
    if (!window_ || !pipeline_) return;
    const auto nowNanoseconds = static_cast<std::uint64_t>(
        std::max<std::int64_t>(0, time.nanoseconds));
    if (!options_.selfTest && framesRendered_ != 0U
        && nowNanoseconds - lastTitleUpdateNanoseconds_ < 200'000'000ULL) {
        return;
    }
    lastTitleUpdateNanoseconds_ = nowNanoseconds;
    const auto snapshot = pipeline_->instance->playbackSnapshot(time);
    const auto diagnostics = backend_.diagnostics();
    const auto playerDiagnostics = player_.diagnostics();
    std::wostringstream title;
    title << L"AveMotion | " << pipeline_->assetPath.filename().wstring()
          << L" | " << playbackStatusName(snapshot.status)
          << L" | " << directionName(snapshot.direction)
          << L" | " << loopName(snapshot.loopMode)
          << L" | frame " << snapshot.frameIndex + 1U << L'/'
          << pipeline_->asset->metadata().totalFrames
          << L" | x" << std::fixed << std::setprecision(2)
          << snapshot.playbackRate
          << L" | " << static_cast<unsigned>(std::lround(graphics_.dpi()))
          << L" DPI | " << (graphics_.usingWarp() ? L"WARP" : L"Hardware")
          << L" | G hit/miss " << diagnostics.geometryCacheHits
          << L'/' << diagnostics.geometryCacheMisses
          << L" | P skip " << playerDiagnostics.skippedDeadlines;
    SetWindowTextW(window_, title.str().c_str());
}

void PreviewApplication::requestFrame() noexcept {
    if (window_ && !minimized_) {
        InvalidateRect(window_, nullptr, FALSE);
    }
}

void PreviewApplication::scheduleFrameWakeup(
    runtime::MotionTime deadline) noexcept {
    if (!frameTimer_ || minimized_) {
        cancelFrameTimer();
        return;
    }
    const auto current = now();
    const auto rawDelta = static_cast<long double>(deadline.nanoseconds)
        - static_cast<long double>(current.nanoseconds);
    const auto deltaNanoseconds = rawDelta <= 1.0L
        ? std::int64_t{1}
        : rawDelta >= static_cast<long double>(
              std::numeric_limits<std::int64_t>::max())
            ? std::numeric_limits<std::int64_t>::max()
            : static_cast<std::int64_t>(rawDelta);
    const auto hundredNanoseconds = std::max<std::int64_t>(
        1,
        deltaNanoseconds / 100 + (deltaNanoseconds % 100 != 0 ? 1 : 0));
    LARGE_INTEGER due{};
    due.QuadPart = -hundredNanoseconds;
    if (SetWaitableTimerEx(
            frameTimer_,
            &due,
            0,
            nullptr,
            nullptr,
            nullptr,
            0U)) {
        frameTimerArmed_ = true;
        return;
    }
    frameTimerArmed_ = false;
    rememberError(L"SetWaitableTimerEx failed while scheduling the AveMotion deadline.");
}

void PreviewApplication::cancelFrameTimer() noexcept {
    if (frameTimer_ && frameTimerArmed_) {
        CancelWaitableTimer(frameTimer_);
    }
    frameTimerArmed_ = false;
}

void PreviewApplication::onFrameTimer() noexcept {
    frameTimerArmed_ = false;
    // The portable Player owns deadline advancement. The Win32 adapter only
    // maps the one-shot wakeup to a host repaint request; WM_PAINT calls tick()
    // and publishes the next nearest deadline.
    requestFrame();
}

void PreviewApplication::playerRequestFrame(void* context) noexcept {
    static_cast<PreviewApplication*>(context)->requestFrame();
}

void PreviewApplication::playerScheduleWakeup(
    void* context,
    runtime::MotionTime deadline) noexcept {
    static_cast<PreviewApplication*>(context)->scheduleFrameWakeup(deadline);
}

void PreviewApplication::playerCancelWakeup(void* context) noexcept {
    static_cast<PreviewApplication*>(context)->cancelFrameTimer();
}

runtime::MotionTime PreviewApplication::now() const noexcept {
    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter);
    const auto nanoseconds = static_cast<long double>(counter.QuadPart)
        * 1'000'000'000.0L / qpcFrequency_.QuadPart;
    if (nanoseconds >= static_cast<long double>(
            std::numeric_limits<std::int64_t>::max())) {
        return runtime::MotionTime::fromNanoseconds(
            std::numeric_limits<std::int64_t>::max());
    }
    return runtime::MotionTime::fromNanoseconds(
        static_cast<std::int64_t>(std::llround(nanoseconds)));
}

float PreviewApplication::currentDpi() const noexcept {
    return window_ != nullptr ? static_cast<float>(GetDpiForWindow(window_)) : 96.0F;
}

std::filesystem::path PreviewApplication::defaultAssetPath() const {
    return executableDirectory()
        / L"preview-assets"
        / L"repeater_content_group.tgs";
}

std::filesystem::path PreviewApplication::executableDirectory() const {
    std::wstring buffer(32'768U, L'\0');
    const auto length = GetModuleFileNameW(
        nullptr,
        buffer.data(),
        static_cast<DWORD>(buffer.size()));
    if (length == 0U || length >= buffer.size()) {
        return std::filesystem::current_path();
    }
    buffer.resize(length);
    return std::filesystem::path{buffer}.parent_path();
}

void PreviewApplication::showFatal(
    std::wstring_view title,
    std::wstring_view message) const {
    if (!options_.selfTest) {
        MessageBoxW(
            window_,
            std::wstring{message}.c_str(),
            std::wstring{title}.c_str(),
            MB_OK | MB_ICONERROR);
    }
}

void PreviewApplication::rememberError(std::wstring message) {
    fatalError_ = true;
    lastError_ = std::move(message);
    cancelFrameTimer();
    showFatal(L"AveMotion preview failure", lastError_);
    if (window_) PostMessageW(window_, WM_CLOSE, 0U, 0U);
}

LRESULT CALLBACK PreviewApplication::windowProcedure(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {
    auto* application = reinterpret_cast<PreviewApplication*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        application = static_cast<PreviewApplication*>(create->lpCreateParams);
        SetWindowLongPtrW(
            window,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(application));
        application->window_ = window;
    }
    return application != nullptr
        ? application->handleMessage(message, wParam, lParam)
        : DefWindowProcW(window, message, wParam, lParam);
}

LRESULT PreviewApplication::handleMessage(
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {
    switch (message) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        BeginPaint(window_, &paint);
        const auto time = now();
        const auto scheduled = player_.tick(
            time,
            player::FrameSelection::AllVisible);
        bool shouldRender = false;
        if (pipeline_) {
            const auto entry = std::find_if(
                scheduled.frames.begin(),
                scheduled.frames.end(),
                [this](const auto& frame) {
                    return frame.handle == pipeline_->playerHandle;
                });
            shouldRender = entry != scheduled.frames.end() && entry->visible;
        }
        const auto rendered = !shouldRender || renderAt(time, true);
        EndPaint(window_, &paint);
        if (!rendered && !fatalError_) {
            rememberError(L"The Win32 preview frame failed.");
        }
        return 0;
    }
    case WM_KEYDOWN:
        handleKey(wParam);
        return 0;
    case WM_SIZE: {
        const auto wasMinimized = minimized_;
        minimized_ = wParam == SIZE_MINIMIZED;
        if (pipeline_ && minimized_ != wasMinimized) {
            static_cast<void>(player_.setVisible(
                pipeline_->playerHandle,
                !minimized_,
                now()));
        }
        if (minimized_) {
            cancelFrameTimer();
            return 0;
        }
        if (graphics_.ready()) {
            const auto width = static_cast<std::uint32_t>(LOWORD(lParam));
            const auto height = static_cast<std::uint32_t>(HIWORD(lParam));
            if (!resizeGraphics(width, height)) return 0;
        }
        requestFrame();
        return 0;
    }
    case WM_DPICHANGED: {
        const auto* rectangle = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(
            window_,
            nullptr,
            rectangle->left,
            rectangle->top,
            rectangle->right - rectangle->left,
            rectangle->bottom - rectangle->top,
            SWP_NOACTIVATE | SWP_NOZORDER);
        RECT client{};
        GetClientRect(window_, &client);
        static_cast<void>(resizeGraphics(
            static_cast<std::uint32_t>(std::max<LONG>(1, client.right)),
            static_cast<std::uint32_t>(std::max<LONG>(1, client.bottom))));
        return 0;
    }
    case WM_DISPLAYCHANGE:
        requestFrame();
        return 0;
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize.x = 360;
        info->ptMinTrackSize.y = 260;
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(window_);
        return 0;
    case WM_DESTROY:
        running_ = false;
        cancelFrameTimer();
        PostQuitMessage(fatalError_ ? EXIT_FAILURE : EXIT_SUCCESS);
        return 0;
    default:
        return DefWindowProcW(window_, message, wParam, lParam);
    }
}

PreviewOptions parsePreviewOptions() {
    PreviewOptions options;
    int count = 0;
    auto arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (arguments == nullptr) return options;
    for (int index = 1; index < count; ++index) {
        const std::wstring_view argument{arguments[index]};
        if (argument == L"--asset" && index + 1 < count) {
            options.assetPath = arguments[++index];
        } else if (argument == L"--artifact-dir" && index + 1 < count) {
            options.artifactDirectory = arguments[++index];
        } else if (argument == L"--warp") {
            options.forceWarp = true;
        } else if (argument == L"--self-test") {
            options.selfTest = true;
            options.hidden = true;
            options.forceWarp = true;
        } else if (argument == L"--hidden") {
            options.hidden = true;
        } else if (argument == L"--help" || argument == L"-h") {
            options.showHelp = true;
        } else if (!argument.empty() && argument.front() != L'-'
            && options.assetPath.empty()) {
            options.assetPath = arguments[index];
        }
    }
    LocalFree(arguments);
    return options;
}

void showPreviewUsage() {
    MessageBoxW(
        nullptr,
        L"AveMotion Win32 Preview\n\n"
        L"Command line:\n"
        L"  avemotion_win32_preview.exe [asset.json|asset.tgs] [--warp]\n\n"
        L"Controls:\n"
        L"  Space     Play / Pause\n"
        L"  R         Reverse direction\n"
        L"  L         Loop / Once\n"
        L"  Left/Right  Seek 5%\n"
        L"  Home/End  Seek to endpoint\n"
        L"  +/-       Playback speed\n"
        L"  F5        Recreate D3D11 / Direct2D device\n"
        L"  Escape    Close\n",
        L"AveMotion Win32 Preview",
        MB_OK | MB_ICONINFORMATION);
}

} // namespace avemotion::preview
