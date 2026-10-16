#include "Mods/Network/Gameplay/Replication/FormTransitionAuthorityTracker.hpp"
#include "Mods/Network/Gameplay/Replication/FormTransitionTimeline.hpp"
#include "Mods/Network/Protocol/Packets/PacketTypes.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace MphRead::Mods::Network;
using Direction = ReplicatedFormState::Transition;
namespace
{
    void Require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }
    FormTransitionRecord Started()
    {
        FormTransitionRecord record{};
        record.Slot = 1; record.Generation = 2; record.Life = 3; record.Id = 1;
        record.StartFrame = record.StampFrame = record.AnchorFrame = 100;
        record.ClipFrames = 16;
        record.Direction = Direction::Morph;
        return record;
    }
    void Authority()
    {
        FormTransitionAuthorityTracker tracker;
        ReplicatedFormState normal{};
        const FormClipObservation clip{0, 16, 100, false};
        Require(!tracker.Observe(99, 1, 2, 3, true, normal, clip) && !tracker.Latest(),
            "rejected requests do not create accepted events");
        normal.Direction = Direction::Morph;
        Require(tracker.Observe(100, 1, 2, 3, true, normal, clip), "accepted tick starts an event");
        Require(tracker.Latest()->StartFrame == 100 && tracker.Latest()->Id == 1, "authority tick/identity");
        Require(!tracker.Observe(101, 1, 2, 3, true, normal, clip), "same transition is not started twice");
        auto paused = clip; paused.Paused = true; paused.Frame = 4; paused.AnchorFrame = 108;
        Require(tracker.Observe(108, 1, 2, 3, true, normal, paused) && tracker.Latest()->Id == 1,
            "freeze changes playback without assigning another event ID");
        auto stable = normal; stable.Direction = Direction::None; stable.AltForm = true;
        Require(tracker.Observe(140, 1, 2, 3, true, stable, clip)
            && tracker.Latest()->Status == FormTransitionStatus::Completed, "completion recorded at its actual tick");
        normal.Direction = Direction::Unmorph;
        auto nextClip = clip; nextClip.AnchorFrame = 141;
        Require(tracker.Observe(141, 1, 2, 3, true, normal, nextClip)
            && tracker.Latest()->Id == 2 && tracker.Latest()->StartFrame == 141, "opposite direction is a new event");
        Require(tracker.Observe(145, 1, 2, 3, false, normal, nextClip)
            && tracker.Latest()->Status == FormTransitionStatus::Cancelled, "death cancels rather than completes");
        Require(!tracker.Observe(146, 1, 2, 4, true, stable, clip) && !tracker.Latest(), "respawn clears the old life");
        Require(!tracker.Observe(147, 1, 3, 1, true, stable, clip) && !tracker.Latest(), "slot reuse clears the old occupant");
    }
    void Codec()
    {
        FormTransitionPacket packet{}; packet.Match = 5; packet.Epoch = 6; packet.Frame = 101; packet.Count = NetworkSlotCapacity;
        for (std::size_t i = 0; i < packet.Count; ++i) { packet.Records[i] = Started(); packet.Records[i].Slot = static_cast<std::uint8_t>(i); }
        std::array<std::uint8_t, FormTransitionPacket::MaxSize> bytes{};
        Require(packet.Write(bytes) == bytes.size() && bytes.size() + 1 <= NetConfig::MaxPacketSize, "eight events fit the wire budget");
        FormTransitionPacket decoded{};
        Require(FormTransitionPacket::TryRead(bytes, decoded) && decoded.Records == packet.Records
            && decoded.Match == 5 && decoded.Epoch == 6, "wire round trip");
        for (std::size_t n = 0; n < bytes.size(); ++n)
            Require(!FormTransitionPacket::TryRead(std::span<const std::uint8_t>(bytes).first(n), decoded), "truncated packet rejected");
        auto invalid = bytes; invalid[14] = 9;
        Require(!FormTransitionPacket::TryRead(invalid, decoded), "oversized count rejected");
        invalid = bytes; invalid[FormTransitionPacket::HeaderSize + FormTransitionRecord::Size] = 0;
        Require(!FormTransitionPacket::TryRead(invalid, decoded), "duplicate slot rejected");
        for (const auto field : {23, 24, 25})
        {
            invalid = bytes; invalid[FormTransitionPacket::HeaderSize + field] = 255;
            Require(!FormTransitionPacket::TryRead(invalid, decoded), "invalid direction/status/flags rejected");
        }
        std::vector<std::uint8_t> trailing(bytes.begin(), bytes.end()); trailing.push_back(0);
        Require(!FormTransitionPacket::TryRead(trailing, decoded), "trailing garbage rejected");
    }
    void Playback()
    {
        FormTransitionTimeline timeline;
        auto start = Started();
        Require(timeline.Receive(start) && !timeline.Receive(start), "duplicate reception is idempotent");
        Require(!timeline.Sample(2, 3, 99.9, Direction::Morph), "no playback before accepted start");
        for (const int fps : {30, 60, 120, 144, 240, 540})
        {
            float previous = -1;
            for (int draw = 0; draw <= fps / 2; ++draw)
            {
                const double frame = 100 + 60.0 * draw / fps;
                const auto phase = timeline.Sample(2, 3, frame, Direction::Morph);
                Require(phase && *phase >= previous && std::abs(*phase - (frame - 100) / 30) < 0.00001,
                    "continuous phase has the same real-time duration at every display rate");
                previous = *phase;
            }
        }
        auto pause = start; pause.StampFrame = pause.AnchorFrame = 110; pause.ClipFrame = 5; pause.Paused = true;
        Require(timeline.Receive(pause), "freeze observation accepted");
        const auto frozen = timeline.Sample(2, 3, 119.5, Direction::Morph);
        Require(frozen && std::abs(*frozen - 1.0F / 3) < 0.00001, "freeze holds the clip while the shared clock runs");
        auto resume = pause; resume.StampFrame = resume.AnchorFrame = 120; resume.Paused = false;
        Require(timeline.Receive(resume), "thaw accepted");
        auto end = resume; end.StampFrame = 141; end.Status = FormTransitionStatus::Completed; end.AltForm = true;
        Require(timeline.Receive(end) && !timeline.Sample(2, 3, 141, Direction::Morph), "terminal event fences off replay");
        auto lateStart = start; lateStart.StampFrame = lateStart.AnchorFrame = 102; lateStart.ClipFrame = 1;
        Require(timeline.Receive(lateStart) && !timeline.Sample(2, 3, 142, Direction::Morph), "late start cannot undo completion");
        Require(timeline.Sample(2, 3, 109, Direction::Morph).has_value(), "reordered observation is usable only in its history");
        Require(!timeline.Sample(2, 4, 109, Direction::Morph) && !timeline.Sample(3, 3, 109, Direction::Morph), "life and generation fence playback");
        auto reverse = start; reverse.Id = 2; reverse.StartFrame = reverse.StampFrame = reverse.AnchorFrame = 142;
        reverse.Direction = Direction::Unmorph;
        auto invalidTime = reverse;
        invalidTime.StartFrame = invalidTime.StampFrame = invalidTime.AnchorFrame = 99;
        Require(!timeline.Receive(invalidTime), "a newer ID cannot move the accepted start backwards");
        auto invalidEnd = start; invalidEnd.Status = FormTransitionStatus::Completed;
        Require(!invalidEnd.Valid(), "completion must confirm the direction's resulting form");
        Require(timeline.Receive(reverse) && !timeline.Receive(start), "old transition ID cannot restore an old event");
        Require(!timeline.Sample(2, 3, 145, Direction::Morph), "new direction fences old clip");
        timeline.Reset(); start.Id = 65535;
        Require(timeline.Receive(start), "last ID before wrap accepted");
        reverse.Id = 1;
        Require(timeline.Receive(reverse), "ID comparison wraps and skips zero");
        timeline.Reset();
        Require(!timeline.Sample(2, 3, 110, Direction::Morph), "stream reset clears the timeline");
        Require(timeline.Receive(end) && !timeline.Sample(2, 3, 142, Direction::Morph), "late join never replays a completed start");
    }
}
int main()
{
    try { Authority(); Codec(); Playback(); std::cout << "FormTransitionEvents PASS\n"; return 0; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
