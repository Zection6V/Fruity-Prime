#pragma once

#include "../Formats/Enums.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace OpenTK::Windowing::GraphicsLibraryFramework
{
    enum class Keys : std::int32_t;
}

namespace MphRead::Mods
{
    class EndScreen final
    {
    public:
        EndScreen() = delete;

        struct Hit
        {
            const float Left;
            const float Top;
            const float Right;
            const float Bottom;

            constexpr Hit()
                : Left(0.0F), Top(0.0F), Right(0.0F), Bottom(0.0F)
            {
            }

            constexpr Hit(float left, float top, float right, float bottom)
                : Left(left), Top(top), Right(right), Bottom(bottom)
            {
            }

            [[nodiscard]] bool Contains(float x, float y) const;
        };

        [[nodiscard]] static bool Available();
        [[nodiscard]] static bool Ready();
        static void ClearReady();
        static void ToggleReady();

        [[nodiscard]] static MphRead::Hunter Hunter();
        [[nodiscard]] static std::int32_t Suit();
        [[nodiscard]] static std::string NextRoomKey();
        [[nodiscard]] static std::string NextRoomName();

        [[nodiscard]] static float PointerX();
        [[nodiscard]] static float PointerY();
        static void NotePointer(float x, float y);
        static void NoteLayout(
            Hit previous,
            Hit next,
            std::shared_ptr<std::vector<Hit>> suits,
            Hit ready = {});

        [[nodiscard]] static std::int32_t HoveredSuit();
        [[nodiscard]] static bool HoveredPrev();
        [[nodiscard]] static bool HoveredNext();
        [[nodiscard]] static bool HoveredReady();

        [[nodiscard]] static bool HandleClick();
        [[nodiscard]] static bool HandleKeyDown(
            OpenTK::Windowing::GraphicsLibraryFramework::Keys key);
        static void PollGamepad();

    private:
        static void Step(std::int32_t hunterBy, std::int32_t suitBy);
        static void Choose(MphRead::Hunter hunter, std::int32_t suit);

        static bool _ready;
        static Hit _hitPrev;
        static Hit _hitNext;
        static Hit _hitReady;
        static std::vector<Hit> _hitSuits;
        static float _pointerX;
        static float _pointerY;
    };
}
