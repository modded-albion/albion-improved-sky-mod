# Masters of Albion: better skybox

A mod DLL (`better_skybox.dll`) for the mod loader of Masters of Albion (the project `mod_loader`). It is the moon and
sky work of the research mod of Masters of Albion Reforged (`tools/research_mod`, the `moon` and `sky` parts, and the
Unity project `tools/unity/moa_sky`), moved into its own DLL. That repository stays as it is.

What it does:

- **The real moon** (`moon.enabled`): the phase, the place and the light of the moon come from the clock of the
  computer and the place of the observer (`moon.latitude`, `moon.longitude`, `moon.northDegrees`).
- **The night sky** (`sky.enabled`, after a restart of the game): the skybox of the mod with the real stars, the
  aurora (`off`, `alignment`, `always`), the moon as a lit ball with the real phase, the sky of the alignment of the
  player (black for neutral, a faint rainbow for good, dark red for evil), and the blood moon of the evil night.
- Readouts in the menu Sky: the moon now (phase, lit part, age, altitude, azimuth) and the alignment of the player.

## The AssetBundle

The shaders and materials of the night sky are in an AssetBundle, `data/moa_sky.bundle`. The build puts the bytes of
the file into the DLL (`cmake/SkyBundleData.cpp.in`, made at configure time), because the mod gives the game only
the DLL. At the first frames the mod writes the bundle to `%LOCALAPPDATA%\MastersOfAlbionReforged\moa_sky.bundle`
(outside the game folder) and loads it with `AssetBundle.LoadFromFileAsync`. Without the file at build time, the DLL
has no bundle and the night sky stays off.

The Unity project that makes the bundle is in `unity/moa_sky` (Unity 6000.5.7f1, URP; no `Library` or `Temp`
folder). `python unity/moa_sky/build_bundles.py` builds the bundle in batch mode and copies it to
`data/moa_sky.bundle`. Then configure and compile again. `unity/moa_sky/Data/stars_bsc5.bin` is the star table (from
the Yale Bright Star Catalogue, `Data/make_stars.py`); a test reads it.

## How it uses the loader

The DLL exports `MoaModApiVersion`, `MoaModGetInfo` (id `better_skybox`), `MoaModInit` and `MoaModShutdown`
(`src/game/moa_mod/sky/ModMain.cpp`). In `MoaModInit` it:

- finds the IL2CPP exports and the managed names that it needs itself (common `moa_mod_game`). A missing name of the
  moon stops the init with 0, so the loader unloads the mod and the game continues;
- registers the settings sections `moon` and `sky` of mods.json, the fields of the menu Sky (`SkyboxFields`), the
  hotkey `toggleMoon` (Ctrl+Shift+F6), two readouts and a status reader;
- asks the loader for two callbacks after `Marcos_Procedural_Sky.Update` (the moon, then the sky). The loader
  installs the hook.

The parts start when the loader gave both objects (`moon` and `sky`), before the first frame. `MoaModShutdown` gives
the game its skybox and its moon material back.

## Build

Requirements: Visual Studio with the C++ tools, and the vcpkg checkout of Masters of Albion Reforged.

```bat
git -c protocol.file.allow=always submodule update --init
tools\preset.bat x64-release
```

The result is `build\x64-release\better_skybox.dll`. It imports only `KERNEL32.dll` (static C runtime).
Dependencies: vcpkg in manifest mode (`vcpkg.json`: nlohmann-json, spdlog, GoogleTest) with the triplet
`x64-windows-static`. The presets use `VCPKG_ROOT` = `D:/masters_of_albion/masters_of_albion_reforged/vcpkg` (the
vcpkg checkout of the reforged repository; this repository has no vcpkg submodule), the binary cache
`D:/masters_of_albion/cache/vcpkg-archives` and the downloads `D:/masters_of_albion/cache/vcpkg-downloads`.

## The link to common

`external/common` is a git submodule with the URL `D:/masters_of_albion/projects/common` (an absolute local path).
Git 2.43 refuses local file submodules by default, so each submodule command needs `-c protocol.file.allow=always`.
`CMakeLists.txt` includes it with `add_subdirectory(external/common)`. The moon and star maths (`moa_astro`) is in
common.

## Tests

`ctest --preset x64-release` runs the GoogleTest tests: the place of the moon, the rules of the sky, the theme of the
alignment, the evil night, the values of the shaders, the settings and fields, the moon readout and the star table.

## Install (not done by the build)

The DLL goes into `<Steam game>\mods\`, next to the loader `version.dll` in the game folder. Only the user starts the
install, with the install tool of `mod_loader`:

```bat
python ..\mod_loader\tools\install.py mods add build\x64-release\better_skybox.dll
```
