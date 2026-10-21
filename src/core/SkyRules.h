/******************************************************************************
 * Copyright (c) 2026 Masters of Albion Reforged developers
 *****************************************************************************/

#pragma once

#include <cstddef>

#include <array>
#include <span>
#include <string_view>

#include <mod_core/UnityMath.h>

/// The parts of the sky mod that do not need the game.
namespace moa_mod::sky
{

/// The name of the file of the AssetBundle in the folder of the mod settings.
inline constexpr std::string_view k_BundleFileName = "moa_sky.bundle";
/// The name of the test material in the AssetBundle.
inline constexpr std::string_view k_TestMaterialName = "MoaSkyTest";
/// The names of the skybox material and the moon material in the AssetBundle, and of their shaders.
inline constexpr std::string_view k_SkyboxMaterialName = "MoaSkybox";
inline constexpr std::string_view k_MoonMaterialName = "MoaMoon";
inline constexpr std::string_view k_SkyboxShaderName = "MoA/Sky/Skybox";
inline constexpr std::string_view k_MoonShaderName = "MoA/Sky/Moon";
/// The shader of the skybox material of the game in the open world. The mod replaces only this skybox.
inline constexpr std::string_view k_GameSkyboxShaderName = "Skybox/Marcos_Sky_Box_Blender";
/// The textures and the values of the skybox of the game. The skybox of the mod has the same names.
inline constexpr std::array<std::string_view, 3> k_SkyboxTextures {"_Tex", "_OverlayTex", "_OverlayTex2"};
inline constexpr std::array<std::string_view, 3> k_SkyboxFloats {"_Blend", "_Blend2", "_Rotation"};

/// The steps that load the AssetBundle. Each step runs in one frame or in more frames, on the main thread.
enum class LoadStage
{
	WriteFile,     ///< Write the bundle bytes to the file, if the file is not the same.
	StartLoad,     ///< Start the asynchronous load of the file.
	WaitForBundle, ///< Wait until the load is complete.
	LoadMaterial,  ///< Load the materials and check their shaders.
	PlaceQuad,     ///< Make the test quad in the sky.
	Ready,         ///< All steps are complete.
	Failed,        ///< A step failed. The mod stops and changes nothing more.
};

/// The result of one step.
enum class StepResult
{
	Done,  ///< The step is complete. The next step starts in the next frame.
	Wait,  ///< The step is not complete. It runs again in the next frame.
	Error, ///< The step failed.
};

/// Return the stage after a step of `stage` gave `result`. `Ready` and `Failed` do not change.
[[nodiscard]] LoadStage NextStage(LoadStage stage, StepResult result);

/// Return the name of a stage, for the log and the status.
[[nodiscard]] std::string_view StageName(LoadStage stage);

/// Return true if the file on the disk holds the same bytes as the bundle in the mod.
[[nodiscard]] bool BundleFileIsCurrent(std::span<const std::byte> onDisk, std::span<const std::byte> embedded);

/// Return true if the sun of the game is below the horizon. `dayNightCycle` is the fraction of the game day from
/// midnight (0 to 1), as the sky of the game keeps it.
[[nodiscard]] bool IsNight(float dayNightCycle);

/// The place of a quad on the sky, relative to the centre of the sky.
struct QuadPlacement
{
	WorldPoint offset;   ///< From the centre of the sky to the quad.
	Quaternion rotation; ///< The front face of the quad (its local +z axis) points to the centre of the sky.
};

/// Return the place of a quad at `distance` from the centre of the sky, at `altitudeDegrees` above the horizon, and
/// at `headingDegrees` clockwise from the world +z axis about the up axis.
[[nodiscard]] QuadPlacement PlaceOnSky(float distance, float altitudeDegrees, float headingDegrees);

} // namespace moa_mod::sky
