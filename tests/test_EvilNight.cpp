/******************************************************************************
 * Copyright (c) 2026 Masters of Albion Reforged developers
 *****************************************************************************/

#include <cmath>

#include <core/EvilNight.h>
#include <core/SkyboxSettings.h>
#include <gtest/gtest.h>

using namespace moa_mod;
using namespace moa_mod::sky;

TEST(EvilNight, TheEvilValueAndTheNightWeight)
{
	EXPECT_EQ(EvilAmount(0.0), 0.0);
	EXPECT_EQ(EvilAmount(0.7), 0.0);
	EXPECT_DOUBLE_EQ(EvilAmount(-0.5), 0.5);
	EXPECT_EQ(EvilAmount(-3.0), 1.0);
	EXPECT_EQ(EvilAmount(std::nan("")), 0.0);
	EXPECT_EQ(EvilNightWeight(0.0), 0.0);
	EXPECT_EQ(EvilNightWeight(0.5), 0.0);
	EXPECT_DOUBLE_EQ(EvilNightWeight(0.7), 0.5);
	EXPECT_EQ(EvilNightWeight(0.9), 1.0);
	EXPECT_EQ(EvilNightWeight(1.0), 1.0);
}

TEST(EvilNight, TheBloodMoonDoesNotShowInTheDayOrForANeutralOrGoodPlayer)
{
	const EvilNightSettings settings;
	EXPECT_EQ(BloodMoonUniform(-1.0, 0.45, settings), Vector4 {});
	for (const auto theme : {0.0, 0.5, 1.0})
	{
		EXPECT_EQ(BloodMoonUniform(theme, 1.0, settings), Vector4 {});
	}
	EXPECT_EQ(BloodMoonUniform(std::nan(""), 1.0, settings), Vector4 {});
}

TEST(EvilNight, TheBloodMoonGrowsSmoothlyWithTheEvilValue)
{
	EvilNightSettings settings;
	float last = 1.0f;
	for (int step = 1; step <= 20; ++step)
	{
		const auto moon = BloodMoonUniform(-step / 20.0, 1.0, settings);
		EXPECT_GE(moon.y, last);
		EXPECT_LE(moon.y - last, 0.05f);
		last = moon.y;
	}
	EXPECT_FLOAT_EQ(last, 1.3f);
	EXPECT_FLOAT_EQ(BloodMoonUniform(-1.0, 1.0, settings).z, 0.6f);
	EXPECT_NEAR(BloodMoonUniform(-1.0, 0.7, settings).x, 0.5f, 1e-6);
	settings.bloodMoonStrength = 0.5;
	EXPECT_NEAR(BloodMoonUniform(-1.0, 1.0, settings).x, 0.5f, 1e-6);
	settings.bloodMoonStrength = 2.0;
	EXPECT_EQ(BloodMoonUniform(-1.0, 1.0, settings).x, 1.0f);
	settings.bloodMoon = false;
	EXPECT_EQ(BloodMoonUniform(-1.0, 1.0, settings), Vector4 {});
}

TEST(EvilNight, TheSettingsReadBack)
{
	const auto read = ParseSkyboxSettings(R"({"sky": {"evilBloodMoon": false, "evilBloodMoonStrength": 1.5}})");
	ASSERT_TRUE(read) << read.GetError().message;
	EXPECT_FALSE(read->sky.evil.bloodMoon);
	EXPECT_EQ(read->sky.evil.bloodMoonStrength, 1.5);
	EXPECT_EQ(*ParseSkyboxSettings(SkyboxSettingsJson(*read).dump()), *read);
	const auto change = CompareSkyboxSettings(SkyboxSettings {}, *read);
	EXPECT_TRUE(change.sky);
	EXPECT_TRUE(change.restartOnly.empty());
	EXPECT_FALSE(ParseSkyboxSettings(R"({"sky": {"evilBloodMoonStrength": 2.5}})"));
	EXPECT_FALSE(ParseSkyboxSettings(R"({"sky": {"evilBloodMoon": 1}})"));
	const SkyboxSettings defaults;
	EXPECT_TRUE(defaults.sky.evil.bloodMoon);
	EXPECT_EQ(defaults.sky.evil.bloodMoonStrength, 1.0);
}

TEST(EvilNight, AnOldSettingsFileWithTheRemovedEffectsLoads)
{
	// The settings of the effects that the mod no longer has: the parser ignores them, also with values that are
	// not correct now, and does not write them again.
	const auto read = ParseSkyboxSettings(R"({"sky": {"evilBloodMoon": true, "evilBloodMoonStrength": 0.75,
	                                       "evilConstellations": true, "evilConstellationStrength": 1.0,
	                                       "evilMeteors": false, "evilMeteorStrength": 1.5, "evilEye": true,
	                                       "evilEyeStrength": 2.5, "evilEmbers": "on", "evilEmberStrength": 1.0,
	                                       "evilLightning": true, "evilLightningStrength": 0.25,
	                                       "reduceFlashing": 1}})");
	ASSERT_TRUE(read) << read.GetError().message;
	EXPECT_TRUE(read->sky.evil.bloodMoon);
	EXPECT_EQ(read->sky.evil.bloodMoonStrength, 0.75);
	const auto written = SkyboxSettingsJson(*read);
	const auto& sky = written.at("sky");
	for (const auto* key :
	     {"evilConstellations", "evilConstellationStrength", "evilMeteors", "evilMeteorStrength", "evilEye", "evilEyeStrength",
	      "evilEmbers", "evilEmberStrength", "evilLightning", "evilLightningStrength", "reduceFlashing"})
	{
		EXPECT_FALSE(sky.contains(key)) << key;
	}
	EXPECT_TRUE(sky.contains("evilBloodMoon"));
	EXPECT_TRUE(sky.contains("evilBloodMoonStrength"));
}
