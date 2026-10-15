#pragma once

#include <string>
#include <vector>

namespace MphRead::Mods::Entry
{
    // The catalogue and the simulation tests that run once the game files
    // are set up, in ModEntry's order: -mechanics, -gamepad, -resourceaudit,
    // -healthsimtest, -powerupsimtest, -netlobbytest, -rooms, -dpstest. True
    // when one ran; false, having touched nothing, when none was asked for.
    //
    // -gamepad is answered before setup too, and through Program::Run that
    // one always wins; this copy is kept while TryHandle can still be called
    // on its own.
    [[nodiscard]] bool TryRunCatalogCommands(const std::vector<std::string>& args);

    // The checks that play the game in a loaded room, in ModEntry's order:
    // -aimcheck, -weavelaltcheck, -movieheadlesscheck, the homing checks
    // (-homingcontinuouscheck, -homingdisplaycheck, -homingparitycheck,
    // -netcombatcheck, -hostauthoritycheck, -homingspawncheck),
    // -lockjawenemycheck, -dialanchepeercheck, -dialanchecheck,
    // -spireposecheck, -simcheck. Same contract as above.
    //
    // -movieheadlesscheck and the homing checks exit 77 -- CTest's skip --
    // when the game files are not ready; the others do not ask. The second
    // -netcombatcheck branch, after -lockjawenemycheck, cannot be reached
    // (the homing checks answer every input it would) and is kept until
    // that is settled on its own.
    [[nodiscard]] bool TryRunGameChecks(const std::vector<std::string>& args);
}
