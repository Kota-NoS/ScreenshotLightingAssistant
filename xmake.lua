set_xmakever("3.0.0")

local plugin_name = "ScreenshotLightingAssistant"
local plugin_version = "0.2.0"
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

includes(commonlib_dir)

target(plugin_name, function()
    set_kind("shared")
    set_targetdir("build/artifacts/" .. plugin_version)

    add_deps("commonlibsse-ng")
    add_rules("commonlibsse-ng.plugin", {
        name = plugin_name,
        author = "kota (@kotaSkyrim) / developed with OpenAI Codex",
        description = "Photography lighting plus an optional persistent player face light for Skyrim.",
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
