#include "Formats/ai_personality.hpp"
#include "Assets/nds_rom.hpp"

#include <string_view>
#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

[[nodiscard]] std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return value;
}

} // namespace

int main() {
    const char* configured = std::getenv("FRUITY_PRIME_TEST_NDS");
    if (configured == nullptr || configured[0] == '\0') {
        std::cout << "real AI personality test skipped: set "
                     "FRUITY_PRIME_TEST_NDS\n";
        return 0;
    }

    try {
        const auto rom = fruityprime::nds::Rom::read_file(configured);
        const fruityprime::nds::FileEntry* personality_entry = nullptr;
        for (const auto& entry : rom.files()) {
            const std::string path = lower_ascii(entry.path);
            if (path.find("aipersonalitydata/") != std::string::npos
                && path.ends_with("aipersonalitydata.bin")) {
                personality_entry = &entry;
                break;
            }
        }
        if (personality_entry == nullptr) {
            throw std::runtime_error(
                "the ROM has no aiPersonalityData/aiPersonalityData.bin");
        }

        const auto bytes = rom.file(personality_entry->file_id);
        constexpr std::array<std::int32_t, 25> offsets{{
            13480, 32896, 32932, 32968, 33012,
            33152, 33196, 33232, 33372, 33416,
            33556, 33696, 33836, 33976, 35428,
            37576, 39420, 40312, 40556, 41492,
            41948, 42772, 45176, 45220, 45696
        }};
        std::size_t total_nodes = 0;
        for (const std::int32_t offset : offsets) {
            const auto personality = fruityprime::ai::File::parse(
                bytes, static_cast<std::size_t>(offset));
            assert(personality.root != nullptr);
            assert(personality.node_count > 0);
            total_nodes += personality.node_count;
        }

        // AiPersonality.TestRead dumps the tree at offset 33196; print_all is
        // what it prints.  Checked against the real file because the dump
        // reaches into every part of a node -- the two subroutine lists, the
        // per-branch preconditions, and the sibling each branch selects -- so
        // a mis-parsed node shows up here as a missing or misnamed line rather
        // than as a node count that still adds up.
        const auto dumped = fruityprime::ai::File::parse(bytes, 33196);
        assert(dumped.root != nullptr);
        const std::string dump = fruityprime::ai::print_all(*dumped.root);
        assert(!dump.empty());
        assert(dump.find("Init (3a):") != std::string::npos);
        assert(dump.find("Proc (F2):") != std::string::npos);
        // Every node prints all five of its fixed lines, so the counts match.
        const auto count_of = [&dump](std::string_view needle) {
            std::size_t count = 0;
            for (std::size_t i = dump.find(needle); i != std::string::npos;
                 i = dump.find(needle, i + needle.size())) {
                ++count;
            }
            return count;
        };
        const std::size_t init_lines = count_of("Init (3a):");
        // A personality is a DAG, not a tree: several parents can share
        // one child node, and the managed dump enqueues children
        // without a visited set, so a shared node is printed once per
        // parent.  The dump therefore has at least as many nodes as the
        // parser decoded, and usually more.
        assert(init_lines >= dumped.node_count);
        assert(count_of("Proc (3b):") == init_lines);
        assert(count_of("Init (F4):") == init_lines);
        assert(count_of("Proc (F2):") == init_lines);
        // A node either lists its branches or says it has none, never both.
        assert(count_of("Switch(") >= init_lines);
        const std::string node_dump =
            fruityprime::ai::print_node(*dumped.root);
        assert(dump.find(node_dump) == 0);
        std::cout << "real AI personality: file="
                  << personality_entry->path
                  << " bytes=" << bytes.size()
                  << " roots=" << offsets.size()
                  << " decoded_nodes=" << total_nodes << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "real AI personality test failed: " << error.what()
                  << '\n';
        return 1;
    }
}
