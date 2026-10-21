/******************************************************************************
 * Copyright (c) 2026 Masters of Albion Reforged developers
 *****************************************************************************/

#pragma once

#include <string_view>

#include <moa_mod/UnityMath.h>
#include <moa_mod/sky/EvilNightSettings.h>

namespace moa_mod::sky
{

/// Return the evil value of the sky for a theme: 0 for a neutral or good theme, 1 for a fully evil theme.
[[nodiscard]] double EvilAmount(double theme);

/// Return the night weight of the evil night for the night blend of the game (`_Blend2`): 0 in the day and in the
/// sunset (the night blend to 0.5), 1 from the night blend 0.9. The stars of the sky mod start earlier (from 0.2), so
/// the sunset of the game does not change.
[[nodiscard]] double EvilNightWeight(double nightBlend);

/// The name of the global shader value of the blood moon. x: the blood moon (0 to 1); y: the scale of the disc; z: the
/// dark rim; w: unused.
inline constexpr std::string_view k_BloodMoonUniformName = "_MoaEvilMoon";

/// Return the shader value of the blood moon for the theme of the sky (-1 evil to +1 good), the night blend of the
/// game (`_Blend2`) and the settings. The value is zero in the day, for a neutral or good player and when the blood
/// moon is off.
[[nodiscard]] Vector4 BloodMoonUniform(double theme, double nightBlend, const EvilNightSettings& settings);

} // namespace moa_mod::sky
