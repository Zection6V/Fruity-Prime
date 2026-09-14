#pragma once

#include "../Formats/Entity.hpp"
#include "../Formats/Formats.hpp"
#include "EntityBase.hpp"

#include <memory>
#include <optional>

namespace MphRead::Formats
{
    class NodeData3;
}

namespace MphRead::Entities
{
    class PlayerEntity;

    class FlagBaseEntity : public EntityBase
    {
    public:
        FlagBaseEntity(FlagBaseEntityData data, Scene* scene);

        FlagBaseEntity(const FlagBaseEntity&) = delete;
        FlagBaseEntity& operator=(const FlagBaseEntity&) = delete;
        FlagBaseEntity(FlagBaseEntity&&) = delete;
        FlagBaseEntity& operator=(FlagBaseEntity&&) = delete;

        [[nodiscard]] FlagBaseEntityData Data() const;
        [[nodiscard]] std::shared_ptr<Formats::NodeData3> ClosestNode() const noexcept;
        void SetClosestNode(std::shared_ptr<Formats::NodeData3> value) noexcept;

        [[nodiscard]] bool Process() override;
        void GetDisplayVolumes() override;

    protected:
        [[nodiscard]] std::optional<OpenTK::Mathematics::Vector4> OverrideColor() const override;

    private:
        [[nodiscard]] bool CheckOwnOctolith(PlayerEntity& player);

        const FlagBaseEntityData _data;
        CollisionVolume _volume{};
        bool _capture = false;
        std::shared_ptr<Formats::NodeData3> _closestNode{};
        const std::optional<OpenTK::Mathematics::Vector4> _overrideColor
            = ColorRgb(15, 207, 255).AsVector4();
    };
}
