#include "Formats/entity_format.hpp"
#include "Utility/repack_entity.hpp"

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

fruityprime::utility::entity_repack::Record make_record(
    std::uint16_t type, std::int16_t id, std::size_t payload_size) {
    fruityprime::utility::entity_repack::Record record;
    record.node_name = "rmMain";
    record.layer_mask = 0x0005;
    record.type = type;
    record.entity_id = id;
    record.position = {{4096}, {-2048}, {8192}};
    record.up_vector = {{0}, {4096}, {0}};
    record.facing_vector = {{0}, {0}, {4096}};
    record.payload.resize(payload_size, 0xa5);
    return record;
}

} // namespace

int main() {
    using fruityprime::entity::File;
    using fruityprime::utility::entity_repack::Document;
    using fruityprime::utility::entity_repack::compare;
    using fruityprime::utility::entity_repack::decode;
    using fruityprime::utility::entity_repack::encode;

    Document mph;
    mph.records.push_back(make_record(2, 7, 43));
    mph.records.push_back(make_record(9, 8, 148));
    const auto mph_bytes = encode(mph);
    const auto mph_file = File::from_bytes(mph_bytes);
    assert(!mph_file.is_first_hunt());
    assert(mph_file.records().size() == 2);
    assert(mph_file.header().lengths[0] == 2);
    assert(mph_file.header().lengths[2] == 2);
    assert(compare(mph_bytes, encode(decode(mph_file))).equal);

    auto edited = decode(mph_file);
    edited.records[0].node_name = "edited";
    edited.records[0].layer_mask = 0x0002;
    edited.records[0].entity_id = 33;
    edited.records[0].position.x.value = 1234;
    const auto edited_bytes = encode(edited);
    const auto edited_file = File::from_bytes(edited_bytes);
    assert(edited_file.records().front().entry.node_name == "edited");
    assert(edited_file.records().front().entry.layer_mask == 0x0002);
    assert(edited_file.records().front().header.entity_id == 33);
    assert(edited_file.records().front().header.position.x.value == 1234);

    Document first_hunt;
    first_hunt.version = 1;
    first_hunt.records.push_back(make_record(11, 4, 236));
    const auto fh_bytes = encode(first_hunt);
    const auto fh_file = File::from_bytes(fh_bytes);
    assert(fh_file.is_first_hunt());
    assert(fh_file.first_hunt_records().size() == 1);
    assert(fh_file.first_hunt_records().front().header.type == 11);
    assert(compare(fh_bytes,
                   encode(decode(fh_file))).equal);

    bool rejected = false;
    try {
        auto invalid = mph;
        invalid.records.front().node_name = "this-name-is-too-long";
        (void)encode(invalid);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);

    rejected = false;
    try {
        auto invalid = mph;
        invalid.records.front().payload.resize(39);
        (void)encode(invalid);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
    return 0;
}
