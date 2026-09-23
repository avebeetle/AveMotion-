#pragma once

#include "avemotion/model/AssetModel.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace avemotion::validation {

enum class AssetSourceFormat : std::uint8_t {
    Unknown,
    LottieJson,
    TelegramTgs,
};

enum class ValidationProfile : std::uint8_t {
    ApplicationAsset,
    TelegramSticker,
    Direct2DNative,
};

enum class ValidationSeverity : std::uint8_t {
    Info,
    Warning,
    Error,
    Fatal,
};

enum class FeatureSupport : std::uint8_t {
    Native,
    NativeSubset,
    ReferenceFallback,
    Unsupported,
};

enum class FeatureKind : std::uint16_t {
    Unknown,
    Composition,
    Precomposition,
    ShapeLayer,
    SolidLayer,
    ImageLayer,
    TextLayer,
    NullLayer,
    ShapePath,
    Rectangle,
    Ellipse,
    Polygon,
    Star,
    SolidFill,
    SolidStroke,
    DashedStroke,
    GradientFill,
    GradientStroke,
    TrimPath,
    Repeater,
    Mask,
    Matte,
    BlendMode,
    AutoOrient,
    SpatialPosition,
    TimeStretch,
    TimeRemap,
    UnknownNode,
};

enum class ValidationIssueCode : std::uint16_t {
    None,
    SchemaVersionUnsupported,
    ParsedModelUnavailable,
    InvalidDimensions,
    InvalidFrameRate,
    InvalidDuration,
    InvalidTableId,
    InvalidReference,
    InvalidRange,
    HierarchyCycle,
    CompositionCycle,
    NonFiniteValue,
    InvalidPropertyFlags,
    InvalidPropertyValue,
    InvalidTrack,
    InvalidSegment,
    UnsortedSegments,
    ShapeTopologyInvalid,
    LimitExceeded,
    UnknownFeature,
    NativeSubsetRequired,
    ReferenceFallbackRequired,
    UnsupportedFeature,
    TelegramSourceMustBeTgs,
    TelegramCompressedSizeExceeded,
    TelegramCanvasMismatch,
    TelegramFrameRateOutOfRange,
    TelegramFrameRateNotPreferred,
    TelegramDurationExceeded,
    TelegramLoopUnknown,
    TelegramLoopRequired,
    TelegramForbiddenFeature,
    TelegramBoundsNotProven,
};

struct AssetValidationContext final {
    AssetSourceFormat sourceFormat = AssetSourceFormat::Unknown;
    std::size_t encodedBytes = 0;
    std::size_t jsonBytes = 0;
    bool loopIntentKnown = false;
    bool intendedLoop = true;
};

struct AssetValidationLimits final {
    std::size_t maximumCompositions = 4096;
    std::size_t maximumSourceNodes = 100000;
    std::size_t maximumProperties = 200000;
    std::size_t maximumTracks = 100000;
    std::size_t maximumSegments = 1000000;
    std::size_t maximumShapePoints = 5000000;
    std::size_t maximumGradientValues = 1000000;
    std::size_t maximumHierarchyDepth = 256;
    std::size_t maximumPropertiesPerNode = 4096;
    std::size_t maximumSegmentsPerTrack = 65536;
    float maximumRepeaterCopies = 4096.0F;

    [[nodiscard]] static AssetValidationLimits applicationAsset() noexcept;
    [[nodiscard]] static AssetValidationLimits telegramSticker() noexcept;
};

struct AssetValidationOptions final {
    ValidationProfile profile = ValidationProfile::ApplicationAsset;
    AssetValidationLimits limits = AssetValidationLimits::applicationAsset();
    bool treatWarningsAsErrors = false;
    bool rejectUnknownFeatures = true;

    [[nodiscard]] static AssetValidationOptions applicationAsset() noexcept;
    [[nodiscard]] static AssetValidationOptions telegramSticker() noexcept;
    [[nodiscard]] static AssetValidationOptions direct2DNative() noexcept;
};

struct FeatureUsage final {
    FeatureKind feature = FeatureKind::Unknown;
    FeatureSupport support = FeatureSupport::Unsupported;
    std::size_t occurrences = 0;
};

struct ValidationIssue final {
    ValidationSeverity severity = ValidationSeverity::Info;
    ValidationIssueCode code = ValidationIssueCode::None;
    FeatureKind feature = FeatureKind::Unknown;
    model::SourceNodeId node;
    model::PropertyId property;
    std::string message;
};

struct AssetComplexity final {
    std::size_t hierarchyDepth = 0;
    std::size_t maximumPropertiesOnNode = 0;
    std::size_t maximumSegmentsOnTrack = 0;
    std::size_t shapePointCount = 0;
    std::size_t gradientValueCount = 0;
    std::size_t animatedPropertyCount = 0;
    std::size_t estimatedWorkUnits = 0;
};

struct AssetValidationReport final {
    ValidationProfile profile = ValidationProfile::ApplicationAsset;
    bool structurallyValid = false;
    bool profileCompliant = false;
    bool nativeDirect2DReady = false;
    bool requiresReferenceFallback = false;
    bool containsUnsupportedFeatures = false;

    std::size_t infoCount = 0;
    std::size_t warningCount = 0;
    std::size_t errorCount = 0;
    std::size_t fatalCount = 0;

    std::size_t nativeFeatureOccurrences = 0;
    std::size_t nativeSubsetFeatureOccurrences = 0;
    std::size_t fallbackFeatureOccurrences = 0;
    std::size_t unsupportedFeatureOccurrences = 0;

    AssetComplexity complexity;
    std::vector<FeatureUsage> features;
    std::vector<ValidationIssue> issues;
    std::uint64_t fingerprint = 0;

    [[nodiscard]] bool accepted() const noexcept {
        return structurallyValid && profileCompliant;
    }

    [[nodiscard]] bool hasErrors() const noexcept {
        return errorCount != 0U || fatalCount != 0U;
    }
};

class AssetValidator final {
public:
    [[nodiscard]] AssetValidationReport validate(
        const model::MotionAssetModel& asset,
        const AssetValidationContext& context = {},
        const AssetValidationOptions& options =
            AssetValidationOptions::applicationAsset()) const;
};

[[nodiscard]] std::string_view toString(AssetSourceFormat value) noexcept;
[[nodiscard]] std::string_view toString(ValidationProfile value) noexcept;
[[nodiscard]] std::string_view toString(ValidationSeverity value) noexcept;
[[nodiscard]] std::string_view toString(FeatureSupport value) noexcept;
[[nodiscard]] std::string_view toString(FeatureKind value) noexcept;
[[nodiscard]] std::string_view toString(ValidationIssueCode value) noexcept;

} // namespace avemotion::validation
