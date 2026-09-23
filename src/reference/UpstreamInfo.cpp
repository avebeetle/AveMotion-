#include "avemotion/reference/UpstreamInfo.hpp"

#include <fstream>
#include <iterator>
#include <string>

namespace avemotion::reference {

UpstreamInfo selectedUpstream() noexcept {
    return {
        AVEMOTION_RLOTTIE_VARIANT_NAME,
        AVEMOTION_RLOTTIE_REPOSITORY,
        AVEMOTION_RLOTTIE_COMMIT,
        "Samsung/rlottie and TelegramMessenger/rlottie reference matrix",
        AVEMOTION_RLOTTIE_LICENSE_FAMILY
    };
}

bool manifestMatchesSelectedUpstream() {
    const std::string path =
        std::string{AVEMOTION_SOURCE_ROOT} + "/third_party/rlottie/UPSTREAM.json";
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        return false;
    }
    const std::string data{
        std::istreambuf_iterator<char>{stream},
        std::istreambuf_iterator<char>{}};
    const auto info = selectedUpstream();
    if (info.variant == "none") {
        return data.find("\"role\": \"offline-only\"") != std::string::npos;
    }
    return data.find(info.repository) != std::string::npos
        && data.find(info.commit) != std::string::npos;
}

} // namespace avemotion::reference
