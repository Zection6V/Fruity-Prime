#include "NetBotInput.hpp"
#include "Entities/Players/PlayerEntity.hpp"
#include "Mods/Network/Session/NetSession.hpp"

namespace MphRead::Mods::Network
{
    namespace
    {
        bool UsesClientInput(const Entities::PlayerEntity& player)
        {
            return NetSession::IsClient() && !NetSession::IsAuthority()
                && player.SlotIndex() == NetSession::LocalSlot();
        }
    }

    void NetBotInput::BeginInput(Entities::PlayerEntity& player)
    {
        if (!UsesClientInput(player)) return;
        // Morph is not among the cartridge AI's physical buttons. Clear the
        // synthetic key every input step so a touch request is a single edge.
        auto& morph = player.Controls().Morph();
        morph.SetIsReleased(morph.IsDown());
        morph.SetIsDown(false);
        morph.SetIsPressed(false);
    }

    void NetBotInput::RequestFormSwitch(Entities::PlayerEntity& player)
    {
        if (UsesClientInput(player))
        {
            auto& morph = player.Controls().Morph();
            morph.SetIsDown(true);
            morph.SetIsPressed(true);
            morph.SetIsReleased(false);
            return;
        }
        // Offline AI and authority bots keep the native request timing.
        player.ModStartFormSwitch();
    }
}
