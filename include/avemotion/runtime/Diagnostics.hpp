#pragma once

#include <cstdint>

namespace avemotion::runtime {

struct DiagnosticsSnapshot final {
    std::uint64_t assetLoadAttempts = 0;
    std::uint64_t assetLoadsSucceeded = 0;
    std::uint64_t assetLoadsFailed = 0;
    std::uint64_t tgsDecodeAttempts = 0;
    std::uint64_t tgsDecodesSucceeded = 0;
    std::uint64_t tgsDecodesFailed = 0;
    std::uint64_t tgsCompressedBytes = 0;
    std::uint64_t tgsJsonBytes = 0;
    std::uint64_t instancesCreated = 0;
    std::uint64_t referenceMetadataSessionsCreated = 0;
    std::uint64_t referenceSceneSessionsCreated = 0;
    std::uint64_t referenceModelSessionsCreated = 0;
    std::uint64_t referenceCpuSessionsCreated = 0;
    std::uint64_t referenceSceneSamples = 0;
    std::uint64_t referenceModelSamples = 0;
    std::uint64_t sceneEvaluations = 0;
    std::uint64_t sceneEvaluationFailures = 0;
    std::uint64_t cpuFramesRendered = 0;
    std::uint64_t evaluatedLayers = 0;
    std::uint64_t evaluatedDrawItems = 0;
    std::uint64_t evaluatedMasks = 0;
    std::uint64_t copiedPathPoints = 0;
    std::uint64_t canonicalGeometriesCreated = 0;
    std::uint64_t canonicalPaintsCreated = 0;
    std::uint64_t canonicalResourceConflicts = 0;
    std::uint64_t sceneEvaluationNanoseconds = 0;
    std::uint64_t cpuRenderNanoseconds = 0;
    std::uint64_t assetModelBuildAttempts = 0;
    std::uint64_t assetModelBuildsSucceeded = 0;
    std::uint64_t assetModelBuildsFailed = 0;
    std::uint64_t assetModelLayers = 0;
    std::uint64_t assetModelNodes = 0;
    std::uint64_t assetModelStaticGeometries = 0;
    std::uint64_t assetModelStaticPaints = 0;
    std::uint64_t modelEvaluations = 0;
    std::uint64_t playbackEvaluations = 0;
};

} // namespace avemotion::runtime
