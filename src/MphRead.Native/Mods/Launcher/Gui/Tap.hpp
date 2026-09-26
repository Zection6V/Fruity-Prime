#pragma once

#include "GuiTheme.hpp"

namespace MphRead::Mods::Launcher::Gui
{
    // Whether a press and the release after it were a tap, or the beginning
    // of a scroll that happened to start on this row. The press decides
    // nothing; a finger that travels more than Slop has stopped meaning the
    // row; the release acts, and only inside the control. The distance test
    // is for a finger or a stylus only -- a mouse keeps what it had.
    //
    // The pointer is an identity only (Avalonia's IPointer, compared by
    // reference), so it is carried as an opaque address.
    class Tap final
    {
    public:
        // How far a finger may wander and still mean the row it started on.
        static constexpr double Slop = 8;

        // A press is still live: it has not been cancelled or let go.
        [[nodiscard]] bool Down() const noexcept { return _pointer != nullptr; }

        // Where the pointer has got to since the press, or nothing if there
        // is no press.
        [[nodiscard]] GuiVector Travel(GuiPoint p) const noexcept
        {
            return _pointer == nullptr ? GuiVector{} : p - _origin;
        }

        void Press(const void* pointer, GuiPoint origin, bool drags) noexcept;
        // True when this move is what cancelled the tap.
        [[nodiscard]] bool Moved(const void* pointer, GuiPoint p) noexcept;
        // True when the press and this release were a tap.
        [[nodiscard]] bool Release(const void* pointer, GuiPoint p, GuiSize size) noexcept;

        // Whether a travel is along a row rather than down the page. Ties go
        // to the row.
        [[nodiscard]] static bool Sideways(GuiVector travel) noexcept;

        // Give the gesture up. True if there was one.
        bool Cancel() noexcept;

    private:
        const void* _pointer = nullptr;
        GuiPoint _origin{};
        bool _drags = false;
    };
}
