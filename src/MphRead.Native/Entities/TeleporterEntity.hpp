#pragma once

#include "../Formats/Entity.hpp"
#include "EntityBase.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace MphRead::Entities
{
    class TeleporterEntity : public EntityBase
    {
    private:
        const TeleporterEntityData _data;
        ::OpenTK::Mathematics::Vector3 _targetPos = ::OpenTK::Mathematics::Vector3::Zero;
        ::OpenTK::Mathematics::Matrix4 _artifact1Transform{};
        ::OpenTK::Mathematics::Matrix4 _artifact2Transform{};
        ::OpenTK::Mathematics::Matrix4 _artifact3Transform{};

        bool _big = false;

    public:
        // C# declares `new bool Active`, so this intentionally hides EntityBase::Active.
        bool Active = false;

    private:
        bool _bool3 = false;
        bool _bool4 = false;
        std::vector<bool> _triggeredSlots = CreateTriggeredSlots();
        std::int32_t _targetRoomId = -1;
        Formats::Culling::NodeRef _targetNodeRef = Formats::Culling::NodeRef::None;

        const std::optional<::OpenTK::Mathematics::Vector4> _overrideColor
            = ColorRgb(0xFF, 0xFF, 0xFF).AsVector4();
        const ::OpenTK::Mathematics::Vector4 _overrideColor2
            = ColorRgb(0xAA, 0xAA, 0xAA).AsVector4();

    public:
        TeleporterEntity(TeleporterEntityData data, std::string nodeName,
            Scene* scene, bool forceMultiplayer = false);

        TeleporterEntity(const TeleporterEntity&) = delete;
        TeleporterEntity& operator=(const TeleporterEntity&) = delete;
        TeleporterEntity(TeleporterEntity&&) = delete;
        TeleporterEntity& operator=(TeleporterEntity&&) = delete;

        [[nodiscard]] TeleporterEntityData Data() const;

        void Initialize() override;
        [[nodiscard]] bool Process() override;
        void SetTriggered();
        void HandleMessage(MessageInfo info) override;
        void Destroy() override;
        void GetDrawInfo() override;
        void GetDisplayVolumes() override;

    protected:
        [[nodiscard]] std::optional<::OpenTK::Mathematics::Vector4>
            OverrideColor() const override;
        [[nodiscard]] ::OpenTK::Mathematics::Matrix4 GetModelTransform(
            ModelInstance& inst, std::int32_t index) override;
        [[nodiscard]] std::optional<::OpenTK::Mathematics::Vector4> GetOverrideColor(
            ModelInstance& inst, std::int32_t index) override;
        [[nodiscard]] std::int32_t GetModelRecolor(
            ModelInstance& inst, std::int32_t index) override;

    private:
        [[nodiscard]] static std::vector<bool> CreateTriggeredSlots();
        [[nodiscard]] bool Compare(
            std::span<const char> data, std::span<const char> room);
        void Activate();
        void ActivateAnimaton();
        void InitiateAnimaton();
    };
}
