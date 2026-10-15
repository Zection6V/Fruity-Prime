#pragma once

#include <string>
#include <vector>

namespace MphRead::Mods::Entry
{
    // The render options a command line may set for one run: -rhi,
    // -vkvalidation, -cel, -performance, -fog, -fov, -fps, -fpsmeasure,
    // -fpscap, -vsync, -celbands, -celedge, -prohud, -weaponstyle,
    // -crosshair, -crosshairsize and the -radar trio, applied in that order.
    // The order is part of it: "-fpscap display" forces VSync on and the
    // -vsync after it may turn it off again.
    //
    // ModEntry::TryHandleHeadless calls this twice: first so the diagnostics
    // that answer before any setting loads see what was typed, then after
    // the settings have loaded, where it has been since the port (between
    // the two, LauncherPrefs::Load requests the saved renderer). Nobody has
    // shown the second call to be redundant; keep both.
    void ApplyRenderOverrides(const std::vector<std::string>& args);
}
