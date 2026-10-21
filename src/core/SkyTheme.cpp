/******************************************************************************
 * Copyright (c) 2026 Masters of Albion Reforged developers
 *****************************************************************************/

#include <cmath>

#include <algorithm>

#include <core/SkyTheme.h>

namespace moa_mod::sky
{

namespace
{
/// The ends of the dial of the game.
constexpr double k_EvilEnd = -1.0;
constexpr double k_GoodEnd = 1.0;
/// The smallest part of the dial between a limit and its end. It stops a division by zero for odd limits.
constexpr double k_SmallestSpan = 1e-3;
/// The theme stops at its target when it is nearer than this.
constexpr double k_Arrive = 1e-4;
/// The star size grows by this part for each depth of field level, to the largest scale.
constexpr double k_StarSizePerLevel = 0.5;
constexpr double k_LargestStarSize = 2.5;
/// The star light grows by this part for each depth of field level, to the largest scale.
constexpr double k_StarLightPerLevel = 0.6;
constexpr double k_LargestStarLight = 3.0;
/// The aurora: the least good theme that shows it, its light there, and its light for a fully good player.
constexpr double k_AuroraFirstTheme = 0.1;
constexpr double k_AuroraFirstLight = 0.1;
constexpr double k_AuroraFullLight = 0.6;
} // namespace

double AlignmentTheme(const AlignmentReading& reading)
{
	const auto value = reading.value;
	if (!std::isfinite(value) || !std::isfinite(reading.evilLimit) || !std::isfinite(reading.goodLimit))
	{
		return 0.0;
	}
	// Odd limits (good below evil) give a band with no width at the centre of the two.
	const auto evil = std::min(reading.evilLimit, reading.goodLimit);
	const auto good = std::max(reading.evilLimit, reading.goodLimit);
	if (value > good)
	{
		return std::clamp((value - good) / std::max(k_GoodEnd - good, k_SmallestSpan), 0.0, 1.0);
	}
	if (value < evil)
	{
		return -std::clamp((evil - value) / std::max(evil - k_EvilEnd, k_SmallestSpan), 0.0, 1.0);
	}
	return 0.0;
}

double ThemeTarget(bool useGame, double manual, const std::optional<AlignmentReading>& reading)
{
	if (useGame)
	{
		return reading ? AlignmentTheme(*reading) : 0.0;
	}
	return std::isfinite(manual) ? std::clamp(manual, -1.0, 1.0) : 0.0;
}

double ThemeEase::Step(double target, double seconds)
{
	if (!current || !std::isfinite(*current))
	{
		current = target;
		return target;
	}
	const auto time = std::isfinite(seconds) ? std::max(seconds, 0.0) : 0.0;
	const auto next = *current + (target - *current) * (1.0 - std::exp(-time / k_TimeConstant));
	current = std::abs(target - next) < k_Arrive ? target : next;
	return *current;
}

double AuroraLightForTheme(double theme)
{
	if (!std::isfinite(theme) || theme < k_AuroraFirstTheme)
	{
		return 0.0;
	}
	const auto part = (std::min(theme, 1.0) - k_AuroraFirstTheme) / (1.0 - k_AuroraFirstTheme);
	return k_AuroraFirstLight + (k_AuroraFullLight - k_AuroraFirstLight) * part;
}

double StarLightForDepthOfField(int level)
{
	return std::min(1.0 + k_StarLightPerLevel * std::max(level, 0), k_LargestStarLight);
}

double StarSizeForDepthOfField(int level)
{
	return std::min(1.0 + k_StarSizePerLevel * std::max(level, 0), k_LargestStarSize);
}

} // namespace moa_mod::sky
