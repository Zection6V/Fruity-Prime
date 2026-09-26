#pragma once

namespace MphRead::Mods::Input
{
    struct SpectatorInput
    {
        bool NextPlayer = false;
        bool PreviousPlayer = false;
        bool ToggleView = false;
        bool Scoreboard = false;
        bool OpenMenu = false;
        float MoveX = 0;
        float MoveY = 0;
        float LookX = 0;
        float LookY = 0;
        float Ascend = 0;
        float Descend = 0;

        [[nodiscard]] static SpectatorInput ReadController(bool replay = false);
        void ApplyView(bool replay = false) const;

        friend bool operator==(const SpectatorInput&, const SpectatorInput&) = default;
    };
}
