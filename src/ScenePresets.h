#pragma once

#include "LightingState.h"

namespace ScreenshotLightingAssistant
{
    struct LightTypeDisplay
    {
        const char* name;
        std::array<std::uint8_t, 3> color;
    };

    // Schematic colors, not a simulated image of the in-game lighting result.
    inline constexpr std::array<LightTypeDisplay, kLightTypeCount> kLightTypes{ {
        { "スタジオ白", { 216, 237, 255 } },
        { "スタジオ暖色", { 255, 205, 135 } },
        { "ろうそく", { 255, 153, 75 } },
        { "スタジオ青", { 110, 165, 255 } },
    } };

    struct ScenePreset
    {
        const char* id;
        const char* name;
        const char* description;
        Scene scene;
        bool includeFace{ false };
    };

    constexpr LightSettings PresetLight(int direction, int type, float strength, float distance, float height, bool shadow, bool enabled = true)
    {
        LightSettings light;
        light.placed = true;
        light.enabled = enabled;
        light.direction = direction;
        light.type = type;
        light.intensity = strength;
        light.distance = distance;
        light.heightOffset = height;
        light.castsShadow = shadow;
        return light;
    }

    constexpr LightSettings DetailedPresetLight(int direction, int type, float strength, float range,
        float distance, float height, bool shadow, float shadowBias, FinePosition fine = {})
    {
        auto light = PresetLight(direction, type, strength, distance, height, shadow);
        light.range = range;
        light.shadowBias = shadowBias;
        light.fine = fine;
        return light;
    }

    // Starter scenes: all slots render in 0.1.9. Shadowed lights default to Spot;
    // shadowless fill lights keep their existing strength and placement. Visual tuning is ongoing.
    inline constexpr std::array<ScenePreset, 8> kScenePresets{ {
        { "studio_white", "白色ポートレート", "白色 / 左右の前方から", {
            PresetLight(5, 0, 1.0F, 2.2F, 0.25F, true),
            PresetLight(3, 0, 0.45F, 2.6F, 0.0F, false), {} } },
        { "studio_warm", "暖色ポートレート", "暖色 / 正面と側面から", {
            PresetLight(4, 1, 0.9F, 2.2F, 0.2F, true),
            PresetLight(6, 1, 0.4F, 2.5F, 0.0F, false), {} } },
        { "rim", "リムライト", "白色 / 後方2灯と正面補助", {
            PresetLight(7, 0, 1.35F, 1.8F, 0.3F, true),
            PresetLight(1, 0, 1.1F, 1.8F, 0.3F, true),
            PresetLight(4, 0, 0.35F, 2.6F, 0.0F, false) } },
        { "rim_blue", "青色リムライト", "暖色1灯＋青色2灯 / フェイスライト込み", [] {
            Scene scene{
                DetailedPresetLight(6, 1, 2.08F, 8.6F, 1.49F, .55F, true, 2.0F, {-.0892856866F, .178571448F, 0}),
                DetailedPresetLight(0, 3, 1.57F, 10.5F, 1.31F, -.04F, false, 1.0F, {-.107142851F, .116071299F, 0}),
                DetailedPresetLight(1, 3, 2.52F, 5.4F, 1.62F, .34F, false, 1.0F, {.526785493F, .214285582F, 0}) };
            scene.face = {true, .29F, .92F, -.07F, FaceLightBasis::Head};
            return scene;
        }(), true },
        { "rim_warm", "暖色リムライト", "暖色2灯＋白色1灯", {
            DetailedPresetLight(5, 1, 1.15F, 4.1F, 2.71F, .18F, true, 1.0F),
            DetailedPresetLight(0, 1, 2.49F, 5.5F, 3.06F, .48F, false, 1.0F),
            DetailedPresetLight(7, 0, 1.94F, 4.5F, 2.95F, -.64F, false, 1.0F, {-.616071165F, .303571552F, 0}) } },
        { "candle", "キャンドル", "ろうそく色 / 低めの光", {
            PresetLight(5, 2, 0.8F, 1.0F, -0.4F, true),
            PresetLight(1, 1, 0.3F, 2.0F, 0.2F, false), {} } },
        { "candle_space", "キャンドル空間照明", "白色1灯＋ろうそく色2灯 / フェイスライト込み", [] {
            Scene scene{
                DetailedPresetLight(3, 0, 1.38F, 5.0F, 2.20F, .25F, true, 1.0F),
                DetailedPresetLight(6, 2, 1.45F, 4.40F, 2.60F, 0.0F, false, 1.0F, {.97321409F, .562500119F, -.100000001F}),
                DetailedPresetLight(1, 2, 1.62F, 2.80F, 2.64F, 1.38F, true, 1.0F, {-.589285553F, .00892858859F, -.140000001F}) };
            scene.face = {false, .29F, .92F, -.07F, FaceLightBasis::Head};
            return scene;
        }(), true },
        { "comparison", "片側ライティング", "補助のLight 2はOFFで待機", {
            PresetLight(6, 0, 1.0F, 2.0F, 0.2F, true),
            PresetLight(2, 0, 0.5F, 2.0F, 0.0F, false, false), {} } },
    } };

    // Opening, hovering and cancelling never modify the editor. Shared by UI and tests.
    class PresetPicker
    {
    public:
        bool IsOpen() const { return open_; }
        bool KeepOpen() const { return keepOpen_; }
        void SetKeepOpen(bool value) { keepOpen_ = value; }
        void Open() { open_ = true; }
        void Close() { open_ = false; }
        void Applied() { if (!keepOpen_) { Close(); } }
        bool Choose(std::size_t a_index, LightingEditor& a_editor)
        {
            if (!open_ || a_index >= kScenePresets.size()) {
                return false;
            }
            const auto& preset = kScenePresets[a_index];
            auto scene = preset.scene;
            if (!preset.includeFace) {
                scene.face = a_editor.GetScene().face;
            }
            if (!a_editor.ApplyScene(scene, preset.name)) {
                return false;
            }
            Applied();
            return true;
        }

    private:
        bool open_{ false };
        bool keepOpen_{ false }; // UI preference, retained only for this process.
    };
}
