#include "Mods/Network/Replay/DemoFrameClock.hpp"
#include "Mods/Network/Replay/DemoEndFrame.hpp"
#include "Mods/Network/Replay/DemoMatchBoundary.hpp"

#include <iostream>
#include <limits>
#include <span>
#include <vector>

namespace
{
    using MphRead::Mods::Network::DemoFrameClock;
    using MphRead::Mods::Network::DemoEndFrame;
    using MphRead::Mods::Network::DemoMatchBoundary;

    bool Check(bool condition, const char* name)
    {
        if (!condition)
        {
            std::cerr << "FAIL " << name << '\n';
            return false;
        }
        return true;
    }
}

int main()
{
    DemoFrameClock clock;
    bool passed = true;
    DemoMatchBoundary boundary;
    passed &= Check(!boundary.Observe(1, 50) && boundary.HasIdentity(),
        "initial match identity starts the first file");
    passed &= Check(!boundary.Observe(1, 50) && !boundary.Observe(0, 0),
        "unchanged or invalid identity does not split files");
    passed &= Check(boundary.Observe(2, 50) && boundary.MatchId() == 2,
        "same-map rematch starts a separate demo file");
    passed &= Check(!boundary.Observe(1, 50) && !boundary.Observe(2, 49),
        "stale delayed control packets do not split a newer match");
    passed &= Check(boundary.Observe(1, 51),
        "authority epoch rollover identifies a new recording");
    boundary.Reset();
    passed &= Check(!boundary.HasIdentity() && !boundary.Observe(9, 55),
        "explicit demo stop/new demo resets match identity");

    // Check the actual binary control packets, not just synthetic IDs.
    MphRead::Mods::Network::MatchStatePacket match{};
    match.MatchId = 9;
    match.AuthorityEpoch = 55;
    match.RoomKey = "MP1 BATTLE";
    std::vector<std::uint8_t> bytes(1 + MphRead::Mods::Network::MatchStatePacket::Size);
    bytes[0] = static_cast<std::uint8_t>(MphRead::Mods::Network::PacketType::MatchState);
    match.Write(std::span<std::uint8_t>(bytes).subspan(1));
    auto decoded = DemoMatchBoundary::Decode(bytes);
    passed &= Check(decoded && decoded->MatchId == 9 && decoded->Epoch == 55
        && decoded->Room == "MP1 BATTLE", "decode real MatchState control packet");
    match.MatchId = 10;
    match.Write(std::span<std::uint8_t>(bytes).subspan(1));
    decoded = DemoMatchBoundary::Decode(bytes);
    passed &= Check(decoded && boundary.Observe(decoded->MatchId, decoded->Epoch),
        "a new match on the same map splits the demo");

    MphRead::Mods::Network::SessionStatePacket session{};
    session.MatchId = 11;
    session.AuthorityEpoch = 55;
    session.Match.RoomKey = "MP2 BATTLE";
    bytes.assign(1 + MphRead::Mods::Network::SessionStatePacket::Size, 0);
    bytes[0] = static_cast<std::uint8_t>(MphRead::Mods::Network::PacketType::SessionState);
    session.Write(std::span<std::uint8_t>(bytes).subspan(1));
    decoded = DemoMatchBoundary::Decode(bytes);
    passed &= Check(decoded && decoded->MatchId == 11 && decoded->Room == "MP2 BATTLE",
        "detect rotation before its first MatchState from SessionState");
    bytes.assign(1, static_cast<std::uint8_t>(MphRead::Mods::Network::PacketType::SessionState));
    passed &= Check(!DemoMatchBoundary::Decode(bytes),
        "truncated session packet must not invent a match boundary");

    // EOF must be a still picture after the final packet has been applied,
    // not an extra simulated frame or a restart from the beginning.
    DemoEndFrame endFrame;
    passed &= Check(!endFrame.Held(), "a newly joined demo is running");
    endFrame.AfterWorldStep(true, false);
    passed &= Check(!endFrame.Held(), "playback continues while records remain");
    endFrame.AfterWorldStep(false, true);
    passed &= Check(!endFrame.Held(), "an inactive session cannot freeze the next scene");
    endFrame.AfterWorldStep(true, true);
    passed &= Check(endFrame.Held(), "the final recorded world step freezes the picture");
    endFrame.AfterWorldStep(true, false);
    passed &= Check(endFrame.Held(), "EOF is terminal, not a loop on the next step");
    endFrame.Reset();
    passed &= Check(!endFrame.Held(), "a new playback or rewind clears the frozen state");

    passed &= Check(clock.FrameAt(1'000'000) == 0,
        "an unstarted clock has no recording frame");
    clock.Start(1'000'000, 1'000'000);
    passed &= Check(clock.FrameAt(999'999) == 0 && clock.FrameAt(1'000'000) == 0,
        "timestamps before or at the origin map to frame zero");
    passed &= Check(clock.FrameAt(1'016'666) == 0 && clock.FrameAt(1'016'667) == 1,
        "frame boundaries use integer 60 Hz conversion");
    passed &= Check(clock.FrameAt(2'000'000) == 60 && clock.FrameAt(51'000'000) == 3000,
        "elapsed monotonic time maps to the corresponding demo frame");

    clock.Start(0, 0);
    passed &= Check(clock.FrameAt(500'000) == 0,
        "a zero timestamp frequency disables conversion safely");
    clock.Start(0, 1);
    passed &= Check(clock.FrameAt(std::numeric_limits<std::int64_t>::max())
        == std::numeric_limits<std::uint32_t>::max(),
        "very long recordings saturate instead of overflowing the frame counter");
    clock.Reset();
    passed &= Check(clock.FrameAt(2'000'000) == 0,
        "reset disables the old recording timeline");
    passed &= Check(!clock.HasAuthorityFrame(), "reset clears the authority anchor");

    clock.Start(100, 60);
    passed &= Check(!clock.HasAuthorityFrame(), "starting a recording clears the authority anchor");
    clock.AnchorAuthorityFrame(1000, 7);
    passed &= Check(clock.HasAuthorityFrame() && clock.FrameAtAuthority(1000, 0) == 7
        && clock.FrameAtAuthority(1001, 0) == 8 && clock.FrameAtAuthority(1050, 0) == 57,
        "authority ticks retain their exact intervals on the demo timeline");
    passed &= Check(clock.FrameAtAuthority(999, 20) == 20,
        "an older reordered snapshot uses its local arrival time safely");
    clock.AnchorAuthorityFrame(2000, 20);
    passed &= Check(clock.FrameAtAuthority(2000, 0) == 20,
        "a new stream can explicitly replace the timeline anchor");
    clock.AnchorAuthorityFrame(std::numeric_limits<std::uint32_t>::max(), 100);
    passed &= Check(clock.FrameAtAuthority(0, 0) == 101,
        "authority frame wrap preserves the next tick");
    clock.Reset();
    passed &= Check(!clock.HasAuthorityFrame() && clock.FrameAtAuthority(2001, 12) == 12,
        "reset clears both local and authority clocks");

    if (!passed)
    {
        return 1;
    }
    std::cout << "DemoFrameClock PASS\n";
    return 0;
}
