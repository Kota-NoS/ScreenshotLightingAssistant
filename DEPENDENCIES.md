# Build dependencies

## CommonLibSSE-NG

- Repository: https://github.com/alandtse/CommonLibSSE-NG
- Release: v8.4.0
- Commit: `adb3e2c4dff61d151370a6777ed9e77f8e005c62`
- License: GPL-3.0-or-later WITH Modding Exception AND GPL-3.0 Linking Exception (with Corresponding Source)
- Exact source: https://github.com/alandtse/CommonLibSSE-NG/tree/v8.4.0

Place the exact checkout at `lib/commonlibsse-ng`, or set `COMMONLIB_SSE_FOLDER` to it before building. Do not substitute the frozen CharmedBaryon 3.x lineage.

```powershell
git clone --branch v8.4.0 --depth 1 https://github.com/alandtse/CommonLibSSE-NG.git lib/commonlibsse-ng
xmake f -m releasedbg
xmake build ScreenshotLightingAssistant
xmake build lighting-state-tests
./build/tests/lighting-state-tests.exe
```

This release is built with SE and AE enabled and VR disabled. CommonLibSSE-NG v8.4.0 supplies Address Library format 5 declarations and runtime definitions through Skyrim 1.7.104. Runtime verification still requires matching SKSE and Address Library data; for Skyrim 1.7.104 use SKSE 2.3.1 and Address Library All in One v13 or newer.

The root `LICENSE` contains this plugin's GPL-3.0 text. CommonLibSSE-NG's exact COPYING, exception and original MIT notice are under `third_party/CommonLibSSE-NG/`.

## spdlog

- Repository: https://github.com/gabime/spdlog
- Release: v1.16.0
- Commit: `486b55554f11c9cccc913e11a87085b2a91f706f`
- License: MIT

The exact headers and compiled-library source files required by this build are vendored under `third_party/spdlog-source/`. The project builds those files directly with xmake so no separate CMake sub-build is required.

## SKSE Menu Framework 3 API header

- Repository: https://github.com/SkyrimScripting/SKSEMenuFramework
- Vendored revision: `3a65dc0147388da177c324cff4d89d9e25094623`
- License: LGPL-2.1

The unmodified API header and license are under `third_party/SKSEMenuFramework/`.

## nifly (asset-generation tool only)

- Repository: https://github.com/SK83RJOSH/nifly
- Commit used: `5504832da8009248ff68a9d306973ee1ba61a0a4`
- License: GPL-3.0

`tools/sparkle_nif_builder` uses nifly to generate and inspect the original
particle-system NIF variants for sparkles, sakura petals, magic stars, autumn leaves, white feathers and pop stars. nifly is not linked into the SKSE plugin DLL and is not
required by players. Pass its checkout as `NIFLY_ROOT` when configuring that CMake
tool. The generated NIFs and this project's original textures are included in the
MO2 package; no ParticleFieldSE assets are used.
