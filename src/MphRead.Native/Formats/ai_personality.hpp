#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::ai {

// Fixed records from Formats/AiPersonality.cs.  The data contains offsets,
// not pointers; keeping these records packed makes their on-disk contract
// explicit while the decoded tree below remains safe C++ ownership.
#pragma pack(push, 1)
struct AiData1 {
    std::int32_t field0 = 0;
    std::int32_t data1_count = 0;
    std::int32_t data1_offset = 0;
    std::int32_t data2_count = 0;
    std::int32_t data2_offset = 0;
    std::int32_t data3a_count = 0;
    std::int32_t data3a_offset = 0;
    std::int32_t data3b_count = 0;
    std::int32_t data3b_offset = 0;
};

struct AiData2 {
    std::int32_t data5_type = 0;
    std::int32_t data4_count = 0;
    std::int32_t data4_offset = 0;
    std::int32_t field_c = 0;
    std::int32_t field_10 = 0;
    std::int32_t data5_offset = 0;
};

struct AiData4 {
    std::int32_t data5_type = 0;
    std::int32_t data5_offset = 0;
};
#pragma pack(pop)

static_assert(sizeof(AiData1) == 36);
static_assert(sizeof(AiData2) == 24);
static_assert(sizeof(AiData4) == 8);

struct Parameters {
    std::int32_t param1 = 0;
    std::int32_t param2 = 0;
    bool is_empty = true;
};

struct Data4 {
    std::int32_t func3_id = 0;
    Parameters parameters;
};

struct Data2 {
    std::int32_t data1_select_index = 0;
    std::int32_t weight = 0;
    std::vector<Data4> data4;
    std::int32_t func3_id = 0;
    Parameters parameters;
};

struct Data1;
using Data1Ptr = std::shared_ptr<Data1>;

struct Data1 {
    std::string label = "?";
    std::int32_t func24_id = 0;
    std::vector<Data1Ptr> data1;
    std::vector<Data2> data2;
    std::vector<std::int32_t> data3a;
    std::vector<std::int32_t> data3b;
    std::weak_ptr<Data1> parent;
};

// AiPersonality.PrintNode: one node of a personality tree, rendered the way
// the managed debug dump renders it -- the init and process subroutine lists
// with their names, and every branch with its weight and where it goes.
//
// It is the only readable view of what a bot's tree actually says: the file
// is a graph of numbered subroutines, and the numbers alone say nothing.
[[nodiscard]] std::string print_node(const Data1& node);

// AiPersonality.PrintAll: the whole tree, breadth first, with a rule between
// depths.
[[nodiscard]] std::string print_all(const Data1& root);

struct Personality {
    Data1Ptr root;
    std::size_t node_count = 0;

    // Equivalent to AiPersonalityData1.SetLabels().
    void set_labels();
};

// Mode-dependent roots copied from AiPersonality.LoadAll.  The mode values
// are the cartridge GameMode byte values, so this header does not depend on
// the networking module's enum declaration.
[[nodiscard]] std::int32_t select_offset(
    std::uint8_t mode, std::int32_t encounter_state,
    std::uint8_t hunter) noexcept;

class File {
public:
    [[nodiscard]] static Personality parse(
        std::span<const std::uint8_t> bytes, std::size_t root_offset);
    [[nodiscard]] static Personality parse(
        const std::vector<std::uint8_t>& bytes, std::size_t root_offset);
    [[nodiscard]] static Personality read_file(
        const std::filesystem::path& path, std::size_t root_offset);
};

} // namespace fruityprime::ai

namespace MphReadNative {
namespace AiPersonality = ::fruityprime::ai;
}
