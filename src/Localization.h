#pragma once
#include <atomic>
#include <filesystem>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace ScreenshotLightingAssistant::Localization
{
    enum class Language { Japanese, English };
    struct Translation { const char* japanese; const char* english; };
    Language Current();
    void Set(Language language); // Presentation only; never changes the editor or saved names.
    const char* Tr(const char* key, Language language);
    inline const char* Tr(const char* key) { return Tr(key, Current()); }
    std::span<const Translation> Table();
    std::string StoredError(const std::string& error);
    std::string HistoryText(const std::string& key);
    template<class... Args> std::string Fmt(const char* format, Args&&... args)
    {
        return std::vformat(format, std::make_format_args(args...));
    }
    struct Message
    {
        std::string japanese, english;
        Message() = default;
        Message(const char* key) : japanese(key), english(Tr(key, Language::English)) {}
        Message(std::string ja, std::string en) : japanese(std::move(ja)), english(std::move(en)) {}
        const char* c_str() const { return (Current() == Language::English ? english : japanese).c_str(); }
        bool operator==(const char* key) const { return japanese == key; }
    };
    // Missing preferences do not create files. Invalid files are preserved.
    bool LoadPreference(const std::filesystem::path& path, std::string& error);
    bool SavePreference(const std::filesystem::path& path, Language language, std::string& error);
}

