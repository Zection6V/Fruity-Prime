#pragma once

#include "../Formats/Entity.hpp"
#include "EntityBase.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace MphRead::Formats
{
    class NodeData3;
}

namespace MphRead::Entities
{
    class PlayerEntity;

    class OctolithFlagEntity : public EntityBase,
                               public std::enable_shared_from_this<OctolithFlagEntity>
    {
    public:
        OctolithFlagEntity(OctolithFlagEntityData data, Scene* scene);

        OctolithFlagEntity(const OctolithFlagEntity&) = delete;
        OctolithFlagEntity& operator=(const OctolithFlagEntity&) = delete;
        OctolithFlagEntity(OctolithFlagEntity&&) = delete;
        OctolithFlagEntity& operator=(OctolithFlagEntity&&) = delete;

        [[nodiscard]] OctolithFlagEntityData Data() const;
        [[nodiscard]] ::OpenTK::Mathematics::Vector3 BasePosition() const noexcept;
        [[nodiscard]] std::shared_ptr<PlayerEntity> Carrier() const noexcept;
        [[nodiscard]] bool AtBase() const noexcept;
        [[nodiscard]] std::shared_ptr<Formats::NodeData3> ClosestNode() const noexcept;
        void SetClosestNode(std::shared_ptr<Formats::NodeData3> value) noexcept;
        [[nodiscard]] std::shared_ptr<Formats::NodeData3> BaseClosestNode() const noexcept;
        void SetBaseClosestNode(std::shared_ptr<Formats::NodeData3> value) noexcept;

        void GetVectors(::OpenTK::Mathematics::Vector3& position,
            ::OpenTK::Mathematics::Vector3& up,
            ::OpenTK::Mathematics::Vector3& facing) override;
        void GetPosition(::OpenTK::Mathematics::Vector3& position) override;
        [[nodiscard]] bool Process() override;
        void OnCaptured();

    protected:
        [[nodiscard]] ::OpenTK::Mathematics::Matrix4 GetModelTransform(
            ModelInstance& inst, std::int32_t index) override;
        [[nodiscard]] std::int32_t GetModelRecolor(
            ModelInstance& inst, std::int32_t index) override;

    private:
        void SetAtBase();
        [[nodiscard]] bool OnTouched(const std::shared_ptr<PlayerEntity>& player);
        void Reset();
        void OnDropped(bool reset);
        [[nodiscard]] std::size_t CheckedSlotIndex(std::int32_t index) const;

        const OctolithFlagEntityData _data;
        ::OpenTK::Mathematics::Vector3 _basePosition = ::OpenTK::Mathematics::Vector3::Zero;
        bool _bounty = false;

        std::shared_ptr<PlayerEntity> _carrier{};
        std::shared_ptr<PlayerEntity> _lastCarrier{};
        bool _atBase = false;
        bool _grounded = false;
        float _resetTimer = 0.0F;
        float _gravity = 0.0F;

        std::shared_ptr<Formats::NodeData3> _closestNode{};
        std::shared_ptr<Formats::NodeData3> _baseClosestNode{};
    };
}
