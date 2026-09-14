#pragma once

#include "../Formats/Entity.hpp"
#include "EntityBase.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace MphRead::Entities::Enemies
{
    class Enemy49Entity;
}

namespace MphRead::Entities
{
    class ForceFieldEntity : public EntityBase
    {
    public:
        ForceFieldEntity(ForceFieldEntityData data, std::string nodeName, Scene* scene);

        ForceFieldEntity(const ForceFieldEntity&) = delete;
        ForceFieldEntity& operator=(const ForceFieldEntity&) = delete;
        ForceFieldEntity(ForceFieldEntity&&) = delete;
        ForceFieldEntity& operator=(ForceFieldEntity&&) = delete;

        [[nodiscard]] ForceFieldEntityData Data() const;
        [[nodiscard]] OpenTK::Mathematics::Vector3 FieldUpVector() const noexcept;
        [[nodiscard]] OpenTK::Mathematics::Vector3 FieldFacingVector() const noexcept;
        [[nodiscard]] OpenTK::Mathematics::Vector3 FieldRightVector() const noexcept;
        [[nodiscard]] OpenTK::Mathematics::Vector4 Plane() const noexcept;
        [[nodiscard]] float Width() const noexcept;
        [[nodiscard]] float Height() const noexcept;
        [[nodiscard]] bool Active() const noexcept;
        [[nodiscard]] std::shared_ptr<Enemies::Enemy49Entity> Lock() const noexcept;

        void Initialize() override;
        [[nodiscard]] bool Process() override;
        void HandleMessage(MessageInfo info) override;
        void GetDrawInfo() override;

    private:
        const ForceFieldEntityData _data;
        std::shared_ptr<Enemies::Enemy49Entity> _lock{};
        OpenTK::Mathematics::Vector3 _upVector = OpenTK::Mathematics::Vector3::Zero;
        OpenTK::Mathematics::Vector3 _facingVector = OpenTK::Mathematics::Vector3::Zero;
        OpenTK::Mathematics::Vector3 _rightVector = OpenTK::Mathematics::Vector3::Zero;
        OpenTK::Mathematics::Vector4 _plane = OpenTK::Mathematics::Vector4::Zero;
        float _width = 0.0F;
        float _height = 0.0F;
        bool _active = false;

        static const std::array<std::int32_t, 10> _scanIds;
    };
}
