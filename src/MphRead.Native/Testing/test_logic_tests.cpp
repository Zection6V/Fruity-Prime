#include "Testing/test_logic.hpp"

#include <cassert>
#include <cstdint>

int main() {
    using namespace fruityprime::formats;
    using namespace fruityprime::testing::logic;

    StorySaveData save;
    save.max_scan_count = 100;
    save.scan_count = 10;
    save.weapons = 0x0005;
    save.found_octos = 0x07;
    save.artifacts = 0x000003;
    save.energy_cap = 200;
    save.ammo_caps = {1000, 250};
    const CompletionValues completion = get_completion_values(save);
    assert(completion.octolith == 37);
    assert(completion.energy_tanks == 2);
    assert(completion.ua_expansions == 2);
    assert(completion.missile_expansions == 2);
    assert(completion.completion == 12);
    save.max_scan_count = 0;
    assert(get_completion_percentage(save) == 0);

    const auto blocks = test_save_blocks();
    assert(blocks.size() == 7);
    assert(blocks[0].field_8 == 4352);
    assert(blocks[0].field_14 > 0);
    for (std::size_t index = 1; index < blocks.size(); ++index) {
        assert(blocks[index].field_c >= blocks[index - 1].field_c);
    }

    assert(test_logic_1() == Logic1Branch::SamusPreviousPosition);
    assert(test_logic_1(Hunter::Samus, 0x80)
           == Logic1Branch::SamusSpeedVector);
    assert(test_logic_1(Hunter::Spire) == Logic1Branch::SpireSpeedVector);
    assert(test_logic_1(Hunter::Kanden) == Logic1Branch::Kanden);
    assert(test_logic_1(Hunter::Noxus) == Logic1Branch::Noxus);
    assert(test_logic_1(Hunter::Trace) == Logic1Branch::OtherHunter);

    PlayerDrawState player;
    player.some_flags = static_cast<std::uint32_t>(SomeFlags::AltForm);
    player.hunter = Hunter::Kanden;
    player.field_4bb = 1;
    player.health = 100;
    DrawTrace trace = test_logic_2(player, 0);
    assert(trace.drew_alt_model && trace.drew_attachment
           && trace.draw_count == 2);

    player.some_flags = static_cast<std::uint32_t>(SomeFlags::DrawGunSmoke);
    player.field_4bb = 0;
    player.field_550 = 10;
    player.field_46c = 0;
    trace = test_logic_2(player, 0);
    assert(trace.drew_gun && trace.drew_gun_smoke && trace.draw_count == 2);

    player.more_flags = static_cast<std::uint32_t>(MoreFlags::HideModel);
    trace = test_logic_2(player, 0);
    assert(trace.hidden_by_flag && trace.draw_count == 0);
    return 0;
}
