/******************************************************************************
 * Copyright (c) 2026 Masters of Albion Reforged developers
 *****************************************************************************/

#pragma once

#include <array>
#include <optional>
#include <string_view>

namespace moa_mod::sky
{

/// When the sky mod shows the aurora.
enum class AuroraMode
{
	Off,       ///< Never.
	Alignment, ///< Only for a good player: the aurora grows with the alignment.
	Always,    ///< Always, at the first light of the mod. For tests.
};

inline constexpr std::array k_AuroraModes {AuroraMode::Off, AuroraMode::Alignment, AuroraMode::Always};

/// The name of the mode in the settings file: "off", "alignment" or "always".
[[nodiscard]] constexpr const char* AuroraModeName(AuroraMode mode)
{
	switch (mode)
	{
	case AuroraMode::Off:
		return "off";
	case AuroraMode::Alignment:
		return "alignment";
	case AuroraMode::Always:
		return "always";
	}
	return "off";
}

/// Return the mode with the name `name`, or nothing if the name is not known. The old name "real" gives the mode
/// "alignment".
[[nodiscard]] constexpr std::optional<AuroraMode> ParseAuroraMode(std::string_view name)
{
	if (name == "real")
	{
		return AuroraMode::Alignment;
	}
	for (const auto mode : k_AuroraModes)
	{
		if (name == AuroraModeName(mode))
		{
			return mode;
		}
	}
	return std::nullopt;
}

} // namespace moa_mod::sky
