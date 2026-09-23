#include "avemotion/reference/UpstreamInfo.hpp"

#include <cstdlib>
#include <iostream>

int main() {
    const auto upstream = avemotion::reference::selectedUpstream();
    if (upstream.variant.empty() || upstream.repository.empty()
        || upstream.commit.size() != 40U) {
        std::cerr << "Invalid selected upstream metadata\n";
        return EXIT_FAILURE;
    }
    if (!avemotion::reference::manifestMatchesSelectedUpstream()) {
        std::cerr << "Selected upstream does not match UPSTREAM.json\n";
        return EXIT_FAILURE;
    }
    std::cout << upstream.variant << ' ' << upstream.commit << '\n';
    return EXIT_SUCCESS;
}
