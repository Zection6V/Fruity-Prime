#pragma once

// Renderer.hpp owns OpenTK::Mathematics::Vector2i and RenderWindow, both of
// which are in these signatures by value.
#include "../Renderer.hpp"

namespace MphRead::Mods
{
    // Put the game window back the size and in the corner it was left.
    //
    // Only the **shell** window -- the one window the program spends its life
    // in. Every other RenderWindow this process opens is a measuring
    // instrument (-maptest, -renderprobe, the thumbnail runs, the network
    // harness) that was given a size on purpose, and letting those write the
    // preference would mean a player who ran one screenshot command opens the
    // game at 400x240 afterwards.
    //
    // Two things here are not obvious and are the whole of why this is a file
    // rather than four lines in the constructor:
    //
    // - A saved rectangle is a claim about hardware that may be gone. A window
    //   restored onto a monitor that has since been unplugged, or onto a
    //   laptop whose dock is not attached, is a window nobody can reach and
    //   cannot be dragged back. So the rectangle is checked against the
    //   displays that exist *now* and pulled onto one if it does not overlap
    //   any of them.
    // - Maximized is not a size. Restoring a maximized window by its rectangle
    //   gets it visibly wrong: it fills the screen without being maximized, so
    //   the caption button offers to restore a window that is not maximized,
    //   and it will not follow a change of resolution. The state is saved
    //   separately and the rectangle underneath it is kept, so un-maximizing
    //   lands where it used to.
    class WindowGeometry final
    {
    public:
        WindowGeometry() = delete;
        ~WindowGeometry() = delete;
        WindowGeometry(const WindowGeometry&) = delete;
        WindowGeometry& operator=(const WindowGeometry&) = delete;
        WindowGeometry(WindowGeometry&&) = delete;
        WindowGeometry& operator=(WindowGeometry&&) = delete;

        // Whether the shell window's size is the player's to keep.
        //
        // False while a capture is driving it. -shellshot walks the real
        // window through the real loop -- which includes maximizing it
        // mid-sequence -- so without this, photographing the launcher would
        // leave a player's next session maximized, and a capture would open at
        // whatever size they had last dragged it to instead of the
        // deterministic one its pictures are compared at.
        [[nodiscard]] static bool Enabled() noexcept;
        static void Enabled(bool value) noexcept;

        // Whether this process has a shell window at all -- the one window a
        // player uses, as opposed to the measuring instruments.
        //
        // Enabled says whether the shape is the player's to keep; this says
        // whether there is a player's window in the first place. Both are
        // needed because NoteMode is called from WindowMode, which is handed a
        // window and has no way to tell which one it is: -maptest -fullscreen
        // enters fullscreen too, and it must not leave the launcher opening
        // that way afterwards.
        [[nodiscard]] static bool Owned() noexcept;
        static void Owned(bool value) noexcept;

        // Apply the saved geometry, before the window is shown.
        //
        // Everything here is best-effort by nature. GLFW will refuse a
        // position on Wayland (there is no concept of one), a window manager
        // is free to ignore a size, and a headless run has no monitor to ask
        // -- none of which is a reason to fail to open a window.
        static void Restore(
            MphRead::RenderWindow& window, OpenTK::Mathematics::Vector2i minimum);

        // The window moved or was resized: keep the new shape, and start the
        // clock on writing it down.
        //
        // Called from the resize and move callbacks, which fire continuously
        // while a window is being dragged -- so this only touches memory. The
        // file is written by Flush once the dragging stops.
        //
        // Recording as it happens rather than only as the window closes is
        // what makes the feature survive the ways a program actually ends: a
        // crash, a kill, a machine that went to sleep and never came back,
        // and -- the one that started this -- an exit path that does not run
        // the close handler. A window size is not worth losing to any of them.
        static void Note(MphRead::RenderWindow& window);

        // Write it down, once the shape has been still for a moment.
        //
        // Called once a frame, and almost always does nothing: the check is a
        // bool. The wait is what keeps a drag -- which is dozens of resize
        // callbacks a second -- from being dozens of file writes.
        static void Flush(bool force = false);

        // The window went fullscreen, or came back: keep that too, so the next
        // session opens the way this one was left.
        //
        // The size and the corner have been remembered since they existed; the
        // *mode* was not, and only the drop-down in Settings ever wrote it. So
        // F11 was a decision the program forgot the moment it closed -- a
        // player who plays fullscreen pressed it again every single launch --
        // and now that the launcher and the match are one window, that is the
        // launcher opening in a window as well.
        //
        // The window's live state, not an intention: whichever of F11,
        // Alt+Enter, the pause menu or the settings row put it there, what
        // gets written down is where it ended up.
        static void NoteMode();

        // Take the geometry now and write it out, for the window closing.
        //
        // A backstop rather than the mechanism: Note has normally already kept
        // it. It still matters for the window that is closed without ever
        // being resized, where there is nothing dirty to flush and the shape
        // is still worth keeping.
        static void Remember(MphRead::RenderWindow& window);
    };
}
