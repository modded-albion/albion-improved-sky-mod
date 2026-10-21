# ******************************************************************************
# Copyright (c) 2026 Masters of Albion Reforged developers
# ******************************************************************************

# The better skybox mod of Masters of Albion: a mod DLL for the mod loader. The real moon (phase, place and light from
# the clock of the computer), and the night sky of an AssetBundle in the DLL: the real stars, the aurora, the sky of
# the alignment of the player and the blood moon. The DLL uses only the static C runtime and static libraries.

cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

project(
  moa_better_skybox
  VERSION 0.1.0
  LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(MOA_SKY_BUILD_TESTS "Build the tests of the sky mod" ON)

# The shared code (git submodule).
add_subdirectory(external/common)

# The sky logic that does not need the game: the place of the moon, the rules of the sky, the theme of the alignment,
# the evil night, the values of the shaders and the settings.
add_library(
  moa_sky_core STATIC
  src/core/include/moa_mod/moon/MoonPlacement.h
  src/core/include/moa_mod/sky/AuroraMode.h
  src/core/include/moa_mod/sky/EvilNight.h
  src/core/include/moa_mod/sky/EvilNightSettings.h
  src/core/include/moa_mod/sky/SkyRules.h
  src/core/include/moa_mod/sky/SkyTheme.h
  src/core/include/moa_mod/sky/SkyUniforms.h
  src/core/include/moa_mod/sky/SkyboxSettings.h
  src/core/src/sky/EvilNight.cpp
  src/core/src/sky/SkyRules.cpp
  src/core/src/sky/SkyTheme.cpp
  src/core/src/sky/SkyUniforms.cpp
  src/core/src/sky/SkyboxSettings.cpp
)
target_include_directories(moa_sky_core PUBLIC src/core/include)
target_link_libraries(moa_sky_core PUBLIC moa_common moa_astro moa_mod_core)
moa_set_warnings(moa_sky_core)

if (WIN32)
  # The AssetBundle of the sky (unity/moa_sky/build_bundles.py writes data/moa_sky.bundle) goes into the DLL, because
  # the mod folder of the game gets only the DLL. Without the file, the DLL has no bundle and the night sky stays off.
  set(MOA_SKY_BUNDLE ${CMAKE_CURRENT_SOURCE_DIR}/data/moa_sky.bundle)
  set(MOA_SKY_BUNDLE_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/SkyBundleData.cpp)
  set(MOA_SKY_BUNDLE_BYTES "")
  if (EXISTS ${MOA_SKY_BUNDLE})
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${MOA_SKY_BUNDLE})
    file(READ ${MOA_SKY_BUNDLE} MOA_SKY_BUNDLE_HEX HEX)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "std::byte{0x\\1}," MOA_SKY_BUNDLE_BYTES "${MOA_SKY_BUNDLE_HEX}")
  endif ()
  configure_file(cmake/SkyBundleData.cpp.in ${MOA_SKY_BUNDLE_SOURCE} @ONLY)

  add_library(
    better_skybox SHARED
    src/game/moa_mod/moon/MoonMod.cpp
    src/game/moa_mod/moon/MoonMod.h
    src/game/moa_mod/sky/ModMain.cpp
    src/game/moa_mod/sky/SkyMod.cpp
    src/game/moa_mod/sky/SkyMod.h
    ${MOA_SKY_BUNDLE_SOURCE}
  )
  target_include_directories(better_skybox PRIVATE src/game)
  target_link_libraries(better_skybox PRIVATE moa_sky_core moa_mod_game moa_mod_sdk)
  moa_set_warnings(better_skybox)
endif ()

if (MOA_SKY_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif ()
