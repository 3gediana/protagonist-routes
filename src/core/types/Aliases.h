#pragma once
//
// core/types/Aliases.h
//
// Strongly-typed entity ids and basic numeric aliases. Using strong typing
// catches accidental id mixing at compile time (e.g. passing an AgentId
// where a SpeciesId is expected).
//
// Style: aggressive use of plain enum-class-backed structs; lightweight,
// trivially-copyable, value-semantic.

#include <cstdint>
#include <functional>

namespace neuro {

using Tick       = std::uint64_t;   // simulation time in discrete steps
using Generation = std::uint32_t;   // evolution generation index

// ---------------------------------------------------------------------------
// Strongly-typed ids. Inspired by https://foonathan.net/2016/10/strong-typedefs/
// Lightweight: no inheritance, no virtuals, all inlined.
// ---------------------------------------------------------------------------
namespace detail {
template <typename Tag>
struct StrongId {
    std::uint64_t value{0};

    constexpr StrongId() = default;
    constexpr explicit StrongId(std::uint64_t v) : value(v) {}

    constexpr bool operator==(const StrongId& other) const noexcept {
        return value == other.value;
    }
    constexpr bool operator!=(const StrongId& other) const noexcept {
        return value != other.value;
    }
    constexpr bool operator<(const StrongId& other) const noexcept {
        return value < other.value;
    }
    constexpr explicit operator bool() const noexcept { return value != 0; }
};
}  // namespace detail

struct AgentTag      {};
struct GenomeTag     {};
struct SpeciesTag    {};
struct InnovationTag {};
struct WorldTag      {};

using AgentId      = detail::StrongId<AgentTag>;
using GenomeId     = detail::StrongId<GenomeTag>;
using SpeciesId    = detail::StrongId<SpeciesTag>;
using InnovationId = detail::StrongId<InnovationTag>;
using WorldId      = detail::StrongId<WorldTag>;

// Sentinel value used to mean "no entity" / "invalid".
inline constexpr AgentId      kInvalidAgentId{0};
inline constexpr GenomeId     kInvalidGenomeId{0};
inline constexpr SpeciesId    kInvalidSpeciesId{0};
inline constexpr InnovationId kInvalidInnovationId{0};

}  // namespace neuro

// std::hash specializations for use in unordered_map etc.
namespace std {
template <typename Tag>
struct hash<neuro::detail::StrongId<Tag>> {
    std::size_t operator()(const neuro::detail::StrongId<Tag>& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};
}  // namespace std
