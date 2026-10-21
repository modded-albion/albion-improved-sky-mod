/******************************************************************************
 * Copyright (c) 2026 Masters of Albion Reforged developers
 *****************************************************************************/

#pragma once

#include <optional>
#include <string_view>

namespace moa_mod::sky
{

/// The alignment of the player as the game keeps it, and the two limits of the neutral band of the game. The game
/// shows the alignment from -1 (evil) to +1 (good) on its dial. Below the evil limit the player is evil, above the
/// good limit the player is good.
struct AlignmentReading
{
	double value = 0.0;
	double evilLimit = 0.0;
	double goodLimit = 0.0;
};

/// Return the theme of the sky for an alignment: 0 in the neutral band, then a straight line to -1 at the evil end of
/// the dial and to +1 at the good end. The theme has no step at the limits of the band.
[[nodiscard]] double AlignmentTheme(const AlignmentReading& reading);

/// Return the theme that the sky goes to: -1 evil, 0 neutral, +1 good. With `useGame`, the theme follows the
/// alignment of the player; with no alignment (for example on the title screen), the theme is neutral. Else the
/// theme is `manual` (the slider of the menu bar), which never changes the alignment in the game.
[[nodiscard]] double ThemeTarget(bool useGame, double manual, const std::optional<AlignmentReading>& reading);

/// The theme of the sky moves to its target smoothly, so that a change of the alignment does not make a jump.
struct ThemeEase
{
	/// The time in seconds for the theme to go about two thirds of the way to its target.
	static constexpr double k_TimeConstant = 2.0;

	std::optional<double> current;

	/// Move the theme toward `target` for a time of `seconds`, and return the new theme. The first call takes the
	/// target at once: the sky has no fade when the town loads.
	double Step(double target, double seconds);
};

/// Return the light of the aurora for the theme of the sky, as a part of the first light of the aurora of the mod.
/// Only a good player sees the aurora: no aurora below 10 % of the way from neutral to fully good, then 10 % of the
/// light, which grows in a straight line to 60 % of the light for a fully good player.
[[nodiscard]] double AuroraLightForTheme(double theme);

/// Return the scale of the size of the stars for a depth of field level of the game. The depth of field blurs the
/// far sky; larger stars keep more light after the blur. Level 0 (no depth of field) gives 1.
[[nodiscard]] double StarSizeForDepthOfField(int level);

/// Return the scale of the light of the stars for a depth of field level of the game. The blur spreads the light of a
/// star over many pixels; more light keeps the star visible. Level 0 gives 1.
[[nodiscard]] double StarLightForDepthOfField(int level);

} // namespace moa_mod::sky
