#include "Testing/test_print.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace fruityprime::testing::print {
namespace {

[[nodiscard]] std::string hex_number(int value) {
    std::ostringstream output;
    output << std::uppercase << std::hex << value;
    return output.str();
}

[[nodiscard]] std::string hex_attr(std::uint32_t value) {
    std::ostringstream output;
    output << std::uppercase << std::hex << std::setw(2)
           << std::setfill('0') << value;
    return output.str();
}

[[nodiscard]] std::string binary_value(std::uint32_t value) {
    if (value == 0) {
        return "0";
    }
    std::string result;
    while (value != 0) {
        result.push_back((value & 1U) != 0 ? '1' : '0');
        value >>= 1;
    }
    std::reverse(result.begin(), result.end());
    return result;
}

[[nodiscard]] const char* polygon_mode_name(formats::PolygonMode value) {
    switch (value) {
    case formats::PolygonMode::Modulate: return "Modulate";
    case formats::PolygonMode::Decal: return "Decal";
    case formats::PolygonMode::Toon: return "Toon";
    case formats::PolygonMode::Shadow: return "Shadow";
    }
    return "Unknown";
}

[[nodiscard]] const char* culling_name(formats::CullingMode value) {
    switch (value) {
    case formats::CullingMode::Neither: return "Neither";
    case formats::CullingMode::Front: return "Front";
    case formats::CullingMode::Back: return "Back";
    }
    return "Unknown";
}

[[nodiscard]] formats::Vector3 component_multiply(
    formats::Vector3 left, formats::Vector3 right) noexcept {
    return {left.x * right.x, left.y * right.y, left.z * right.z};
}

[[nodiscard]] std::string replace_all(std::string value,
                                      std::string_view from,
                                      std::string_view to) {
    if (from.empty()) {
        return value;
    }
    std::size_t position = 0;
    while ((position = value.find(from, position)) != std::string::npos) {
        value.replace(position, from.size(), to);
        position += to.size();
    }
    return value;
}

[[nodiscard]] std::string pascal_case(std::string_view value) {
    std::string result;
    std::size_t start = 0;
    while (start <= value.size()) {
        const std::size_t separator = value.find('_', start);
        const std::size_t end = separator == std::string_view::npos
            ? value.size() : separator;
        if (end > start) {
            result.push_back(static_cast<char>(std::toupper(
                static_cast<unsigned char>(value[start]))));
            result.append(value.substr(start + 1, end - start - 1));
        }
        if (separator == std::string_view::npos) {
            break;
        }
        start = separator + 1;
    }
    return result;
}

struct ParsedDeclaration {
    std::string type;
    std::string name;
};

[[nodiscard]] ParsedDeclaration parse_declaration(std::string line) {
    line = replace_all(std::move(line), "signed ", "signed");
    line = replace_all(std::move(line), "unsigned ", "unsigned");
    line = replace_all(std::move(line), " *", "* ");
    line = replace_all(std::move(line), ";", "");

    std::istringstream input(line);
    ParsedDeclaration result;
    input >> result.type >> result.name;
    if (result.type.empty() || result.name.empty()) {
        throw std::invalid_argument("invalid ParseStruct declaration");
    }
    return result;
}

struct FieldDescription {
    std::string type;
    std::string getter;
    std::string setter;
    int size = 0;
    bool enums = false;
    bool embed = false;
    std::string cast;
    std::string comment;

    FieldDescription() = default;
    FieldDescription(std::string type_value, std::string getter_value,
                     std::string setter_value, int size_value,
                     bool enums_value = false, bool embed_value = false,
                     std::string cast_value = {},
                     std::string comment_value = {})
        : type(std::move(type_value)), getter(std::move(getter_value)),
          setter(std::move(setter_value)), size(size_value),
          enums(enums_value), embed(embed_value),
          cast(std::move(cast_value)), comment(std::move(comment_value)) {}
};

[[nodiscard]] FieldDescription describe_field(std::string_view source_type) {
    FieldDescription field;
    const std::string type(source_type);
    if (type.find('*') != std::string::npos) {
        field = {"IntPtr", "ReadPointer", "WritePointer", 4, false,
                 false, {}, " // " + type};
    } else if (type == "EntityPtrUnion") {
        field = {"IntPtr", "ReadPointer", "WritePointer", 4, false,
                 false, {}, " // CEntity*"};
    } else if (type == "EntityIdOrRef") {
        field = {"IntPtr", "ReadPointer", "WritePointer", 4, false,
                 false, {}, " // EntityIdOrRef"};
    } else if (type == "int") {
        field = {"int", "ReadInt32", "WriteInt32", 4};
    } else if (type == "unsignedint" || type == "uint") {
        field = {"uint", "ReadUInt32", "WriteUInt32", 4};
    } else if (type == "signed__int16") {
        field = {"short", "ReadInt16", "WriteInt16", 2};
    } else if (type == "__int16" || type == "unsigned__int16") {
        field = {"ushort", "ReadUInt16", "WriteUInt16", 2};
    } else if (type == "char" || type == "__int8"
               || type == "unsigned__int8") {
        field = {"byte", "ReadByte", "WriteByte", 1};
    } else if (type == "signed__int8") {
        field = {"sbyte", "ReadSByte", "WriteSByte", 1};
    } else if (type == "Color3") {
        field = {"ColorRgb", "ReadColor3", "WriteColor3", 3};
    } else if (type == "VecFx32") {
        field = {"Vector3", "ReadVec3", "WriteVec3", 12};
    } else if (type == "Vec4") {
        field = {"Vector4", "ReadVec4", "WriteVec4", 16};
    } else if (type == "MtxFx43") {
        field = {"Matrix4x3", "ReadMtx43", "WriteMtx43", 48};
    } else if (type == "RoomState") {
        field = {"RoomState", {}, {}, 60, false, true};
    } else if (type == "CModel") {
        field = {"CModel", {}, {}, 0x48, false, true};
    } else if (type == "BeamInfo") {
        field = {"BeamInfo", {}, {}, 0x14, false, true};
    } else if (type == "EntityCollision") {
        field = {"EntityCollision", {}, {}, 0xB4, false, true};
    } else if (type == "SFXParameters") {
        field = {"SfxParameters", {}, {}, 4, false, true};
    } else if (type == "CollisionVolume") {
        field = {"CollisionVolume", {}, {}, 0x40, false, true};
    } else if (type == "Light") {
        field = {"Light", {}, {}, 0xF, false, true};
    } else if (type == "LightInfo") {
        field = {"LightInfo", {}, {}, 0x1F, false, true};
    } else if (type == "CameraInfo") {
        field = {"CameraInfo", {}, {}, 0x11C, false, true};
    } else if (type == "PlayerControls") {
        field = {"PlayerControls", {}, {}, 0x9C, false, true};
    } else if (type == "ButtonControlUnion") {
        field = {"ButtonControlUnion", {}, {}, 4, false, true};
    } else if (type == "PlayerInput") {
        field = {"PlayerInput", {}, {}, 0x48, false, true};
    } else if (type == "CBeamProjectile") {
        field = {"CBeamProjectile", {}, {}, 0x158, false, true};
    } else if (type == "EquipInfo") {
        field = {"EquipInfo", {}, {}, 0x14, false, true};
    } else if (type == "AIButton") {
        field = {"AiButton", {}, {}, 6, false, true};
    } else if (type == "ENEMY_TYPE") {
        field = {"EnemyType", "ReadByte", "WriteByte", 1, true, false,
                 "byte"};
    } else if (type == "HUNTER") {
        field = {"Hunter", "ReadByte", "WriteByte", 1, true, false,
                 "byte"};
    } else if (type == "GAME_MODE") {
        field = {"GameMode", "ReadByte", "WriteByte", 1, true, false,
                 "byte"};
    } else if (type == "ITEM_TYPE") {
        field = {"ItemType", "ReadUInt16", "WriteUInt16", 2, true, false,
                 "ushort"};
    } else if (type == "EVENT_TYPE") {
        field = {"Message", "ReadUInt32", "WriteUInt32", 4, true, false,
                 "uint"};
    } else if (type == "DOOR_TYPE") {
        field = {"DoorType", "ReadUInt32", "WriteUInt32", 4, true, false,
                 "uint"};
    } else if (type == "COLLISION_VOLUME_TYPE") {
        field = {"VolumeType", "ReadUInt32", "WriteUInt32", 4, true, false,
                 "uint"};
    } else {
        throw std::invalid_argument("unknown ParseStruct type: " + type);
    }
    return field;
}

[[nodiscard]] bool is_editor_base_property(std::string_view name) {
    return name == "Id" || name == "Type" || name == "LayerMask"
        || name == "NodeName" || name == "Position" || name == "Up"
        || name == "Facing";
}

} // namespace

std::string print_struct(std::string_view name, int size) {
    if (size <= 0 || size % 4 != 0) {
        throw std::invalid_argument("PrintStruct size must be a positive multiple of 4");
    }
    std::ostringstream output;
    output << "struct " << name << "\n{\n";
    for (int offset = 0; offset < size; offset += 4) {
        output << "  int field_" << hex_number(offset) << ";\n";
    }
    output << "}\n";
    return output.str();
}

std::uint32_t build_polygon_attr(const Material& material, int polygon_id) {
    if (polygon_id < 0) {
        throw std::invalid_argument("polygon id must be non-negative");
    }
    const std::uint32_t v19 = polygon_id == 1 ? 0x4000U : 0U;
    const std::uint32_t v20 = v19 | 0x8000U;
    return v20 | material.lighting
        | (static_cast<std::uint32_t>(material.polygon_mode) * 16U)
        | (static_cast<std::uint32_t>(material.culling) << 6)
        | (static_cast<std::uint32_t>(polygon_id) << 24)
        | (static_cast<std::uint32_t>(material.alpha) << 16);
}

std::string dump_polygon_attr(std::uint32_t attr) {
    std::ostringstream output;
    output << "0x" << hex_attr(attr) << "\n";
    output << binary_value(attr) << "\n";
    output << "light1: " << (attr & 0x1U) << "\n";
    output << "light2: " << ((attr >> 1) & 0x1U) << "\n";
    output << "light3: " << ((attr >> 2) & 0x1U) << "\n";
    output << "light4: " << ((attr >> 3) & 0x1U) << "\n";
    output << "mode: " << ((attr >> 4) & 0x2U) << "\n";
    output << "back: " << ((attr >> 6) & 0x1U) << "\n";
    output << "front: " << ((attr >> 7) & 0x1U) << "\n";
    output << "clear: " << ((attr >> 11) & 0x1U) << "\n";
    output << "far: " << ((attr >> 12) & 0x1U) << "\n";
    output << "1dot: " << ((attr >> 13) & 0x1U) << "\n";
    output << "depth: " << ((attr >> 14) & 0x1U) << "\n";
    output << "fog: " << ((attr >> 15) & 0x1U) << "\n";
    output << "alpha: " << ((attr >> 16) & 0x1FU) << "\n";
    output << "id: " << ((attr >> 24) & 0x3FU) << "\n\n";
    return output.str();
}

std::string get_polygon_attrs(std::string_view model_name,
                              const Material& material, int polygon_id) {
    const std::uint32_t attr = build_polygon_attr(material, polygon_id);
    std::ostringstream output;
    output << model_name << " - " << material.name << "\n";
    output << "light = " << static_cast<int>(material.lighting)
           << ", mode = " << static_cast<std::uint32_t>(material.polygon_mode)
           << " (" << polygon_mode_name(material.polygon_mode) << ")"
           << ", cull = " << static_cast<int>(material.culling)
           << " (" << culling_name(material.culling) << ")"
           << ", alpha = " << static_cast<int>(material.alpha)
           << ", id = " << polygon_id << "\n";
    output << dump_polygon_attr(attr);
    return output.str();
}

std::string get_polygon_attrs(const Model& model, int polygon_id) {
    std::ostringstream output;
    for (const Material& material : model.materials) {
        output << get_polygon_attrs(model.name, material, polygon_id);
    }
    return output.str();
}

formats::Vector3 light_calc(formats::Vector3 light_vec,
                             formats::Vector3 light_col,
                             formats::Vector3 normal_vec,
                             formats::Vector3 dif_col,
                             formats::Vector3 amb_col,
                             formats::Vector3 spe_col) noexcept {
    const formats::Vector3 sight_vec{0.0F, 0.0F, -1.0F};
    const float dif_factor = std::max(0.0F,
        -formats::dot(light_vec, normal_vec));
    const formats::Vector3 half_vec = (light_vec + sight_vec) / 2.0F;
    float spe_factor = std::max(0.0F,
        formats::dot(-half_vec, normal_vec));
    spe_factor *= spe_factor;
    const formats::Vector3 spe_out = component_multiply(
        spe_col, light_col) * spe_factor;
    const formats::Vector3 dif_out = component_multiply(
        dif_col, light_col) * dif_factor;
    const formats::Vector3 amb_out = component_multiply(amb_col, light_col);
    return spe_out + dif_out + amb_out;
}

std::string print_entity_editor(std::string_view type_name,
                                std::span<const EntityProperty> properties) {
    std::string raw_name(type_name);
    raw_name = replace_all(std::move(raw_name), "Editor", "Data");
    std::ostringstream output;
    output << "public " << type_name << "(Entity header, " << raw_name
           << " raw) : base(header)\n        {\n";
    for (const EntityProperty& property : properties) {
        if (property.type == EntityPropertyType::Bool) {
            output << "            " << property.name << " = raw."
                   << property.name << " != 0;\n";
        } else if (property.type == EntityPropertyType::CollisionVolume) {
            output << "            " << property.name << " = new CollisionVolume(raw."
                   << property.name << ");\n";
        } else {
            if (is_editor_base_property(property.name)) {
                continue;
            }
            std::string suffix;
            if (property.type == EntityPropertyType::Vector3) {
                suffix = ".ToFloatVector()";
            } else if (property.type == EntityPropertyType::String) {
                suffix = ".MarshalString()";
            }
            output << "            " << property.name << " = raw."
                   << property.name << suffix << ";\n";
        }
    }
    output << "        }\n";
    return output.str();
}

std::string parse_struct(std::string_view class_name,
                         std::string_view base_class,
                         std::string_view data) {
    if (data.empty()) {
        return {};
    }
    const int initial_offset = base_class == "CEntity" ? 0x18
        : base_class == "CEnemyBase" ? 0x170 : 0;
    const std::string parent = base_class.empty()
        ? "MemoryClass" : std::string(base_class);

    std::ostringstream output;
    output << "public class " << class_name << " : " << parent << "\n"
           << "    {\n";
    std::vector<std::string> news;
    int offset = initial_offset;
    int index = 0;

    std::size_t line_start = 0;
    while (line_start <= data.size()) {
        const std::size_t line_end = data.find('\n', line_start);
        std::string line(data.substr(line_start,
            line_end == std::string_view::npos
                ? data.size() - line_start : line_end - line_start));
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            ParsedDeclaration declaration = parse_declaration(std::move(line));
            std::string property_name = pascal_case(declaration.name);
            FieldDescription field = describe_field(declaration.type);
            bool array = property_name.find('[') != std::string::npos;
            int number = 0;
            if (array) {
                const std::size_t left = property_name.find('[');
                const std::size_t right = property_name.find(']', left);
                if (right == std::string::npos) {
                    throw std::invalid_argument("invalid ParseStruct array");
                }
                number = std::stoi(property_name.substr(left + 1,
                                                         right - left - 1));
                if (number <= 1) {
                    throw std::invalid_argument("ParseStruct arrays must have more than one element");
                }
                property_name.resize(left);
                if (field.comment.empty()) {
                    field.comment = " // " + field.type;
                }
                field.comment += "[" + std::to_string(number) + "]";
                if (field.embed) {
                    field.type = "StructArray<" + field.type + ">";
                } else if (field.enums && field.size == 1) {
                    field.type = "U8EnumArray<" + field.type + ">";
                } else if (field.enums && field.size == 2) {
                    field.type = "U16EnumArray<" + field.type + ">";
                } else if (field.enums && field.size == 4) {
                    field.type = "U32EnumArray<" + field.type + ">";
                } else {
                    std::string array_type = field.getter;
                    array_type = replace_all(std::move(array_type), "Read", "");
                    array_type = replace_all(std::move(array_type), "Pointer", "IntPtr");
                    field.type = array_type + "Array";
                }
                field.size *= number;
            }

            output << "        private const int _off" << index << " = 0x"
                   << hex_number(offset) << ";" << field.comment << "\n";
            if (array) {
                output << "        public " << field.type << " "
                       << property_name << " { get; }\n";
                output << "            " << property_name << " = new "
                       << field.type << "(memory, address + _off" << index
                       << ", " << number;
                if (field.embed) {
                    // field.type is StructArray<T>; recover T for the
                    // generator lambda exactly as the managed code does.
                    const std::size_t left = field.type.find('<');
                    const std::size_t right = field.type.rfind('>');
                    const std::string element = field.type.substr(
                        left + 1, right - left - 1);
                    output << ",\n                " << field.size / number
                           << ", (Memory m, int a) => new " << element
                           << "(m, a)";
                }
                output << ");\n";
            } else if (field.embed) {
                output << "        public " << field.type << " "
                       << property_name << " { get; }\n";
                news.push_back("            " + property_name + " = new "
                    + field.type + "(memory, address + _off"
                    + std::to_string(index) + ");");
            } else if (field.enums) {
                output << "        public " << field.type << " "
                       << property_name << " { get => (" << field.type
                       << ")" << field.getter << "(_off" << index
                       << "); set => " << field.setter << "(_off" << index
                       << ", (" << field.cast << ")value); }\n";
            } else {
                output << "        public " << field.type << " "
                       << property_name << " { get => " << field.getter
                       << "(_off" << index << "); set => " << field.setter
                       << "(_off" << index << ", value); }\n";
            }
            output << "\n";
            ++index;
            offset += field.size;
        }
        if (line_end == std::string_view::npos) {
            break;
        }
        line_start = line_end + 1;
    }

    output << "        public " << class_name
           << "(Memory memory, int address) : base(memory, address)\n"
           << "        {\n";
    for (const std::string& line : news) {
        output << line << "\n";
    }
    output << "        }\n\n";
    output << "        public " << class_name
           << "(Memory memory, IntPtr address) : base(memory, address)\n"
           << "        {\n";
    for (const std::string& line : news) {
        output << line << "\n";
    }
    output << "        }\n    }\n";
    return output.str();
}

} // namespace fruityprime::testing::print
