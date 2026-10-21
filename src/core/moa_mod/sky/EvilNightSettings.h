/******************************************************************************
 * Copyright (c) 2026 Masters of Albion Reforged developers
 *****************************************************************************/

#pragma once

namespace moa_mod::sky
{

/// The settings of the evil night: the blood moon has a switch and a strength (0 to 2, 1 is normal). The blood moon
/// shows only for an evil player, and only at night.
struct EvilNightSettings
{
	bool bloodMoon = true;
	double bloodMoonStrength = 1.0;

	friend bool operator==(const EvilNightSettings&, const EvilNightSettings&) = default;
};

} // namespace moa_mod::sky
