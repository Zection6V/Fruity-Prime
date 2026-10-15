#include "GameChecks.hpp"

#include "CommandLine.hpp"
#include "OptionValues.hpp"

#include "../../Formats/Enums.hpp"
#include "../../NativeRuntime/System/Runtime.hpp"
#include "../../Formats/Formats.hpp"
#include "../../NativeRuntime/System/Number.hpp"
#include "../Diagnostics/AimCheck.hpp"
#include "../Diagnostics/HomingContinuousCheck.hpp"
#include "../Diagnostics/HomingDisplayCheck.hpp"
#include "../Diagnostics/HomingParityCheck.hpp"
#include "../Diagnostics/HomingSpawnCheck.hpp"
#include "../Diagnostics/HostedAuthorityCheck.hpp"
#include "../Diagnostics/LockjawEnemyCheck.hpp"
#include "../Diagnostics/MovieHeadlessCheck.hpp"
#include "../Diagnostics/WeavelAltFormCheck.hpp"
#include "../Input/GamepadProbe.hpp"
#include "../Launcher/Portable/GameFiles.hpp"
#include "../Multiplayer/ResourceAudit.hpp"
#include "../Network/Diagnostics/MechanicsDump.hpp"
#include "../Network/Testing/Combat/DialancheCombatCheck.hpp"
#include "../Network/Testing/Combat/HealthSimulationTest.hpp"
#include "../Network/Testing/Combat/NetCombatCheck.hpp"
#include "../Network/Testing/Combat/SpireAltPoseCheck.hpp"
#include "../Network/Testing/Combat/WeaponDps.hpp"
#include "../Network/Testing/Lobby/NetLobbyTest.hpp"
#include "../Network/Testing/Pickups/PowerUpSimulationTest.hpp"
#include "../Network/Testing/Simulation/ServerSimCheck.hpp"
#include "../ThumbnailGenerator.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iostream>
#include <optional>
#include <string_view>

using ::MphRead::NativeRuntime::Int32TryParseCurrentCulture;

namespace
{
    void WriteLine(std::string_view value)
    {
        std::cout << value << '\n';
    }

    void SetExitCode(std::int32_t value) noexcept
    {
        MphRead::NativeRuntime::SetEnvironmentExitCode(value);
    }
}

namespace MphRead::Mods::Entry
{
    bool TryRunCatalogCommands(const std::vector<std::string>& args)
    {
        if (HasFlag(args, "mechanics"))
        {
            Network::MechanicsDump::Run();
            return true;
        }

        if (HasFlag(args, "gamepad"))
        {
            double seconds = 15;
            const std::optional<std::string> given = ValueAfter(args, "seconds");
            double parsed = 0;
            if (given.has_value() && TryParseDoubleCurrent(*given, parsed) && parsed > 0)
            {
                seconds = parsed;
            }
            SetExitCode(Input::GamepadProbe::Run(seconds, HasFlag(args, "verbose")));
            return true;
        }

        if (HasFlag(args, "resourceaudit"))
        {
            SetExitCode(Multiplayer::ResourceAudit::Run());
            return true;
        }

        if (const std::optional<std::string> healthRoom = ValueAfter(args, "healthsimtest");
            healthRoom.has_value())
        {
            SetExitCode(Network::HealthSimulationTest::Run(*healthRoom));
            return true;
        }

        if (const std::optional<std::string> powerUpRoom = ValueAfter(args, "powerupsimtest");
            powerUpRoom.has_value())
        {
            SetExitCode(Network::PowerUpSimulationTest::Run(*powerUpRoom));
            return true;
        }

        if (HasFlag(args, "netlobbytest"))
        {
            SetExitCode(Network::NetLobbyTest::Run());
            return true;
        }

        if (HasFlag(args, "rooms"))
        {
            for (const std::string& room : ThumbnailGenerator::MultiplayerRooms())
            {
                WriteLine(room);
            }
            return true;
        }

        const std::optional<std::string> dpsTest = ValueAfter(args, "dpstest");
        if (dpsTest.has_value())
        {
            Hunter dpsHunter = Hunter::Sylux;
            (void)TryParseHunter(ValueAfter(args, "hunter"), dpsHunter);
            BeamType dpsBeam = BeamType::ShockCoil;
            (void)TryParseBeam(ValueAfter(args, "weapon"), dpsBeam);
            double dpsSeconds = 10;
            double parsedSeconds = 0;
            if (TryParseDoubleInvariant(ValueAfter(args, "seconds"), parsedSeconds))
            {
                dpsSeconds = parsedSeconds;
            }
            float dpsDistance = 2.2f;
            float parsedDistance = 0;
            if (TryParseFloatInvariant(ValueAfter(args, "distance"), parsedDistance))
            {
                dpsDistance = parsedDistance;
            }
            SetExitCode(Network::WeaponDps::Run(*dpsTest, dpsHunter, dpsBeam,
                dpsSeconds, dpsDistance, HasFlag(args, "bombs")));
            return true;
        }
        return false;
    }

    bool TryRunGameChecks(const std::vector<std::string>& args)
    {
        const auto lockjawEnemyCheck = ValueAfter(args, "lockjawenemycheck");
        if (const auto aimCheck = ValueAfter(args, "aimcheck"); aimCheck.has_value())
        {
            SetExitCode(Diagnostics::AimCheck::Run(*aimCheck));
            return true;
        }
        const auto weavelAltCheck = ValueAfter(args, "weavelaltcheck");
        if (weavelAltCheck.has_value())
        {
            SetExitCode(Diagnostics::WeavelAltFormCheck::Run(*weavelAltCheck));
            return true;
        }
        if (HasFlag(args, "movieheadlesscheck"))
        {
            if (!Launcher::GameFiles::Ready())
            {
                WriteLine("[check] -movieheadlesscheck needs the game files; skipped");
                SetExitCode(77);
                return true;
            }
            SetExitCode(Diagnostics::MovieHeadlessCheck::Run());
            return true;
        }
        // The homing checks fly real projectiles in a real room: without the
        // player's game files there is nothing to load, which CTest reads as
        // a skip (77) rather than a failure.
        const auto homingCheck = [&args](std::string_view name, const std::function<std::int32_t()>& run)
        {
            if (!HasFlag(args, std::string(name)))
            {
                return false;
            }
            if (!Launcher::GameFiles::Ready())
            {
                WriteLine("[check] -" + std::string(name) + " needs the game files; skipped");
                SetExitCode(77);
                return true;
            }
            SetExitCode(run());
            return true;
        };
        const auto roomFor = [&args](const char* name)
        {
            return ValueAfter(args, name).value_or("MP3 PROVING GROUND");
        };
        if (homingCheck("homingcontinuouscheck",
                [&] { return Diagnostics::HomingContinuousCheck::Run(roomFor("homingcontinuouscheck")); })
            || homingCheck("homingdisplaycheck",
                [&] { return Diagnostics::HomingDisplayCheck::Run(roomFor("homingdisplaycheck")); })
            || homingCheck("homingparitycheck",
                [&] { return Diagnostics::HomingParityCheck::Run(roomFor("homingparitycheck")); })
            || homingCheck("netcombatcheck",
                [&] { return Network::NetCombatCheck::Run(roomFor("netcombatcheck")); })
            || homingCheck("hostauthoritycheck",
                [&] { return Diagnostics::HostedAuthorityCheck::Run(roomFor("hostauthoritycheck")); })
            || homingCheck("homingspawncheck", [&]
                {
                    std::int32_t seconds = 20;
                    if (const auto value = ValueAfter(args, "homingspawncheck"); value.has_value())
                    {
                        static_cast<void>(Int32TryParseCurrentCulture(*value, seconds));
                    }
                    return Diagnostics::HomingSpawnCheck::Run(std::clamp(seconds, 1, 600));
                }))
        {
            return true;
        }
        if (lockjawEnemyCheck.has_value())
        {
            SetExitCode(Diagnostics::LockjawEnemyCheck::Run(*lockjawEnemyCheck));
            return true;
        }

        const auto netCombatCheck = ValueAfter(args, "netcombatcheck");
        if (netCombatCheck.has_value())
        {
            SetExitCode(Network::NetCombatCheck::Run(*netCombatCheck));
            return true;
        }
        const auto dialancheCheck = ValueAfter(args, "dialanchecheck");
        const auto dialanchePeerCheck = ValueAfter(args, "dialanchepeercheck");
        if (dialanchePeerCheck.has_value())
        {
            int port = 0;
            const auto value = ValueAfter(args, "port");
            if (value.has_value()) Int32TryParseCurrentCulture(*value, port);
            SetExitCode(Network::DialancheCombatCheck::RunPeer(*dialanchePeerCheck, port));
            return true;
        }
        if (dialancheCheck.has_value())
        {
            int peerA = 0, peerB = 0;
            const auto a = ValueAfter(args, "dialanchepeera"), b = ValueAfter(args, "dialanchepeerb");
            if (a.has_value()) Int32TryParseCurrentCulture(*a, peerA);
            if (b.has_value()) Int32TryParseCurrentCulture(*b, peerB);
            SetExitCode(Network::DialancheCombatCheck::Run(*dialancheCheck, peerA, peerB));
            return true;
        }

        const std::optional<std::string> spirePoseCheck = ValueAfter(args, "spireposecheck");
        if (spirePoseCheck.has_value())
        {
            SetExitCode(Network::SpireAltPoseCheck::Run(*spirePoseCheck));
            return true;
        }

        const std::optional<std::string> simCheck = ValueAfter(args, "simcheck");
        if (simCheck.has_value())
        {
            int players = 8;
            const std::optional<std::string> playersValue = ValueAfter(args, "players");
            std::int32_t parsedPlayers = 0;
            if (playersValue.has_value() && Int32TryParseCurrentCulture(*playersValue, parsedPlayers))
            {
                players = parsedPlayers;
            }
            double seconds = 10;
            double parsedSeconds = 0;
            if (TryParseDoubleInvariant(ValueAfter(args, "seconds"), parsedSeconds))
            {
                seconds = parsedSeconds;
            }
            GameMode mode = GameMode::Battle;
            (void)TryParseGameMode(ValueAfter(args, "mode"), mode);
            SetExitCode(Network::ServerSimCheck::Run(*simCheck, players, seconds, mode, HasFlag(args, "formcheck")));
            return true;
        }
        return false;
    }
}
