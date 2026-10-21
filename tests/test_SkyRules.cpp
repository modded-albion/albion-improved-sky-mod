/******************************************************************************
 * Copyright (c) 2026 Masters of Albion Reforged developers
 *****************************************************************************/

#include <cstddef>

#include <algorithm>
#include <array>

#include <gtest/gtest.h>
#include <moa_mod/sky/SkyRules.h>
#include <moa_mod/sky/SkyboxSettings.h>

using namespace moa_mod;
using namespace moa_mod::sky;

TEST(SkyRules, TheStagesGoInOrderToReady)
{
	auto stage = LoadStage::WriteFile;
	const std::array expected {LoadStage::StartLoad, LoadStage::WaitForBundle, LoadStage::LoadMaterial,
	                           LoadStage::PlaceQuad, LoadStage::Ready,         LoadStage::Ready};
	for (const auto next : expected)
	{
		stage = NextStage(stage, StepResult::Done);
		EXPECT_EQ(stage, next) << StageName(stage);
	}
}

TEST(SkyRules, AStageThatWaitsStaysAndAnErrorStopsAll)
{
	EXPECT_EQ(NextStage(LoadStage::WaitForBundle, StepResult::Wait), LoadStage::WaitForBundle);
	EXPECT_EQ(NextStage(LoadStage::LoadMaterial, StepResult::Error), LoadStage::Failed);
	EXPECT_EQ(NextStage(LoadStage::Failed, StepResult::Done), LoadStage::Failed);
	EXPECT_EQ(NextStage(LoadStage::Ready, StepResult::Error), LoadStage::Ready);
}

TEST(SkyRules, TheFileIsCurrentOnlyWithTheSameBytes)
{
	const std::array bundle {std::byte {1}, std::byte {2}, std::byte {3}};
	const std::array other {std::byte {1}, std::byte {2}, std::byte {4}};
	const std::array shorter {std::byte {1}, std::byte {2}};
	EXPECT_TRUE(BundleFileIsCurrent(bundle, bundle));
	EXPECT_FALSE(BundleFileIsCurrent(other, bundle));
	EXPECT_FALSE(BundleFileIsCurrent(shorter, bundle));
	// A mod with no bundle never uses a file.
	EXPECT_FALSE(BundleFileIsCurrent({}, {}));
}

TEST(SkyRules, TheNightIsWhenTheSunOfTheGameIsDown)
{
	EXPECT_TRUE(IsNight(0.0f));
	EXPECT_TRUE(IsNight(0.2f));
	EXPECT_FALSE(IsNight(0.25f));
	EXPECT_FALSE(IsNight(0.5f));
	EXPECT_FALSE(IsNight(0.74f));
	EXPECT_TRUE(IsNight(0.75f));
	EXPECT_TRUE(IsNight(0.99f));
	EXPECT_TRUE(IsNight(1.1f));
}

TEST(SkyRules, TheQuadFacesTheCentreOfTheSky)
{
	const auto placement = PlaceOnSky(1000.0f, 30.0f, 90.0f);
	EXPECT_NEAR(placement.offset.x, 866.03f, 0.01f);
	EXPECT_NEAR(placement.offset.y, 500.0f, 0.01f);
	EXPECT_NEAR(placement.offset.z, 0.0f, 0.01f);
	const auto front = Rotate(placement.rotation, WorldPoint {0.0f, 0.0f, 1.0f});
	EXPECT_NEAR(front.x, -0.86603f, 1e-4f);
	EXPECT_NEAR(front.y, -0.5f, 1e-4f);
	EXPECT_NEAR(front.z, 0.0f, 1e-4f);
	const auto up = Rotate(placement.rotation, WorldPoint {0.0f, 1.0f, 0.0f});
	EXPECT_GT(up.y, 0.0f);
}

TEST(SkyRules, TheSkySettingsAreOffByDefaultAndReadBack)
{
	EXPECT_FALSE(SkyboxSettings {}.sky.enabled);
	const auto read = ParseSkyboxSettings(R"({"sky": {"enabled": true, "testQuad": false}})");
	ASSERT_TRUE(read) << read.GetError().message;
	EXPECT_TRUE(read->sky.enabled);
	EXPECT_FALSE(read->sky.testQuad);
	EXPECT_FALSE(ParseSkyboxSettings(R"({"sky": {"enabled": 1}})"));
	EXPECT_EQ(*ParseSkyboxSettings(SkyboxSettingsJson(*read).dump()), *read);
	const auto change = CompareSkyboxSettings(SkyboxSettings {}, *read);
	EXPECT_NE(std::ranges::find(change.restartOnly, "sky.enabled"), change.restartOnly.end());
	EXPECT_FALSE(LiveSkyboxSettings(SkyboxSettings {}, *read).sky.enabled);
}

TEST(SkyRules, TheEffectSettingsReadBackAndWorkAtOnce)
{
	const auto read =
	    ParseSkyboxSettings(R"({"sky": {"stars": false, "starBrightness": 2.5, "aurora": "always", "moonShader": false}})");
	ASSERT_TRUE(read) << read.GetError().message;
	EXPECT_FALSE(read->sky.stars);
	EXPECT_EQ(read->sky.starBrightness, 2.5);
	EXPECT_EQ(read->sky.aurora, AuroraMode::Always);
	EXPECT_FALSE(read->sky.moonShader);
	EXPECT_EQ(*ParseSkyboxSettings(SkyboxSettingsJson(*read).dump()), *read);
	const auto change = CompareSkyboxSettings(SkyboxSettings {}, *read);
	EXPECT_TRUE(change.sky);
	EXPECT_TRUE(change.restartOnly.empty());
	EXPECT_EQ(LiveSkyboxSettings(SkyboxSettings {}, *read).sky, read->sky);

	EXPECT_FALSE(ParseSkyboxSettings(R"({"sky": {"aurora": "sometimes"}})"));
	EXPECT_FALSE(ParseSkyboxSettings(R"({"sky": {"aurora": 1}})"));
	EXPECT_FALSE(ParseSkyboxSettings(R"({"sky": {"starBrightness": 6}})"));
}
