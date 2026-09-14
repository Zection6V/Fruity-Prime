#pragma once

#include "../Formats/Entity.hpp"
#include "../Formats/Formats.hpp"
#include "EntityBase.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace MphRead::Effects
{
    class EffectEntry;
}

namespace MphRead
{
    class ObjectMetadata;
}

namespace MphRead::Entities
{
    enum class ObjectFlags : std::uint8_t
    {
        None = 0x0,
        StateBit0 = 0x1,
        StateBit1 = 0x2,
        State = 0x3,
        NoAnimation = 0x4,
        EntityLinked = 0x8,
        IsVisible = 0x10
    };

    [[nodiscard]] constexpr ObjectFlags operator|(ObjectFlags left, ObjectFlags right) noexcept
    {
        return static_cast<ObjectFlags>(
            static_cast<std::uint8_t>(left) | static_cast<std::uint8_t>(right));
    }

    [[nodiscard]] constexpr ObjectFlags operator&(ObjectFlags left, ObjectFlags right) noexcept
    {
        return static_cast<ObjectFlags>(
            static_cast<std::uint8_t>(left) & static_cast<std::uint8_t>(right));
    }

    [[nodiscard]] constexpr ObjectFlags operator^(ObjectFlags left, ObjectFlags right) noexcept
    {
        return static_cast<ObjectFlags>(
            static_cast<std::uint8_t>(left) ^ static_cast<std::uint8_t>(right));
    }

    [[nodiscard]] constexpr ObjectFlags operator~(ObjectFlags value) noexcept
    {
        return static_cast<ObjectFlags>(
            static_cast<std::uint8_t>(~static_cast<std::uint8_t>(value)));
    }

    constexpr ObjectFlags& operator|=(ObjectFlags& left, ObjectFlags right) noexcept
    {
        left = left | right;
        return left;
    }

    constexpr ObjectFlags& operator&=(ObjectFlags& left, ObjectFlags right) noexcept
    {
        left = left & right;
        return left;
    }

    constexpr ObjectFlags& operator^=(ObjectFlags& left, ObjectFlags right) noexcept
    {
        left = left ^ right;
        return left;
    }

    enum class ObjEffFlags : std::uint32_t
    {
        None = 0x0,
        UseEffectVolume = 0x1,
        UseEffectOffset = 0x2,
        RepeatScanMessage = 0x4,
        WeaponZoom = 0x8,
        AttachEffect = 0x10,
        DestroyEffect = 0x20,
        AlwaysUpdateEffect = 0x40,
        Unknown = 0x8000
    };

    [[nodiscard]] constexpr ObjEffFlags operator|(ObjEffFlags left, ObjEffFlags right) noexcept
    {
        return static_cast<ObjEffFlags>(
            static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr ObjEffFlags operator&(ObjEffFlags left, ObjEffFlags right) noexcept
    {
        return static_cast<ObjEffFlags>(
            static_cast<std::uint32_t>(left) & static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr ObjEffFlags operator^(ObjEffFlags left, ObjEffFlags right) noexcept
    {
        return static_cast<ObjEffFlags>(
            static_cast<std::uint32_t>(left) ^ static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr ObjEffFlags operator~(ObjEffFlags value) noexcept
    {
        return static_cast<ObjEffFlags>(~static_cast<std::uint32_t>(value));
    }

    constexpr ObjEffFlags& operator|=(ObjEffFlags& left, ObjEffFlags right) noexcept
    {
        left = left | right;
        return left;
    }

    constexpr ObjEffFlags& operator&=(ObjEffFlags& left, ObjEffFlags right) noexcept
    {
        left = left & right;
        return left;
    }

    constexpr ObjEffFlags& operator^=(ObjEffFlags& left, ObjEffFlags right) noexcept
    {
        left = left ^ right;
        return left;
    }

    class ObjectEntity : public EntityBase
    {
    public:
        ObjectEntity(ObjectEntityData data, std::string nodeName, Scene* scene);

        [[nodiscard]] ObjectEntityData Data() const;

        void Initialize() override;
        void Destroy() override;
        void GetPosition(::OpenTK::Mathematics::Vector3& position) override;
        void GetVectors(::OpenTK::Mathematics::Vector3& position,
            ::OpenTK::Mathematics::Vector3& up,
            ::OpenTK::Mathematics::Vector3& facing) override;
        void OnScanned() override;
        void HandleMessage(MessageInfo info) override;
        [[nodiscard]] bool Process() override;
        void GetDrawInfo() override;
        void GetDisplayVolumes() override;

    protected:
        [[nodiscard]] std::optional<::OpenTK::Mathematics::Vector4> OverrideColor() const override;

    private:
        class EffectSfxInfo
        {
        public:
            const std::int32_t SfxId;
            const std::uint8_t Data;
            const bool Environment;

            EffectSfxInfo(
                std::int32_t sfxId, std::uint8_t data, bool environment = true)
                : SfxId(sfxId), Data(data), Environment(environment)
            {
            }
        };

        void UpdateVisiblePosition();
        void UpdateState(std::int32_t state);
        void RemoveEffect();

        const ObjectEntityData _data;
        CollisionVolume _effectVolume{};
        ::OpenTK::Mathematics::Matrix4 _prevTransform{};
        ::OpenTK::Mathematics::Vector3 _visiblePosition{};

        ObjectFlags _flags = ObjectFlags::None;
        std::int32_t _effectInterval = 0;
        std::int32_t _effectIntervalTimer = 0;
        std::int32_t _effectIntervalIndex = 0;
        bool _effectProcessing = false;
        std::shared_ptr<Effects::EffectEntry> _effectEntry{};

    public:
        bool _effectActive = false;

    private:
        std::int32_t _state = 0;
        const ObjectMetadata* _meta = nullptr;

        std::shared_ptr<EntityBase> _parent{};
        std::shared_ptr<Formats::Collision::EntityCollision> _parentEntCol{};
        ::OpenTK::Mathematics::Matrix4 _invTransform{
            ::OpenTK::Mathematics::Vector4(1.0F, 0.0F, 0.0F, 0.0F),
            ::OpenTK::Mathematics::Vector4(0.0F, 1.0F, 0.0F, 0.0F),
            ::OpenTK::Mathematics::Vector4(0.0F, 0.0F, 1.0F, 0.0F),
            ::OpenTK::Mathematics::Vector4(0.0F, 0.0F, 0.0F, 1.0F)};
        std::shared_ptr<EntityBase> _scanMsgTarget{};

        const std::optional<::OpenTK::Mathematics::Vector4> _overrideColor
            = ColorRgb(0x22, 0x8B, 0x22).AsVector4();

        static const std::array<SfxId, 6> _secretSwitchSfx;
        static const std::unordered_map<std::int32_t, EffectSfxInfo> _sfxInfo;
    };
}
