/******************************************************************************
 * Copyright (c) 2026 Masters of Albion Reforged developers
 *****************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include <moa/common/Result.h>
#include <moa_mod/sky/EvilNight.h>
#include <moa_mod/sky/SkyRules.h>
#include <moa_mod/sky/SkyUniforms.h>
#include <moa_mod/sky/SkyboxSettings.h>
#include <nlohmann/json.hpp>

#include "moa_mod/Managed.h"
#include "moa_mod/ModStatus.h"

namespace moa_mod::sky
{

/// The bytes of the AssetBundle of the sky mod that the build put into the mod. It is empty if the build did not
/// find the bundle file.
[[nodiscard]] std::span<const std::byte> EmbeddedBundle();

/// The names of the game and of Unity that the sky mod uses.
struct SkyModBindings
{
	// Marcos_Procedural_Sky
	size_t dayNightCycle = 0;
	size_t moonObject = 0;
	// GameManager
	const MethodInfo* isInterior = nullptr;
	const MethodInfo* isDesignTable = nullptr;
	const MethodInfo* isTitleScreen = nullptr;
	const MethodInfo* is3DTitleScreen = nullptr;
	const MethodInfo* isCavelike = nullptr;
	// Unity
	const MethodInfo* loadFromFileAsync = nullptr;
	const MethodInfo* isDone = nullptr;
	const MethodInfo* requestBundle = nullptr;
	const MethodInfo* loadAsset = nullptr;
	const MethodInfo* unloadBundle = nullptr;
	Il2CppObject* materialType = nullptr;
	Il2CppObject* rendererType = nullptr;
	const MethodInfo* getShader = nullptr;
	const MethodInfo* isSupported = nullptr;
	const MethodInfo* objectName = nullptr;
	const MethodInfo* getTexture = nullptr;
	const MethodInfo* setTexture = nullptr;
	const MethodInfo* getFloat = nullptr;
	const MethodInfo* setFloat = nullptr;
	const MethodInfo* propertyToId = nullptr;
	const MethodInfo* setGlobalVector = nullptr;
	const MethodInfo* instantiate = nullptr;
	const MethodInfo* destroy = nullptr;
	const MethodInfo* componentTransform = nullptr;
	const MethodInfo* getComponent = nullptr;
	const MethodInfo* getSharedMaterial = nullptr;
	const MethodInfo* setSharedMaterial = nullptr;
	const MethodInfo* setEnabled = nullptr;
	const MethodInfo* setActive = nullptr;
	const MethodInfo* setLocalPosition = nullptr;
	const MethodInfo* setLocalRotation = nullptr;
	const MethodInfo* getSkybox = nullptr;
	const MethodInfo* setSkybox = nullptr;
	// The alignment of the player and the depth of field. These are not necessary: without them, the sky is neutral
	// and the stars have their normal size. The text tells what is missing.
	const MethodInfo* getAlignment = nullptr;
	size_t evilLimit = 0;
	size_t goodLimit = 0;
	const MethodInfo* depthOfFieldLevel = nullptr;
	std::string alignmentError;
	// The blood moon of the evil night reads the night blend of the skybox. This is not necessary: without it, the
	// blood moon does not show.
	const MethodInfo* getFloatById = nullptr;
	std::string evilError;

	[[nodiscard]] static moa::Result<SkyModBindings> Bind(const Managed& managed);
};

/// The place of the observer: the settings `moon.latitude`, `moon.longitude` and `moon.northDegrees`. The stars,
/// the aurora and the moon use one sky.
struct SkyPlace
{
	double latitude = 0.0;
	double longitude = 0.0;
	double northDegrees = 0.0;
};

/// The sky mod. It works after `Marcos_Procedural_Sky.Update`. In the first frames it writes the AssetBundle of the
/// mod to a file outside the game folder and loads it with `AssetBundle.LoadFromFileAsync`. Then, in each frame:
///
/// - The skybox of the mod replaces the skybox of the game. It has the same property names, so the sky script of
///   the game writes its blend values into it. It adds the real stars and the aurora.
/// - The moon material of the mod replaces the material of the moon quad: a lit ball with the real phase.
/// - The mod sets the global values of the shaders: the sky matrix, the sun, the moon and the aurora.
///
/// The mod gives the game its materials back in interiors, caves, the title screen and the design table, and when
/// it stops. Each load step that fails stops the mod and writes the reason to the log. A failure of one effect
/// stops only that effect.
class SkyMod
{
public:
	SkyMod(const Managed& managed, SkyModBindings bindings, SkyboxSettings::Sky settings, SkyPlace place,
	       std::filesystem::path bundleFile, ModStatus& status);
	~SkyMod();
	SkyMod(const SkyMod&) = delete;
	SkyMod& operator=(const SkyMod&) = delete;

	void After(void* sky);

	/// Use new live settings: the effects, and the place of the observer.
	void ApplySettings(const SkyboxSettings::Sky& settings, SkyPlace place);

	/// Give the game its skybox and its moon material back, and turn the effects off. The bundle stays loaded.
	/// Call it on the main thread.
	void StandBack();

	/// Do `StandBack`, remove the test quad and unload the bundle. Call it on the main thread before the hook goes.
	void Restore();

	/// Show the test quad also in the day (`true`), or only at night (`false`). For tests only.
	void ForceShow(bool show) { _forceShow = show; }

	/// Use the skybox of the mod (`true`) or the skybox of the game (`false`), for a comparison of the two. The
	/// moon and the other effects do not change. Call it on the main thread.
	[[nodiscard]] moa::Result<bool> SkyboxTest(bool on);

	/// Set a test mode of the moon shader: 0 off, 1 a magenta quad, 2 the texture coordinates, 3 an opaque disc, 4 a
	/// magenta rectangle at the centre of the screen, 5 a magenta quad in front of all. For tests only.
	void SetMoonDebug(int mode)
	{
		_moonDebug = mode;
		_uniformsSet = false;
	}

	/// Multiply the brightness of the moon disc by a scale (1 is the normal brightness). For tests only.
	void SetMoonBrightnessScale(float scale)
	{
		_moonBrightnessScale = scale;
		_uniformsSet = false;
	}

	/// Use a test theme (-1 evil to +1 good) in the place of the theme of the settings, or the theme of the settings
	/// again (no value). With `snap`, the sky shows the theme in the next frame, with no fade. For tests only.
	void SetThemeTest(std::optional<double> theme, bool snap)
	{
		_themeTest = theme;
		_themeSnap = snap;
		_uniformsDirty = true;
	}

	/// Turn the renderer of the moon quad off and on in the next frame, so that Unity takes the renderer again with
	/// the new material. For tests only.
	void RestartMoonRenderer() { _restartMoonRenderer = true; }

	/// Describe the blood moon of the evil night, for `sky.state`.
	[[nodiscard]] nlohmann::json EvilState() const;

	/// Read the alignment of the player and the limits of the neutral band from the game. Call it on the main thread.
	[[nodiscard]] moa::Result<AlignmentReading> ReadAlignment() const;

	/// Describe the alignment of the player and the theme of the sky, for the menu bar and `sky.state`.
	[[nodiscard]] nlohmann::json ThemeState() const;

	/// Describe the state of the mod, for the query `sky.state`.
	[[nodiscard]] nlohmann::json State() const;

private:
	/// A managed object that the mod keeps between frames. A GC handle keeps the garbage collector from freeing it.
	class Held
	{
	public:
		Held() = default;
		Held(const Managed& managed, Il2CppObject* object);
		~Held();
		Held(Held&& other) noexcept;
		Held& operator=(Held&& other) noexcept;
		Held(const Held&) = delete;
		Held& operator=(const Held&) = delete;

		[[nodiscard]] Il2CppObject* Get() const { return _object; }
		void Reset();

	private:
		const Managed* _managed = nullptr;
		Il2CppObject* _object = nullptr;
		uintptr_t _handle = 0;
	};

	[[nodiscard]] moa::Result<StepResult> Step(void* sky);
	[[nodiscard]] moa::Result<StepResult> WriteFile();
	[[nodiscard]] moa::Result<StepResult> StartLoad();
	[[nodiscard]] moa::Result<StepResult> WaitForBundle();
	[[nodiscard]] moa::Result<StepResult> LoadMaterial();
	[[nodiscard]] moa::Result<StepResult> PlaceQuad(void* sky);
	[[nodiscard]] moa::Result<Il2CppObject*> LoadBundleMaterial(std::string_view name, std::string_view shaderName);
	void UpdateQuad(void* sky);

	[[nodiscard]] std::optional<std::string> StandBackReason() const;
	[[nodiscard]] moa::Result<std::string> ShaderName(Il2CppObject* material) const;
	[[nodiscard]] moa::Result<bool> UpdateSkybox();
	[[nodiscard]] moa::Result<bool> UpdateMoon(void* sky);
	void RestoreSkybox();
	void RestoreMoon();
	[[nodiscard]] moa::Result<bool> UploadUniforms(const SkyUniforms& uniforms);
	[[nodiscard]] const SkyUniforms& CurrentUniforms();
	/// Move the theme of the sky toward the alignment, and set its shader value when it changes.
	void UpdateTheme();
	[[nodiscard]] int DepthOfFieldLevel() const;
	/// Find the shader value of the blood moon and the night blend of the skybox.
	void LoadEvilNight();
	/// Calculate the blood moon for this frame and set its shader value when it changes.
	void UpdateEvilNight();
	[[nodiscard]] moa::Result<bool> UploadBloodMoon(const Vector4& value);
	void FailEffect(std::string& error, const char* effect, const moa::Error& reason);

	const Managed& _managed;
	SkyModBindings _bindings;
	SkyboxSettings::Sky _settings;
	SkyPlace _place;
	std::filesystem::path _bundleFile;
	ModStatus& _status;

	LoadStage _stage = LoadStage::WriteFile;
	std::string _error;
	bool _fileWritten = false;
	bool _shaderSupported = false;
	int _waitFrames = 0;
	Held _request;
	Held _bundle;
	Held _material;
	Held _quad;
	Held _quadRenderer;
	std::optional<bool> _quadShown;
	bool _forceShow = false;
	/// The sky that the test quad belongs to, and how many times the mod made the quad.
	void* _quadSky = nullptr;
	int _quadCount = 0;

	/// The skybox: the material of the mod, the material of the game that it replaces, and the test switch.
	Held _skyboxMaterial;
	Held _gameSkybox;
	bool _skyboxSwapped = false;
	bool _skyboxTestOff = false;
	std::string _skyboxError;
	int _skyboxSwaps = 0;

	/// The moon: the material of the mod, the renderer of the moon quad and the material of the game.
	Held _moonMaterial;
	Held _moonRenderer;
	Held _gameMoonMaterial;
	std::string _moonError;
	int _moonSwaps = 0;

	/// The global shader values: their property identifiers, the last values and when the mod calculated them.
	std::optional<std::array<int32_t, k_SkyUniformCount>> _uniformIds;
	std::optional<SkyUniforms> _uniforms;
	std::chrono::system_clock::time_point _uniformTime;
	bool _uniformsDirty = true;
	bool _uniformsSet = false;
	std::string _uniformError;
	std::optional<std::string> _standBackReason;
	int _moonDebug = 0;
	float _moonBrightnessScale = 1.0f;
	bool _restartMoonRenderer = false;

	/// The theme of the sky: the last alignment that the mod read, the theme that the sky goes to, the theme now
	/// and the value that the shaders have.
	std::optional<AlignmentReading> _alignment;
	std::string _alignmentError;
	double _themeTarget = 0.0;
	ThemeEase _themeEase;
	std::optional<Vector4> _themeUploaded;
	std::chrono::steady_clock::time_point _themeTime;
	int _depthOfFieldLevel = 0;
	std::optional<double> _themeTest;
	bool _themeSnap = false;

	/// The blood moon of the evil night: the property identifiers, the last night blend and the last values.
	std::optional<int32_t> _bloodMoonId;
	std::optional<Vector4> _bloodMoonUploaded;
	Vector4 _bloodMoonLast;
	double _nightBlend = 0.0;
	std::string _evilError;
	int32_t _blend2Id = 0;
};

} // namespace moa_mod::sky
