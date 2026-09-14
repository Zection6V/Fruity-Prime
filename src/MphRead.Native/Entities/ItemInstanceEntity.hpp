#pragma once

#include "../Formats/Culling.hpp"
#include "EntityBase.hpp"

#include <array>
#include <cstdint>
#include <memory>

namespace MphRead
{
    namespace Effects
    {
        class EffectEntry;
    }

    namespace Formats
    {
        class NodeData3;
    }
}

namespace MphRead::Entities
{
    class ItemSpawnEntity;

    struct ItemInstanceEntityData
    {
        const OpenTK::Mathematics::Vector3 Position{};
        const MphRead::ItemType ItemType{};
        const std::int32_t DespawnTimer = 0;

        constexpr ItemInstanceEntityData() noexcept = default;
        constexpr ItemInstanceEntityData(
            OpenTK::Mathematics::Vector3 position,
            MphRead::ItemType type,
            std::int32_t despawnTimer) noexcept
            : Position(position),
              ItemType(type),
              DespawnTimer(despawnTimer)
        {
        }

        ItemInstanceEntityData(const ItemInstanceEntityData&) noexcept = default;
        ItemInstanceEntityData& operator=(const ItemInstanceEntityData& other) noexcept;
    };

    class SpinningEntityBase : public EntityBase
    {
    public:
        SpinningEntityBase(
            float spinSpeed,
            OpenTK::Mathematics::Vector3 spinAxis,
            EntityType type,
            Scene* scene);
        SpinningEntityBase(
            float spinSpeed,
            OpenTK::Mathematics::Vector3 spinAxis,
            std::int32_t spinModelIndex,
            EntityType type,
            Scene* scene);
        SpinningEntityBase(
            float spinSpeed,
            OpenTK::Mathematics::Vector3 spinAxis,
            std::int32_t spinModelIndex,
            std::int32_t floatModelIndex,
            EntityType type,
            Scene* scene);
        SpinningEntityBase(
            float spinSpeed,
            OpenTK::Mathematics::Vector3 spinAxis,
            std::int32_t spinModelIndex,
            std::int32_t floatModelIndex,
            EntityType type,
            Formats::Culling::NodeRef nodeRef,
            Scene* scene);

        SpinningEntityBase(const SpinningEntityBase&) = delete;
        SpinningEntityBase& operator=(const SpinningEntityBase&) = delete;
        SpinningEntityBase(SpinningEntityBase&&) = delete;
        SpinningEntityBase& operator=(SpinningEntityBase&&) = delete;
        ~SpinningEntityBase() override = 0;

        [[nodiscard]] bool Process() override;

    protected:
        [[nodiscard]] OpenTK::Mathematics::Matrix4 GetModelTransform(
            ModelInstance& inst,
            std::int32_t index) override;

    private:
        float _spin;
        const float _spinSpeed;
        const OpenTK::Mathematics::Vector3 _spinAxis;

    protected:
        std::int32_t _spinModelIndex;
        std::int32_t _floatModelIndex;

    private:
        [[nodiscard]] static float GetItemRotation() noexcept;

        static std::uint16_t _nextItemRotation;
    };

    class ItemInstanceEntity : public SpinningEntityBase
    {
    public:
        ItemInstanceEntity(
            ItemInstanceEntityData data,
            Formats::Culling::NodeRef nodeRef,
            Scene* scene);

        ItemInstanceEntity(const ItemInstanceEntity&) = delete;
        ItemInstanceEntity& operator=(const ItemInstanceEntity&) = delete;
        ItemInstanceEntity(ItemInstanceEntity&&) = delete;
        ItemInstanceEntity& operator=(ItemInstanceEntity&&) = delete;

        [[nodiscard]] MphRead::ItemType ItemType() const noexcept;
        [[nodiscard]] std::int32_t ParentId() const noexcept;
        void SetParentId(std::int32_t value) noexcept;
        [[nodiscard]] std::int32_t DespawnTimer() const noexcept;
        void SetDespawnTimer(std::int32_t value) noexcept;
        [[nodiscard]] ItemSpawnEntity* Owner() const noexcept;
        void SetOwner(ItemSpawnEntity* value) noexcept;
        [[nodiscard]] std::shared_ptr<Formats::NodeData3> ClosestNode() const noexcept;
        void SetClosestNode(std::shared_ptr<Formats::NodeData3> value) noexcept;

        void Initialize() override;
        void GetVectors(
            OpenTK::Mathematics::Vector3& position,
            OpenTK::Mathematics::Vector3& up,
            OpenTK::Mathematics::Vector3& facing) override;
        [[nodiscard]] bool Process() override;
        void OnPickedUp();
        void GetDrawInfo() override;
        void Destroy() override;

    private:
        MphRead::ItemType _itemType{};
        std::shared_ptr<Effects::EffectEntry> _effectEntry{};
        bool _linkDone = false;
        std::int32_t _parentId = -1;
        std::shared_ptr<EntityBase> _parent{};
        OpenTK::Mathematics::Vector3 _invPos{};
        std::int32_t _despawnTimer = -1;
        ItemSpawnEntity* _owner = nullptr;
        std::shared_ptr<Formats::NodeData3> _closestNode{};

        static const std::array<std::int32_t, 22> _scanIds;
        static const std::array<std::int32_t, 22> _sfxIds;
    };

    struct FhItemInstanceEntityData
    {
        const OpenTK::Mathematics::Vector3 Position{};
        const MphRead::FhItemType ItemType{};

        constexpr FhItemInstanceEntityData() noexcept = default;
        constexpr FhItemInstanceEntityData(
            OpenTK::Mathematics::Vector3 position,
            MphRead::FhItemType type) noexcept
            : Position(position),
              ItemType(type)
        {
        }

        FhItemInstanceEntityData(const FhItemInstanceEntityData&) noexcept = default;
        FhItemInstanceEntityData& operator=(const FhItemInstanceEntityData& other) noexcept;
    };

    class FhItemEntity : public SpinningEntityBase
    {
    public:
        FhItemEntity(FhItemInstanceEntityData data, Scene* scene);

        FhItemEntity(const FhItemEntity&) = delete;
        FhItemEntity& operator=(const FhItemEntity&) = delete;
        FhItemEntity(FhItemEntity&&) = delete;
        FhItemEntity& operator=(FhItemEntity&&) = delete;
    };
}
