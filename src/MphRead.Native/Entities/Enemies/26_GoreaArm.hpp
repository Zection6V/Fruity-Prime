#pragma once

#include "24_Gorea1A.hpp"

#include <cstdint>
#include <memory>

namespace MphRead
{
    class EquipInfo;
    class ModelInstance;
    class Node;
    class WeaponInfo;

    namespace Effects
    {
        class EffectEntry;
    }
}

namespace MphRead::Entities::Enemies
{
    enum class GoreaArmFlags : std::uint8_t
    {
        None = 0x0,
        Bit0 = 0x1,
        Bit1 = 0x2,
        Bit2 = 0x4,
        Unused3 = 0x8,
        Unused4 = 0x10,
        Unused5 = 0x20,
        Unused6 = 0x40,
        Unused7 = 0x80
    };

    [[nodiscard]] constexpr GoreaArmFlags operator|(
        GoreaArmFlags left, GoreaArmFlags right) noexcept
    {
        return static_cast<GoreaArmFlags>(
            static_cast<std::uint8_t>(left) | static_cast<std::uint8_t>(right));
    }

    [[nodiscard]] constexpr GoreaArmFlags operator&(
        GoreaArmFlags left, GoreaArmFlags right) noexcept
    {
        return static_cast<GoreaArmFlags>(
            static_cast<std::uint8_t>(left) & static_cast<std::uint8_t>(right));
    }

    [[nodiscard]] constexpr GoreaArmFlags operator^(
        GoreaArmFlags left, GoreaArmFlags right) noexcept
    {
        return static_cast<GoreaArmFlags>(
            static_cast<std::uint8_t>(left) ^ static_cast<std::uint8_t>(right));
    }

    [[nodiscard]] constexpr GoreaArmFlags operator~(GoreaArmFlags value) noexcept
    {
        return static_cast<GoreaArmFlags>(
            static_cast<std::uint8_t>(~static_cast<std::uint8_t>(value)));
    }

    constexpr GoreaArmFlags& operator|=(
        GoreaArmFlags& left, GoreaArmFlags right) noexcept
    {
        left = left | right;
        return left;
    }

    constexpr GoreaArmFlags& operator&=(
        GoreaArmFlags& left, GoreaArmFlags right) noexcept
    {
        left = left & right;
        return left;
    }

    constexpr GoreaArmFlags& operator^=(
        GoreaArmFlags& left, GoreaArmFlags right) noexcept
    {
        left = left ^ right;
        return left;
    }

    class Enemy26Entity : public GoreaEnemyEntityBase
    {
    public:
        Enemy26Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy26Entity(const Enemy26Entity&) = delete;
        Enemy26Entity& operator=(const Enemy26Entity&) = delete;
        Enemy26Entity(Enemy26Entity&&) = delete;
        Enemy26Entity& operator=(Enemy26Entity&&) = delete;

        std::int32_t Index = 0;
        GoreaArmFlags ArmFlags = GoreaArmFlags::None;
        std::int32_t Ammo = 65535;
        std::int32_t Damage = 0;
        std::int32_t Cooldown = 0;
        std::int32_t RegenTimer = 0;

        [[nodiscard]] std::int32_t ScanId() const noexcept;
        [[nodiscard]] std::shared_ptr<MphRead::EquipInfo> EquipInfo() const noexcept;
        [[nodiscard]] std::int32_t ColorTimer() const noexcept;

        void Activate();
        void UpdateWeapon(std::shared_ptr<MphRead::WeaponInfo> weapon);
        void GetElbowNodeVectors(
            OpenTK::Mathematics::Vector3& position,
            OpenTK::Mathematics::Vector3& up,
            OpenTK::Mathematics::Vector3& facing);
        void SpawnShotEffect(std::int32_t effectId);
        void StopShotEffect(bool deatch);
        void DrawRegen(ModelInstance& regenModel);

    protected:
        void EnemyInitialize() override;
        void EnemyProcess() override;
        [[nodiscard]] bool EnemyTakeDamage(EntityBase* source) override;

    private:
        Enemy24Entity* _gorea1A = nullptr;
        std::shared_ptr<Node> _shoulderNode{};
        std::shared_ptr<Node> _elbowNode{};
        std::shared_ptr<Node> _upperArmNode{};
        std::shared_ptr<MphRead::EquipInfo> _equipInfo{};
        std::int32_t _colorTimer = 0;
        std::shared_ptr<Effects::EffectEntry> _shotEffect{};
        std::shared_ptr<Effects::EffectEntry> _damageEffect{};

        void GetNodeVectors(
            Node* node,
            OpenTK::Mathematics::Vector3& position,
            OpenTK::Mathematics::Vector3& up,
            OpenTK::Mathematics::Vector3& facing);
        void DrawRegen(
            ModelInstance& regenModel,
            OpenTK::Mathematics::Vector3 position,
            OpenTK::Mathematics::Vector3 up,
            OpenTK::Mathematics::Vector3 facing,
            float factor);
    };
}
