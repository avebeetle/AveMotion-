#include "avemotion/backends/direct2d/Direct2DBackend.hpp"

#include <cstdlib>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<avemotion::backends::direct2d::Backend>);
static_assert(std::is_move_constructible_v<avemotion::backends::direct2d::Backend>);
static_assert(std::is_aggregate_v<avemotion::backends::direct2d::RenderSession>);

int main() {
    avemotion::backends::direct2d::RenderSession session;
    return session.deviceContext == nullptr ? EXIT_SUCCESS : EXIT_FAILURE;
}
