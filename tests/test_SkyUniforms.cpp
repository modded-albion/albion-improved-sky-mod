/******************************************************************************
 * Copyright (c) 2026 Masters of Albion Reforged developers
 *****************************************************************************/

#include <cmath>

#include <algorithm>
#include <numbers>
#include <set>
#include <string>

#include <core/MoonPlacement.h>
#include <core/SkyUniforms.h>
#include <gtest/gtest.h>

using namespace moa_mod;
using namespace moa_mod::sky;

namespace
{

constexpr double k_Degrees = 180.0 / std::numbers::pi;
constexpr moa::astro::Observer k_London {.latitude = 51.5, .longitude = -0.13};
/// 2026 October 20 at 0h UTC: the moon is waxing, near the first quarter.
constexpr double k_Midnight = 2461333.5;

SkyUniformInput Input(double julianDay, SkyEffects effects = {}, double northDegrees = 0.0)
{
	return {.julianDay = julianDay,
	        .observer = k_London,
	        .northDegrees = northDegrees,
	        .effects = effects,
	        .moon = moa::astro::ComputeMoon(julianDay)};
}

moa::astro::Vec3 Xyz(const Vector4& v)
{
	return {v.x, v.y, v.z};
}

/// Return the world direction of a catalogue direction: the rows of the matrix are the world axes of the
/// catalogue, so the world direction is the sum of the rows times the parts of the catalogue direction.
moa::astro::Vec3 WorldOfCatalogue(const SkyUniforms& u, moa::astro::Vec3 catalogue)
{
	return Xyz(u[SkyUniform::Celestial0]) * catalogue.x + Xyz(u[SkyUniform::Celestial1]) * catalogue.y +
	       Xyz(u[SkyUniform::Celestial2]) * catalogue.z;
}

} // namespace

TEST(SkyUniforms, HasOneUniqueShaderNameForEachValue)
{
	std::set<std::string> names;
	for (const auto name : k_SkyUniformNames)
	{
		EXPECT_TRUE(name.starts_with("_Moa")) << name;
		names.emplace(name);
	}
	EXPECT_EQ(names.size(), k_SkyUniformCount);
}

TEST(SkyUniforms, MakesTurnMatricesForTheStarsAndTheAurora)
{
	const auto u = ComputeSkyUniforms(Input(k_Midnight));
	for (const auto first : {SkyUniform::Celestial0, SkyUniform::Geomagnetic0})
	{
		const auto index = static_cast<size_t>(first);
		for (size_t a = 0; a < 3; ++a)
		{
			for (size_t b = 0; b < 3; ++b)
			{
				EXPECT_NEAR(moa::astro::Dot(Xyz(u.values[index + a]), Xyz(u.values[index + b])), a == b ? 1.0 : 0.0, 1e-6);
			}
		}
	}
}

TEST(SkyUniforms, PutsThePoleOfTheSkyAboveTheNorth)
{
	// With the north on +z, the pole of the sky is at the altitude of the latitude toward +z.
	const auto pole = WorldOfCatalogue(ComputeSkyUniforms(Input(k_Midnight)), {0.0, 0.0, 1.0});
	EXPECT_NEAR(std::asin(pole.y) * k_Degrees, k_London.latitude, 0.5);
	EXPECT_NEAR(pole.x, 0.0, 0.01);
	EXPECT_GT(pole.z, 0.6);
	// With the north on +x, the pole turns to +x.
	const auto turned = WorldOfCatalogue(ComputeSkyUniforms(Input(k_Midnight, {}, 90.0)), {0.0, 0.0, 1.0});
	EXPECT_GT(turned.x, 0.6);
	EXPECT_NEAR(turned.z, 0.0, 0.01);
}

TEST(SkyUniforms, AgreesWithTheMoonPlacement)
{
	const auto input = Input(k_Midnight);
	const auto u = ComputeSkyUniforms(input);
	const auto horizontal = moa::astro::MoonHorizontal(input.moon, input.observer);
	const auto expected = moon::WorldFromLocal(moa::astro::ToLocalDirection(horizontal), input.northDegrees);
	EXPECT_NEAR(u[SkyUniform::MoonWorld].x, expected.x, 1e-5);
	EXPECT_NEAR(u[SkyUniform::MoonWorld].y, expected.y, 1e-5);
	EXPECT_NEAR(u[SkyUniform::MoonWorld].z, expected.z, 1e-5);
	// The brightness of the disc is a part of the brightness of the game moon, not more.
	EXPECT_GT(u[SkyUniform::MoonWorld].w, 0.0f);
	EXPECT_LE(u[SkyUniform::MoonWorld].w, 1.0f);
}

TEST(SkyUniforms, PutsTheSunBelowTheHorizonAtMidnightAndAboveAtNoon)
{
	EXPECT_LT(ComputeSkyUniforms(Input(k_Midnight)).values[static_cast<size_t>(SkyUniform::SunWorld)].y, -0.3f);
	EXPECT_GT(ComputeSkyUniforms(Input(k_Midnight + 0.5)).values[static_cast<size_t>(SkyUniform::SunWorld)].y, 0.3f);
}

TEST(SkyUniforms, TurnsEachEffectOff)
{
	const auto off = ComputeSkyUniforms(
	    Input(k_Midnight, {.stars = false, .aurora = AuroraMode::Off, .moonShader = false, .alignmentTheme = false}));
	EXPECT_EQ(off[SkyUniform::SkyParams].x, 0.0f);
	EXPECT_EQ(off[SkyUniform::SkyParams].y, 0.0f);
	EXPECT_EQ(off[SkyUniform::StarParams].x, 0.0f);
	EXPECT_EQ(off[SkyUniform::AuroraParams].x, 0.0f);
	EXPECT_EQ(off[SkyUniform::MoonParams].w, 0.0f);
	EXPECT_EQ(off[SkyUniform::ThemeParams].x, 0.0f);

	// The aurora alone keeps the painted stars of the game.
	const auto aurora =
	    ComputeSkyUniforms(Input(k_Midnight, {.stars = false, .aurora = AuroraMode::Always, .alignmentTheme = false}));
	EXPECT_EQ(aurora[SkyUniform::SkyParams].x, 1.0f);
	EXPECT_EQ(aurora[SkyUniform::SkyParams].y, 0.0f);
	EXPECT_EQ(aurora[SkyUniform::SkyParams].z, 1.0f);

	const auto stars = ComputeSkyUniforms(Input(k_Midnight, {.stars = true, .starBrightness = 2.0}));
	EXPECT_EQ(stars[SkyUniform::SkyParams].x, 1.0f);
	EXPECT_GT(stars[SkyUniform::SkyParams].y, 0.0f);
	EXPECT_EQ(stars[SkyUniform::SkyParams].z, 0.0f);
	EXPECT_EQ(stars[SkyUniform::StarParams].x, 2.0f);
}

TEST(SkyUniforms, PutsTheAuroraBandAboveTheObserver)
{
	for (const auto mode : {AuroraMode::Alignment, AuroraMode::Always})
	{
		const auto u = ComputeSkyUniforms(Input(k_Midnight, {.aurora = mode}));
		EXPECT_EQ(u[SkyUniform::AuroraParams].x, 1.0f);
		EXPECT_NEAR(u[SkyUniform::AuroraParams].y, moa::astro::GeomagneticLatitude(k_London) + 1.5, 1e-4);
	}
	EXPECT_EQ(ComputeSkyUniforms(Input(k_Midnight, {.aurora = AuroraMode::Off}))[SkyUniform::AuroraParams].x, 0.0f);
}

TEST(SkyUniforms, LowersTheLimitOfTheStarsWithAHighBrightMoon)
{
	// Search one month for the night with the brightest high moon, and one with no moon in the sky.
	double brightest = 10.0;
	double darkest = 0.0;
	for (int hour = 0; hour < 30 * 24; hour += 3)
	{
		const auto u = ComputeSkyUniforms(Input(k_Midnight + hour / 24.0));
		brightest = std::min(brightest, static_cast<double>(u[SkyUniform::StarParams].y));
		darkest = std::max(darkest, static_cast<double>(u[SkyUniform::StarParams].y));
	}
	EXPECT_NEAR(darkest, 6.5, 1e-6);
	EXPECT_LT(brightest, 5.0);
}

TEST(SkyUniforms, ReadsTheAuroraModeNames)
{
	for (const auto mode : k_AuroraModes)
	{
		EXPECT_EQ(ParseAuroraMode(AuroraModeName(mode)), mode);
	}
	EXPECT_FALSE(ParseAuroraMode("sometimes").has_value());
}

TEST(SkyUniforms, GivesTheAuroraLightOnlyToAGoodTheme)
{
	const SkyEffects alignment {.aurora = AuroraMode::Alignment};
	EXPECT_EQ(ThemeUniform(alignment, -1.0).z, 0.0f);
	EXPECT_EQ(ThemeUniform(alignment, 0.0).z, 0.0f);
	EXPECT_FLOAT_EQ(ThemeUniform(alignment, 1.0).z, 0.6f);
	// The aurora follows the alignment also with the painted night sky.
	EXPECT_FLOAT_EQ(ThemeUniform({.aurora = AuroraMode::Alignment, .alignmentTheme = false}, 1.0).z, 0.6f);
	// The test mode shows the first light of the aurora for each theme; "off" shows none.
	EXPECT_EQ(ThemeUniform({.aurora = AuroraMode::Always}, -1.0).z, 1.0f);
	EXPECT_EQ(ThemeUniform({.aurora = AuroraMode::Off}, 1.0).z, 0.0f);
}

TEST(SkyUniforms, SetsTheThemeOfTheNightSky)
{
	auto input = Input(k_Midnight, {.goodStrength = 1.5, .evilStrength = 0.5});
	input.theme = -0.25;
	const auto u = ComputeSkyUniforms(input);
	EXPECT_EQ(u[SkyUniform::SkyParams].x, 1.0f);
	EXPECT_EQ(u[SkyUniform::ThemeParams].x, 1.0f);
	EXPECT_FLOAT_EQ(u[SkyUniform::ThemeParams].y, -0.25f);
	EXPECT_FLOAT_EQ(u[SkyUniform::ThemeStyle].y, 1.5f);
	EXPECT_FLOAT_EQ(u[SkyUniform::ThemeStyle].z, 0.5f);
	// The theme stays in the range of the shader.
	EXPECT_FLOAT_EQ(ThemeUniform({}, 3.0).y, 1.0f);
	// With the theme off, the painted night sky of the game and the colours of the real stars stay.
	EXPECT_EQ(ThemeUniform({.alignmentTheme = false}, 1.0).x, 0.0f);
	EXPECT_EQ(ThemeUniform({.alignmentTheme = false}, 1.0).y, 0.0f);
	// The theme alone turns the effects of the skybox on.
	const auto themeOnly = ComputeSkyUniforms(Input(k_Midnight, {.stars = false, .aurora = AuroraMode::Off}));
	EXPECT_EQ(themeOnly[SkyUniform::SkyParams].x, 1.0f);
}

TEST(SkyUniforms, MakesTheStarsLargerWithTheDepthOfField)
{
	auto input = Input(k_Midnight);
	EXPECT_FLOAT_EQ(ComputeSkyUniforms(input)[SkyUniform::ThemeStyle].x, 1.0f);
	EXPECT_FLOAT_EQ(ComputeSkyUniforms(input)[SkyUniform::ThemeStyle].w, 1.0f);
	input.depthOfFieldLevel = 3;
	EXPECT_GT(ComputeSkyUniforms(input)[SkyUniform::ThemeStyle].x, 1.5f);
	EXPECT_GT(ComputeSkyUniforms(input)[SkyUniform::ThemeStyle].w, 2.0f);
}
