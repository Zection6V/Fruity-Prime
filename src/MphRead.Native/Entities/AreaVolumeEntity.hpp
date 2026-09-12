#pragma once

#include "../Formats/Entity.hpp"
#include "../Formats/Formats.hpp"
#include "EntityBase.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace MphRead::Entities
{
    class PlayerEntity;

    class AreaVolumeEntity : public EntityBase
    {
    public:
        AreaVolumeEntity(AreaVolumeEntityData data, std::string nodeName, Scene* scene);

        AreaVolumeEntity(const AreaVolumeEntity&) = delete;
        AreaVolumeEntity& operator=(const AreaVolumeEntity&) = delete;
        AreaVolumeEntity(AreaVolumeEntity&&) = delete;
        AreaVolumeEntity& operator=(AreaVolumeEntity&&) = delete;

        // C# declares `new bool Active`, so this intentionally hides EntityBase::Active.
        bool Active = false;

        [[nodiscard]] AreaVolumeEntityData Data() const;

        void Initialize() override;
        [[nodiscard]] bool GetTargetable() override;
        void HandleMessage(MessageInfo info) override;
        void GetDisplayVolumes() override;
        [[nodiscard]] EntityBase* GetParent() override;
        [[nodiscard]] EntityBase* GetChild() override;
        [[nodiscard]] bool Process() override;

    protected:
        [[nodiscard]] std::optional<::OpenTK::Mathematics::Vector4> OverrideColor() const override;

    private:
        void Trigger(PlayerEntity& player);
        void SendInsideEvent(PlayerEntity& player);
        void SendExitEvent(PlayerEntity& player);
        bool PrioritizeGravity(
            ::OpenTK::Mathematics::Vector3 position, std::int32_t slot);

        [[nodiscard]] std::size_t CheckedSlotIndex(std::int32_t slot) const;

        const AreaVolumeEntityData _data;
        std::shared_ptr<EntityBase> _parent{};
        std::shared_ptr<EntityBase> _child{};

        CollisionVolume _volume{};
        ::OpenTK::Mathematics::Vector3 _insideEventColor{};
        ::OpenTK::Mathematics::Vector3 _exitEventColor{};

        std::vector<std::int32_t> _cooldownSlots;
        std::vector<bool> _triggeredSlots;
        std::vector<std::uint32_t> _prioritySlots;
        std::int32_t _cooldownTime = 0;

        const std::optional<::OpenTK::Mathematics::Vector4> _overrideColor
            = ColorRgb(0xFF, 0xFF, 0x00).AsVector4();
    };

    class FhAreaVolumeEntity : public EntityBase
    {
    public:
        FhAreaVolumeEntity(FhAreaVolumeEntityData data, Scene* scene);

        FhAreaVolumeEntity(const FhAreaVolumeEntity&) = delete;
        FhAreaVolumeEntity& operator=(const FhAreaVolumeEntity&) = delete;
        FhAreaVolumeEntity(FhAreaVolumeEntity&&) = delete;
        FhAreaVolumeEntity& operator=(FhAreaVolumeEntity&&) = delete;

        [[nodiscard]] FhAreaVolumeEntityData Data() const;

        void GetDisplayVolumes() override;

    protected:
        [[nodiscard]] std::optional<::OpenTK::Mathematics::Vector4> OverrideColor() const override;

    private:
        const FhAreaVolumeEntityData _data;
        CollisionVolume _volume{};
        ::OpenTK::Mathematics::Vector3 _insideEventColor{};
        ::OpenTK::Mathematics::Vector3 _exitEventColor{};

        const std::optional<::OpenTK::Mathematics::Vector4> _overrideColor
            = ColorRgb(0xFF, 0xFF, 0x00).AsVector4();
    };
}
