#pragma once

#include "LightingState.h"
#include <filesystem>

namespace ScreenshotLightingAssistant
{
    struct SavedLight
    {
        std::string name;
        int type{};
        std::optional<LightTint> tint;
        std::filesystem::path file;
    };
    struct SavedScene
    {
        std::string name;
        Scene scene;
        bool includeFace{};
        std::filesystem::path file;
    };

    // No handles, actor identity, camera transform, or master running state on disk.
    // New registrations never overwrite files. Explicit rename backs up and atomically
    // replaces the same file so favorites/order retain their stable identity.
    class PresetLibrary
    {
    public:
        static constexpr std::size_t kEntryLimit = 128; // per category
        static constexpr std::size_t kFavoriteLimit = 5;
        explicit PresetLibrary(std::filesystem::path directory) : directory_(std::move(directory)) {}
        void Load();
        bool SaveLight(std::string name, const LightSettings& light);
        bool SaveScene(std::string name, const Scene& scene, bool includeFace);
        bool RenameLight(std::size_t index, std::string name);
        bool RenameScene(std::size_t index, std::string name);
        bool ArchiveLight(std::size_t index);
        bool ArchiveScene(std::size_t index);
        std::vector<std::size_t> Favorites() const;
        bool IsFavorite(std::size_t index) const;
        // A full favorite bar requires an explicit index to replace.
        bool SetFavorite(std::size_t index, bool favorite, std::optional<std::size_t> replace = {});
        bool MoveScene(std::size_t index, int delta);
        bool IsSampleHidden(const std::string& id) const;
        bool SetSampleHidden(const std::string& id, bool hidden);
        const auto& ViewError() const { return viewError_; }
        const auto& Lights() const { return lights_; }
        const auto& Scenes() const { return scenes_; }
        const auto& Directory() const { return directory_; }
        const auto& Error() const { return error_; }
        static bool ValidName(const std::string& name);
        static std::string Encode(const SavedLight& entry);
        static std::string Encode(const SavedScene& entry);
        static std::optional<SavedLight> DecodeLight(const std::string& data);
        static std::optional<SavedScene> DecodeScene(const std::string& data);
        static bool Apply(const SavedScene& entry, LightingEditor& editor)
        {
            auto scene = entry.scene;
            if (!entry.includeFace) { scene.face = editor.GetScene().face; }
            return editor.ApplyScene(scene, entry.name);
        }
    private:
        bool WriteNew(const std::string& data, const char* extension, std::filesystem::path& path);
        bool Archive(const std::filesystem::path& path);
        bool ReplaceName(const std::filesystem::path& path, const std::string& expected, const std::string& replacement);
        struct ViewState {
            std::vector<std::string> favorites;
            std::vector<std::string> sceneOrder;
            std::vector<std::string> hiddenSamples;
        };
        static std::string FileID(const std::filesystem::path& file);
        void LoadView();
        bool SaveView(const ViewState& next);
        ViewState view_;
        bool viewWritable_{true};
        std::string viewError_;
        std::filesystem::path directory_;
        std::vector<SavedLight> lights_;
        std::vector<SavedScene> scenes_;
        std::string error_;
    };
}
