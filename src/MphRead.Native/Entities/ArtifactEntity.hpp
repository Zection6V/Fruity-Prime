#pragma once

#include "../Formats/Entity.hpp"
#include "EntityBase.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace MphRead::Entities
{
    class ArtifactEntity : public EntityBase
    {
    public:
        bool Active = false;

        ArtifactEntity(ArtifactEntityData data, std::string nodeName, Scene* scene);

        ArtifactEntity(const ArtifactEntity&) = delete;
        ArtifactEntity& operator=(const ArtifactEntity&) = delete;
        ArtifactEntity(ArtifactEntity&&) = delete;
        ArtifactEntity& operator=(ArtifactEntity&&) = delete;

        [[nodiscard]] std::uint8_t ModelId() const noexcept;
        [[nodiscard]] std::uint8_t ArtifactId() const noexcept;

        void Initialize() override;
        [[nodiscard]] bool Process() override;
        void GetDrawInfo() override;
        void HandleMessage(MessageInfo info) override;
        void Destroy() override;

    protected:
        [[nodiscard]] LightInfo GetLightInfo() override;
        [[nodiscard]] ::OpenTK::Mathematics::Matrix4 GetModelTransform(
            ModelInstance& inst, std::int32_t index) override;

    private:
        const ArtifactEntityData _data;
        float _heightOffset = 0.0F;

        bool _invSetUp = false;
        std::shared_ptr<EntityBase> _parent{};
        ::OpenTK::Mathematics::Vector3 _invPos{};
        std::shared_ptr<EntityBase> _msgTarget1{};
        std::shared_ptr<EntityBase> _msgTarget2{};
        std::shared_ptr<EntityBase> _msgTarget3{};

        const std::array<std::int32_t, 32> _scanIds{
            48, 48, 48, 48, 48, 48, 48, 48, 40, 41, 42, 40, 41, 42, 40, 41,
            42, 40, 41, 42, 40, 41, 42, 40, 41, 42, 40, 41, 42, 40, 41, 42
        };

        const std::array<std::int32_t, 8> _octolithMessageIds{
            14, 31, 32, 33, 34, 35, 36, 51
        };
    };
}
