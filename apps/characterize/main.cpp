#include "avemotion/reference/ReferenceRuntime.hpp"
#include "avemotion/reference/UpstreamInfo.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace {
std::string readText(const fs::path& path) {
    std::ifstream stream{path, std::ios::binary};
    return {
        std::istreambuf_iterator<char>{stream},
        std::istreambuf_iterator<char>{}};
}

std::string safeStem(std::string value) {
    for (auto& character : value) {
        const auto byte = static_cast<unsigned char>(character);
        if (!std::isalnum(byte) && character != '-' && character != '_') {
            character = '_';
        }
    }
    return value;
}

std::uint8_t unpremultiply(std::uint8_t component, std::uint8_t alpha) {
    if (alpha == 0U) {
        return 0U;
    }
    const auto expanded =
        (static_cast<unsigned int>(component) * 255U + alpha / 2U) / alpha;
    return static_cast<std::uint8_t>(std::min(expanded, 255U));
}

bool writePpm(const fs::path& path, const avemotion::reference::RenderedFrame& frame) {
    std::ofstream stream{path, std::ios::binary};
    if (!stream) {
        return false;
    }
    stream << "P6\n" << frame.width << ' ' << frame.height << "\n255\n";
    for (const auto pixel : frame.argbPremultiplied) {
        const auto alpha = static_cast<std::uint8_t>((pixel >> 24U) & 0xFFU);
        const auto red = unpremultiply(
            static_cast<std::uint8_t>((pixel >> 16U) & 0xFFU), alpha);
        const auto green = unpremultiply(
            static_cast<std::uint8_t>((pixel >> 8U) & 0xFFU), alpha);
        const auto blue = unpremultiply(
            static_cast<std::uint8_t>(pixel & 0xFFU), alpha);
        const char bytes[] = {
            static_cast<char>(red),
            static_cast<char>(green),
            static_cast<char>(blue)};
        stream.write(bytes, sizeof(bytes));
    }
    return static_cast<bool>(stream);
}

void usage(const char* executable) {
    std::cerr << "Usage: " << executable
              << " --output DIR [--size PIXELS] ASSET.json...\n";
}
} // namespace

int main(int argc, char** argv) {
    fs::path output;
    std::size_t renderSize = 128U;
    std::vector<fs::path> assets;

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--output" && index + 1 < argc) {
            output = argv[++index];
        } else if (argument == "--size" && index + 1 < argc) {
            renderSize = static_cast<std::size_t>(std::stoul(argv[++index]));
        } else if (!argument.empty() && argument.front() == '-') {
            usage(argv[0]);
            return EXIT_FAILURE;
        } else {
            assets.emplace_back(argument);
        }
    }

    if (output.empty() || assets.empty() || renderSize == 0U) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    fs::create_directories(output / "frames");
    std::ofstream manifest{output / "manifest.tsv", std::ios::binary};
    if (!manifest) {
        std::cerr << "Unable to create output manifest\n";
        return EXIT_FAILURE;
    }

    const auto upstream = avemotion::reference::selectedUpstream();
    manifest << "variant\trepository\tcommit\tasset\tasset_fnv64\twidth\theight\tframerate"
                "\tduration\ttotal_frames\tsample\tframe\trender_width\trender_height\tframe_fnv64"
                "\tnontransparent_pixels\talpha_sum\tbounds\n";

    avemotion::reference::ReferenceRuntime runtime;
    std::size_t renderedCount = 0U;
    for (const auto& assetPath : assets) {
        const auto json = readText(assetPath);
        if (json.empty()) {
            std::cerr << "Unable to read asset: " << assetPath << '\n';
            return EXIT_FAILURE;
        }
        const auto assetHash = avemotion::reference::fnv1a64(std::span<const std::byte>{
            reinterpret_cast<const std::byte*>(json.data()), json.size()});

        auto loaded = runtime.loadJson(json, assetPath.filename().string(), false);
        if (!loaded) {
            std::cerr << "Unable to load " << assetPath << ": " << loaded.error << '\n';
            return EXIT_FAILURE;
        }

        const auto& metadata = loaded.animation->metadata();
        const std::vector<std::pair<std::string_view, std::size_t>> samples = {
            {"p000", 0U},
            {"p025", metadata.totalFrames / 4U},
            {"p050", metadata.totalFrames / 2U},
            {"p075", (metadata.totalFrames * 3U) / 4U},
            {"p100", metadata.totalFrames == 0U ? 0U : metadata.totalFrames - 1U},
        };

        for (const auto& [sampleName, frameIndex] : samples) {
            const auto frame = loaded.animation->renderFrame(
                frameIndex, renderSize, renderSize, true);
            const auto stem = safeStem(assetPath.stem().string());
            const auto filename = stem + "_" + std::string{sampleName} + "_f"
                + std::to_string(frame.frameIndex) + ".ppm";
            if (!writePpm(output / "frames" / filename, frame)) {
                std::cerr << "Unable to write frame: " << filename << '\n';
                return EXIT_FAILURE;
            }

            const auto& bounds = frame.nonTransparentBounds;
            manifest << upstream.variant << '\t'
                     << upstream.repository << '\t'
                     << upstream.commit << '\t'
                     << assetPath.filename().string() << '\t'
                     << avemotion::reference::formatHash(assetHash) << '\t'
                     << metadata.width << '\t' << metadata.height << '\t'
                     << std::setprecision(17) << metadata.frameRate << '\t'
                     << metadata.durationSeconds << '\t'
                     << metadata.totalFrames << '\t'
                     << sampleName << '\t'
                     << frame.frameIndex << '\t'
                     << frame.width << '\t' << frame.height << '\t'
                     << avemotion::reference::formatHash(frame.fnv1a64) << '\t'
                     << frame.nonTransparentPixels << '\t'
                     << frame.alphaSum << '\t';
            if (bounds.valid) {
                manifest << bounds.x << ',' << bounds.y << ','
                         << bounds.width << ',' << bounds.height;
            } else {
                manifest << "empty";
            }
            manifest << '\n';
            ++renderedCount;
        }
    }

    std::cout << "Characterized " << assets.size() << " assets and "
              << renderedCount << " frames using " << upstream.variant
              << " rlottie at " << upstream.commit << '\n';
    return EXIT_SUCCESS;
}
