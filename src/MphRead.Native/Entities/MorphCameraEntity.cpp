#include "MorphCameraEntity.hpp"

#include "../Formats/CollisionDetection.hpp"
#include "../Scene.hpp"
#include "Players/PlayerEntity.hpp"

#include <memory>
#include <utility>

namespace
{
    template <typename T>
    [[nodiscard]] T& RequireReference(const std::shared_ptr<T>& value)
    {
        if (!value)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

}

namespace MphRead::Entities
{
    const ::OpenTK::Mathematics::Vector3 MorphCameraEntity::_volumeColor(1.0F, 1.0F, 0.0F);
    const ::OpenTK::Mathematics::Vector3 FhMorphCameraEntity::_volumeColor(1.0F, 1.0F, 0.0F);

    MorphCameraEntity::MorphCameraEntity(
        MorphCameraEntityData data, std::string nodeName, Scene* scene)
        : EntityBase(EntityType::MorphCamera, std::move(nodeName), scene),
          _data(data)
    {
        Id = data.Header.EntityId;
        SetTransform(data.Header.FacingVector, data.Header.UpVector, data.Header.Position);
        _volume = CollisionVolume::Move(_data.Volume, Position);
        AddPlaceholderModel();
    }

    std::optional<::OpenTK::Mathematics::Vector4> MorphCameraEntity::OverrideColor() const
    {
        return _overrideColor;
    }

    bool MorphCameraEntity::Process()
    {
        Formats::CollisionResult discard{};
        if (_scene == nullptr)
        {
            throw System::NullReferenceException();
        }

        auto enumerator = _scene->GetPlayerEntities().GetEnumerator();
        while (enumerator.MoveNext())
        {
            std::shared_ptr<PlayerEntity> playerValue = enumerator.Current();
            if (!playerValue)
            {
                throw System::NullReferenceException();
            }
            PlayerEntity& player = *playerValue;

            if (player.IsAltForm())
            {
                if (player.MorphCamera() == nullptr)
                {
                    CollisionVolume playerVolume = player.Volume();
                    if (Formats::CollisionDetection::CheckVolumesOverlap(
                        &_volume, &playerVolume, discard))
                    {
                        player.SetMorphCamera(SharedFrom(this));
                        RequireReference(player.CameraInfo()).NodeRef = NodeRef;
                        player.RefreshExternalCamera();
                    }
                }
                else if (player.MorphCamera().get() == this)
                {
                    CollisionVolume playerVolume = player.Volume();
                    if (!Formats::CollisionDetection::CheckVolumesOverlap(
                        &_volume, &playerVolume, discard))
                    {
                        player.SetMorphCamera(nullptr);
                        player.ResumeOwnCamera();
                        player.RefreshExternalCamera();
                    }
                }
            }
            else if (player.MorphCamera() != nullptr)
            {
                player.SetMorphCamera(nullptr);
                player.ResumeOwnCamera();
            }
        }
        return EntityBase::Process();
    }

    void MorphCameraEntity::GetDisplayVolumes()
    {
        if (_scene == nullptr)
        {
            throw System::NullReferenceException();
        }
        if (_scene->ShowVolumes() == VolumeDisplay::MorphCamera)
        {
            AddVolumeItem(_volume, _volumeColor);
        }
    }

    FhMorphCameraEntity::FhMorphCameraEntity(
        FhMorphCameraEntityData data, Scene* scene)
        : EntityBase(EntityType::FhMorphCamera, scene),
          _data(data)
    {
        Id = data.Header.EntityId;
        SetTransform(data.Header.FacingVector, data.Header.UpVector, data.Header.Position);
        _volume = CollisionVolume::Move(_data.Volume, Position);
        AddPlaceholderModel();
    }

    std::optional<::OpenTK::Mathematics::Vector4> FhMorphCameraEntity::OverrideColor() const
    {
        return _overrideColor;
    }

    void FhMorphCameraEntity::GetDisplayVolumes()
    {
        if (_scene == nullptr)
        {
            throw System::NullReferenceException();
        }
        if (_scene->ShowVolumes() == VolumeDisplay::MorphCamera)
        {
            AddVolumeItem(_volume, _volumeColor);
        }
    }
}
