#pragma once

#include "NativeEllipseInput.hpp"
#include "avemotion/model/AssetModel.hpp"

#include <optional>

namespace avemotion::runtime::detail {

enum class NativeEllipseBindingCode {
    Bound, UnsupportedNumericConversion, InvalidModelTable,
    CompositionMismatch, TopologyMismatch, SourceIdentityMismatch,
    PropertyShapeMismatch, ValueMismatch, TrackMismatch
};

struct NativeEllipseModelBinding final {
    model::SourceNodeId root, layer, group, ellipse, fill;
    model::PropertyId layerTransform, layerOpacity, groupTransform, groupOpacity;
    model::PropertyId position, size, color, fillOpacity;
};

struct NativeEllipseBindingResult final {
    NativeEllipseBindingCode code = NativeEllipseBindingCode::InvalidModelTable;
    std::optional<NativeEllipseModelBinding> binding;
    [[nodiscard]] explicit operator bool() const noexcept {
        return code == NativeEllipseBindingCode::Bound && binding.has_value();
    }
};

[[nodiscard]] NativeEllipseBindingResult bindNativeEllipseModel(
    const NativeEllipseInput&, const model::MotionAssetModel&);

} // namespace avemotion::runtime::detail
