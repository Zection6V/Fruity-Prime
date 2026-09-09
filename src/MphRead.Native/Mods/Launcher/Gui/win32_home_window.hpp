#pragma once

// The Win32 head for the launcher front screen.
//
// HomeView already decides everything about this screen -- which card is on,
// where the split between the splash and the card panel falls, and when the
// game files are ready.  This is only the part that puts pixels on a window:
// it reads that model, draws it in the same dark theme the Avalonia launcher
// uses, and turns clicks back into model calls.
//
// It is drawn rather than built out of Win32 controls because the theme is the
// point.  A combo box is the one control the common controls will not draw
// dark, which is why the managed launcher draws its own rows too; matching it
// with a grey dialog would make the two heads look like two products.

#include "Mods/Launcher/native_launcher.hpp"

#include <filesystem>

namespace fruityprime::launcher::gui::win32 {

// Shows the front screen and blocks until it is closed.
//
// Returns true when the player asked to go on: `selection` then carries the
// ROM they picked and whatever the saved preferences held.  Returns false when
// the window was closed, which the caller treats as "quit" rather than as
// "start with nothing".
//
// On a build with no window station -- a service, a headless CI box -- this
// returns false without creating a window, so the caller falls back to its own
// path rather than hanging.
// `open_details`, when given, is set to whether the screen asked for the
// older selection dialog rather than starting from its own rows.  Every card
// is drawn here now, so nothing sets it; it stays because the dialog is still
// the fallback and "the front screen asked for it" is the only thing that
// should open it.
[[nodiscard]] bool run_front_screen(
    const std::filesystem::path& executable_directory, Selection& selection,
    bool* open_details = nullptr);

} // namespace fruityprime::launcher::gui::win32
