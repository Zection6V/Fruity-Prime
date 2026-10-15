#pragma once

#include <string>
#include <vector>

namespace MphRead::Mods::Entry
{
    // -mapbundle [NAME|all] [-out PATH]: cook custom maps into the .fpmap
    // they are handed out as. Answered before setup -- it needs no game
    // files -- and after -mapdir has been applied. Exit 1 if any failed.
    // True when it ran; false, having touched nothing, otherwise.
    [[nodiscard]] bool TryRunMapBundle(const std::vector<std::string>& args);

    // The custom-map commands that run once the game files are set up, in
    // ModEntry's order: -mapgen, -q3maps, -q3shaders, -mapcheck, -altprobe,
    // -portalviews, -shotat, -mphtexdump, -mapsync, -bottour, -moverprobe,
    // -padprobe, -mapitems, -mapmaterials. True when one ran (its result is
    // the process exit code); false, having touched nothing, when none was
    // asked for. The work itself is MapGen's; this reads the command line
    // and hands it over.
    //
    // -shotat takes its -size as "W,H", unlike every other command's "WxH".
    [[nodiscard]] bool TryRunMapCommands(const std::vector<std::string>& args);

    // A room loaded and driven by the map audit, in ModEntry's order:
    // -gpulifetime, -mapteststorymoviecycle, -mapteststorymovietransition,
    // -maptest (whose -size is "WxH" and, when it does not parse, leaves the
    // audit's window size alone). Same contract as above.
    [[nodiscard]] bool TryRunMapAudits(const std::vector<std::string>& args);

    // -thumbnail ROOM (as many times as given, in that order) and then
    // -thumbnails [-force] [-jobs N]: the launcher's map previews, at the
    // size TryHandle read from -size before any command ran.
    [[nodiscard]] bool TryRunThumbnails(const std::vector<std::string>& args, int width, int height);
}
