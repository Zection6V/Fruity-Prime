#pragma once

#include "../Formats/Culling.hpp"
#include "../Formats/Formats.hpp"
#include "EntityBase.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace MphRead::Entities
{
    class BeamProjectileEntity;
    class BombEntity;
    class EnemyInstanceEntity;

    enum class EnemyFlags : std::uint16_t
    {
        Visible = 1,
        NoHomingNc = 2,
        NoHomingCo = 4,
        Invincible = 8,
        NoBombDamage = 0x10,
        CollidePlayer = 0x20,
        CollideBeam = 0x40,
        NoMaxDistance = 0x80,
        OnRadar = 0x100,
        Static = 0x200
    };

    [[nodiscard]] constexpr EnemyFlags operator|(EnemyFlags left, EnemyFlags right) noexcept
    {
        return static_cast<EnemyFlags>(
            static_cast<std::uint16_t>(left) | static_cast<std::uint16_t>(right));
    }

    [[nodiscard]] constexpr EnemyFlags operator&(EnemyFlags left, EnemyFlags right) noexcept
    {
        return static_cast<EnemyFlags>(
            static_cast<std::uint16_t>(left) & static_cast<std::uint16_t>(right));
    }

    [[nodiscard]] constexpr EnemyFlags operator^(EnemyFlags left, EnemyFlags right) noexcept
    {
        return static_cast<EnemyFlags>(
            static_cast<std::uint16_t>(left) ^ static_cast<std::uint16_t>(right));
    }

    [[nodiscard]] constexpr EnemyFlags operator~(EnemyFlags value) noexcept
    {
        return static_cast<EnemyFlags>(
            static_cast<std::uint16_t>(~static_cast<std::uint16_t>(value)));
    }

    constexpr EnemyFlags& operator|=(EnemyFlags& left, EnemyFlags right) noexcept
    {
        left = left | right;
        return left;
    }

    constexpr EnemyFlags& operator&=(EnemyFlags& left, EnemyFlags right) noexcept
    {
        left = left & right;
        return left;
    }

    constexpr EnemyFlags& operator^=(EnemyFlags& left, EnemyFlags right) noexcept
    {
        left = left ^ right;
        return left;
    }

    enum class Effectiveness : std::uint8_t
    {
        Zero,
        Half,
        Normal,
        Double
    };

    struct EnemyInstanceEntityData
    {
        const MphRead::EnemyType Type;
        EntityBase* const Spawner;

        EnemyInstanceEntityData() noexcept
            : Type{}, Spawner(nullptr)
        {
        }

        EnemyInstanceEntityData(MphRead::EnemyType type, EntityBase* spawner) noexcept
            : Type(type), Spawner(spawner)
        {
        }
    };

    template <typename T>
    struct EnemyBehavior;

    template <typename T>
    struct EnemySubroutine;

    class EnemyInstanceEntity : public EntityBase
    {
    public:
        class FlagsProperty final
        {
        public:
            FlagsProperty() noexcept = default;

            FlagsProperty& operator=(EnemyFlags value) noexcept
            {
                _value = value;
                return *this;
            }

            FlagsProperty& operator|=(EnemyFlags value) noexcept
            {
                _value |= value;
                return *this;
            }

            FlagsProperty& operator&=(EnemyFlags value) noexcept
            {
                _value &= value;
                return *this;
            }

            FlagsProperty& operator^=(EnemyFlags value) noexcept
            {
                _value ^= value;
                return *this;
            }

            [[nodiscard]] operator EnemyFlags() const noexcept
            {
                return _value;
            }

            [[nodiscard]] EnemyFlags operator()() const noexcept
            {
                return _value;
            }

        private:
            EnemyFlags _value{};
        };

        class Int32Property final
        {
        public:
            Int32Property() noexcept = default;
            explicit Int32Property(std::int32_t value) noexcept : _value(value) {}

            Int32Property& operator=(std::int32_t value) noexcept
            {
                _value = value;
                return *this;
            }

            [[nodiscard]] operator std::int32_t() const noexcept
            {
                return _value;
            }

            [[nodiscard]] std::int32_t operator()() const noexcept
            {
                return _value;
            }

        private:
            std::int32_t _value = 0;
        };

        EnemyInstanceEntity(EnemyInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef, Scene* scene);

        EnemyInstanceEntity(const EnemyInstanceEntity&) = delete;
        EnemyInstanceEntity& operator=(const EnemyInstanceEntity&) = delete;
        EnemyInstanceEntity(EnemyInstanceEntity&&) = delete;
        EnemyInstanceEntity& operator=(EnemyInstanceEntity&&) = delete;

        [[nodiscard]] std::uint16_t Health() const noexcept;
        [[nodiscard]] std::uint16_t HealthMax() const noexcept;
        [[nodiscard]] std::uint8_t StateA() const noexcept;
        [[nodiscard]] std::uint8_t StateB() const noexcept;
        [[nodiscard]] CollisionVolume HurtVolume() const noexcept;
        [[nodiscard]] MphRead::EnemyType EnemyType() const noexcept;
        [[nodiscard]] EntityBase* Owner() const noexcept;

        std::array<bool, 8> HitPlayers{};
        std::array<Effectiveness, 9> BeamEffectiveness{};
        FlagsProperty Flags{};
        Int32Property HealthbarMessageId{};

        static void DestroyBeams();

        void Initialize() override;
        void Destroy() override;
        void GetPosition(OpenTK::Mathematics::Vector3& position) override;
        void GetVectors(OpenTK::Mathematics::Vector3& position,
            OpenTK::Mathematics::Vector3& up,
            OpenTK::Mathematics::Vector3& facing) override;
        [[nodiscard]] bool GetTargetable() override;
        [[nodiscard]] bool Process() override;
        void GetDrawInfo() override;
        void GetDisplayVolumes() override;

        void ClearHitPlayers();
        void SetHealth(std::uint16_t health) noexcept;
        [[nodiscard]] bool CheckHitByBomb(BombEntity* bomb);
        void TakeDamage(std::uint32_t damage, EntityBase* source);
        [[nodiscard]] Effectiveness GetEffectiveness(BeamType beam) const;

        [[nodiscard]] static OpenTK::Mathematics::Vector3 RotateVector(
            OpenTK::Mathematics::Vector3 vec,
            OpenTK::Mathematics::Vector3 axis,
            float angle);
        [[nodiscard]] static bool SeekTargetVector(
            OpenTK::Mathematics::Vector3 target,
            OpenTK::Mathematics::Vector3& current,
            OpenTK::Mathematics::Vector3 axis,
            std::uint16_t& steps,
            float angle);
        [[nodiscard]] bool SeekTargetFacing(
            OpenTK::Mathematics::Vector3 target,
            OpenTK::Mathematics::Vector3 up,
            std::uint16_t& steps,
            float angle);

    protected:
        const EnemyInstanceEntityData _data;
        std::uint16_t _timeSinceDamage = 510;
        std::uint16_t _health = 20;
        std::uint16_t _healthMax = 20;
        EntityBase* _owner = nullptr;
        CollisionVolume _hurtVolume{};
        CollisionVolume _hurtVolumeInit{};
        std::uint8_t _state1 = 0;
        std::uint8_t _state2 = 0;
        std::uint8_t _subId = 0;
        OpenTK::Mathematics::Vector3 _prevPos = OpenTK::Mathematics::Vector3::Zero;
        OpenTK::Mathematics::Vector3 _speed = OpenTK::Mathematics::Vector3::Zero;
        float _boundingRadius = 0.0F;
        std::shared_ptr<ManagedArray<std::function<void()>>> _stateProcesses{};

        static std::shared_ptr<std::vector<std::shared_ptr<BeamProjectileEntity>>> _beams;

        [[nodiscard]] OpenTK::Mathematics::Vector3 FixParallelVectors(
            OpenTK::Mathematics::Vector3 facing,
            OpenTK::Mathematics::Vector3 up) const;
        [[nodiscard]] bool ContactDamagePlayer(std::uint32_t damage, bool knockback);
        [[nodiscard]] virtual bool BaseProcess();
        void DrawGeneric();
        virtual void EnemyInitialize();
        virtual void EnemyProcess();
        [[nodiscard]] virtual bool EnemyGetDrawInfo();
        virtual void Detach();
        [[nodiscard]] virtual bool EnemyTakeDamage(EntityBase* source);
        void CallStateProcess();

        template <typename T>
        [[nodiscard]] bool CallSubroutine(
            const std::vector<EnemySubroutine<T>>& subroutines, T* enemy);

        [[nodiscard]] bool HandleBlockingCollision(
            OpenTK::Mathematics::Vector3 position,
            CollisionVolume volume,
            bool updateSpeed);
        [[nodiscard]] bool HandleBlockingCollision(
            OpenTK::Mathematics::Vector3 position,
            CollisionVolume volume,
            bool updateSpeed,
            bool& withGround,
            bool& withWall);

    private:
        bool _onlyMoveHurtVolume = false;
        bool _noIneffectiveEffect = false;

        void DoMovement();
        void UpdateHurtVolume();
        void PlayEnemySfx(std::int32_t sfx, bool noUpdate);
    };

    template <typename T>
    struct EnemyBehavior
    {
        const std::uint8_t NextState;
        const std::function<bool(T*)> Function;

        EnemyBehavior()
            : NextState(0), Function{}
        {
        }

        EnemyBehavior(std::uint8_t nextState, std::function<bool(T*)> function)
            : NextState(nextState), Function(std::move(function))
        {
        }
    };

    template <typename T>
    struct EnemySubroutine
    {
        const std::shared_ptr<std::vector<EnemyBehavior<T>>> Behaviors;

        EnemySubroutine() noexcept
            : Behaviors{}
        {
        }

        explicit EnemySubroutine(std::vector<EnemyBehavior<T>> behaviors)
            : Behaviors(std::make_shared<std::vector<EnemyBehavior<T>>>(
                std::move(behaviors)))
        {
        }

        explicit EnemySubroutine(
            std::shared_ptr<std::vector<EnemyBehavior<T>>> behaviors) noexcept
            : Behaviors(std::move(behaviors))
        {
        }
    };

    template <typename T>
    bool EnemyInstanceEntity::CallSubroutine(
        const std::vector<EnemySubroutine<T>>& subroutines, T* enemy)
    {
        static_assert(std::is_base_of_v<EnemyInstanceEntity, T>);
        assert(enemy == this);
        if (static_cast<std::size_t>(_subId) >= subroutines.size())
        {
            throw std::out_of_range("Index was outside the bounds of the array.");
        }
        const EnemySubroutine<T>& subroutine = subroutines[static_cast<std::size_t>(_subId)];
        if (!subroutine.Behaviors)
        {
            throw System::NullReferenceException();
        }
        const std::vector<EnemyBehavior<T>>& behaviors = *subroutine.Behaviors;
        if (behaviors.empty())
        {
            return false;
        }
        std::size_t index = 0;
        while (true)
        {
            const std::function<bool(T*)>& function = behaviors[index].Function;
            if (!function)
            {
                throw System::NullReferenceException();
            }
            if (function(enemy))
            {
                break;
            }
            ++index;
            if (index >= behaviors.size())
            {
                return false;
            }
        }
        _state2 = behaviors[index].NextState;
        return true;
    }
}
