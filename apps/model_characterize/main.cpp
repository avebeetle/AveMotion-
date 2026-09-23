#include "avemotion/core/Hash.hpp"
#include "avemotion/reference/UpstreamInfo.hpp"
#include "avemotion/runtime/Runtime.hpp"

#include <bit>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <string>
#include <sstream>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace {
std::string readText(const fs::path& path) {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) return {};
    return {
        std::istreambuf_iterator<char>{stream},
        std::istreambuf_iterator<char>{}};
}

void usage(const char* executable) {
    std::cerr << "Usage: " << executable << " --output DIR ASSET.json...\n";
}

std::string hex32(std::uint32_t value) {
    std::ostringstream stream;
    stream << std::hex << std::setw(8) << std::setfill('0') << value;
    return stream.str();
}

std::string hex64(std::uint64_t value) {
    std::ostringstream stream;
    stream << std::hex << std::setw(16) << std::setfill('0') << value;
    return stream.str();
}

std::string floatBits(float value) {
    return hex32(std::bit_cast<std::uint32_t>(value));
}

std::string doubleBits(double value) {
    return hex64(std::bit_cast<std::uint64_t>(value));
}

std::uint64_t stringHash(std::string_view value) {
    avemotion::core::Fnv1a64 hash;
    hash.appendString(value);
    return hash.value();
}

std::string valueRef(avemotion::model::MotionValueRef value) {
    return std::to_string(static_cast<unsigned>(value.type))
        + ":" + std::to_string(value.index);
}

std::string idList(
    const std::vector<avemotion::model::SourceNodeId>& values,
    avemotion::model::IndexRange range) {
    std::ostringstream stream;
    for (std::uint32_t offset = 0; offset < range.count; ++offset) {
        if (offset != 0U) stream << ',';
        stream << values[range.first + offset].value;
    }
    return stream.str();
}

std::string propertyIdList(
    const std::vector<avemotion::model::PropertyId>& values,
    avemotion::model::IndexRange range) {
    std::ostringstream stream;
    for (std::uint32_t offset = 0; offset < range.count; ++offset) {
        if (offset != 0U) stream << ',';
        stream << values[range.first + offset].value;
    }
    return stream.str();
}

void writeDetailsHeader(std::ostream& out) {
    out << "variant\tasset\trecord\tid\tpayload\n";
}

void writeParsedDetails(
    std::ostream& out,
    std::string_view variant,
    std::string_view asset,
    const avemotion::model::MotionAssetModel& model) {
    using namespace avemotion::model;
    const auto row = [&](std::string_view record, std::uint32_t id, const std::string& payload) {
        out << variant << '\t' << asset << '\t' << record << '\t'
            << id << '\t' << payload << '\n';
    };

    row("asset", 0U,
        "schema=" + std::to_string(model.schemaVersion)
        + ";source=" + hex64(model.sourceAssetHash)
        + ";parsed=" + hex64(model.parsedModelFingerprint)
        + ";compositions=" + std::to_string(model.compositions.size())
        + ";nodes=" + std::to_string(model.sourceNodes.size())
        + ";properties=" + std::to_string(model.properties.size())
        + ";tracks=" + std::to_string(model.tracks.size())
        + ";segments=" + std::to_string(model.segments.size()));

    for (const auto& value : model.compositions) {
        row("composition", value.id.value,
            "root=" + std::to_string(value.rootNode.value)
            + ";name=" + hex64(stringHash(value.debugName))
            + ";size=" + std::to_string(value.logicalWidth) + "x"
                + std::to_string(value.logicalHeight)
            + ";first=" + doubleBits(value.firstFrame)
            + ";end=" + doubleBits(value.endFrame)
            + ";fps=" + doubleBits(value.frameRate));
    }
    for (const auto& value : model.sourceNodes) {
        row("node", value.id.value,
            "parent=" + std::to_string(value.parent.value)
            + ";xparent=" + std::to_string(value.transformParent.value)
            + ";comp=" + std::to_string(value.composition.value)
            + ";refcomp=" + std::to_string(value.referencedComposition.value)
            + ";kind=" + std::to_string(static_cast<unsigned>(value.kind))
            + ";layer=" + std::to_string(static_cast<unsigned>(value.layerKind))
            + ";children=" + idList(model.sourceChildIds, value.children)
            + ";properties=" + propertyIdList(model.sourcePropertyIds, value.properties)
            + ";name=" + hex64(value.nameHash)
            + ";hidden=" + std::to_string(value.hidden ? 1 : 0)
            + ";static=" + std::to_string(value.authoredStatic ? 1 : 0)
            + ";auto=" + std::to_string(value.autoOrient ? 1 : 0)
            + ";layerid=" + std::to_string(value.authoredLayerId)
            + ";parentid=" + std::to_string(value.authoredParentLayerId)
            + ";in=" + doubleBits(value.inFrame)
            + ";out=" + doubleBits(value.outFrame)
            + ";start=" + doubleBits(value.startFrame)
            + ";stretch=" + floatBits(value.timeStretch)
            + ";deps=" + std::to_string(value.dependencyBits)
            + ";fill=" + std::to_string(static_cast<unsigned>(value.fillRule))
            + ";cap=" + std::to_string(static_cast<unsigned>(value.strokeCap))
            + ";join=" + std::to_string(static_cast<unsigned>(value.strokeJoin))
            + ";gradient=" + std::to_string(static_cast<unsigned>(value.gradientType))
            + ";mask=" + std::to_string(static_cast<unsigned>(value.maskMode))
            + ";matte=" + std::to_string(static_cast<unsigned>(value.matteMode))
            + ";blend=" + std::to_string(static_cast<unsigned>(value.blendMode))
            + ";direction=" + std::to_string(static_cast<unsigned>(value.pathDirection))
            + ";polystar=" + std::to_string(static_cast<unsigned>(value.polystarType))
            + ";trim=" + std::to_string(static_cast<unsigned>(value.trimMode))
            + ";miter=" + floatBits(value.miterLimit)
            + ";maxcopies=" + floatBits(value.repeaterMaximumCopies)
            + ";colorpoints=" + std::to_string(value.gradientColorPointCount)
            + ";layersize=" + std::to_string(value.layerWidth) + "x"
                + std::to_string(value.layerHeight)
            + ";solid=" + floatBits(value.solidColor.r) + ","
                + floatBits(value.solidColor.g) + ","
                + floatBits(value.solidColor.b) + ","
                + floatBits(value.solidColor.a)
            + ";assetref=" + hex64(value.sourceAssetRefHash)
            + ";enabled=" + std::to_string(value.enabled ? 1 : 0)
            + ";inverted=" + std::to_string(value.maskInverted ? 1 : 0));
    }
    for (const auto& value : model.properties) {
        row("property", value.id.value,
            "owner=" + std::to_string(value.owner.value)
            + ";semantic=" + std::to_string(static_cast<unsigned>(value.semantic))
            + ";component=" + std::to_string(value.semanticIndex)
            + ";type=" + std::to_string(static_cast<unsigned>(value.valueType))
            + ";flags=" + std::to_string(value.flags)
            + ";static=" + valueRef(value.staticValue)
            + ";track=" + std::to_string(value.track.value));
    }
    for (const auto& value : model.tracks) {
        row("track", value.id.value,
            "property=" + std::to_string(value.property.value)
            + ";segments=" + std::to_string(value.segments.first) + ":"
                + std::to_string(value.segments.count)
            + ";first=" + doubleBits(value.firstFrame)
            + ";end=" + doubleBits(value.endFrame));
    }
    for (const auto& value : model.segments) {
        row("segment", value.id.value,
            "track=" + std::to_string(value.track.value)
            + ";first=" + doubleBits(value.firstFrame)
            + ";end=" + doubleBits(value.endFrame)
            + ";temporal=" + std::to_string(static_cast<unsigned>(value.interpolation))
            + ";spatial=" + std::to_string(static_cast<unsigned>(value.spatialInterpolation))
            + ";start=" + valueRef(value.startValue)
            + ";finish=" + valueRef(value.endValue)
            + ";ease=" + floatBits(value.temporalControl1.x) + ","
                + floatBits(value.temporalControl1.y) + ","
                + floatBits(value.temporalControl2.x) + ","
                + floatBits(value.temporalControl2.y)
            + ";tangents=" + floatBits(value.spatialInTangent.x) + ","
                + floatBits(value.spatialInTangent.y) + ","
                + floatBits(value.spatialOutTangent.x) + ","
                + floatBits(value.spatialOutTangent.y));
    }
    for (std::size_t index = 0; index < model.scalarValues.size(); ++index) {
        row("scalar", static_cast<std::uint32_t>(index),
            "bits=" + floatBits(model.scalarValues[index]));
    }
    for (std::size_t index = 0; index < model.vec2Values.size(); ++index) {
        const auto value = model.vec2Values[index];
        row("vec2", static_cast<std::uint32_t>(index),
            "bits=" + floatBits(value.x) + "," + floatBits(value.y));
    }
    for (std::size_t index = 0; index < model.colorValues.size(); ++index) {
        const auto value = model.colorValues[index];
        row("color", static_cast<std::uint32_t>(index),
            "bits=" + floatBits(value.r) + "," + floatBits(value.g) + ","
                + floatBits(value.b) + "," + floatBits(value.a));
    }
    for (std::size_t index = 0; index < model.matrixValues.size(); ++index) {
        const auto value = model.matrixValues[index];
        row("matrix", static_cast<std::uint32_t>(index),
            "bits=" + floatBits(value.m11) + "," + floatBits(value.m12) + ","
                + floatBits(value.m21) + "," + floatBits(value.m22) + ","
                + floatBits(value.dx) + "," + floatBits(value.dy));
    }
    for (std::size_t index = 0; index < model.shapeValues.size(); ++index) {
        const auto& value = model.shapeValues[index];
        avemotion::core::Fnv1a64 hash;
        for (std::uint32_t offset = 0; offset < value.points.count; ++offset) {
            const auto point = model.shapePoints[value.points.first + offset];
            hash.appendFloat(point.x);
            hash.appendFloat(point.y);
        }
        row("shape", static_cast<std::uint32_t>(index),
            "range=" + std::to_string(value.points.first) + ":"
                + std::to_string(value.points.count)
            + ";closed=" + std::to_string(value.closed ? 1 : 0)
            + ";hash=" + hex64(hash.value()));
    }
    for (std::size_t index = 0; index < model.gradientValues.size(); ++index) {
        const auto& value = model.gradientValues[index];
        avemotion::core::Fnv1a64 hash;
        for (std::uint32_t offset = 0; offset < value.values.count; ++offset) {
            hash.appendFloat(model.gradientFloats[value.values.first + offset]);
        }
        row("gradient", static_cast<std::uint32_t>(index),
            "range=" + std::to_string(value.values.first) + ":"
                + std::to_string(value.values.count)
            + ";hash=" + hex64(hash.value()));
    }
}

void writeHeader(std::ostream& out) {
    out << "variant\tasset\tasset_fnv64\tschema\trevision\tmodel"
           "\ttopology\tresources\tparsed_model\tlogical_size\tframerate\ttotal_frames"
           "\tdeclared_layers\tobserved_layers\tdeclared_nodes\tobserved_nodes"
           "\tdeclared_geometries\tobserved_geometries\tstatic_geometries"
           "\tdeclared_paints\tobserved_paints\tstatic_paints\tmasks\tclips"
           "\tdraw_order\tchild_refs\tlayer_node_refs"
           "\tdirect_parsed\tcompositions\tsource_nodes\tproperties"
           "\tstatic_properties\tanimated_properties\ttracks\tsegments"
           "\tscalar_values\tvec2_values\tcolor_values\tmatrix_values"
           "\tshape_values\tgradient_values\tsource_child_refs"
           "\tsource_property_refs\n";
}
} // namespace

int main(int argc, char** argv) {
    fs::path output;
    std::vector<fs::path> assets;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--output" && index + 1 < argc) {
            output = argv[++index];
        } else if (!argument.empty() && argument.front() == '-') {
            usage(argv[0]);
            return EXIT_FAILURE;
        } else {
            assets.emplace_back(argument);
        }
    }
    if (output.empty() || assets.empty()) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    fs::create_directories(output);
    std::ofstream manifest{output / "model_manifest.tsv", std::ios::binary};
    if (!manifest) {
        std::cerr << "Unable to create model manifest\n";
        return EXIT_FAILURE;
    }
    writeHeader(manifest);
    std::ofstream details{output / "parsed_model_details.tsv", std::ios::binary};
    if (!details) {
        std::cerr << "Unable to create parsed-model detail manifest\n";
        return EXIT_FAILURE;
    }
    writeDetailsHeader(details);

    const auto upstream = avemotion::reference::selectedUpstream();
    avemotion::runtime::Runtime runtime;
    for (const auto& assetPath : assets) {
        const auto json = readText(assetPath);
        if (json.empty()) {
            std::cerr << "Unable to read " << assetPath << '\n';
            return EXIT_FAILURE;
        }
        auto loaded = runtime.loadLottieJson(json, assetPath.filename().string());
        if (!loaded) {
            std::cerr << "Unable to load " << assetPath << ": "
                      << loaded.error.message << '\n';
            return EXIT_FAILURE;
        }
        const auto prepared = loaded.asset->prepareModel();
        if (!prepared) {
            std::cerr << "Unable to prepare model for " << assetPath << ": "
                      << prepared.error << '\n';
            return EXIT_FAILURE;
        }
        const auto& model = *prepared.model;
        const auto& stats = model.statistics;
        manifest << upstream.variant << '\t'
                 << assetPath.filename().string() << '\t'
                 << avemotion::core::formatHash(model.sourceAssetHash) << '\t'
                 << model.schemaVersion << '\t'
                 << model.revision << '\t'
                 << avemotion::core::formatHash(model.fingerprint) << '\t'
                 << avemotion::core::formatHash(model.topologyFingerprint) << '\t'
                 << avemotion::core::formatHash(model.resourceFingerprint) << '\t'
                 << avemotion::core::formatHash(model.parsedModelFingerprint) << '\t'
                 << model.logicalWidth << 'x' << model.logicalHeight << '\t'
                 << std::setprecision(17) << model.frameRate << '\t'
                 << model.totalFrames << '\t'
                 << stats.declaredLayerCount << '\t'
                 << stats.observedLayerCount << '\t'
                 << stats.declaredNodeCount << '\t'
                 << stats.observedNodeCount << '\t'
                 << stats.declaredGeometryCount << '\t'
                 << stats.observedGeometryCount << '\t'
                 << stats.assetStaticGeometryCount << '\t'
                 << stats.declaredPaintCount << '\t'
                 << stats.observedPaintCount << '\t'
                 << stats.assetStaticPaintCount << '\t'
                 << stats.maskCount << '\t'
                 << stats.clipCount << '\t'
                 << model.drawOrder.size() << '\t'
                 << model.childLayerIds.size() << '\t'
                 << model.layerNodeIds.size() << '\t'
                 << (stats.directParsedModel ? 1 : 0) << '\t'
                 << stats.compositionCount << '\t'
                 << stats.sourceNodeCount << '\t'
                 << stats.propertyCount << '\t'
                 << stats.staticPropertyCount << '\t'
                 << stats.animatedPropertyCount << '\t'
                 << stats.trackCount << '\t'
                 << stats.segmentCount << '\t'
                 << stats.scalarValueCount << '\t'
                 << stats.vec2ValueCount << '\t'
                 << stats.colorValueCount << '\t'
                 << stats.matrixValueCount << '\t'
                 << stats.shapeValueCount << '\t'
                 << stats.gradientValueCount << '\t'
                 << model.sourceChildIds.size() << '\t'
                 << model.sourcePropertyIds.size() << '\n';
        writeParsedDetails(
            details,
            upstream.variant,
            assetPath.filename().string(),
            model);
    }

    std::cout << "Recorded " << assets.size()
              << " immutable asset models using " << upstream.variant
              << " rlottie\n";
    return EXIT_SUCCESS;
}
