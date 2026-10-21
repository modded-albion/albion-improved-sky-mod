/******************************************************************************
 * Copyright (c) 2026 Masters of Albion Reforged developers
 *****************************************************************************/

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iterator>
#include <set>
#include <span>
#include <string>
#include <vector>

#include <astro/StarTable.h>
#include <core/SkyboxSettings.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using namespace moa_mod;
using settings::FieldKind;
using settings::FieldValue;

namespace
{

/// Write one value into the JSON of the settings at the path of the field.
nlohmann::json WithValue(const SkyboxSettings& settings, const settings::Field<SkyboxSettings>& field,
                         const nlohmann::json& value)
{
	auto json = SkyboxSettingsJson(settings);
	json[std::string(field.section)][std::string(field.key)] = value;
	return json;
}

} // namespace

TEST(SkyboxSettings, KeepsTheMoonAndTheSkyOffByDefault)
{
	const auto settings = ParseSkyboxSettings(R"({"camera": {"enabled": true}})");
	ASSERT_TRUE(settings);
	EXPECT_FALSE(settings->moon.enabled);
	EXPECT_FALSE(settings->sky.enabled);
	EXPECT_EQ(*settings, SkyboxSettings {});
}

TEST(SkyboxSettings, ReadsTheMoon)
{
	const auto settings = ParseSkyboxSettings(R"({"moon": {"enabled": true, "latitude": 55.95, "longitude": -3.19,
		"northDegrees": 30, "followRealPosition": false, "phaseOnly": true, "lightScale": 2}})");
	ASSERT_TRUE(settings) << settings.GetError().message;
	EXPECT_TRUE(settings->moon.enabled);
	EXPECT_DOUBLE_EQ(settings->moon.latitude, 55.95);
	EXPECT_DOUBLE_EQ(settings->moon.longitude, -3.19);
	EXPECT_DOUBLE_EQ(settings->moon.northDegrees, 30.0);
	EXPECT_FALSE(settings->moon.followRealPosition);
	EXPECT_TRUE(settings->moon.phaseOnly);
	EXPECT_DOUBLE_EQ(settings->moon.lightScale, 2.0);
	for (const auto* text :
	     {R"({"moon": {"latitude": "north"}})", R"({"moon": {"latitude": 91}})", R"({"moon": {"lightScale": -1}})",
	      R"({"moon": {"phaseOnly": 1}})", R"({"moon": 3})", R"({"sky": [1]})", "[]", "not json"})
	{
		EXPECT_FALSE(ParseSkyboxSettings(text)) << text;
	}
}

TEST(SkyboxSettings, ASectionThatIsNotCorrectKeepsTheSettings)
{
	SkyboxSettings settings;
	settings.moon.enabled = true;
	EXPECT_FALSE(ReadMoonSettings(nlohmann::json::parse(R"({"latitude": 100})"), settings));
	EXPECT_TRUE(settings.moon.enabled);
	EXPECT_DOUBLE_EQ(settings.moon.latitude, 51.5);
	ASSERT_TRUE(ReadSkySettings(nlohmann::json::parse(R"({"enabled": true})"), settings));
	EXPECT_TRUE(settings.sky.enabled);
}

TEST(SkyboxSettings, TheMoonHotkeyAndTheChanges)
{
	SkyboxSettings settings;
	const auto edit = ToggleMoon(settings);
	EXPECT_TRUE(settings.moon.enabled);
	EXPECT_EQ(edit.message, "Moon mod: on");
	EXPECT_EQ(edit.patch, (nlohmann::json {{"moon", {{"enabled", true}}}}));
	auto next = settings;
	next.moon.lightScale = 2.0;
	const auto change = CompareSkyboxSettings(settings, next);
	EXPECT_FALSE(change.moonEnabled);
	EXPECT_TRUE(change.moon);
	EXPECT_FALSE(change.sky);
	const auto read = ParseSkyboxSettings(SkyboxSettingsJson(next).dump());
	ASSERT_TRUE(read) << read.GetError().message;
	EXPECT_EQ(*read, next);
}

TEST(SkyboxFields, EachFieldIsInTheJsonOneTimeAndItsRangeAgreesWithTheFile)
{
	const SkyboxSettings defaults;
	const auto json = SkyboxSettingsJson(defaults);
	std::set<std::string> paths;
	std::set<std::string> restart;
	for (const auto& field : SkyboxFields())
	{
		const auto path = settings::FieldPath(field);
		EXPECT_TRUE(paths.insert(path).second) << path;
		EXPECT_TRUE(json.at(std::string(field.section)).contains(std::string(field.key))) << path;
		EXPECT_FALSE(field.label.empty()) << path;
		EXPECT_FALSE(field.help.empty()) << path;
		if (field.restartOnly)
		{
			restart.insert(path);
		}
		if (field.kind != FieldKind::Number)
		{
			continue;
		}
		EXPECT_TRUE(ParseSkyboxSettings(WithValue(defaults, field, field.min).dump())) << path << " min";
		EXPECT_TRUE(ParseSkyboxSettings(WithValue(defaults, field, field.max).dump())) << path << " max";
		EXPECT_FALSE(ParseSkyboxSettings(WithValue(defaults, field, field.min - field.step).dump())) << path << " below";
		EXPECT_FALSE(ParseSkyboxSettings(WithValue(defaults, field, field.max + field.step).dump())) << path << " above";
	}
	for (const auto* path : {"moon.enabled", "moon.latitude", "moon.longitude", "moon.northDegrees", "moon.followRealPosition",
	                         "moon.phaseOnly", "moon.lightScale", "sky.enabled", "sky.aurora", "sky.evilBloodMoon"})
	{
		EXPECT_TRUE(paths.contains(path)) << path;
	}
	// The same fields are the fields that the live reload keeps until a restart.
	EXPECT_EQ(restart, (std::set<std::string> {"sky.enabled"}));
}

TEST(SkyboxFields, EditMakesAPatchOfOneField)
{
	SkyboxSettings settings;
	const auto* latitude = settings::FindField<SkyboxSettings>(SkyboxFields(), "moon", "latitude");
	ASSERT_NE(latitude, nullptr);
	// A number out of the range stops at the end of the range.
	auto edit = EditSkyboxField(settings, *latitude, FieldValue {120.0});
	ASSERT_TRUE(edit) << edit.GetError().message;
	EXPECT_DOUBLE_EQ(settings.moon.latitude, 90.0);
	EXPECT_EQ(edit->patch, (nlohmann::json {{"moon", {{"latitude", 90.0}}}}));
	const auto* aurora = settings::FindField<SkyboxSettings>(SkyboxFields(), "sky", "aurora");
	ASSERT_NE(aurora, nullptr);
	edit = EditSkyboxField(settings, *aurora, FieldValue {size_t {2}});
	ASSERT_TRUE(edit) << edit.GetError().message;
	EXPECT_EQ(settings.sky.aurora, sky::AuroraMode::Always);
	EXPECT_EQ(edit->patch, (nlohmann::json {{"sky", {{"aurora", "always"}}}}));
	EXPECT_FALSE(EditSkyboxField(settings, *aurora, FieldValue {true}));
}

TEST(MoonReadout, NamesThePhases)
{
	EXPECT_EQ(PhaseName(0.0), "New moon");
	EXPECT_EQ(PhaseName(0.99), "New moon");
	EXPECT_EQ(PhaseName(0.1), "Waxing crescent");
	EXPECT_EQ(PhaseName(0.25), "First quarter");
	EXPECT_EQ(PhaseName(0.4), "Waxing gibbous");
	EXPECT_EQ(PhaseName(0.5), "Full moon");
	EXPECT_EQ(PhaseName(0.6), "Waning gibbous");
	EXPECT_EQ(PhaseName(0.75), "Last quarter");
	EXPECT_EQ(PhaseName(0.9), "Waning crescent");
	EXPECT_EQ(PhaseName(1.25), "First quarter");
}

TEST(MoonReadout, ReadsAFullMoon)
{
	// A full moon of 2024: 17 October 2024 at 11:26 UTC.
	using namespace std::chrono;
	const auto time = sys_days {year {2024} / October / 17} + hours {11} + minutes {26};
	const auto readout = ReadMoon(time, SkyboxSettings::Moon {});
	EXPECT_EQ(readout.phaseName, "Full moon");
	EXPECT_GT(readout.illuminatedPercent, 99.0);
	EXPECT_GE(readout.altitudeDegrees, -90.0);
	EXPECT_LE(readout.altitudeDegrees, 90.0);
}

TEST(StarTable, ReadsTheStarTableOfTheSkyMod)
{
	std::ifstream file(MOA_STAR_TABLE, std::ios::binary);
	if (!file)
	{
		GTEST_SKIP() << "no star table at " << MOA_STAR_TABLE;
	}
	const std::vector<char> chars {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
	const auto stars = moa::astro::ReadStarTable(std::as_bytes(std::span(chars)));
	ASSERT_TRUE(stars.has_value());
	EXPECT_GT(stars->size(), 9000u);
	// The brightest star is Sirius; the table has the brightest star first.
	EXPECT_NEAR(stars->front().position.rightAscension, 101.287, 0.01);
	EXPECT_FLOAT_EQ(stars->front().magnitude, -1.46f);
	EXPECT_TRUE(std::ranges::is_sorted(*stars, {}, &moa::astro::CatalogueStar::magnitude));
}
