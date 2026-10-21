/******************************************************************************
 * Copyright (c) 2026 Masters of Albion Reforged developers
 *****************************************************************************/

#pragma once

#include <chrono>
#include <optional>
#include <string>

#include <moa/astro/Moon.h>
#include <moa/common/Result.h>
#include <moa_mod/UnityMath.h>
#include <moa_mod/sky/SkyboxSettings.h>
#include <nlohmann/json.hpp>

#include "moa_mod/Managed.h"
#include "moa_mod/ModStatus.h"

namespace moa_mod::moon
{

/// The names of the game and of Unity that the moon mod and the query `moon.state` use.
struct SkyBindings
{
	// Marcos_Procedural_Sky
	size_t moonPhase = 0;
	size_t dayNightCycle = 0;
	size_t sunAngle = 0;
	size_t moonObject = 0;
	size_t moonLight = 0;
	size_t moonIntensity = 0;
	// GameManager
	const MethodInfo* isInterior = nullptr;
	const MethodInfo* isDesignTable = nullptr;
	const MethodInfo* isTitleScreen = nullptr;
	const MethodInfo* is3DTitleScreen = nullptr;
	const MethodInfo* isCavelike = nullptr;
	// Unity
	Il2CppObject* rendererType = nullptr;
	const MethodInfo* gameObjectTransform = nullptr;
	const MethodInfo* getComponent = nullptr;
	const MethodInfo* componentTransform = nullptr;
	const MethodInfo* getPosition = nullptr;
	const MethodInfo* setPosition = nullptr;
	const MethodInfo* getRotation = nullptr;
	const MethodInfo* setRotation = nullptr;
	const MethodInfo* getLocalPosition = nullptr;
	const MethodInfo* setLocalPosition = nullptr;
	const MethodInfo* getMaterial = nullptr;
	const MethodInfo* getEnabled = nullptr;
	const MethodInfo* setEnabled = nullptr;
	const MethodInfo* getShader = nullptr;
	const MethodInfo* hasProperty = nullptr;
	const MethodInfo* setTextureOffset = nullptr;
	const MethodInfo* getTextureOffset = nullptr;
	const MethodInfo* getTextureScale = nullptr;
	const MethodInfo* getColor = nullptr;
	const MethodInfo* getFloat = nullptr;
	const MethodInfo* getVector = nullptr;
	const MethodInfo* propertyCount = nullptr;
	const MethodInfo* propertyName = nullptr;
	const MethodInfo* propertyType = nullptr;
	const MethodInfo* objectName = nullptr;
	const MethodInfo* getIntensity = nullptr;
	const MethodInfo* setIntensity = nullptr;
	const MethodInfo* getLightColor = nullptr;

	[[nodiscard]] static moa::Result<SkyBindings> Bind(const Managed& managed);
};

/// The real moon. It works after `Marcos_Procedural_Sky.Update`, which writes the sky of the game in each frame:
///
/// - The moon texture shows the frame of the real phase for the clock of the computer.
/// - The moon quad goes to the real place of the moon on the sky of the observer, and turns so that the lit side
///   is at the bright limb angle.
/// - The moon light comes from the real moon, and its intensity follows the lit part of the disc. It is zero when
///   the moon is below the horizon.
///
/// The mod does nothing in interiors, caves, the title screen and the design table.
class MoonMod
{
public:
	MoonMod(const Managed& managed, SkyBindings bindings, SkyboxSettings::Moon settings, ModStatus& status);

	void After(void* sky);

	/// Put back what the game does not write again in each frame: the texture offset, the visible state of the quad
	/// and the local position of the quad. Call it on the main thread before the hook goes.
	void Restore(void* sky);

	/// Describe the sky, the moon and the state of the mod, for the query `moon.state`.
	[[nodiscard]] nlohmann::json State(void* sky);

private:
	/// The moon for the clock of the computer, calculated one time in each second.
	struct Sky
	{
		std::chrono::system_clock::time_point time;
		moa::astro::MoonState moon;
		moa::astro::Horizontal horizontal;
		int frame = 0;
	};

	[[nodiscard]] std::optional<std::string> StandBackReason() const;
	[[nodiscard]] const Sky& CurrentSky();
	[[nodiscard]] moa::Result<Il2CppObject*> MoonMaterial(Il2CppObject* moonObject, Il2CppObject** renderer) const;
	[[nodiscard]] moa::Result<std::string> TextureProperty(Il2CppObject* material);
	void Fail(const std::string& error);

	const Managed& _managed;
	SkyBindings _bindings;
	SkyboxSettings::Moon _settings;
	ModStatus& _status;

	std::optional<Sky> _sky;
	std::optional<std::string> _textureProperty;
	std::optional<Vector2> _originalOffset;
	std::optional<WorldPoint> _originalLocalPosition;
	bool _quadHidden = false;
	float _lastLightIntensity = 0.0f;
};

/// Describe the sky with no mod: for the query `moon.state` when the moon mod is off.
[[nodiscard]] nlohmann::json DescribeSky(const Managed& managed, const SkyBindings& bindings, void* sky);

} // namespace moa_mod::moon
