/******************************************************************************
 * Copyright (c) 2026 Masters of Albion Reforged developers
 *****************************************************************************/

#include <cmath>

#include <numbers>
#include <utility>

#include <core/MoonPlacement.h>
#include <gtest/gtest.h>

using namespace moa_mod;
using namespace moa_mod::moon;

namespace
{

void ExpectNear(WorldPoint actual, WorldPoint expected, float tolerance = 1e-4f)
{
	EXPECT_NEAR(actual.x, expected.x, tolerance);
	EXPECT_NEAR(actual.y, expected.y, tolerance);
	EXPECT_NEAR(actual.z, expected.z, tolerance);
}

} // namespace

TEST(MoonPlacement, PutsTheNorthOnTheWorldAxisOfTheSetting)
{
	const auto north = moa::astro::ToLocalDirection({.altitude = 0.0, .azimuth = 0.0});
	const auto east = moa::astro::ToLocalDirection({.altitude = 0.0, .azimuth = 90.0});
	const auto zenith = moa::astro::ToLocalDirection({.altitude = 90.0, .azimuth = 0.0});
	ExpectNear(WorldFromLocal(north, 0.0), {0.0f, 0.0f, 1.0f});
	ExpectNear(WorldFromLocal(east, 0.0), {1.0f, 0.0f, 0.0f});
	ExpectNear(WorldFromLocal(zenith, 0.0), {0.0f, 1.0f, 0.0f});
	// With the north at 90 degrees, the north is on +x and the east on -z (clockwise when seen from above).
	ExpectNear(WorldFromLocal(north, 90.0), {1.0f, 0.0f, 0.0f});
	ExpectNear(WorldFromLocal(east, 90.0), {0.0f, 0.0f, -1.0f});
}

TEST(MoonPlacement, PutsTheQuadOnTheSkyFacingTheCentre)
{
	const WorldPoint centre {10.0f, 20.0f, 30.0f};
	const auto placement = PlaceMoon({.horizontal = {.altitude = 30.0, .azimuth = 135.0},
	                                  .latitude = 51.5,
	                                  .brightLimbAngle = 250.0,
	                                  .waxing = true,
	                                  .northDegrees = 0.0,
	                                  .centre = centre,
	                                  .distance = 3000.0f});
	EXPECT_TRUE(placement.aboveHorizon);
	EXPECT_NEAR(Length(placement.position - centre), 3000.0f, 0.1f);
	EXPECT_NEAR(placement.direction.y, 0.5f, 1e-4f);
	// South-east is +x and -z.
	EXPECT_GT(placement.direction.x, 0.0f);
	EXPECT_LT(placement.direction.z, 0.0f);
	// The front face of the mesh (local +z) points to the centre. The light shines from the moon.
	ExpectNear(Rotate(placement.quadRotation, {0.0f, 0.0f, 1.0f}), placement.direction * -1.0f);
	ExpectNear(Rotate(placement.lightRotation, {0.0f, 0.0f, 1.0f}), placement.direction * -1.0f);
	EXPECT_NEAR(placement.rollDegrees, -20.0, 1e-9);
	EXPECT_FALSE(PlaceMoon({.horizontal = {.altitude = -5.0, .azimuth = 0.0}}).aboveHorizon);
}

TEST(MoonPlacement, TurnsTheLitSideOfTheTextureToTheBrightLimb)
{
	// The moon is in the south on the meridian, for an observer at 50 degrees north.
	MoonPlacementInput input {.horizontal = {.altitude = 30.0, .azimuth = 180.0}, .latitude = 50.0, .distance = 1.0f};
	const auto axes = moa::astro::SkyAxesAt(input.horizontal, input.latitude);
	const auto north = WorldFromLocal(axes.north, 0.0);
	const auto east = WorldFromLocal(axes.east, 0.0);
	for (const auto& [limb, waxing] : {std::pair {270.0, true}, std::pair {90.0, false}, std::pair {300.0, true}})
	{
		input.brightLimbAngle = limb;
		input.waxing = waxing;
		const auto placement = PlaceMoon(input);
		// The lit side of the frame is the right of the texture (local -x) for a waxing frame, and the left for a
		// waning frame. It must point to the bright limb on the sky.
		const auto litSide = Rotate(placement.quadRotation, {waxing ? -1.0f : 1.0f, 0.0f, 0.0f});
		const auto radians = static_cast<float>(limb * std::numbers::pi / 180.0);
		ExpectNear(litSide, north * std::cos(radians) + east * std::sin(radians), 1e-4f);
	}
	// In the south on the meridian, the north on the sky is up, and the east is the world east (left of the view).
	EXPECT_GT(north.y, 0.0f);
	ExpectNear(east, {1.0f, 0.0f, 0.0f});
}

TEST(MoonPlacement, GivesTheTextureOffsetOfEachFrame)
{
	EXPECT_FLOAT_EQ(PhaseTextureOffset(0, 8), 0.0f);
	EXPECT_FLOAT_EQ(PhaseTextureOffset(3, 8), 0.375f);
	EXPECT_FLOAT_EQ(PhaseTextureOffset(7, 8), 0.875f);
	EXPECT_FLOAT_EQ(PhaseTextureOffset(3, 0), 0.0f);
}
