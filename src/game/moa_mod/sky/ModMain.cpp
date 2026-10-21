/******************************************************************************
 * Copyright (c) 2026 Masters of Albion Reforged developers
 *****************************************************************************/

// The entry points of the better skybox mod DLL. The mod loader (version.dll) loads the DLL from the folder `mods` of
// the game and calls these functions. The mod reads the objects `moon` and `sky` of mods.json, adds its fields and its
// readouts to the menu bar, and runs after `Marcos_Procedural_Sky.Update`: the real moon, and the night sky of the
// AssetBundle in this DLL (the stars, the aurora, the alignment sky and the blood moon).

#include <algorithm>
#include <chrono>
#include <exception>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include <moa_mod/api/ModApi.h>
#include <moa_mod/sdk/HostLogSink.h>
#include <moa_mod/sdk/ModHost.h>
#include <moa_mod/sky/SkyRules.h>
#include <moa_mod/sky/SkyboxSettings.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "moa_mod/Il2Cpp.h"
#include "moa_mod/Managed.h"
#include "moa_mod/ModStatus.h"
#include "moa_mod/moon/MoonMod.h"
#include "moa_mod/sky/SkyMod.h"

namespace moa_mod::sky
{

namespace
{
constexpr const char* k_ModVersion = "0.1.0";

const sdk::MethodRef k_SkyUpdate {.space = "", .className = "Marcos_Procedural_Sky", .method = "Update", .parameterTypes = {}};

[[nodiscard]] std::filesystem::path PathOf(const std::string& utf8)
{
	return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

/// The state of the sky mod. It lives from `MoaModInit` to `MoaModShutdown`.
struct SkyModState
{
	std::unique_ptr<sdk::ModHost> host;
	std::shared_ptr<sdk::HostLogSink> sink;
	Il2CppApi api;
	std::optional<Managed> managed;
	std::optional<moon::SkyBindings> moonBindings;
	ModStatus moonStatus {"moon"};
	ModStatus skyStatus {"sky"};
	/// The settings that the user asked for, and the settings that the mod uses now. `sky.enabled` and `sky.testQuad`
	/// work only after a restart of the game.
	SkyboxSettings requested;
	SkyboxSettings live;
	/// The objects `moon` and `sky` came one time each. The parts of the mod start when both came.
	bool moonLoaded = false;
	bool skyLoaded = false;
	/// The parts of the mod exist.
	bool started = false;
	std::unique_ptr<moon::MoonMod> moon;
	std::unique_ptr<SkyMod> sky;
	/// The last object of `Marcos_Procedural_Sky` that the hook saw, to give the moon back to the game.
	void* skyObject = nullptr;
};

std::unique_ptr<SkyModState> s_state;

/// The place of the observer of the sky: the place of the moon settings.
[[nodiscard]] SkyPlace PlaceOf(const SkyboxSettings::Moon& moon)
{
	return {.latitude = moon.latitude, .longitude = moon.longitude, .northDegrees = moon.northDegrees};
}

/// Make the parts of the mod with the first settings. The loader gives the first settings before the first frame.
void Start(SkyModState& state)
{
	state.started = true;
	state.moon = std::make_unique<moon::MoonMod>(*state.managed, *state.moonBindings, state.live.moon, state.moonStatus);
	state.moonStatus.SetReady(true);
	spdlog::info("moon mod: {}", state.live.moon.enabled ? "on" : "ready, off");
	state.skyStatus.SetEnabled(state.live.sky.enabled);
	if (!state.live.sky.enabled)
	{
		spdlog::info("sky mod: off; a change of sky.enabled works after a restart of the game");
		return;
	}
	auto bindings = SkyModBindings::Bind(*state.managed);
	if (!bindings)
	{
		spdlog::error("sky mod: {}; the sky mod is off", bindings.GetError().message);
		state.skyStatus.SetError(bindings.GetError().message);
		return;
	}
	// The bundle file goes next to mods.json, outside the game folder.
	const auto bundleFile = PathOf(state.host->SettingsFolder()) / std::string(k_BundleFileName);
	state.sky = std::make_unique<SkyMod>(*state.managed, *bindings, state.live.sky, PlaceOf(state.live.moon), bundleFile,
	                                     state.skyStatus);
	state.skyStatus.SetReady(true);
	spdlog::info("sky mod: on, the bundle goes to {}", bundleFile.string());
}

/// Use new settings. After the first settings, the calls run on the main thread, between two frames. A part that the
/// new settings stop sets the values that it changed to the values of the game.
void Apply(SkyModState& state, const SkyboxSettings& next)
{
	const auto previous = state.live;
	const auto change = CompareSkyboxSettings(previous, next);
	for (const auto& name : change.restartOnly)
	{
		spdlog::warn("sky mod: '{}' changed; the change works after a restart of the game", name);
	}
	state.requested = next;
	state.live = state.started ? LiveSkyboxSettings(previous, next) : next;
	state.moonStatus.SetEnabled(state.live.moon.enabled);
	if (!state.started)
	{
		if (state.moonLoaded && state.skyLoaded)
		{
			Start(state);
		}
		return;
	}
	if (change.moonEnabled || change.moon)
	{
		if (previous.moon.enabled && state.skyObject != nullptr)
		{
			state.moon->Restore(state.skyObject);
		}
		// A new moon mod reads the values of the game again before it changes them.
		state.moon = std::make_unique<moon::MoonMod>(*state.managed, *state.moonBindings, state.live.moon, state.moonStatus);
		if (!state.live.moon.enabled)
		{
			state.moonStatus.SetActive(false, "the settings turned the moon mod off");
		}
	}
	if ((change.sky || change.moon) && state.sky)
	{
		// The sky uses the place of the moon settings: the stars and the moon are on one sky.
		state.sky->ApplySettings(state.live.sky, PlaceOf(state.live.moon));
	}
}

/// Read one object of mods.json into a copy of the requested settings and use it.
moa::Result<void> Load(SkyModState& state, const nlohmann::json& object, bool moonObject)
{
	auto next = state.requested;
	auto read = moonObject ? ReadMoonSettings(object, next) : ReadSkySettings(object, next);
	(moonObject ? state.moonLoaded : state.skyLoaded) = true;
	if (!read)
	{
		// The first settings start the mod also when one object is not correct: that object keeps its defaults.
		if (!state.started)
		{
			Apply(state, state.requested);
		}
		return read;
	}
	Apply(state, next);
	return {};
}

[[nodiscard]] std::string MoonText(const SkyModState& state)
{
	const auto moon = ReadMoon(std::chrono::system_clock::now(), state.requested.moon);
	auto text = std::format("Phase: {} ({})\nLit part of the disc: {:.1f} %\nAge: {:.1f} days\n"
	                        "Altitude: {:.1f} deg, azimuth {:.1f} deg",
	                        moon.phaseName, moon.waxing ? "waxing" : "waning", moon.illuminatedPercent, moon.ageDays,
	                        moon.altitudeDegrees, moon.azimuthDegrees);
	if (!state.requested.moon.enabled)
	{
		text += "\nThe moon mod is off: the game shows its own moon.";
	}
	return text;
}

[[nodiscard]] std::string AlignmentText(const SkyModState& state)
{
	if (!state.sky)
	{
		return "Game alignment: the sky mod is off";
	}
	const auto theme = state.sky->ThemeState();
	std::string text = theme.value("read", false)
	                       ? std::format("Game alignment: {:+.2f} (neutral from {:+.2f} to {:+.2f})", theme.value("value", 0.0),
	                                     theme.value("evilLimit", 0.0), theme.value("goodLimit", 0.0))
	                       : std::string("Game alignment: not known now");
	return text + std::format("\nSky alignment now: {:+.2f}", theme.value("theme", 0.0));
}

/// The sky part of the mod, for a query that changes it. Throw an error if the sky part is off.
[[nodiscard]] SkyMod& SkyOf(SkyModState& state)
{
	if (!state.sky)
	{
		throw std::runtime_error("the sky mod is off");
	}
	return *state.sky;
}

/// Register the queries of the inspector. The loader calls them on the main thread. A refused query does not stop the
/// mod: the log tells it.
void RegisterQueries(SkyModState& state)
{
	auto& host = *state.host;
	if (!host.HasQueries())
	{
		spdlog::info("sky mod: the loader has no inspector queries");
		return;
	}
	bool ok = host.AddQuery("sky.state", "The state of the night sky, the theme and the evil night.",
	                        [&state](const nlohmann::json& /*params*/) {
		                        if (!state.sky)
		                        {
			                        return nlohmann::json {{"installed", false}, {"status", state.skyStatus.Describe()}};
		                        }
		                        auto result = state.sky->State();
		                        result["status"] = state.skyStatus.Describe();
		                        return result;
	                        });
	ok = host.AddQuery("sky.show", "Show the test quad also in the day. Parameter: on (true or false). For tests only.",
	                   [&state](const nlohmann::json& params) {
		                   const auto on = params.value("on", true);
		                   SkyOf(state).ForceShow(on);
		                   return nlohmann::json {{"forceShow", on}};
	                   }) &&
	     ok;
	ok = host.AddQuery("sky.skybox", "Use the skybox of the mod (on true) or the skybox of the game (on false). Parameter: on.",
	                   [&state](const nlohmann::json& params) {
		                   auto result = SkyOf(state).SkyboxTest(params.value("on", false));
		                   if (!result)
		                   {
			                   throw std::runtime_error(result.GetError().message);
		                   }
		                   return nlohmann::json {{"skyboxSwapped", *result}};
	                   }) &&
	     ok;
	ok = host.AddQuery("sky.debug",
	                   "A test mode of the moon shader (moon, 0 to 5), a scale of the moon brightness (moonBrightness, 0 "
	                   "to 4) and a restart of the moon renderer (restartRenderer). For tests only.",
	                   [&state](const nlohmann::json& params) {
		                   auto& sky = SkyOf(state);
		                   const auto mode = std::clamp(params.value("moon", 0), 0, 5);
		                   const auto restart = params.value("restartRenderer", false);
		                   const auto brightness = std::clamp(params.value("moonBrightness", 1.0), 0.0, 4.0);
		                   sky.SetMoonDebug(mode);
		                   sky.SetMoonBrightnessScale(static_cast<float>(brightness));
		                   if (restart)
		                   {
			                   sky.RestartMoonRenderer();
		                   }
		                   return nlohmann::json {
		                       {"moonDebug", mode}, {"restartRenderer", restart}, {"moonBrightness", brightness}};
	                   }) &&
	     ok;
	ok = host.AddQuery("sky.theme",
	                   "A test theme of the sky (theme, -1 to 1, or null for the theme of the settings), and snap to "
	                   "show it at once. For tests only.",
	                   [&state](const nlohmann::json& params) {
		                   auto& sky = SkyOf(state);
		                   std::optional<double> theme;
		                   if (const auto value = params.find("theme"); value != params.end() && value->is_number())
		                   {
			                   theme = std::clamp(value->get<double>(), -1.0, 1.0);
		                   }
		                   const auto snap = params.value("snap", false);
		                   sky.SetThemeTest(theme, snap);
		                   return nlohmann::json {{"theme", theme ? nlohmann::json(*theme) : nlohmann::json()}, {"snap", snap}};
	                   }) &&
	     ok;
	ok = host.AddQuery("moon.state", "The sky of the game, the moon that the mod calculates, and the state of the moon mod.",
	                   [&state](const nlohmann::json& /*params*/) -> nlohmann::json {
		                   auto sky = state.managed->SingletonInstance("Marcos_Procedural_Sky");
		                   if (!sky)
		                   {
			                   return {{"error", sky.GetError().message}};
		                   }
		                   if (state.moon)
		                   {
			                   return state.moon->State(*sky);
		                   }
		                   // The moon mod did not start: show the sky and the moon that the mod calculates, and change
		                   // nothing.
		                   ModStatus status("moon preview");
		                   moon::MoonMod preview(*state.managed, *state.moonBindings, state.live.moon, status);
		                   return preview.State(*sky);
	                   }) &&
	     ok;
	if (!ok)
	{
		spdlog::warn("sky mod: the loader refused a query; see the log of the loader");
	}
}

/// Register the settings, the fields, the hotkey, the readouts, the status and the method callback.
bool Register(SkyModState& state)
{
	auto& host = *state.host;
	bool ok = host.AddSettings("moon", [&state](const nlohmann::json& object) { return Load(state, object, true); });
	ok = host.AddSettings("sky", [&state](const nlohmann::json& object) { return Load(state, object, false); }) && ok;
	const auto refused = sdk::BindFields<SkyboxSettings>(
	    host, SkyboxFields(), "Sky",
	    {.requested = [&state]() -> const SkyboxSettings& { return state.requested; },
	     .live = [&state]() -> const SkyboxSettings& { return state.live; },
	     .range = [](const settings::Field<SkyboxSettings>& field,
	                 const SkyboxSettings& /*settings*/) { return settings::FieldRange {.min = field.min, .max = field.max}; },
	     .edit = [&state](const settings::Field<SkyboxSettings>& field, const settings::FieldValue& value,
	                      bool /*save*/) -> moa::Result<std::string> {
		     auto next = state.requested;
		     auto edit = EditSkyboxField(next, field, value);
		     if (!edit)
		     {
			     return edit.GetError();
		     }
		     Apply(state, next);
		     return edit->message;
	     }});
	ok = ok && refused == 0;
	const MoaActionDesc toggle {.menu = "Sky",
	                            .group = "Moon",
	                            .label = nullptr,
	                            .help = nullptr,
	                            .hotkey = "toggleMoon",
	                            .defaultHotkey = "Ctrl+Shift+F6"};
	ok = host.AddAction(toggle, [&state] {
		if (!state.moon)
		{
			state.host->ShowMessage("Moon mod: not ready (see the log)", 2.0);
			return;
		}
		auto next = state.requested;
		const auto edit = ToggleMoon(next);
		Apply(state, next);
		state.host->ShowMessage(edit.message, 2.0);
		(void)state.host->SaveSettings(edit.patch);
	}) && ok;
	ok = host.AddReadout("Sky", "Alignment", [&state] { return AlignmentText(state); }) && ok;
	ok = host.AddReadout("Sky", "The moon now", [&state] { return MoonText(state); }) && ok;
	ok = host.SetStatusReader([&state] {
		auto status = state.moonStatus.Describe();
		status["sky"] = state.skyStatus.Describe();
		return status;
	}) && ok;
	// The moon first, then the sky: the sky reads the moon of the frame.
	ok = host.AddMethodCallback(k_SkyUpdate, MOA_HOOK_AFTER, "moon", [&state](const MoaCall& call) {
		state.skyObject = call.instance;
		if (state.live.moon.enabled && state.moon)
		{
			state.moon->After(call.instance);
		}
	}) && ok;
	ok = host.AddMethodCallback(k_SkyUpdate, MOA_HOOK_AFTER, "sky", [&state](const MoaCall& call) {
		if (state.sky)
		{
			state.sky->After(call.instance);
		}
	}) && ok;
	return ok;
}

uint32_t Init(const MoaHostApi* api, uint32_t apiVersion)
{
	if (auto checked = sdk::ModHost::Check(api, apiVersion); !checked)
	{
		if (api != nullptr && api->log != nullptr)
		{
			api->log(api->context, MOA_LOG_ERROR, checked.GetError().message.c_str());
		}
		return 0;
	}
	auto state = std::make_unique<SkyModState>();
	state->sink = sdk::UseHostLog(*api, "better_skybox");
	state->host = std::make_unique<sdk::ModHost>(*api, "better_skybox");
	auto il2cpp = Il2CppApi::Load();
	if (!il2cpp)
	{
		spdlog::error("sky mod: {}", il2cpp.GetError().message);
		return 0;
	}
	state->api = std::move(*il2cpp);
	auto managed = Managed::Load(state->api);
	if (!managed)
	{
		spdlog::error("sky mod: {}", managed.GetError().message);
		return 0;
	}
	state->managed = *managed;
	auto bindings = moon::SkyBindings::Bind(*state->managed);
	if (!bindings)
	{
		spdlog::error("moon mod: {}; the sky mod does not start", bindings.GetError().message);
		return 0;
	}
	state->moonBindings = *bindings;
	if (EmbeddedBundle().empty())
	{
		spdlog::warn("sky mod: this DLL has no AssetBundle; the night sky stays off");
	}
	if (!Register(*state))
	{
		spdlog::error("sky mod: the loader refused a registration; the sky mod does not start");
		return 0;
	}
	RegisterQueries(*state);
	s_state = std::move(state);
	return 1;
}

void Shutdown()
{
	if (!s_state)
	{
		return;
	}
	auto& state = *s_state;
	// The game gets its skybox and its moon material back.
	if (state.sky)
	{
		state.sky->StandBack();
	}
	spdlog::info("sky mod: stopped");
	state.sink->Detach();
}
} // namespace

} // namespace moa_mod::sky

extern "C" {
__declspec(dllexport) uint32_t MoaModApiVersion()
{
	return MOA_MOD_API_VERSION;
}

__declspec(dllexport) const MoaModInfo* MoaModGetInfo()
{
	static const MoaModInfo info {.id = "better_skybox", .name = "Sky", .version = moa_mod::sky::k_ModVersion};
	return &info;
}

__declspec(dllexport) uint32_t MoaModInit(const MoaHostApi* host, uint32_t apiVersion)
{
	// No C++ exception may leave the mod into the loader.
	try
	{
		return moa_mod::sky::Init(host, apiVersion);
	}
	catch (const std::exception& error)
	{
		spdlog::error("sky mod: the init stopped: {}", error.what());
	}
	catch (...)
	{
		spdlog::error("sky mod: the init stopped with an unknown exception");
	}
	return 0;
}

__declspec(dllexport) void MoaModShutdown()
{
	try
	{
		moa_mod::sky::Shutdown();
	}
	catch (...)
	{
		// The game quits. Nothing more can go to the log.
	}
}
}
