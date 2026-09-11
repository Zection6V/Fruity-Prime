#pragma once

#include "../Formats/Entity.hpp"
#include "../Formats/Types.hpp"
#include "EntityBase.hpp"

#include <optional>

namespace MphRead::Entities
{
    class LightSourceEntity : public EntityBase
    {
    public:
        LightSourceEntity(LightSourceEntityData data, Scene* scene);

        LightSourceEntity(const LightSourceEntity&) = delete;
        LightSourceEntity& operator=(const LightSourceEntity&) = delete;
        LightSourceEntity(LightSourceEntity&&) = delete;
        LightSourceEntity& operator=(LightSourceEntity&&) = delete;

        [[nodiscard]] CollisionVolume Volume() const;
        [[nodiscard]] bool Light1Enabled() const;
        [[nodiscard]] ::OpenTK::Mathematics::Vector3 Light1Vector() const;
        [[nodiscard]] ::OpenTK::Mathematics::Vector3 Light1Color() const;
        [[nodiscard]] bool Light2Enabled() const;
        [[nodiscard]] ::OpenTK::Mathematics::Vector3 Light2Vector() const;
        [[nodiscard]] ::OpenTK::Mathematics::Vector3 Light2Color() const;

    protected:
        [[nodiscard]] std::optional<::OpenTK::Mathematics::Vector4> OverrideColor() const override;
        void GetDisplayVolumes() override;

    private:
        const LightSourceEntityData _data;
        const std::optional<::OpenTK::Mathematics::Vector4> _overrideColor
            = ColorRgb(0xFF, 0xDE, 0xAD).AsVector4();

        CollisionVolume _volume;
        bool _light1Enabled;
        ::OpenTK::Mathematics::Vector3 _light1Vector;
        ::OpenTK::Mathematics::Vector3 _light1Color;
        bool _light2Enabled;
        ::OpenTK::Mathematics::Vector3 _light2Vector;
        ::OpenTK::Mathematics::Vector3 _light2Color;

        const ::OpenTK::Mathematics::Vector4 _volumeColor
            = ColorRgb(0xBB, 0x9D, 0x7A).AsVector4();
    };
}
