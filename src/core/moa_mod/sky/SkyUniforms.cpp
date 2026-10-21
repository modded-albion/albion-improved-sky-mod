/******************************************************************************
 * Copyright (c) 2026 Masters of Albion Reforged developers
 *****************************************************************************/

#include <cmath>

#include <algorithm>
#include <numbers>

#include <moa_mod/sky/SkyUniforms.h>

namespace moa_mod::sky
{

namespace
{
constexpr double k_DegreesToRadiansD = std::numbers::pi / 180.0;
/// The mip of the night cubemap (2048 pixels on a face) for the sky behind the stars: 128 pixels on a face. The
/// painted stars go, the clouds and the glow stay.
constexpr float k_NightMip = 4.0f;
/// The strength of the Milky Way that the shader adds along the plane of the galaxy.
constexpr float k_MilkyWay = 0.5f;
/// The faintest star that shows in a dark sky with no moon.
constexpr double k_DarkSkyLimit = 6.5;
/// A full moon high in the sky makes the faintest star this much brighter.
constexpr double k_FullMoonPenalty = 2.0;
/// The brightness of the night cubemap (with no painted stars) for a clear sky, and for a full cloud.
constexpr float k_ClearSkyLevel = 0.03f;
constexpr float k_FullCloudLevel = 0.08f;
/// The earthshine at new moon, as a part of the light of the lit side.
constexpr double k_NewMoonEarthshine = 0.02;
/// The radius of the moon disc in the texture coordinates of the moon quad (0 to 1 across the quad). It is the size of
/// the moon in the phase texture of the game, so the moon of the mod has the same size on the screen.
constexpr float k_DiscRadius = 0.33f;
constexpr float k_Halo = 0.15f;
/// The brightness of the moon disc. The test shots in the game (with the night colour of the game and its bloom) gave
/// this value: the lit side is near white but not cut off, as bright as the moon of the game.
constexpr float k_MoonDiscBrightness = 0.2f;

[[nodiscard]] Vector4 Row(moa::astro::Vec3 row, float w = 0.0f)
{
	return {static_cast<float>(row.x), static_cast<float>(row.y), static_cast<float>(row.z), w};
}

[[nodiscard]] Vector4 Direction(WorldPoint direction, float w)
{
	return {direction.x, direction.y, direction.z, w};
}

[[nodiscard]] moa::astro::Vec3 LocalVector(moa::astro::Horizontal direction)
{
	const auto local = moa::astro::ToLocalDirection(direction);
	return {local.north, local.east, local.up};
}
} // namespace

moa::astro::Matrix3 WorldToLocal(double northDegrees)
{
	const auto radians = northDegrees * k_DegreesToRadiansD;
	const auto s = std::sin(radians);
	const auto c = std::cos(radians);
	// The rows are the world directions of the north, the east and the up (see `moon::WorldFromLocal`).
	return {{moa::astro::Vec3 {s, 0.0, c}, moa::astro::Vec3 {c, 0.0, -s}, moa::astro::Vec3 {0.0, 1.0, 0.0}}};
}

Vector4 ThemeUniform(const SkyEffects& effects, double theme)
{
	const auto clamped = std::clamp(theme, -1.0, 1.0);
	float aurora = 0.0f;
	switch (effects.aurora)
	{
	case AuroraMode::Off:
		break;
	case AuroraMode::Alignment:
		aurora = static_cast<float>(AuroraLightForTheme(clamped));
		break;
	case AuroraMode::Always:
		aurora = 1.0f;
		break;
	}
	// With the theme off, the painted night sky of the game and the colours of the real stars stay.
	if (!effects.alignmentTheme)
	{
		return {0.0f, 0.0f, aurora, 0.0f};
	}
	return {1.0f, static_cast<float>(clamped), aurora, 0.0f};
}

WorldPoint WorldDirection(moa::astro::Vec3 local, double northDegrees)
{
	const auto world = WorldToLocal(northDegrees).Transposed().Apply(local);
	return {static_cast<float>(world.x), static_cast<float>(world.y), static_cast<float>(world.z)};
}

SkyUniforms ComputeSkyUniforms(const SkyUniformInput& input)
{
	using moa::astro::Matrix3;
	const auto& effects = input.effects;
	const auto& phase = input.moon.phase;
	SkyUniforms u;

	// The stars: the matrix from the world to the catalogue is the transpose of the matrix from the catalogue to
	// the world, because both are turns.
	const auto worldToLocal = WorldToLocal(input.northDegrees);
	const Matrix3 worldToCatalogue = moa::astro::CatalogueToLocal(input.julianDay, input.observer).Transposed() * worldToLocal;
	u[SkyUniform::Celestial0] = Row(worldToCatalogue.rows[0]);
	u[SkyUniform::Celestial1] = Row(worldToCatalogue.rows[1]);
	u[SkyUniform::Celestial2] = Row(worldToCatalogue.rows[2]);

	const auto moonHorizontal = moa::astro::MoonHorizontal(input.moon, input.observer);
	const auto moonHeight = std::clamp(moonHorizontal.altitude / 20.0, 0.0, 1.0);
	const auto limit = k_DarkSkyLimit - k_FullMoonPenalty * phase.illuminatedFraction * moonHeight;
	const auto brightness = effects.stars ? static_cast<float>(std::max(effects.starBrightness, 0.0)) : 0.0f;
	u[SkyUniform::StarParams] = {brightness, static_cast<float>(limit), k_ClearSkyLevel, k_FullCloudLevel};
	u[SkyUniform::ThemeStyle] = {static_cast<float>(StarSizeForDepthOfField(input.depthOfFieldLevel)),
	                             static_cast<float>(std::max(effects.goodStrength, 0.0)),
	                             static_cast<float>(std::max(effects.evilStrength, 0.0)),
	                             static_cast<float>(StarLightForDepthOfField(input.depthOfFieldLevel))};
	u[SkyUniform::ThemeParams] = ThemeUniform(effects, input.theme);

	// The aurora: the band is a little to the north of the observer, high in the northern sky. The theme decides its
	// light in each frame (`ThemeUniform`).
	const auto oval = moa::astro::AuroraOvalOverhead(moa::astro::GeomagneticLatitude(input.observer));
	const auto auroraOn = effects.aurora != AuroraMode::Off;
	u[SkyUniform::AuroraParams] = {auroraOn ? 1.0f : 0.0f, static_cast<float>(oval.equatorwardEdge),
	                               static_cast<float>(oval.width), 0.0f};
	const Matrix3 worldToGeomagnetic = moa::astro::LocalToGeomagnetic(input.observer) * worldToLocal;
	u[SkyUniform::Geomagnetic0] = Row(worldToGeomagnetic.rows[0]);
	u[SkyUniform::Geomagnetic1] = Row(worldToGeomagnetic.rows[1]);
	u[SkyUniform::Geomagnetic2] = Row(worldToGeomagnetic.rows[2]);

	const bool anyEffect = effects.stars || auroraOn || effects.alignmentTheme;
	u[SkyUniform::SkyParams] = {anyEffect ? 1.0f : 0.0f, effects.stars ? k_NightMip : 0.0f, effects.stars ? 0.0f : 1.0f,
	                            effects.stars ? k_MilkyWay : 0.0f};

	// The moon: the shader lights the disc with the real sun, and looks at it from the real moon direction.
	const auto sun = moa::astro::ToHorizontal(input.moon.sun.equatorial, input.observer, input.julianDay);
	const auto earthshine = k_NewMoonEarthshine * (1.0 - std::cos(phase.phaseAngle * k_DegreesToRadiansD)) / 2.0;
	u[SkyUniform::SunWorld] = Direction(WorldDirection(LocalVector(sun), input.northDegrees), static_cast<float>(earthshine));
	u[SkyUniform::MoonWorld] = Direction(WorldDirection(LocalVector(moonHorizontal), input.northDegrees), k_MoonDiscBrightness);
	u[SkyUniform::MoonParams] = {static_cast<float>(phase.illuminatedFraction), k_DiscRadius, k_Halo,
	                             effects.moonShader ? 1.0f : 0.0f};
	return u;
}

} // namespace moa_mod::sky
