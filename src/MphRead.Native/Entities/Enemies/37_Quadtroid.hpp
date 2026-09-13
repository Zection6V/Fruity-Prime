#pragma once

#include "../../Formats/Culling.hpp"
#include "../EnemyInstanceEntity.hpp"

#include <cstdint>
#include <functional>

namespace MphRead::Entities
{
    class EnemySpawnEntity;
    class PlayerEntity;
}

namespace MphRead::Entities::Enemies
{
    class Enemy37Entity : public EnemyInstanceEntity
    {
    public:
        Enemy37Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy37Entity(const Enemy37Entity&) = delete;
        Enemy37Entity& operator=(const Enemy37Entity&) = delete;
        Enemy37Entity(Enemy37Entity&&) = delete;
        Enemy37Entity& operator=(Enemy37Entity&&) = delete;

        void UpdateAttached(PlayerEntity* player);

    protected:
        void EnemyInitialize() override;
        void EnemyProcess() override;
        [[nodiscard]] bool EnemyTakeDamage(EntityBase* source) override;
        [[nodiscard]] bool EnemyGetDrawInfo() override;

    private:
        enum class QuadtroidFlags : std::int32_t
        {
            None = 0,
            Bit0 = 1,
            Bit1 = 2,
            Bit2 = 4,
            Bit3 = 8,
            Bit4 = 0x10,
            Bit5 = 0x20,
            Bit6 = 0x40,
            Bit7 = 0x80
        };

        friend constexpr QuadtroidFlags operator|(
            QuadtroidFlags left, QuadtroidFlags right) noexcept
        {
            return static_cast<QuadtroidFlags>(
                static_cast<std::int32_t>(left)
                | static_cast<std::int32_t>(right));
        }

        friend constexpr QuadtroidFlags operator&(
            QuadtroidFlags left, QuadtroidFlags right) noexcept
        {
            return static_cast<QuadtroidFlags>(
                static_cast<std::int32_t>(left)
                & static_cast<std::int32_t>(right));
        }

        friend constexpr QuadtroidFlags operator~(
            QuadtroidFlags value) noexcept
        {
            return static_cast<QuadtroidFlags>(
                ~static_cast<std::int32_t>(value));
        }

        friend constexpr QuadtroidFlags& operator|=(
            QuadtroidFlags& left, QuadtroidFlags right) noexcept
        {
            left = left | right;
            return left;
        }

        friend constexpr QuadtroidFlags& operator&=(
            QuadtroidFlags& left, QuadtroidFlags right) noexcept
        {
            left = left & right;
            return left;
        }

        EnemySpawnEntity* const _spawner;
        CollisionVolume _volume1{};
        OpenTK::Mathematics::Vector3 _rightVector{};
        OpenTK::Mathematics::Vector3 _field1B8{};
        OpenTK::Mathematics::Vector3 _field1D0{};
        OpenTK::Mathematics::Vector3 _field1DC{};
        OpenTK::Mathematics::Vector3 _field1E8{};
        OpenTK::Mathematics::Vector3 _field224{};

        std::uint16_t _prevHealth = 0;
        std::uint16_t _damageTaken = 0;
        PlayerEntity* _target = nullptr;
        QuadtroidFlags _flags = QuadtroidFlags::None;
        bool _hitByBomb = false;
        bool _hitByBeam = false;
        std::int32_t _field238 = 0;
        std::int32_t _field23C = 0;

        std::function<void()> _field234{};

        void Func214EF68(std::function<void()> func);
        void Func214E708(std::function<void()> func);
        [[nodiscard]] bool Func214D5B8(std::function<bool(PlayerEntity*)> func);
        void Func214DCB8();
        void UpdateCollision();
        [[nodiscard]] bool HandleCollision(
            OpenTK::Mathematics::Vector3& dest,
            OpenTK::Mathematics::Vector3 someVec,
            OpenTK::Mathematics::Vector3 right,
            float dist);
        void Func214E750();
        void Func214E668(float a2);
        void Func214DC90();
        [[nodiscard]] bool Func214E5EC();
        [[nodiscard]] bool Func214E58C();
        [[nodiscard]] bool Func214D6E0(PlayerEntity* player);
        [[nodiscard]] bool Func214D690(PlayerEntity* player);
        [[nodiscard]] bool Func214D828(PlayerEntity* player);
        void Func214EC08();
        void Func214D954();
        [[nodiscard]] bool Func214E110();
        void Func214ED20();
        void Func214EDD0();
        void Func214EB84();
        void Func214E4D8(PlayerEntity* player);
        [[nodiscard]] bool Func214D65C(PlayerEntity* player);
        void Func214EB08();
        void Func214EEC4();
        [[nodiscard]] bool Func214DF58();
        [[nodiscard]] bool Func214DAF8();
        void Func214E994();
        void Func214DAB0(OpenTK::Mathematics::Vector3 vec);
        void Func214D9F8();
        void Func214E8D4();
        void Func214D864(PlayerEntity* player);
        void Func214E7B8();
        [[nodiscard]] bool Func214EE28();
        void Func214DE50();
        void Func214ED78();
        void Func214EC60();
        void Func214E9F4();
        void Func214E314(OpenTK::Mathematics::Vector3 vec);
        void Func214E82C();
        void Func214E92C();
        void Func214E788();
        void Func214DBDC();
        void Func214ECB8();
        [[nodiscard]] OpenTK::Mathematics::Vector3 RotateVectorRandom(
            OpenTK::Mathematics::Vector3 vec,
            OpenTK::Mathematics::Vector3 axis);
        [[nodiscard]] OpenTK::Mathematics::Vector3 Func204D518(
            OpenTK::Mathematics::Vector3 vec,
            OpenTK::Mathematics::Vector3 axis);
        [[nodiscard]] OpenTK::Mathematics::Vector3 Func204D57C(
            OpenTK::Mathematics::Vector3 vec,
            OpenTK::Mathematics::Vector3 axis);
        void Func214E1C0(EntityBase* entity);
        [[nodiscard]] bool CheckInVolume();
        void Func214D9B0();
        void Func214E444(
            OpenTK::Mathematics::Vector3 vec,
            std::uint8_t state,
            std::int32_t animId,
            AnimFlags animFlags);
        [[nodiscard]] bool Func214D500(
            OpenTK::Mathematics::Vector3 vec1,
            OpenTK::Mathematics::Vector3 vec2,
            OpenTK::Mathematics::Vector3 vec3);
    };
}
