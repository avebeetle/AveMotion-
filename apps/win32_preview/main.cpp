#include "PreviewApp.hpp"

#include <windows.h>

#include <cstdlib>
#include <utility>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    // CMake merges AveMotionPreview.manifest into the linker-generated
    // executable manifest exactly once. This explicit call is a defensive
    // fallback for development hosts that strip or replace embedded manifests.
    static_cast<void>(SetProcessDpiAwarenessContext(
        DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2));

    auto options = avemotion::preview::parsePreviewOptions();
    if (options.showHelp) {
        avemotion::preview::showPreviewUsage();
        return EXIT_SUCCESS;
    }

    avemotion::preview::PreviewApplication application{std::move(options)};
    return application.run(instance, showCommand);
}
