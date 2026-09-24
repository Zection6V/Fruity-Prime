#include "TeamVisuals.hpp"

#include "../../Entities/Players/PlayerEntity.hpp"

#include <array>

namespace MphRead::Mods::Multiplayer
{
    namespace
    {
        const std::array<TeamPresentation, 4>& Teams()
        {
            static const std::array<TeamPresentation, 4> teams = {
                TeamPresentation("Team A", ::MphRead::ColorRgba(255, 156, 0, 255),
                    ::MphRead::ColorRgb(31, 19, 0), ::MphRead::Team::Orange, 4),
                TeamPresentation("Team B", ::MphRead::ColorRgba(0, 255, 0, 255),
                    ::MphRead::ColorRgb(0, 31, 0), ::MphRead::Team::Green, 5),
                TeamPresentation("Team C", ::MphRead::ColorRgba(41, 156, 255, 255),
                    ::MphRead::ColorRgb(5, 19, 31), ::MphRead::Team::None, -1),
                TeamPresentation("Team D", ::MphRead::ColorRgba(222, 66, 255, 255),
                    ::MphRead::ColorRgb(27, 8, 31), ::MphRead::Team::None, -1)};
            return teams;
        }

        const TeamPresentation& Neutral()
        {
            static const TeamPresentation neutral("Unassigned",
                ::MphRead::ColorRgba(255, 255, 255, 255),
                ::MphRead::ColorRgb(31, 31, 31), ::MphRead::Team::None, -1);
            return neutral;
        }
    }

    const TeamPresentation& TeamVisuals::Get(std::int32_t teamIndex)
    {
        return static_cast<std::uint32_t>(teamIndex) < Teams().size()
            ? Teams()[static_cast<std::size_t>(teamIndex)]
            : Neutral();
    }

    void TeamVisuals::Apply(::MphRead::Entities::PlayerEntity& player)
    {
        const TeamPresentation& visual = Get(player.TeamIndex());
        player.SetTeam(visual.ModelTeam);
        // Original models only provide two team skins. C/D keep a normal
        // hunter suit.
        if (visual.ModelRecolor >= 0)
        {
            player.SetRecolor(visual.ModelRecolor);
        }
        else if (player.Recolor() >= 4)
        {
            player.SetRecolor(0);
        }
    }
}
