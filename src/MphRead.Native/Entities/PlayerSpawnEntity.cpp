#include "PlayerSpawnEntity.hpp"

#include "../Features.hpp"
#include "../GameState.hpp"
#include "../Messaging.hpp"
#include "../Scene.hpp"

#include <any>
#include <cstdint>
#include <typeinfo>
#include <utility>

namespace
{
    [[nodiscard]] std::int32_t UnboxInt32(const std::any& value)
    {
        if (!value.has_value())
        {
            throw System::NullReferenceException();
        }
        if (value.type() != typeid(std::int32_t))
        {
            throw System::InvalidCastException();
        }
        return std::any_cast<std::int32_t>(value);
    }

    [[nodiscard]] std::int32_t GetRoomId(MphRead::Scene* scene)
    {
        if (scene == nullptr)
        {
            throw System::NullReferenceException();
        }
        return scene->RoomId;
    }
}

namespace MphRead::Entities
{
    PlayerSpawnEntity::PlayerSpawnEntity(
        PlayerSpawnEntityData data,
        std::string nodeName,
        Scene* scene)
        : EntityBase(EntityType::PlayerSpawn, std::move(nodeName), scene),
          _data(data)
    {
        Id = data.Header.EntityId;
        AddPlaceholderModel();
    }

    PlayerSpawnEntityData PlayerSpawnEntity::Data() const
    {
        return _data;
    }

    bool PlayerSpawnEntity::IsActive() const
    {
        return _active;
    }

    bool PlayerSpawnEntity::Availability() const
    {
        return _data.Availability != 0;
    }

    std::uint16_t PlayerSpawnEntity::Cooldown() const
    {
        return _cooldown;
    }

    void PlayerSpawnEntity::SetCooldown(std::uint16_t value)
    {
        _cooldown = value;
    }

    std::optional<::OpenTK::Mathematics::Vector4> PlayerSpawnEntity::OverrideColor() const
    {
        return _overrideColor;
    }

    void PlayerSpawnEntity::Initialize()
    {
        EntityBase::Initialize();

        SetTransform(
            _data.Header.FacingVector,
            _data.Header.UpVector,
            _data.Header.Position);

        if (GameState::Mode == GameMode::SinglePlayer)
        {
            const bool active
                = Cheats::SkipPlanetIntros ? true : (_data.Active != 0);

            StorySave* storySave = GameState::StorySave;
            const std::int32_t roomId = GetRoomId(_scene);

            if (storySave == nullptr)
            {
                throw System::NullReferenceException();
            }

            _active = storySave->InitRoomState(roomId, Id, active) != 0;
        }
        else
        {
            _active = _data.Active != 0;
        }
    }

    bool PlayerSpawnEntity::Process()
    {
        if (_cooldown > 0)
        {
            _cooldown--;
        }

        return EntityBase::Process();
    }

    void PlayerSpawnEntity::HandleMessage(MessageInfo info)
    {
        if (info.Message == Message::Activate
            || (info.Message == Message::SetActive
                && UnboxInt32(info.Param1) != 0))
        {
            _active = true;

            if (GameState::Mode == GameMode::SinglePlayer)
            {
                StorySave* storySave = GameState::StorySave;
                const std::int32_t roomId = GetRoomId(_scene);

                if (storySave == nullptr)
                {
                    throw System::NullReferenceException();
                }

                storySave->SetRoomState(roomId, Id, 3);
            }
        }
        else if (info.Message == Message::SetActive
            && UnboxInt32(info.Param1) == 0)
        {
            _active = false;

            if (GameState::Mode == GameMode::SinglePlayer)
            {
                StorySave* storySave = GameState::StorySave;
                const std::int32_t roomId = GetRoomId(_scene);

                if (storySave == nullptr)
                {
                    throw System::NullReferenceException();
                }

                storySave->SetRoomState(roomId, Id, 1);
            }
        }
    }
}
