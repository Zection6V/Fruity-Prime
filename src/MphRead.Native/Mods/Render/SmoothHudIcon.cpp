#include "SmoothHudIcon.hpp"

#include "../../Scene.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace
{
    [[nodiscard]] std::int32_t WrappedAdd(std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] std::int32_t WrappedProduct(std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) * static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] MphRead::ColorRgba& TextureAt(
        std::vector<MphRead::ColorRgba>& texture, std::int32_t index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= texture.size())
        {
            throw MphRead::SceneDetail::IndexOutOfRangeException();
        }
        return texture[static_cast<std::size_t>(index)];
    }
}

namespace MphRead::Mods::Render
{
    std::shared_ptr<Hud::HudObjectInstance> SmoothHudIcon::Create(
        const std::shared_ptr<Hud::HudObject>& sheet)
    {
        if (!sheet)
        {
            throw System::NullReferenceException();
        }
        auto inst = std::make_shared<Hud::HudObjectInstance>(
            sheet->Width, sheet->Height,
            WrappedProduct(sheet->Width, Factor), WrappedProduct(sheet->Height, Factor));
        inst->Smooth = true;
        inst->Enabled = true;
        return inst;
    }

    void SmoothHudIcon::Tint(const std::shared_ptr<Hud::HudObjectInstance>& inst,
        Hud::ReadOnlyList<std::uint8_t> data, std::int32_t frame,
        ColorRgba color, Scene& scene)
    {
        if (!inst)
        {
            throw System::NullReferenceException();
        }
        if (inst->CharacterData == data && inst->CurrentFrame == frame
            && inst->Color.has_value() && inst->Color.value() == color
            && inst->BindingId != -1)
        {
            return;
        }
        inst->CharacterData = data;
        inst->CurrentFrame = frame;
        inst->Color = color;
        inst->PaletteIndex = -1;
        Build(*inst, data, frame, color, scene);
    }

    void SmoothHudIcon::Build(Hud::HudObjectInstance& inst,
        const Hud::ReadOnlyList<std::uint8_t>& data, std::int32_t frame,
        ColorRgba color, Scene& scene)
    {
        const std::int32_t width = inst.Width;
        const std::int32_t height = inst.Height;
        const std::int32_t outWidth = WrappedProduct(width, Factor);
        const std::int32_t outHeight = WrappedProduct(height, Factor);
        std::vector<ColorRgba>& texture = *inst.Texture;
        const std::int32_t requiredLength = WrappedProduct(outWidth, outHeight);
        if (static_cast<std::int32_t>(texture.size()) < requiredLength)
        {
            return;
        }
        const std::int32_t tilesX = width / 8;
        const std::int32_t image = WrappedProduct(WrappedProduct(frame, width), height);
        const ColorRgba transparent{};
        const ColorRgba ink(color.Red, color.Green, color.Blue, 255);
        for (std::int32_t y = 0; y < outHeight; ++y)
        {
            const std::int32_t sourceY = y / Factor;
            for (std::int32_t x = 0; x < outWidth; ++x)
            {
                TextureAt(texture, WrappedAdd(WrappedProduct(y, outWidth), x)) =
                    Ink(data, image, tilesX, width, height, x / Factor, sourceY) > 0.0F
                        ? ink
                        : transparent;
            }
        }
        if (inst.BindingId == -1)
        {
            inst.BindingId = scene.BindGetTexture(texture, outWidth, outHeight);
        }
        else
        {
            scene.BindTexture(texture, outWidth, outHeight, inst.BindingId);
        }
    }

    float SmoothHudIcon::Ink(const Hud::ReadOnlyList<std::uint8_t>& data,
        std::int32_t image, std::int32_t tilesX, std::int32_t width,
        std::int32_t height, std::int32_t x, std::int32_t y)
    {
        if (x < 0 || y < 0 || x >= width || y >= height)
        {
            return 0.0F;
        }
        std::int32_t index = image;
        index = WrappedAdd(index, WrappedProduct(WrappedProduct(y / 8, tilesX), 64));
        index = WrappedAdd(index, WrappedProduct(x / 8, 64));
        index = WrappedAdd(index, WrappedProduct(y % 8, 8));
        index = WrappedAdd(index, x % 8);
        if (index < 0)
        {
            return 0.0F;
        }
        if (!data)
        {
            throw System::NullReferenceException();
        }
        if (static_cast<std::size_t>(index) >= data->size())
        {
            return 0.0F;
        }
        return data->at(static_cast<std::size_t>(index)) == 0 ? 0.0F : 1.0F;
    }
}
