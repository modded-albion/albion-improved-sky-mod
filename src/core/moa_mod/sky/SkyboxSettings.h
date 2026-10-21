/******************************************************************************
 * Copyright (c) 2026 Masters of Albion Reforged developers
 *****************************************************************************/

#pragma once

#include <chrono>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <moa/common/Result.h>
#include <moa_mod/SettingsFields.h>
#include <moa_mod/sky/AuroraMode.h>
#include <moa_mod/sky/EvilNightSettings.h>
#include <nlohmann/json.hpp>

namespace moa_mod
{

/// The settings of the sky mod: the objects `moon` and `sky` of mods.json. Each part is off until the file turns it
/// on. A missing field keeps its default.
struct SkyboxSettings
{
	/// The real moon: phase, position and light from the clock of the computer.
	struct Moon
	{
		bool enabled = false;
		double latitude = 51.5;         ///< Degrees, north positive.
		double longitude = -0.1;        ///< Degrees, east positive.
		double northDegrees = 0.0;      ///< The heading of the north in the world, clockwise from +z, degrees.
		bool followRealPosition = true; ///< Put the moon at its real place on the sky.
		bool phaseOnly = false;         ///< Change only the phase frame. The game keeps the place and the light.
		double lightScale = 1.0;        ///< Multiplies the moon light, after the lit part of the disc.

		friend bool operator==(const Moon&, const Moon&) = default;
	} moon;

	/// The night sky: shaders from an AssetBundle that the mod loads. `enabled` and `testQuad` work only after a
	/// restart of the game. The other values work at once.
	struct Sky
	{
		bool enabled = false;        ///< Load the AssetBundle of the sky mod and use its skybox.
		bool stars = true;           ///< Show the real stars in the place of the painted stars.
		double starBrightness = 1.0; ///< Multiplies the light of the stars, 0 to 5.
		sky::AuroraMode aurora = sky::AuroraMode::Alignment;
		bool moonShader = true; ///< Show the moon as a lit ball with the real phase.
		/// Show the night sky of the mod in the place of the painted night sky: black for a neutral player, a faint
		/// rainbow for a good player, a dark red for an evil player. Off keeps the painted night sky of the game.
		bool alignmentTheme = true;
		/// The sky, the stars and the aurora follow the alignment of the player. Off: they follow `alignment`.
		bool useGameAlignment = true;
		/// The alignment that the sky shows when `useGameAlignment` is off: -1 fully evil, 0 neutral, +1 fully good.
		/// It changes only the sky of the mod, never the alignment in the game or the save.
		double alignment = 0.0;
		double goodStrength = 1.0; ///< Multiplies the colours of the good sky, 0 to 2.
		double evilStrength = 1.0; ///< Multiplies the colours of the evil sky, 0 to 2.
		/// The blood moon of the evil night (the keys `evilBloodMoon` and `evilBloodMoonStrength` of the object `sky`).
		sky::EvilNightSettings evil;
		/// Show a test quad with the material of the bundle in the night sky. For research only.
		bool testQuad = false;

		friend bool operator==(const Sky&, const Sky&) = default;
	} sky;

	friend bool operator==(const SkyboxSettings&, const SkyboxSettings&) = default;
};

/// Read the object `moon` of mods.json into `settings.moon`. Return an error and keep `settings` if a field has a wrong
/// type or a value is out of range.
[[nodiscard]] moa::Result<void> ReadMoonSettings(const nlohmann::json& object, SkyboxSettings& settings);

/// Read the object `sky` of mods.json into `settings.sky`. Return an error and keep `settings` if a field has a wrong
/// type or a value is out of range.
[[nodiscard]] moa::Result<void> ReadSkySettings(const nlohmann::json& object, SkyboxSettings& settings);

/// Read the objects `moon` and `sky` of the JSON text of mods.json. The other objects stay unread.
[[nodiscard]] moa::Result<SkyboxSettings> ParseSkyboxSettings(std::string_view json);

/// The settings as the objects `moon` and `sky` of mods.json, in the form that `ParseSkyboxSettings` reads.
[[nodiscard]] nlohmann::json SkyboxSettingsJson(const SkyboxSettings& settings);

/// What new settings change in the running sky mod.
struct SkyboxSettingsChange
{
	bool moonEnabled = false; ///< `moon.enabled` changed.
	bool moon = false;        ///< A value of the moon other than `enabled` changed.
	bool sky = false;         ///< A value of `sky` changed that works at once (not `enabled` and `testQuad`).
	/// The names of the changed fields that the mod uses only after a restart of the game.
	std::vector<std::string> restartOnly;

	[[nodiscard]] bool Any() const { return moonEnabled || moon || sky; }
};

[[nodiscard]] SkyboxSettingsChange CompareSkyboxSettings(const SkyboxSettings& current, const SkyboxSettings& next);

/// The settings that the mod can use now: `next`, with the values of `current` for the fields that need a restart.
[[nodiscard]] SkyboxSettings LiveSkyboxSettings(const SkyboxSettings& current, const SkyboxSettings& next);

/// Turn the moon mod on or off, for its hotkey. Return the change of mods.json and a message.
[[nodiscard]] settings::SettingsEdit ToggleMoon(SkyboxSettings& settings);

/// The fields of the menu Sky, in the order of the menu.
[[nodiscard]] std::span<const settings::Field<SkyboxSettings>> SkyboxFields();

/// Change one field. Return the change of mods.json and a message, or an error and keep `settings`.
[[nodiscard]] moa::Result<settings::SettingsEdit>
EditSkyboxField(SkyboxSettings& settings, const settings::Field<SkyboxSettings>& field, const settings::FieldValue& value);

/// The moon as the menu bar shows it.
struct MoonReadout
{
	std::string_view phaseName {};
	double illuminatedPercent = 0.0;
	double ageDays = 0.0;
	double altitudeDegrees = 0.0;
	double azimuthDegrees = 0.0;
	bool waxing = false;
};

/// The name of the phase for the part of the lunar month that passed (0 and 1 are the new moon).
[[nodiscard]] std::string_view PhaseName(double cycleFraction);

/// Calculate the moon for a time (UTC) and the place of the observer in the settings.
[[nodiscard]] MoonReadout ReadMoon(std::chrono::system_clock::time_point utc, const SkyboxSettings::Moon& settings);

} // namespace moa_mod
