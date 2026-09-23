#include "PlayerEntityNetHud.hpp"

#include "../../HUD/HudInfo.hpp"
#include "NetSession.hpp"

#include <cstdint>
#include <string>

namespace MphRead::Entities
{
    float PlayerEntity::ModScoreColumn1() const
    {
        return Mods::Network::NetSession::Active() ? 145.0F : 160.0F;
    }

    float PlayerEntity::ModScoreColumn2() const
    {
        return Mods::Network::NetSession::Active() ? 193.0F : 215.0F;
    }

    void PlayerEntity::ModDrawPingHeader(float posY)
    {
        if (!Mods::Network::NetSession::Active())
        {
            return;
        }

        (void)DrawText2D(_pingColumnX,
            posY,
            Hud::Align::Center,
            0,
            "ping",
            ColorRgba(0x3FEFU),
            8);
    }

    void PlayerEntity::ModDrawPingRow(
        float posY, ColorRgba, std::int32_t slot)
    {
        if (!Mods::Network::NetSession::Active())
        {
            return;
        }
        if (slot < 0)
        {
            return;
        }
        if (slot >= static_cast<std::int32_t>(Mods::Network::NetSession::SlotPing.size()))
        {
            return;
        }

        const std::int32_t ping = Mods::Network::NetSession::SlotPing[slot];

        std::string text;
        if (ping <= 0)
        {
            text = "--";
        }
        else if (ping > 999)
        {
            text = "999";
        }
        else
        {
            text = std::to_string(ping);
        }

        (void)DrawText2D(_pingColumnX,
            posY,
            Hud::Align::Center,
            0,
            text,
            PingColor(ping),
            8);
    }

    ColorRgba PlayerEntity::PingColor(std::int32_t ping)
    {
        if (ping <= 0)
        {
            return ColorRgba(120, 120, 140, 255);
        }
        if (ping < 80)
        {
            return ColorRgba(110, 231, 135, 255);
        }
        if (ping < 160)
        {
            return ColorRgba(255, 200, 80, 255);
        }
        return ColorRgba(255, 110, 110, 255);
    }
}
