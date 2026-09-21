#pragma once

#include "../Formats/Entity.hpp"
#include "../Formats/Formats.hpp"
#include "EntityBase.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace MphRead::Formats
{
    class NodeData3;
}

namespace MphRead::Entities
{
    class PlayerEntity;

    class NodeDefenseEntity : public EntityBase
    {
    public:
        NodeDefenseEntity(NodeDefenseEntityData data, Scene* scene);

        NodeDefenseEntity(const NodeDefenseEntity&) = delete;
        NodeDefenseEntity& operator=(const NodeDefenseEntity&) = delete;
        NodeDefenseEntity(NodeDefenseEntity&&) = delete;
        NodeDefenseEntity& operator=(NodeDefenseEntity&&) = delete;

        [[nodiscard]] CollisionVolume Volume() const noexcept;
        [[nodiscard]] std::shared_ptr<PlayerEntity> CapturedPlayer() const noexcept;
        [[nodiscard]] bool Contested() const noexcept;
        [[nodiscard]] bool InProgress() const noexcept;
        [[nodiscard]] std::int32_t CurrentTeam() const noexcept;
        [[nodiscard]] std::int32_t OccupyingTeam() const noexcept;
        [[nodiscard]] bool Blinking() const noexcept;
        [[nodiscard]] std::shared_ptr<const std::vector<bool>> OccupiedBy() const noexcept;
        [[nodiscard]] bool IsOccupied() const;
        [[nodiscard]] float Progress() const noexcept;
        [[nodiscard]] std::shared_ptr<Formats::NodeData3> ClosestNode() const noexcept;
        void SetClosestNode(std::shared_ptr<Formats::NodeData3> value) noexcept;

        [[nodiscard]] bool Process() override;
        void GetDrawInfo() override;
        void GetDisplayVolumes() override;

    protected:
        [[nodiscard]] ::OpenTK::Mathematics::Matrix4 GetModelTransform(
            ModelInstance& inst, std::int32_t index) override;

    private:
        void ProcessDefender();
        void ProcessNodes();
        void Complete(std::int32_t& dest1, std::int32_t& dest2);
        [[nodiscard]] std::size_t CheckedOccupiedIndex(std::int32_t index) const;

        const NodeDefenseEntityData _data;
        ::OpenTK::Mathematics::Matrix4 _circleScale{};
        CollisionVolume _volume{};
        bool _defender = false;

        std::int32_t _currentTeam = 4;
        std::int32_t _occupyingTeam = 4;
        std::shared_ptr<std::vector<bool>> _occupiedBy;
        float _blinkTimer = 0.0F;
        std::shared_ptr<PlayerEntity> _capturedPlayer{};
        float _progress = 0.0F;
        float _scoreTimer = 0.0F;
        float _curRotation = 0.0F;
        float _spinSpeed = 0.0F;
        bool _contested = false;
        bool _inProgress = false;

        std::shared_ptr<Material> _terminalMat{};
        std::shared_ptr<Material> _ringMat{};
        std::shared_ptr<Formats::NodeData3> _closestNode{};

        static const ColorRgb _neutralColor;
        static const ColorRgb _selfColor;
        static const ColorRgb _enemyColor;
    };
}
