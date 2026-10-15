#include "Mods/Entry/NetworkOptions.hpp"
#include "Mods/Entry/RenderOverrides.hpp"
#include "Mods/Network/Gameplay/Replication/NetSmoothing.hpp"
#include "Mods/Network/Integration/Hooks/NetHooks.hpp"
#include "Mods/Network/Testing/Combat/HitRig.hpp"
#include "Mods/Network/Transport/NetLag.hpp"
#include "Mods/Render/FrameTiming.hpp"
#include "Mods/RenderOptions.hpp"

#include <iostream>
#include <string>
#include <vector>

// The options ModEntry applies before any command runs, checked as state
// rather than as output (refactoring instructions 2026-10-15, D10/D11/D13).
// What is pinned is the *order* of application: the source order of the
// blocks decides, not the order the flags were typed in.
using namespace MphRead::Mods;
using MphRead::Mods::Render::FrameTiming;

namespace
{
    using Args = std::vector<std::string>;

    bool Check(bool condition, const char* id, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL " << id << ": " << message << '\n';
        }
        return condition;
    }

    bool RenderOverrides()
    {
        bool passed = true;

        Entry::ApplyRenderOverrides(Args{"-fpscap", "display"});
        passed &= Check(FrameTiming::VSync() && FrameTiming::VSyncForced()
            && FrameTiming::FrameRateCap() == FrameTiming::Unlimited,
            "D11a", "-fpscap display forces VSync on and leaves the cap unlimited");

        Entry::ApplyRenderOverrides(Args{"-fpscap", "display", "-vsync", "off"});
        passed &= Check(!FrameTiming::VSync() && FrameTiming::FrameRateCap() == FrameTiming::Unlimited,
            "D11b", "-vsync is applied after -fpscap display and wins");

        Entry::ApplyRenderOverrides(Args{"-vsync", "off", "-fpscap", "display"});
        passed &= Check(!FrameTiming::VSync(),
            "D11c", "the order of the blocks decides, not the order typed");

        Entry::ApplyRenderOverrides(Args{"-fpscap", "120", "-vsync", "on"});
        passed &= Check(FrameTiming::VSync() && FrameTiming::FrameRateCap() == 120,
            "D11d", "a numeric cap is kept and -vsync on is honoured");

        RenderOptions::CelShading(false);
        Entry::ApplyRenderOverrides(Args{"-cel", "-fog"});
        passed &= Check(RenderOptions::CelShading(),
            "D11e", "-cel followed by another flag still turns cel shading on");

        RenderOptions::CelShading(true);
        Entry::ApplyRenderOverrides(Args{"-cel", "off"});
        passed &= Check(!RenderOptions::CelShading(), "D11f", "-cel off turns it off");
        return passed;
    }

    bool LineSimulation()
    {
        bool passed = true;
        passed &= Check(Entry::ConfigureLineSimulation(Args{"-credits"}) && !Network::NetLag::Active(),
            "D13a", "no line options: nothing simulated, the command goes on");
        passed &= Check(!Entry::ConfigureLineSimulation(Args{"-netlag", "INVALID"}),
            "D13b", "a refused -netlag ends the command");
        passed &= Check(!Entry::ConfigureLineSimulation(Args{"-netjitter", "x"}),
            "D13c", "a refused -netjitter ends the command");
        passed &= Check(Entry::ConfigureLineSimulation(Args{"-netlag", "200:40"}) && Network::NetLag::Active(),
            "D13d", "a valid -netlag is applied and the command goes on");
        return passed;
    }

    bool HitRigOptions()
    {
        bool passed = true;
        Entry::ApplyHitRigOptions(Args{"-hitrig", "bogus"});
        passed &= Check(!Network::HitRig::Active(), "NET-rig-a", "a refused -hitrig leaves the rig off");
        Entry::ApplyHitRigOptions(Args{"-hitrig", "sniper"});
        passed &= Check(Network::HitRig::Active(), "NET-rig-b", "-hitrig sniper turns the rig on");
        return passed;
    }

    bool PuppetSources()
    {
        bool passed = true;
        Network::NetSmoothing::Enabled(true);
        Network::NetHooks::SnapshotOwnsPuppets(false);
        Entry::ApplyLagCompensationOptions(Args{"-snapshotpuppets"});
        passed &= Check(Network::NetHooks::SnapshotOwnsPuppets() && Network::NetSmoothing::Enabled(),
            "D10a", "-snapshotpuppets hands puppets to the snapshot and leaves smoothing on");

        Entry::ApplyHitResolutionOptions(Args{"-relayedpuppets", "-nointerp"});
        passed &= Check(!Network::NetHooks::SnapshotOwnsPuppets() && !Network::NetSmoothing::Enabled(),
            "D10b", "-nointerp with -relayedpuppets: relayed positions, smoothing off");

        Network::NetSmoothing::Enabled(true);
        Network::NetHooks::SnapshotOwnsPuppets(true);
        Entry::ApplyHitResolutionOptions(Args{"-relayedpuppets"});
        passed &= Check(!Network::NetHooks::SnapshotOwnsPuppets() && !Network::NetSmoothing::Enabled(),
            "D10c", "-relayedpuppets alone also turns smoothing off");
        return passed;
    }
}

int main()
{
    bool passed = true;
    passed &= RenderOverrides();
    passed &= LineSimulation();
    passed &= HitRigOptions();
    passed &= PuppetSources();
    if (!passed)
    {
        return 1;
    }
    std::cout << "ModEntry option application: all cases pass\n";
    return 0;
}
