// The Morph Ball touch block of IntentPacket: what an owner reports so the
// authority can run the same touch roll and touch/shoulder boost branches.
#include "../Mods/Network/NetProtocol.hpp"

#include <cstdio>
#include <stdexcept>
#include <vector>

using MphRead::Mods::Network::IntentPacket;

namespace
{
    void Expect(bool value, const char* reason)
    {
        if (!value) throw std::runtime_error(reason);
    }
}

int main()
{
    try
    {
        IntentPacket touch{};
        touch.ChargeLevel = 7;
        touch.BoostDamage = 30;
        touch.TouchFlags = IntentPacket::TouchPresent | IntentPacket::TouchDown | IntentPacket::TouchContinued;
        touch.TouchDelta4X = -300;
        touch.TouchDelta4Y = 91;
        std::vector<std::uint8_t> bytes(IntentPacket::FullSize);
        touch.Write(bytes);
        const IntentPacket full = IntentPacket::Read(bytes);
        Expect(full.HasState && full.ChargeLevel == 7 && full.BoostDamage == 30, "the state block survives");
        Expect(full.HasTouch() && full.TouchFlags == touch.TouchFlags
            && full.TouchDelta4X == -300 && full.TouchDelta4Y == 91, "the touch block round-trips, signed");

        // What a demo recorded before the touch block holds: the state block
        // with its fourth byte zero, and nothing after it.
        bytes.resize(static_cast<std::size_t>(IntentPacket::Size + IntentPacket::StateSize));
        bytes[static_cast<std::size_t>(IntentPacket::Size + 3)] = 0;
        const IntentPacket older = IntentPacket::Read(bytes);
        Expect(older.HasState && older.ChargeLevel == 7, "a state-only payload still carries the state");
        Expect(!older.HasTouch() && older.TouchFlags == 0 && older.TouchDelta4X == 0 && older.TouchDelta4Y == 0,
            "and reads as no contact");

        bytes.resize(static_cast<std::size_t>(IntentPacket::Size));
        const IntentPacket bare = IntentPacket::Read(bytes);
        Expect(!bare.HasState && !bare.HasTouch(), "a bare intent has neither block");
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "IntentTouchPayload: %s\n", e.what());
        return 1;
    }
    std::puts("IntentTouchPayload: ok");
    return 0;
}
