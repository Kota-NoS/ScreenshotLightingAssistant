#include <NifFile.hpp>
#include <Animation.hpp>
#include <Particles.hpp>
#include <Shaders.hpp>

#include <filesystem>
#include <iostream>

int main(int argc, char** argv)
{
    using namespace nifly;
    if (argc != 2) {
        std::cerr << "usage: inspect_nif <input.nif>\n";
        return 2;
    }

    NifFile nif{std::filesystem::path{argv[1]}};
    auto* root = nif.GetRootNode();
    std::cout << "root=" << (root ? root->GetBlockName() : "<none>") << '\n';
    const auto& header = nif.GetHeader();
    std::cout << "blocks=" << header.GetNumBlocks() << '\n';
    for (std::uint32_t index = 0; index < header.GetNumBlocks(); ++index) {
        auto* block = header.GetBlock<NiObject>(index);
        std::cout << '[' << index << "] " << (block ? block->GetBlockName() : "<none>");
        if (auto* object = dynamic_cast<NiObjectNET*>(block)) {
            std::cout << " name=\"" << object->name.get() << "\" controller=" << object->controllerRef.index;
        }
        if (auto* object = dynamic_cast<NiAVObject*>(block)) {
            std::cout << " flags=" << object->flags
                      << " transform=(" << object->transform.translation.x << ','
                      << object->transform.translation.y << ',' << object->transform.translation.z
                      << "; scale=" << object->transform.scale << ')';
        }
        if (auto* node = dynamic_cast<NiNode*>(block)) {
            std::cout << " children=";
            for (const auto& ref : node->childRefs) {
                std::cout << ref.index << ',';
            }
        }
        if (auto* controller = dynamic_cast<NiTimeController*>(block)) {
            std::cout << " flags=0x" << std::hex << controller->flags << std::dec
                      << " frequency=" << controller->frequency
                      << " start=" << controller->startTime << " stop=" << controller->stopTime
                      << " next=" << controller->nextControllerRef.index;
        }
        if (auto* system = dynamic_cast<NiParticleSystem*>(block)) {
            std::cout << " data=" << system->psysDataRef.index
                      << " shader=" << system->shaderPropertyRef.index
                      << " alpha=" << system->alphaPropertyRef.index
                      << " world=" << system->isWorldSpace << " modifiers=";
            for (const auto& ref : system->modifierRefs) {
                std::cout << ref.index << ',';
            }
        }
        if (auto* data = dynamic_cast<NiPSysData*>(block)) {
            std::cout << " vertices=" << data->GetNumVertices()
                      << " active=" << data->numActive
                      << " sizes=" << data->hasSizes
                      << " rotations=" << data->hasRotations
                      << " rotSpeeds=" << data->hasRotationSpeeds
                      << " textureIndices=" << data->hasTextureIndices
                      << " subtextures=" << data->subtexOffsets.size();
            for (const auto& uv : data->subtexOffsets) {
                std::cout << " [" << uv.x << ',' << uv.y << ',' << uv.z << ',' << uv.w << ']';
            }
        }
        if (auto* modifier = dynamic_cast<NiPSysModifier*>(block)) {
            std::cout << " modifier=\"" << modifier->name.get() << "\" order=" << modifier->order
                      << " active=" << modifier->isActive;
        }
        if (auto* emitter = dynamic_cast<NiPSysEmitter*>(block)) {
            std::cout << " speed=" << emitter->speed << "+/-" << emitter->speedVariation
                      << " decl=" << emitter->declination << "+/-" << emitter->declinationVariation
                      << " particleRadius=" << emitter->NiPSysEmitter::radius
                      << " life=" << emitter->lifeSpan << "+/-" << emitter->lifeSpanVariation
                      << " color=" << emitter->color.r << ',' << emitter->color.g << ','
                      << emitter->color.b << ',' << emitter->color.a;
        }
        if (auto* volume = dynamic_cast<NiPSysVolumeEmitter*>(block)) {
            std::cout << " emitterNode=" << volume->emitterNodeRef.index;
        }
        if (auto* cylinder = dynamic_cast<NiPSysCylinderEmitter*>(block)) {
            std::cout << " cylinderRadius=" << cylinder->radius << " cylinderHeight=" << cylinder->height;
        }
        if (auto* box = dynamic_cast<NiPSysBoxEmitter*>(block)) {
            std::cout << " box=" << box->width << 'x' << box->depth << 'x' << box->height;
        }
        if (auto* grow = dynamic_cast<NiPSysGrowFadeModifier*>(block)) {
            std::cout << " grow=" << grow->growTime << " fade=" << grow->fadeTime
                      << " baseScale=" << grow->baseScale;
        }
        if (auto* simpleColor = dynamic_cast<BSPSysSimpleColorModifier*>(block)) {
            std::cout << " fade=" << simpleColor->fadeInPercent << ',' << simpleColor->fadeOutPercent
                      << " colors=" << simpleColor->color1.r << ',' << simpleColor->color1.g << ','
                      << simpleColor->color1.b << ',' << simpleColor->color1.a << " -> "
                      << simpleColor->color2.r << ',' << simpleColor->color2.g << ','
                      << simpleColor->color2.b << ',' << simpleColor->color2.a << " -> "
                      << simpleColor->color3.r << ',' << simpleColor->color3.g << ','
                      << simpleColor->color3.b << ',' << simpleColor->color3.a;
        }
        if (auto* lod = dynamic_cast<BSPSysLODModifier*>(block)) {
            std::cout << " lod=" << lod->lodBeginDistance << ',' << lod->lodEndDistance
                      << " emit=" << lod->endEmitScale << " size=" << lod->endSize;
        }
        if (auto* rotation = dynamic_cast<NiPSysRotationModifier*>(block)) {
            std::cout << " rotationSpeed=" << rotation->initialSpeed << "+/-" << rotation->initialSpeedVariation
                      << " angle=" << rotation->initialAngle << "+/-" << rotation->initialAngleVariation
                      << " randomAxis=" << rotation->randomInitialAxis;
        }
        if (auto* bounds = dynamic_cast<NiPSysBoundUpdateModifier*>(block)) {
            std::cout << " updateSkip=" << bounds->updateSkip;
        }
        if (auto* gravity = dynamic_cast<NiPSysGravityModifier*>(block)) {
            std::cout << " gravityStrength=" << gravity->strength
                      << " axis=" << gravity->gravityAxis.x << ',' << gravity->gravityAxis.y << ',' << gravity->gravityAxis.z;
        }
        if (auto* subtex = dynamic_cast<BSPSysSubTexModifier*>(block)) {
            std::cout << " subtex=" << subtex->startFrame << "+/-" << subtex->startFrameVariation
                      << " ->" << subtex->endFrame << " loop=" << subtex->loopStartFrame
                      << "+/-" << subtex->loopStartFrameVariation << " count=" << subtex->frameCount
                      << "+/-" << subtex->frameCountVariation;
        }
        if (auto* vortex = dynamic_cast<NiPSysVortexFieldModifier*>(block)) {
            std::cout << " vortexMagnitude=" << vortex->magnitude
                      << " attenuation=" << vortex->attenuation
                      << " max=" << vortex->useMaxDistance << ':' << vortex->maxDistance
                      << " direction=" << vortex->direction.x << ',' << vortex->direction.y << ','
                      << vortex->direction.z;
        }
        if (auto* emitterController = dynamic_cast<NiPSysEmitterCtlr*>(block)) {
            std::cout << " emitterModifier=\"" << emitterController->modifierName.get()
                      << "\" interp=" << emitterController->interpolatorRef.index
                      << " visInterp=" << emitterController->visInterpolatorRef.index;
        }
        if (auto* floatInterpolator = dynamic_cast<NiFloatInterpolator*>(block)) {
            std::cout << " value=" << floatInterpolator->floatValue
                      << " data=" << floatInterpolator->dataRef.index;
        }
        if (auto* boolInterpolator = dynamic_cast<NiBoolInterpolator*>(block)) {
            std::cout << " value=" << static_cast<unsigned>(boolInterpolator->boolValue)
                      << " data=" << boolInterpolator->dataRef.index;
        }
        if (auto* shader = dynamic_cast<BSEffectShaderProperty*>(block)) {
            std::cout << " texture=\"" << shader->sourceTexture.get() << "\" f1=0x"
                      << std::hex << shader->shaderFlags1 << " f2=0x" << shader->shaderFlags2 << std::dec;
        }
        std::cout << '\n';
    }
    for (auto* shape : nif.GetShapes()) {
        std::cout << "shape=" << shape->GetBlockName();
        if (const auto* shader = dynamic_cast<const BSEffectShaderProperty*>(nif.GetShader(shape))) {
            std::cout << " effect f1=0x" << std::hex << shader->shaderFlags1
                      << " f2=0x" << shader->shaderFlags2 << std::dec
                      << " texture=" << shader->sourceTexture.get();
        }
        if (auto* parent = nif.GetParentNode(shape)) {
            std::cout << " parent=" << parent->GetBlockName();
        }
        std::cout << '\n';
    }
}
