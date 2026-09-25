#include "HunterPreview.hpp"

#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Metadata/Metadata.hpp"
#include "../../Read.hpp"

#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>

namespace MphRead::Mods::Render
{
namespace
{
::OpenTK::Mathematics::Matrix4 CreateFacing()
{
    constexpr float degreesToRadians = 0.01745329251994329576923690768489F;
    const float angle = 180.0F * degreesToRadians;
    const float cos = std::cos(angle);
    const float sin = std::sin(angle);
    return ::OpenTK::Mathematics::Matrix4(
        ::OpenTK::Mathematics::Vector4(cos, 0.0F, -sin, 0.0F),
        ::OpenTK::Mathematics::Vector4(0.0F, 1.0F, 0.0F, 0.0F),
        ::OpenTK::Mathematics::Vector4(sin, 0.0F, cos, 0.0F),
        ::OpenTK::Mathematics::Vector4(0.0F, 0.0F, 0.0F, 1.0F));
}

std::string HunterString(Hunter hunter)
{
    return ::MphRead::ToString(hunter);
}
}

const LightInfo HunterPreviewEntity::_light(
    ::OpenTK::Mathematics::Vector3(-0.35F, -0.30F, -0.89F),
    ::OpenTK::Mathematics::Vector3(1.0F, 0.97F, 0.92F),
    ::OpenTK::Mathematics::Vector3(0.60F, 0.20F, 0.77F),
    ::OpenTK::Mathematics::Vector3(0.32F, 0.36F, 0.48F));

const ::OpenTK::Mathematics::Matrix4 HunterPreviewEntity::_facing = CreateFacing();

HunterPreviewEntity::HunterPreviewEntity(Scene* scene)
    : EntityBase(EntityType::Model, scene)
{
}

bool HunterPreviewEntity::Ready() const noexcept
{
    return _model != nullptr;
}

void HunterPreviewEntity::SetUp(Hunter hunter, std::int32_t recolor)
{
    if (_model != nullptr && hunter == _hunter && recolor == _recolor)
    {
        return;
    }
    if (hunter != _hunter || _model == nullptr)
    {
        try
        {
            const auto models = Metadata::HunterModels.find(hunter);
            if (models == Metadata::HunterModels.end() || models->second.empty())
            {
                return;
            }
            std::shared_ptr<ModelInstance> inst = Read::GetModelInstance(models->second[0]);
            _models = ModelList{};
            _models.Add(inst);
            _model = inst;
            _hunter = hunter;
            inst->SetAnimation(static_cast<std::int32_t>(MphRead::Entities::PlayerAnimation::Idle));
        }
        catch (const std::exception& ex)
        {
            std::cout << "[endscreen] no model for " << HunterString(hunter)
                << ": " << ex.what() << '\n';
            _model.reset();
            return;
        }
    }
    _recolor = recolor;
    SetRecolor(recolor);
}

void HunterPreviewEntity::Step()
{
    if (_model != nullptr)
    {
        _model->UpdateAnimFrames();
    }
}

void HunterPreviewEntity::Reset()
{
}

LightInfo HunterPreviewEntity::GetLightInfo()
{
    return _light;
}

void HunterPreviewEntity::GetDrawInfo()
{
    if (_model == nullptr)
    {
        return;
    }
    UpdateTransforms(*_model, _facing, _recolor < 0 ? 0 : _recolor);
    GetDrawItems(*_model, 0, _light);
}
}
