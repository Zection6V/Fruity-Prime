#pragma once

#include "../Formats/Entity.hpp"
#include "../Formats/Types.hpp"
#include "EntityBase.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace MphRead::Entities
{
    class PlayerSpawnEntity : public EntityBase
    {
    public:
        PlayerSpawnEntity(PlayerSpawnEntityData data, std::string nodeName, Scene* scene);

        PlayerSpawnEntity(const PlayerSpawnEntity&) = delete;
        PlayerSpawnEntity& operator=(const PlayerSpawnEntity&) = delete;
        PlayerSpawnEntity(PlayerSpawnEntity&&) = delete;
        PlayerSpawnEntity& operator=(PlayerSpawnEntity&&) = delete;

        [[nodiscard]] PlayerSpawnEntityData Data() const;
        [[nodiscard]] bool IsActive() const;
        [[nodiscard]] bool Availability() const;
        [[nodiscard]] std::uint16_t Cooldown() const;
        void SetCooldown(std::uint16_t value);

        void Initialize() override;
        bool Process() override;
        void HandleMessage(MessageInfo info) override;

    protected:
        [[nodiscard]] std::optional<::OpenTK::Mathematics::Vector4> OverrideColor() const override;

    private:
        const PlayerSpawnEntityData _data;
        const std::optional<::OpenTK::Mathematics::Vector4> _overrideColor
            = ColorRgb(0x7F, 0x00, 0x00).AsVector4();
        bool _active = false;
        std::uint16_t _cooldown = 0;
    };
}
