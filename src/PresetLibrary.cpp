#include "PresetLibrary.h"
#include "ScenePresets.h"
#include <Windows.h>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace ScreenshotLightingAssistant
{
    namespace
    {
        constexpr std::size_t kMaxBytes = 65536;
        std::string UniqueName()
        {
            static std::atomic_uint64_t sequence{};
            return std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + "-" +
                std::to_string(GetCurrentProcessId()) + "-" + std::to_string(sequence++);
        }
        void WriteTint(std::ostream& out, const std::optional<LightTint>& tint)
        {
            const auto rgb = tint.value_or(LightTint{1, 1, 1});
            out << bool(tint) << ' ' << rgb.red << ' ' << rgb.green << ' ' << rgb.blue << '\n';
        }
        bool ReadTint(std::istream& in, std::optional<LightTint>& tint)
        {
            bool custom{};
            LightTint rgb{};
            if (!(in >> custom >> rgb.red >> rgb.green >> rgb.blue) || !ValidTint(rgb)) { return false; }
            tint = custom ? std::optional{rgb} : std::nullopt;
            return true;
        }
        std::ostringstream Output(const char* kind, const std::string& name, int version = 1)
        {
            std::ostringstream out;
            out.imbue(std::locale::classic());
            out << std::setprecision(std::numeric_limits<float>::max_digits10);
            out << "SLA " << kind << ' ' << version << '\n' << std::quoted(name) << '\n';
            return out;
        }
        bool Header(std::istream& in, const char* expected, std::string& name, int* sceneVersion = nullptr)
        {
            std::string magic, kind;
            int version{};
            if (!(in >> magic >> kind >> version >> std::quoted(name)) || magic != "SLA" || kind != expected ||
                (version != 1 && !(sceneVersion && version == 2)) || !PresetLibrary::ValidName(name)) { return false; }
            if (sceneVersion) { *sceneVersion = version; }
            return true;
        }
        bool End(std::istream& in) { in >> std::ws; return in.eof() && !in.bad(); }
        std::string ReadFile(const std::filesystem::path& file)
        {
            if (std::filesystem::file_size(file) > kMaxBytes) { return {}; }
            std::ifstream in(file, std::ios::binary);
            if (!in) { return {}; }
            // A second bound also covers growth between file_size and read.
            std::string data(kMaxBytes + 1, '\0');
            in.read(data.data(), static_cast<std::streamsize>(data.size()));
            data.resize(static_cast<std::size_t>(in.gcount()));
            if (in.bad() || data.size() > kMaxBytes) { return {}; }
            return data;
        }
    }

    bool PresetLibrary::ValidName(const std::string& name)
    {
        if (name.empty() || name.size() > 120 || name.find("##") != std::string::npos ||
            name.find_first_not_of(' ') == std::string::npos) { return false; }
        for (const unsigned char c : name) { if (c < 32 || c == 127) { return false; } }
        constexpr UINT utf8CodePage = 65001;
        return MultiByteToWideChar(utf8CodePage, MB_ERR_INVALID_CHARS, name.data(), static_cast<int>(name.size()), nullptr, 0) > 0;
    }

    std::string PresetLibrary::Encode(const SavedLight& entry)
    {
        auto out = Output("LIGHT", entry.name);
        out << entry.type << '\n';
        WriteTint(out, entry.tint);
        return out.str();
    }
    std::string PresetLibrary::Encode(const SavedScene& entry)
    {
        auto face = entry.includeFace ? entry.scene.face : FaceLightSettings{};
        // Excluded face data is deliberately normalized to the legacy Camera value.
        // Changing the new-session default must not upgrade a three-light-only file.
        if (!entry.includeFace) { face.basis = FaceLightBasis::Camera; }
        // Keep legacy camera-only files byte-compatible. Head mode needs a versioned field;
        // old builds reject it safely instead of silently using the wrong placement.
        const int version = face.basis == FaceLightBasis::Camera ? 1 : 2;
        auto out = Output("SCENE", entry.name, version);
        out << entry.includeFace << '\n';
        for (const auto& light : entry.scene) {
            out << light.placed << ' ' << light.enabled << ' ' << light.direction << ' ' << light.type << ' '
                << light.intensity << ' ' << light.range << ' ' << light.distance << ' ' << light.heightOffset << ' '
                << light.castsShadow << ' ' << light.shadowBias << ' ' << static_cast<int>(light.shadowProjection) << ' '
                << light.fine.horizontal << ' ' << light.fine.vertical << ' ' << light.fine.depth << '\n';
            WriteTint(out, light.customTint);
        }
        out << face.enabled << ' ' << face.intensity << ' ' << face.range << ' ' << face.heightOffset << '\n';
        if (version == 2) { out << static_cast<int>(face.basis) << '\n'; }
        return out.str();
    }
    std::optional<SavedLight> PresetLibrary::DecodeLight(const std::string& data)
    {
        if (data.size() > kMaxBytes) { return std::nullopt; }
        std::istringstream in(data);
        in.imbue(std::locale::classic());
        SavedLight entry;
        if (!Header(in, "LIGHT", entry.name) || !(in >> entry.type) || entry.type < 0 || entry.type >= kLightTypeCount ||
            !ReadTint(in, entry.tint) || !End(in)) { return std::nullopt; }
        return entry;
    }
    std::optional<SavedScene> PresetLibrary::DecodeScene(const std::string& data)
    {
        if (data.size() > kMaxBytes) { return std::nullopt; }
        std::istringstream in(data);
        in.imbue(std::locale::classic());
        SavedScene entry;
        int version{};
        if (!Header(in, "SCENE", entry.name, &version) || !(in >> entry.includeFace)) { return std::nullopt; }
        for (auto& light : entry.scene) {
            int projection{};
            if (!(in >> light.placed >> light.enabled >> light.direction >> light.type >> light.intensity >> light.range >>
                light.distance >> light.heightOffset >> light.castsShadow >> light.shadowBias >> projection >>
                light.fine.horizontal >> light.fine.vertical >> light.fine.depth) || !ReadTint(in, light.customTint)) { return std::nullopt; }
            light.shadowProjection = static_cast<ShadowProjection>(projection);
        }
        auto& face = entry.scene.face;
        if (!(in >> face.enabled >> face.intensity >> face.range >> face.heightOffset)) { return std::nullopt; }
        if (version == 2) {
            int basis{};
            if (!(in >> basis)) { return std::nullopt; }
            face.basis = static_cast<FaceLightBasis>(basis);
        } else {
            // SCENE 1 predates the basis field and always meant camera-relative.
            face.basis = FaceLightBasis::Camera;
        }
        if (!LightingEditor::IsValidScene(entry.scene) || !End(in)) { return std::nullopt; }
        return entry;
    }

    void PresetLibrary::Load()
    {
        error_.clear();
        try {
            if (!std::filesystem::exists(directory_)) { lights_.clear(); scenes_.clear(); LoadView(); return; }
            std::vector<std::filesystem::path> files;
            for (const auto& file : std::filesystem::directory_iterator(directory_)) {
                if (file.is_regular_file() && (file.path().extension() == ".slalight" || file.path().extension() == ".slaset")) {
                    files.push_back(file.path());
                }
            }
            std::sort(files.begin(), files.end());
            std::vector<SavedLight> lights;
            std::vector<SavedScene> scenes;
            std::size_t skipped = 0;
            for (const auto& file : files) {
                try {
                    if (file.extension() == ".slalight") {
                        auto entry = DecodeLight(ReadFile(file));
                        if (entry && lights.size() < kEntryLimit) { entry->file = file; lights.push_back(*entry); }
                        else { ++skipped; }
                    } else {
                        auto entry = DecodeScene(ReadFile(file));
                        if (entry && scenes.size() < kEntryLimit) { entry->file = file; scenes.push_back(*entry); }
                        else { ++skipped; }
                    }
                } catch (const std::exception&) { ++skipped; }
            }
            lights_ = std::move(lights);
            scenes_ = std::move(scenes);
            LoadView();
            if (skipped) { error_ = "読み込めない登録が " + std::to_string(skipped) + " 件あります。元のファイルは保持しています。"; }
        } catch (const std::exception&) { error_ = "登録フォルダーを読み込めません。保存先とアクセス権を確認してください。"; }
    }

    bool PresetLibrary::WriteNew(const std::string& data, const char* extension, std::filesystem::path& path)
    {
        try {
            std::filesystem::create_directories(directory_);
            path = directory_ / (UniqueName() + extension);
            auto temporary = path;
            temporary += ".tmp";
            const HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == reinterpret_cast<HANDLE>(static_cast<std::intptr_t>(-1))) { error_ = "登録を書き込めません。保存先とアクセス権を確認してください。"; return false; }
            DWORD written{};
            const bool complete = WriteFile(file, data.data(), static_cast<DWORD>(data.size()), &written, nullptr) &&
                written == data.size() && FlushFileBuffers(file);
            CloseHandle(file);
            // No REPLACE_EXISTING: even a name collision cannot destroy an earlier registration.
            if (!complete || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH)) {
                error_ = "登録を保存できませんでした。既存の登録は変更していません。";
                return false; // leave the owned .tmp for diagnosis; loader ignores it
            }
            return true;
        } catch (const std::exception&) { error_ = "登録を保存できません。保存先と空き容量を確認してください。"; return false; }
    }
    bool PresetLibrary::SaveLight(std::string name, const LightSettings& light)
    {
        error_.clear();
        if (!ValidName(name)) { error_ = "名前を入力してください（UTF-8で120バイト以内、改行・##は使えません）。"; return false; }
        if (lights_.size() >= kEntryLimit) { error_ = "ライト登録は128件までです。不要な登録を外してください。"; return false; }
        Scene probe;
        probe[0] = light;
        if (!light.placed || !LightingEditor::IsValidScene(probe)) { error_ = "配置済みの有効なライトを選んでください。"; return false; }
        if (std::any_of(lights_.begin(), lights_.end(), [&](const auto& entry) { return entry.name == name; })) {
            error_ = "同じ名前が登録されています。別の名前で登録してください。"; return false;
        }
        SavedLight entry{std::move(name), light.type, light.customTint, {}};
        if (!WriteNew(Encode(entry), ".slalight", entry.file)) { return false; }
        lights_.push_back(std::move(entry));
        if (Favorites().size() < kFavoriteLimit && viewWritable_) {
            // Registration is already durable. If preferences fail, report the warning
            // without pretending that the successful registration failed.
            SetFavorite(lights_.size() - 1, true);
        }
        return true;
    }
    bool PresetLibrary::SaveScene(std::string name, const Scene& scene, bool includeFace)
    {
        error_.clear();
        if (!ValidName(name)) { error_ = "名前を入力してください（UTF-8で120バイト以内、改行・##は使えません）。"; return false; }
        if (scenes_.size() >= kEntryLimit) { error_ = "プリセット登録は128件までです。不要な登録を外してください。"; return false; }
        if (!LightingEditor::IsValidScene(scene)) { error_ = "無効なライト設定が含まれています。"; return false; }
        if (std::any_of(scenes_.begin(), scenes_.end(), [&](const auto& entry) { return entry.name == name; })) {
            error_ = "同じ名前が登録されています。別の名前で登録してください。"; return false;
        }
        SavedScene entry{std::move(name), scene, includeFace, {}};
        if (!includeFace) {
            entry.scene.face = {};
            entry.scene.face.basis = FaceLightBasis::Camera;
        }
        if (!WriteNew(Encode(entry), ".slaset", entry.file)) { return false; }
        scenes_.push_back(std::move(entry));
        return true;
    }
    bool PresetLibrary::ReplaceName(const std::filesystem::path& path, const std::string& expected, const std::string& replacement)
    {
        try {
            const auto original = ReadFile(path);
            std::string canonical;
            if (path.extension() == ".slalight") {
                if (const auto decoded = DecodeLight(original)) { canonical = Encode(*decoded); }
            } else if (path.extension() == ".slaset") {
                if (const auto decoded = DecodeScene(original)) { canonical = Encode(*decoded); }
            }
            if (path.parent_path() != directory_ || canonical != expected) {
                error_ = "ファイルが変更されています。一覧を読み込み直してから名前を変更してください。";
                return false;
            }
            // Keep the opaque filename: favorites and scene ordering refer to it.
            // The old contents are backed up before an atomic, flushed replacement.
            const auto archive = directory_ / "Archived";
            std::filesystem::create_directories(archive);
            const auto backup = archive / (UniqueName() + "-before-rename-" + path.filename().string());
            if (!CopyFileW(path.c_str(), backup.c_str(), TRUE)) { throw std::runtime_error("rename backup"); }
            const auto temporary = directory_ / (UniqueName() + ".rename.tmp");
            const HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == reinterpret_cast<HANDLE>(static_cast<std::intptr_t>(-1))) { throw std::runtime_error("rename open"); }
            DWORD written{};
            const bool complete = WriteFile(file, replacement.data(), static_cast<DWORD>(replacement.size()), &written, nullptr) &&
                written == replacement.size() && FlushFileBuffers(file);
            CloseHandle(file);
            if (!complete || ReadFile(path) != original ||
                !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                throw std::runtime_error("rename commit");
            }
            return true;
        } catch (const std::exception&) {
            error_ = "名前を変更できませんでした。保存先とアクセス権を確認してください。";
            return false;
        }
    }
    bool PresetLibrary::RenameLight(std::size_t index, std::string name)
    {
        error_.clear();
        if (index >= lights_.size()) { error_ = "登録が見つかりません。"; return false; }
        if (!ValidName(name)) { error_ = "名前を入力してください（UTF-8で120バイト以内、改行・##は使えません）。"; return false; }
        auto& entry = lights_[index];
        if (entry.name == name) { return true; }
        if (std::any_of(lights_.begin(), lights_.end(), [&](const auto& item) { return item.name == name; })) {
            error_ = "同じ名前が登録されています。別の名前で登録してください。"; return false;
        }
        auto next = entry;
        next.name = std::move(name);
        if (!ReplaceName(entry.file, Encode(entry), Encode(next))) { return false; }
        entry = std::move(next);
        return true;
    }
    bool PresetLibrary::RenameScene(std::size_t index, std::string name)
    {
        error_.clear();
        if (index >= scenes_.size()) { error_ = "登録が見つかりません。"; return false; }
        if (!ValidName(name)) { error_ = "名前を入力してください（UTF-8で120バイト以内、改行・##は使えません）。"; return false; }
        auto& entry = scenes_[index];
        if (entry.name == name) { return true; }
        if (std::any_of(scenes_.begin(), scenes_.end(), [&](const auto& item) { return item.name == name; })) {
            error_ = "同じ名前が登録されています。別の名前で登録してください。"; return false;
        }
        auto next = entry;
        next.name = std::move(name);
        if (!ReplaceName(entry.file, Encode(entry), Encode(next))) { return false; }
        entry = std::move(next);
        return true;
    }
    bool PresetLibrary::Archive(const std::filesystem::path& path)
    {
        error_.clear();
        try {
            if (path.parent_path() != directory_) { return false; }
            const auto archive = directory_ / "Archived";
            std::filesystem::create_directories(archive);
            const auto target = archive / (UniqueName() + path.extension().string());
            if (MoveFileExW(path.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH)) { return true; }
        } catch (const std::exception&) {}
        error_ = "登録を外せませんでした。ファイルと一覧は保持しています。";
        return false;
    }
    bool PresetLibrary::ArchiveLight(std::size_t index)
    {
        if (index >= lights_.size() || !Archive(lights_[index].file)) { return false; }
        lights_.erase(lights_.begin() + index);
        return true;
    }
    bool PresetLibrary::ArchiveScene(std::size_t index)
    {
        if (index >= scenes_.size() || !Archive(scenes_[index].file)) { return false; }
        scenes_.erase(scenes_.begin() + index);
        return true;
    }

    std::string PresetLibrary::FileID(const std::filesystem::path& file)
    {
        const auto utf8 = file.filename().u8string();
        return {reinterpret_cast<const char*>(utf8.data()), utf8.size()};
    }

    void PresetLibrary::LoadView()
    {
        view_ = {};
        viewError_.clear();
        viewWritable_ = true;
        try {
            // Preserve both older files for rollback. Never fall back past a
            // present but invalid newer generation, or silently refill favorites.
            const int generation = std::filesystem::exists(directory_ / "library-view-v3.sla") ? 3 :
                (std::filesystem::exists(directory_ / "library-view-v2.sla") ? 2 : 1);
            const auto file = directory_ / (generation == 3 ? "library-view-v3.sla" :
                (generation == 2 ? "library-view-v2.sla" : "library-view.sla"));
            if (!std::filesystem::exists(file)) {
                // Migration: preserve the first three old quick-access entries.
                for (std::size_t i = 0; i < std::min<std::size_t>(3, lights_.size()); ++i) {
                    view_.favorites.push_back(FileID(lights_[i].file));
                }
                return; // reading never creates or rewrites files
            }
            std::istringstream in(ReadFile(file));
            in.imbue(std::locale::classic());
            std::string magic, kind;
            int version{};
            ViewState next;
            const auto readList = [&](std::vector<std::string>& list, std::size_t limit) {
                std::size_t count{};
                if (!(in >> count) || count > limit) { return false; }
                for (std::size_t i = 0; i < count; ++i) {
                    std::string id;
                    if (!(in >> std::quoted(id)) || !ValidName(id) ||
                        id.find_first_of("/\\:") != std::string::npos || id == "." || id == ".." ||
                        std::find(list.begin(), list.end(), id) != list.end()) { return false; }
                    list.push_back(std::move(id));
                }
                return true;
            };
            if (!(in >> magic >> kind >> version) || magic != "SLA" || kind != "VIEW" || version != generation ||
                !readList(next.favorites, generation == 1 ? 3 : (generation == 2 ? 4 : kFavoriteLimit)) || !readList(next.sceneOrder, kEntryLimit) ||
                !readList(next.hiddenSamples, kEntryLimit) || !End(in)) {
                throw std::runtime_error("invalid view metadata");
            }
            // File IDs are opaque lookup keys, never paths to open or delete.
            std::erase_if(next.favorites, [&](const auto& id) {
                return std::none_of(lights_.begin(), lights_.end(), [&](const auto& light) { return FileID(light.file) == id; });
            });
            const auto rank = [&](const SavedScene& scene) {
                return std::find(next.sceneOrder.begin(), next.sceneOrder.end(), FileID(scene.file)) - next.sceneOrder.begin();
            };
            std::stable_sort(scenes_.begin(), scenes_.end(), [&](const auto& a, const auto& b) { return rank(a) < rank(b); });
            view_ = std::move(next);
        } catch (const std::exception&) {
            viewWritable_ = false;
            viewError_ = "一覧設定を読み込めません。既存の設定ファイルを保持し、一覧設定の保存を停止しています。登録データは利用できます。";
        }
    }

    bool PresetLibrary::SaveView(const ViewState& next)
    {
        if (!viewWritable_) { return false; }
        try {
            if (next.favorites.size() > kFavoriteLimit || next.sceneOrder.size() > kEntryLimit || next.hiddenSamples.size() > kEntryLimit) {
                throw std::runtime_error("view bounds");
            }
            std::ostringstream out;
            out.imbue(std::locale::classic());
            out << "SLA VIEW 3\n";
            for (const auto* list : {&next.favorites, &next.sceneOrder, &next.hiddenSamples}) {
                out << list->size() << '\n';
                for (const auto& id : *list) {
                    if (!ValidName(id) || id.find_first_of("/\\:") != std::string::npos || id == "." || id == "..") {
                        throw std::runtime_error("view id");
                    }
                    out << std::quoted(id) << '\n';
                }
            }
            const auto data = out.str();
            std::filesystem::create_directories(directory_);
            const auto destination = directory_ / "library-view-v3.sla";
            const auto temporary = directory_ / (UniqueName() + ".view.tmp");
            const HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == reinterpret_cast<HANDLE>(static_cast<std::intptr_t>(-1))) { throw std::runtime_error("view open"); }
            DWORD written{};
            const bool complete = WriteFile(file, data.data(), static_cast<DWORD>(data.size()), &written, nullptr) &&
                written == data.size() && FlushFileBuffers(file);
            CloseHandle(file);
            // Replace only our view metadata, after flushing a complete unique temporary.
            // Registration files and their formats are untouched.
            if (!complete || !MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                throw std::runtime_error("view commit");
            }
            view_ = next;
            viewError_.clear();
            return true;
        } catch (const std::exception&) {
            viewError_ = "一覧設定を保存できませんでした。お気に入り・並び順・表示設定は変更していません。";
            return false;
        }
    }

    std::vector<std::size_t> PresetLibrary::Favorites() const
    {
        std::vector<std::size_t> result;
        for (const auto& id : view_.favorites) {
            for (std::size_t i = 0; i < lights_.size(); ++i) {
                if (FileID(lights_[i].file) == id) { result.push_back(i); break; }
            }
        }
        return result;
    }
    bool PresetLibrary::IsFavorite(std::size_t index) const
    {
        if (index >= lights_.size()) { return false; }
        const auto id = FileID(lights_[index].file);
        return std::find(view_.favorites.begin(), view_.favorites.end(), id) != view_.favorites.end();
    }
    bool PresetLibrary::SetFavorite(std::size_t index, bool favorite, std::optional<std::size_t> replace)
    {
        if (index >= lights_.size()) { return false; }
        auto next = view_;
        next.favorites.clear();
        for (const auto i : Favorites()) { next.favorites.push_back(FileID(lights_[i].file)); }
        const auto id = FileID(lights_[index].file);
        auto current = std::find(next.favorites.begin(), next.favorites.end(), id);
        if (!favorite) {
            if (current != next.favorites.end()) { next.favorites.erase(current); }
        } else if (current == next.favorites.end()) {
            if (next.favorites.size() < kFavoriteLimit) { next.favorites.push_back(id); }
            else {
                if (!replace || *replace >= lights_.size()) { return false; }
                auto victim = std::find(next.favorites.begin(), next.favorites.end(), FileID(lights_[*replace].file));
                if (victim == next.favorites.end()) { return false; }
                *victim = id;
            }
        }
        return SaveView(next);
    }
    bool PresetLibrary::MoveScene(std::size_t index, int delta)
    {
        if (index >= scenes_.size() || (delta != -1 && delta != 1)) { return false; }
        const auto target = static_cast<std::ptrdiff_t>(index) + delta;
        if (target < 0 || target >= static_cast<std::ptrdiff_t>(scenes_.size())) { return false; }
        auto next = view_;
        next.sceneOrder.clear();
        for (const auto& scene : scenes_) { next.sceneOrder.push_back(FileID(scene.file)); }
        std::swap(next.sceneOrder[index], next.sceneOrder[target]);
        if (!SaveView(next)) { return false; }
        std::swap(scenes_[index], scenes_[target]);
        return true;
    }
    bool PresetLibrary::IsSampleHidden(const std::string& id) const
    {
        return std::find(view_.hiddenSamples.begin(), view_.hiddenSamples.end(), id) != view_.hiddenSamples.end();
    }
    bool PresetLibrary::SetSampleHidden(const std::string& id, bool hidden)
    {
        if (std::none_of(kScenePresets.begin(), kScenePresets.end(), [&](const auto& p) { return p.id == id; })) { return false; }
        auto next = view_;
        std::erase(next.hiddenSamples, id);
        if (hidden) { next.hiddenSamples.push_back(id); }
        return SaveView(next);
    }
}
