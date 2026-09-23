#include "avemotion/core/Hash.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {
[[noreturn]] void fail(const char* message) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

void require(bool condition, const char* message) {
    if (!condition) {
        fail(message);
    }
}
} // namespace

int main() {
    constexpr std::string_view text{"hello"};
    const auto direct = avemotion::core::fnv1a64({
        reinterpret_cast<const std::byte*>(text.data()), text.size()});
    require(direct == 0xa430d84680aabd0bULL,
            "FNV-1a reference vector does not match");
    require(avemotion::core::formatHash(direct) == "a430d84680aabd0b",
            "formatted hash must use sixteen lower-case hexadecimal digits");

    avemotion::core::Fnv1a64 encoded;
    encoded.appendU32(0x01020304U);
    constexpr std::array expectedBytes{
        std::byte{0x04}, std::byte{0x03}, std::byte{0x02}, std::byte{0x01}};
    require(encoded.value() == avemotion::core::fnv1a64(expectedBytes),
            "integer hashing must use canonical little-endian encoding");

    avemotion::core::Fnv1a64 positiveZero;
    avemotion::core::Fnv1a64 negativeZero;
    positiveZero.appendFloat(0.0F);
    negativeZero.appendFloat(-0.0F);
    require(positiveZero.value() == negativeZero.value(),
            "positive and negative zero must have one canonical hash");

    std::cout << "AveMotion core tests passed\n";
    return EXIT_SUCCESS;
}
