#pragma once

#include "../Formats/Entity.hpp"
#include "EntityBase.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace MphRead::Formats::Collision
{
    class CollisionInstance;
    class Portal;
}

namespace MphRead::Entities
{
    enum class DoorFlags : std::uint16_t
    {
        None = 0,
        Loaded = 1,
        Locked = 2,
        Unlocked = 4,
        ShotOpen = 8,
        Opening = 0x10,
        ShouldOpen = 0x20,
        Open = 0x40,
        Closed = 0x80,
        Bit8 = 0x100,
        Bit9 = 0x200,
        Bit10 = 0x400,
        Bit11 = 0x800, // unused?
        ShowLock = 0x1000
    };

    [[nodiscard]] constexpr DoorFlags operator|(DoorFlags left, DoorFlags right) noexcept
    {
        return static_cast<DoorFlags>(
            static_cast<std::uint16_t>(left) | static_cast<std::uint16_t>(right));
    }

    [[nodiscard]] constexpr DoorFlags operator&(DoorFlags left, DoorFlags right) noexcept
    {
        return static_cast<DoorFlags>(
            static_cast<std::uint16_t>(left) & static_cast<std::uint16_t>(right));
    }

    [[nodiscard]] constexpr DoorFlags operator^(DoorFlags left, DoorFlags right) noexcept
    {
        return static_cast<DoorFlags>(
            static_cast<std::uint16_t>(left) ^ static_cast<std::uint16_t>(right));
    }

    [[nodiscard]] constexpr DoorFlags operator~(DoorFlags value) noexcept
    {
        return static_cast<DoorFlags>(
            static_cast<std::uint16_t>(~static_cast<std::uint16_t>(value)));
    }

    constexpr DoorFlags& operator|=(DoorFlags& left, DoorFlags right) noexcept
    {
        left = left | right;
        return left;
    }

    constexpr DoorFlags& operator&=(DoorFlags& left, DoorFlags right) noexcept
    {
        left = left & right;
        return left;
    }

    constexpr DoorFlags& operator^=(DoorFlags& left, DoorFlags right) noexcept
    {
        left = left ^ right;
        return left;
    }

    class DoorEntity : public EntityBase
    {
    public:
        DoorEntity(DoorEntityData data, std::string nodeName, Scene* scene,
            std::int32_t targetRoomId = -1, std::int32_t targetLayerId = -1);

        DoorEntity(const DoorEntity&) = delete;
        DoorEntity& operator=(const DoorEntity&) = delete;
        DoorEntity(DoorEntity&&) = delete;
        DoorEntity& operator=(DoorEntity&&) = delete;

        [[nodiscard]] float Radius() const noexcept;
        [[nodiscard]] float RadiusSquared() const noexcept;
        [[nodiscard]] DoorFlags Flags() const noexcept;
        void SetFlags(DoorFlags value) noexcept;
        [[nodiscard]] std::int32_t TargetRoomId() const noexcept;
        [[nodiscard]] std::int32_t TargetLayerId() const noexcept; // see note at usage
        [[nodiscard]] std::shared_ptr<DoorEntity> LoaderDoor() const noexcept;
        void SetLoaderDoor(std::shared_ptr<DoorEntity> value) noexcept;
        [[nodiscard]] std::shared_ptr<DoorEntity> ConnectorDoor() const noexcept;
        void SetConnectorDoor(std::shared_ptr<DoorEntity> value) noexcept;
        [[nodiscard]] std::shared_ptr<Formats::Collision::Portal> Portal() const noexcept;
        [[nodiscard]] std::shared_ptr<ModelInstance> ConnectorModel() const noexcept;
        void SetConnectorModel(std::shared_ptr<ModelInstance> value) noexcept;
        [[nodiscard]] std::shared_ptr<Formats::Collision::CollisionInstance>
            ConnectorCollision() const noexcept;
        void SetConnectorCollision(
            std::shared_ptr<Formats::Collision::CollisionInstance> value) noexcept;
        [[nodiscard]] bool ConnectorInactive() const noexcept;
        void SetConnectorInactive(bool value) noexcept;

        [[nodiscard]] ::OpenTK::Mathematics::Vector3 LockPosition() const;
        [[nodiscard]] DoorEntityData Data() const;

        void Initialize() override;
        [[nodiscard]] std::shared_ptr<Formats::Collision::Portal> SetUpPort(
            std::string roomNodeName, std::string conNodeName);
        void GetPosition(::OpenTK::Mathematics::Vector3& position) override;
        void GetVectors(::OpenTK::Mathematics::Vector3& position,
            ::OpenTK::Mathematics::Vector3& up,
            ::OpenTK::Mathematics::Vector3& facing) override;
        [[nodiscard]] std::int32_t GetScanId(bool alternate = false) override;
        [[nodiscard]] bool Process() override;
        void SetAnimationFrame(std::int32_t frame);
        [[nodiscard]] std::int32_t GetAnimationFrame() const;
        void Lock(bool updateState);
        void Unlock(bool updateState, bool noLockAnimSfx);
        void HandleMessage(MessageInfo info) override;
        void Destroy() override;
        void GetDrawInfo() override;

    protected:
        [[nodiscard]] ::OpenTK::Mathematics::Matrix4 GetModelTransform(
            ModelInstance& inst, std::int32_t index) override;
        [[nodiscard]] std::int32_t GetModelRecolor(
            ModelInstance& inst, std::int32_t index) override;

    private:
        [[nodiscard]] AnimationInfo& AnimInfo();
        [[nodiscard]] const AnimationInfo& AnimInfo() const;
        [[nodiscard]] bool Locked() const noexcept;
        [[nodiscard]] bool Unlocked() const noexcept;
        [[nodiscard]] bool Compare(const char (&data)[16], const std::string& room) const;
        void UpdateScanId();
        [[nodiscard]] bool ShouldOpen();
        void ForceClose();

        const DoorEntityData _data;
        ::OpenTK::Mathematics::Matrix4 _lockTransform{};
        ModelInstance* _lock = nullptr;

        float _radius = 0.0F;
        float _radiusSquared = 0.0F;
        DoorFlags _flags = DoorFlags::None;
        std::int32_t _targetRoomId = -1;
        std::int32_t _targetLayerId = -1;
        std::shared_ptr<DoorEntity> _loaderDoor{};
        std::shared_ptr<DoorEntity> _connectorDoor{};
        std::shared_ptr<Formats::Collision::Portal> _portal{};
        std::shared_ptr<ModelInstance> _connectorModel{};
        std::shared_ptr<Formats::Collision::CollisionInstance> _connectorCollision{};
        bool _connectorInactive = false;

        static const std::array<std::int32_t, 10> _scanIds;
        static constexpr float _portWidth = 2.1F;
        static const std::array<float, 4> _portHeights;
    };

    class FhDoorEntity : public EntityBase
    {
    public:
        FhDoorEntity(FhDoorEntityData data, Scene* scene);

        FhDoorEntity(const FhDoorEntity&) = delete;
        FhDoorEntity& operator=(const FhDoorEntity&) = delete;
        FhDoorEntity(FhDoorEntity&&) = delete;
        FhDoorEntity& operator=(FhDoorEntity&&) = delete;

    private:
        const FhDoorEntityData _data;
    };
}
