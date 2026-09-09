#include "Mods/Launcher/Portable/adventure_save.hpp"
#include "Formats/paths.hpp"
#include "Entities/room_catalog.hpp"
#include "Mods/Launcher/Portable/text_launcher.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <sstream>
#include <string>

int main() {
    const auto root = std::filesystem::temp_directory_path()
        / ("fruity_prime_native_text_launcher_test_"
           + std::to_string(std::chrono::steady_clock::now()
                                .time_since_epoch().count()));
    std::filesystem::create_directories(root / "files" / "AMHE1");

    fruityprime::formats::Paths paths;
    const auto game_root = std::filesystem::absolute(root / "files" / "AMHE1");
    assert(paths.set_path("AMHE1", game_root));
    paths.choose_mph_path();
    assert(paths.write(root));

    std::istringstream input("3\n1\n1\n2\n2\n8\n");
    std::ostringstream output;
    fruityprime::launcher::TextLauncher launcher({root, &input, &output});
    const auto plan = launcher.run();
    assert(plan.has_value());
    assert(plan->kind == fruityprime::launcher::LaunchKind::Offline);
    assert(!fruityprime::scene::multiplayer_rooms().empty());
    assert(plan->room_key == fruityprime::scene::multiplayer_rooms().front().name);
    assert(plan->mode == 3);
    assert(plan->bots == 2);
    assert(plan->bot_level == 2);
    assert(plan->hunter != fruityprime::metadata::Hunter::Random);
    assert(output.str().find("Play offline") != std::string::npos);

    fruityprime::launcher::SaveStore saves(root);
    fruityprime::game::StorySave save;
    save.checkpoint_room_id = 58;
    assert(saves.write(1, save));
    std::istringstream adventure_input("1\n1\n1\n3\n");
    std::ostringstream adventure_output;
    fruityprime::launcher::TextLauncher adventure_launcher({
        root, &adventure_input, &adventure_output
    });
    const auto adventure = adventure_launcher.run();
    assert(adventure.has_value());
    assert(adventure->kind == fruityprime::launcher::LaunchKind::Adventure);
    assert(adventure->room_key == "UNIT2_RM4");
    assert(adventure->mode == 2);
    assert(adventure->save_slot == 1);
    assert(!adventure->new_game);
    assert(adventure->hunter == fruityprime::metadata::Hunter::Trace);

    std::error_code error;
    std::filesystem::remove_all(root, error);
    assert(!std::filesystem::exists(root));
    return 0;
}
