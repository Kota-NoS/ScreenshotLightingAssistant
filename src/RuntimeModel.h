#pragma once

#include "LightingState.h"
#include <cmath>
#include <numbers>
#include <optional>

namespace ScreenshotLightingAssistant
{
    // Creation-only parameters: compare these before touching the registered engine light.
    // Bias/projection are inactive without shadows; retained edits then cause no rebuild.
    struct LightRegistrationSettings
    {
        bool shadow = false;
        float depthBias = 1.0F;
        bool spot = false;
        float fov = 2.0F * std::numbers::pi_v<float>;
        float falloff = 1.0F;
        float nearDistance = 5.0F;
        static LightRegistrationSettings From(const LightSettings& light)
        {
            LightRegistrationSettings result;
            result.shadow = light.castsShadow;
            result.depthBias = light.castsShadow ? light.shadowBias : 1.0F;
            result.spot = light.castsShadow && light.shadowProjection == ShadowProjection::Spot;
            if (result.spot) {
                // 90-degree shadow cone; template parameters observed in SAM's selected
                // WRShadowDirectional form. Do not borrow its colour, fade, forms or assets.
                result.fov = std::numbers::pi_v<float> * 0.5F;
                result.falloff = 5.0F;
                result.nearDistance = 7.508994F;
            }
            return result;
        }
        bool operator==(const LightRegistrationSettings&) const = default;
    };

    // No engine pointers in UI requests, history, or session tokens.
    struct LightSessionSnapshot
    {
        std::uint64_t epoch = 0;
        std::uint64_t alignment = 0;
        bool ready = false;
        bool blocked = false;
        bool running = false;
        Scene lights{};
    };

    inline constexpr std::size_t kFaceRuntimeSlot = kManualLightCount;
    inline constexpr std::size_t kPersistentFaceRuntimeSlot = kManualLightCount + 1;
    inline constexpr std::size_t kScreenPlacementRuntimeSlot = kManualLightCount + 2;
    inline constexpr std::size_t kRuntimeLightCount = kManualLightCount + 3;
    inline std::optional<LightSettings> RuntimeSettings(const Scene& scene, std::size_t slot)
    {
        if (slot < scene.size()) { return scene[slot]; }
        if (slot != kFaceRuntimeSlot) { return std::nullopt; }
        LightSettings face;
        face.placed = true;
        face.enabled = scene.face.enabled;
        face.intensity = scene.face.intensity;
        face.range = scene.face.range;
        face.customTint = scene.face.tint;
        face.castsShadow = false;
        face.shadowProjection = ShadowProjection::Omni;
        return face;  // Face-owned colour; no manual slot's colour/position/shadow can leak in.
    }

    class LightSession
    {
    public:
        const LightSessionSnapshot& Get() const { return state_; }
        static bool Valid(const LightSettings& light)
        {
            Scene scene{};
            scene[0] = light;
            return LightingEditor::IsValidScene(scene);
        }
        void SetReady(bool ready)
        {
            ++state_.epoch;
            state_.ready = ready;
            state_.running = false;
        }
        void Block(bool blocked)
        {
            ++state_.epoch;
            state_.blocked = blocked;
            state_.running = false;
        }
        bool Start(std::uint64_t epoch, const Scene& lights)
        {
            if (epoch != state_.epoch || !state_.ready || state_.blocked || !LightingEditor::IsValidScene(lights)) {
                return false;
            }
            ++state_.epoch;
            ++state_.alignment;
            state_.running = true;
            state_.lights = lights;
            return true;
        }
        bool Stop(std::uint64_t epoch)
        {
            if (epoch != state_.epoch) { return false; }
            ++state_.epoch;
            state_.running = false;
            return true;
        }
        bool Submit(std::uint64_t epoch, const Scene& lights)
        {
            if (epoch != state_.epoch || !state_.running || !LightingEditor::IsValidScene(lights)) { return false; }
            state_.lights = lights;
            return true;
        }
        bool Align(std::uint64_t epoch)
        {
            if (epoch != state_.epoch || !state_.running) { return false; }
            ++state_.alignment;
            return true;
        }
        static bool ShouldIlluminate(const LightSessionSnapshot& state, std::size_t slot)
        {
            if (slot >= state.lights.size() || !state.ready || state.blocked || !state.running) { return false; }
            const auto& light = state.lights[slot];
            return Valid(light) && light.placed && light.enabled && light.intensity > 0.0F;
        }
        static bool ShouldIlluminateFace(const LightSessionSnapshot& state)
        {
            return state.ready && !state.blocked && state.running &&
                LightingEditor::IsValidScene(state.lights) && state.lights.face.enabled && state.lights.face.intensity > 0.0F;
        }
        static bool ShouldIlluminateRuntime(const LightSessionSnapshot& state, std::size_t slot)
        {
            return slot == kFaceRuntimeSlot ? ShouldIlluminateFace(state) : ShouldIlluminate(state, slot);
        }
    private:
        LightSessionSnapshot state_;
    };

    inline std::size_t ActiveManualLightCount(const LightSessionSnapshot& state)
    {
        std::size_t count = 0;
        for (std::size_t slot = 0; slot < kManualLightCount; ++slot) {
            if (LightSession::ShouldIlluminate(state, slot)) { ++count; }
        }
        return count;
    }

    inline bool ScreenPlacementMustSwitchOff(bool enabled, std::size_t activeManualLights)
    {
        return enabled && activeManualLights >= kManualLightCount;
    }

    // Authored fixed tints, not Kelvin/photometric values. White preserves 0.1.8.
    // UI schematic colors are intentionally separate from renderer diffuse values.
    inline constexpr std::array<LightTint, kLightTypeCount> kRuntimeTints{ {
        { 1.0F, 1.0F, 1.0F },
        { 1.0F, 0.80F, 0.53F },
        { 1.0F, 0.60F, 0.29F },
        { 0.30F, 0.55F, 1.0F }
    } };
    // Face lights reuse the authored manual-light tints, but present blue before candle
    // for the compact White / Warm / Blue / Candle picker requested by photographers.
    inline constexpr std::array<std::size_t, kLightTypeCount> kFaceTintPresetTypes{ 0, 1, 3, 2 };
    inline constexpr LightTint FaceTintPreset(std::size_t index)
    {
        return kRuntimeTints[kFaceTintPresetTypes[index]];
    }
    inline std::optional<std::size_t> FaceTintPresetIndex(LightTint tint)
    {
        for (std::size_t index = 0; index < kFaceTintPresetTypes.size(); ++index) {
            if (FaceTintPreset(index) == tint) { return index; }
        }
        return std::nullopt;
    }
    inline std::optional<LightTint> RuntimeTint(int type)
    {
        if (type < 0 || type >= static_cast<int>(kRuntimeTints.size())) { return std::nullopt; }
        return kRuntimeTints[type];
    }

    inline std::optional<LightTint> RuntimeTint(const LightSettings& light)
    {
        if (!RuntimeTint(light.type) || (light.customTint && !ValidTint(*light.customTint))) { return std::nullopt; }
        return light.customTint ? light.customTint : RuntimeTint(light.type);
    }

    struct LightVector { float x = 0, y = 0, z = 0; };
    struct CameraBasis { LightVector front; LightVector right; };
    // Skyrim's conventional 128 units = 6 feet; UI metres remain renderer-independent.
    inline constexpr float kUnitsPerMetre = 128.0F / 1.8288F;
    // The cursor maps to one camera-facing plane in front of the subject. Depth
    // remains subject-relative, so changing it cannot rotate or orbit the light.
    inline constexpr float kScreenPlacementDepthDefault = 1.0F;
    inline constexpr float kScreenPlacementDepthMin = 0.25F;
    inline constexpr float kScreenPlacementDepthMax = 8.0F;
    inline constexpr float kScreenPlacementCameraMargin = 0.10F;
    inline constexpr float kScreenPlacementRangeMin = 0.5F;
    inline constexpr float kScreenPlacementRangeMax = 20.0F;
    inline constexpr float kScreenPlacementRangeDefault = 6.0F;
    inline constexpr float kScreenPlacementDistanceWheelStep = 0.03F;
    inline constexpr float kScreenPlacementDistanceWheelFastStep = 0.20F;
    // NiCamera looks along local +X; +Y is up, +Z is right. The dynamic shadow
    // frustum camera copies the point light's world rotation (see ENGINE_NOTES).
    struct SpotlightBasis { LightVector forward; LightVector up; LightVector right; };
    inline bool Finite(LightVector p)
    {
        return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
    }
    inline LightVector Cross(LightVector a, LightVector b)
    {
        return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
    }
    inline std::optional<LightVector> Unit(LightVector p)
    {
        if (!Finite(p)) { return std::nullopt; }
        const float length = std::hypot(p.x, p.y, p.z);
        if (!std::isfinite(length) || length < 0.0001F) { return std::nullopt; }
        return LightVector{ p.x / length, p.y / length, p.z / length };
    }
    inline float Dot(LightVector a, LightVector b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    // Intersect the cursor ray with the fixed camera-facing placement plane.
    inline std::optional<LightVector> ScreenPlacementPosition(LightVector rayOrigin,
        LightVector rayDirection, LightVector centreRayOrigin,
        LightVector centreRayDirection, LightVector anchor, float requestedDepth)
    {
        if (!Finite(rayOrigin) || !Finite(centreRayOrigin) || !Finite(anchor) ||
            !std::isfinite(requestedDepth) || requestedDepth < kScreenPlacementDepthMin ||
            requestedDepth > kScreenPlacementDepthMax) {
            return std::nullopt;
        }
        auto ray = Unit(rayDirection);
        auto forward = Unit(centreRayDirection);
        const auto centreToAnchor = Unit({anchor.x - centreRayOrigin.x,
            anchor.y - centreRayOrigin.y, anchor.z - centreRayOrigin.z});
        if (!ray || !forward || !centreToAnchor) { return std::nullopt; }

        // WindowPointToRay is the authority for the current rendered camera.
        // Still orient its result toward the selected subject so a camera whose
        // local forward convention is reversed cannot place the light behind it.
        if (Dot(*forward, *centreToAnchor) < 0.0F) {
            *forward = {-forward->x, -forward->y, -forward->z};
        }
        if (Dot(*ray, *forward) < 0.0F) {
            *ray = {-ray->x, -ray->y, -ray->z};
        }
        const LightVector centreToAnchorVector{anchor.x - centreRayOrigin.x,
            anchor.y - centreRayOrigin.y, anchor.z - centreRayOrigin.z};
        const float subjectDepth = Dot(centreToAnchorVector, *forward) / kUnitsPerMetre;
        if (!std::isfinite(subjectDepth) ||
            subjectDepth <= kScreenPlacementDepthMin + kScreenPlacementCameraMargin) {
            return std::nullopt;
        }
        // Never let a large remembered value cross the current camera. This is
        // particularly important after loading a preset at a closer camera distance.
        const float depth = std::clamp(requestedDepth, kScreenPlacementDepthMin,
            std::min(kScreenPlacementDepthMax, subjectDepth - kScreenPlacementCameraMargin));
        const LightVector planePoint{
            anchor.x - forward->x * depth * kUnitsPerMetre,
            anchor.y - forward->y * depth * kUnitsPerMetre,
            anchor.z - forward->z * depth * kUnitsPerMetre
        };
        const float denominator = Dot(*ray, *forward);
        if (!std::isfinite(denominator) || std::abs(denominator) < 0.0001F) { return std::nullopt; }
        const LightVector toPlane{planePoint.x - rayOrigin.x, planePoint.y - rayOrigin.y,
            planePoint.z - rayOrigin.z};
        const float distance = Dot(toPlane, *forward) / denominator;
        if (!std::isfinite(distance) || distance <= 0.0F) { return std::nullopt; }
        const LightVector result{rayOrigin.x + ray->x * distance,
            rayOrigin.y + ray->y * distance, rayOrigin.z + ray->z * distance};
        return Finite(result) ? std::optional(result) : std::nullopt;
    }
    inline float ScreenPlacementDistanceAfterWheel(float current, float wheel, bool fast)
    {
        if (!std::isfinite(current) || !std::isfinite(wheel)) { return current; }
        const float step = fast ? kScreenPlacementDistanceWheelFastStep :
            kScreenPlacementDistanceWheelStep;
        return std::clamp(current + wheel * step,
            kScreenPlacementDepthMin, kScreenPlacementDepthMax);
    }
    inline bool ValidScreenPlacementLightSettings(const LightSettings& light)
    {
        const auto inRange = [](float value, float minimum, float maximum) {
            return std::isfinite(value) && value >= minimum && value <= maximum;
        };
        return light.placed && light.enabled && light.type >= 0 && light.type < kLightTypeCount &&
            inRange(light.intensity, 0.0F, 3.0F) &&
            inRange(light.range, kScreenPlacementRangeMin, kScreenPlacementRangeMax) &&
            inRange(light.distance, kScreenPlacementDepthMin, kScreenPlacementDepthMax) &&
            inRange(light.shadowBias, kShadowBiasLevels.front(), kShadowBiasLevels.back()) &&
            ValidProjection(light.shadowProjection) &&
            (!light.customTint || ValidTint(*light.customTint));
    }
    inline LightSettings ScreenPlacementLightSettings(const LightSettings& source)
    {
        LightSettings result;
        result.placed = true;
        result.enabled = true;
        result.type = source.type;
        result.intensity = source.intensity;
        // A new free light uses a visible, practical placement range. Reopening,
        // stop/start and preset restore take the remembered-settings path instead.
        result.range = kScreenPlacementRangeDefault;
        result.distance = kScreenPlacementDepthDefault;
        result.castsShadow = source.castsShadow;
        result.shadowBias = source.shadowBias;
        result.shadowProjection = source.shadowProjection;
        result.customTint = source.customTint;
        return result;
    }
    inline LightSettings NormalizeScreenPlacementLightSettings(const LightSettings& source)
    {
        auto result = ScreenPlacementLightSettings(source);
        // Normalization is also used after edits and while rebuilding remembered
        // lights. Only brand-new placement starts at the 6m default; an existing
        // value must remain authoritative for both the UI and the runtime light.
        result.range = source.range;
        result.distance = source.distance;
        return result;
    }
    inline bool ValidScreenPlacementSettings(const ScreenPlacementSettings& settings)
    {
        return std::isfinite(settings.normalizedX) && std::isfinite(settings.normalizedY) &&
            settings.normalizedX >= 0.0F && settings.normalizedX <= 1.0F &&
            settings.normalizedY >= 0.0F && settings.normalizedY <= 1.0F &&
            ValidScreenPlacementLightSettings(settings.light);
    }
    inline LightSettings ScreenPlacementLightForBegin(const LightSettings& source,
        const std::optional<LightSettings>& remembered)
    {
        // Screen-placement edits may legally use the wider 0.5-20m range. Reopening
        // within the same lighting session must therefore prefer the already-
        // validated placement settings instead of normalizing the selected main light
        // again (whose normal runtime range is narrower).
        if (remembered && ValidScreenPlacementLightSettings(*remembered)) {
            return *remembered;
        }
        return ScreenPlacementLightSettings(source);
    }
    inline std::optional<SpotlightBasis> AimSpotlight(LightVector position, LightVector target)
    {
        if (!Finite(position) || !Finite(target)) { return std::nullopt; }
        const auto forward = Unit({ target.x - position.x, target.y - position.y, target.z - position.z });
        if (!forward) { return std::nullopt; }
        const LightVector referenceUp = std::abs(forward->z) < 0.99F ? LightVector{ 0, 0, 1 } : LightVector{ 0, 1, 0 };
        const auto right = Unit(Cross(*forward, referenceUp));
        if (!right) { return std::nullopt; }
        const auto up = Unit(Cross(*right, *forward));
        if (!up) { return std::nullopt; }
        return SpotlightBasis{ *forward, *up, *right };
    }
    inline std::optional<CameraBasis> MakeCameraBasis(LightVector anchor, LightVector camera)
    {
        if (!Finite(anchor) || !Finite(camera)) { return std::nullopt; }
        const float x = camera.x - anchor.x, y = camera.y - anchor.y;
        const float length = std::hypot(x, y);
        if (!std::isfinite(length) || length < 1.0F) { return std::nullopt; }
        const LightVector front{ x / length, y / length, 0 };
        return CameraBasis{ front, { -front.y, front.x, 0 } };
    }
    inline std::optional<LightVector> FaceLightPosition(LightVector head, LightVector camera, float heightOffset)
    {
        if (!Finite(head) || !Finite(camera) || !std::isfinite(heightOffset) ||
            heightOffset < kFaceHeightMin || heightOffset > kFaceHeightMax) { return std::nullopt; }
        const LightVector delta{ camera.x - head.x, camera.y - head.y, camera.z - head.z };
        const auto towardCamera = Unit(delta);
        const float separation = std::hypot(delta.x, delta.y, delta.z);
        // Coincident/inside-head cameras and invalid coordinates suppress only the face fill.
        if (!towardCamera || !std::isfinite(separation) || separation < 0.10F * kUnitsPerMetre) { return std::nullopt; }
        const float distance = std::min(0.45F * kUnitsPerMetre, separation * 0.5F);
        // Retain the old camera-side XY placement, but decouple elevation from camera pitch.
        // Projecting the 3D direction (rather than normalizing XY) remains smooth at overhead
        // views: horizontal displacement tends to zero, with no arbitrary azimuth fallback.
        const LightVector position{ head.x + towardCamera->x * distance,
            head.y + towardCamera->y * distance, head.z + heightOffset * kUnitsPerMetre };
        return Finite(position) ? std::optional(position) : std::nullopt;
    }
    // Head-local +Y is forward and +Z is up in the humanoid reference skeleton.
    // Use rotation only: offsets stay in metres and do not inherit skeleton scale.
    inline std::optional<LightVector> HeadFaceLightPosition(LightVector head, LightVector forward,
        LightVector up, float heightOffset)
    {
        if (!Finite(head) || !std::isfinite(heightOffset) || heightOffset < kFaceHeightMin ||
            heightOffset > kFaceHeightMax) { return std::nullopt; }
        const auto f = Unit(forward);
        const auto u = Unit(up);
        if (!f || !u || std::abs(f->x * u->x + f->y * u->y + f->z * u->z) > .01F) {
            return std::nullopt;
        }
        const float distance = .45F * kUnitsPerMetre;
        const float height = heightOffset * kUnitsPerMetre;
        const LightVector position{head.x + f->x * distance + u->x * height,
            head.y + f->y * distance + u->y * height, head.z + f->z * distance + u->z * height};
        return Finite(position) ? std::optional(position) : std::nullopt;
    }

    inline std::optional<LightVector> LightPosition(LightVector anchor, const CameraBasis& basis, const LightSettings& light)
    {
        if (!Finite(anchor) || !Finite(basis.front) || !Finite(basis.right) || !LightSession::Valid(light)) {
            return std::nullopt;
        }
        const float angle = light.direction * std::numbers::pi_v<float> / 4.0F;
        const float forward = -std::cos(angle), right = std::sin(angle);
        const float distance = light.distance * kUnitsPerMetre;
        const LightVector result{
            anchor.x + distance * (forward * basis.front.x + right * basis.right.x) +
                kUnitsPerMetre * (light.fine.horizontal * basis.right.x + light.fine.depth * basis.front.x),
            anchor.y + distance * (forward * basis.front.y + right * basis.right.y) +
                kUnitsPerMetre * (light.fine.horizontal * basis.right.y + light.fine.depth * basis.front.y),
            anchor.z + (light.heightOffset + light.fine.vertical) * kUnitsPerMetre
        };
        return Finite(result) ? std::optional(result) : std::nullopt;
    }

    inline std::optional<SpotlightBasis> AimPlacedSpotlight(LightVector position, LightVector anchor,
        const CameraBasis& basis, const LightSettings& light)
    {
        if (!Finite(position) || !Finite(anchor) || !Finite(basis.front) || !Finite(basis.right) ||
            !LightSession::Valid(light)) { return std::nullopt; }
        if (const auto aim = AimSpotlight(position, anchor)) { return aim; }
        // A fine offset can put the source exactly at its target. There is no unique
        // direction there: retain the base placement's aim without shifting the source
        // or stopping the other lights. Nonfinite inputs still fail closed.
        const float separation = std::hypot(position.x - anchor.x, position.y - anchor.y, position.z - anchor.z);
        if (!std::isfinite(separation) || separation >= 0.0001F) { return std::nullopt; }
        auto base = light;
        base.fine = {};
        const auto basePosition = LightPosition(anchor, basis, base);
        return basePosition ? AimSpotlight(*basePosition, anchor) : std::nullopt;
    }

    inline std::optional<SpotlightBasis> AimScreenPlacementSpotlight(
        LightVector position, LightVector anchor, const LightSettings& light)
    {
        // The screen-plane light deliberately has a wider 0.5-20m radius than
        // the three regular lights (0.5-12m). Do not run it through
        // LightSession::Valid/AimPlacedSpotlight or a valid value above 12m is
        // mistaken for corrupt scene data and stops the entire lighting session.
        if (!Finite(position) || !Finite(anchor) || !ValidScreenPlacementLightSettings(light)) {
            return std::nullopt;
        }
        return AimSpotlight(position, anchor);
    }
}
