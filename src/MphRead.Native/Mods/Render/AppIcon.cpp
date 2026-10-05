#include "AppIcon.hpp"

#include "../DebugLog.hpp"
#include "../../NativeRuntime/System/AssetLoader.hpp"
#include "../../NativeRuntime/Stb/Image.hpp"
#include "../../NativeRuntime/System/ExceptionText.hpp"

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace MphRead::Mods::Render
{
    std::optional<::MphRead::RendererPlatform::WindowIcon> AppIcon::_icon;
    bool AppIcon::_tried = false;

    const ::MphRead::RendererPlatform::WindowIcon* AppIcon::Load()
    {
        if (_tried)
        {
            return _icon.has_value() ? &*_icon : nullptr;
        }
        _tried = true;
        try
        {
            constexpr std::string_view resource = "avares://FruityPrime/Assets/fruity-prime-mark.png";
            if (!::MphRead::NativeRuntime::AssetLoader::Exists(resource))
            {
                DebugLog::Line("window", "no icon resource in this build");
                return nullptr;
            }
            const std::vector<std::uint8_t> bytes
                = ::MphRead::NativeRuntime::AssetLoader::Open(resource);
            const ::MphRead::NativeRuntime::Image image = ::MphRead::NativeRuntime::LoadPng(bytes, 4);
            if (image.Width <= 0 || image.Height <= 0)
            {
                throw std::runtime_error("Unable to decode the window icon PNG.");
            }
            const std::size_t byteCount = static_cast<std::size_t>(image.Width)
                * static_cast<std::size_t>(image.Height) * 4;
            if (image.Pixels.size() < byteCount)
            {
                throw std::out_of_range("The window icon image does not contain four bytes per pixel.");
            }
            const std::vector<std::uint8_t> pixels(image.Pixels.begin(), image.Pixels.begin() + byteCount);

            ::MphRead::RendererPlatform::WindowIcon icon;
            icon.Images.reserve(4);
            icon.Images.push_back(Scaled(pixels, image.Width, image.Height, 16));
            icon.Images.push_back(Scaled(pixels, image.Width, image.Height, 32));
            icon.Images.push_back(Scaled(pixels, image.Width, image.Height, 48));
            icon.Images.push_back(::MphRead::RendererPlatform::WindowIconImage{
                image.Width, image.Height, pixels});
            _icon = std::move(icon);
            DebugLog::Line("window", "window icon " + std::to_string(image.Width) + "x"
                + std::to_string(image.Height));
            return &*_icon;
        }
        catch (...)
        {
            DebugLog::Line("window", "no window icon: "
                + ::MphRead::NativeRuntime::ExceptionMessage(std::current_exception()));
            return nullptr;
        }
    }

    ::MphRead::RendererPlatform::WindowIconImage AppIcon::Scaled(
        const std::vector<std::uint8_t>& source, std::int32_t width, std::int32_t height,
        std::int32_t size)
    {
        ::MphRead::RendererPlatform::WindowIconImage result;
        result.Width = size;
        result.Height = size;
        result.Pixels.resize(static_cast<std::size_t>(size) * static_cast<std::size_t>(size) * 4);
        for (std::int32_t y = 0; y < size; y++)
        {
            const std::int32_t y0 = y * height / size;
            const std::int32_t y1 = std::max(y0 + 1, (y + 1) * height / size);
            for (std::int32_t x = 0; x < size; x++)
            {
                const std::int32_t x0 = x * width / size;
                const std::int32_t x1 = std::max(x0 + 1, (x + 1) * width / size);
                std::int64_t red = 0;
                std::int64_t green = 0;
                std::int64_t blue = 0;
                std::int64_t alpha = 0;
                std::int32_t count = 0;
                for (std::int32_t sy = y0; sy < y1 && sy < height; sy++)
                {
                    for (std::int32_t sx = x0; sx < x1 && sx < width; sx++)
                    {
                        const std::size_t offset = (static_cast<std::size_t>(sy)
                            * static_cast<std::size_t>(width) + static_cast<std::size_t>(sx)) * 4;
                        const std::int32_t coverage = source[offset + 3];
                        red += static_cast<std::int64_t>(source[offset]) * coverage;
                        green += static_cast<std::int64_t>(source[offset + 1]) * coverage;
                        blue += static_cast<std::int64_t>(source[offset + 2]) * coverage;
                        alpha += coverage;
                        count++;
                    }
                }
                const std::size_t destination = (static_cast<std::size_t>(y)
                    * static_cast<std::size_t>(size) + static_cast<std::size_t>(x)) * 4;
                if (alpha > 0)
                {
                    result.Pixels[destination] = static_cast<std::uint8_t>(red / alpha);
                    result.Pixels[destination + 1] = static_cast<std::uint8_t>(green / alpha);
                    result.Pixels[destination + 2] = static_cast<std::uint8_t>(blue / alpha);
                }
                result.Pixels[destination + 3] = count > 0
                    ? static_cast<std::uint8_t>(alpha / count)
                    : static_cast<std::uint8_t>(0);
            }
        }
        return result;
    }
}
