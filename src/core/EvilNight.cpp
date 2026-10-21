/******************************************************************************
 * Copyright (c) 2026 Masters of Albion Reforged developers
 *****************************************************************************/

#include <cmath>

#include <algorithm>

#include <core/EvilNight.h>

namespace moa_mod::sky
{

namespace
{
/// Below this evil value or night weight, the blood moon does not show.
constexpr double k_LeastEvil = 1e-3;
constexpr double k_LeastNight = 1e-3;

/// The blood moon: the largest change of the size of the disc, and the dark rim.
constexpr double k_BloodMoonGrowth = 0.3;
constexpr double k_BloodMoonRim = 0.6;

[[nodiscard]] double Smooth(double edge0, double edge1, double x)
{
	const auto t = std::clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
	return t * t * (3.0 - 2.0 * t);
}

[[nodiscard]] double Clamp01(double x)
{
	return std::isfinite(x) ? std::clamp(x, 0.0, 1.0) : 0.0;
}

[[nodiscard]] double Strength(bool on, double strength)
{
	return on && std::isfinite(strength) ? std::clamp(strength, 0.0, 2.0) : 0.0;
}
} // namespace

double EvilAmount(double theme)
{
	return Clamp01(-theme);
}

double EvilNightWeight(double nightBlend)
{
	return Clamp01((nightBlend - 0.5) / 0.4);
}

Vector4 BloodMoonUniform(double theme, double nightBlend, const EvilNightSettings& settings)
{
	const auto evil = EvilAmount(theme);
	const auto night = EvilNightWeight(nightBlend);
	if (evil < k_LeastEvil || night < k_LeastNight)
	{
		return {};
	}
	// The blood moon grows smoothly with the evil value.
	const auto weight = Smooth(0.0, 1.0, evil) * night;
	const auto blood = std::min(Strength(settings.bloodMoon, settings.bloodMoonStrength) * weight, 1.0);
	if (blood <= 0.0)
	{
		return {};
	}
	return {static_cast<float>(blood), static_cast<float>(1.0 + k_BloodMoonGrowth * blood),
	        static_cast<float>(k_BloodMoonRim * blood), 0.0f};
}

} // namespace moa_mod::sky
