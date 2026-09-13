#pragma once

#include "../../Formats/Culling.hpp"
#include "../EnemyInstanceEntity.hpp"

#include <cstdint>

namespace MphRead::Entities
{
    class EnemySpawnEntity;
}

namespace MphRead::Entities::Enemies
{
    class Enemy03Entity : public EnemyInstanceEntity
    {
    public:
        Enemy03Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy03Entity(const Enemy03Entity&) = delete;
        Enemy03Entity& operator=(const Enemy03Entity&) = delete;
        Enemy03Entity(Enemy03Entity&&) = delete;
        Enemy03Entity& operator=(Enemy03Entity&&) = delete;

        [[nodiscard]] static bool Behavior00(Enemy03Entity* enemy);
        [[nodiscard]] static bool Behavior01(Enemy03Entity* enemy);
        [[nodiscard]] static bool Behavior02(Enemy03Entity* enemy);

    protected:
        void EnemyInitialize() override;
        void EnemyProcess() override;

    private:
        EnemySpawnEntity* const _spawner;
        float _idleRangeX = 0.0F;
        float _idleRangeZ = 0.0F;
        OpenTK::Mathematics::Vector3 _initialPos{};
        OpenTK::Mathematics::Vector3 _idleLimits{};
        OpenTK::Mathematics::Vector3 _field194{};
        OpenTK::Mathematics::Vector3 _field1A0{};
        float _bobAngle = 0.0F;
        float _bobOffset = 0.0F;
        float _bobSpeed = 0.0F;
        std::uint16_t _field170 = 0;
        std::uint16_t _field172 = 0;
        bool _teleportInAtInitial = true;
        std::int32_t _field18C = 0;

        void UpdateState();

        void State00();
        void State01();
        void State02();

        [[nodiscard]] bool Behavior00();
        [[nodiscard]] bool Behavior01();
        [[nodiscard]] bool Behavior02();
    };
}
