set_xmakever("3.0.0")

local plugin_name = "ScreenshotLightingAssistant"
local plugin_version = "0.3.0"
local commonlib_dir = os.getenv("COMMONLIB_SSE_FOLDER") or "lib/commonlibsse-ng"

set_project(plugin_name)
set_version(plugin_version)
set_languages("c++23")
add_rules("mode.debug", "mode.releasedbg")
set_defaultmode("releasedbg")

set_config("skyrim_se", true)
set_config("skyrim_ae", true)
set_config("skyrim_vr", false)
set_config("rex_ini", false)
set_config("rex_json", false)
set_config("rex_toml", false)
set_config("skse_xbyak", false)

-- CommonLibSSE-NG supports spdlog in header-only form as well.  Keeping it
-- header-only avoids a separate CMake sub-build.  Wide-character spdlog is
-- intentionally disabled here because its Windows macros collide with REX::W32
-- constants when the implementation is expanded in headers; the plugin logs
-- UTF-8 narrow strings and does not use spdlog's wchar API.
add_requireconfs("spdlog", {
    override = true,
    configs = { header_only = true, wchar = false, std_format = true }
})

includes(commonlib_dir)

target("spdlog-compiled", function()
    set_kind("static")
    add_files("third_party/spdlog-source/src/*.cpp|bundled_fmtlib_format.cpp")
    add_includedirs("third_party/spdlog-source/include", { public = true })
    add_defines("SPDLOG_COMPILED_LIB", "SPDLOG_USE_STD_FORMAT", { public = true })
end)

target("commonlibsse-ng", function()
    add_deps("spdlog-compiled")
    add_defines("SPDLOG_COMPILED_LIB", { public = true })
end)

target(plugin_name, function()
    set_kind("shared")
    set_targetdir("build/artifacts/" .. plugin_version)

    add_deps("commonlibsse-ng", "spdlog-compiled")
    add_rules("commonlibsse-ng.plugin", {
        name = plugin_name,
        author = "kota (@kotaSkyrim) / developed with OpenAI Codex",
        description = "Photography lighting, a persistent player face light, and optional photo effects for Skyrim.",
        version = plugin_version,
        options = { address_library = true }
    })

    add_files("src/**.cpp")
    add_headerfiles("src/**.h", "third_party/**.h")
    add_includedirs("src", "third_party/SKSEMenuFramework")
    set_pcxxheader("src/PCH.h")

    add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN")
    add_cxxflags("/utf-8", { force = true })
end)

target("lighting-state-tests", function()
    set_default(false)
    set_kind("binary")
    set_targetdir("build/tests")
    add_files("tests/LightingStateTests.cpp")
    add_files("src/PresetLibrary.cpp")
    add_files("src/Localization.cpp")
    add_files("src/StorageLocation.cpp")
    add_files("src/PersistentFaceSettings.cpp")
    add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN")
    add_includedirs("src")
    add_cxxflags("/utf-8", { force = true })
end)
