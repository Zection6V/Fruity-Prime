#include "Mods/Network/Protocol/Packets/FormTransitionPacket.hpp"
#include "Mods/Network/Protocol/Packets/PacketTypes.hpp"
#include "Mods/Network/Protocol/Codec/WirePrimitives.hpp"

namespace MphRead::Mods::Network
{
    using namespace Protocol::Wire;
    static_assert(FormTransitionPacket::MaxSize + 1 <= NetConfig::MaxPacketSize);

    bool FormTransitionRecord::Valid() const noexcept
    {
        return Slot < NetworkSlotCapacity && Generation != 0 && Life != 0 && Id != 0
            && StartFrame != 0 && static_cast<std::int32_t>(StampFrame - StartFrame) >= 0
            && static_cast<std::int32_t>(StampFrame - AnchorFrame) >= 0
            && ClipFrames > 1 && ClipFrames <= 4096 && ClipFrame < ClipFrames
            && (Direction == ReplicatedFormState::Transition::Morph
                || Direction == ReplicatedFormState::Transition::Unmorph)
            && (Status == FormTransitionStatus::Active || Status == FormTransitionStatus::Completed
                || Status == FormTransitionStatus::Cancelled)
            && (Status != FormTransitionStatus::Completed
                || AltForm == (Direction == ReplicatedFormState::Transition::Morph));
    }

    std::size_t FormTransitionPacket::Write(std::span<std::uint8_t> dest) const
    {
        if (Match == 0 || Epoch == 0 || Frame == 0 || Count > Records.size()) return 0;
        const std::size_t size = HeaderSize + Count * FormTransitionRecord::Size;
        if (dest.size() < size) return 0;
        W16(dest, Match);
        W64(dest.subspan(2), Epoch);
        W32(dest.subspan(10), Frame);
        dest[14] = Count;
        std::uint32_t occupied = 0;
        for (std::size_t i = 0; i < Count; ++i)
        {
            const auto& record = Records[i];
            if (!record.Valid() || static_cast<std::int32_t>(Frame - record.StampFrame) < 0
                || (occupied & (1U << record.Slot)) != 0) return 0;
            occupied |= 1U << record.Slot;
            auto out = dest.subspan(HeaderSize + i * FormTransitionRecord::Size);
            out[0] = record.Slot;
            W16(out.subspan(1), record.Generation);
            W16(out.subspan(3), record.Life);
            W16(out.subspan(5), record.Id);
            W32(out.subspan(7), record.StartFrame);
            W32(out.subspan(11), record.StampFrame);
            W32(out.subspan(15), record.AnchorFrame);
            W16(out.subspan(19), record.ClipFrame);
            W16(out.subspan(21), record.ClipFrames);
            out[23] = static_cast<std::uint8_t>(record.Direction);
            out[24] = static_cast<std::uint8_t>(record.Status);
            out[25] = static_cast<std::uint8_t>((record.AltForm ? 1 : 0) | (record.Paused ? 2 : 0));
        }
        return size;
    }

    bool FormTransitionPacket::TryRead(std::span<const std::uint8_t> src, FormTransitionPacket& packet)
    {
        if (src.size() < HeaderSize || src[14] > NetworkSlotCapacity
            || src.size() != HeaderSize + src[14] * FormTransitionRecord::Size) return false;
        FormTransitionPacket result{};
        result.Match = R16(src);
        result.Epoch = R64(src.subspan(2));
        result.Frame = R32(src.subspan(10));
        result.Count = src[14];
        if (result.Match == 0 || result.Epoch == 0 || result.Frame == 0) return false;
        std::uint32_t occupied = 0;
        for (std::size_t i = 0; i < result.Count; ++i)
        {
            auto in = src.subspan(HeaderSize + i * FormTransitionRecord::Size);
            auto& record = result.Records[i];
            record.Slot = in[0];
            record.Generation = R16(in.subspan(1));
            record.Life = R16(in.subspan(3));
            record.Id = R16(in.subspan(5));
            record.StartFrame = R32(in.subspan(7));
            record.StampFrame = R32(in.subspan(11));
            record.AnchorFrame = R32(in.subspan(15));
            record.ClipFrame = R16(in.subspan(19));
            record.ClipFrames = R16(in.subspan(21));
            record.Direction = static_cast<ReplicatedFormState::Transition>(in[23]);
            record.Status = static_cast<FormTransitionStatus>(in[24]);
            record.AltForm = (in[25] & 1) != 0;
            record.Paused = (in[25] & 2) != 0;
            if ((in[25] & ~3U) != 0 || !record.Valid()
                || static_cast<std::int32_t>(result.Frame - record.StampFrame) < 0
                || (occupied & (1U << record.Slot)) != 0) return false;
            occupied |= 1U << record.Slot;
        }
        packet = result;
        return true;
    }
}
