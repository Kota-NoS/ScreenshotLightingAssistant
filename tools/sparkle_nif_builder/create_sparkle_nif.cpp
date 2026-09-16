#include <Animation.hpp>
#include <NifFile.hpp>
#include <Particles.hpp>
#include <Shaders.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace
{
    using namespace nifly;

    constexpr std::string_view kMoteTexturePath =
        "textures\\ScreenshotLightingAssistant\\effects\\luminous_mote_twinkle_atlas.dds";
    constexpr std::string_view kPetalTexturePath =
        "textures\\ScreenshotLightingAssistant\\effects\\sakura_petal_atlas.dds";
    constexpr std::string_view kStarTexturePath =
        "textures\\ScreenshotLightingAssistant\\effects\\magic_star.dds";
    constexpr std::string_view kLeafTexturePath =
        "textures\\ScreenshotLightingAssistant\\effects\\autumn_leaf_atlas.dds";
    constexpr std::string_view kFeatherTexturePath =
        "textures\\ScreenshotLightingAssistant\\effects\\white_feather_atlas.dds";
    constexpr std::string_view kPopStarTexturePath =
        "textures\\ScreenshotLightingAssistant\\effects\\pop_star_atlas.dds";
    constexpr std::string_view kSnowCrystalTexturePath =
        "textures\\ScreenshotLightingAssistant\\effects\\snow_crystal_atlas.dds";

    enum class Visual
    {
        Motes,
        Petals,
        Stars,
        Leaves,
        Feathers,
        PopStars,
        SnowCrystals
    };

    struct Variant
    {
        std::string filename;
        float emissionRate;
        std::uint16_t capacity;
        Visual visual{Visual::Motes};
        float lifetimeMultiplier{1.0F};
        int orbitDirection{};
    };

    template <class T>
    std::uint32_t Add(NifFile& nif, std::unique_ptr<T> block)
    {
        return nif.GetHeader().AddBlock(std::move(block));
    }

    void ConfigureModifier(NiPSysModifier& modifier, std::string_view name,
        std::uint32_t order, std::uint32_t systemId)
    {
        modifier.name.get() = std::string{name};
        modifier.order = order;
        modifier.targetRef.index = systemId;
        modifier.isActive = true;
    }

    int BuildField(const std::filesystem::path& output, const Variant& variant)
    {
        NiVersion version;
        version.SetFile(V20_2_0_7);
        version.SetUser(12);
        version.SetStream(100);

        NifFile nif;
        nif.Create(version);

        auto root = std::make_unique<BSFadeNode>();
        const bool motes = variant.visual == Visual::Motes;
        const bool petals = variant.visual == Visual::Petals;
        const bool stars = variant.visual == Visual::Stars;
        const bool leaves = variant.visual == Visual::Leaves;
        const bool feathers = variant.visual == Visual::Feathers;
        const bool popStars = variant.visual == Visual::PopStars;
        const bool snowCrystals = variant.visual == Visual::SnowCrystals;
        const bool neutralMaterial = petals || stars || feathers || snowCrystals;
        const float neutralPeakAlpha = stars ? 0.94F : snowCrystals ? 0.86F : 0.92F;
        const bool identityAtlas = leaves || feathers || popStars || snowCrystals;
        const bool atlasVisual = motes || petals || identityAtlas;
        const auto texturePath = petals ? kPetalTexturePath : stars ? kStarTexturePath :
            leaves ? kLeafTexturePath : feathers ? kFeatherTexturePath :
            popStars ? kPopStarTexturePath : snowCrystals ? kSnowCrystalTexturePath :
            kMoteTexturePath;
        const std::string_view effectName = petals ? "SLA Sakura Petals" :
            stars ? "SLA Magic Stars" : leaves ? "SLA Autumn Leaves" :
            feathers ? "SLA White Feathers" : popStars ? "SLA Pop Stars" :
            snowCrystals ? "SLA Snow Crystals" : "SLA Luminous Motes";
        const std::string_view emitterName = petals ? "SLA Petal Box Emitter" :
            stars ? "SLA Star Box Emitter" : leaves ? "SLA Leaf Box Emitter" :
            feathers ? "SLA Feather Box Emitter" : popStars ? "SLA Pop Star Box Emitter" :
            snowCrystals ? "SLA Snow Crystal Box Emitter" : "SLA Mote Box Emitter";

        root->name.get() = std::string{effectName};
        root->flags = 524302;
        root->transform.Clear();
        auto* rootPtr = root.get();
        nif.GetHeader().ReplaceBlock(0, std::move(root));

        auto emitterNode = std::make_unique<NiNode>();
        emitterNode->name.get() = std::string{effectName} + " Emitter";
        emitterNode->flags = 524302;
        emitterNode->transform.Clear();
        auto* emitterNodePtr = emitterNode.get();
        const auto emitterNodeId = Add(nif, std::move(emitterNode));
        rootPtr->childRefs.AddBlockRef(emitterNodeId);

        // Leaves, feathers and pop stars use four independent particle systems.
        // Each system owns one atlas row so visual identities never morph.
        const std::size_t particleSystemCount = identityAtlas ? 4U : 1U;
        for (std::size_t visualIndex = 0; visualIndex < particleSystemCount; ++visualIndex) {
        const auto systemSuffix = identityAtlas ? " " + std::to_string(visualIndex + 1U) : std::string{};
        const auto particleEmitterName = std::string{emitterName} + systemSuffix;

        auto system = std::make_unique<NiParticleSystem>();
        // Keep this distinct from the root node name so the runtime lookup
        // cannot stop at the BSFadeNode before reaching the particle system.
        system->name.get() = std::string{effectName} + " Particles" + systemSuffix;
        system->flags = 524302;
        system->transform.Clear();
        system->bounds = BoundingSphere{{0.0F, 0.0F, 42.0F},
            petals ? 210.0F : leaves ? 220.0F : feathers ? 220.0F :
            popStars ? 200.0F : snowCrystals ? 205.0F : stars ? 195.0F : 180.0F};
        // Ordinary particles remain in world space after release. Orbit meshes
        // deliberately simulate in target-local space so the runtime can rotate
        // the entire live field reliably around the actor.
        system->isWorldSpace = variant.orbitDirection == 0;
        auto* systemPtr = system.get();
        const auto systemId = Add(nif, std::move(system));
        emitterNodePtr->childRefs.AddBlockRef(systemId);

        auto data = std::make_unique<NiPSysData>();
        const auto perSystemCapacity = static_cast<std::uint16_t>(std::max<std::size_t>(1U,
            (static_cast<std::size_t>(variant.capacity) + particleSystemCount - 1U) /
                particleSystemCount));
        std::vector<Vector3> particleSlots(perSystemCapacity, {0.0F, 0.0F, 0.0F});
        data->Create(version, &particleSlots, nullptr, nullptr, nullptr);
        data->SetBounds(BoundingSphere{{0.0F, 0.0F, 42.0F},
            petals ? 210.0F : leaves ? 220.0F : feathers ? 220.0F :
            popStars ? 200.0F : snowCrystals ? 205.0F : stars ? 195.0F : 180.0F});
        data->hasSizes = true;
        data->hasRotations = false;
        data->hasRotationAngles = petals || stars || identityAtlas;
        data->hasRotationAxes = false;
        data->hasRotationSpeeds = true;
        if (atlasVisual) {
            data->hasTextureIndices = true;
            // Skyrim stores atlas entries as U offset, U width, V offset, V height.
            // Every enabled texture-index stream must contain at least one entry.
            if (identityAtlas) {
                const auto row = static_cast<float>(visualIndex);
                for (int column = 0; column < 8; ++column) {
                    data->subtexOffsets.push_back(Vector4{
                        static_cast<float>(column) * 0.125F, 0.125F,
                        row * 0.25F, 0.25F});
                }
            } else if (motes) {
                for (int column = 0; column < 8; ++column) {
                    data->subtexOffsets.push_back(Vector4{
                        static_cast<float>(column) * 0.125F, 0.125F, 0.0F, 1.0F});
                }
            } else {
                for (int row = 0; row < 2; ++row) {
                    for (int column = 0; column < 4; ++column) {
                        Vector4 frame{column * 0.25F, 0.25F, row * 0.50F, 0.50F};
                        data->subtexOffsets.push_back(frame);
                    }
                }
            }
        }
        data->consistencyFlags = CT_VOLATILE;
        const auto dataId = Add(nif, std::move(data));
        systemPtr->dataRef.index = dataId;
        systemPtr->psysDataRef.index = dataId;

        auto shader = std::make_unique<BSEffectShaderProperty>();
        shader->name.get() = petals ? "SLA Sakura Petal Shader" :
            stars ? "SLA Magic Star Shader" : leaves ? "SLA Autumn Leaf Shader" :
            feathers ? "SLA White Feather Shader" : popStars ? "SLA Pop Star Shader" :
            snowCrystals ? "SLA Snow Crystal Shader" : "SLA Luminous Mote Shader";
        shader->shaderFlags1 = 0x80000010U;
        shader->shaderFlags2 = 0x00000030U;
        shader->sourceTexture.get() = std::string{texturePath};
        shader->falloffStartOpacity = 1.0F;
        shader->falloffStopOpacity = 1.0F;
        shader->baseColor = {1.0F, 1.0F, 1.0F, 1.0F};
        // These atlases own their own colour and tonal range. A neutral material
        // makes UI brightness 1.00 the authored baseline instead of multiplying
        // another colour cast and sub-unity scale over the texture.
        shader->baseColorScale = neutralMaterial ? 1.0F : leaves ? 0.88F :
            popStars ? 0.86F : 1.0F;
        const auto shaderId = Add(nif, std::move(shader));
        systemPtr->shaderPropertyRef.index = shaderId;

        auto alpha = std::make_unique<NiAlphaProperty>();
        alpha->flags = 0x12ED;
        alpha->threshold = 0;
        const auto alphaId = Add(nif, std::move(alpha));
        systemPtr->alphaPropertyRef.index = alphaId;

        auto ageDeath = std::make_unique<NiPSysAgeDeathModifier>();
        ConfigureModifier(*ageDeath, "SLA Age/Death", 0, systemId);
        const auto ageDeathId = Add(nif, std::move(ageDeath));

        auto lod = std::make_unique<BSPSysLODModifier>();
        ConfigureModifier(*lod, "SLA LOD", 1, systemId);
        lod->lodBeginDistance = 0.08F;
        lod->lodEndDistance = 0.35F;
        lod->endEmitScale = 0.35F;
        lod->endSize = 0.80F;
        const auto lodId = Add(nif, std::move(lod));

        auto emitter = std::make_unique<NiPSysBoxEmitter>();
        ConfigureModifier(*emitter, particleEmitterName, 1000, systemId);
        // Direction is supplied by the runtime movement controls. Starting at
        // rest makes zero mean hover and lets negative vertical motion fall
        // immediately instead of first overcoming a baked-in upward launch.
        emitter->speed = 0.0F;
        emitter->speedVariation = 0.0F;
        emitter->declination = 0.0F;
        emitter->declinationVariation = 0.0F;
        emitter->planarAngle = 0.0F;
        emitter->planarAngleVariation = 3.14159265F;
        // Neutral materials keep all authored atlas colour and contrast.
        emitter->color = neutralMaterial ? Color4{1.0F, 1.0F, 1.0F, 1.0F} :
            leaves ? Color4{1.0F, 0.96F, 0.88F, 1.0F} :
            popStars ? Color4{1.0F, 1.0F, 1.0F, 1.0F} :
            Color4{1.0F, 0.94F, 0.72F, 1.0F};
        emitter->NiPSysEmitter::radius = petals ? 3.5F : leaves ? 3.8F :
            feathers ? 3.6F : popStars ? 3.25F : snowCrystals ? 3.15F :
            stars ? 3.1F : 2.7F;
        emitter->radiusVariation = petals ? 1.4F : leaves ? 1.6F :
            feathers ? 1.5F : popStars ? 1.3F : snowCrystals ? 1.25F :
            stars ? 1.2F : 1.1F;
        emitter->lifeSpan = (petals ? 5.4F : leaves ? 5.8F : feathers ? 6.2F :
            popStars ? 4.8F : snowCrystals ? 5.8F : stars ? 4.2F : 3.2F) *
            variant.lifetimeMultiplier;
        emitter->lifeSpanVariation = (petals ? 1.2F : leaves ? 1.4F : feathers ? 1.5F :
            popStars ? 1.1F : snowCrystals ? 1.35F : stars ? 1.0F : 0.9F) *
            variant.lifetimeMultiplier;
        emitter->emitterNodeRef.index = emitterNodeId;
        emitter->width = petals ? 130.0F : leaves ? 135.0F : feathers ? 132.0F :
            popStars ? 126.0F : snowCrystals ? 128.0F : 120.0F;
        emitter->depth = petals ? 130.0F : leaves ? 135.0F : feathers ? 132.0F :
            popStars ? 126.0F : snowCrystals ? 128.0F : 120.0F;
        emitter->height = petals ? 115.0F : leaves ? 120.0F : feathers ? 118.0F :
            popStars ? 112.0F : snowCrystals ? 116.0F : stars ? 110.0F : 105.0F;
        const auto emitterId = Add(nif, std::move(emitter));

        auto color = std::make_unique<BSPSysSimpleColorModifier>();
        ConfigureModifier(*color, petals ? "SLA Petal Fade" :
            stars ? "SLA Star Fade" : leaves ? "SLA Autumn Leaf Fade" :
            feathers ? "SLA White Feather Fade" : popStars ? "SLA Pop Star Fade" :
            snowCrystals ? "SLA Snow Crystal Fade" : "SLA Soft Gold Fade", 3000, systemId);
        color->fadeInPercent = petals ? 0.08F : leaves ? 0.06F : feathers ? 0.08F :
            popStars ? 0.08F : snowCrystals ? 0.10F : stars ? 0.10F : 0.12F;
        color->fadeOutPercent = petals ? 0.78F : leaves ? 0.82F : feathers ? 0.84F :
            popStars ? 0.78F : snowCrystals ? 0.82F : stars ? 0.68F : 0.72F;
        color->color1EndPercent = 0.20F;
        color->color2StartPercent = 0.12F;
        color->color2EndPercent = 0.78F;
        color->color3StartPercent = 0.68F;
        color->color1 = neutralMaterial ? Color4{1.0F, 1.0F, 1.0F, 0.0F} :
            leaves ? Color4{1.0F, 0.92F, 0.72F, 0.0F} :
            popStars ? Color4{1.0F, 1.0F, 1.0F, 0.0F} :
            Color4{1.0F, 0.95F, 0.78F, 0.0F};
        color->color2 = neutralMaterial ? Color4{1.0F, 1.0F, 1.0F, neutralPeakAlpha} :
            leaves ? Color4{1.0F, 0.98F, 0.90F, 0.94F} :
            popStars ? Color4{1.0F, 1.0F, 1.0F, 0.94F} :
            Color4{1.0F, 0.98F, 0.88F, 0.82F};
        color->color3 = neutralMaterial ? Color4{1.0F, 1.0F, 1.0F, 0.0F} :
            leaves ? Color4{0.88F, 0.50F, 0.22F, 0.0F} :
            popStars ? Color4{1.0F, 1.0F, 1.0F, 0.0F} :
            Color4{1.0F, 0.84F, 0.58F, 0.0F};
        const auto colorId = Add(nif, std::move(color));

        auto rotation = std::make_unique<NiPSysRotationModifier>();
        ConfigureModifier(*rotation, petals ? "SLA Petal Rotation" :
            stars ? "SLA Star Rotation" : leaves ? "SLA Leaf Tumble" :
            feathers ? "SLA Feather Tumble" : popStars ? "SLA Pop Star Rotation" :
            snowCrystals ? "SLA Snow Crystal Rotation" : "SLA Slow Rotation", 3000, systemId);
        rotation->initialSpeed = petals ? 0.95F : leaves ? 0.72F : feathers ? 0.62F :
            popStars ? 0.48F : snowCrystals ? 0.28F : stars ? 0.55F : 0.18F;
        rotation->initialSpeedVariation = petals ? 1.35F : leaves ? 1.15F :
            feathers ? 1.00F : popStars ? 0.85F : snowCrystals ? 0.48F :
            stars ? 0.70F : 0.30F;
        rotation->initialAngle = 0.0F;
        rotation->initialAngleVariation = 3.14159265F;
        rotation->randomSpeedSign = true;
        rotation->randomInitialAxis = true;
        const auto rotationId = Add(nif, std::move(rotation));

        std::optional<std::uint32_t> subTextureId;
        if (atlasVisual) {
            auto subTexture = std::make_unique<BSPSysSubTexModifier>();
            ConfigureModifier(*subTexture, petals ? "SLA Petal Tumble Frames" :
                leaves ? "SLA Leaf Tumble Frames" : feathers ? "SLA Feather Tumble Frames" :
                popStars ? "SLA Pop Star Fixed Frames" : snowCrystals ?
                "SLA Snow Crystal Fixed Frames" : "SLA Mote Twinkle Frames", 3000, systemId);
            if (identityAtlas) {
                // The system sees only the eight views in one identity row.
                // Random phase keeps nearby objects from tumbling in lockstep.
                subTexture->startFrame = 0.0F;
                subTexture->startFrameVariation = 8.0F;
                subTexture->endFrame = 7.0F;
                subTexture->loopStartFrame = 0.0F;
                subTexture->loopStartFrameVariation = 2.0F;
                subTexture->frameCount = 42.0F;
                subTexture->frameCountVariation = 18.0F;
            } else if (motes) {
                subTexture->startFrame = 0.0F;
                subTexture->startFrameVariation = 8.0F;
                subTexture->endFrame = 7.0F;
                subTexture->loopStartFrame = 0.0F;
                subTexture->loopStartFrameVariation = 4.0F;
                subTexture->frameCount = 32.0F;
                subTexture->frameCountVariation = 16.0F;
            } else {
                subTexture->startFrame = 0.0F;
                subTexture->startFrameVariation = 8.0F;
                subTexture->endFrame = 7.0F;
                subTexture->loopStartFrame = 0.0F;
                subTexture->loopStartFrameVariation = 2.0F;
                // A long, slightly varied cycle avoids synchronized flipbook motion.
                subTexture->frameCount = 42.0F;
                subTexture->frameCountVariation = 18.0F;
            }
            subTextureId = Add(nif, std::move(subTexture));
        }

        auto gravity = std::make_unique<NiPSysGravityModifier>();
        ConfigureModifier(*gravity, petals ? "SLA Petal Drift" :
            stars ? "SLA Star Drift" : leaves ? "SLA Leaf Drift" :
            feathers ? "SLA Feather Drift" : popStars ? "SLA Pop Star Drift" :
            snowCrystals ? "SLA Snow Crystal Drift" : "SLA Mote Drift", 4000, systemId);
        gravity->gravityObjRef.index = emitterNodeId;
        gravity->gravityAxis = Vector3{0.0F, 0.0F, -1.0F};
        gravity->decay = 0.0F;
        gravity->strength = 6.0F;
        gravity->forceType = FORCE_PLANAR;
        gravity->turbulence = 0.18F;
        gravity->turbulenceScale = 0.70F;
        gravity->worldAligned = true;
        const auto gravityId = Add(nif, std::move(gravity));

        auto position = std::make_unique<NiPSysPositionModifier>();
        ConfigureModifier(*position, "SLA Position", 6000, systemId);
        const auto positionId = Add(nif, std::move(position));

        auto boundUpdate = std::make_unique<NiPSysBoundUpdateModifier>();
        ConfigureModifier(*boundUpdate, "SLA Bound Update", 7000, systemId);
        boundUpdate->updateSkip = 0;
        const auto boundUpdateId = Add(nif, std::move(boundUpdate));

        std::vector<std::uint32_t> modifierIds{ageDeathId, lodId, emitterId, colorId, rotationId};
        if (subTextureId) { modifierIds.push_back(*subTextureId); }
        modifierIds.push_back(gravityId);
        modifierIds.push_back(positionId);
        modifierIds.push_back(boundUpdateId);
        for (const auto modifierId : modifierIds) {
            systemPtr->modifierRefs.AddBlockRef(modifierId);
        }

        auto rate = std::make_unique<NiFloatInterpolator>();
        rate->floatValue = variant.emissionRate / static_cast<float>(particleSystemCount);
        const auto rateId = Add(nif, std::move(rate));

        auto visibilityData = std::make_unique<NiBoolData>();
        visibilityData->data.SetInterpolationType(CONST_KEY);
        NiAnimationKey<std::uint8_t> visibleAtStart;
        visibleAtStart.type = CONST_KEY;
        visibleAtStart.time = 0.0F;
        visibleAtStart.value = 1;
        visibilityData->data.AddKey(visibleAtStart);
        NiAnimationKey<std::uint8_t> visibleAtEnd = visibleAtStart;
        // Match the runtime temp-effect lifetime so emission does not stop during
        // a long photography session.
        visibleAtEnd.time = 3600.0F;
        visibilityData->data.AddKey(visibleAtEnd);
        const auto visibilityDataId = Add(nif, std::move(visibilityData));

        auto visibility = std::make_unique<NiBoolInterpolator>();
        visibility->boolValue = 1;
        visibility->dataRef.index = visibilityDataId;
        const auto visibilityId = Add(nif, std::move(visibility));

        auto updateController = std::make_unique<NiPSysUpdateCtlr>();
        updateController->flags = 0x004CU;
        updateController->frequency = 1.0F;
        updateController->phase = 0.0F;
        updateController->startTime = 0.0F;
        updateController->stopTime = 0.0F;
        updateController->targetRef.index = systemId;
        const auto updateControllerId = Add(nif, std::move(updateController));

        auto emitterController = std::make_unique<NiPSysEmitterCtlr>();
        emitterController->flags = 0x0048U;
        emitterController->frequency = 1.0F;
        emitterController->phase = 0.0F;
        // Four identity-preserving systems would otherwise begin accumulating
        // fractional emissions on the same update and appear in obvious groups.
        // A sub-second stagger keeps the combined field visually continuous.
        emitterController->startTime = identityAtlas ?
            static_cast<float>(visualIndex) * 0.11F : 0.0F;
        emitterController->stopTime = 3600.0F;
        emitterController->targetRef.index = systemId;
        emitterController->nextControllerRef.index = updateControllerId;
        emitterController->interpolatorRef.index = rateId;
        emitterController->visInterpolatorRef.index = visibilityId;
        emitterController->modifierName.get() = particleEmitterName;
        const auto emitterControllerId = Add(nif, std::move(emitterController));
        systemPtr->controllerRef.index = emitterControllerId;
        }

        std::filesystem::create_directories(output.parent_path());
        if (const int result = nif.Save(output); result != 0) {
            std::cerr << "save failed for " << output << ": " << result << '\n';
            return 3;
        }

        NifFile verify{output};
        const auto& header = verify.GetHeader();
        std::size_t verifiedSystemCount = 0;
        std::size_t verifiedEmitterCount = 0;
        std::size_t verifiedControllerCount = 0;
        std::size_t verifiedShaderCount = 0;
        std::size_t verifiedGravityCount = 0;
        std::size_t verifiedSubTextureCount = 0;
        std::size_t verifiedDataCount = 0;
        bool verifiedSystemLayout = true;
        bool verifiedAtlasLayout = true;
        bool verifiedIdentityTumbleFrames = true;
        bool verifiedNeutralMaterial = true;
        for (std::uint32_t index = 0; index < header.GetNumBlocks(); ++index) {
            if (auto* system = header.GetBlock<NiParticleSystem>(index)) {
                ++verifiedSystemCount;
                verifiedSystemLayout = verifiedSystemLayout &&
                    system->isWorldSpace == (variant.orbitDirection == 0) &&
                    system->modifierRefs.GetSize() == (atlasVisual ? 9U : 8U);
            }
            if (header.GetBlock<NiPSysBoxEmitter>(index)) { ++verifiedEmitterCount; }
            if (header.GetBlock<NiPSysEmitterCtlr>(index)) { ++verifiedControllerCount; }
            if (auto* shader = header.GetBlock<BSEffectShaderProperty>(index)) {
                ++verifiedShaderCount;
                verifiedSystemLayout = verifiedSystemLayout &&
                    shader->sourceTexture.get() == texturePath;
                if (neutralMaterial) {
                    verifiedNeutralMaterial = verifiedNeutralMaterial &&
                        shader->baseColorScale == 1.0F;
                }
            }
            if (auto* emitter = header.GetBlock<NiPSysBoxEmitter>(index);
                neutralMaterial && emitter) {
                verifiedNeutralMaterial = verifiedNeutralMaterial &&
                    emitter->color.r == 1.0F && emitter->color.g == 1.0F &&
                    emitter->color.b == 1.0F && emitter->color.a == 1.0F;
            }
            if (auto* simpleColor = header.GetBlock<BSPSysSimpleColorModifier>(index);
                neutralMaterial && simpleColor) {
                verifiedNeutralMaterial = verifiedNeutralMaterial &&
                    simpleColor->color1.r == 1.0F && simpleColor->color1.g == 1.0F &&
                    simpleColor->color1.b == 1.0F && simpleColor->color1.a == 0.0F &&
                    simpleColor->color2.r == 1.0F && simpleColor->color2.g == 1.0F &&
                    simpleColor->color2.b == 1.0F &&
                    simpleColor->color2.a == neutralPeakAlpha &&
                    simpleColor->color3.r == 1.0F && simpleColor->color3.g == 1.0F &&
                    simpleColor->color3.b == 1.0F && simpleColor->color3.a == 0.0F;
            }
            if (header.GetBlock<NiPSysGravityModifier>(index)) { ++verifiedGravityCount; }
            if (auto* subTexture = header.GetBlock<BSPSysSubTexModifier>(index)) {
                ++verifiedSubTextureCount;
                if (identityAtlas) {
                    verifiedIdentityTumbleFrames = verifiedIdentityTumbleFrames &&
                        subTexture->startFrame == 0.0F &&
                        subTexture->startFrameVariation == 8.0F &&
                        subTexture->endFrame == 7.0F &&
                        subTexture->loopStartFrame == 0.0F &&
                        subTexture->loopStartFrameVariation == 2.0F &&
                        subTexture->frameCount == 42.0F &&
                        subTexture->frameCountVariation == 18.0F;
                }
            }
            if (auto* data = header.GetBlock<NiPSysData>(index)) {
                ++verifiedDataCount;
                if (atlasVisual) {
                    verifiedAtlasLayout = verifiedAtlasLayout && data->hasTextureIndices &&
                        data->subtexOffsets.size() == 8U;
                    if (identityAtlas && data->subtexOffsets.size() == 8U) {
                        const auto row = static_cast<float>(verifiedDataCount - 1U);
                        for (std::size_t column = 0; column < 8U; ++column) {
                            const auto& offset = data->subtexOffsets[column];
                            verifiedAtlasLayout = verifiedAtlasLayout &&
                                offset.x == static_cast<float>(column) * 0.125F &&
                                offset.y == 0.125F && offset.z == row * 0.25F &&
                                offset.w == 0.25F;
                        }
                    }
                } else {
                    verifiedAtlasLayout = verifiedAtlasLayout && !data->hasTextureIndices;
                }
            }
        }
        if (!dynamic_cast<BSFadeNode*>(verify.GetRootNode()) || !verifiedSystemLayout ||
            !verifiedAtlasLayout || !verifiedIdentityTumbleFrames ||
            !verifiedNeutralMaterial ||
            verifiedSystemCount != particleSystemCount ||
            verifiedEmitterCount != particleSystemCount ||
            verifiedControllerCount != particleSystemCount ||
            verifiedShaderCount != particleSystemCount ||
            verifiedGravityCount != particleSystemCount ||
            verifiedDataCount != particleSystemCount ||
            verifiedSubTextureCount != (atlasVisual ? particleSystemCount : 0U)) {
            std::cerr << "verification failed for " << output << '\n';
            return 4;
        }

        std::cout << output.string() << " rate=" << variant.emissionRate
                  << " capacity=" << variant.capacity
                  << " lifetime=" << variant.lifetimeMultiplier << "x"
                  << " orbit=" << variant.orbitDirection << "\n";
        return 0;
    }
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "usage: particle_field_nif_builder <output-directory>\n";
        return 2;
    }

    struct BaseVariant
    {
        std::string_view effect;
        std::string_view density;
        float emissionRate;
        std::uint16_t capacity;
        Visual visual;
    };
    constexpr std::array bases{
        BaseVariant{"luminous_motes", "low", 4.0F, 32, Visual::Motes},
        BaseVariant{"luminous_motes", "standard", 8.0F, 48, Visual::Motes},
        BaseVariant{"luminous_motes", "high", 14.0F, 72, Visual::Motes},
        BaseVariant{"sakura_petals", "low", 3.0F, 32, Visual::Petals},
        BaseVariant{"sakura_petals", "standard", 6.0F, 48, Visual::Petals},
        BaseVariant{"sakura_petals", "high", 10.0F, 64, Visual::Petals},
        BaseVariant{"magic_stars", "low", 3.0F, 32, Visual::Stars},
        BaseVariant{"magic_stars", "standard", 6.0F, 48, Visual::Stars},
        BaseVariant{"magic_stars", "high", 10.0F, 72, Visual::Stars},
        BaseVariant{"autumn_leaves", "low", 3.0F, 32, Visual::Leaves},
        BaseVariant{"autumn_leaves", "standard", 6.0F, 48, Visual::Leaves},
        BaseVariant{"autumn_leaves", "high", 10.0F, 72, Visual::Leaves},
        BaseVariant{"white_feathers", "low", 3.0F, 32, Visual::Feathers},
        BaseVariant{"white_feathers", "standard", 6.0F, 48, Visual::Feathers},
        BaseVariant{"white_feathers", "high", 10.0F, 72, Visual::Feathers},
        BaseVariant{"pop_stars", "low", 3.0F, 32, Visual::PopStars},
        BaseVariant{"pop_stars", "standard", 6.0F, 48, Visual::PopStars},
        BaseVariant{"pop_stars", "high", 10.0F, 72, Visual::PopStars},
        BaseVariant{"snow_crystals", "low", 3.0F, 32, Visual::SnowCrystals},
        BaseVariant{"snow_crystals", "standard", 6.0F, 48, Visual::SnowCrystals},
        BaseVariant{"snow_crystals", "high", 10.0F, 72, Visual::SnowCrystals},
    };
    constexpr std::array lifetimePercents{50, 75, 100, 125, 150, 175, 200};

    for (const auto& base : bases) {
        for (const int percent : lifetimePercents) {
            const float multiplier = static_cast<float>(percent) / 100.0F;
            const auto capacity = static_cast<std::uint16_t>(std::min(192.0F,
                std::ceil(static_cast<float>(base.capacity) * std::max(1.0F, multiplier))));
            struct MotionVariant { std::string_view suffix; int orbitDirection; };
            constexpr std::array motions{
                MotionVariant{"", 0},
                MotionVariant{"_orbit_cw", 1},
                MotionVariant{"_orbit_ccw", -1}
            };
            for (const auto& motion : motions) {
                Variant variant{
                    std::string{base.effect} + "_" + std::string{base.density} +
                        std::string{motion.suffix} +
                        "_l" + (percent < 100 ? "0" : "") + std::to_string(percent) + ".nif",
                    base.emissionRate,
                    capacity,
                    base.visual,
                    multiplier,
                    motion.orbitDirection
                };
                if (const int result = BuildField(std::filesystem::path{argv[1]} /
                    variant.filename, variant); result != 0) {
                    return result;
                }
            }
        }
    }
    return 0;
}
