#include "PlayerEntityNetHud.hpp"

#include <cstdint>
#include <string>

namespace MphRead::Entities::PlayerEntityNetHudDetail
{
    // Declaration-only seams into current NetSession state and the later
    // PlayerEntity draw owner. They expose existing state/calls only.
    enum class Align
    {
        Center
    };

    [[nodiscard]] bool NetSessionActive();
    [[nodiscard]] std::int32_t NetSessionSlotPingLength();
    [[nodiscard]] std::int32_t NetSessionSlotPing(std::int32_t slot);

    void DrawText2D(
        PlayerEntity& player,
        float x,
        float y,
        Align align,
        std::int32_t layer,
        const std::string& text,
        ColorRgba color,
        std::int32_t fontSpacing);
}

namespace MphRead::Entities
{
    float PlayerEntity::ModScoreColumn1() const
    {
        return PlayerEntityNetHudDetail::NetSessionActive() ? 145.0F : 160.0F;
    }

    float PlayerEntity::ModScoreColumn2() const
    {
        return PlayerEntityNetHudDetail::NetSessionActive() ? 193.0F : 215.0F;
    }

    void PlayerEntity::ModDrawPingHeader(float posY)
    {
        if (!PlayerEntityNetHudDetail::NetSessionActive())
        {
            return;
        }

        PlayerEntityNetHudDetail::DrawText2D(
            *this,
            _pingColumnX,
            posY,
            PlayerEntityNetHudDetail::Align::Center,
            0,
            "ping",
            ColorRgba(0x3FEFU),
            8);
    }

    void PlayerEntity::ModDrawPingRow(
        float posY, ColorRgba, std::int32_t slot)
    {
        if (!PlayerEntityNetHudDetail::NetSessionActive())
        {
            return;
        }
        if (slot < 0)
        {
            return;
        }
        if (slot >= PlayerEntityNetHudDetail::NetSessionSlotPingLength())
        {
            return;
        }

        const std::int32_t ping = PlayerEntityNetHudDetail::NetSessionSlotPing(slot);

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

        PlayerEntityNetHudDetail::DrawText2D(
            *this,
            _pingColumnX,
            posY,
            PlayerEntityNetHudDetail::Align::Center,
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
