#pragma once

#include "Mods/Network/Domain/NetworkCapacity.hpp"
#include "Mods/Network/Gameplay/Replication/ReplicatedFormState.hpp"
#include <array>
#include <cstdint>
#include <span>

namespace MphRead::Mods::Network
{
    enum class FormTransitionStatus : std::uint8_t { Active, Completed, Cancelled };

    // A server observation, including the original accepted start and the
    // native 30 Hz clip's current anchor. Pauses never change StartFrame/Id.
    struct FormTransitionRecord final
    {
        static constexpr std::size_t Size = 26;
        std::uint8_t Slot = 0;
        std::uint16_t Generation = 0, Life = 0, Id = 0;
        std::uint32_t StartFrame = 0, StampFrame = 0, AnchorFrame = 0;
        std::uint16_t ClipFrame = 0, ClipFrames = 0;
        ReplicatedFormState::Transition Direction = ReplicatedFormState::Transition::None;
        FormTransitionStatus Status = FormTransitionStatus::Active;
        bool AltForm = false, Paused = false;

        [[nodiscard]] bool Valid() const noexcept;
        bool operator==(const FormTransitionRecord&) const = default;
    };

    struct FormTransitionPacket final
    {
        static constexpr std::size_t HeaderSize = 15;
        static constexpr std::size_t MaxSize = HeaderSize + NetworkSlotCapacity * FormTransitionRecord::Size;
        std::uint16_t Match = 0;
        std::uint64_t Epoch = 0;
        std::uint32_t Frame = 0;
        std::uint8_t Count = 0;
        std::array<FormTransitionRecord, NetworkSlotCapacity> Records{};

        [[nodiscard]] std::size_t Write(std::span<std::uint8_t> dest) const;
        [[nodiscard]] static bool TryRead(std::span<const std::uint8_t> src, FormTransitionPacket& packet);
    };
}
