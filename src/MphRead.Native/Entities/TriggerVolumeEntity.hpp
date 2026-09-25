#pragma once

#include "../Formats/Entity.hpp"
#include "../Formats/Formats.hpp"
#include "EntityBase.hpp"

#include <cstdint>
#include <memory>
#include <optional>

namespace MphRead::Entities
{
    class TriggerVolumeEntity : public EntityBase
    {
    public:
        TriggerVolumeEntity(TriggerVolumeEntityData data, Scene* scene);

        TriggerVolumeEntity(const TriggerVolumeEntity&) = delete;
        TriggerVolumeEntity& operator=(const TriggerVolumeEntity&) = delete;
        TriggerVolumeEntity(TriggerVolumeEntity&&) = delete;
        TriggerVolumeEntity& operator=(TriggerVolumeEntity&&) = delete;

        // C# declares `new bool Active { get; set; }`, so this intentionally hides EntityBase::Active.
        bool Active = false;

        [[nodiscard]] CollisionVolume Volume() const;
        [[nodiscard]] TriggerVolumeEntityData Data() const;

        void Initialize() override;
        [[nodiscard]] bool GetTargetable() override;
        void GetDisplayVolumes() override;
        [[nodiscard]] EntityBase* GetParent() override;
        [[nodiscard]] EntityBase* GetChild() override;
        [[nodiscard]] bool Process() override;
        void HandleMessage(MessageInfo info) override;

    protected:
        [[nodiscard]] std::optional<::OpenTK::Mathematics::Vector4> OverrideColor() const override;

    private:
        [[nodiscard]] bool Trigger();
        [[nodiscard]] std::int32_t GetParam2(Message message, std::int32_t param2) const noexcept;
        void Deactivate();

        const TriggerVolumeEntityData _data;
        std::int32_t _parentMsgParam2 = 0;
        std::int32_t _childMsgParam2 = 0;
        std::shared_ptr<EntityBase> _parent{};
        std::shared_ptr<EntityBase> _child{};

        CollisionVolume _volume{};
        ::OpenTK::Mathematics::Vector3 _parentEventColor{};
        ::OpenTK::Mathematics::Vector3 _childEventColor{};

        std::int32_t _count = 0;
        std::int32_t _delayTimer = 0;

        const std::optional<::OpenTK::Mathematics::Vector4> _overrideColor
            = ColorRgb(0xFF, 0x8C, 0x00).AsVector4();
    };

    // Area volumes don't use IncludeBots; jump pads only use PlayerBiped and PlayerAlt.
    enum class TriggerFlags : std::uint32_t
    {
        None = 0x0,
        PowerBeam = 0x1,
        VoltDriver = 0x2,
        Missile = 0x4,
        Battlehammer = 0x8,
        Imperialist = 0x10,
        Judicator = 0x20,
        Magmaul = 0x40,
        ShockCoil = 0x80,
        BeamCharged = 0x100,
        PlayerBiped = 0x200,
        PlayerAlt = 0x400,
        Bit11 = 0x800,
        IncludeBots = 0x1000
    };

    [[nodiscard]] constexpr TriggerFlags operator|(TriggerFlags left, TriggerFlags right) noexcept
    {
        return static_cast<TriggerFlags>(
            static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr TriggerFlags operator&(TriggerFlags left, TriggerFlags right) noexcept
    {
        return static_cast<TriggerFlags>(
            static_cast<std::uint32_t>(left) & static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr TriggerFlags operator^(TriggerFlags left, TriggerFlags right) noexcept
    {
        return static_cast<TriggerFlags>(
            static_cast<std::uint32_t>(left) ^ static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr TriggerFlags operator~(TriggerFlags value) noexcept
    {
        return static_cast<TriggerFlags>(~static_cast<std::uint32_t>(value));
    }

    constexpr TriggerFlags& operator|=(TriggerFlags& left, TriggerFlags right) noexcept
    {
        left = left | right;
        return left;
    }

    constexpr TriggerFlags& operator&=(TriggerFlags& left, TriggerFlags right) noexcept
    {
        left = left & right;
        return left;
    }

    constexpr TriggerFlags& operator^=(TriggerFlags& left, TriggerFlags right) noexcept
    {
        left = left ^ right;
        return left;
    }

    class FhTriggerVolumeEntity : public EntityBase
    {
    public:
        FhTriggerVolumeEntity(FhTriggerVolumeEntityData data, Scene* scene);

        FhTriggerVolumeEntity(const FhTriggerVolumeEntity&) = delete;
        FhTriggerVolumeEntity& operator=(const FhTriggerVolumeEntity&) = delete;
        FhTriggerVolumeEntity(FhTriggerVolumeEntity&&) = delete;
        FhTriggerVolumeEntity& operator=(FhTriggerVolumeEntity&&) = delete;

        [[nodiscard]] FhTriggerVolumeEntityData Data() const;

        void Initialize() override;
        void GetDisplayVolumes() override;
        [[nodiscard]] EntityBase* GetParent() override;
        [[nodiscard]] EntityBase* GetChild() override;

    protected:
        [[nodiscard]] std::optional<::OpenTK::Mathematics::Vector4> OverrideColor() const override;

    private:
        const FhTriggerVolumeEntityData _data;
        std::shared_ptr<EntityBase> _parent{};
        std::shared_ptr<EntityBase> _child{};

        CollisionVolume _volume{};
        ::OpenTK::Mathematics::Vector3 _parentEventColor{};
        ::OpenTK::Mathematics::Vector3 _childEventColor{};

        const std::optional<::OpenTK::Mathematics::Vector4> _overrideColor
            = ColorRgb(0xFF, 0x8C, 0x00).AsVector4();
    };

    // Jump pads only use PlayerBiped and PlayerAlt.
    enum class FhTriggerFlags : std::uint32_t
    {
        None = 0x0,
        Beam = 0x1,
        PlayerBiped = 0x2,
        PlayerAlt = 0x4
    };

    // TriggerFlags.ToString() ([Flags]) / FhTriggerFlags.ToString() (not).
    [[nodiscard]] std::string ToString(TriggerFlags value);
    [[nodiscard]] std::string ToString(FhTriggerFlags value);
}
