// File: dsp/include/plugin_dsp/DrumTypes.h
// The nine drum types of docs/01-concept.md and small helpers shared by the dsp/ core.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace plugin_dsp
{
enum class DrumType : std::uint8_t { kick, snare, closedHat, openHat, crash, ride, rimshot, clap, misc };

constexpr std::size_t numDrumTypes = 9;

constexpr std::size_t index (DrumType t) noexcept { return static_cast<std::size_t> (t); }

constexpr std::uint32_t bit (DrumType t) noexcept { return 1u << static_cast<std::uint32_t> (t); }

constexpr std::uint32_t allDrumTypes = (1u << numDrumTypes) - 1u;

inline const char* drumTypeName (DrumType t) noexcept
{
    constexpr std::array<const char*, numDrumTypes> names { "kick", "snare", "closed hat", "open hat", "crash",
                                                            "ride", "rimshot", "clap", "misc" };
    return names[index (t)];
}
} // namespace plugin_dsp
