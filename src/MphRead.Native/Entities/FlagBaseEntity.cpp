#include "FlagBaseEntity.hpp"

#include "../GameState.hpp"
#include "../Renderer.hpp"
#include "../Scene.hpp"
#include "OctolithFlagEntity.hpp"
#include "Players/PlayerEntity.hpp"

#include <memory>
#include <utility>

namespace MphRead::Entities
{
    FlagBaseEntity::FlagBaseEntity(FlagBaseEntityData data, Scene* scene)
        : EntityBase(EntityType::FlagBase, scene),
          _data(data)
    {
        Id = data.Header.EntityId;
        SetTransform(data.Header.FacingVector, data.Header.UpVector, data.Header.Position);
        _volume = CollisionVolume::Move(_data.Volume, Position);

        const GameMode mode = GameState::Mode();
        if (mode == GameMode::Capture)
        {
            AddPlaceholderModel();
        }
        else if (mode == GameMode::Bounty || mode == GameMode::BountyTeams)
        {
            SetUpModel("flagbase_cap");
        }
        _capture = mode == GameMode::Capture;
    }

    FlagBaseEntityData FlagBaseEntity::Data() const
    {
        return _data;
    }

    std::shared_ptr<Formats::NodeData3> FlagBaseEntity::ClosestNode() const noexcept
    {
        return _closestNode;
    }

    void FlagBaseEntity::SetClosestNode(std::shared_ptr<Formats::NodeData3> value) noexcept
    {
        _closestNode = std::move(value);
    }

    std::optional<OpenTK::Mathematics::Vector4> FlagBaseEntity::OverrideColor() const
    {
        return _overrideColor;
    }

    bool FlagBaseEntity::Process()
    {
        (void)EntityBase::Process();

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

            if (player.OctolithFlag() == nullptr
                || (_capture && std::cmp_not_equal(player.TeamIndex(), _data.TeamId)))
            {
                continue;
            }

            if (_volume.TestPoint(player.Position))
            {
                if (_capture && !CheckOwnOctolith(player))
                {
                    if (&player == PlayerEntity::Main().get())
                    {
                        std::shared_ptr<PlayerEntity> main = PlayerEntity::Main();
                        if (main == nullptr)
                        {
                            throw System::NullReferenceException();
                        }
                        main->QueueHudMessage(128, 50, 1.0F / 1000.0F, 0, 232);
                    }
                    continue;
                }

                std::shared_ptr<OctolithFlagEntity> octolithFlag = player.OctolithFlag();
                if (!octolithFlag)
                {
                    throw System::NullReferenceException();
                }
                octolithFlag->OnCaptured();
            }
        }
        return true;
    }

    bool FlagBaseEntity::CheckOwnOctolith(PlayerEntity& player)
    {
        if (_scene == nullptr)
        {
            throw System::NullReferenceException();
        }

        auto enumerator = _scene->GetOctolithFlagEntities().GetEnumerator();
        while (enumerator.MoveNext())
        {
            std::shared_ptr<OctolithFlagEntity> octolithValue = enumerator.Current();
            if (!octolithValue)
            {
                throw System::NullReferenceException();
            }
            OctolithFlagEntity& octolith = *octolithValue;

            if (octolith.Data().TeamId == player.TeamIndex() && !octolith.AtBase())
            {
                return false;
            }
        }
        return true;
    }

    void FlagBaseEntity::GetDisplayVolumes()
    {
        if (_scene == nullptr)
        {
            throw System::NullReferenceException();
        }

        if (_scene->ShowVolumes() == VolumeDisplay::FlagBase)
        {
            AddVolumeItem(_volume, OpenTK::Mathematics::Vector3(1.0F, 1.0F, 1.0F));
        }
    }
}
