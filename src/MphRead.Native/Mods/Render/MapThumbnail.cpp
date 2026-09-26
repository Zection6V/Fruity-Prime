#include "MapThumbnail.hpp"

#include "../DebugLog.hpp"
#include "../Headless.hpp"
#include "../ThumbnailGenerator.hpp"
#include "../../Formats/Types.hpp"
#include "../../Scene.hpp"
#include "../../NativeRuntime/Stb/Image.hpp"
#include "../../NativeRuntime/System/Globalization.hpp"
#include "../../NativeRuntime/System/IO.hpp"

#include <algorithm>
#include <exception>

namespace MphRead::Mods::Render
{
    namespace Runtime = ::MphRead::NativeRuntime;

    void MapThumbnail::Clear()
    {
        _cache.clear();
    }

    std::int32_t MapThumbnail::For(const std::string& roomKey, Scene& scene)
    {
        if (roomKey.empty() || Headless::Active())
        {
            return 0;
        }
        const std::string key = Runtime::ToUpperInvariant(roomKey);
        const auto found = _cache.find(key);
        if (found != _cache.end())
        {
            return found->second.Missing ? 0 : found->second.BindingId;
        }
        if (_decodedThisFrame)
        {
            return 0;
        }
        _decodedThisFrame = true;
        Entry& made = _cache[key];
        const std::optional<std::vector<ColorRgba>> pixels = Decode(roomKey);
        if (!pixels.has_value())
        {
            made.Missing = true;
            DebugLog::Line("hud", "no map preview for " + roomKey);
            return 0;
        }
        try
        {
            made.BindingId = ReservedName + _nextName % NameRing;
            _nextName++;
            scene.BindTexture(*pixels, Width, Height, made.BindingId);
        }
        catch (const std::exception& ex)
        {
            made.Missing = true;
            DebugLog::Line("hud", "map preview for " + roomKey + " would not bind: " + ex.what());
        }
        DebugLog::Line("hud", "map preview for " + roomKey + " bound as " + std::to_string(made.BindingId));
        return made.Missing ? 0 : made.BindingId;
    }

    std::optional<std::vector<ColorRgba>> MapThumbnail::Decode(const std::string& roomKey)
    {
        std::string path;
        try
        {
            path = ThumbnailGenerator::PathFor(roomKey);
        }
        catch (...)
        {
            return std::nullopt;
        }
        if (!Runtime::FileExists(path))
        {
            return std::nullopt;
        }
        try
        {
            const Runtime::Image image = Runtime::LoadPng(Runtime::FileReadAllBytes(path), 3);
            if (image.Width <= 0 || image.Height <= 0)
            {
                return std::nullopt;
            }
            const std::vector<std::uint8_t>& source = image.Pixels;
            constexpr std::int32_t channels = 3;
            const std::int32_t stride = image.Width * channels;
            const std::int32_t limit = std::min(static_cast<std::int32_t>(source.size()), image.Width * image.Height * channels);
            if (limit < stride)
            {
                return std::nullopt;
            }
            std::vector<ColorRgba> result(static_cast<std::size_t>(Width * Height));
            for (std::int32_t y = 0; y < Height; y++)
            {
                const std::int32_t y0 = y * image.Height / Height;
                const std::int32_t y1 = std::max(y0 + 1, (y + 1) * image.Height / Height);
                for (std::int32_t x = 0; x < Width; x++)
                {
                    const std::int32_t x0 = x * image.Width / Width;
                    const std::int32_t x1 = std::max(x0 + 1, (x + 1) * image.Width / Width);
                    std::int32_t red = 0;
                    std::int32_t green = 0;
                    std::int32_t blue = 0;
                    std::int32_t count = 0;
                    for (std::int32_t sy = y0; sy < y1; sy++)
                    {
                        const std::int32_t row = sy * stride;
                        for (std::int32_t sx = x0; sx < x1; sx++)
                        {
                            const std::int32_t at = row + sx * channels;
                            if (at + 2 >= limit)
                            {
                                continue;
                            }
                            red += source[static_cast<std::size_t>(at)];
                            green += source[static_cast<std::size_t>(at + 1)];
                            blue += source[static_cast<std::size_t>(at + 2)];
                            count++;
                        }
                    }
                    if (count == 0)
                    {
                        continue;
                    }
                    result[static_cast<std::size_t>(y * Width + x)] = ColorRgba(static_cast<std::uint8_t>(red / count),
                        static_cast<std::uint8_t>(green / count), static_cast<std::uint8_t>(blue / count), 255);
                }
            }
            return result;
        }
        catch (const std::exception& ex)
        {
            DebugLog::Line("hud", "map preview for " + roomKey + " would not load: " + ex.what());
            return std::nullopt;
        }
    }
}
