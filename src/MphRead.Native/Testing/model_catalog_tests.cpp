#include "Assets/model_catalog.hpp"

#include <cassert>

int main() {
    const auto candidates = fruityprime::assets::model_path_candidates(
        "Alimbic_Console");
    assert(candidates.size() == 4);
    assert(candidates[0] == "models/Alimbic_Console_Model.bin");
    assert(candidates[1] == "models/Alimbic_Console_mdl_Model.bin");
    assert(candidates[2] == "models/Alimbic_Console_model.bin");

    const auto lower_case = fruityprime::assets::model_path_candidates(
        "cylinderbase");
    assert(lower_case[2] == "models/cylinderbase_model.bin");

    const auto effect_model = fruityprime::assets::model_path_candidates(
        "particles");
    assert(effect_model[0] == "_archives/effectsBase/particles_Model.bin");
    assert(effect_model[1] == "models/particles_Model.bin");

    const auto hud_model = fruityprime::assets::model_path_candidates("icons");
    assert(hud_model[0] == "hud/icons_Model.bin");
    assert(hud_model[1] == "models/icons_Model.bin");

    const auto tear_model = fruityprime::assets::model_path_candidates(
        "TearParticle");
    assert(tear_model[0] == "models/TearParticle_Model.bin");
    
    const auto animations = fruityprime::assets::animation_path_candidates(
        "unit3_jar");
    assert(animations[0] == "models/unit3_jar_Anim.bin");
    assert(animations[1] == "models/unit3_jar_mdl_Anim.bin");
    assert(fruityprime::assets::model_path_candidates({}).empty());
    return 0;
}
