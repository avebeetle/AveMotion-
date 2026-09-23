#pragma once

#include <string>
#include <string_view>

namespace avemotion::runtime::detail {

enum class NativeEllipseAdmissionCode {
    Accepted, InvalidJson, ResourceLimit, UnsupportedField,
    InvalidType, UnsupportedValue, UnsupportedStructure
};

struct NativeEllipseAdmission final {
    NativeEllipseAdmissionCode code = NativeEllipseAdmissionCode::InvalidJson;
    std::string path;

    [[nodiscard]] bool accepted() const noexcept {
        return code == NativeEllipseAdmissionCode::Accepted;
    }
};

[[nodiscard]] NativeEllipseAdmission auditNativeEllipseInput(std::string_view json);

} // namespace avemotion::runtime::detail
