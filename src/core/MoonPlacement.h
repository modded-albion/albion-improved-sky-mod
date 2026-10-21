/******************************************************************************
 * Copyright (c) 2026 Masters of Albion Reforged developers
 *****************************************************************************/

#pragma once

#include <cmath>

#include <numbers>

#include <astro/Moon.h>
#include <mod_core/UnityMath.h>

namespace moa_mod::moon
{

/// The moon texture of the game shows the moon with the north up and the west to the right, as an observer in
/// the northern hemisphere sees it. Thus the lit side of a waxing frame points to the west (position angle 270
/// degrees), and the lit side of a waning frame points to the east (90 degrees).
constexpr double k_WaxingTextureLitAngle = 270.0;
constexpr double k_WaningTextureLitAngle = 90.0;

/// Return the world direction (Unity axes) of a direction in the frame of the observer. `northDegrees` is the
/// heading of the north in the world, clockwise from +z when seen from above: 0 puts the north on +z and the east
/// on +x.
[[nodiscard]] inline WorldPoint WorldFromLocal(const moa::astro::LocalDirection& local, double northDegrees)
{
	const auto radians = northDegrees * std::numbers::pi / 180.0;
	const auto northX = std::sin(radians);
	const auto northZ = std::cos(radians);
	// The east is 90 degrees clockwise from the north: (cos, 0, -sin) of the north heading.
	const auto eastX = northZ;
	const auto eastZ = -northX;
	return {
	    .x = static_cast<float>(local.north * northX + local.east * eastX),
	    .y = static_cast<float>(local.up),
	    .z = static_cast<float>(local.north * northZ + local.east * eastZ),
	};
}

/// Where the moon mod puts the moon quad and the moon light.
struct MoonPlacement
{
	WorldPoint direction;    ///< From the sky centre to the moon, length 1.
	WorldPoint position;     ///< The world position of the moon quad.
	Quaternion quadRotation; ///< The world rotation of the moon quad.
	Quaternion lightRotation;
	double rollDegrees = 0.0; ///< The turn of the texture from the north on the sky, as a position angle.
	bool aboveHorizon = false;
};

/// The data that `PlaceMoon` needs.
struct MoonPlacementInput
{
	moa::astro::Horizontal horizontal; ///< The direction of the moon for the observer.
	double latitude = 0.0;             ///< The latitude of the observer, degrees.
	double brightLimbAngle = 0.0;      ///< Position angle of the lit limb, degrees.
	bool waxing = true;                ///< True if the game shows a waxing frame.
	double northDegrees = 0.0;         ///< The heading of the north in the world.
	WorldPoint centre;                 ///< The centre of the sky (the camera).
	float distance = 0.0f;             ///< From the centre of the sky to the moon quad.
};

/// Calculate the moon quad and the moon light. The mesh of the moon quad has its front face to local +z, the top
/// of the texture to local +y and the right of the texture to local -x. The quad must look at the centre. Its
/// texture turns so that the lit side of the frame is at the bright limb angle.
[[nodiscard]] inline MoonPlacement PlaceMoon(const MoonPlacementInput& input)
{
	const auto local = moa::astro::ToLocalDirection(input.horizontal);
	const auto axes = moa::astro::SkyAxesAt(input.horizontal, input.latitude);
	const auto direction = Normalize(WorldFromLocal(local, input.northDegrees));
	const auto north = WorldFromLocal(axes.north, input.northDegrees);
	const auto east = WorldFromLocal(axes.east, input.northDegrees);

	const auto litAngle = input.waxing ? k_WaxingTextureLitAngle : k_WaningTextureLitAngle;
	const auto roll = input.brightLimbAngle - litAngle;
	const auto radians = static_cast<float>(roll * std::numbers::pi / 180.0);
	// The top of the texture points to the position angle `roll`.
	const auto textureUp = north * std::cos(radians) + east * std::sin(radians);
	const auto toCentre = direction * -1.0f;
	return {
	    .direction = direction,
	    .position = input.centre + direction * input.distance,
	    .quadRotation = LookRotation(toCentre, textureUp),
	    .lightRotation = LookRotation(toCentre, WorldPoint {0.0f, 1.0f, 0.0f}),
	    .rollDegrees = roll,
	    .aboveHorizon = input.horizontal.altitude > 0.0,
	};
}

/// Return the horizontal offset of the moon texture that shows `frame` of a strip of `frameCount` frames. The mesh
/// of the quad shows one frame, so the offset of frame n is n divided by the frame count.
[[nodiscard]] inline float PhaseTextureOffset(int frame, int frameCount)
{
	return frameCount > 0 ? static_cast<float>(frame) / static_cast<float>(frameCount) : 0.0f;
}

} // namespace moa_mod::moon
