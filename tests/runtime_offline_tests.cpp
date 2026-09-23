#include "avemotion/runtime/Runtime.hpp"

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
    require(!avemotion::runtime::Runtime::compiledWithReferenceEngine(),
            "offline runtime must not contain rlottie");
    avemotion::runtime::Runtime runtime;
    const auto loaded = runtime.loadLottieJson("{}", "offline");
    require(!loaded, "offline runtime cannot load Lottie JSON");
    require(loaded.error.code
                == avemotion::runtime::RuntimeErrorCode::ReferenceUnavailable,
            "offline runtime must report ReferenceUnavailable");
    const auto diagnostics = runtime.diagnostics();
    require(diagnostics.assetLoadAttempts == 1U,
            "offline load attempt must be diagnosed");
    require(diagnostics.assetLoadsFailed == 1U,
            "offline load failure must be diagnosed");
    std::cout << "AveMotion offline runtime seam tests passed\n";
    return EXIT_SUCCESS;
}
