#include "PlayerSpawnEntity.hpp"

#include "../Features.hpp"
#include "../GameState.hpp"
#include "../Messaging.hpp"
#include "../MemoryArrays.hpp"
#include "../Scene.hpp"

#include <any>
#include <cstdint>
#include <memory>
#include <utility>

namespace
{
    [[nodiscard]] std::int32_t UnboxInt32(const MphRead::MessageObject& value)
    {
        if (!value || !value->has_value())
        {
            throw MphRead::Memory::Detail::NullReferenceException();
        }
        try
        {
            return std::any_cast<std::int32_t>(*value);
        }
        catch (const std::bad_any_cast&)
        {
            throw MphRead::Memory::Detail::InvalidCastException();
        }
    }

    [[nodiscard]] std::int32_t GetRoomId(MphRead::Scene* scene)
    {
        if (scene == nullptr)
        {
            throw MphRead::Memory::Detail::NullReferenceException();
        }
        return scene->RoomId();
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

        if (GameState::Mode() == GameMode::SinglePlayer)
        {
            const bool active
                = Cheats::SkipPlanetIntros() ? true : (_data.Active != 0);

            std::shared_ptr<StorySave> storySave = GameState::StorySave;
            const std::int32_t roomId = GetRoomId(_scene);
            const std::int32_t id = Id;

            if (storySave == nullptr)
            {
                throw Memory::Detail::NullReferenceException();
            }

            _active = storySave->InitRoomState(roomId, id, active) != 0;
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

            if (GameState::Mode() == GameMode::SinglePlayer)
            {
                std::shared_ptr<StorySave> storySave = GameState::StorySave;
                const std::int32_t roomId = GetRoomId(_scene);
                const std::int32_t id = Id;

                if (storySave == nullptr)
                {
                    throw Memory::Detail::NullReferenceException();
                }

                storySave->SetRoomState(roomId, id, 3);
            }
        }
        else if (info.Message == Message::SetActive
            && UnboxInt32(info.Param1) == 0)
        {
            _active = false;

            if (GameState::Mode() == GameMode::SinglePlayer)
            {
                std::shared_ptr<StorySave> storySave = GameState::StorySave;
                const std::int32_t roomId = GetRoomId(_scene);
                const std::int32_t id = Id;

                if (storySave == nullptr)
                {
                    throw Memory::Detail::NullReferenceException();
                }

                storySave->SetRoomState(roomId, id, 1);
            }
        }
    }
}
