#include "SpireAltPoseCheck.hpp"

#include "NetProtocol.hpp"
#include "NetSession.hpp"
#include "ServerSim.hpp"
#include "../Headless.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../NativeRuntime/System/Console.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../NativeRuntime/System/Number.hpp"

#include <algorithm>
#include <memory>
#include <vector>

namespace MphRead::Mods::Network
{
    namespace Runtime = ::MphRead::NativeRuntime;
    using OpenTK::Mathematics::Vector3;

    std::int32_t SpireAltPoseCheck::Run(const std::string& room)
    {
        std::string reason;
        if (!ServerSim::Available(reason))
        {
            Runtime::ConsoleWriteLine("SPIREPOSE FAIL " + reason);
            return 1;
        }
        ServerSim sim{};
        if (!sim.Start(room, GameMode::Battle, 2, [](std::span<const std::uint8_t>) {}, []() {}))
        {
            return 1;
        }
        std::int32_t result = 1;
        try
        {
            RosterPacket roster = RosterPacket::Create();
            (*roster.Slots)[0] = 0;
            (*roster.Hunters)[0] = static_cast<std::uint8_t>(Hunter::Spire);
            (*roster.Colors)[0] = 0;
            (*roster.Pings)[0] = 0;
            (*roster.Names)[0] = "SPIREPOSE";
            roster.Count = 1;
            NetSession::ApplyRoster(roster);

            bool morphSent = false;
            bool attackSent = false;
            std::int32_t activeFrames = 0;
            std::int32_t movingFramesLeft = 0;
            std::int32_t movingFramesRight = 0;
            float maxLeftOffset = 0;
            float maxRightOffset = 0;
            float maxLeftChange = 0;
            float maxRightChange = 0;
            Vector3 firstLeft = Vector3::Zero;
            Vector3 firstRight = Vector3::Zero;
            Vector3 previousLeft = Vector3::Zero;
            Vector3 previousRight = Vector3::Zero;
            Entities::PlayerEntity& player = Runtime::RequireReference(Entities::PlayerEntity::Players()[0]);
            const auto spawned = [&player]() { return ::MphRead::TestFlag(player.LoadFlags(), Entities::LoadFlags::Spawned); };
            bool failed = false;
            for (std::uint32_t frame = 1; frame <= 240; frame++)
            {
                IntentButtons buttons = spawned() && player.Health() > 0 ? IntentButtons::InPlayState : IntentButtons::None;
                if (morphSent || player.IsAltForm())
                {
                    buttons |= IntentButtons::AltFormState;
                }
                if (!morphSent && frame >= 30 && spawned())
                {
                    buttons |= IntentButtons::Morph;
                    morphSent = true;
                }
                else if (morphSent && !attackSent && player.IsAltForm() && !player.IsMorphing())
                {
                    buttons |= IntentButtons::AltAttack;
                    attackSent = true;
                }
                auto presses = std::make_shared<std::vector<std::uint32_t>>(IntentPacket::PressHistory);
                (*presses)[0] = static_cast<std::uint32_t>(buttons & (IntentButtons::Morph | IntentButtons::AltAttack));
                IntentPacket intent{};
                intent.Frame = frame;
                intent.Buttons = buttons;
                intent.Presses = presses;
                intent.Aim = Vector3(0.0F, 0.0F, 1.0F);
                intent.Position = player.Position;
                intent.WeaponSelect = 0xFF;
                intent.AmmoUa = 400;
                intent.AmmoMissiles = 50;
                NetSession::AcceptSlotIntent(0, intent);
                sim.Step();
                if (sim.StepFailures() != 0)
                {
                    Runtime::ConsoleWriteLine("SPIREPOSE FAIL simulation step threw");
                    failed = true;
                    break;
                }
                if (!::MphRead::TestFlag(player.Flags2(), Entities::PlayerFlags2::AltAttack))
                {
                    continue;
                }
                const auto [left, right] = player.ModSpireAltCollisionPose();
                const Vector3 position = player.Position;
                const Vector3 localLeft = left - position;
                const Vector3 localRight = right - position;
                if (activeFrames == 0)
                {
                    firstLeft = localLeft;
                    firstRight = localRight;
                }
                else
                {
                    if (OpenTK::Mathematics::Length(localLeft - previousLeft) > 0.001F)
                    {
                        movingFramesLeft++;
                    }
                    if (OpenTK::Mathematics::Length(localRight - previousRight) > 0.001F)
                    {
                        movingFramesRight++;
                    }
                }
                previousLeft = localLeft;
                previousRight = localRight;
                activeFrames++;
                maxLeftOffset = std::max(maxLeftOffset, OpenTK::Mathematics::Length(localLeft));
                maxRightOffset = std::max(maxRightOffset, OpenTK::Mathematics::Length(localRight));
                maxLeftChange = std::max(maxLeftChange, OpenTK::Mathematics::Length(localLeft - firstLeft));
                maxRightChange = std::max(maxRightChange, OpenTK::Mathematics::Length(localRight - firstRight));
            }
            if (!failed)
            {
                const bool ok = Mods::Headless::Active() && morphSent && player.IsAltForm() && attackSent
                    && activeFrames >= 3 && movingFramesLeft >= 2 && movingFramesRight >= 2
                    && maxLeftOffset > 0.1F && maxRightOffset > 0.1F
                    && maxLeftChange > 0.05F && maxRightChange > 0.05F;
                const auto text = [](bool value) { return value ? std::string("True") : std::string("False"); };
                Runtime::ConsoleWriteLine("SPIREPOSE " + std::string(ok ? "ok" : "FAIL") + " " + room
                    + " | headless " + text(Mods::Headless::Active()) + " | spawned " + text(spawned())
                    + " | morph sent " + text(morphSent) + " | morphed " + text(player.IsAltForm())
                    + " | attack sent " + text(attackSent) + " | active frames " + std::to_string(activeFrames)
                    + " | moving L/R " + std::to_string(movingFramesLeft) + "/" + std::to_string(movingFramesRight)
                    + " | offset L/R " + Runtime::ToString(maxLeftOffset, "0.000") + "/" + Runtime::ToString(maxRightOffset, "0.000")
                    + " | change L/R " + Runtime::ToString(maxLeftChange, "0.000") + "/" + Runtime::ToString(maxRightChange, "0.000"));
                result = ok ? 0 : 1;
            }
        }
        catch (...)
        {
            sim.Stop();
            throw;
        }
        sim.Stop();
        return result;
    }
}
