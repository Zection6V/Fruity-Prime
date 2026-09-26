#pragma once

#include "../../Formats/Enums.hpp"

#include <cstdint>
#include <future>
#include <memory>
#include <optional>
#include <vector>

namespace MphRead::Mods::Render
{
    // A picture of a hunter for a head whose screens have no window under
    // them to draw a model into (Android): the head renders it and hands the
    // pixels back.
    class IHunterShot
    {
    public:
        virtual ~IHunterShot() = default;
        [[nodiscard]] virtual std::shared_future<std::optional<std::vector<std::uint8_t>>> RenderAsync(
            Hunter hunter, std::int32_t suit, std::int32_t width, std::int32_t height) = 0;
    };

    class HunterShot final
    {
    public:
        HunterShot() = delete;

        inline static std::shared_ptr<IHunterShot> Current{};
        inline static bool InFrame = false;
        inline static bool HoleWanted = false;
        inline static float HoleLeft = 0;
        inline static float HoleTop = 0;
        inline static float HoleRight = 0;
        inline static float HoleBottom = 0;
        inline static Hunter HoleHunter = Hunter::Samus;
        inline static std::int32_t HoleSuit = 0;
        inline static double FrameWidth = 0;
        inline static double FrameHeight = 0;
        inline static double FrameScale = 1;
    };
}
