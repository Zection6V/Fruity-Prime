#pragma once

#include "../../Formats/Culling.hpp"
#include "../EnemyInstanceEntity.hpp"

#include <array>
#include <cstdint>

namespace MphRead::Entities
{
    class BombEntity;
    class EnemySpawnEntity;
    class PlayerEntity;
}

namespace MphRead::Entities::Enemies
{
    class Enemy02Entity : public EnemyInstanceEntity
    {
    public:
        Enemy02Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy02Entity(const Enemy02Entity&) = delete;
        Enemy02Entity& operator=(const Enemy02Entity&) = delete;
        Enemy02Entity(Enemy02Entity&&) = delete;
        Enemy02Entity& operator=(Enemy02Entity&&) = delete;

        [[nodiscard]] bool Field1D0() const noexcept;

        void Func216469C(
            OpenTK::Mathematics::Vector3 point1,
            OpenTK::Mathematics::Vector3 point2,
            float sign);
        void UpdateAttached(PlayerEntity* player);
        [[nodiscard]] bool CheckTemroidHitByBomb(BombEntity* bomb);

        [[nodiscard]] static bool Behavior00(Enemy02Entity* enemy);
        [[nodiscard]] static bool Behavior01(Enemy02Entity* enemy);
        [[nodiscard]] static bool Behavior02(Enemy02Entity* enemy);
        [[nodiscard]] static bool Behavior03(Enemy02Entity* enemy);
        [[nodiscard]] static bool Behavior04(Enemy02Entity* enemy);
        [[nodiscard]] static bool Behavior05(Enemy02Entity* enemy);
        [[nodiscard]] static bool Behavior06(Enemy02Entity* enemy);
        [[nodiscard]] static bool Behavior07(Enemy02Entity* enemy);
        [[nodiscard]] static bool Behavior08(Enemy02Entity* enemy);
        [[nodiscard]] static bool Behavior09(Enemy02Entity* enemy);
        [[nodiscard]] static bool Behavior10(Enemy02Entity* enemy);
        [[nodiscard]] static bool Behavior11(Enemy02Entity* enemy);
        [[nodiscard]] static bool Behavior12(Enemy02Entity* enemy);

    protected:
        void EnemyInitialize() override;
        void Detach() override;
        void EnemyProcess() override;

    private:
        EnemySpawnEntity* const _spawner;
        std::array<OpenTK::Mathematics::Vector3, 4> _idlePoints{};
        std::int32_t _field170 = 0;
        std::uint8_t _field1A4 = 0;
        std::uint8_t _drainDamageTimer = 0;
        float _field1B8 = 0.0F;
        OpenTK::Mathematics::Vector3 _field1BC{};
        bool _field1D0 = false;
        bool _hitByBomb = false;

        void Func21648A4();
        [[nodiscard]] static bool FloatEqual(float a, float b) noexcept;

        void State00();
        void State01();
        void State02();
        void UpdateHeight(float sign);
        void State03();
        void State04();
        void State05();
        void State06();
        void State07();
        void State08();
        void State09();
        void State10();

        [[nodiscard]] bool Behavior00();
        [[nodiscard]] bool Behavior01();
        [[nodiscard]] bool Behavior02();
        [[nodiscard]] bool Behavior03();
        [[nodiscard]] bool Behavior04();
        [[nodiscard]] bool Behavior05();
        [[nodiscard]] bool Behavior06();
        [[nodiscard]] bool Behavior07();
        [[nodiscard]] bool Behavior08();
        [[nodiscard]] bool Behavior09();
        [[nodiscard]] bool Behavior10();
        [[nodiscard]] bool Behavior11();
        [[nodiscard]] bool Behavior12();
    };
}
