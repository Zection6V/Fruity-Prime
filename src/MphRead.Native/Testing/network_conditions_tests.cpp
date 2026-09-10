#include "Mods/Network/match_client.hpp"
#include "Mods/Network/net_lag.hpp"
#include "Mods/Network/net_transport.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

template <typename Function>
void assert_throws(Function&& function) {
    bool thrown = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    assert(thrown);
}

} // namespace

int main() {
    using fruityprime::net::NetLag;
    assert(!NetLag::active());
    assert(NetLag::configure(" 200:40:1 "));
    assert(NetLag::round_trip_ms() == 200);
    assert(NetLag::jitter_ms() == 40);
    assert(NetLag::describe()
        == "+200 ms round trip (jitter up to 40 ms each way)");
    assert(NetLag::configure_loss("2.50"));
    assert(NetLag::loss_percent() == 2.5);
    assert(NetLag::describe()
        == "+200 ms round trip (jitter up to 40 ms each way), 2.5% packet loss each way");
    assert(!NetLag::configure("not-a-number"));
    assert(NetLag::round_trip_ms() == 200);
    assert(!NetLag::configure_loss("101"));
    assert(NetLag::loss_percent() == 2.5);
    assert(NetLag::configure("0"));
    assert(NetLag::configure_loss("0"));
    assert(!NetLag::active());

    const auto real_line = fruityprime::net::NetworkConditions::from_options(
        "", "");
    assert(!real_line.active());
    assert(real_line.describe().empty());

    const auto jittered = fruityprime::net::NetworkConditions::from_options(
        "200:40", "2.50");
    assert(jittered.round_trip_ms == 200);
    assert(jittered.jitter_ms == 40);
    assert(jittered.loss_percent == 2.5);
    assert(jittered.active());
    assert(jittered.describe()
        == "+200 ms round trip (jitter up to 40 ms each way), 2.5% packet loss each way");

    const auto comma_form = fruityprime::net::NetworkConditions::from_options(
        "0,5", "100");
    assert(comma_form.round_trip_ms == 0);
    assert(comma_form.jitter_ms == 5);
    assert(comma_form.loss_percent == 100.0);
    assert(comma_form.describe()
        == "no added latency (jitter up to 5 ms each way), 100% packet loss each way");

    assert_throws([] {
        static_cast<void>(fruityprime::net::NetworkConditions::from_options(
            "200:40:1", ""));
    });
    assert_throws([] {
        static_cast<void>(fruityprime::net::NetworkConditions::from_options(
            "10001", ""));
    });
    assert_throws([] {
        static_cast<void>(fruityprime::net::NetworkConditions::from_options(
            "200", "100.1"));
    });

    fruityprime::net::SpectateSchedule schedule(0.0, 1.0);
    schedule.update(1);
    assert(schedule.spectating());
    assert(schedule.started_frame() == 1);
    const auto spectator_buttons = schedule.buttons(
        static_cast<fruityprime::net::IntentButtons>(
            static_cast<std::uint32_t>(
                fruityprime::net::IntentButtons::MoveRight)
            | static_cast<std::uint32_t>(
                fruityprime::net::IntentButtons::InPlayState)));
    assert((static_cast<std::uint32_t>(spectator_buttons)
            & static_cast<std::uint32_t>(
                fruityprime::net::IntentButtons::MoveRight)) == 0);
    assert((static_cast<std::uint32_t>(spectator_buttons)
            & static_cast<std::uint32_t>(
                fruityprime::net::IntentButtons::InPlayState)) != 0);
    assert((static_cast<std::uint32_t>(spectator_buttons)
            & static_cast<std::uint32_t>(
                fruityprime::net::IntentButtons::SpectatingState)) != 0);
    schedule.update(60);
    assert(!schedule.spectating());
    assert(schedule.rejoined_frame() == 60);
    assert(schedule.buttons(fruityprime::net::IntentButtons::MoveRight)
           == fruityprime::net::IntentButtons::MoveRight);

    std::cout << "native network condition tests passed\n";
    return 0;
}
