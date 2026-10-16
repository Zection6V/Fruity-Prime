#include "NetworkOptions.hpp"

#include "CommandLine.hpp"

#include "../Network/Diagnostics/HomingShotTrace.hpp"
#include "../Network/Diagnostics/FormRequestTrace.hpp"
#include "../Network/Gameplay/Claims/Hit/NetHitClaims.hpp"
#include "../Network/Gameplay/Combat/HitLocation.hpp"
#include "../Network/Gameplay/Combat/NetHitPrediction.hpp"
#include "../Network/Gameplay/Combat/NetUnlagged.hpp"
#include "../Network/Gameplay/Player/NetPlayerBridge.hpp"
#include "../Network/Gameplay/Replication/NetSmoothing.hpp"
#include "../Network/Session/SessionFormTransition.hpp"
#include "../Network/Integration/Hooks/NetHooks.hpp"
#include "../Network/Testing/Combat/HitRig.hpp"
#include "../Network/Transport/NetLag.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string_view>
#include <utility>

namespace
{
    void WriteLine(std::string_view value)
    {
        std::cout << value << '\n';
    }
}

namespace MphRead::Mods::Entry
{
    bool ConfigureLineSimulation(const std::vector<std::string>& args)
    {
        const std::optional<std::string> netLag = ValueAfter(args, "netlag");
        if (netLag.has_value() && !Network::NetLag::Configure(*netLag))
        {
            WriteLine("[net] -netlag " + *netLag
                + " is not a number of milliseconds (try -netlag 200 or -netlag 200:40)");
            return false;
        }
        const std::optional<std::string> netSpike = ValueAfter(args, "netspike");
        if (netSpike.has_value() && !Network::NetLag::ConfigureSpikes(*netSpike))
        {
            WriteLine("[net] -netspike " + *netSpike + " is not SPIKES_PER_MINUTE:MIN_MS-MAX_MS (try -netspike 6:50-500)");
            return false;
        }
        const std::optional<std::string> netLoss = ValueAfter(args, "netloss");
        if (netLoss.has_value() && !Network::NetLag::ConfigureLoss(*netLoss))
        {
            WriteLine("[net] -netloss " + *netLoss + " is not a percentage");
            return false;
        }
        const std::array<std::pair<std::string_view,
            bool (*)(const std::optional<std::string>&)>, 4> netLagOptions{{
            {"netjitter", &Network::NetLag::ConfigureJitter},
            {"netseed", &Network::NetLag::ConfigureSeed},
            {"netreorder", &Network::NetLag::ConfigureReorder},
            {"netduplicate", &Network::NetLag::ConfigureDuplicate}
        }};
        for (const auto& [name, configure] : netLagOptions)
        {
            const std::optional<std::string> value = ValueAfter(args, name);
            if (value.has_value() && !configure(value))
            {
                WriteLine("[net] invalid -" + std::string(name) + " value: " + *value);
                return false;
            }
        }
        if (Network::NetLag::Active())
        {
            const std::optional<std::string> description = Network::NetLag::Describe();
            WriteLine("[net] simulating a bad line: " + description.value_or(""));
        }
        return true;
    }

    void ApplyLagCompensationOptions(const std::vector<std::string>& args)
    {
        if (HasFlag(args, "formtrace"))
        {
            Network::FormTransitionTrace::Enabled(true);
            WriteLine("[net] form trace on: accepted starts and playout samples go to the network log");
        }
        if (HasFlag(args, "homingtrace"))
        {
            Network::HomingShotTrace::Enable(true);
            WriteLine("[net] homing trace on: every projectile, claim and verdict goes to the debug log");
        }
        if (HasFlag(args, "nounlagged"))
        {
            Network::NetUnlagged::SetEnabled(false);
            WriteLine("[net] lag compensation off: shots resolve against the present");
        }
        if (HasFlag(args, "snapshotpuppets"))
        {
            Network::NetHooks::SnapshotOwnsPuppets(true);
            WriteLine("[net] puppet positions on this client come from the authority's snapshot, not from relayed intents");
        }
        if (HasFlag(args, "clientpin"))
        {
            Network::NetHooks::PinPuppetsOnClients(true);
            WriteLine("[net] puppets are pinned to their owner's reported position on clients as well as on the authority");
        }
        if (HasFlag(args, "pressage"))
        {
            Network::NetUnlagged::PressAgeEnabled(true);
            WriteLine("[net] recovered trigger pulls are rewound by their own age as well as by their packet's ack");
        }
    }

    void ApplyHitRigOptions(const std::vector<std::string>& args)
    {
        const std::optional<std::string> rig = ValueAfter(args, "hitrig");
        if (rig.has_value())
        {
            if (Network::HitRig::Configure(rig))
            {
                WriteLine("[net] hit rig: " + Network::ToString(Network::HitRig::Mode()));
            }
            else
            {
                WriteLine("[net] -hitrig " + *rig + " refused: jump, sniper, duel, dialanche, all, wells, lanes, strafe, homing or a weapon name");
            }
        }
        if (HasFlag(args, "rigmorph"))
        {
            Network::HitRig::ConfigureMorph(true);
            WriteLine("[net] hit rig runners roll into their alt form and out again");
        }
        if (const std::optional<std::string> speed = ValueAfter(args, "rigspeed"); speed.has_value())
        {
            WriteLine(Network::HitRig::ConfigureSpeed(speed)
                ? "[net] hit rig strafes x" + *speed + " faster"
                : "[net] -rigspeed " + *speed + " refused: 1 to 3");
        }
        if (const std::optional<std::string> range = ValueAfter(args, "rigrange"); range.has_value())
        {
            WriteLine(Network::HitRig::ConfigureRange(range)
                ? "[net] hit rig pairs stand " + *range + " units apart"
                : "[net] -rigrange " + *range + " refused: 8 to 40 units");
        }
        if (const std::optional<std::string> period = ValueAfter(args, "rigstrafe"); period.has_value())
        {
            WriteLine(Network::HitRig::ConfigureStrafePeriod(period)
                ? "[net] hit rig turns round every " + *period + " frames"
                : "[net] -rigstrafe " + *period + " refused: 8 to 240 frames");
        }
    }

    void ApplyHitResolutionOptions(const std::vector<std::string>& args)
    {
        const std::optional<std::string> maxRewind = ValueAfter(args, "maxrewind");
        if (maxRewind.has_value())
        {
            if (Network::NetUnlagged::ConfigureMaxRewind(maxRewind))
            {
                const std::int32_t frames = Network::NetUnlagged::MaxRewindFrames();
                WriteLine("[net] rewind ceiling " + std::to_string(frames) + " frames ("
                    + std::to_string(frames * 1000 / 60) + " ms)");
            }
            else
            {
                WriteLine("[net] -maxrewind " + *maxRewind + " refused: 1 to "
                    + std::to_string(Network::NetUnlagged::MaxRewindCeiling)
                    + " frames, and the history cannot serve more");
            }
        }
        if (HasFlag(args, "nohitprediction"))
        {
            Network::NetHitPrediction::SetEnabled(false);
            WriteLine("[net] hit prediction off: a client's hits land when the authority says so");
        }
        if (HasFlag(args, "nohitmarker"))
        {
            Network::NetHitPrediction::SetMarkerEnabled(false);
            WriteLine("[hud] hit marker off");
        }
        if (const std::optional<std::string> hitlog = ValueAfter(args, "hitlog"); hitlog.has_value())
        {
            Network::HitLocation::Open(*hitlog);
        }
        if (HasFlag(args, "servershots"))
        {
            Network::NetHitClaims::ShooterHits(false);
            WriteLine("[net] the authority resolves remote players' hits itself; claims only rescue");
        }
        if (HasFlag(args, "noobservedimpacts"))
        {
            Network::NetPlayerBridge::ObservedImpacts(false);
            WriteLine("[net] shots between two other players are drawn as they fly");
        }
        if (HasFlag(args, "noconfirmedimpacts"))
        {
            Network::NetPlayerBridge::ConfirmedImpacts(false);
            WriteLine("[net] remote shots are drawn as they fly; damage does not bring the shot in");
        }
        if (HasFlag(args, "noretarget"))
        {
            Network::NetPlayerBridge::RetargetEnabled(false);
            WriteLine("[net] remote shots are drawn with their shooter's own aim");
        }
        if (HasFlag(args, "nodeathprediction"))
        {
            Network::NetHitPrediction::SetDeathEnabled(false);
            WriteLine("[net] remote death waits for authority; self-death remains predicted");
        }
        if (HasFlag(args, "noclaims"))
        {
            Network::NetHitClaims::Enabled(false);
            WriteLine("[net] hit claims off: a shot counts only where the authority finds it itself");
        }
        if (HasFlag(args, "nointerp"))
        {
            Network::NetSmoothing::Enabled(false);
            WriteLine("[net] puppet interpolation off: remote players move when their snapshots arrive");
        }
        if (HasFlag(args, "noformevents"))
        {
            Network::SessionFormTransition::Enabled(false);
            WriteLine("[net] remote form clips use snapshot progress without the event timeline");
        }
        if (HasFlag(args, "relayedpuppets"))
        {
            Network::NetHooks::SnapshotOwnsPuppets(false);
            Network::NetSmoothing::Enabled(false);
            WriteLine("[net] puppet positions on this client come from relayed intents, not from the snapshot");
        }
    }
}
