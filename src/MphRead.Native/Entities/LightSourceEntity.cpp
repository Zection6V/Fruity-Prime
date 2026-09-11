#include "LightSourceEntity.hpp"

#include "../Renderer.hpp"

namespace MphRead::Entities
{
    LightSourceEntity::LightSourceEntity(LightSourceEntityData data, Scene* scene)
        : EntityBase(EntityType::LightSource, scene),
          _data(data)
    {
        Id = data.Header.EntityId;
        SetTransform(data.Header.FacingVector, data.Header.UpVector, data.Header.Position);
        _volume = CollisionVolume::Move(_data.Volume, Position);
        _light1Enabled = _data.Light1Enabled != 0;
        _light1Vector = _data.Light1Vector.ToFloatVector();
        _light1Color = _data.Light1Color.AsVector3();
        _light2Enabled = _data.Light2Enabled != 0;
        _light2Vector = _data.Light2Vector.ToFloatVector();
        _light2Color = _data.Light2Color.AsVector3();
        AddPlaceholderModel();
    }

    CollisionVolume LightSourceEntity::Volume() const
    {
        return _volume;
    }

    bool LightSourceEntity::Light1Enabled() const
    {
        return _light1Enabled;
    }

    ::OpenTK::Mathematics::Vector3 LightSourceEntity::Light1Vector() const
    {
        return _light1Vector;
    }

    ::OpenTK::Mathematics::Vector3 LightSourceEntity::Light1Color() const
    {
        return _light1Color;
    }

    bool LightSourceEntity::Light2Enabled() const
    {
        return _light2Enabled;
    }

    ::OpenTK::Mathematics::Vector3 LightSourceEntity::Light2Vector() const
    {
        return _light2Vector;
    }

    ::OpenTK::Mathematics::Vector3 LightSourceEntity::Light2Color() const
    {
        return _light2Color;
    }

    std::optional<::OpenTK::Mathematics::Vector4> LightSourceEntity::OverrideColor() const
    {
        return _overrideColor;
    }

    void LightSourceEntity::GetDisplayVolumes()
    {
        if (_scene->ShowVolumes == VolumeDisplay::LightColor1
            || _scene->ShowVolumes == VolumeDisplay::LightColor2)
        {
            ::OpenTK::Mathematics::Vector3 color = ::OpenTK::Mathematics::Vector3::Zero;
            if (_scene->ShowVolumes == VolumeDisplay::LightColor1)
            {
                if (_data.Light1Enabled != 0)
                {
                    color = Light1Color();
                }
            }
            else if (_scene->ShowVolumes == VolumeDisplay::LightColor2)
            {
                if (_data.Light2Enabled != 0)
                {
                    color = Light2Color();
                }
            }
            AddVolumeItem(Volume(), color);
        }
    }
}
