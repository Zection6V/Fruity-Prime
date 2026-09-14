#pragma once

#include "../Formats/Entity.hpp"
#include "../Formats/Formats.hpp"
#include "EntityBase.hpp"

#include <optional>
#include <string>

namespace MphRead::Entities
{
    class MorphCameraEntity : public EntityBase
    {
    public:
        MorphCameraEntity(MorphCameraEntityData data, std::string nodeName, Scene* scene);

        MorphCameraEntity(const MorphCameraEntity&) = delete;
        MorphCameraEntity& operator=(const MorphCameraEntity&) = delete;
        MorphCameraEntity(MorphCameraEntity&&) = delete;
        MorphCameraEntity& operator=(MorphCameraEntity&&) = delete;

        [[nodiscard]] bool Process() override;
        void GetDisplayVolumes() override;

    protected:
        [[nodiscard]] std::optional<::OpenTK::Mathematics::Vector4> OverrideColor() const override;

    private:
        const MorphCameraEntityData _data;
        const std::optional<::OpenTK::Mathematics::Vector4> _overrideColor
            = ColorRgb(0x00, 0xFF, 0x00).AsVector4();

        CollisionVolume _volume{};
        static const ::OpenTK::Mathematics::Vector3 _volumeColor;
    };

    class FhMorphCameraEntity : public EntityBase
    {
    public:
        FhMorphCameraEntity(FhMorphCameraEntityData data, Scene* scene);

        FhMorphCameraEntity(const FhMorphCameraEntity&) = delete;
        FhMorphCameraEntity& operator=(const FhMorphCameraEntity&) = delete;
        FhMorphCameraEntity(FhMorphCameraEntity&&) = delete;
        FhMorphCameraEntity& operator=(FhMorphCameraEntity&&) = delete;

        void GetDisplayVolumes() override;

    protected:
        [[nodiscard]] std::optional<::OpenTK::Mathematics::Vector4> OverrideColor() const override;

    private:
        const FhMorphCameraEntityData _data;
        const std::optional<::OpenTK::Mathematics::Vector4> _overrideColor
            = ColorRgb(0x00, 0xFF, 0x00).AsVector4();

        CollisionVolume _volume{};
        static const ::OpenTK::Mathematics::Vector3 _volumeColor;
    };
}
