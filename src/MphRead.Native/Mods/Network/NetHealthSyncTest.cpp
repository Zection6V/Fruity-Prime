#include "NetHealthSyncTest.hpp"

#include "NetHealthSync.hpp"
#include "NetMatchTimeSync.hpp"
#include "NetProtocol.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../GameState.hpp"
#include "../../NativeRuntime/System/BinaryPrimitives.hpp"
#include "../../NativeRuntime/System/Exceptions.hpp"

#include <array>
#include <limits>
#include <span>
#include <string>

namespace MphRead::Mods::Network
{
    namespace Runtime = ::MphRead::NativeRuntime;

    void NetHealthSyncTest::Run()
    {
        const auto check = [](bool condition, const std::string& message)
        {
            if (!condition)
            {
                throw System::InvalidOperationException("Health snapshot: " + message);
            }
        };
        NetHealthSync::BeginRoom();
        std::array<std::uint8_t, NetHealthSync::HeaderSize> empty{};
        check(NetHealthSync::Write(empty) == static_cast<std::int32_t>(empty.size()) && NetHealthSync::Validate(empty),
            "empty world round trip");
        check(!NetHealthSync::Validate(std::span<const std::uint8_t>(empty).first(2)), "truncated header accepted");
        std::array<std::uint8_t, NetHealthSync::HeaderSize + 2 * NetHealthSync::EntrySize> data{};
        const std::span<std::uint8_t> bytes(data);
        data[2] = 2;
        Runtime::WriteInt16LittleEndian(bytes.subspan(3), 4);
        constexpr std::int32_t pickerSlot = 4;
        data[5] = static_cast<std::uint8_t>(3 | ((pickerSlot + 1) << 2));
        Runtime::WriteUInt16LittleEndian(bytes.subspan(6), 600);
        Runtime::WriteUInt16LittleEndian(bytes.subspan(8), 8);
        Runtime::WriteInt16LittleEndian(bytes.subspan(10), 7);
        check(NetHealthSync::Validate(data), "valid state rejected");
        NetHealthSync::Receive(data);
        HealthSpawnState state{};
        check(NetHealthSync::TryGet(4, state) && state == HealthSpawnState{true, true, 600, 8, pickerSlot}, "state round trip");
        check(NetHealthSync::TryGet(7, state) && !state.Available && state.PickerSlot == -1, "unavailable pickup");
        data[5] = 0x80;
        check(!NetHealthSync::Validate(data), "reserved flags accepted");
        data[5] = static_cast<std::uint8_t>(3 | ((Entities::PlayerEntity::SlotCapacity + 1) << 2));
        check(!NetHealthSync::Validate(data), "out-of-range picker accepted");
        data[5] = static_cast<std::uint8_t>(3 | ((pickerSlot + 1) << 2));
        data[10] = 4;
        check(!NetHealthSync::Validate(data), "duplicate entity accepted");
        data[10] = 7;
        check(!NetHealthSync::Validate(std::span<const std::uint8_t>(data).first(data.size() - 1)), "truncated entry accepted");
        Runtime::WriteUInt16LittleEndian(bytes, 99);
        data[5] = 0;
        NetHealthSync::Receive(data);
        check(NetHealthSync::TryGet(4, state) && state.Available, "old match mutated health");
        check(SnapshotHeader::Size + PlayerState::Size * Entities::PlayerEntity::SlotCapacity
            + NetMatchTimeSync::Size + NetHealthSync::HeaderSize + NetHealthSync::MaxSpawns * NetHealthSync::EntrySize
            < NetConfig::MaxPacketSize, "full snapshot exceeds datagram budget");
        std::array<std::uint8_t, NetMatchTimeSync::Size> times{};
        Runtime::WriteSingleLittleEndian(std::span<std::uint8_t>(times).subspan(60), 100);
        check(NetMatchTimeSync::Validate(times), "valid objective clocks rejected");
        NetMatchTimeSync::Receive(times);
        check(GameState::TeamTime()[7] == 100, "slot-seven time not replicated");
        Runtime::WriteSingleLittleEndian(times, std::numeric_limits<float>::quiet_NaN());
        check(!NetMatchTimeSync::Validate(times), "NaN objective clock accepted");
        GameState::TeamTime()[7] = 0;
        NetHealthSync::BeginRoom();
    }
}
