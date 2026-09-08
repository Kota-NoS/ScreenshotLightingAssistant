#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ScreenshotLightingAssistant
{
    inline constexpr int kLightTypeCount = 4;
    // Display order only. Never change serialized type IDs or tint indexing.
    inline constexpr std::array<int, 4> kLightDisplayOrder{0, 1, 3, 2};
    inline constexpr std::size_t kManualLightCount = 3;
    inline constexpr float kWideLightRange = 12.0F;
    inline constexpr float kLightIntensityDefault = 1.0F;
    inline constexpr float kLightRangeDefault = 4.0F;
    inline constexpr float kLightDistanceDefault = 2.2F;
    inline constexpr float kLightHeightDefault = 0.0F;
    inline constexpr float kShadowBiasDefault = 1.0F;
    inline constexpr float kFaceIntensityMax = 3.0F;
    inline constexpr float kFaceRangeMin = 0.5F;
    inline constexpr float kFaceRangeMax = 2.0F;
    inline constexpr float kFaceRangeDefault = 1.2F;
    inline constexpr float kFaceHeightMin = -0.5F;
    inline constexpr float kFaceHeightMax = 0.5F;
    inline constexpr float kFaceHeightDefault = 0.15F;
    // Shadow bias choices, not calibrated softness presets. Keep 1.0 as the baseline.
    inline constexpr std::array<float, 5> kShadowBiasLevels{ 0.5F, 1.0F, 2.0F, 4.0F, 8.0F };
    enum class ShadowProjection { Omni, Spot };
    inline bool ValidProjection(ShadowProjection projection)
    {
        return projection == ShadowProjection::Omni || projection == ShadowProjection::Spot;
    }

    inline constexpr float kFinePositionLimit = 1.0F;  // metres per camera-relative axis
    struct FinePosition
    {
        float horizontal{ 0.0F };  // camera right
        float vertical{ 0.0F };    // world up (camera pitch/roll do not tilt the pad)
        float depth{ 0.0F };       // toward the frozen camera side
        bool operator==(const FinePosition&) const = default;
    };
    inline bool HasFinePosition(const FinePosition& value) { return value != FinePosition{}; }

    struct LightTint
    {
        float red, green, blue;
        bool operator==(const LightTint&) const = default;
    };
    inline bool ValidTint(const LightTint& tint)
    {
        const auto valid = [](float v) { return std::isfinite(v) && v >= 0.0F && v <= 1.0F; };
        return valid(tint.red) && valid(tint.green) && valid(tint.blue);
    }

    // Value-only editor state: never store engine objects in undo history.
    struct LightSettings
    {
        bool placed{ false };
        bool enabled{ true };  // A placed light can be muted without losing its slot/settings.
        int direction{ 1 };
        int type{ 0 };
        float intensity{ kLightIntensityDefault };
        float range{ kLightRangeDefault };
        float distance{ kLightDistanceDefault };
        float heightOffset{ kLightHeightDefault };
        bool castsShadow{ true };
        float shadowBias{ kShadowBiasDefault };
        ShadowProjection shadowProjection{ ShadowProjection::Spot };
        FinePosition fine{};
        std::optional<LightTint> customTint{}; // Value-owned: independent of saved-library entries.
        bool operator==(const LightSettings&) const = default;
    };

    enum class FaceLightBasis { Camera, Head };
    inline bool ValidFaceBasis(FaceLightBasis basis)
    {
        return basis == FaceLightBasis::Camera || basis == FaceLightBasis::Head;
    }

    struct FaceLightSettings
    {
        bool enabled{ false };
        float intensity{ 0.35F };
        float range{ kFaceRangeDefault };
        float heightOffset{ kFaceHeightDefault }; // world up in Camera mode; head-local up in Head mode
        FaceLightBasis basis{ FaceLightBasis::Head };
        bool operator==(const FaceLightSettings&) const = default;
    };

    struct Scene
    {
        std::array<LightSettings, kManualLightCount> lights{};
        FaceLightSettings face{};
        constexpr Scene() = default;
        constexpr Scene(LightSettings first, LightSettings second, LightSettings third) : lights{ first, second, third } {}
        // Iteration/indexing deliberately covers ONLY the three manually placed lights.
        // The independent face fill travels in the same value snapshot/history.
        constexpr auto size() const { return lights.size(); }
        constexpr auto begin() { return lights.begin(); }
        constexpr auto end() { return lights.end(); }
        constexpr auto begin() const { return lights.begin(); }
        constexpr auto end() const { return lights.end(); }
        constexpr LightSettings& operator[](std::size_t index) { return lights[index]; }
        constexpr const LightSettings& operator[](std::size_t index) const { return lights[index]; }
        bool operator==(const Scene&) const = default;
    };
    enum class NumericField { Intensity, Range, Distance, Height, FineHorizontal, FineVertical, FineDepth, FaceIntensity, FaceRange, FaceHeight, Color };

    struct HistoryEntry
    {
        std::uint64_t id;
        Scene scene;
        int selectedSlot;
        std::string label;
    };

    class LightingEditor
    {
    public:
        static constexpr std::size_t kHistoryLimit = 50;

        LightingEditor()
        {
            scene_[0].placed = true;  // Retain the prototype's initial sample light.
            history_.push_back({ nextId_++, scene_, selectedSlot_, "開始時の構成" });
        }

        const Scene& GetScene() const { return scene_; }
        const LightSettings& CurrentLight() const { return scene_[selectedSlot_]; }
        int SelectedSlot() const { return selectedSlot_; }
        const std::vector<HistoryEntry>& History() const { return history_; }
        std::size_t HistoryCursor() const { return cursor_; }
        bool HasPendingChanges() const { return scene_ != history_[cursor_].scene; }
        bool CanUndo() const { return HasPendingChanges() || cursor_ > 0; }
        bool CanRedo() const { return !HasPendingChanges() && cursor_ + 1 < history_.size(); }
        bool HasBookmark() const { return bookmark_.has_value(); }
        bool MatchesBookmark() const { return bookmark_ && scene_ == bookmark_->scene; }

        void SelectSlot(int a_slot)
        {
            if (a_slot < 0 || a_slot >= static_cast<int>(scene_.size()) || a_slot == selectedSlot_) {
                return;
            }
            FinishNumericEdit();
            selectedSlot_ = a_slot;  // Selecting an editor is not a lighting change.
        }

        // Left-click picks placed occupants, including muted/zero-strength lights.
        // Enter a group at its lowest slot; subsequent clicks cycle in slot order.
        std::optional<int> DirectionSelection(int a_direction) const
        {
            if (a_direction < 0 || a_direction >= 8) { return std::nullopt; }
            const bool currentOccupies = CurrentLight().placed && CurrentLight().direction == a_direction;
            const int count = static_cast<int>(scene_.size());
            const int start = currentOccupies ? selectedSlot_ + 1 : 0;
            for (int offset = 0; offset < count; ++offset) {
                const int slot = (start + offset) % count;
                if (scene_[slot].placed && scene_[slot].direction == a_direction) {
                    return slot;
                }
            }
            return std::nullopt;
        }

        void ActivateDirection(int a_direction)
        {
            if (a_direction < 0 || a_direction >= 8) { return; }
            if (const auto slot = DirectionSelection(a_direction)) {
                SelectSlot(*slot);
            } else {
                SetDirection(a_direction);
            }
        }

        // Explicit move (context-menu action) deliberately allows occupied directions.
        // An unplaced slot only stages its direction; it never creates a light here.
        void SetDirection(int a_direction)
        {
            if (a_direction < 0 || a_direction >= 8) {
                return;
            }
            FinishNumericEdit();
            scene_[selectedSlot_].direction = a_direction;
            Commit(Label("位置変更"));
        }

        void SetType(int a_type)
        {
            ApplyLightRecipe(a_type, std::nullopt);
        }

        void ApplyLightRecipe(int a_type, std::optional<LightTint> tint)
        {
            if (a_type < 0 || a_type >= kLightTypeCount || (tint && !ValidTint(*tint))) {
                return;
            }
            FinishNumericEdit();
            auto& light = scene_[selectedSlot_];
            const bool wasPlaced = light.placed;
            light.type = a_type;
            light.customTint = tint;
            light.placed = true;
            if (!wasPlaced) {
                light.enabled = true;
            }
            Commit(Label(wasPlaced ? "ライト選択" : "配置"));
        }

        void SetTint(LightTint tint)
        {
            if (!CurrentLight().placed || !ValidTint(tint) || CurrentLight().customTint == tint) { return; }
            if (pendingField_ && *pendingField_ != NumericField::Color) { FinishNumericEdit(); }
            pendingField_ = NumericField::Color;
            pendingLabel_ = Label("ライトの色");
            scene_[selectedSlot_].customTint = tint;
        }

        void ToggleLight(int a_slot)
        {
            if (a_slot < 0 || a_slot >= static_cast<int>(scene_.size()) || !scene_[a_slot].placed) {
                return;
            }
            FinishNumericEdit();
            auto& light = scene_[a_slot];
            light.enabled = !light.enabled;
            Commit("Light " + std::to_string(a_slot + 1) + (light.enabled ? " : ON" : " : OFF"));
        }

        void SetWideRange()
        {
            if (!CurrentLight().placed) { return; }
            FinishNumericEdit();
            scene_[selectedSlot_].range = kWideLightRange;
            Commit(Label("広域 (12m)"));
        }

        void ToggleFaceLight()
        {
            FinishNumericEdit();
            scene_.face.enabled = !scene_.face.enabled;
            Commit(scene_.face.enabled ? "フェイスライト : ON" : "フェイスライト : OFF");
        }

        void SetFaceIntensity(float value)
        {
            if (!std::isfinite(value)) { return; }
            value = std::clamp(value, 0.0F, kFaceIntensityMax);
            if (scene_.face.intensity == value) { return; }
            if (pendingField_ && *pendingField_ != NumericField::FaceIntensity) { FinishNumericEdit(); }
            pendingField_ = NumericField::FaceIntensity;
            pendingLabel_ = "フェイスライト : 強さ";
            scene_.face.intensity = value;
        }

        void SetFaceBasis(FaceLightBasis basis)
        {
            if (!ValidFaceBasis(basis) || scene_.face.basis == basis) { return; }
            FinishNumericEdit();
            scene_.face.basis = basis;
            Commit(basis == FaceLightBasis::Camera ? "フェイスライト : カメラ基準" : "フェイスライト : 顔の向き基準");
        }

        void SetFaceRange(float value)
        {
            if (!std::isfinite(value)) { return; }
            value = std::clamp(value, kFaceRangeMin, kFaceRangeMax);
            if (scene_.face.range == value) { return; }
            if (pendingField_ && *pendingField_ != NumericField::FaceRange) { FinishNumericEdit(); }
            pendingField_ = NumericField::FaceRange;
            pendingLabel_ = "フェイスライト : 光の範囲";
            scene_.face.range = value;
        }

        void SetFaceHeight(float value)
        {
            if (!std::isfinite(value)) { return; }
            value = std::clamp(value, kFaceHeightMin, kFaceHeightMax);
            if (scene_.face.heightOffset == value) { return; }
            if (pendingField_ && *pendingField_ != NumericField::FaceHeight) { FinishNumericEdit(); }
            pendingField_ = NumericField::FaceHeight;
            pendingLabel_ = "フェイスライト : 上下";
            scene_.face.heightOffset = value;
        }

        static bool IsValidScene(const Scene& a_scene)
        {
            const auto inRange = [](float value, float min, float max) {
                return std::isfinite(value) && value >= min && value <= max;
            };
            for (const auto& light : a_scene) {
                if (light.direction < 0 || light.direction >= 8 || light.type < 0 || light.type >= kLightTypeCount ||
                    !inRange(light.intensity, 0.0F, 3.0F) || !inRange(light.range, 0.5F, 12.0F) ||
                    !inRange(light.distance, 0.25F, 8.0F) || !inRange(light.heightOffset, -2.0F, 2.0F) ||
                    !inRange(light.shadowBias, kShadowBiasLevels.front(), kShadowBiasLevels.back()) ||
                    !inRange(light.fine.horizontal, -kFinePositionLimit, kFinePositionLimit) ||
                    !inRange(light.fine.vertical, -kFinePositionLimit, kFinePositionLimit) ||
                    !inRange(light.fine.depth, -kFinePositionLimit, kFinePositionLimit) ||
                    !ValidProjection(light.shadowProjection) || (light.customTint && !ValidTint(*light.customTint))) {
                    return false;
                }
            }
            return ValidFaceBasis(a_scene.face.basis) && inRange(a_scene.face.intensity, 0.0F, kFaceIntensityMax) &&
                inRange(a_scene.face.range, kFaceRangeMin, kFaceRangeMax) &&
                inRange(a_scene.face.heightOffset, kFaceHeightMin, kFaceHeightMax);
        }

        bool ApplyScene(const Scene& a_scene, const std::string& a_name)
        {
            if (!IsValidScene(a_scene)) {
                return false;  // Validate every slot before changing anything.
            }
            FinishNumericEdit();
            scene_ = a_scene;
            selectedSlot_ = 0;
            for (int slot = 0; slot < static_cast<int>(scene_.size()); ++slot) {
                if (scene_[slot].placed) {
                    selectedSlot_ = slot;
                    break;
                }
            }
            Commit("プリセット : " + a_name);  // One atomic history entry for all three slots.
            return true;
        }

        void DeleteCurrent()
        {
            FinishNumericEdit();
            // Retain this slot's settings so replacing a deleted light is predictable.
            scene_[selectedSlot_].placed = false;
            Commit(Label("削除"));
        }

        void SetShadow(bool a_enabled)
        {
            FinishNumericEdit();
            if (!CurrentLight().placed) {
                return;
            }
            scene_[selectedSlot_].castsShadow = a_enabled;
            Commit(Label(a_enabled ? "影 ON" : "影 OFF"));
        }

        void SetShadowBias(float a_value)
        {
            if (!CurrentLight().placed || !std::isfinite(a_value)) { return; }
            FinishNumericEdit();
            scene_[selectedSlot_].shadowBias = std::clamp(a_value, kShadowBiasLevels.front(), kShadowBiasLevels.back());
            Commit(Label("影の補正"));
        }

        void SetShadowMode(bool a_shadow, ShadowProjection a_projection)
        {
            if (!CurrentLight().placed || !ValidProjection(a_projection)) { return; }
            FinishNumericEdit();
            auto& light = scene_[selectedSlot_];
            light.castsShadow = a_shadow;
            light.shadowProjection = a_projection;
            Commit(Label(!a_shadow ? "影なし" : a_projection == ShadowProjection::Spot ? "スポットの影" : "全方向の影"));
        }

        static float Value(const LightSettings& a_light, NumericField a_field)
        {
            switch (a_field) {
            case NumericField::Intensity: return a_light.intensity;
            case NumericField::Range: return a_light.range;
            case NumericField::Distance: return a_light.distance;
            case NumericField::Height: return a_light.heightOffset;
            case NumericField::FineHorizontal: return a_light.fine.horizontal;
            case NumericField::FineVertical: return a_light.fine.vertical;
            case NumericField::FineDepth: return a_light.fine.depth;
            case NumericField::FaceIntensity:
            case NumericField::Color:
            case NumericField::FaceHeight:
            case NumericField::FaceRange: return 0.0F; // Independent Scene::face; not a manual light field.
            }
            return 0.0F;
        }

        void SetNumeric(NumericField a_field, float a_value)
        {
            if (!std::isfinite(a_value) || !CurrentLight().placed) {
                return;
            }
            float* destination = nullptr;
            const char* description = nullptr;
            auto& light = scene_[selectedSlot_];
            switch (a_field) {
            case NumericField::FineHorizontal:
            case NumericField::FineVertical:
            case NumericField::FineDepth: {
                auto fine = light.fine;
                if (a_field == NumericField::FineHorizontal) { fine.horizontal = a_value; }
                if (a_field == NumericField::FineVertical) { fine.vertical = a_value; }
                if (a_field == NumericField::FineDepth) { fine.depth = a_value; }
                SetFinePosition(fine);
                return;
            }
            case NumericField::Intensity:
                destination = &light.intensity;
                description = "強さ";
                a_value = std::clamp(a_value, 0.0F, 3.0F);
                break;
            case NumericField::Range:
                destination = &light.range;
                description = "光の範囲";
                a_value = std::clamp(a_value, 0.5F, 12.0F);
                break;
            case NumericField::Distance:
                destination = &light.distance;
                description = "距離";
                a_value = std::clamp(a_value, 0.25F, 8.0F);
                break;
            case NumericField::Height:
                destination = &light.heightOffset;
                description = "高さ";
                a_value = std::clamp(a_value, -2.0F, 2.0F);
                break;
            default: return;
            }
            if (*destination == a_value) {
                return;
            }
            if (pendingField_ && *pendingField_ != a_field) {
                FinishNumericEdit();
            }
            pendingField_ = a_field;
            pendingLabel_ = Label(description);
            *destination = a_value;
        }

        void SetFinePosition(FinePosition value)
        {
            if (!CurrentLight().placed || !std::isfinite(value.horizontal) ||
                !std::isfinite(value.vertical) || !std::isfinite(value.depth)) { return; }
            value.horizontal = std::clamp(value.horizontal, -kFinePositionLimit, kFinePositionLimit);
            value.vertical = std::clamp(value.vertical, -kFinePositionLimit, kFinePositionLimit);
            value.depth = std::clamp(value.depth, -kFinePositionLimit, kFinePositionLimit);
            if (CurrentLight().fine == value) { return; }
            // All axes form ONE pad gesture. Fine sliders use the same value path;
            // their release/activation boundaries still finish independent gestures.
            constexpr auto group = NumericField::FineHorizontal;
            if (pendingField_ && *pendingField_ != group) { FinishNumericEdit(); }
            pendingField_ = group;
            pendingLabel_ = Label("位置の微調整");
            scene_[selectedSlot_].fine = value;
        }

        void ResetFinePosition()
        {
            if (!CurrentLight().placed) { return; }
            FinishNumericEdit();
            scene_[selectedSlot_].fine = {};
            Commit(Label("位置だけ基準に戻す"));
        }

        bool CanResetCurrentValues() const
        {
            const auto& light = CurrentLight();
            return light.placed && (light.intensity != kLightIntensityDefault ||
                light.range != kLightRangeDefault || light.distance != kLightDistanceDefault ||
                light.heightOffset != kLightHeightDefault || HasFinePosition(light.fine) ||
                light.shadowBias != kShadowBiasDefault);
        }

        void ResetCurrentValues()
        {
            if (!CurrentLight().placed) { return; }
            FinishNumericEdit();
            if (!CanResetCurrentValues()) { return; }
            auto& light = scene_[selectedSlot_];
            light.intensity = kLightIntensityDefault;
            light.range = kLightRangeDefault;
            light.distance = kLightDistanceDefault;
            light.heightOffset = kLightHeightDefault;
            light.fine = {};
            light.shadowBias = kShadowBiasDefault;
            Commit(Label("調整を初期値へ"));
        }

        // Called when a slider/pad gesture ends, not once for each live-preview frame.
        void FinishNumericEdit()
        {
            if (pendingField_) {
                Commit(pendingLabel_);
                pendingField_.reset();
                pendingLabel_.clear();
            }
        }

        void Undo()
        {
            FinishNumericEdit();
            if (cursor_ > 0) {
                RestoreHistory(--cursor_);
            }
        }

        void Redo()
        {
            FinishNumericEdit();
            if (cursor_ + 1 < history_.size()) {
                RestoreHistory(++cursor_);
            }
        }

        bool JumpTo(std::uint64_t a_id)
        {
            // Finishing a gesture can prune old entries; resolve the stable ID afterwards.
            FinishNumericEdit();
            for (std::size_t i = 0; i < history_.size(); ++i) {
                if (history_[i].id == a_id) {
                    RestoreHistory(i);
                    return true;
                }
            }
            return false;
        }

        void Adopt()
        {
            FinishNumericEdit();
            bookmark_ = HistoryEntry{ 0, scene_, selectedSlot_, "栞の構成" };
        }

        void RestoreBookmark()
        {
            FinishNumericEdit();
            if (bookmark_) {
                scene_ = bookmark_->scene;
                selectedSlot_ = bookmark_->selectedSlot;
                Commit("栞へ復帰");
            }
        }

    private:
        std::string Label(const char* a_action) const
        {
            return "Light " + std::to_string(selectedSlot_ + 1) + " : " + a_action;
        }

        void Commit(const std::string& a_label)
        {
            if (!HasPendingChanges()) {
                return;  // No-ops must not destroy the redo branch.
            }
            history_.resize(cursor_ + 1);
            history_.push_back({ nextId_++, scene_, selectedSlot_, a_label });
            if (history_.size() > kHistoryLimit) {
                history_.erase(history_.begin());
            }
            cursor_ = history_.size() - 1;
        }

        void RestoreHistory(std::size_t a_cursor)
        {
            cursor_ = a_cursor;
            scene_ = history_[cursor_].scene;
            selectedSlot_ = history_[cursor_].selectedSlot;
        }

        Scene scene_{};
        int selectedSlot_{ 0 };
        std::vector<HistoryEntry> history_;
        std::size_t cursor_{ 0 };
        std::uint64_t nextId_{ 1 };
        std::optional<NumericField> pendingField_;
        std::string pendingLabel_;
        std::optional<HistoryEntry> bookmark_;
    };
}
