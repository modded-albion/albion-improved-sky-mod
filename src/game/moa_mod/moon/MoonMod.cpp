/******************************************************************************
 * Copyright (c) 2026 Masters of Albion Reforged developers
 *****************************************************************************/

#include "moa_mod/moon/MoonMod.h"

#include <array>
#include <format>
#include <utility>

#include <moa_mod/moon/MoonPlacement.h>

namespace moa_mod::moon
{

namespace
{
constexpr int k_PhaseFrames = 8;
constexpr auto k_SkyInterval = std::chrono::seconds(1);
/// The quad stays visible a little below the horizon, so that the disc goes down behind the horizon.
constexpr double k_HideBelowAltitude = -1.0;
constexpr std::array<const char*, 2> k_TextureProperties {"_BaseMap", "_MainTex"};

/// The values of `UnityEngine.Rendering.ShaderPropertyType`.
enum class ShaderPropertyType : int32_t
{
	Color = 0,
	Vector = 1,
	Float = 2,
	Range = 3,
	Texture = 4,
	Int = 5,
};

[[nodiscard]] nlohmann::json ToJson(WorldPoint point)
{
	return nlohmann::json::array({point.x, point.y, point.z});
}

[[nodiscard]] nlohmann::json ToJson(Quaternion rotation)
{
	return nlohmann::json::array({rotation.x, rotation.y, rotation.z, rotation.w});
}

[[nodiscard]] nlohmann::json ToJson(Vector2 value)
{
	return nlohmann::json::array({value.x, value.y});
}

[[nodiscard]] nlohmann::json ToJson(Vector4 value)
{
	return nlohmann::json::array({value.x, value.y, value.z, value.w});
}

template <typename T>
[[nodiscard]] nlohmann::json ToJson(const moa::Result<T>& result)
{
	if (!result)
	{
		return {{"error", result.GetError().message}};
	}
	if constexpr (std::is_arithmetic_v<T>)
	{
		return *result;
	}
	else
	{
		return ToJson(*result);
	}
}
} // namespace

moa::Result<SkyBindings> SkyBindings::Bind(const Managed& managed)
{
	Binder binder(managed);
	SkyBindings b;
	auto* sky = binder.Class("", "Marcos_Procedural_Sky");
	auto* gameManager = binder.Class("", "GameManager");
	auto* gameObject = binder.Class("UnityEngine", "GameObject");
	auto* component = binder.Class("UnityEngine", "Component");
	auto* transform = binder.Class("UnityEngine", "Transform");
	auto* renderer = binder.Class("UnityEngine", "Renderer");
	auto* material = binder.Class("UnityEngine", "Material");
	auto* shader = binder.Class("UnityEngine", "Shader");
	auto* object = binder.Class("UnityEngine", "Object");
	auto* light = binder.Class("UnityEngine", "Light");

	b.moonPhase = binder.Field(sky, "moonPhase", "System.Int32");
	b.dayNightCycle = binder.Field(sky, "dayNightCycle", "System.Single");
	b.sunAngle = binder.Field(sky, "sunAngle", "System.Single");
	b.moonObject = binder.Field(sky, "moonObject", "UnityEngine.GameObject");
	b.moonLight = binder.Field(sky, "moonDirectionLight", "UnityEngine.Light");
	b.moonIntensity = binder.Field(sky, "moon_Intensity_Mixed", "System.Single");

	b.isInterior = binder.Method(gameManager, "get_IsInterior", {}, "System.Boolean");
	b.isDesignTable = binder.Method(gameManager, "get_IsDesignTable", {}, "System.Boolean");
	b.isTitleScreen = binder.Method(gameManager, "IsInTitleScreen", {}, "System.Boolean");
	b.is3DTitleScreen = binder.Method(gameManager, "IsIn3DTitleScreen", {}, "System.Boolean");
	b.isCavelike = binder.Method(gameManager, "get_IsInCavelike", {}, "System.Boolean", true);

	if (renderer != nullptr)
	{
		b.rendererType = managed.TypeObject(renderer);
	}
	b.gameObjectTransform = binder.Method(gameObject, "get_transform", {}, "UnityEngine.Transform");
	b.getComponent = binder.Method(gameObject, "GetComponent", {"System.Type"}, "UnityEngine.Component");
	b.componentTransform = binder.Method(component, "get_transform", {}, "UnityEngine.Transform");
	b.getPosition = binder.Method(transform, "get_position", {}, "UnityEngine.Vector3");
	b.setPosition = binder.Method(transform, "set_position", {"UnityEngine.Vector3"}, "System.Void");
	b.getRotation = binder.Method(transform, "get_rotation", {}, "UnityEngine.Quaternion");
	b.setRotation = binder.Method(transform, "set_rotation", {"UnityEngine.Quaternion"}, "System.Void");
	b.getLocalPosition = binder.Method(transform, "get_localPosition", {}, "UnityEngine.Vector3");
	b.setLocalPosition = binder.Method(transform, "set_localPosition", {"UnityEngine.Vector3"}, "System.Void");
	b.getMaterial = binder.Method(renderer, "get_material", {}, "UnityEngine.Material");
	b.getEnabled = binder.Method(renderer, "get_enabled", {}, "System.Boolean");
	b.setEnabled = binder.Method(renderer, "set_enabled", {"System.Boolean"}, "System.Void");
	b.getShader = binder.Method(material, "get_shader", {}, "UnityEngine.Shader");
	b.hasProperty = binder.Method(material, "HasProperty", {"System.String"}, "System.Boolean");
	b.setTextureOffset = binder.Method(material, "SetTextureOffset", {"System.String", "UnityEngine.Vector2"}, "System.Void");
	b.getTextureOffset = binder.Method(material, "GetTextureOffset", {"System.String"}, "UnityEngine.Vector2");
	b.getTextureScale = binder.Method(material, "GetTextureScale", {"System.String"}, "UnityEngine.Vector2");
	b.getColor = binder.Method(material, "GetColor", {"System.String"}, "UnityEngine.Color");
	b.getFloat = binder.Method(material, "GetFloat", {"System.String"}, "System.Single");
	b.getVector = binder.Method(material, "GetVector", {"System.String"}, "UnityEngine.Vector4");
	b.propertyCount = binder.Method(shader, "GetPropertyCount", {}, "System.Int32");
	b.propertyName = binder.Method(shader, "GetPropertyName", {"System.Int32"}, "System.String");
	b.propertyType = binder.Method(shader, "GetPropertyType", {"System.Int32"}, "UnityEngine.Rendering.ShaderPropertyType");
	b.objectName = binder.Method(object, "get_name", {}, "System.String");
	b.getIntensity = binder.Method(light, "get_intensity", {}, "System.Single");
	b.setIntensity = binder.Method(light, "set_intensity", {"System.Single"}, "System.Void");
	b.getLightColor = binder.Method(light, "get_color", {}, "UnityEngine.Color");

	if (!binder.Errors().empty())
	{
		return moa::MakeError("{}", binder.Errors());
	}
	if (b.rendererType == nullptr)
	{
		return moa::MakeError("the mod cannot get the type object of UnityEngine.Renderer");
	}
	return b;
}

MoonMod::MoonMod(const Managed& managed, SkyBindings bindings, SkyboxSettings::Moon settings, ModStatus& status)
    : _managed(managed)
    , _bindings(std::move(bindings))
    , _settings(settings)
    , _status(status)
{
}

std::optional<std::string> MoonMod::StandBackReason() const
{
	auto gameManager = _managed.SingletonInstance("GameManager");
	if (!gameManager)
	{
		return gameManager.GetError().message;
	}
	struct Check
	{
		const MethodInfo* method;
		bool isStatic;
		const char* reason;
	};
	const std::array<Check, 5> checks {{
	    {_bindings.isInterior, false, "the camera is in an interior"},
	    {_bindings.isCavelike, true, "the camera is in a cave"},
	    {_bindings.isTitleScreen, false, "the title screen is open"},
	    {_bindings.is3DTitleScreen, false, "the title screen is open"},
	    {_bindings.isDesignTable, false, "the design table is open"},
	}};
	for (const auto& check : checks)
	{
		auto value = _managed.Call<bool>(check.method, check.isStatic ? nullptr : *gameManager);
		if (!value)
		{
			return value.GetError().message;
		}
		if (*value)
		{
			return check.reason;
		}
	}
	return std::nullopt;
}

const MoonMod::Sky& MoonMod::CurrentSky()
{
	const auto now = std::chrono::system_clock::now();
	if (!_sky || now - _sky->time >= k_SkyInterval || now < _sky->time)
	{
		const auto moon = moa::astro::ComputeMoon(now);
		const moa::astro::Observer observer {.latitude = _settings.latitude, .longitude = _settings.longitude};
		_sky = Sky {
		    .time = now,
		    .moon = moon,
		    .horizontal = moa::astro::MoonHorizontal(moon, observer),
		    .frame = moa::astro::PhaseFrame(moon.phase, k_PhaseFrames),
		};
	}
	return *_sky;
}

moa::Result<Il2CppObject*> MoonMod::MoonMaterial(Il2CppObject* moonObject, Il2CppObject** renderer) const
{
	auto component = _managed.Invoke(_bindings.getComponent, moonObject, {_bindings.rendererType});
	if (!component)
	{
		return component.GetError();
	}
	if (*component == nullptr)
	{
		return moa::MakeError("the moon object has no renderer");
	}
	*renderer = *component;
	auto material = _managed.Invoke(_bindings.getMaterial, *component);
	if (!material)
	{
		return material.GetError();
	}
	if (*material == nullptr)
	{
		return moa::MakeError("the moon renderer has no material");
	}
	return *material;
}

moa::Result<std::string> MoonMod::TextureProperty(Il2CppObject* material)
{
	if (_textureProperty)
	{
		return *_textureProperty;
	}
	for (const auto* name : k_TextureProperties)
	{
		auto has = _managed.Call<bool>(_bindings.hasProperty, material, {_managed.String(name)});
		if (!has)
		{
			return has.GetError();
		}
		if (*has)
		{
			_textureProperty = name;
			return *_textureProperty;
		}
	}
	return moa::MakeError("the moon material has no '_BaseMap' and no '_MainTex'");
}

void MoonMod::Fail(const std::string& error)
{
	_status.SetError(error);
	_status.SetActive(false, error);
}

void MoonMod::After(void* sky)
{
	const auto& b = _bindings;
	if (sky == nullptr)
	{
		return;
	}
	if (auto reason = StandBackReason())
	{
		Restore(sky);
		_status.SetActive(false, *reason);
		return;
	}
	auto* moonObject = ReadField<Il2CppObject*>(sky, b.moonObject);
	if (moonObject == nullptr)
	{
		_status.SetActive(false, "the sky has no moon object");
		return;
	}
	const auto& now = CurrentSky();

	// The phase: the game keeps the texture offset of the asset, so the mod sets it in each frame.
	Il2CppObject* renderer = nullptr;
	auto material = MoonMaterial(moonObject, &renderer);
	if (!material)
	{
		Fail(material.GetError().message);
		return;
	}
	auto property = TextureProperty(*material);
	if (!property)
	{
		Fail(property.GetError().message);
		return;
	}
	if (!_originalOffset)
	{
		auto offset = _managed.Call<Vector2>(b.getTextureOffset, *material, {_managed.String(property->c_str())});
		if (!offset)
		{
			Fail(offset.GetError().message);
			return;
		}
		_originalOffset = *offset;
	}
	Vector2 offset {.x = PhaseTextureOffset(now.frame, k_PhaseFrames), .y = _originalOffset->y};
	if (auto result = _managed.Invoke(b.setTextureOffset, *material, {_managed.String(property->c_str()), &offset}); !result)
	{
		Fail(result.GetError().message);
		return;
	}
	if (_settings.phaseOnly || !_settings.followRealPosition)
	{
		_status.SetActive(true);
		return;
	}

	// The place: the quad goes to the real direction of the moon, at the distance that the game uses.
	auto moonTransform = _managed.Invoke(b.gameObjectTransform, moonObject);
	auto skyTransform = _managed.Invoke(b.componentTransform, sky);
	if (!moonTransform || !skyTransform || *moonTransform == nullptr || *skyTransform == nullptr)
	{
		Fail("the mod cannot get the transforms of the sky and the moon");
		return;
	}
	auto centre = _managed.Call<WorldPoint>(b.getPosition, *skyTransform);
	auto gamePosition = _managed.Call<WorldPoint>(b.getPosition, *moonTransform);
	if (!centre || !gamePosition)
	{
		Fail(!centre ? centre.GetError().message : gamePosition.GetError().message);
		return;
	}
	if (!_originalLocalPosition)
	{
		auto local = _managed.Call<WorldPoint>(b.getLocalPosition, *moonTransform);
		if (!local)
		{
			Fail(local.GetError().message);
			return;
		}
		_originalLocalPosition = *local;
	}
	const auto placement = PlaceMoon({
	    .horizontal = now.horizontal,
	    .latitude = _settings.latitude,
	    .brightLimbAngle = now.moon.phase.brightLimbAngle,
	    .waxing = now.moon.phase.waxing,
	    .northDegrees = _settings.northDegrees,
	    .centre = *centre,
	    .distance = Length(*gamePosition - *centre),
	});
	auto position = placement.position;
	auto rotation = placement.quadRotation;
	if (!_managed.Invoke(b.setPosition, *moonTransform, {&position}) ||
	    !_managed.Invoke(b.setRotation, *moonTransform, {&rotation}))
	{
		Fail("the mod cannot move the moon quad");
		return;
	}
	const auto visible = now.horizontal.altitude > k_HideBelowAltitude;
	if (auto enabled = _managed.Call<bool>(b.getEnabled, renderer); enabled && *enabled != visible)
	{
		bool value = visible;
		if (_managed.Invoke(b.setEnabled, renderer, {&value}))
		{
			_quadHidden = !visible;
		}
	}

	// The light: from the moon, with the lit part of the disc. The game writes its own intensity before this.
	if (auto* light = ReadField<Il2CppObject*>(sky, b.moonLight); light != nullptr)
	{
		auto lightTransform = _managed.Invoke(b.componentTransform, light);
		auto lightRotation = placement.lightRotation;
		float intensity = placement.aboveHorizon
		                      ? ReadField<float>(sky, b.moonIntensity) *
		                            static_cast<float>(now.moon.phase.illuminatedFraction * _settings.lightScale)
		                      : 0.0f;
		if (!lightTransform || *lightTransform == nullptr ||
		    !_managed.Invoke(b.setRotation, *lightTransform, {&lightRotation}) ||
		    !_managed.Invoke(b.setIntensity, light, {&intensity}))
		{
			Fail("the mod cannot change the moon light");
			return;
		}
		_lastLightIntensity = intensity;
	}
	_status.SetActive(true);
}

void MoonMod::Restore(void* sky)
{
	const auto& b = _bindings;
	if (!_originalOffset && !_quadHidden && !_originalLocalPosition)
	{
		return;
	}
	auto* moonObject = sky != nullptr ? ReadField<Il2CppObject*>(sky, b.moonObject) : nullptr;
	if (moonObject == nullptr)
	{
		return;
	}
	Il2CppObject* renderer = nullptr;
	auto material = MoonMaterial(moonObject, &renderer);
	if (material && _originalOffset && _textureProperty)
	{
		auto offset = *_originalOffset;
		if (_managed.Invoke(b.setTextureOffset, *material, {_managed.String(_textureProperty->c_str()), &offset}))
		{
			_originalOffset.reset();
		}
	}
	if (_quadHidden && renderer != nullptr)
	{
		bool value = true;
		if (_managed.Invoke(b.setEnabled, renderer, {&value}))
		{
			_quadHidden = false;
		}
	}
	if (_originalLocalPosition)
	{
		if (auto transform = _managed.Invoke(b.gameObjectTransform, moonObject); transform && *transform != nullptr)
		{
			auto local = *_originalLocalPosition;
			if (_managed.Invoke(b.setLocalPosition, *transform, {&local}))
			{
				_originalLocalPosition.reset();
			}
		}
	}
}

nlohmann::json DescribeSky(const Managed& managed, const SkyBindings& b, void* sky)
{
	if (sky == nullptr)
	{
		return {{"error", "there is no Marcos_Procedural_Sky"}};
	}
	nlohmann::json result {
	    {"moonPhase", ReadField<int32_t>(sky, b.moonPhase)},
	    {"dayNightCycle", ReadField<float>(sky, b.dayNightCycle)},
	    {"sunAngle", ReadField<float>(sky, b.sunAngle)},
	    {"moonIntensityMixed", ReadField<float>(sky, b.moonIntensity)},
	};
	if (auto transform = managed.Invoke(b.componentTransform, sky); transform && *transform != nullptr)
	{
		result["skyCentre"] = ToJson(managed.Call<WorldPoint>(b.getPosition, *transform));
	}
	auto* moonObject = ReadField<Il2CppObject*>(sky, b.moonObject);
	if (moonObject == nullptr)
	{
		result["moonObject"] = nullptr;
		return result;
	}
	auto& moon = result["moonObject"];
	if (auto name = managed.Invoke(b.objectName, moonObject); name)
	{
		moon["name"] = managed.Text(*name);
	}
	if (auto transform = managed.Invoke(b.gameObjectTransform, moonObject); transform && *transform != nullptr)
	{
		moon["position"] = ToJson(managed.Call<WorldPoint>(b.getPosition, *transform));
		moon["rotation"] = ToJson(managed.Call<Quaternion>(b.getRotation, *transform));
		moon["localPosition"] = ToJson(managed.Call<WorldPoint>(b.getLocalPosition, *transform));
		if (auto rotation = managed.Call<Quaternion>(b.getRotation, *transform); rotation)
		{
			moon["frontFaceDirection"] = ToJson(Rotate(*rotation, WorldPoint {0.0f, 0.0f, 1.0f}));
		}
	}
	auto renderer = managed.Invoke(b.getComponent, moonObject, {b.rendererType});
	if (!renderer || *renderer == nullptr)
	{
		moon["renderer"] = {{"error", renderer ? "no renderer" : renderer.GetError().message}};
		return result;
	}
	moon["rendererEnabled"] = ToJson(managed.Call<bool>(b.getEnabled, *renderer));
	auto material = managed.Invoke(b.getMaterial, *renderer);
	if (!material || *material == nullptr)
	{
		moon["material"] = {{"error", material ? "no material" : material.GetError().message}};
		return result;
	}
	auto& materialJson = moon["material"];
	if (auto name = managed.Invoke(b.objectName, *material); name)
	{
		materialJson["name"] = managed.Text(*name);
	}
	auto shader = managed.Invoke(b.getShader, *material);
	if (!shader || *shader == nullptr)
	{
		materialJson["shader"] = nullptr;
		return result;
	}
	if (auto name = managed.Invoke(b.objectName, *shader); name)
	{
		materialJson["shader"] = managed.Text(*name);
	}
	auto properties = nlohmann::json::array();
	const auto count = managed.Call<int32_t>(b.propertyCount, *shader);
	for (int32_t index = 0; count && index < *count; ++index)
	{
		auto nameObject = managed.Invoke(b.propertyName, *shader, {&index});
		auto type = managed.Call<int32_t>(b.propertyType, *shader, {&index});
		if (!nameObject || !type)
		{
			continue;
		}
		const auto name = managed.Text(*nameObject);
		auto* nameString = managed.String(name.c_str());
		nlohmann::json entry {{"name", name}, {"type", *type}};
		switch (static_cast<ShaderPropertyType>(*type))
		{
		case ShaderPropertyType::Color:
			entry["value"] = ToJson(managed.Call<Vector4>(b.getColor, *material, {nameString}));
			break;
		case ShaderPropertyType::Vector:
			entry["value"] = ToJson(managed.Call<Vector4>(b.getVector, *material, {nameString}));
			break;
		case ShaderPropertyType::Float:
		case ShaderPropertyType::Range:
			entry["value"] = ToJson(managed.Call<float>(b.getFloat, *material, {nameString}));
			break;
		case ShaderPropertyType::Texture:
			entry["offset"] = ToJson(managed.Call<Vector2>(b.getTextureOffset, *material, {nameString}));
			entry["scale"] = ToJson(managed.Call<Vector2>(b.getTextureScale, *material, {nameString}));
			break;
		default:
			break;
		}
		properties.push_back(std::move(entry));
	}
	materialJson["properties"] = std::move(properties);

	if (auto* light = ReadField<Il2CppObject*>(sky, b.moonLight); light != nullptr)
	{
		auto& lightJson = result["moonLight"];
		lightJson["intensity"] = ToJson(managed.Call<float>(b.getIntensity, light));
		lightJson["color"] = ToJson(managed.Call<Vector4>(b.getLightColor, light));
		if (auto transform = managed.Invoke(b.componentTransform, light); transform && *transform != nullptr)
		{
			if (auto rotation = managed.Call<Quaternion>(b.getRotation, *transform); rotation)
			{
				lightJson["rotation"] = ToJson(*rotation);
				lightJson["forward"] = ToJson(Rotate(*rotation, WorldPoint {0.0f, 0.0f, 1.0f}));
			}
		}
	}
	return result;
}

nlohmann::json MoonMod::State(void* sky)
{
	auto result = DescribeSky(_managed, _bindings, sky);
	const auto& now = CurrentSky();
	const auto reason = StandBackReason();
	const auto& phase = now.moon.phase;
	const auto placement = PlaceMoon({
	    .horizontal = now.horizontal,
	    .latitude = _settings.latitude,
	    .brightLimbAngle = phase.brightLimbAngle,
	    .waxing = phase.waxing,
	    .northDegrees = _settings.northDegrees,
	    .centre = {},
	    .distance = 1.0f,
	});
	result["mod"] = {
	    {"standBackReason", reason ? *reason : ""},
	    {"utc", std::format("{:%Y-%m-%d %H:%M:%S}", std::chrono::floor<std::chrono::seconds>(now.time))},
	    {"altitude", now.horizontal.altitude},
	    {"azimuth", now.horizontal.azimuth},
	    {"illuminatedFraction", phase.illuminatedFraction},
	    {"phaseAngle", phase.phaseAngle},
	    {"elongation", phase.elongation},
	    {"waxing", phase.waxing},
	    {"brightLimbAngle", phase.brightLimbAngle},
	    {"rollDegrees", placement.rollDegrees},
	    {"frame", now.frame},
	    {"textureOffsetX", PhaseTextureOffset(now.frame, k_PhaseFrames)},
	    {"textureProperty", _textureProperty.value_or("")},
	    {"worldDirection", ToJson(placement.direction)},
	    {"quadHidden", _quadHidden},
	    {"lastLightIntensity", _lastLightIntensity},
	    {"settings",
	     {
	         {"latitude", _settings.latitude},
	         {"longitude", _settings.longitude},
	         {"northDegrees", _settings.northDegrees},
	         {"followRealPosition", _settings.followRealPosition},
	         {"phaseOnly", _settings.phaseOnly},
	         {"lightScale", _settings.lightScale},
	     }},
	};
	return result;
}

} // namespace moa_mod::moon
