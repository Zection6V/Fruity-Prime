#pragma once

#include "../Formats/Entity.hpp"
#include "../Formats/Formats.hpp"
#include "EnemyInstanceEntity.hpp"
#include "EntityBase.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace MphRead
{
    class BeamProjectileArray;
    class EquipInfo;
    class PlatformMetadata;
    enum class PlatAnimId : std::int32_t;
}

namespace MphRead::Effects
{
    class EffectEntry;
}

namespace MphRead::Entities
{
    class BeamProjectileEntity;

    enum class PlatAnimFlags : std::uint16_t
    {
        None = 0x0,
        Active = 0x1,
        DisableReflect = 0x2,
        Draw = 0x4,
        Bit03 = 0x8,
        Bit04 = 0x10,
        Bit05 = 0x20,
        SeekPlayerHeight = 0x40,
        WasDrawn = 0x80,
        HasAnim = 0x100,
        Bit09 = 0x200,
        Bit10 = 0x400,
        Bit11 = 0x800,
        Bit12 = 0x1000,
        Bit13 = 0x2000,
        Bit14 = 0x4000,
        Bit15 = 0x8000
    };

    [[nodiscard]] constexpr PlatAnimFlags operator|(PlatAnimFlags left, PlatAnimFlags right) noexcept
    {
        return static_cast<PlatAnimFlags>(
            static_cast<std::uint16_t>(left) | static_cast<std::uint16_t>(right));
    }

    [[nodiscard]] constexpr PlatAnimFlags operator&(PlatAnimFlags left, PlatAnimFlags right) noexcept
    {
        return static_cast<PlatAnimFlags>(
            static_cast<std::uint16_t>(left) & static_cast<std::uint16_t>(right));
    }

    [[nodiscard]] constexpr PlatAnimFlags operator^(PlatAnimFlags left, PlatAnimFlags right) noexcept
    {
        return static_cast<PlatAnimFlags>(
            static_cast<std::uint16_t>(left) ^ static_cast<std::uint16_t>(right));
    }

    [[nodiscard]] constexpr PlatAnimFlags operator~(PlatAnimFlags value) noexcept
    {
        return static_cast<PlatAnimFlags>(
            static_cast<std::uint16_t>(~static_cast<std::uint16_t>(value)));
    }

    constexpr PlatAnimFlags& operator|=(PlatAnimFlags& left, PlatAnimFlags right) noexcept
    {
        left = left | right;
        return left;
    }

    constexpr PlatAnimFlags& operator&=(PlatAnimFlags& left, PlatAnimFlags right) noexcept
    {
        left = left & right;
        return left;
    }

    constexpr PlatAnimFlags& operator^=(PlatAnimFlags& left, PlatAnimFlags right) noexcept
    {
        left = left ^ right;
        return left;
    }

    enum class PlatformFlags : std::uint32_t
    {
        None = 0x0,
        Hazard = 0x1,
        ContactDamage = 0x2,
        BeamSpawner = 0x4,
        BeamColEffect = 0x8,
        DamageReflect1 = 0x10,
        DamageReflect2 = 0x20,
        StandingColOnly = 0x40,
        StartSleep = 0x80,
        SleepAtEnd = 0x100,
        DripMoat = 0x200,
        SkipNodeRef = 0x400,
        DrawIfNodeRef = 0x800,
        DrawAlways = 0x1000,
        HideOnSleep = 0x2000,
        SyluxShip = 0x4000,
        Bit15 = 0x8000,
        BeamReflection = 0x10000,
        UseRoomState = 0x20000,
        BeamTarget = 0x40000,
        SamusShip = 0x80000,
        Breakable = 0x100000,
        PersistRoomState = 0x200000,
        NoBeamIfCull = 0x400000,
        NoRecoil = 0x800000,
        Bit24 = 0x1000000,
        Bit25 = 0x2000000,
        Bit26 = 0x4000000,
        Bit27 = 0x8000000,
        Bit28 = 0x10000000,
        Bit29 = 0x20000000,
        Bit30 = 0x40000000,
        Bit31 = 0x80000000
    };

    [[nodiscard]] constexpr PlatformFlags operator|(PlatformFlags left, PlatformFlags right) noexcept
    {
        return static_cast<PlatformFlags>(
            static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr PlatformFlags operator&(PlatformFlags left, PlatformFlags right) noexcept
    {
        return static_cast<PlatformFlags>(
            static_cast<std::uint32_t>(left) & static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr PlatformFlags operator^(PlatformFlags left, PlatformFlags right) noexcept
    {
        return static_cast<PlatformFlags>(
            static_cast<std::uint32_t>(left) ^ static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr PlatformFlags operator~(PlatformFlags value) noexcept
    {
        return static_cast<PlatformFlags>(~static_cast<std::uint32_t>(value));
    }

    constexpr PlatformFlags& operator|=(PlatformFlags& left, PlatformFlags right) noexcept
    {
        left = left | right;
        return left;
    }

    constexpr PlatformFlags& operator&=(PlatformFlags& left, PlatformFlags right) noexcept
    {
        left = left & right;
        return left;
    }

    constexpr PlatformFlags& operator^=(PlatformFlags& left, PlatformFlags right) noexcept
    {
        left = left ^ right;
        return left;
    }

    enum class PlatStateFlags : std::uint32_t
    {
        None = 0x0,
        Awake = 0x1,
        Activated = 0x2,
        Reverse = 0x4,
        WasAwake = 0x8
    };

    [[nodiscard]] constexpr PlatStateFlags operator|(PlatStateFlags left, PlatStateFlags right) noexcept
    {
        return static_cast<PlatStateFlags>(
            static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr PlatStateFlags operator&(PlatStateFlags left, PlatStateFlags right) noexcept
    {
        return static_cast<PlatStateFlags>(
            static_cast<std::uint32_t>(left) & static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr PlatStateFlags operator^(PlatStateFlags left, PlatStateFlags right) noexcept
    {
        return static_cast<PlatStateFlags>(
            static_cast<std::uint32_t>(left) ^ static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr PlatStateFlags operator~(PlatStateFlags value) noexcept
    {
        return static_cast<PlatStateFlags>(~static_cast<std::uint32_t>(value));
    }

    constexpr PlatStateFlags& operator|=(PlatStateFlags& left, PlatStateFlags right) noexcept
    {
        left = left | right;
        return left;
    }

    constexpr PlatStateFlags& operator&=(PlatStateFlags& left, PlatStateFlags right) noexcept
    {
        left = left & right;
        return left;
    }

    constexpr PlatStateFlags& operator^=(PlatStateFlags& left, PlatStateFlags right) noexcept
    {
        left = left ^ right;
        return left;
    }

    enum class PlatformState : std::uint8_t
    {
        Inactive = 0,
        Moving = 1,
        Waiting = 2
    };

    class PlatformEntity : public EntityBase
    {
    public:
        PlatformEntity(PlatformEntityData data, std::string nodeName, Scene* scene);

        PlatformEntity(const PlatformEntity&) = delete;
        PlatformEntity& operator=(const PlatformEntity&) = delete;
        PlatformEntity(PlatformEntity&&) = delete;
        PlatformEntity& operator=(PlatformEntity&&) = delete;

        [[nodiscard]] PlatformEntityData Data() const;
        [[nodiscard]] PlatformFlags Flags() const noexcept;
        [[nodiscard]] PlatStateFlags StateFlags() const noexcept;
        [[nodiscard]] OpenTK::Mathematics::Vector3 Velocity() const noexcept;

        static void DestroyBeams();

        void Initialize() override;
        void Destroy() override;
        void GetPosition(OpenTK::Mathematics::Vector3& position) override;
        void GetVectors(OpenTK::Mathematics::Vector3& position,
            OpenTK::Mathematics::Vector3& up,
            OpenTK::Mathematics::Vector3& facing) override;
        [[nodiscard]] std::int32_t GetScanId(bool alternate = false) override;
        void OnScanned() override;
        [[nodiscard]] bool Process() override;
        void GetDrawInfo() override;
        void SetActive(bool active) override;
        void Recoil();
        void CheckContactDamage(DamageResult& result) override;
        void CheckBeamReflection(bool& result) override;
        void HandleMessage(MessageInfo info) override;

    protected:
        [[nodiscard]] std::optional<OpenTK::Mathematics::Vector4> OverrideColor() const override;
        [[nodiscard]] OpenTK::Mathematics::Matrix4 GetModelTransform(
            ModelInstance& inst, std::int32_t index) override;

    private:
        enum class PlatSfxFlags : std::int32_t
        {
            None = 0,
            Environment = 0x400,
            NoUpdate = 0x800,
            CheckRecent = 0x1000,
            Loop = 0x2000
        };

        struct SfxData
        {
            std::int32_t Id = 0;
            PlatSfxFlags Flags = PlatSfxFlags::None;

            SfxData() noexcept = default;
            explicit SfxData(std::int32_t data) noexcept;
            SfxData(std::int32_t id, PlatSfxFlags flags) noexcept;
        };

        struct MoveSfxInfo
        {
            SfxData Start1{};
            SfxData Start2{};
            SfxData Stop{};
            SfxData Destoryed{};

            MoveSfxInfo() noexcept = default;
            MoveSfxInfo(SfxData start1, SfxData start2, SfxData stop, SfxData destoryed) noexcept;
        };

        struct BeamSfxInfo
        {
            SfxData Data{};
            float Offset = 0.0F;
            std::int32_t RangeIndex = 0;

            BeamSfxInfo() noexcept = default;
            BeamSfxInfo(std::int32_t id, PlatSfxFlags flags, float offset, std::int32_t rangeIndex) noexcept;
        };

        void SetPlatAnimation(PlatAnimId id, AnimFlags flags);
        void SetPlatAnimation(std::int32_t index, AnimFlags flags);
        [[nodiscard]] std::int32_t GetAnimation(PlatAnimId id) const;
        void SleepWake(bool wake, bool instant);
        void Activate();
        void Deactivate();
        void PlaySfx(SfxData data);
        void UpdatePosition();
        void UpdateMovement();
        [[nodiscard]] static OpenTK::Mathematics::Vector4 ChooseVectors(
            OpenTK::Mathematics::Vector3 vec1,
            OpenTK::Mathematics::Vector3 vec2,
            OpenTK::Mathematics::Vector3 vec3);
        [[nodiscard]] static OpenTK::Mathematics::Vector4 ComputeRotationLinear(
            OpenTK::Mathematics::Vector4 fromRot,
            OpenTK::Mathematics::Vector4 toRot,
            float pct);
        [[nodiscard]] static OpenTK::Mathematics::Vector4 ComputeRotationSin(
            OpenTK::Mathematics::Vector4 fromRot,
            OpenTK::Mathematics::Vector4 toRot,
            float pct);
        void UpdateState();
        [[nodiscard]] OpenTK::Mathematics::Matrix4 GetTransform() const;
        [[nodiscard]] OpenTK::Mathematics::Matrix4 GetTransformMatrix() const;

        const PlatformEntityData _data;
        const PlatformMetadata* _meta = nullptr;

        PlatformFlags _flags = PlatformFlags::None;
        std::array<std::int32_t, 4> _effectNodeIds{{-1, -1, -1, -1}};
        std::array<std::shared_ptr<Effects::EffectEntry>, 4> _effects{};
        static constexpr std::int32_t _nozzleEffectId = 182;

        bool _beamActive = false;
        std::int32_t _beamInterval = 0;
        std::int32_t _beamIntervalTimer = 0;
        std::int32_t _beamIntervalIndex = 0;
        std::shared_ptr<EquipInfo> _equipInfo{};
        OpenTK::Mathematics::Vector3 _beamSpawnPos{};
        OpenTK::Mathematics::Vector3 _beamSpawnDir{};
        std::int32_t _ammo = 0;

        std::int32_t _health = 0;
        std::int32_t _halfHealth = 0;
        std::array<Effectiveness, 9> _beamEffectiveness{};

        std::uint16_t _timeSincePlayerCol = 0;
        bool _playerCol = false;
        std::shared_ptr<PlatformEntity> _parent{};
        std::shared_ptr<Formats::Collision::EntityCollision> _parentEntCol{};
        std::shared_ptr<EntityBase> _scanMessageTarget{};
        std::shared_ptr<EntityBase> _hitMessageTarget{};
        std::shared_ptr<EntityBase> _playerColMessageTarget{};
        std::shared_ptr<EntityBase> _deathMessageTarget{};
        std::array<std::shared_ptr<EntityBase>, 4> _lifetimeMessageTargets{};
        std::array<Message, 4> _lifetimeMessages{};
        std::array<std::int32_t, 4> _lifetimeMessageParam1s{};
        std::array<std::int32_t, 4> _lifetimeMessageParam2s{};
        std::array<std::int32_t, 4> _lifetimeMessageIndices{};

        PlatAnimFlags _animFlags = PlatAnimFlags::None;
        PlatStateFlags _stateFlags = PlatStateFlags::None;
        PlatformState _state = PlatformState::Inactive;
        std::int32_t _fromIndex = 0;
        std::int32_t _toIndex = 1;
        std::int32_t _delay = 0;
        std::int32_t _moveTimer = 0;
        std::int32_t _recoilTimer = 0;
        float _forwardSpeed = 0.0F;
        float _backwardSpeed = 0.0F;
        std::int32_t _currentAnimState = 0;
        std::int32_t _currentAnimId = 0;
        float _moveSfxAmount = 0.0F;

        OpenTK::Mathematics::Vector3 _posOffset{};
        OpenTK::Mathematics::Vector3 _curPosition{};
        OpenTK::Mathematics::Vector4 _curRotation{};
        OpenTK::Mathematics::Vector4 _fromRotation{};
        OpenTK::Mathematics::Vector4 _toRotation{};
        std::vector<OpenTK::Mathematics::Vector3> _posList{};
        std::vector<OpenTK::Mathematics::Vector4> _rotList{};
        OpenTK::Mathematics::Vector3 _velocity{};
        float _movePercent = 0.0F;
        float _moveIncrement = 0.0F;
        OpenTK::Mathematics::Vector3 _visiblePosition{};
        OpenTK::Mathematics::Vector3 _prevVisiblePosition{};
        std::int32_t _sfxRangeIndex = 0;
        MoveSfxInfo _moveSfx{};

        const std::optional<OpenTK::Mathematics::Vector4> _overrideColor
            = ColorRgb(0x2F, 0x4F, 0x4F).AsVector4();

        static std::shared_ptr<MphRead::BeamProjectileArray> _beams;
        static std::array<BeamSfxInfo, 4> _beamSfx;
    };

    class FhPlatformEntity : public EntityBase
    {
    public:
        FhPlatformEntity(FhPlatformEntityData data, Scene* scene);

        FhPlatformEntity(const FhPlatformEntity&) = delete;
        FhPlatformEntity& operator=(const FhPlatformEntity&) = delete;
        FhPlatformEntity(FhPlatformEntity&&) = delete;
        FhPlatformEntity& operator=(FhPlatformEntity&&) = delete;

        [[nodiscard]] bool Process() override;

    private:
        enum class MoveState : std::uint8_t
        {
            Sleep,
            MoveForward,
            Wait,
            MoveBackward
        };

        void UpdateMovement();

        const FhPlatformEntityData _data;
        float _speed = 0.0F;
        std::vector<OpenTK::Mathematics::Vector3> _posList{};
        MoveState _state = MoveState::Sleep;
        std::int32_t _fromIndex = 0;
        std::int32_t _toIndex = 1;
        std::int32_t _delay = 0;
        std::int32_t _moveTimer = 0;
        OpenTK::Mathematics::Vector3 _velocity{};
    };
}
