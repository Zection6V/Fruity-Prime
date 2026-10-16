#pragma once

namespace MphRead::Entities { class PlayerEntity; }

namespace MphRead::Mods::Network
{
    // Adapts cartridge AI touch actions to the network client's input path.
    class NetBotInput final
    {
    public:
        NetBotInput() = delete;
        static void BeginInput(Entities::PlayerEntity& player);
        static void RequestFormSwitch(Entities::PlayerEntity& player);
        // Whether a camera-only or chat step should clear this main player's
        // binds: true for a person's keys, false for an AI driving them (a
        // headless -ai client's main player starts in camera-only mode).
        [[nodiscard]] static bool KeepsHumanInputClear(const Entities::PlayerEntity& player) noexcept;
    };
}
