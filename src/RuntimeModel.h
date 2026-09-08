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
    inline constexpr std::size_t kRuntimeLightCount = kManualLightCount + 1;
    inline std::optional<LightSettings> RuntimeSettings(const Scene& scene, std::size_t slot)
    {
        if (slot < scene.size()) { return scene[slot]; }
        if (slot != kFaceRuntimeSlot) { return std::nullopt; }
        LightSettings face;
        face.placed = true;
        face.enabled = scene.face.enabled;
        face.intensity = scene.face.intensity;
        face.range = scene.face.range;
        face.castsShadow = false;
        face.shadowProjection = ShadowProjection::Omni;
        return face;  // fixed white, no manual slot's colour/position/shadow can leak in
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

    // Authored fixed tints, not Kelvin/photometric values. White preserves 0.1.8.
    // UI schematic colors are intentionally separate from renderer diffuse values.
    inline constexpr std::array<LightTint, kLightTypeCount> kRuntimeTints{ {
        { 1.0F, 1.0F, 1.0F },
        { 1.0F, 0.80F, 0.53F },
        { 1.0F, 0.60F, 0.29F },
        { 0.30F, 0.55F, 1.0F }
    } };
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
    // Skyrim's conventional 128 units = 6 feet; UI metres remain renderer-independent.
    inline constexpr float kUnitsPerMetre = 128.0F / 1.8288F;

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
}
