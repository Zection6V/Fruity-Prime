#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace MphRead
{
    class Scene;
    struct ColorRgba;
}

namespace MphRead::Mods::Render
{
    // A room's launcher preview as a HUD texture, decoded at most once a frame
    // and bound in the scene's own names.
    class MapThumbnail final
    {
    public:
        MapThumbnail() = delete;

        static void Clear();
        static void BeginFrame() noexcept { _decodedThisFrame = false; }
        [[nodiscard]] static std::int32_t For(const std::string& roomKey, Scene& scene);

    private:
        struct Entry
        {
            std::int32_t BindingId = 0;
            bool Missing = false;
        };

        [[nodiscard]] static std::optional<std::vector<ColorRgba>> Decode(const std::string& roomKey);

        static constexpr std::int32_t Width = 256;
        static constexpr std::int32_t Height = 144;
        static constexpr std::int32_t ReservedName = 1'100'000;
        static constexpr std::int32_t NameRing = 64;

        // StringComparer.OrdinalIgnoreCase: keyed on the upper-cased room.
        inline static std::map<std::string, Entry> _cache{};
        inline static bool _decodedThisFrame = false;
        inline static std::int32_t _nextName = 0;
    };
}
