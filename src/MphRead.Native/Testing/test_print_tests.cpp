#include "Testing/test_print.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <stdexcept>
#include <string_view>

int main() {
    using namespace fruityprime::formats;
    using namespace fruityprime::testing::print;

    assert(print_struct("Demo", 12)
        == "struct Demo\n{\n"
           "  int field_0;\n"
           "  int field_4;\n"
           "  int field_8;\n"
           "}\n");

    const Material material{"mat", 1, PolygonMode::Toon,
                            CullingMode::Back, 17};
    const std::uint32_t attr = build_polygon_attr(material, 1);
    assert(attr == ((0x4000U | 0x8000U | 1U | (2U * 16U)
                    | (2U << 6) | (1U << 24) | (17U << 16))));
    const std::string polygon = get_polygon_attrs(
        Model{"model", {material}}, 1);
    assert(polygon.find("model - mat") != std::string::npos);
    assert(polygon.find("mode = 2 (Toon)") != std::string::npos);
    assert(polygon.find("cull = 2 (Back)") != std::string::npos);
    assert(dump_polygon_attr(attr).find("alpha: 17\nid: 1")
           != std::string::npos);

    const Vector3 light = light_calc(
        {0.0F, 0.0F, -1.0F}, {1.0F, 0.5F, 0.25F},
        {0.0F, 0.0F, 1.0F}, {0.2F, 0.3F, 0.4F},
        {0.1F, 0.2F, 0.3F}, {0.5F, 0.6F, 0.7F});
    assert(std::fabs(light.x - 0.8F) < 0.0001F);
    assert(std::fabs(light.y - 0.55F) < 0.0001F);
    assert(std::fabs(light.z - 0.35F) < 0.0001F);

    const std::array<EntityProperty, 5> properties{{
        {"Id", EntityPropertyType::Value},
        {"Enabled", EntityPropertyType::Bool},
        {"Volume", EntityPropertyType::CollisionVolume},
        {"Position", EntityPropertyType::Vector3},
        {"Label", EntityPropertyType::String}}};
    const std::string editor = print_entity_editor("DoorEditor", properties);
    assert(editor.find("DoorData raw") != std::string::npos);
    assert(editor.find("Enabled = raw.Enabled != 0;") != std::string::npos);
    assert(editor.find("Volume = new CollisionVolume(raw.Volume);")
           != std::string::npos);
    assert(editor.find("Position =") == std::string::npos);
    assert(editor.find("Label = raw.Label.MarshalString();")
           != std::string::npos);

    const std::string generated = parse_struct(
        "Demo", "CEntity", "int count;\nVecFx32 position;\nHUNTER hunter;\nint values[2];");
    assert(generated.find("_off0 = 0x18") != std::string::npos);
    assert(generated.find("_off1 = 0x1C") != std::string::npos);
    assert(generated.find("public Vector3 Position") != std::string::npos);
    assert(generated.find("public Hunter Hunter") != std::string::npos);
    assert(generated.find("Int32Array Values") != std::string::npos);
    assert(generated.find("public Demo(Memory memory, IntPtr address)")
           != std::string::npos);

    bool rejected = false;
    try {
        (void)print_struct("Bad", 3);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
    return 0;
}
