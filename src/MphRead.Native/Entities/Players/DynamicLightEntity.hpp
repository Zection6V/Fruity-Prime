#pragma once

#include "../EntityBase.hpp"

namespace MphRead::Entities
{
    class DynamicLightEntityBase : public EntityBase
    {
    protected:
        ::OpenTK::Mathematics::Vector3 _light1Vector{};
        ::OpenTK::Mathematics::Vector3 _light1Color{};
        ::OpenTK::Mathematics::Vector3 _light2Vector{};
        ::OpenTK::Mathematics::Vector3 _light2Color{};

    public:
        [[nodiscard]] ::OpenTK::Mathematics::Vector3 Light1Vector() const;
        [[nodiscard]] ::OpenTK::Mathematics::Vector3 Light1Color() const;
        [[nodiscard]] ::OpenTK::Mathematics::Vector3 Light2Vector() const;
        [[nodiscard]] ::OpenTK::Mathematics::Vector3 Light2Color() const;

    protected:
        bool _useRoomLights = false;

    public:
        DynamicLightEntityBase(EntityType type, Scene* scene);

    private:
        static constexpr float _colorStep = 8.0F / 255.0F;

    protected:
        void UpdateLightSources(::OpenTK::Mathematics::Vector3 position);
        [[nodiscard]] LightInfo GetLightInfo() override;
    };
}
