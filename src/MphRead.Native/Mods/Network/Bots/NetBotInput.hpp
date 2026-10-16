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
    };
}
