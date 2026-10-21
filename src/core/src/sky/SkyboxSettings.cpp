/******************************************************************************
 * Copyright (c) 2026 Masters of Albion Reforged developers
 *****************************************************************************/

#include <cmath>

#include <array>
#include <format>

#include <moa/astro/Moon.h>
#include <moa_mod/sky/SkyboxSettings.h>

namespace moa_mod
{

using settings::Field;
using settings::FieldKind;
using settings::ReadField;

namespace
{
/// The names of the aurora modes in mods.json, in the order of `sky::k_AuroraModes`.
constexpr std::array<std::string_view, 3> k_AuroraModeNames {sky::AuroraModeName(sky::k_AuroraModes[0]),
                                                             sky::AuroraModeName(sky::k_AuroraModes[1]),
                                                             sky::AuroraModeName(sky::k_AuroraModes[2])};

/// Read the settings of the evil night from the object `sky`. A missing key keeps its value. The keys of the effects
/// that the mod no longer has (for example `evilMeteors` and `reduceFlashing`) are not settings: the parser ignores
/// them, as all the keys of `sky` that it does not know, so that an old settings file loads.
std::optional<moa::Error> ReadEvilNight(const nlohmann::json& object, sky::EvilNightSettings& value)
{
	if (!ReadField(object, "evilBloodMoon", value.bloodMoon) ||
	    !ReadField(object, "evilBloodMoonStrength", value.bloodMoonStrength))
	{
		return moa::MakeError("the object 'sky' has an evil night field with a wrong type");
	}
	if (!(value.bloodMoonStrength >= 0.0 && value.bloodMoonStrength <= 2.0))
	{
		return moa::MakeError("the strength of the blood moon must be 0 to 2");
	}
	return std::nullopt;
}

// The accessors of each field. A lambda with no capture becomes a plain function pointer.
#define MOA_TOGGLE(path) \
	.getToggle = [](const SkyboxSettings& s) { return s.path; }, .setToggle = [](SkyboxSettings& s, bool v) { s.path = v; }
#define MOA_NUMBER(path) \
	.getNumber = [](const SkyboxSettings& s) { return s.path; }, .setNumber = [](SkyboxSettings& s, double v) { s.path = v; }

const std::array k_Fields {
    Field<SkyboxSettings> {.group = "Moon",
                           .section = "moon",
                           .key = "enabled",
                           .label = "Moon mod on",
                           .help = "The phase, place and light of the moon come from the clock of the computer.",
                           MOA_TOGGLE(moon.enabled)},
    Field<SkyboxSettings> {.group = "Moon",
                           .section = "moon",
                           .key = "followRealPosition",
                           .label = "Real position",
                           .help = "Put the moon at its real place on the sky.",
                           MOA_TOGGLE(moon.followRealPosition)},
    Field<SkyboxSettings> {.group = "Moon",
                           .section = "moon",
                           .key = "phaseOnly",
                           .label = "Phase only",
                           .help = "Change only the phase. The game keeps the place and the light.",
                           MOA_TOGGLE(moon.phaseOnly)},
    Field<SkyboxSettings> {.group = "Moon",
                           .section = "moon",
                           .key = "lightScale",
                           .label = "Light scale",
                           .help = "Multiplies the moon light, after the lit part of the disc.",
                           .kind = FieldKind::Number,
                           .min = 0.0,
                           .max = 10.0,
                           .step = 0.01,
                           .format = "%.2f",
                           MOA_NUMBER(moon.lightScale)},
    Field<SkyboxSettings> {.group = "Place",
                           .section = "moon",
                           .key = "latitude",
                           .label = "Latitude",
                           .help = "Degrees, north positive.",
                           .kind = FieldKind::Number,
                           .min = -90.0,
                           .max = 90.0,
                           .step = 0.01,
                           .format = "%.2f deg",
                           MOA_NUMBER(moon.latitude)},
    Field<SkyboxSettings> {.group = "Place",
                           .section = "moon",
                           .key = "longitude",
                           .label = "Longitude",
                           .help = "Degrees, east positive.",
                           .kind = FieldKind::Number,
                           .min = -180.0,
                           .max = 180.0,
                           .step = 0.01,
                           .format = "%.2f deg",
                           MOA_NUMBER(moon.longitude)},
    Field<SkyboxSettings> {.group = "Place",
                           .section = "moon",
                           .key = "northDegrees",
                           .label = "North heading",
                           .help = "The heading of the north in the world, clockwise from +z.",
                           .kind = FieldKind::Number,
                           .min = -360.0,
                           .max = 360.0,
                           .step = 1.0,
                           .format = "%.0f deg",
                           MOA_NUMBER(moon.northDegrees)},
    // The night sky, in the sky menu.
    Field<SkyboxSettings> {.group = "Night sky",
                           .section = "sky",
                           .key = "enabled",
                           .label = "Sky mod on",
                           .help = "Load the shaders of the sky mod: the real stars, the aurora and the lit moon.",
                           .restartOnly = true,
                           MOA_TOGGLE(sky.enabled)},
    Field<SkyboxSettings> {.group = "Night sky",
                           .section = "sky",
                           .key = "stars",
                           .label = "Real stars",
                           .help = "Show the stars of the Yale Bright Star Catalogue in the place of the painted stars.",
                           MOA_TOGGLE(sky.stars)},
    Field<SkyboxSettings> {.group = "Night sky",
                           .section = "sky",
                           .key = "starBrightness",
                           .label = "Star brightness",
                           .help = "Multiplies the light of the stars.",
                           .kind = FieldKind::Number,
                           .min = 0.0,
                           .max = 5.0,
                           .step = 0.05,
                           .format = "%.2f",
                           MOA_NUMBER(sky.starBrightness)},
    Field<SkyboxSettings> {.group = "Night sky",
                           .section = "sky",
                           .key = "aurora",
                           .label = "Aurora",
                           .help =
                               "off: no aurora. alignment: only for a good player, stronger when more good. always: for tests.",
                           .kind = FieldKind::Choice,
                           .choices = k_AuroraModeNames,
                           .getChoice = [](const SkyboxSettings& s) { return static_cast<size_t>(s.sky.aurora); },
                           .setChoice = [](SkyboxSettings& s, size_t v) { s.sky.aurora = sky::k_AuroraModes[v]; }},
    Field<SkyboxSettings> {.group = "Night sky",
                           .section = "sky",
                           .key = "moonShader",
                           .label = "Lit moon",
                           .help = "Show the moon as a lit ball with the real phase. It hides the stars behind it.",
                           MOA_TOGGLE(sky.moonShader)},
    Field<SkyboxSettings> {.group = "Alignment sky",
                           .section = "sky",
                           .key = "alignmentTheme",
                           .label = "Alignment sky",
                           .help =
                               "Show a night sky for the alignment: black for neutral, a faint rainbow for good, a dark red "
                               "for evil. Off keeps the painted night sky of the game.",
                           MOA_TOGGLE(sky.alignmentTheme)},
    Field<SkyboxSettings> {.group = "Alignment sky",
                           .section = "sky",
                           .key = "useGameAlignment",
                           .label = "Use the game's alignment",
                           .help = "The sky, the stars and the aurora follow the alignment of the player. Off: they follow the "
                                   "slider below. The slider never changes the alignment in the game or the save.",
                           MOA_TOGGLE(sky.useGameAlignment)},
    Field<SkyboxSettings> {.group = "Alignment sky",
                           .section = "sky",
                           .key = "alignment",
                           .label = "Sky alignment",
                           .help =
                               "The alignment of the sky when the box above is off: -1 fully evil, 0 neutral, +1 fully good. "
                               "For tests and for a sky that you like.",
                           .kind = FieldKind::Number,
                           .min = -1.0,
                           .max = 1.0,
                           .step = 0.05,
                           .format = "%+.2f",
                           MOA_NUMBER(sky.alignment)},
    Field<SkyboxSettings> {.group = "Alignment sky",
                           .section = "sky",
                           .key = "goodStrength",
                           .label = "Good strength",
                           .help = "Multiplies the colours of the good sky and of its stars.",
                           .kind = FieldKind::Number,
                           .min = 0.0,
                           .max = 2.0,
                           .step = 0.05,
                           .format = "%.2f",
                           MOA_NUMBER(sky.goodStrength)},
    Field<SkyboxSettings> {.group = "Alignment sky",
                           .section = "sky",
                           .key = "evilStrength",
                           .label = "Evil strength",
                           .help = "Multiplies the colours of the evil sky and of its stars.",
                           .kind = FieldKind::Number,
                           .min = 0.0,
                           .max = 2.0,
                           .step = 0.05,
                           .format = "%.2f",
                           MOA_NUMBER(sky.evilStrength)},
    Field<SkyboxSettings> {
        .group = "Evil night",
        .section = "sky",
        .key = "evilBloodMoon",
        .label = "Blood moon",
        .help = "The moon turns deep red with a dark rim and grows to 1.3 times its size for a fully evil player.",
        MOA_TOGGLE(sky.evil.bloodMoon)},
    Field<SkyboxSettings> {.group = "Evil night",
                           .section = "sky",
                           .key = "evilBloodMoonStrength",
                           .label = "Blood moon strength",
                           .help = "Multiplies the light of this effect. 1 is normal.",
                           .kind = FieldKind::Number,
                           .min = 0.0,
                           .max = 2.0,
                           .step = 0.05,
                           .format = "%.2f",
                           MOA_NUMBER(sky.evil.bloodMoonStrength)},
};

#undef MOA_TOGGLE
#undef MOA_NUMBER
} // namespace

moa::Result<void> ReadMoonSettings(const nlohmann::json& object, SkyboxSettings& settings)
{
	if (!object.is_object())
	{
		return moa::MakeError("the object 'moon' must be a JSON object");
	}
	auto value = settings.moon;
	if (!ReadField(object, "enabled", value.enabled) || !ReadField(object, "latitude", value.latitude) ||
	    !ReadField(object, "longitude", value.longitude) || !ReadField(object, "northDegrees", value.northDegrees) ||
	    !ReadField(object, "followRealPosition", value.followRealPosition) ||
	    !ReadField(object, "phaseOnly", value.phaseOnly) || !ReadField(object, "lightScale", value.lightScale))
	{
		return moa::MakeError("the object 'moon' has a field with a wrong type");
	}
	if (value.latitude < -90.0 || value.latitude > 90.0 || value.longitude < -180.0 || value.longitude > 180.0)
	{
		return moa::MakeError("the latitude must be -90 to 90 and the longitude -180 to 180");
	}
	if (value.northDegrees < -360.0 || value.northDegrees > 360.0 || value.lightScale < 0.0 || value.lightScale > 10.0)
	{
		return moa::MakeError("'northDegrees' must be -360 to 360 and 'lightScale' 0 to 10");
	}
	settings.moon = value;
	return {};
}

moa::Result<void> ReadSkySettings(const nlohmann::json& object, SkyboxSettings& settings)
{
	if (!object.is_object())
	{
		return moa::MakeError("the object 'sky' must be a JSON object");
	}
	auto value = settings.sky;
	if (!ReadField(object, "enabled", value.enabled) || !ReadField(object, "testQuad", value.testQuad) ||
	    !ReadField(object, "stars", value.stars) || !ReadField(object, "starBrightness", value.starBrightness) ||
	    !ReadField(object, "moonShader", value.moonShader) || !ReadField(object, "alignmentTheme", value.alignmentTheme) ||
	    !ReadField(object, "goodStrength", value.goodStrength) || !ReadField(object, "evilStrength", value.evilStrength) ||
	    !ReadField(object, "useGameAlignment", value.useGameAlignment) || !ReadField(object, "alignment", value.alignment))
	{
		return moa::MakeError("the object 'sky' has a field with a wrong type");
	}
	if (value.starBrightness < 0.0 || value.starBrightness > 5.0)
	{
		return moa::MakeError("'starBrightness' must be 0 to 5");
	}
	if (value.goodStrength < 0.0 || value.goodStrength > 2.0 || value.evilStrength < 0.0 || value.evilStrength > 2.0)
	{
		return moa::MakeError("'goodStrength' and 'evilStrength' must be 0 to 2");
	}
	if (value.alignment < -1.0 || value.alignment > 1.0)
	{
		return moa::MakeError("'alignment' must be -1 to 1");
	}
	if (auto error = ReadEvilNight(object, value.evil))
	{
		return *error;
	}
	if (const auto mode = object.find("aurora"); mode != object.end())
	{
		const auto parsed = mode->is_string() ? sky::ParseAuroraMode(mode->get<std::string>()) : std::nullopt;
		if (!parsed)
		{
			return moa::MakeError("'aurora' must be \"off\", \"alignment\" or \"always\"");
		}
		value.aurora = *parsed;
	}
	settings.sky = value;
	return {};
}

moa::Result<SkyboxSettings> ParseSkyboxSettings(std::string_view json)
{
	const auto root = nlohmann::json::parse(json, nullptr, false);
	if (root.is_discarded() || !root.is_object())
	{
		return moa::MakeError("the mod settings are not a JSON object");
	}
	SkyboxSettings settings;
	if (const auto moon = root.find("moon"); moon != root.end())
	{
		if (auto read = ReadMoonSettings(*moon, settings); !read)
		{
			return read.GetError();
		}
	}
	if (const auto sky = root.find("sky"); sky != root.end())
	{
		if (auto read = ReadSkySettings(*sky, settings); !read)
		{
			return read.GetError();
		}
	}
	return settings;
}

nlohmann::json SkyboxSettingsJson(const SkyboxSettings& settings)
{
	return {
	    {"moon",
	     {
	         {"enabled", settings.moon.enabled},
	         {"latitude", settings.moon.latitude},
	         {"longitude", settings.moon.longitude},
	         {"northDegrees", settings.moon.northDegrees},
	         {"followRealPosition", settings.moon.followRealPosition},
	         {"phaseOnly", settings.moon.phaseOnly},
	         {"lightScale", settings.moon.lightScale},
	     }},
	    {"sky",
	     {
	         {"enabled", settings.sky.enabled},
	         {"stars", settings.sky.stars},
	         {"starBrightness", settings.sky.starBrightness},
	         {"aurora", sky::AuroraModeName(settings.sky.aurora)},
	         {"moonShader", settings.sky.moonShader},
	         {"alignmentTheme", settings.sky.alignmentTheme},
	         {"useGameAlignment", settings.sky.useGameAlignment},
	         {"alignment", settings.sky.alignment},
	         {"goodStrength", settings.sky.goodStrength},
	         {"evilStrength", settings.sky.evilStrength},
	         {"evilBloodMoon", settings.sky.evil.bloodMoon},
	         {"evilBloodMoonStrength", settings.sky.evil.bloodMoonStrength},
	         {"testQuad", settings.sky.testQuad},
	     }},
	};
}

SkyboxSettingsChange CompareSkyboxSettings(const SkyboxSettings& current, const SkyboxSettings& next)
{
	SkyboxSettingsChange change;
	change.moonEnabled = current.moon.enabled != next.moon.enabled;
	auto moon = next.moon;
	moon.enabled = current.moon.enabled;
	change.moon = moon != current.moon;
	if (current.sky.enabled != next.sky.enabled)
	{
		change.restartOnly.emplace_back("sky.enabled");
	}
	if (current.sky.testQuad != next.sky.testQuad)
	{
		change.restartOnly.emplace_back("sky.testQuad");
	}
	auto sky = next.sky;
	sky.enabled = current.sky.enabled;
	sky.testQuad = current.sky.testQuad;
	change.sky = sky != current.sky;
	return change;
}

SkyboxSettings LiveSkyboxSettings(const SkyboxSettings& current, const SkyboxSettings& next)
{
	auto live = next;
	live.sky.enabled = current.sky.enabled;
	live.sky.testQuad = current.sky.testQuad;
	return live;
}

settings::SettingsEdit ToggleMoon(SkyboxSettings& settings)
{
	settings.moon.enabled = !settings.moon.enabled;
	return settings::SettingsEdit {std::format("Moon mod: {}", settings.moon.enabled ? "on" : "off"),
	                               {{"moon", {{"enabled", settings.moon.enabled}}}}};
}

std::span<const settings::Field<SkyboxSettings>> SkyboxFields()
{
	return k_Fields;
}

moa::Result<settings::SettingsEdit> EditSkyboxField(SkyboxSettings& current, const settings::Field<SkyboxSettings>& field,
                                                    const settings::FieldValue& value)
{
	return settings::EditField(current, field, value, settings::FieldRange {.min = field.min, .max = field.max},
	                           [](const SkyboxSettings& next) -> moa::Result<void> {
		                           if (auto checked = ParseSkyboxSettings(SkyboxSettingsJson(next).dump()); !checked)
		                           {
			                           return checked.GetError();
		                           }
		                           return {};
	                           });
}

std::string_view PhaseName(double cycleFraction)
{
	constexpr std::array<std::string_view, 8> k_Names {
	    "New moon",  "Waxing crescent", "First quarter", "Waxing gibbous",
	    "Full moon", "Waning gibbous",  "Last quarter",  "Waning crescent",
	};
	auto fraction = cycleFraction - std::floor(cycleFraction);
	// Each name covers one eighth of the month, with the exact phase at its centre.
	const auto index = static_cast<size_t>(std::floor(fraction * 8.0 + 0.5)) % k_Names.size();
	return k_Names[index];
}

MoonReadout ReadMoon(std::chrono::system_clock::time_point utc, const SkyboxSettings::Moon& settings)
{
	const auto state = moa::astro::ComputeMoon(utc);
	const auto horizontal = moa::astro::MoonHorizontal(
	    state, moa::astro::Observer {.latitude = settings.latitude, .longitude = settings.longitude});
	return {
	    .phaseName = PhaseName(state.phase.cycleFraction),
	    .illuminatedPercent = state.phase.illuminatedFraction * 100.0,
	    .ageDays = state.phase.ageDays,
	    .altitudeDegrees = horizontal.altitude,
	    .azimuthDegrees = horizontal.azimuth,
	    .waxing = state.phase.waxing,
	};
}

} // namespace moa_mod
