#pragma once

#include "../../Formats/Culling.hpp"
#include "../../Formats/Enums.hpp"

#include <array>
#include <cstdint>
#include <string_view>
#include <type_traits>

#if __has_include("../EnemyInstanceEntity.hpp") && __has_include("../EnemySpawnEntity.hpp")
#include "../EnemyInstanceEntity.hpp"
#include "../EnemySpawnEntity.hpp"
#else
#define MPHREAD_NATIVE_ENEMY43_LOCAL_ENTITY_ADAPTER 1

namespace OpenTK::Mathematics
{
    struct Vector3
    {
        float X = 0.0F;
        float Y = 0.0F;
        float Z = 0.0F;

        static const Vector3 Zero;
    };

    struct Matrix4
    {
        std::array<float, 16> Values{};
    };
}

namespace MphRead::Entities
{
    class Scene;

    // Temporary narrow contracts for this migration unit. They intentionally
    // model only the members 43_SlenchNest.cs touches and disappear once the
    // real Native entity hierarchy headers exist.
    class EntityBase
    {
    public:
        virtual ~EntityBase() = default;

        OpenTK::Mathematics::Matrix4 Transform{};
    };

    class EnemySpawnEntity : public EntityBase
    {
    };

    struct EnemyInstanceEntityData
    {
        EnemyType Type{};
        EntityBase* Spawner = nullptr; // C#: EntityBase Spawner; "todo: nullable?"
    };

    enum class EnemyFlags : std::uint16_t
    {
        Visible = 0x0001,
        Invincible = 0x0008,
        NoMaxDistance = 0x0080
    };

    [[nodiscard]] constexpr EnemyFlags operator|(EnemyFlags left, EnemyFlags right) noexcept
    {
        using Underlying = std::underlying_type_t<EnemyFlags>;
        return static_cast<EnemyFlags>(
            static_cast<Underlying>(left) | static_cast<Underlying>(right));
    }

    constexpr EnemyFlags& operator|=(EnemyFlags& left, EnemyFlags right) noexcept
    {
        left = left | right;
        return left;
    }

    struct CollisionVolume
    {
        OpenTK::Mathematics::Vector3 Center{};
        float Radius = 0.0F;

        constexpr CollisionVolume() noexcept = default;
        constexpr CollisionVolume(OpenTK::Mathematics::Vector3 center, float rad) noexcept
            : Center(center), Radius(rad)
        {
        }
    };

    class EnemyInstanceEntity : public EntityBase
    {
    public:
        EnemyInstanceEntity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);
        virtual ~EnemyInstanceEntity() = default;

    protected:
        virtual void EnemyInitialize();
        void SetUpModel(std::string_view name);

        std::uint16_t _health = 20;
        std::uint16_t _healthMax = 20;
        EnemyFlags Flags = static_cast<EnemyFlags>(0);
        std::int32_t HealthbarMessageId = 0;
        float _boundingRadius = 0.0F;
        CollisionVolume _hurtVolumeInit{};
    };
}
#endif

namespace MphRead::Entities::Enemies
{
    class Enemy43Entity : public EnemyInstanceEntity
    {
    public:
        Enemy43Entity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        Enemy43Entity(const Enemy43Entity&) = delete;
        Enemy43Entity& operator=(const Enemy43Entity&) = delete;
        Enemy43Entity(Enemy43Entity&&) = delete;
        Enemy43Entity& operator=(Enemy43Entity&&) = delete;

    protected:
        void EnemyInitialize() override;

    private:
        EnemySpawnEntity* const _spawner;
    };
}
