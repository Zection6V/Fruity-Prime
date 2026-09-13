#pragma once

#include "24_Gorea1A.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>

namespace MphRead
{
    class Material;
    class ModelInstance;
    class Node;
}

namespace MphRead::Entities
{
    class EnemySpawnEntity;
    class TriggerVolumeEntity;
}

namespace MphRead::Entities::Enemies
{
    class Enemy32Entity;
    class Enemy33Entity;

    enum class Gorea2Flags : std::uint32_t
    {
        None = 0,
        Bit0 = 1,
        LaserActive = 2,
        LaserBlocked = 4,
        LaserOnTarget = 8,
        Bit4 = 0x10,
        Bit5 = 0x20,
        Bit6 = 0x40,
        Bit7 = 0x80,
        Bit8 = 0x100,
        Bit9 = 0x200,
        Bit10 = 0x400,
        Bit11 = 0x800,
        Bit12 = 0x1000,
        Bit13 = 0x2000,
        Bit14 = 0x4000,
        Bit15 = 0x8000,
        Bit16 = 0x10000,
        Bit17 = 0x20000,
        Bit18 = 0x40000,
        Bit19 = 0x80000,
        Bit20 = 0x100000,
        Bit21 = 0x200000,
        Bit22 = 0x400000,
        Bit23 = 0x800000,
        Bit24 = 0x1000000,
        Bit25 = 0x2000000,
        Bit26 = 0x4000000,
        Bit27 = 0x8000000,
        Bit28 = 0x10000000,
        Bit29 = 0x20000000,
        Bit30 = 0x40000000,
        Bit31 = 0x80000000
    };

    [[nodiscard]] constexpr Gorea2Flags operator|(Gorea2Flags left, Gorea2Flags right) noexcept
    {
        return static_cast<Gorea2Flags>(
            static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr Gorea2Flags operator&(Gorea2Flags left, Gorea2Flags right) noexcept
    {
        return static_cast<Gorea2Flags>(
            static_cast<std::uint32_t>(left) & static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr Gorea2Flags operator^(Gorea2Flags left, Gorea2Flags right) noexcept
    {
        return static_cast<Gorea2Flags>(
            static_cast<std::uint32_t>(left) ^ static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr Gorea2Flags operator~(Gorea2Flags value) noexcept
    {
        return static_cast<Gorea2Flags>(~static_cast<std::uint32_t>(value));
    }

    constexpr Gorea2Flags& operator|=(Gorea2Flags& left, Gorea2Flags right) noexcept
    {
        left = left | right;
        return left;
    }

    constexpr Gorea2Flags& operator&=(Gorea2Flags& left, Gorea2Flags right) noexcept
    {
        left = left & right;
        return left;
    }

    constexpr Gorea2Flags& operator^=(Gorea2Flags& left, Gorea2Flags right) noexcept
    {
        left = left ^ right;
        return left;
    }

    class Enemy31Entity : public GoreaEnemyEntityBase
    {
    public:
        Enemy31Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy31Entity(const Enemy31Entity&) = delete;
        Enemy31Entity& operator=(const Enemy31Entity&) = delete;
        Enemy31Entity(Enemy31Entity&&) = delete;
        Enemy31Entity& operator=(Enemy31Entity&&) = delete;

        Gorea2Flags GoreaFlags = Gorea2Flags::None;

        [[nodiscard]] std::uint8_t Field244() const noexcept;

        void HandleMessage(MessageInfo info) override;

        [[nodiscard]] static OpenTK::Mathematics::Vector3 Func21418EC(
            OpenTK::Mathematics::Vector3 vec1,
            OpenTK::Mathematics::Vector3 vec2);

        [[nodiscard]] bool Func214080C();
        void UpdatePhase();

        [[nodiscard]] static bool BehaviorXX(Enemy31Entity* enemy);
        [[nodiscard]] static bool Behavior00(Enemy31Entity* enemy);
        [[nodiscard]] static bool Behavior01(Enemy31Entity* enemy);
        [[nodiscard]] static bool Behavior02(Enemy31Entity* enemy);
        [[nodiscard]] static bool Behavior03(Enemy31Entity* enemy);
        [[nodiscard]] static bool Behavior04(Enemy31Entity* enemy);
        [[nodiscard]] static bool Behavior05(Enemy31Entity* enemy);
        [[nodiscard]] static bool Behavior06(Enemy31Entity* enemy);
        [[nodiscard]] static bool Behavior07(Enemy31Entity* enemy);
        [[nodiscard]] static bool Behavior08(Enemy31Entity* enemy);
        [[nodiscard]] static bool Behavior09(Enemy31Entity* enemy);
        [[nodiscard]] static bool Behavior10(Enemy31Entity* enemy);
        [[nodiscard]] static bool Behavior11(Enemy31Entity* enemy);
        [[nodiscard]] static bool Behavior12(Enemy31Entity* enemy);
        [[nodiscard]] static bool Behavior13(Enemy31Entity* enemy);
        [[nodiscard]] static bool Behavior14(Enemy31Entity* enemy);
        [[nodiscard]] static bool Behavior15(Enemy31Entity* enemy);
        [[nodiscard]] static bool Behavior16(Enemy31Entity* enemy);
        [[nodiscard]] static bool Behavior17(Enemy31Entity* enemy);

    protected:
        void EnemyInitialize() override;
        void EnemyProcess() override;
        [[nodiscard]] bool EnemyTakeDamage(EntityBase* source) override;
        [[nodiscard]] bool EnemyGetDrawInfo() override;

    private:
        static const std::array<const char*, 6> _lightMaterialNames;
        static const std::array<const char*, 12> _allMaterialNames;

        EnemySpawnEntity* const _spawner;
        std::shared_ptr<Node> _headNode{};
        ModelInstance* _laserModel = nullptr;
        std::shared_ptr<Enemy32Entity> _sealSphere{};

        OpenTK::Mathematics::Vector3 _teleportDestination{};
        OpenTK::Mathematics::Vector3 _field1E8{};
        OpenTK::Mathematics::Vector3 _laserTargetPos{};
        OpenTK::Mathematics::Vector3 _laserColNormal{};
        OpenTK::Mathematics::Vector3 _field20C{};
        std::int32_t _field22C = 0;
        std::int32_t _field22E = 0;
        std::int32_t _field230 = 0;
        std::int32_t _field232 = 0;
        std::int32_t _field234 = 0;
        std::int32_t _field236 = 0;
        std::int32_t _field23C = 0;
        float _field23E = 0.0F;
        float _field240 = 0.0F;
        std::uint8_t _field242 = 0;
        std::uint8_t _field243 = 0;
        std::uint8_t _field244 = 0;

        const OpenTK::Mathematics::Vector3 _spawnerField28;
        const float _spawnerField34;
        const float _spawnerField38;

        TriggerVolumeEntity* _currentTrigger = nullptr;
        std::shared_ptr<Effects::EffectEntry> _chargeEffect{};
        std::shared_ptr<Effects::EffectEntry> _colEffect{};
        std::shared_ptr<Effects::EffectEntry> _flashEffect{};

        void CheckPlayerCollision();
        void CreateTeleportEffect(bool useNode);
        void UpdateChestMaterial();
        void DrawLaser();
        void Teleport(TriggerVolumeEntity* trigger,
            std::optional<OpenTK::Mathematics::Vector3> position);
        void UpdateLaserTargeting();
        void CheckLaserHit();
        void UpdateMaterialColors();
        void UpdateMaterialColor(Material* material, ColorRgb color);
        void Func213D194();
        void InterpolateAlpha(std::int32_t frame,
            std::int32_t frameCount, std::int32_t color);
        void Func213D3C8();
        void Func213D30C();
        void Func213D204();
        void Func213D5D0();
        void UpdateHoverPosition();

        void State00();
        void State01();
        void State02();
        void State03();
        void State04();
        void State05();
        void State06();
        void State07();
        void State08();
        void State09();
        void State10();
        void State11();
        void State12();
        void State13();
        void State14();
        void State15();
        void State16();
        void State17();
        void State18();

        void ShootMeteor();
        void SearchAndTeleport(std::int32_t mode, bool checkCollision);
        [[nodiscard]] TriggerVolumeEntity* FindTrigger(
            std::int32_t mode, bool checkCollision);
        void InitTrigger();
        void Func213D7C4();
        void Func213D80C();
        [[nodiscard]] OpenTK::Mathematics::Vector3 Func213F9B8(
            OpenTK::Mathematics::Vector3 vec);
        [[nodiscard]] OpenTK::Mathematics::Vector3 Func204D518(
            OpenTK::Mathematics::Vector3 vec,
            OpenTK::Mathematics::Vector3 axis);
        [[nodiscard]] OpenTK::Mathematics::Vector3 Func204D57C(
            OpenTK::Mathematics::Vector3 vec,
            OpenTK::Mathematics::Vector3 axis);
        [[nodiscard]] OpenTK::Mathematics::Vector3 Func213FA58();
        void Func213D974(OpenTK::Mathematics::Vector3 toPlayer);
        [[nodiscard]] OpenTK::Mathematics::Vector3 Func21405FC();
        [[nodiscard]] OpenTK::Mathematics::Vector3 Func204E2A8(
            OpenTK::Mathematics::Vector3 vec1,
            OpenTK::Mathematics::Vector3 vec2,
            float a4);
        [[nodiscard]] bool Func2140844();
        [[nodiscard]] bool Func2140390();
        [[nodiscard]] bool Func21403FC();
        void Func2140414();

        [[nodiscard]] bool BehaviorXX();
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
        [[nodiscard]] bool Behavior13();
        [[nodiscard]] bool Behavior14();
        [[nodiscard]] bool Behavior15();
        [[nodiscard]] bool Behavior16();
        [[nodiscard]] bool Behavior17();
    };
}
