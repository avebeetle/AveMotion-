#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

namespace avemotion::model {

inline constexpr std::uint32_t kInvalidModelId =
    std::numeric_limits<std::uint32_t>::max();

#define AVEMOTION_DEFINE_MODEL_ID(Name)                                      \
    struct Name final {                                                      \
        std::uint32_t value = kInvalidModelId;                               \
        [[nodiscard]] constexpr bool valid() const noexcept {                \
            return value != kInvalidModelId;                                 \
        }                                                                    \
        [[nodiscard]] constexpr std::uint32_t index() const noexcept {        \
            return value;                                                    \
        }                                                                    \
        [[nodiscard]] friend constexpr bool operator==(                      \
            const Name&, const Name&) noexcept = default;                    \
    }

AVEMOTION_DEFINE_MODEL_ID(CompositionId);
AVEMOTION_DEFINE_MODEL_ID(LayerId);
AVEMOTION_DEFINE_MODEL_ID(NodeId);
AVEMOTION_DEFINE_MODEL_ID(SourceNodeId);
AVEMOTION_DEFINE_MODEL_ID(GeometryId);
AVEMOTION_DEFINE_MODEL_ID(PaintId);
AVEMOTION_DEFINE_MODEL_ID(DrawItemId);
AVEMOTION_DEFINE_MODEL_ID(ClipId);
AVEMOTION_DEFINE_MODEL_ID(PropertyId);
AVEMOTION_DEFINE_MODEL_ID(TrackId);
AVEMOTION_DEFINE_MODEL_ID(SegmentId);

#undef AVEMOTION_DEFINE_MODEL_ID

struct IndexRange final {
    std::uint32_t first = 0;
    std::uint32_t count = 0;

    [[nodiscard]] constexpr bool empty() const noexcept { return count == 0U; }
    [[nodiscard]] constexpr std::uint32_t end() const noexcept {
        return first + count;
    }
};

template <typename Id>
[[nodiscard]] constexpr Id makeId(std::size_t index) noexcept {
    return index < static_cast<std::size_t>(kInvalidModelId)
        ? Id{static_cast<std::uint32_t>(index)}
        : Id{};
}

} // namespace avemotion::model
