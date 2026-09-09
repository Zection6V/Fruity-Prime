#include "Assets/nds_rom.hpp"
#include "Formats/node_data.hpp"

#include <algorithm>
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
        std::cout << "real node-data test skipped: set FRUITY_PRIME_TEST_NDS\n";
        return 0;
    }

    try {
        const auto rom = fruityprime::nds::Rom::read_file(configured);
        std::size_t candidates = 0;
        std::size_t parsed = 0;
        std::size_t skipped_versions = 0;
        std::size_t total_nodes = 0;
        for (const auto& entry : rom.files()) {
            const std::string path = lower_ascii(entry.path);
            if (path.find("levels/nodedata/") == std::string::npos
                || path.size() < 4
                || path.substr(path.size() - 4) != ".bin") {
                continue;
            }
            ++candidates;
            const auto bytes = rom.file(entry.file_id);
            if (bytes.size() < 2) {
                throw std::runtime_error("node-data candidate is truncated: "
                                         + entry.path);
            }
            const std::uint16_t version = static_cast<std::uint16_t>(
                bytes[0] | (static_cast<std::uint16_t>(bytes[1]) << 8));
            // The managed reader intentionally leaves version 4 (the
            // unit2_Land file) unsupported until its layout is documented.
            if (version == 4) {
                ++skipped_versions;
                continue;
            }
            const auto data = fruityprime::node::NodeData::from_bytes(bytes);
            assert(data.header().version == version);
            assert(version == 0 || version == 6);
            for (const auto& sub : data.data()) {
                for (const auto& list : sub) {
                    total_nodes += list.size();
                }
            }
            ++parsed;
        }
        if (candidates == 0 || parsed == 0) {
            throw std::runtime_error("the ROM has no supported node-data files");
        }
        std::cout << "real node data: candidates=" << candidates
                  << " parsed=" << parsed
                  << " skipped_versions=" << skipped_versions
                  << " nodes=" << total_nodes << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "real node-data test failed: " << error.what() << '\n';
        return 1;
    }
}
