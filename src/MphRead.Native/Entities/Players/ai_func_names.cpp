// Generated from src/MphRead/Entities/Players/PlayerAi.cs by
// tools/gen-ai-func-names.py -- do not edit by hand.
//
// Every AI subroutine is named after the cartridge address it sits at,
// which is the only name any of them has.  A trailing '*' marks an id
// that shares one implementation with a whole range of others, exactly as
// the managed tables mark it.
#include "ai_func_names.hpp"

#include <array>
#include <stdexcept>

namespace fruityprime::players {

namespace {

// init (d3a) and process (d3b)
constexpr std::array<const char*, 84> funcs1_names{{
    "Func1_214A39C", "Func1_214A098", "Func1_2149D3C",
    "Func1_2149C98", "Func1_2149C80", "Func1_2149C68",
    "Func1_2149C50", "Func1_2149C38", "Func1_2149C20",
    "Func1_2149C08", "Func1_2149BF0", "Func1_2149BD8",
    "Func1_2149BC0", "Func1_2149BA8", "Func1_2149B98",
    "Func1_2149AD8", "Func1_2149AC8", "Func1_2149ABC",
    "Func1_2149AB0", "Func1_2149AA4", "Func1_2149A98",
    "Func1_2149A64", "Func1_2149824", "Func1_21497F0",
    "Func1_2149570", "Func1_21494FC", "Func1_2149488",
    "Func1_2149414", "Func1_21493A0", "Func1_214932C",
    "Func1_21495A4", "Func1_2149530", "Func1_21494BC",
    "Func1_2149448", "Func1_21493D4", "Func1_2149360",
    "Func1_21492EC", "Func1_21492DC", "Func1_21492CC",
    "Func1_21492BC", "Func1_21492AC", "Func1_214929C",
    "Func1_214928C", "Func1_214927C", "Func1_214926C",
    "Func1_214925C", "Func1_214924C", "Func1_214923C",
    "Func1_214922C", "Func1_214921C", "Func1_214920C",
    "Func1_21491FC", "Func1_21491E4", "Func1_21491CC",
    "Func1_21491B4", "Func1_214919C", "Func1_2149184",
    "Func1_214916C", "Func1_2149154", "Func1_214913C",
    "Func1_2149124", "Func1_214910C", "Func1_21490F4",
    "Func1_21490DC", "Func1_21490C4", "Func1_21490AC",
    "Func1_2149094", "Func1_2149088", "Func1_2149034",
    "Func1_2148F10", "Func1_2148EDC", "Func1_2148ECC",
    "Func1_2148EB8", "Func1_2148EA8", "Func1_2148E98",
    "Func1_2148E88", "Func1_2148E74", "Func1_2148E64",
    "Func1_2148E54", "Func1_2148DF8", "Func1_2148DE8",
    "Func1_2148D50", "Func1_UnlockEchoHallForceField", "Func1_SetInvulnerable"
}};

// proc (f*2*4)
constexpr std::array<const char*, 126> funcs2_names{{
    "empty", "Func2_213EA10", "Func2_213EA48*",
    "Func2_213EA48*", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213EA48*", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213EA48*", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213EA48*", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213EA48*", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213EA48*", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213EA48*", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213EA48*", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213EA48*", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213EA48*", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213EA48*", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213EA48*", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213EA48*", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213EA48*", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213DDCC", "Func2_213EA48*", "Func2_213DA88",
    "Func2_213EA48*", "Func2_213E148", "Func2_213EA48*",
    "Func2_213EA48*", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213EA48*", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213EA48*", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213EA48*", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213EA48*", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213EA48*", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213EA48*", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213EA48*", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213EA48*", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213EA48*", "Func2_213E9C8", "Func2_213E984",
    "Func2_213E934", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213EA48*", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213EA48*", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213EA48*", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213EA48*", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213EA48*", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213E904", "Func2_213E684", "Func2_213EA48*",
    "Func2_213E3C4", "Func2_213EA48*", "Func2_213E31C",
    "Func2_213E274", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213EA48*", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213EA48*", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213E1CC", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213EA48*", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213EA48*", "Func2_213EA48*", "Func2_213EA48*",
    "Func2_213D9B8", "Func2_213D96C", "empty"
}};

// preconditions (d2->d4->d5) and path updates (d2->d5)
constexpr std::array<const char*, 212> funcs3_names{{
    "Func3_213D87C", "Func3_213D83C", "Func3_213D814",
    "Func3_213D7F0", "Func3_213D800", "Func3_213D7E8",
    "Func3_213D7D0", "Func3_213D7B8", "Func3_213D7A0",
    "Func3_213D77C", "Func3_213D758", "Func3_213D734",
    "Func3_213D710", "Func3_213D6D0", "Func3_213D6AC",
    "Func3_213D624", "Func3_213D608", "Func3_213D564",
    "Func3_213D540", "Func3_213D530", "Func3_213D514",
    "Func3_213D4C0", "Func3_213D49C", "Func3_213D43C",
    "Func3_213D418", "Func3_213D388", "Func3_213D36C",
    "Func3_213D2C0", "Func3_213D2A4", "Func3_213D234",
    "Func3_213D218", "Func3_213D178", "Func3_213D15C",
    "Func3_213D128", "Func3_213D0F4", "Func3_213D0C4",
    "Func3_213D0A8", "Func3_213D078", "Func3_213D05C",
    "Func3_213D044", "Func3_213D028", "Func3_213D010",
    "Func3_213CFF4", "Func3_213CFDC", "Func3_213CFC0",
    "Func3_213CFA4", "Func3_213CF0C", "Func3_213CEE8",
    "Func3_213CDB8", "Func3_213CF94", "Func3_213CF7C",
    "Func3_213CDA4", "Func3_213CD74", "Func3_213CD58",
    "Func3_213CD34", "Func3_213CD18", "Func3_213CCF4",
    "Func3_213CCD8", "Func3_213CCBC", "Func3_213CCB0",
    "Func3_213CC94", "Func3_213CBE4", "Func3_213CBC0",
    "Func3_213CBB0", "Func3_213CB8C", "Func3_213CADC",
    "Func3_213CAA8", "Func3_213CA84", "Func3_213CA70",
    "Func3_213CA58", "Func3_213CA2C", "Func3_213CA00",
    "Func3_213C9D4", "Func3_213C9C4", "Func3_213C89C",
    "Func3_213C88C", "Func3_213C764", "Func3_213C75C",
    "Func3_213C698", "Func3_213C64C", "Func3_213C600",
    "Func3_213C52C", "Func3_213C48C", "Func3_213C470",
    "Func3_213C334", "Func3_213C310", "Func3_213C0D0",
    "Func3_213C078", "Func3_213C054", "Func3_213BFFC",
    "Func3_213BFD8", "Func3_213BED8", "Func3_213BEBC",
    "Func3_213BEA0", "Func3_213BE48", "Func3_213BE10",
    "Func3_213BDF4", "Func3_213BD7C", "Func3_213BCE8",
    "Func3_213BCC4", "Func3_213BCB0", "Func3_213BC8C",
    "Func3_213BC70", "Func3_213BC4C", "Func3_213BC0C",
    "Func3_213BBE8", "Func3_213BBA0", "Func3_213BB7C",
    "Func3_213BAF4", "Func3_213BAD0", "Func3_213BA68",
    "Func3_213BA44", "Func3_213BA28", "Func3_213BA04",
    "Func3_213B99C", "Func3_213B978", "Func3_213B8B0",
    "Func3_213B88C", "Func3_213B7A0", "Func3_213B77C",
    "Func3_213B690", "Func3_213B5DC", "Func3_213B528",
    "Func3_213B4E4", "Func3_213B4A0", "Func3_213B45C",
    "Func3_213B3F0", "Func3_213B3A0", "Func3_213B37C",
    "Func3_213B34C", "Func3_213B328", "Func3_213B284",
    "Func3_213B260", "Func3_213B1F0", "Func3_213B1D8",
    "Func3_213B1C0", "Func3_213B1A8", "Func3_213B190",
    "Func3_213B178", "Func3_213B160", "Func3_213B148",
    "Func3_213B130", "Func3_213B118", "Func3_213B100",
    "Func3_213B0E8", "Func3_213B0D0", "Func3_213B0B8",
    "Func3_213B0A0", "Func3_213B088", "Func3_213B070",
    "Func3_213B058", "Func3_213B040", "Func3_213B020",
    "Func3_213B000", "Func3_213AFE0", "Func3_213AFC0",
    "Func3_213AFA0", "Func3_213AF80", "Func3_213AF68",
    "Func3_213AF50", "Func3_213AF38", "Func3_213AF20",
    "Func3_213AF08", "Func3_213AEF0", "Func3_213AED8",
    "Func3_213AEC0", "Func3_213AEA8", "Func3_213AE90",
    "Func3_213AE78", "Func3_213AE60", "Func3_213AE48",
    "Func3_213AE30", "Func3_213AE14", "Func3_213ADF8",
    "Func3_213ADC4", "Func3_213ADA0", "Func3_213AD88",
    "Func3_213AD64", "Func3_213ACE8", "Func3_213ACCC",
    "Func3_213ACA8", "Func3_213AC8C", "Func3_213AC70",
    "Func3_213AC54", "Func3_213AC38", "Func3_213AC04",
    "Func3_213ABC0", "Func3_213AB8C", "Func3_213AB58",
    "Func3_213AB24", "Func3_213AAF0", "Func3_213AA64",
    "Func3_213AA20", "Func3_213A9B8", "Func3_213A94C",
    "Func3_213A938", "Func3_213A91C", "Func3_213A900",
    "Func3_213A8DC", "Func3_213A8A8", "Func3_213A884",
    "Func3_213A868", "Func3_213A844", "Func3_213A828",
    "Func3_213A804", "Func3_213A798", "Func3_213A72C",
    "Func3_213A714", "Func3_213A698", "Func3_213A688",
    "Func3_213A660", "Func3_213A650"
}};

// init (f2*4*)
constexpr std::array<const char*, 126> funcs4_names{{
    "empty", "empty", "Func4_21462DC*",
    "Func4_21462DC*", "Func4_21462DC*", "Func4_21462DC*",
    "Func4_21462DC*", "Func4_21462DC*", "Func4_21462DC*",
    "Func4_21462DC*", "Func4_21462DC*", "Func4_21462DC*",
    "Func4_21462DC*", "Func4_21462DC*", "Func4_21462DC*",
    "Func4_21462DC*", "Func4_21462DC*", "Func4_21462DC*",
    "Func4_21462DC*", "Func4_21462DC*", "Func4_21462DC*",
    "Func4_21462DC*", "Func4_21462DC*", "Func4_21462DC*",
    "Func4_21462DC*", "Func4_21462DC*", "Func4_21462DC*",
    "Func4_21462DC*", "Func4_21462DC*", "Func4_21462DC*",
    "Func4_21462DC*", "Func4_21462DC*", "Func4_21462DC*",
    "Func4_21462DC*", "Func4_21462DC*", "Func4_21462DC*",
    "Func4_21462DC*", "Func4_21462DC*", "Func4_21462DC*",
    "Func4_21462DC*", "Func4_21462DC*", "Func4_21462DC*",
    "Func4_21462DC*", "Func4_21462DC*", "Func4_21462DC*",
    "Func4_2145EB0", "Func4_21462DC*", "empty",
    "Func4_21462DC*", "empty", "Func4_21462DC*",
    "Func4_21462DC*", "Func4_21462DC*", "Func4_21462DC*",
    "Func4_21462DC*", "Func4_21462DC*", "Func4_21462DC*",
    "Func4_21462DC*", "Func4_21462DC*", "Func4_21462DC*",
    "Func4_21462DC*", "Func4_21462DC*", "Func4_21462DC*",
    "Func4_21462DC*", "Func4_21462DC*", "Func4_21462DC*",
    "Func4_21462DC*", "Func4_21462DC*", "Func4_21462DC*",
    "Func4_21462DC*", "Func4_21462DC*", "Func4_21462DC*",
    "Func4_21462DC*", "Func4_21462DC*", "Func4_21462DC*",
    "Func4_21462DC*", "Func4_21462DC*", "Func4_21462DC*",
    "Func4_21462DC*", "Func4_21462AC", "Func4_2146284",
    "Func4_21461EC", "Func4_21462DC*", "Func4_21462DC*",
    "Func4_21462DC*", "Func4_21462DC*", "Func4_21462DC*",
    "Func4_21462DC*", "Func4_21462DC*", "Func4_21462DC*",
    "Func4_21462DC*", "Func4_21462DC*", "Func4_21462DC*",
    "Func4_21462DC*", "Func4_21462DC*", "Func4_21462DC*",
    "Func4_21462DC*", "Func4_21462DC*", "Func4_21462DC*",
    "empty", "Func4_214612C", "Func4_21462DC*",
    "Func4_2145F78", "Func4_21462DC*", "Func4_2145F50",
    "Func4_2145F28", "Func4_21462DC*", "Func4_21462DC*",
    "Func4_21462DC*", "Func4_21462DC*", "Func4_21462DC*",
    "Func4_21462DC*", "Func4_21462DC*", "Func4_21462DC*",
    "Func4_2145F00", "Func4_21462DC*", "Func4_21462DC*",
    "Func4_21462DC*", "Func4_21462DC*", "Func4_21462DC*",
    "Func4_21462DC*", "Func4_21462DC*", "Func4_21462DC*",
    "Func4_2145E54", "Func4_2145E40", "Func4_SetDespawned"
}};

} // namespace

std::string_view funcs1_name(int id) {
    if (id < 0 || id >= 84) {
        throw std::out_of_range("Invalid AI func id.");
    }
    return funcs1_names[static_cast<std::size_t>(id)];
}

std::string_view funcs2_name(int id) {
    if (id < 0 || id >= 126) {
        throw std::out_of_range("Invalid AI func id.");
    }
    return funcs2_names[static_cast<std::size_t>(id)];
}

std::string_view funcs3_name(int id) {
    if (id < 0 || id >= 212) {
        throw std::out_of_range("Invalid AI func id.");
    }
    return funcs3_names[static_cast<std::size_t>(id)];
}

std::string_view funcs4_name(int id) {
    if (id < 0 || id >= 126) {
        throw std::out_of_range("Invalid AI func id.");
    }
    return funcs4_names[static_cast<std::size_t>(id)];
}

std::vector<std::string_view> funcs1_name_list(
    std::span<const int> ids) {
    std::vector<std::string_view> names;
    names.reserve(ids.size());
    for (int id : ids) {
        names.push_back(funcs1_name(id));
    }
    return names;
}

std::vector<std::string_view> funcs3_name_list(
    std::span<const int> ids) {
    std::vector<std::string_view> names;
    names.reserve(ids.size());
    for (int id : ids) {
        names.push_back(funcs3_name(id));
    }
    return names;
}

} // namespace fruityprime::players
