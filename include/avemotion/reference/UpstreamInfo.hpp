#pragma once

#include <string_view>

namespace avemotion::reference {

struct UpstreamInfo final {
    std::string_view variant;
    std::string_view repository;
    std::string_view commit;
    std::string_view lineage;
    std::string_view expectedLicenseFamily;
};

[[nodiscard]] UpstreamInfo selectedUpstream() noexcept;
[[nodiscard]] bool manifestMatchesSelectedUpstream();

} // namespace avemotion::reference
