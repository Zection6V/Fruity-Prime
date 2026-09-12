#include "DynamicLightEntity.hpp"

#include "../LightSourceEntity.hpp"
#include "../../Scene.hpp"

#include <cmath>
#include <cstdint>
#include <limits>

namespace
{
    [[nodiscard]] std::int32_t ConvertSingleToInt32Net9(float value) noexcept
    {
        if (std::isnan(value))
        {
            return 0;
        }

        const double wide = static_cast<double>(value);

        if (wide < static_cast<double>(std::numeric_limits<std::int32_t>::min()))
        {
            return std::numeric_limits<std::int32_t>::min();
        }

        if (wide > static_cast<double>(std::numeric_limits<std::int32_t>::max()))
        {
            return std::numeric_limits<std::int32_t>::max();
        }

        return static_cast<std::int32_t>(wide);
    }

    [[nodiscard]] std::int32_t SubtractOneUnchecked(std::int32_t value) noexcept
    {
        if (value == std::numeric_limits<std::int32_t>::min())
        {
            return std::numeric_limits<std::int32_t>::max();
        }

        return value - 1;
    }
}

namespace MphRead::Entities
{
    ::OpenTK::Mathematics::Vector3 DynamicLightEntityBase::Light1Vector() const
    {
        return _light1Vector;
    }

    ::OpenTK::Mathematics::Vector3 DynamicLightEntityBase::Light1Color() const
    {
        return _light1Color;
    }

    ::OpenTK::Mathematics::Vector3 DynamicLightEntityBase::Light2Vector() const
    {
        return _light2Vector;
    }

    ::OpenTK::Mathematics::Vector3 DynamicLightEntityBase::Light2Color() const
    {
        return _light2Color;
    }

    DynamicLightEntityBase::DynamicLightEntityBase(EntityType type, Scene* scene)
        : EntityBase(type, scene)
    {
    }

    void DynamicLightEntityBase::UpdateLightSources(
        ::OpenTK::Mathematics::Vector3 position)
    {
        using ::OpenTK::Mathematics::Vector3;

        const auto updateChannel = [](float current, float source, float frames) -> float
        {
            const float diff = source - current;

            if (std::fabs(diff) < DynamicLightEntityBase::_colorStep)
            {
                return source;
            }

            std::int32_t factor;

            if (current > source)
            {
                factor = ConvertSingleToInt32Net9(
                    std::trunc(
                        (diff + DynamicLightEntityBase::_colorStep)
                        / (8.0F * DynamicLightEntityBase::_colorStep)));

                if (factor <= -1)
                {
                    const std::int32_t adjustedFactor
                        = SubtractOneUnchecked(factor);

                    return current
                        + static_cast<float>(adjustedFactor)
                        * DynamicLightEntityBase::_colorStep
                        * frames;
                }

                return current
                    - DynamicLightEntityBase::_colorStep
                    * frames;
            }

            factor = ConvertSingleToInt32Net9(
                std::trunc(
                    diff
                    / (8.0F * DynamicLightEntityBase::_colorStep)));

            if (factor >= 1)
            {
                return current
                    + static_cast<float>(factor)
                    * DynamicLightEntityBase::_colorStep
                    * frames;
            }

            return current
                + DynamicLightEntityBase::_colorStep
                * frames;
        };

        bool hasLight1 = false;
        bool hasLight2 = false;

        Vector3 light1Color = _light1Color;
        Vector3 light1Vector = _light1Vector;
        Vector3 light2Color = _light2Color;
        Vector3 light2Vector = _light2Vector;

        if (_scene == nullptr)
        {
            throw System::NullReferenceException();
        }

        const float frames = _scene->FrameTime * 30.0F;

        for (LightSourceEntity& lightSource : _scene->GetLightSourceEntities())
        {
            if (lightSource.Volume().TestPoint(position))
            {
                if (lightSource.Light1Enabled())
                {
                    hasLight1 = true;

                    light1Vector.X +=
                        (lightSource.Light1Vector().X - light1Vector.X)
                        / 8.0F * frames;
                    light1Vector.Y +=
                        (lightSource.Light1Vector().Y - light1Vector.Y)
                        / 8.0F * frames;
                    light1Vector.Z +=
                        (lightSource.Light1Vector().Z - light1Vector.Z)
                        / 8.0F * frames;

                    light1Color.X = updateChannel(
                        light1Color.X,
                        lightSource.Light1Color().X,
                        frames);
                    light1Color.Y = updateChannel(
                        light1Color.Y,
                        lightSource.Light1Color().Y,
                        frames);
                    light1Color.Z = updateChannel(
                        light1Color.Z,
                        lightSource.Light1Color().Z,
                        frames);
                }

                if (lightSource.Light2Enabled())
                {
                    hasLight2 = true;

                    light2Vector.X +=
                        (lightSource.Light2Vector().X - light2Vector.X)
                        / 8.0F * frames;
                    light2Vector.Y +=
                        (lightSource.Light2Vector().Y - light2Vector.Y)
                        / 8.0F * frames;
                    light2Vector.Z +=
                        (lightSource.Light2Vector().Z - light2Vector.Z)
                        / 8.0F * frames;

                    light2Color.X = updateChannel(
                        light2Color.X,
                        lightSource.Light2Color().X,
                        frames);
                    light2Color.Y = updateChannel(
                        light2Color.Y,
                        lightSource.Light2Color().Y,
                        frames);
                    light2Color.Z = updateChannel(
                        light2Color.Z,
                        lightSource.Light2Color().Z,
                        frames);
                }
            }
        }

        if (!hasLight1)
        {
            light1Vector.X +=
                (_scene->Light1Vector.X - light1Vector.X)
                / 8.0F * frames;
            light1Vector.Y +=
                (_scene->Light1Vector.Y - light1Vector.Y)
                / 8.0F * frames;
            light1Vector.Z +=
                (_scene->Light1Vector.Z - light1Vector.Z)
                / 8.0F * frames;

            light1Color.X = updateChannel(
                light1Color.X,
                _scene->Light1Color.X,
                frames);
            light1Color.Y = updateChannel(
                light1Color.Y,
                _scene->Light1Color.Y,
                frames);
            light1Color.Z = updateChannel(
                light1Color.Z,
                _scene->Light1Color.Z,
                frames);
        }

        if (!hasLight2)
        {
            light2Vector.X +=
                (_scene->Light2Vector.X - light2Vector.X)
                / 8.0F * frames;
            light2Vector.Y +=
                (_scene->Light2Vector.Y - light2Vector.Y)
                / 8.0F * frames;
            light2Vector.Z +=
                (_scene->Light2Vector.Z - light2Vector.Z)
                / 8.0F * frames;

            light2Color.X = updateChannel(
                light2Color.X,
                _scene->Light2Color.X,
                frames);
            light2Color.Y = updateChannel(
                light2Color.Y,
                _scene->Light2Color.Y,
                frames);
            light2Color.Z = updateChannel(
                light2Color.Z,
                _scene->Light2Color.Z,
                frames);
        }

        _light1Color = light1Color;
        _light1Vector = light1Vector.Normalized();
        _light2Color = light2Color;
        _light2Vector = light2Vector.Normalized();
    }

    LightInfo DynamicLightEntityBase::GetLightInfo()
    {
        if (_useRoomLights)
        {
            if (_scene == nullptr)
            {
                throw System::NullReferenceException();
            }

            return EntityBase::GetLightInfo();
        }

        return LightInfo(
            _light1Vector,
            _light1Color,
            _light2Vector,
            _light2Color);
    }
}
