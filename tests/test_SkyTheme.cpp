/******************************************************************************
 * Copyright (c) 2026 Masters of Albion Reforged developers
 *****************************************************************************/

#include <cmath>

#include <limits>

#include <core/AuroraMode.h>
#include <core/SkyTheme.h>
#include <core/SkyboxSettings.h>
#include <gtest/gtest.h>

using namespace moa_mod;
using namespace moa_mod::sky;

namespace
{
/// Limits of the neutral band for the tests. The game reads its limits from NGManager.
constexpr double k_Evil = -0.3;
constexpr double k_Good = 0.3;

double Theme(double value)
{
	return AlignmentTheme({.value = value, .evilLimit = k_Evil, .goodLimit = k_Good});
}
} // namespace

TEST(SkyTheme, IsNeutralInTheBandOfTheGame)
{
	EXPECT_EQ(Theme(0.0), 0.0);
	EXPECT_EQ(Theme(k_Good), 0.0);
	EXPECT_EQ(Theme(k_Evil), 0.0);
	EXPECT_EQ(Theme(0.29), 0.0);
}

TEST(SkyTheme, GoesToTheEndsOfTheDialWithNoStep)
{
	EXPECT_NEAR(Theme(1.0), 1.0, 1e-12);
	EXPECT_NEAR(Theme(-1.0), -1.0, 1e-12);
	EXPECT_NEAR(Theme(0.65), 0.5, 1e-12);
	EXPECT_NEAR(Theme(-0.65), -0.5, 1e-12);
	// Just outside the band, the theme is near zero: no jump.
	EXPECT_LT(Theme(k_Good + 1e-4), 1e-3);
	EXPECT_GT(Theme(k_Evil - 1e-4), -1e-3);
	// The game does not stop the value at the ends of the dial; the theme does.
	EXPECT_EQ(Theme(7.0), 1.0);
	EXPECT_EQ(Theme(-12.0), -1.0);
}

TEST(SkyTheme, StaysSafeWithOddValues)
{
	EXPECT_EQ(AlignmentTheme({.value = std::numeric_limits<double>::quiet_NaN(), .evilLimit = k_Evil, .goodLimit = k_Good}),
	          0.0);
	// A limit at the end of the dial does not divide by zero.
	EXPECT_EQ(AlignmentTheme({.value = 2.0, .evilLimit = -1.0, .goodLimit = 1.0}), 1.0);
	// Limits in the wrong order give a band with no width.
	EXPECT_GT(AlignmentTheme({.value = 0.5, .evilLimit = 0.2, .goodLimit = -0.2}), 0.0);
}

TEST(SkyTheme, TheSliderReplacesTheAlignmentOfTheGame)
{
	const AlignmentReading evil {.value = -1.0, .evilLimit = k_Evil, .goodLimit = k_Good};
	EXPECT_EQ(ThemeTarget(true, 0.7, evil), -1.0);
	EXPECT_EQ(ThemeTarget(true, 0.7, std::nullopt), 0.0);
	EXPECT_EQ(ThemeTarget(false, 0.7, evil), 0.7);
	EXPECT_EQ(ThemeTarget(false, 0.0, evil), 0.0);
	EXPECT_EQ(ThemeTarget(false, -3.0, std::nullopt), -1.0);
	EXPECT_EQ(ThemeTarget(false, std::numeric_limits<double>::quiet_NaN(), evil), 0.0);
}

TEST(SkyTheme, MovesSmoothlyToANewTarget)
{
	ThemeEase ease;
	// The first value comes at once.
	EXPECT_EQ(ease.Step(-1.0, 0.0), -1.0);
	// One time constant goes about two thirds of the way.
	const auto after = ease.Step(1.0, ThemeEase::k_TimeConstant);
	EXPECT_NEAR(after, -1.0 + 2.0 * (1.0 - std::exp(-1.0)), 1e-9);
	// The theme never jumps past the target, and it arrives.
	double theme = after;
	for (int frame = 0; frame < 2000; ++frame)
	{
		const auto next = ease.Step(1.0, 1.0 / 60.0);
		EXPECT_GE(next, theme);
		EXPECT_LE(next, 1.0);
		theme = next;
	}
	EXPECT_EQ(theme, 1.0);
	// A bad time does not move the theme.
	EXPECT_EQ(ease.Step(-1.0, std::numeric_limits<double>::quiet_NaN()), 1.0);
	EXPECT_EQ(ease.Step(-1.0, -5.0), 1.0);
}

TEST(SkyTheme, MakesTheStarsLargerForEachDepthOfFieldLevel)
{
	EXPECT_EQ(StarSizeForDepthOfField(0), 1.0);
	EXPECT_EQ(StarSizeForDepthOfField(-2), 1.0);
	EXPECT_GT(StarSizeForDepthOfField(3), StarSizeForDepthOfField(1));
	EXPECT_LE(StarSizeForDepthOfField(50), 2.5);
	EXPECT_EQ(StarLightForDepthOfField(0), 1.0);
	EXPECT_GT(StarLightForDepthOfField(3), 2.0);
	EXPECT_LE(StarLightForDepthOfField(50), 3.0);
}

TEST(SkyTheme, ShowsTheAuroraOnlyForAGoodPlayer)
{
	EXPECT_EQ(AuroraLightForTheme(-1.0), 0.0);
	EXPECT_EQ(AuroraLightForTheme(0.0), 0.0);
	EXPECT_EQ(AuroraLightForTheme(0.099), 0.0);
	EXPECT_NEAR(AuroraLightForTheme(0.1), 0.1, 1e-12);
	EXPECT_NEAR(AuroraLightForTheme(0.55), 0.35, 1e-12);
	EXPECT_NEAR(AuroraLightForTheme(1.0), 0.6, 1e-12);
	EXPECT_NEAR(AuroraLightForTheme(4.0), 0.6, 1e-12);
	EXPECT_EQ(AuroraLightForTheme(std::numeric_limits<double>::quiet_NaN()), 0.0);
	// The light grows with the alignment.
	double last = 0.0;
	for (int step = 10; step <= 100; ++step)
	{
		const auto light = AuroraLightForTheme(step / 100.0);
		EXPECT_GE(light, last);
		last = light;
	}
}

TEST(SkyTheme, TheSettingsReadBackAndWorkAtOnce)
{
	const auto read = ParseSkyboxSettings(R"({"sky": {"alignmentTheme": false, "useGameAlignment": false, "alignment": -0.4,
	                                       "goodStrength": 0.5,
	                                       "evilStrength": 2, "aurora": "real"}})");
	ASSERT_TRUE(read) << read.GetError().message;
	EXPECT_FALSE(read->sky.alignmentTheme);
	EXPECT_FALSE(read->sky.useGameAlignment);
	EXPECT_EQ(read->sky.alignment, -0.4);
	EXPECT_EQ(read->sky.goodStrength, 0.5);
	EXPECT_EQ(read->sky.evilStrength, 2.0);
	// The old aurora mode "real" reads as the mode "alignment".
	EXPECT_EQ(read->sky.aurora, AuroraMode::Alignment);
	EXPECT_EQ(*ParseSkyboxSettings(SkyboxSettingsJson(*read).dump()), *read);
	const auto change = CompareSkyboxSettings(SkyboxSettings {}, *read);
	EXPECT_TRUE(change.sky);
	EXPECT_TRUE(change.restartOnly.empty());

	// The defaults: the theme on, the alignment of the player, the aurora of the alignment.
	const SkyboxSettings defaults;
	EXPECT_TRUE(defaults.sky.alignmentTheme);
	EXPECT_TRUE(defaults.sky.useGameAlignment);
	EXPECT_EQ(defaults.sky.aurora, AuroraMode::Alignment);

	// An old file with the Kp of the aurora mode "real" still reads.
	EXPECT_TRUE(ParseSkyboxSettings(R"({"sky": {"aurora": "real", "auroraKp": 3}})"));
	EXPECT_FALSE(ParseSkyboxSettings(R"({"sky": {"alignment": 1.5}})"));
	EXPECT_FALSE(ParseSkyboxSettings(R"({"sky": {"useGameAlignment": 1}})"));
	EXPECT_FALSE(ParseSkyboxSettings(R"({"sky": {"goodStrength": 3}})"));
	EXPECT_FALSE(ParseSkyboxSettings(R"({"sky": {"evilStrength": -0.1}})"));
}
