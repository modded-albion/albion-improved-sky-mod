/******************************************************************************
 * Copyright (c) 2026 Masters of Albion Reforged developers
 *****************************************************************************/

#pragma once

#include <cstddef>

#include <array>
#include <string_view>

#include <astro/Moon.h>
#include <astro/Sky.h>
#include <core/AuroraMode.h>
#include <core/SkyTheme.h>
#include <mod_core/UnityMath.h>

namespace moa_mod::sky
{

/// The global shader values that the sky mod sets in each frame. The shaders of the bundle read them. A value of
/// zero turns each effect off, so all zeros give the sky of the game.
enum class SkyUniform : size_t
{
	SkyParams,    ///< x: effects on; y: mip of the night cubemap; z: painted stars on; w: Milky Way strength.
	StarParams,   ///< x: star brightness; y: limiting magnitude; z, w: the clear sky and the full cloud levels.
	ThemeStyle,   ///< x, w: the scales of the size and the light of the stars (0 is 1); y, z: good and evil strengths.
	ThemeParams,  ///< x: the night sky of the mod on; y: the theme (-1 evil to +1 good); z: the aurora light.
	Celestial0,   ///< The three rows of the matrix from the world to the J2000 axes of the star catalogue.
	Celestial1,   ///<
	Celestial2,   ///<
	AuroraParams, ///< x: aurora on; y: geomagnetic latitude of the edge of the band; z: width.
	Geomagnetic0, ///< The three rows of the matrix from the world to the geomagnetic axes.
	Geomagnetic1, ///<
	Geomagnetic2, ///<
	SunWorld,     ///< xyz: the direction of the real sun in the world; w: the earthshine on the moon.
	MoonWorld,    ///< xyz: the direction of the real moon in the world; w: the brightness of the moon disc.
	MoonParams,   ///< x: the lit part of the disc; y: the radius of the disc on the quad; z: the halo; w: moon on.
	Count,
};

inline constexpr size_t k_SkyUniformCount = static_cast<size_t>(SkyUniform::Count);

/// The names of the values in the shaders, in the order of `SkyUniform`.
inline constexpr std::array<std::string_view, k_SkyUniformCount> k_SkyUniformNames {
    "_MoaSkyParams",    "_MoaStarParams", "_MoaThemeStyle",   "_MoaThemeParams",  "_MoaCelestial0",
    "_MoaCelestial1",   "_MoaCelestial2", "_MoaAuroraParams", "_MoaGeomagnetic0", "_MoaGeomagnetic1",
    "_MoaGeomagnetic2", "_MoaSunWorld",   "_MoaMoonWorld",    "_MoaMoonParams",
};

/// All the values, in the order of `SkyUniform`.
struct SkyUniforms
{
	std::array<Vector4, k_SkyUniformCount> values {};

	[[nodiscard]] Vector4& operator[](SkyUniform u) { return values[static_cast<size_t>(u)]; }
	[[nodiscard]] const Vector4& operator[](SkyUniform u) const { return values[static_cast<size_t>(u)]; }
};

/// What the sky mod shows. These are the live values of the settings `sky.*`.
struct SkyEffects
{
	bool stars = true;
	double starBrightness = 1.0;
	AuroraMode aurora = AuroraMode::Alignment;
	bool moonShader = true;
	/// The night sky of the mod (black, or the colours of the alignment) in the place of the painted night sky.
	bool alignmentTheme = true;
	double goodStrength = 1.0; ///< Multiplies the colours of the good sky.
	double evilStrength = 1.0; ///< Multiplies the colours of the evil sky.
};

/// The data that `ComputeSkyUniforms` needs.
struct SkyUniformInput
{
	double julianDay = 0.0;        ///< UTC.
	moa::astro::Observer observer; ///< The place of the settings `moon.latitude` and `moon.longitude`.
	double northDegrees = 0.0;     ///< The heading of the north in the world (`moon.northDegrees`).
	SkyEffects effects;            ///< What to show.
	moa::astro::MoonState moon;    ///< The moon and the sun at `julianDay`.
	double theme = 0.0;            ///< The theme of the sky now: -1 evil, 0 neutral, +1 good.
	int depthOfFieldLevel = 0;     ///< The depth of field level of the game.
};

/// Return the matrix from the world (Unity axes: y up) to the local axes of the observer (north, east, up), for a
/// north at `northDegrees` clockwise from +z.
[[nodiscard]] moa::astro::Matrix3 WorldToLocal(double northDegrees);

/// Return the world direction of a direction in the local axes of the observer.
[[nodiscard]] WorldPoint WorldDirection(moa::astro::Vec3 local, double northDegrees);

/// Return the value `SkyUniform::ThemeParams` for the theme `theme` (-1 evil to +1 good). The mod sets this value
/// in each frame while the theme moves.
[[nodiscard]] Vector4 ThemeUniform(const SkyEffects& effects, double theme);

/// Calculate all the global values of the sky shaders.
[[nodiscard]] SkyUniforms ComputeSkyUniforms(const SkyUniformInput& input);

} // namespace moa_mod::sky
