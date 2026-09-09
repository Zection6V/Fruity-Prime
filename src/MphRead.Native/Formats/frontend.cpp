#include "Formats/frontend_layouts.hpp"
#include "Formats/frontend.hpp"

#include "Read.hpp"

#include <stdexcept>
#include <utility>

namespace fruityprime::frontend {

File File::parse(std::span<const std::uint8_t> bytes) {
    File result;
    result.header_ = read::read_struct<Header>(bytes);
    if (!result.is_marm()) {
        throw std::runtime_error("frontend header is not MARM");
    }

    if (result.header_.offset1 != 0) {
        const auto offsets = read::do_list_null_end(bytes,
                                                     result.header_.offset1);
        result.menu1_.reserve(offsets.size());
        for (const auto offset : offsets) {
            Menu1Entry entry;
            entry.value = read::read_struct<MenuStruct1>(bytes, offset);
            if (entry.value.offset2 != 0) {
                const auto children = read::do_list_null_end(
                    bytes, entry.value.offset2);
                entry.children.reserve(children.size());
                for (const auto child_offset : children) {
                    entry.children.push_back(
                        read::read_struct<MenuStruct1A>(bytes, child_offset));
                }
            }
            result.menu1_.push_back(std::move(entry));
        }
    }

    // The original debug reader only walked list1, but list2 is part of the
    // stable frontend header and is useful to model/menu consumers.  Decode
    // it when present while leaving the still-unmapped list3 opaque.
    if (result.header_.offset2 != 0) {
        const auto offsets = read::do_list_null_end(bytes,
                                                     result.header_.offset2);
        result.menu2_.reserve(offsets.size());
        for (const auto offset : offsets) {
            result.menu2_.push_back(
                read::read_struct<MenuStruct2>(bytes, offset));
        }
    }
    return result;
}

File File::read_file(const std::filesystem::path& path) {
    const auto bytes = read::file(path);
    return parse(bytes);
}

} // namespace fruityprime::frontend
