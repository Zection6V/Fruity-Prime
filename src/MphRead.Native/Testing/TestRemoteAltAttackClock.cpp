#include "Mods/Network/Gameplay/Player/RemoteAltAttackClock.hpp"

#include <iostream>
#include <stdexcept>

using namespace MphRead::Mods::Network;

namespace
{
    void Require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    RemoteAltAttackClock::Sample Held(std::uint32_t frame, std::uint32_t local, bool pressed = false,
        std::int32_t age = 0, std::uint16_t life = 1)
    {
        RemoteAltAttackClock::Sample sample{};
        sample.Generation = 1;
        sample.Life = life;
        sample.IntentFrame = frame;
        sample.LocalFrame = local;
        sample.Pressed = pressed;
        sample.PressAge = age;
        sample.Held = true;
        return sample;
    }

    void Run()
    {
        using C = RemoteAltAttackClock;
        C::Reset();
        // Local frames run 500.. while the owner's run 100..; only differences matter.
        Require(!C::Observe(0, Held(100, 500)), "a hold whose press was never seen is not counted");
        Require(C::Observe(0, Held(103, 501, true, 3)) == 3U,
            "a press arriving three frames late is counted from the owner's press frame");
        Require(C::Observe(0, Held(103, 502)) == 4U && C::Observe(0, Held(103, 505)) == 7U,
            "between intents the owner's clock runs on a step a step from the intent's first application");
        Require(C::Observe(0, Held(108, 506)) == 8U, "the next intent re-anchors on the owner's frame");
        Require(C::Observe(0, Held(108, 506)) == 8U, "applying the same intent twice in a step adds nothing");
        auto released = Held(109, 507);
        released.Held = false;
        Require(!C::Observe(0, released) && !C::Observe(0, Held(110, 508)), "a release ends the hold");
        Require(C::Observe(1, Held(50, 600, true, 0)) == 0U && !C::Observe(0, Held(110, 600)),
            "slots are independent");
        Require(!C::Observe(1, Held(51, 601, false, 0, 2)), "a new life never inherits a hold");
        Require(C::Observe(2, Held(60, 700, true, 2)) == 2U, "start of forget case");
        C::ForgetSlot(2);
        Require(!C::Observe(2, Held(61, 701)), "forgetting a slot drops its hold");
        Require(C::Observe(3, Held(5, 800, true, 7)) == 7U && C::Observe(3, Held(0xFFFFFFF0U, 801)) == 0U,
            "a wrapped, impossible difference is no time held");
        Require(!C::Observe(-1, Held(1, 0, true)) && !C::Observe(8, Held(1, 0, true)), "out-of-range slots");
        C::Reset();
    }
}

int main()
{
    try { Run(); std::cout << "RemoteAltAttackClock PASS\n"; return 0; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
