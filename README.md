# Masters of Albion: better skybox

A mod DLL (`better_skybox.dll`) for the mod loader of Masters of Albion (the project `mod_loader`).

- **The real moon!** Dynamic phase, placement, and light of the moon using the OS clock.
- **The night sky!** Adds stars, auroras.
- **Player alignment** affects the skybox and the moon. Auroras appear if you're good and the moon turns blood-red
  if you're evil.


![Colourful Aurora](https://github.com/user-attachments/assets/e2bb7876-27f1-4d1b-bfc8-e8c0e90ca2eb)
*A colourful aurora from being good.*
![Evil night sky](https://github.com/user-attachments/assets/7199aabe-bca4-4b1a-a3ba-26fc9fb146bd)
*A red night sky from being evil.*
![Neutral night sky](https://github.com/user-attachments/assets/26a9ad46-2f58-4261-b296-3cfea10f83e0)
*A neutral night sky with realistic stars and moon.*

## Build

Requirements: Visual Studio with the C++ tools, and the vcpkg checkout of Masters of Albion Reforged.

```bat
git -c protocol.file.allow=always submodule update --init
tools\preset.bat x64-release
```

## Tests

`ctest --preset x64-release` runs the GoogleTest tests.

## How to install

The DLL goes into `<Steam game>\mods\`, next to the loader `version.dll` in the game folder.

```bat
python ..\mod_loader\tools\install.py mods add build\x64-release\better_skybox.dll
```
