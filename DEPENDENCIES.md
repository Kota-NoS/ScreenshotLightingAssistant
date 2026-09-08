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
