#pragma once

// Native counterpart of FrontendMeta.MovieInfo / MovieFiles and the two
// standalone model entries.  A cutscene is two VX files, one per DS screen;
// some have no bottom-screen half.

#include <array>
#include <optional>
#include <string_view>

namespace fruityprime::metadata {

// FrontendMeta.MovieInfo
struct MovieInfo {
    std::string_view TopScreenPath;
    std::string_view BottomScreenPath;
};

// FrontendMeta.MovieFiles
// The managed array deliberately contains null entries at indices 13 and 34.
// Keep those slots instead of compacting the list: Movie enum values index this
// table directly.
inline constexpr std::array<std::optional<MovieInfo>, 36> MovieFiles{{
    MovieInfo{"movies\\01_top.vx", "movies\\01_bot.vx"},
    MovieInfo{"movies\\02_top.vx", "movies\\02_bot.vx"},
    MovieInfo{"movies\\03_top.vx", "movies\\03_bot.vx"},
    MovieInfo{"movies\\04.vx", {}},
    MovieInfo{"movies\\05.vx", {}},
    MovieInfo{"movies\\06.vx", {}},
    MovieInfo{"movies\\07.vx", {}},
    MovieInfo{"movies\\08.vx", {}},
    MovieInfo{"movies\\09.vx", {}},
    MovieInfo{"movies\\10.vx", {}},
    MovieInfo{"movies\\11.vx", {}},
    MovieInfo{"movies\\12_top.vx", "movies\\12_bot.vx"},
    MovieInfo{"movies\\13.vx", {}},
    std::nullopt,
    MovieInfo{"movies\\15_top.vx", "movies\\15_bot.vx"},
    MovieInfo{"movies\\16_top.vx", "movies\\16_bot.vx"},
    MovieInfo{"movies\\17_top.vx", "movies\\17_bot.vx"},
    MovieInfo{"movies\\18_top.vx", "movies\\18_bot.vx"},
    MovieInfo{"movies\\19_top.vx", "movies\\19_bot.vx"},
    MovieInfo{"movies\\20_top.vx", "movies\\20_bot.vx"},
    MovieInfo{"movies\\21_top.vx", "movies\\21_bot.vx"},
    MovieInfo{"movies\\22_top.vx", "movies\\22_bot.vx"},
    MovieInfo{"movies\\23_top.vx", "movies\\23_bot.vx"},
    MovieInfo{"movies\\24_top.vx", "movies\\24_bot.vx"},
    MovieInfo{"movies\\25_top.vx", "movies\\25_bot.vx"},
    MovieInfo{"movies\\26_top.vx", "movies\\26_bot.vx"},
    MovieInfo{"movies\\27_top.vx", "movies\\27_bot.vx"},
    MovieInfo{"movies\\28_top.vx", "movies\\28_bot.vx"},
    MovieInfo{"movies\\29_top.vx", "movies\\29_bot.vx"},
    MovieInfo{"movies\\30_top.vx", "movies\\30_bot.vx"},
    MovieInfo{"movies\\31_top.vx", "movies\\31_bot.vx"},
    MovieInfo{"movies\\32_top.vx", "movies\\32_bot.vx"},
    MovieInfo{"movies\\33_top.vx", "movies\\33_bot.vx"},
    MovieInfo{"movies\\34_top.vx", "movies\\34_bot.vx"},
    std::nullopt,
    MovieInfo{"movies\\36_top.vx", "movies\\36_bot.vx"}
}};

// FrontendMeta.Ad2Dm2: a stage model the frontend loads by itself.
inline constexpr std::string_view Ad2Dm2Name = "ad2_dm2";

} // namespace fruityprime::metadata
