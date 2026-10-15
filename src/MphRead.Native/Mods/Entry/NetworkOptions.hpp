#pragma once

#include <string>
#include <vector>

// The network switches a command line sets for one run, before any command
// starts. Each is a process-wide setting with a line of its own on stdout,
// and ModEntry applies them in a fixed order; the functions below are that
// order cut into subsystems, and are called one after the other.
namespace MphRead::Mods::Entry
{
    // -netlag, -netspike, -netloss, then -netjitter, -netseed, -netreorder and
    // -netduplicate: the line this client makes up. False when one of them
    // was refused -- the refusal is printed and the command is over, with
    // the exit code left as it was. True otherwise, after saying what line
    // is being simulated, if any.
    [[nodiscard]] bool ConfigureLineSimulation(const std::vector<std::string>& args);

    // -homingtrace, -nounlagged, -snapshotpuppets, -clientpin, -pressage:
    // the homing trace, the rewind and where puppet positions come from.
    void ApplyLagCompensationOptions(const std::vector<std::string>& args);

    // -hitrig, -rigmorph, -rigspeed, -rigrange, -rigstrafe: the scripted
    // clients' hit rig. A refused value is said and ignored; it does not
    // end the command.
    void ApplyHitRigOptions(const std::vector<std::string>& args);

    // -maxrewind, -nohitprediction, -nohitmarker, -hitlog, -servershots,
    // -noobservedimpacts, -noconfirmedimpacts, -noretarget,
    // -nodeathprediction, -noclaims, -nointerp, -relayedpuppets: how a hit
    // is resolved, predicted, claimed and drawn. -relayedpuppets comes last
    // and also turns interpolation off, whatever came before it.
    void ApplyHitResolutionOptions(const std::vector<std::string>& args);
}
