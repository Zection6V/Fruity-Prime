#pragma once

#include "../../Formats/Enums.hpp"
#include "../../Formats/Types.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace MphRead::Entities
{
    class PlayerEntity;
}

namespace MphRead::Mods::Multiplayer
{
    struct TeamPresentation final
    {
        std::string Label;
        ::MphRead::ColorRgba Color{};
        ::MphRead::ColorRgb ObjectiveColor{};
        ::MphRead::Team ModelTeam = ::MphRead::Team::None;
        std::int32_t ModelRecolor = 0;

        TeamPresentation() = default;
        TeamPresentation(std::string label, ::MphRead::ColorRgba color,
            ::MphRead::ColorRgb objectiveColor, ::MphRead::Team modelTeam,
            std::int32_t modelRecolor)
            : Label(std::move(label)),
              Color(color),
              ObjectiveColor(objectiveColor),
              ModelTeam(modelTeam),
              ModelRecolor(modelRecolor)
        {
        }

        [[nodiscard]] ::MphRead::ColorRgba HudAccent() const noexcept { return Color; }
        [[nodiscard]] ::MphRead::ColorRgb RadarColor() const noexcept { return ObjectiveColor; }
    };

    class TeamVisuals final
    {
    public:
        TeamVisuals() = delete;
        ~TeamVisuals() = delete;
        TeamVisuals(const TeamVisuals&) = delete;
        TeamVisuals& operator=(const TeamVisuals&) = delete;
        TeamVisuals(TeamVisuals&&) = delete;
        TeamVisuals& operator=(TeamVisuals&&) = delete;

        [[nodiscard]] static const TeamPresentation& Get(std::int32_t teamIndex);
        static void Apply(::MphRead::Entities::PlayerEntity& player);
    };
}
