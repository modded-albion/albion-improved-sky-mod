/******************************************************************************
 * Copyright (c) 2026 Masters of Albion Reforged developers
 *****************************************************************************/

#include "game/SkyMod.h"

#include <algorithm>
#include <format>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>
#include <vector>

#include <astro/Moon.h>
#include <spdlog/spdlog.h>

namespace moa_mod::sky
{

namespace
{
/// The most frames that the mod waits for the bundle. After that, the load counts as failed.
constexpr int k_MostWaitFrames = 1200;
/// The test quad: its distance from the centre of the sky, its height over the horizon and its heading.
constexpr float k_QuadDistance = 3000.0f;
constexpr float k_QuadAltitude = 35.0f;
constexpr float k_QuadHeading = 90.0f;
/// The mod calculates the sky again after this time. The stars turn 0.004 degrees in one second.
constexpr auto k_UniformInterval = std::chrono::seconds(1);

/// Read all the bytes of a file, or nothing if the file cannot be read.
[[nodiscard]] std::vector<std::byte> ReadBytes(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		return {};
	}
	const std::vector<char> chars {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
	std::vector<std::byte> bytes(chars.size());
	std::ranges::transform(chars, bytes.begin(), [](char c) { return static_cast<std::byte>(c); });
	return bytes;
}

[[nodiscard]] SkyEffects EffectsOf(const SkyboxSettings::Sky& settings)
{
	return {.stars = settings.stars,
	        .starBrightness = settings.starBrightness,
	        .aurora = settings.aurora,
	        .moonShader = settings.moonShader,
	        .alignmentTheme = settings.alignmentTheme,
	        .goodStrength = settings.goodStrength,
	        .evilStrength = settings.evilStrength};
}

/// Bind the names that the theme of the sky uses. A missing name turns off only the part that needs it.
void BindTheme(const Managed& managed, SkyModBindings& b)
{
	Binder binder(managed);
	auto* alignment = binder.Class("", "AlignmentManager");
	auto* manager = binder.Class("", "NGManager");
	auto* depthOfField = binder.Class("", "DepthOfFieldController");
	b.getAlignment = binder.Method(alignment, "get_Alignment", {}, "System.Single");
	b.evilLimit = binder.Field(manager, "m_evilAlignmentThreshold", "System.Single");
	b.goodLimit = binder.Field(manager, "m_goodAlignmentThreshold", "System.Single");
	b.depthOfFieldLevel = binder.Method(depthOfField, "get_Level", {}, "System.Int32", true);
	if (!binder.Errors().empty())
	{
		b.alignmentError = binder.Errors();
		spdlog::warn("sky mod: the sky stays neutral or the stars keep their size: {}", binder.Errors());
	}
}

/// Bind the names that the blood moon of the evil night uses. A missing name turns off only the blood moon.
void BindEvil(const Managed& managed, SkyModBindings& b)
{
	Binder binder(managed);
	auto* material = binder.Class("UnityEngine", "Material");
	b.getFloatById = binder.Method(material, "GetFloat", {"System.Int32"}, "System.Single");
	if (!binder.Errors().empty())
	{
		b.evilError = binder.Errors();
		spdlog::warn("sky mod: the blood moon is off: {}", binder.Errors());
	}
}
} // namespace

moa::Result<SkyModBindings> SkyModBindings::Bind(const Managed& managed)
{
	Binder binder(managed);
	SkyModBindings b;
	auto* sky = binder.Class("", "Marcos_Procedural_Sky");
	auto* gameManager = binder.Class("", "GameManager");
	auto* assetBundle = binder.Class("UnityEngine", "AssetBundle");
	auto* asyncOperation = binder.Class("UnityEngine", "AsyncOperation");
	auto* createRequest = binder.Class("UnityEngine", "AssetBundleCreateRequest");
	auto* material = binder.Class("UnityEngine", "Material");
	auto* shader = binder.Class("UnityEngine", "Shader");
	auto* object = binder.Class("UnityEngine", "Object");
	auto* gameObject = binder.Class("UnityEngine", "GameObject");
	auto* component = binder.Class("UnityEngine", "Component");
	auto* transform = binder.Class("UnityEngine", "Transform");
	auto* renderer = binder.Class("UnityEngine", "Renderer");
	auto* renderSettings = binder.Class("UnityEngine", "RenderSettings");

	b.dayNightCycle = binder.Field(sky, "dayNightCycle", "System.Single");
	b.moonObject = binder.Field(sky, "moonObject", "UnityEngine.GameObject");

	b.isInterior = binder.Method(gameManager, "get_IsInterior", {}, "System.Boolean");
	b.isDesignTable = binder.Method(gameManager, "get_IsDesignTable", {}, "System.Boolean");
	b.isTitleScreen = binder.Method(gameManager, "IsInTitleScreen", {}, "System.Boolean");
	b.is3DTitleScreen = binder.Method(gameManager, "IsIn3DTitleScreen", {}, "System.Boolean");
	b.isCavelike = binder.Method(gameManager, "get_IsInCavelike", {}, "System.Boolean", true);

	b.loadFromFileAsync =
	    binder.Method(assetBundle, "LoadFromFileAsync", {"System.String"}, "UnityEngine.AssetBundleCreateRequest", true);
	b.isDone = binder.Method(asyncOperation, "get_isDone", {}, "System.Boolean");
	b.requestBundle = binder.Method(createRequest, "get_assetBundle", {}, "UnityEngine.AssetBundle");
	b.loadAsset = binder.Method(assetBundle, "LoadAsset", {"System.String", "System.Type"}, "UnityEngine.Object");
	b.unloadBundle = binder.Method(assetBundle, "Unload", {"System.Boolean"}, "System.Void");
	b.getShader = binder.Method(material, "get_shader", {}, "UnityEngine.Shader");
	b.isSupported = binder.Method(shader, "get_isSupported", {}, "System.Boolean");
	b.objectName = binder.Method(object, "get_name", {}, "System.String");
	b.getTexture = binder.Method(material, "GetTexture", {"System.String"}, "UnityEngine.Texture");
	b.setTexture = binder.Method(material, "SetTexture", {"System.String", "UnityEngine.Texture"}, "System.Void");
	b.getFloat = binder.Method(material, "GetFloat", {"System.String"}, "System.Single");
	b.setFloat = binder.Method(material, "SetFloat", {"System.String", "System.Single"}, "System.Void");
	b.propertyToId = binder.Method(shader, "PropertyToID", {"System.String"}, "System.Int32", true);
	b.setGlobalVector = binder.Method(shader, "SetGlobalVector", {"System.Int32", "UnityEngine.Vector4"}, "System.Void", true);
	b.instantiate = binder.Method(object, "Instantiate", {"UnityEngine.Object", "UnityEngine.Transform", "System.Boolean"},
	                              "UnityEngine.Object", true);
	b.destroy = binder.Method(object, "Destroy", {"UnityEngine.Object"}, "System.Void", true);
	b.componentTransform = binder.Method(component, "get_transform", {}, "UnityEngine.Transform");
	b.getComponent = binder.Method(gameObject, "GetComponent", {"System.Type"}, "UnityEngine.Component");
	b.getSharedMaterial = binder.Method(renderer, "get_sharedMaterial", {}, "UnityEngine.Material");
	b.setSharedMaterial = binder.Method(renderer, "set_sharedMaterial", {"UnityEngine.Material"}, "System.Void");
	b.setEnabled = binder.Method(renderer, "set_enabled", {"System.Boolean"}, "System.Void");
	b.setActive = binder.Method(gameObject, "SetActive", {"System.Boolean"}, "System.Void");
	b.setLocalPosition = binder.Method(transform, "set_localPosition", {"UnityEngine.Vector3"}, "System.Void");
	b.setLocalRotation = binder.Method(transform, "set_localRotation", {"UnityEngine.Quaternion"}, "System.Void");
	b.getSkybox = binder.Method(renderSettings, "get_skybox", {}, "UnityEngine.Material", true);
	b.setSkybox = binder.Method(renderSettings, "set_skybox", {"UnityEngine.Material"}, "System.Void", true);

	if (material != nullptr)
	{
		b.materialType = managed.TypeObject(material);
	}
	if (renderer != nullptr)
	{
		b.rendererType = managed.TypeObject(renderer);
	}
	if (!binder.Errors().empty())
	{
		return moa::MakeError("{}", binder.Errors());
	}
	BindTheme(managed, b);
	BindEvil(managed, b);
	if (b.materialType == nullptr || b.rendererType == nullptr)
	{
		return moa::MakeError("the mod cannot get the type objects of UnityEngine.Material and UnityEngine.Renderer");
	}
	if (managed.il2cpp_gchandle_new == nullptr || managed.il2cpp_gchandle_free == nullptr)
	{
		return moa::MakeError("the game does not export the GC handle functions");
	}
	return b;
}

SkyMod::Held::Held(const Managed& managed, Il2CppObject* object)
    : _managed(&managed)
    , _object(object)
{
	if (object != nullptr)
	{
		_handle = managed.il2cpp_gchandle_new(object, false);
	}
}

SkyMod::Held::~Held()
{
	Reset();
}

SkyMod::Held::Held(Held&& other) noexcept
    : _managed(std::exchange(other._managed, nullptr))
    , _object(std::exchange(other._object, nullptr))
    , _handle(std::exchange(other._handle, 0))
{
}

SkyMod::Held& SkyMod::Held::operator=(Held&& other) noexcept
{
	if (this != &other)
	{
		Reset();
		_managed = std::exchange(other._managed, nullptr);
		_object = std::exchange(other._object, nullptr);
		_handle = std::exchange(other._handle, 0);
	}
	return *this;
}

void SkyMod::Held::Reset()
{
	if (_handle != 0 && _managed != nullptr)
	{
		_managed->il2cpp_gchandle_free(_handle);
	}
	_handle = 0;
	_object = nullptr;
}

SkyMod::SkyMod(const Managed& managed, SkyModBindings bindings, SkyboxSettings::Sky settings, SkyPlace place,
               std::filesystem::path bundleFile, ModStatus& status)
    : _managed(managed)
    , _bindings(bindings)
    , _settings(settings)
    , _place(place)
    , _bundleFile(std::move(bundleFile))
    , _status(status)
{
}

SkyMod::~SkyMod() = default;

void SkyMod::After(void* sky)
{
	if (sky == nullptr || _stage == LoadStage::Failed)
	{
		return;
	}
	if (_stage == LoadStage::Ready)
	{
		if (_settings.testQuad && sky != _quadSky)
		{
			// A new scene has a new sky, and the old quad went with the old scene. Make the quad again.
			_quadRenderer.Reset();
			_quad.Reset();
			_quadShown.reset();
			_stage = LoadStage::PlaceQuad;
			spdlog::info("sky mod: the sky changed; the mod makes the test quad again");
		}
		else
		{
			if (_settings.testQuad)
			{
				UpdateQuad(sky);
			}
			auto reason = StandBackReason();
			if (reason)
			{
				if (_standBackReason != reason)
				{
					StandBack();
					spdlog::info("sky mod: stands back: {}", *reason);
				}
				_standBackReason = std::move(reason);
				_status.SetActive(false, *_standBackReason);
				return;
			}
			if (_standBackReason)
			{
				spdlog::info("sky mod: the open world is back");
				_standBackReason.reset();
				_uniformsDirty = true;
			}
			if (_skyboxError.empty())
			{
				if (auto result = UpdateSkybox(); !result)
				{
					FailEffect(_skyboxError, "skybox", result.GetError());
					RestoreSkybox();
				}
			}
			if (_settings.moonShader && _moonError.empty() && _moonMaterial.Get() != nullptr)
			{
				if (auto result = UpdateMoon(sky); !result)
				{
					FailEffect(_moonError, "moon", result.GetError());
					RestoreMoon();
				}
			}
			else
			{
				RestoreMoon();
			}
			if (_uniformError.empty())
			{
				const auto& uniforms = CurrentUniforms();
				if (!_uniformsSet)
				{
					if (auto result = UploadUniforms(uniforms); !result)
					{
						FailEffect(_uniformError, "shader values", result.GetError());
					}
				}
			}
			if (_uniformError.empty())
			{
				UpdateTheme();
			}
			if (_uniformError.empty() && _evilError.empty())
			{
				UpdateEvilNight();
			}
			_status.SetActive(_skyboxSwapped, _skyboxSwapped ? std::string() : "the skybox of the game is in use");
			return;
		}
	}
	const auto stage = _stage;
	auto result = Step(sky);
	if (!result)
	{
		_error = std::format("{}: {}", StageName(stage), result.GetError().message);
		_stage = LoadStage::Failed;
		_status.SetError(_error);
		_status.SetActive(false, "the load of the bundle failed");
		spdlog::error("sky mod: {}; the sky mod stops", _error);
		return;
	}
	_stage = NextStage(stage, *result);
	if (_stage != stage)
	{
		spdlog::info("sky mod: the step '{}' is complete", StageName(stage));
	}
}

void SkyMod::FailEffect(std::string& error, const char* effect, const moa::Error& reason)
{
	error = reason.message;
	_status.SetError(std::format("{}: {}", effect, reason.message));
	spdlog::error("sky mod: {}: {}; this effect stops, the others continue", effect, reason.message);
}

void SkyMod::ApplySettings(const SkyboxSettings::Sky& settings, SkyPlace place)
{
	_settings.stars = settings.stars;
	_settings.starBrightness = settings.starBrightness;
	_settings.aurora = settings.aurora;
	_settings.moonShader = settings.moonShader;
	_settings.alignmentTheme = settings.alignmentTheme;
	_settings.useGameAlignment = settings.useGameAlignment;
	_settings.alignment = settings.alignment;
	_settings.goodStrength = settings.goodStrength;
	_settings.evilStrength = settings.evilStrength;
	_settings.evil = settings.evil;
	_place = place;
	_uniformsDirty = true;
}

moa::Result<StepResult> SkyMod::Step(void* sky)
{
	switch (_stage)
	{
	case LoadStage::WriteFile:
		return WriteFile();
	case LoadStage::StartLoad:
		return StartLoad();
	case LoadStage::WaitForBundle:
		return WaitForBundle();
	case LoadStage::LoadMaterial:
		return LoadMaterial();
	case LoadStage::PlaceQuad:
		return PlaceQuad(sky);
	case LoadStage::Ready:
	case LoadStage::Failed:
		break;
	}
	return StepResult::Done;
}

moa::Result<StepResult> SkyMod::WriteFile()
{
	const auto embedded = EmbeddedBundle();
	if (embedded.empty())
	{
		return moa::MakeError("the mod has no bundle (build it with tools/unity/moa_sky/build_bundles.py)");
	}
	if (BundleFileIsCurrent(ReadBytes(_bundleFile), embedded))
	{
		return StepResult::Done;
	}
	std::error_code error;
	std::filesystem::create_directories(_bundleFile.parent_path(), error);
	const auto temporary = std::filesystem::path(_bundleFile).concat(".tmp");
	{
		std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
		file.write(reinterpret_cast<const char*>(embedded.data()), static_cast<std::streamsize>(embedded.size()));
		if (!file)
		{
			return moa::MakeError("the mod cannot write {}", temporary.string());
		}
	}
	std::filesystem::rename(temporary, _bundleFile, error);
	if (error)
	{
		return moa::MakeError("the mod cannot write {}: {}", _bundleFile.string(), error.message());
	}
	_fileWritten = true;
	spdlog::info("sky mod: wrote {} bytes to {}", embedded.size(), _bundleFile.string());
	return StepResult::Done;
}

moa::Result<StepResult> SkyMod::StartLoad()
{
	auto request = _managed.Invoke(_bindings.loadFromFileAsync, nullptr, {_managed.String(_bundleFile.string().c_str())});
	if (!request)
	{
		return request.GetError();
	}
	if (*request == nullptr)
	{
		return moa::MakeError("LoadFromFileAsync returned no request");
	}
	_request = Held(_managed, *request);
	return StepResult::Done;
}

moa::Result<StepResult> SkyMod::WaitForBundle()
{
	auto done = _managed.Call<bool>(_bindings.isDone, _request.Get());
	if (!done)
	{
		return done.GetError();
	}
	if (!*done)
	{
		_waitFrames += 1;
		if (_waitFrames > k_MostWaitFrames)
		{
			return moa::MakeError("the load did not complete in {} frames", k_MostWaitFrames);
		}
		return StepResult::Wait;
	}
	auto bundle = _managed.Invoke(_bindings.requestBundle, _request.Get());
	_request.Reset();
	if (!bundle)
	{
		return bundle.GetError();
	}
	if (*bundle == nullptr)
	{
		return moa::MakeError("Unity did not load the bundle (see Player.log; the bundle can be for another Unity "
		                      "version, or a bundle with the same name is already loaded)");
	}
	_bundle = Held(_managed, *bundle);
	spdlog::info("sky mod: the bundle is loaded after {} frames", _waitFrames);
	return StepResult::Done;
}

moa::Result<Il2CppObject*> SkyMod::LoadBundleMaterial(std::string_view name, std::string_view shaderName)
{
	const std::string text(name);
	auto material =
	    _managed.Invoke(_bindings.loadAsset, _bundle.Get(), {_managed.String(text.c_str()), _bindings.materialType});
	if (!material)
	{
		return material.GetError();
	}
	if (*material == nullptr)
	{
		return moa::MakeError("the bundle has no material '{}'", name);
	}
	auto shader = _managed.Invoke(_bindings.getShader, *material);
	if (!shader || *shader == nullptr)
	{
		return moa::MakeError("the material '{}' has no shader", name);
	}
	auto loadedName = ShaderName(*material);
	if (!loadedName)
	{
		return loadedName.GetError();
	}
	if (*loadedName != shaderName)
	{
		return moa::MakeError("the material '{}' has the shader '{}', not '{}'", name, *loadedName, shaderName);
	}
	auto supported = _managed.Call<bool>(_bindings.isSupported, *shader);
	if (!supported)
	{
		return supported.GetError();
	}
	if (!*supported)
	{
		return moa::MakeError("the shader '{}' does not work on this graphics card or graphics API", shaderName);
	}
	return *material;
}

moa::Result<StepResult> SkyMod::LoadMaterial()
{
	// The skybox is necessary. Without it the mod stops.
	auto skybox = LoadBundleMaterial(k_SkyboxMaterialName, k_SkyboxShaderName);
	if (!skybox)
	{
		return skybox.GetError();
	}
	_skyboxMaterial = Held(_managed, *skybox);
	_shaderSupported = true;

	// The moon and the test quad are not necessary. A failure stops only that effect.
	if (auto moon = LoadBundleMaterial(k_MoonMaterialName, k_MoonShaderName); moon)
	{
		_moonMaterial = Held(_managed, *moon);
	}
	else
	{
		FailEffect(_moonError, "moon", moon.GetError());
	}
	if (_settings.testQuad)
	{
		if (auto test = LoadBundleMaterial(k_TestMaterialName, "MoA/Sky/Test"); test)
		{
			_material = Held(_managed, *test);
		}
		else
		{
			spdlog::warn("sky mod: no test quad: {}", test.GetError().message);
			_settings.testQuad = false;
		}
	}

	std::array<int32_t, k_SkyUniformCount> ids {};
	for (size_t index = 0; index < k_SkyUniformCount; ++index)
	{
		const std::string name(k_SkyUniformNames[index]);
		auto id = _managed.Call<int32_t>(_bindings.propertyToId, nullptr, {_managed.String(name.c_str())});
		if (!id)
		{
			FailEffect(_uniformError, "shader values", id.GetError());
			return StepResult::Done;
		}
		ids[index] = *id;
	}
	_uniformIds = ids;
	LoadEvilNight();
	return StepResult::Done;
}

void SkyMod::LoadEvilNight()
{
	// The blood moon is not necessary. A failure stops only the blood moon.
	auto id =
	    _managed.Call<int32_t>(_bindings.propertyToId, nullptr, {_managed.String(std::string(k_BloodMoonUniformName).c_str())});
	if (!id)
	{
		FailEffect(_evilError, "evil night", id.GetError());
		return;
	}
	auto blend2 = _managed.Call<int32_t>(_bindings.propertyToId, nullptr, {_managed.String("_Blend2")});
	if (!blend2)
	{
		FailEffect(_evilError, "evil night", blend2.GetError());
		return;
	}
	_blend2Id = *blend2;
	_bloodMoonId = *id;
}

moa::Result<StepResult> SkyMod::PlaceQuad(void* sky)
{
	if (!_settings.testQuad)
	{
		return StepResult::Done;
	}
	auto* moonObject = ReadField<Il2CppObject*>(sky, _bindings.moonObject);
	if (moonObject == nullptr)
	{
		// The game has no moon object in this scene. Wait for a scene with a sky.
		return StepResult::Wait;
	}
	auto skyTransform = _managed.Invoke(_bindings.componentTransform, sky);
	if (!skyTransform || *skyTransform == nullptr)
	{
		return moa::MakeError("the sky has no transform");
	}
	// The quad is a copy of the moon quad of the game, with the material of the bundle. Its parent is the sky,
	// which follows the camera, thus the quad stays at the same place on the sky.
	bool worldPositionStays = false;
	auto quad = _managed.Invoke(_bindings.instantiate, nullptr, {moonObject, *skyTransform, &worldPositionStays});
	if (!quad || *quad == nullptr)
	{
		return moa::MakeError("the mod cannot copy the moon quad");
	}
	_quad = Held(_managed, *quad);
	_quadSky = sky;
	_quadCount += 1;
	auto renderer = _managed.Invoke(_bindings.getComponent, *quad, {_bindings.rendererType});
	if (!renderer || *renderer == nullptr)
	{
		return moa::MakeError("the copy of the moon quad has no renderer");
	}
	_quadRenderer = Held(_managed, *renderer);
	// The game hides its moon in the day, and the copy keeps that state. The renderer shows or hides the quad.
	bool active = true;
	if (auto set = _managed.Invoke(_bindings.setActive, *quad, {&active}); !set)
	{
		return set.GetError();
	}
	if (auto set = _managed.Invoke(_bindings.setSharedMaterial, *renderer, {_material.Get()}); !set)
	{
		return set.GetError();
	}
	auto transform = _managed.Invoke(_bindings.componentTransform, *renderer);
	if (!transform || *transform == nullptr)
	{
		return moa::MakeError("the copy of the moon quad has no transform");
	}
	auto placement = PlaceOnSky(k_QuadDistance, k_QuadAltitude, k_QuadHeading);
	if (auto set = _managed.Invoke(_bindings.setLocalPosition, *transform, {&placement.offset}); !set)
	{
		return set.GetError();
	}
	if (auto set = _managed.Invoke(_bindings.setLocalRotation, *transform, {&placement.rotation}); !set)
	{
		return set.GetError();
	}
	spdlog::info("sky mod: the test quad is in the sky");
	return StepResult::Done;
}

void SkyMod::UpdateQuad(void* sky)
{
	if (_quadRenderer.Get() == nullptr)
	{
		return;
	}
	const bool show = _forceShow || IsNight(ReadField<float>(sky, _bindings.dayNightCycle));
	if (_quadShown != show)
	{
		bool enabled = show;
		if (auto set = _managed.Invoke(_bindings.setEnabled, _quadRenderer.Get(), {&enabled}); !set)
		{
			_status.SetError(set.GetError().message);
			return;
		}
		_quadShown = show;
	}
}

std::optional<std::string> SkyMod::StandBackReason() const
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

moa::Result<std::string> SkyMod::ShaderName(Il2CppObject* material) const
{
	auto shader = _managed.Invoke(_bindings.getShader, material);
	if (!shader)
	{
		return shader.GetError();
	}
	if (*shader == nullptr)
	{
		return std::string();
	}
	auto name = _managed.Invoke(_bindings.objectName, *shader);
	if (!name)
	{
		return name.GetError();
	}
	return _managed.Text(*name);
}

moa::Result<bool> SkyMod::UpdateSkybox()
{
	if (_skyboxTestOff)
	{
		RestoreSkybox();
		return false;
	}
	auto current = _managed.Invoke(_bindings.getSkybox, nullptr);
	if (!current)
	{
		return current.GetError();
	}
	if (*current == _skyboxMaterial.Get())
	{
		_skyboxSwapped = _gameSkybox.Get() != nullptr;
		return true;
	}
	_skyboxSwapped = false;
	if (*current == nullptr)
	{
		// The sky script of the game sets its skybox in its next update.
		return false;
	}
	auto shaderName = ShaderName(*current);
	if (!shaderName)
	{
		return shaderName.GetError();
	}
	if (*shaderName != k_GameSkyboxShaderName)
	{
		// This scene has another sky. The mod does not change it.
		return false;
	}
	// The skybox of the game: copy its cubemaps and its blend values into the skybox of the mod, then use it.
	auto* ours = _skyboxMaterial.Get();
	for (const auto property : k_SkyboxTextures)
	{
		const std::string name(property);
		auto texture = _managed.Invoke(_bindings.getTexture, *current, {_managed.String(name.c_str())});
		if (!texture)
		{
			return texture.GetError();
		}
		if (*texture == nullptr)
		{
			return moa::MakeError("the skybox of the game has no texture '{}'", name);
		}
		if (auto set = _managed.Invoke(_bindings.setTexture, ours, {_managed.String(name.c_str()), *texture}); !set)
		{
			return set.GetError();
		}
	}
	for (const auto property : k_SkyboxFloats)
	{
		const std::string name(property);
		auto value = _managed.Call<float>(_bindings.getFloat, *current, {_managed.String(name.c_str())});
		if (!value)
		{
			return value.GetError();
		}
		float copy = *value;
		if (auto set = _managed.Invoke(_bindings.setFloat, ours, {_managed.String(name.c_str()), &copy}); !set)
		{
			return set.GetError();
		}
	}
	if (auto set = _managed.Invoke(_bindings.setSkybox, nullptr, {ours}); !set)
	{
		return set.GetError();
	}
	_gameSkybox = Held(_managed, *current);
	_skyboxSwapped = true;
	_skyboxSwaps += 1;
	spdlog::info("sky mod: the skybox of the mod replaces the skybox of the game");
	return true;
}

void SkyMod::RestoreSkybox()
{
	if (_gameSkybox.Get() == nullptr)
	{
		_skyboxSwapped = false;
		return;
	}
	// Put the skybox of the game back only if the skybox of the mod is in use. Another scene can have its own sky.
	auto current = _managed.Invoke(_bindings.getSkybox, nullptr);
	if (current && *current == _skyboxMaterial.Get())
	{
		if (auto set = _managed.Invoke(_bindings.setSkybox, nullptr, {_gameSkybox.Get()}); !set)
		{
			spdlog::warn("sky mod: the mod cannot put the skybox of the game back: {}", set.GetError().message);
			return;
		}
		spdlog::info("sky mod: the skybox of the game is back");
	}
	_gameSkybox.Reset();
	_skyboxSwapped = false;
}

moa::Result<bool> SkyMod::UpdateMoon(void* sky)
{
	auto* moonObject = ReadField<Il2CppObject*>(sky, _bindings.moonObject);
	if (moonObject == nullptr)
	{
		return false;
	}
	auto renderer = _managed.Invoke(_bindings.getComponent, moonObject, {_bindings.rendererType});
	if (!renderer)
	{
		return renderer.GetError();
	}
	if (*renderer == nullptr)
	{
		return moa::MakeError("the moon object has no renderer");
	}
	auto shared = _managed.Invoke(_bindings.getSharedMaterial, *renderer);
	if (!shared)
	{
		return shared.GetError();
	}
	// The game asks the renderer for its material in each frame. Unity then makes a copy of the material of the
	// mod, with the same shader. A material with the shader of the mod needs no change.
	if (*shared != nullptr && *renderer == _moonRenderer.Get())
	{
		auto name = ShaderName(*shared);
		if (!name)
		{
			return name.GetError();
		}
		if (*name == k_MoonShaderName)
		{
			if (_restartMoonRenderer)
			{
				_restartMoonRenderer = false;
				bool off = false;
				bool on = true;
				if (auto set = _managed.Invoke(_bindings.setEnabled, *renderer, {&off}); !set)
				{
					return set.GetError();
				}
				if (auto set = _managed.Invoke(_bindings.setEnabled, *renderer, {&on}); !set)
				{
					return set.GetError();
				}
				spdlog::info("sky mod: the renderer of the moon quad is off and on again (test)");
			}
			return true;
		}
	}
	if (auto set = _managed.Invoke(_bindings.setSharedMaterial, *renderer, {_moonMaterial.Get()}); !set)
	{
		return set.GetError();
	}
	_gameMoonMaterial = Held(_managed, *shared);
	_moonRenderer = Held(_managed, *renderer);
	_moonSwaps += 1;
	spdlog::info("sky mod: the moon material of the mod replaces the moon material of the game");
	return true;
}

void SkyMod::RestoreMoon()
{
	if (_moonRenderer.Get() == nullptr)
	{
		return;
	}
	if (auto set = _managed.Invoke(_bindings.setSharedMaterial, _moonRenderer.Get(), {_gameMoonMaterial.Get()}); !set)
	{
		// The scene of the moon quad is gone. There is nothing to put back.
		spdlog::warn("sky mod: the mod cannot put the moon material back: {}", set.GetError().message);
	}
	else
	{
		spdlog::info("sky mod: the moon material of the game is back");
	}
	_moonRenderer.Reset();
	_gameMoonMaterial.Reset();
}

const SkyUniforms& SkyMod::CurrentUniforms()
{
	const auto now = std::chrono::system_clock::now();
	if (!_uniforms || _uniformsDirty || now - _uniformTime >= k_UniformInterval || now < _uniformTime)
	{
		const auto julianDay = moa::astro::JulianDay(now);
		// The alignment changes only with the actions of the player, so one read each second is enough.
		if (auto reading = ReadAlignment(); reading)
		{
			_alignment = *reading;
			_alignmentError.clear();
		}
		else
		{
			if (_alignmentError != reading.GetError().message)
			{
				spdlog::info("sky mod: no alignment, the sky is neutral: {}", reading.GetError().message);
			}
			_alignment.reset();
			_alignmentError = reading.GetError().message;
		}
		_themeTarget = _themeTest ? std::clamp(*_themeTest, -1.0, 1.0)
		                          : ThemeTarget(_settings.useGameAlignment, _settings.alignment, _alignment);
		_depthOfFieldLevel = DepthOfFieldLevel();
		_uniforms = ComputeSkyUniforms({
		    .julianDay = julianDay,
		    .observer = {.latitude = _place.latitude, .longitude = _place.longitude},
		    .northDegrees = _place.northDegrees,
		    .effects = EffectsOf(_settings),
		    .moon = moa::astro::ComputeMoon(julianDay),
		    .theme = _themeEase.current.value_or(_themeTarget),
		    .depthOfFieldLevel = _depthOfFieldLevel,
		});
		_uniformTime = now;
		_uniformsDirty = false;
		_uniformsSet = false;
	}
	return *_uniforms;
}

moa::Result<bool> SkyMod::UploadUniforms(const SkyUniforms& uniforms)
{
	if (!_uniformIds)
	{
		return moa::MakeError("the mod has no property identifiers");
	}
	for (size_t index = 0; index < k_SkyUniformCount; ++index)
	{
		auto id = (*_uniformIds)[index];
		auto value = uniforms.values[index];
		if (index == static_cast<size_t>(SkyUniform::MoonWorld))
		{
			value.w *= _moonBrightnessScale;
		}
		if (index == static_cast<size_t>(SkyUniform::MoonParams) && _moonDebug > 0)
		{
			// The moon shader reads a test mode from a value below zero.
			value.z = -static_cast<float>(_moonDebug);
		}
		if (auto set = _managed.Invoke(_bindings.setGlobalVector, nullptr, {&id, &value}); !set)
		{
			return set.GetError();
		}
	}
	_uniformsSet = true;
	return true;
}

moa::Result<AlignmentReading> SkyMod::ReadAlignment() const
{
	if (_bindings.getAlignment == nullptr || _bindings.evilLimit == 0 || _bindings.goodLimit == 0)
	{
		return moa::MakeError("the game has no alignment for the mod: {}", _bindings.alignmentError);
	}
	auto alignment = _managed.SingletonInstance("AlignmentManager");
	if (!alignment)
	{
		return alignment.GetError();
	}
	auto value = _managed.Call<float>(_bindings.getAlignment, *alignment);
	if (!value)
	{
		return value.GetError();
	}
	auto manager = _managed.SingletonInstance("NGManager");
	if (!manager)
	{
		return manager.GetError();
	}
	return AlignmentReading {.value = *value,
	                         .evilLimit = ReadField<float>(*manager, _bindings.evilLimit),
	                         .goodLimit = ReadField<float>(*manager, _bindings.goodLimit)};
}

int SkyMod::DepthOfFieldLevel() const
{
	if (_bindings.depthOfFieldLevel == nullptr)
	{
		return 0;
	}
	auto level = _managed.Call<int32_t>(_bindings.depthOfFieldLevel, nullptr);
	return level ? *level : 0;
}

void SkyMod::UpdateTheme()
{
	if (!_uniformIds)
	{
		return;
	}
	const auto now = std::chrono::steady_clock::now();
	const auto seconds = _themeEase.current ? std::chrono::duration<double>(now - _themeTime).count() : 0.0;
	_themeTime = now;
	if (_themeSnap)
	{
		_themeEase.current = _themeTarget;
		_themeSnap = false;
	}
	const auto theme = _themeEase.Step(_themeTarget, seconds);
	auto value = ThemeUniform(EffectsOf(_settings), theme);
	if (_themeUploaded && *_themeUploaded == value)
	{
		return;
	}
	auto id = (*_uniformIds)[static_cast<size_t>(SkyUniform::ThemeParams)];
	if (auto set = _managed.Invoke(_bindings.setGlobalVector, nullptr, {&id, &value}); !set)
	{
		FailEffect(_uniformError, "shader values", set.GetError());
		return;
	}
	if (_uniforms)
	{
		(*_uniforms)[SkyUniform::ThemeParams] = value;
	}
	_themeUploaded = value;
}

void SkyMod::StandBack()
{
	RestoreSkybox();
	RestoreMoon();
	if (_uniformIds && _uniformError.empty())
	{
		// Zero values turn every effect off, also in a material of the mod that stays in use.
		if (auto zero = UploadUniforms(SkyUniforms {}); !zero)
		{
			spdlog::warn("sky mod: the mod cannot turn the shader values off: {}", zero.GetError().message);
		}
		_uniformsSet = false;
		_uniformsDirty = true;
		_themeUploaded.reset();
	}
	if (_bloodMoonId && _evilError.empty())
	{
		if (auto zero = UploadBloodMoon(Vector4 {}); !zero)
		{
			spdlog::warn("sky mod: the mod cannot turn the blood moon off: {}", zero.GetError().message);
		}
	}
	_bloodMoonLast = {};
}

moa::Result<bool> SkyMod::SkyboxTest(bool on)
{
	if (_skyboxMaterial.Get() == nullptr)
	{
		return moa::MakeError("the skybox of the mod is not loaded");
	}
	_skyboxTestOff = !on;
	if (!on)
	{
		RestoreSkybox();
		return false;
	}
	auto result = UpdateSkybox();
	if (!result)
	{
		return result.GetError();
	}
	return _skyboxSwapped;
}

void SkyMod::Restore()
{
	StandBack();
	if (_quad.Get() != nullptr)
	{
		if (auto destroyed = _managed.Invoke(_bindings.destroy, nullptr, {_quad.Get()}); !destroyed)
		{
			spdlog::warn("sky mod: the mod cannot remove the test quad: {}", destroyed.GetError().message);
		}
	}
	_quadRenderer.Reset();
	_quad.Reset();
	_material.Reset();
	_skyboxMaterial.Reset();
	_moonMaterial.Reset();
	_request.Reset();
	if (_bundle.Get() != nullptr)
	{
		bool unloadAll = true;
		if (auto unloaded = _managed.Invoke(_bindings.unloadBundle, _bundle.Get(), {&unloadAll}); !unloaded)
		{
			spdlog::warn("sky mod: the mod cannot unload the bundle: {}", unloaded.GetError().message);
		}
	}
	_bundle.Reset();
	if (_stage != LoadStage::Failed)
	{
		_stage = LoadStage::Failed;
		_error = "the mod was removed";
	}
}

nlohmann::json SkyMod::ThemeState() const
{
	return {
	    {"read", _alignment.has_value()},
	    {"value", _alignment ? _alignment->value : 0.0},
	    {"evilLimit", _alignment ? _alignment->evilLimit : 0.0},
	    {"goodLimit", _alignment ? _alignment->goodLimit : 0.0},
	    {"error", _alignmentError},
	    {"target", _themeTarget},
	    {"theme", _themeEase.current.value_or(0.0)},
	    {"auroraLight", AuroraLightForTheme(_themeEase.current.value_or(0.0))},
	    {"depthOfFieldLevel", _depthOfFieldLevel},
	    {"test", _themeTest ? nlohmann::json(*_themeTest) : nlohmann::json()},
	};
}

nlohmann::json SkyMod::State() const
{
	auto uniforms = nlohmann::json::object();
	if (_uniforms)
	{
		for (size_t index = 0; index < k_SkyUniformCount; ++index)
		{
			const auto& v = _uniforms->values[index];
			uniforms[std::string(k_SkyUniformNames[index])] = {v.x, v.y, v.z, v.w};
		}
	}
	return {
	    {"stage", StageName(_stage)},
	    {"error", _error},
	    {"bundleFile", _bundleFile.string()},
	    {"embeddedBytes", EmbeddedBundle().size()},
	    {"fileWritten", _fileWritten},
	    {"waitFrames", _waitFrames},
	    {"bundleLoaded", _bundle.Get() != nullptr},
	    {"shaderSupported", _shaderSupported},
	    {"standBackReason", _standBackReason.value_or("")},
	    {"skybox",
	     {{"loaded", _skyboxMaterial.Get() != nullptr},
	      {"swapped", _skyboxSwapped},
	      {"testOff", _skyboxTestOff},
	      {"swaps", _skyboxSwaps},
	      {"error", _skyboxError}}},
	    {"moon",
	     {{"loaded", _moonMaterial.Get() != nullptr},
	      {"swapped", _moonRenderer.Get() != nullptr},
	      {"swaps", _moonSwaps},
	      {"error", _moonError}}},
	    {"uniformsSet", _uniformsSet},
	    {"uniformError", _uniformError},
	    {"uniforms", std::move(uniforms)},
	    {"settings",
	     {{"stars", _settings.stars},
	      {"starBrightness", _settings.starBrightness},
	      {"aurora", AuroraModeName(_settings.aurora)},
	      {"moonShader", _settings.moonShader},
	      {"alignmentTheme", _settings.alignmentTheme},
	      {"useGameAlignment", _settings.useGameAlignment},
	      {"alignment", _settings.alignment},
	      {"goodStrength", _settings.goodStrength},
	      {"evilStrength", _settings.evilStrength},
	      {"testQuad", _settings.testQuad},
	      {"latitude", _place.latitude},
	      {"longitude", _place.longitude},
	      {"northDegrees", _place.northDegrees}}},
	    {"theme", ThemeState()},
	    {"evil", EvilState()},
	    {"testQuad", _quad.Get() != nullptr},
	    {"quadShown", _quadShown.value_or(false)},
	    {"forceShow", _forceShow},
	    {"moonDebug", _moonDebug},
	    {"moonBrightnessScale", _moonBrightnessScale},
	    {"quadCount", _quadCount},
	};
}

void SkyMod::UpdateEvilNight()
{
	if (!_bloodMoonId)
	{
		return;
	}
	const auto theme = _themeEase.current.value_or(0.0);
	_nightBlend = 0.0;
	if (EvilAmount(theme) > 0.0 && _skyboxSwapped && _bindings.getFloatById != nullptr)
	{
		// The sky script of the game writes its night blend into the skybox of the mod.
		auto id = _blend2Id;
		if (auto blend = _managed.Call<float>(_bindings.getFloatById, _skyboxMaterial.Get(), {&id}); blend)
		{
			_nightBlend = *blend;
		}
	}
	_bloodMoonLast = BloodMoonUniform(theme, _nightBlend, _settings.evil);
	if (auto result = UploadBloodMoon(_bloodMoonLast); !result)
	{
		FailEffect(_evilError, "evil night", result.GetError());
	}
}

moa::Result<bool> SkyMod::UploadBloodMoon(const Vector4& value)
{
	if (!_bloodMoonId)
	{
		return moa::MakeError("the mod has no property identifier for the blood moon");
	}
	if (_bloodMoonUploaded == value)
	{
		return true;
	}
	auto id = *_bloodMoonId;
	auto copy = value;
	if (auto set = _managed.Invoke(_bindings.setGlobalVector, nullptr, {&id, &copy}); !set)
	{
		_bloodMoonUploaded.reset();
		return set.GetError();
	}
	_bloodMoonUploaded = value;
	return true;
}

nlohmann::json SkyMod::EvilState() const
{
	const auto& s = _settings.evil;
	const auto& v = _bloodMoonLast;
	return {
	    {"error", _evilError},
	    {"bindings", _bindings.evilError},
	    {"evil", EvilAmount(_themeEase.current.value_or(0.0))},
	    {"nightBlend", _nightBlend},
	    {"uniforms", {{std::string(k_BloodMoonUniformName), {v.x, v.y, v.z, v.w}}}},
	    {"settings", {{"bloodMoon", s.bloodMoon}, {"bloodMoonStrength", s.bloodMoonStrength}}},
	};
}

} // namespace moa_mod::sky
