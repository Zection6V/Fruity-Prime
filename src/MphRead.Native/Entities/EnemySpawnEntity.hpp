#pragma once

#include "../Formats/EntityEnemy.hpp"
#include "EntityBase.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace MphRead::Formats::Collision
{
    class EntityCollision;
}

namespace MphRead::Entities
{
    class EnemyInstanceEntity;
    class PlayerEntity;

    enum class SpawnerFlags : std::uint8_t
    {
        Suspended = 1,
        Active = 2,
        HasModel = 4,
        PlayAnimation = 8,
        CounterBit0 = 0x10,
        CounterBit1 = 0x20,
        CounterBit2 = 0x40,
        CounterBit3 = 0x80
    };

    [[nodiscard]] constexpr SpawnerFlags operator|(SpawnerFlags left, SpawnerFlags right) noexcept
    {
        return static_cast<SpawnerFlags>(
            static_cast<std::uint8_t>(left) | static_cast<std::uint8_t>(right));
    }

    [[nodiscard]] constexpr SpawnerFlags operator&(SpawnerFlags left, SpawnerFlags right) noexcept
    {
        return static_cast<SpawnerFlags>(
            static_cast<std::uint8_t>(left) & static_cast<std::uint8_t>(right));
    }

    [[nodiscard]] constexpr SpawnerFlags operator^(SpawnerFlags left, SpawnerFlags right) noexcept
    {
        return static_cast<SpawnerFlags>(
            static_cast<std::uint8_t>(left) ^ static_cast<std::uint8_t>(right));
    }

    [[nodiscard]] constexpr SpawnerFlags operator~(SpawnerFlags value) noexcept
    {
        return static_cast<SpawnerFlags>(
            static_cast<std::uint8_t>(~static_cast<std::uint8_t>(value)));
    }

    constexpr SpawnerFlags& operator|=(SpawnerFlags& left, SpawnerFlags right) noexcept
    {
        left = left | right;
        return left;
    }

    constexpr SpawnerFlags& operator&=(SpawnerFlags& left, SpawnerFlags right) noexcept
    {
        left = left & right;
        return left;
    }

    constexpr SpawnerFlags& operator^=(SpawnerFlags& left, SpawnerFlags right) noexcept
    {
        left = left ^ right;
        return left;
    }

    class EnemySpawnEntity : public EntityBase
    {
    private:
        const EnemySpawnEntityData _data;
        std::int32_t _spawnedCount = 0;
        std::int32_t _activeCount = 0;
        std::int32_t _cooldownTimer = 0;
        std::shared_ptr<EntityBase> _entity1{};
        std::shared_ptr<EntityBase> _entity2{};
        std::shared_ptr<EntityBase> _entity3{};
        std::shared_ptr<EntityBase> _parent{};
        std::shared_ptr<Formats::Collision::EntityCollision> _parentEntCol{};
        OpenTK::Mathematics::Matrix4 _invTransform{
            OpenTK::Mathematics::Vector4(1.0F, 0.0F, 0.0F, 0.0F),
            OpenTK::Mathematics::Vector4(0.0F, 1.0F, 0.0F, 0.0F),
            OpenTK::Mathematics::Vector4(0.0F, 0.0F, 1.0F, 0.0F),
            OpenTK::Mathematics::Vector4(0.0F, 0.0F, 0.0F, 1.0F)};
        Formats::Culling::NodeRef _rangeNodeRef = Formats::Culling::NodeRef::None;

        [[nodiscard]] PlayerEntity* SpawnHunter();
        void DeactivateAndSendMessages();

    protected:
        [[nodiscard]] std::optional<OpenTK::Mathematics::Vector4> OverrideColor() const override;

    public:
        const EnemySpawnEntityData& Data;
        const std::int32_t& SpawnedCount;
        const std::int32_t& ActiveCount;
        const std::shared_ptr<Formats::Collision::EntityCollision>& ParentEntCol;
        SpawnerFlags Flags{};

        EnemySpawnEntity(EnemySpawnEntityData data, std::string nodeName, Scene* scene);
        EnemySpawnEntity(const EnemySpawnEntity&) = delete;
        EnemySpawnEntity& operator=(const EnemySpawnEntity&) = delete;
        EnemySpawnEntity(EnemySpawnEntity&&) = delete;
        EnemySpawnEntity& operator=(EnemySpawnEntity&&) = delete;

        void Initialize() override;
        void SetActive(bool active) override;
        [[nodiscard]] bool Process() override;
        void HandleMessage(MessageInfo info) override;

        [[nodiscard]] static std::shared_ptr<EnemyInstanceEntity> SpawnEnemy(
            EntityBase* spawner,
            MphRead::EnemyType type,
            Formats::Culling::NodeRef nodeRef,
            Scene* scene);
    };

    class FhEnemySpawnEntity : public EntityBase
    {
    private:
        const FhEnemySpawnEntityData _data;

    protected:
        [[nodiscard]] std::optional<OpenTK::Mathematics::Vector4> OverrideColor() const override;

    public:
        FhEnemySpawnEntity(FhEnemySpawnEntityData data, Scene* scene);
        FhEnemySpawnEntity(const FhEnemySpawnEntity&) = delete;
        FhEnemySpawnEntity& operator=(const FhEnemySpawnEntity&) = delete;
        FhEnemySpawnEntity(FhEnemySpawnEntity&&) = delete;
        FhEnemySpawnEntity& operator=(FhEnemySpawnEntity&&) = delete;
    };
}
