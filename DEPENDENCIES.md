# Build dependencies

## CommonLibSSE-NG

- Repository: https://github.com/alandtse/CommonLibSSE-NG
- Release: v6.1.0
- Commit: `b1e0e03`
- License: GPL-3.0-or-later WITH Modding Exception AND GPL-3.0 Linking Exception (with Corresponding Source)
- Exact source: https://github.com/alandtse/CommonLibSSE-NG/tree/v6.1.0

Place the exact checkout at `lib/commonlibsse-ng`, or set `COMMONLIB_SSE_FOLDER` to it before building. Do not substitute the frozen CharmedBaryon 3.x lineage.

```powershell
git clone --branch v6.1.0 --depth 1 https://github.com/alandtse/CommonLibSSE-NG.git lib/commonlibsse-ng
xmake f -m releasedbg
xmake build ScreenshotLightingAssistant
xmake build lighting-state-tests
./build/tests/lighting-state-tests.exe
```

The root `LICENSE` contains the GPL-3.0 text. CommonLibSSE-NG's exception and original MIT notice are under `third_party/CommonLibSSE-NG/`.

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
