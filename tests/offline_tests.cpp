#include "avemotion/reference/ReferenceRuntime.hpp"
#include "avemotion/reference/UpstreamInfo.hpp"

#include <cstdlib>
#include <iostream>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
} // namespace

int main() {
    require(!avemotion::reference::ReferenceRuntime::compiledWithRlottie(),
            "offline target must not contain rlottie");
    require(avemotion::reference::manifestMatchesSelectedUpstream(),
            "offline manifest metadata must be available");

    avemotion::reference::ReferenceRuntime runtime;
    const auto result = runtime.inspectJson("{}");
    require(!result.loaded, "offline runtime cannot load JSON");
    require(!result.error.empty(), "offline runtime must explain unavailability");

    std::cout << "AveMotion offline facade tests passed\n";
    return EXIT_SUCCESS;
}
