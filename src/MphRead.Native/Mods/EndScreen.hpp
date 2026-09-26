#pragma once

#include "../Formats/Enums.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace OpenTK::Windowing::GraphicsLibraryFramework
{
    enum class Keys : std::int32_t;
}

namespace MphRead::Mods::Input
{
    class GamepadUiRouter;
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
        // Whether the deck panel is drawn over the results, so the HUD's own
        // picker knows to leave the right-hand side alone. Written on the
        // toolkit's thread and read inside the render loop.
        [[nodiscard]] static bool PanelUp() noexcept { return _panelUp.load(); }
        static void PanelUp(bool value) noexcept { _panelUp.store(value); }
        // The screen coming up and going away again: the map ballot and its
        // previews hang off it. Called once a frame by the window.
        static void Tick(const std::string& roomKey, double time);
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
        // Set both, from a screen that offers them as rows rather than arrows.
        static void Pick(MphRead::Hunter hunter, std::int32_t suit);

    private:
        [[nodiscard]] static Input::GamepadUiRouter& ResultPad();
        static void StepList(std::int32_t by);
        static void Step(std::int32_t hunterBy, std::int32_t suitBy);
        static void Choose(MphRead::Hunter hunter, std::int32_t suit);

        static bool _ready;
        inline static std::atomic_bool _panelUp{false};
        inline static bool _wasUp = false;
        inline static double _resentAt = 0;
        static Hit _hitPrev;
        static Hit _hitNext;
        static Hit _hitReady;
        static std::vector<Hit> _hitSuits;
        static float _pointerX;
        static float _pointerY;
    };
}
