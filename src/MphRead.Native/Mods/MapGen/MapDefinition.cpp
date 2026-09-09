/*
 * Native translation of MphRead/Mods/MapGen/MapDefinition.cs.
 *
 * This unit owns the source recipe representation: JSON-compatible parsing,
 * defaults, validation, item-name conversion, and generated-file naming.
 * Map geometry and binary emission remain in mapgen.cpp.
 */
#include "Mods/MapGen/mapgen.hpp"

#include "map_bundle.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <stdexcept>
#include <sstream>
#include <string_view>
#include <type_traits>
#include <utility>

namespace fruityprime::mapgen {
namespace {

struct JsonValue {
    enum class Kind { Null, Boolean, Number, String, Array, Object };
    using Array = std::vector<JsonValue>;
    using Object = std::map<std::string, JsonValue>;

    Kind kind = Kind::Null;
    bool boolean = false;
    double number = 0.0;
    std::string text;
    Array array;
    Object object;
};

class JsonParser {
public:
    explicit JsonParser(std::string input) : input_(std::move(input)) {}

    [[nodiscard]] JsonValue parse() {
        skip_space_and_comments();
        JsonValue result = parse_value();
        skip_space_and_comments();
        if (position_ != input_.size()) {
            fail("unexpected data after the JSON document");
        }
        return result;
    }

private:
    [[noreturn]] void fail(std::string_view message) const {
        throw std::runtime_error("map JSON error at byte "
                                 + std::to_string(position_) + ": "
                                 + std::string(message));
    }

    void skip_space_and_comments() {
        for (;;) {
            while (position_ < input_.size()
                   && std::isspace(static_cast<unsigned char>(input_[position_])) != 0) {
                ++position_;
            }
            if (position_ + 1 >= input_.size() || input_[position_] != '/') {
                return;
            }
            if (input_[position_ + 1] == '/') {
                position_ += 2;
                while (position_ < input_.size() && input_[position_] != '\n') {
                    ++position_;
                }
                continue;
            }
            if (input_[position_ + 1] == '*') {
                position_ += 2;
                while (position_ + 1 < input_.size()
                       && !(input_[position_] == '*'
                            && input_[position_ + 1] == '/')) {
                    ++position_;
                }
                if (position_ + 1 >= input_.size()) {
                    fail("unterminated block comment");
                }
                position_ += 2;
                continue;
            }
            return;
        }
    }

    [[nodiscard]] JsonValue parse_value() {
        skip_space_and_comments();
        if (position_ >= input_.size()) {
            fail("expected a value");
        }
        switch (input_[position_]) {
        case '{': return parse_object();
        case '[': return parse_array();
        case '"': {
            JsonValue result;
            result.kind = JsonValue::Kind::String;
            result.text = parse_string();
            return result;
        }
        case 't':
            expect_word("true");
            {
                JsonValue result;
                result.kind = JsonValue::Kind::Boolean;
                result.boolean = true;
                return result;
            }
        case 'f':
            expect_word("false");
            {
                JsonValue result;
                result.kind = JsonValue::Kind::Boolean;
                result.boolean = false;
                return result;
            }
        case 'n':
            expect_word("null");
            return JsonValue{};
        default:
            return parse_number();
        }
    }

    [[nodiscard]] JsonValue parse_object() {
        JsonValue result;
        result.kind = JsonValue::Kind::Object;
        ++position_; // {
        skip_space_and_comments();
        if (consume('}')) {
            return result;
        }
        for (;;) {
            skip_space_and_comments();
            if (position_ >= input_.size() || input_[position_] != '"') {
                fail("object key must be a string");
            }
            std::string key = parse_string();
            skip_space_and_comments();
            if (!consume(':')) {
                fail("expected ':' after object key");
            }
            result.object.insert_or_assign(std::move(key), parse_value());
            skip_space_and_comments();
            if (consume('}')) {
                return result;
            }
            if (!consume(',')) {
                fail("expected ',' or '}' in object");
            }
            skip_space_and_comments();
            if (consume('}')) { // C# JsonSerializer allows trailing commas.
                return result;
            }
        }
    }

    [[nodiscard]] JsonValue parse_array() {
        JsonValue result;
        result.kind = JsonValue::Kind::Array;
        ++position_; // [
        skip_space_and_comments();
        if (consume(']')) {
            return result;
        }
        for (;;) {
            result.array.push_back(parse_value());
            skip_space_and_comments();
            if (consume(']')) {
                return result;
            }
            if (!consume(',')) {
                fail("expected ',' or ']' in array");
            }
            skip_space_and_comments();
            if (consume(']')) {
                return result;
            }
        }
    }

    [[nodiscard]] std::string parse_string() {
        if (!consume('"')) {
            fail("expected a string");
        }
        std::string result;
        while (position_ < input_.size()) {
            const char value = input_[position_++];
            if (value == '"') {
                return result;
            }
            if (value == '\\') {
                if (position_ >= input_.size()) {
                    fail("unterminated string escape");
                }
                const char escaped = input_[position_++];
                switch (escaped) {
                case '"': result.push_back('"'); break;
                case '\\': result.push_back('\\'); break;
                case '/': result.push_back('/'); break;
                case 'b': result.push_back('\b'); break;
                case 'f': result.push_back('\f'); break;
                case 'n': result.push_back('\n'); break;
                case 'r': result.push_back('\r'); break;
                case 't': result.push_back('\t'); break;
                case 'u': {
                    if (position_ + 4 > input_.size()) {
                        fail("short unicode escape");
                    }
                    unsigned value16 = 0;
                    for (int i = 0; i < 4; ++i) {
                        const char digit = input_[position_++];
                        value16 <<= 4;
                        if (digit >= '0' && digit <= '9') {
                            value16 |= static_cast<unsigned>(digit - '0');
                        } else if (digit >= 'a' && digit <= 'f') {
                            value16 |= static_cast<unsigned>(digit - 'a' + 10);
                        } else if (digit >= 'A' && digit <= 'F') {
                            value16 |= static_cast<unsigned>(digit - 'A' + 10);
                        } else {
                            fail("invalid unicode escape");
                        }
                    }
                    // Map names and enum values are ASCII. Preserve ASCII
                    // escapes and make non-ASCII descriptive text harmless.
                    result.push_back(value16 <= 0x7f
                                         ? static_cast<char>(value16)
                                         : '?');
                    break;
                }
                default:
                    fail("unknown string escape");
                }
                continue;
            }
            if (static_cast<unsigned char>(value) < 0x20) {
                fail("control character in string");
            }
            result.push_back(value);
        }
        fail("unterminated string");
    }

    [[nodiscard]] JsonValue parse_number() {
        const char* begin = input_.data() + position_;
        char* end = nullptr;
        const double value = std::strtod(begin, &end);
        if (end == begin) {
            fail("expected a number, string, object, array, true, false, or null");
        }
        position_ = static_cast<std::size_t>(end - input_.data());
        if (!std::isfinite(value)) {
            fail("number is not finite");
        }
        JsonValue result;
        result.kind = JsonValue::Kind::Number;
        result.number = value;
        return result;
    }

    void expect_word(std::string_view word) {
        if (input_.compare(position_, word.size(), word) != 0) {
            fail("invalid literal");
        }
        position_ += word.size();
    }

    [[nodiscard]] bool consume(char expected) {
        if (position_ < input_.size() && input_[position_] == expected) {
            ++position_;
            return true;
        }
        return false;
    }

    std::string input_;
    std::size_t position_ = 0;
};

[[nodiscard]] std::string lower(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const char character : value) {
        result.push_back(static_cast<char>(std::tolower(
            static_cast<unsigned char>(character))));
    }
    return result;
}

[[nodiscard]] const JsonValue* member(const JsonValue& object,
                                      std::string_view name) {
    if (object.kind != JsonValue::Kind::Object) {
        return nullptr;
    }
    const std::string wanted = lower(name);
    for (const auto& [key, value] : object.object) {
        if (lower(key) == wanted) {
            return &value;
        }
    }
    return nullptr;
}

[[nodiscard]] const JsonValue& require_object(const JsonValue& value,
                                              std::string_view name) {
    if (value.kind != JsonValue::Kind::Object) {
        throw std::runtime_error("map JSON field " + std::string(name)
                                 + " must be an object");
    }
    return value;
}

[[nodiscard]] const JsonValue& require_array(const JsonValue& value,
                                             std::string_view name) {
    if (value.kind != JsonValue::Kind::Array) {
        throw std::runtime_error("map JSON field " + std::string(name)
                                 + " must be an array");
    }
    return value;
}

[[nodiscard]] std::string string_value(const JsonValue& object,
                                       std::string_view name,
                                       std::string fallback = {}) {
    const JsonValue* value = member(object, name);
    if (value == nullptr || value->kind == JsonValue::Kind::Null) {
        return fallback;
    }
    if (value->kind != JsonValue::Kind::String) {
        throw std::runtime_error("map JSON field " + std::string(name)
                                 + " must be a string");
    }
    return value->text;
}

[[nodiscard]] double number_value(const JsonValue& object,
                                  std::string_view name, double fallback) {
    const JsonValue* value = member(object, name);
    if (value == nullptr || value->kind == JsonValue::Kind::Null) {
        return fallback;
    }
    if (value->kind != JsonValue::Kind::Number) {
        throw std::runtime_error("map JSON field " + std::string(name)
                                 + " must be a number");
    }
    return value->number;
}

[[nodiscard]] int int_value(const JsonValue& object, std::string_view name,
                            int fallback) {
    const double value = number_value(object, name, fallback);
    if (!std::isfinite(value) || value < static_cast<double>(std::numeric_limits<int>::min())
        || value > static_cast<double>(std::numeric_limits<int>::max())
        || std::floor(value) != value) {
        throw std::runtime_error("map JSON field " + std::string(name)
                                 + " must be a finite integer");
    }
    return static_cast<int>(value);
}

[[nodiscard]] std::uint32_t uint_value(const JsonValue& object,
                                       std::string_view name,
                                       std::uint32_t fallback) {
    const double value = number_value(object, name, fallback);
    if (!std::isfinite(value) || value < 0.0
        || value > static_cast<double>(std::numeric_limits<std::uint32_t>::max())
        || std::floor(value) != value) {
        throw std::runtime_error("map JSON field " + std::string(name)
                                 + " must be a non-negative integer");
    }
    return static_cast<std::uint32_t>(value);
}

[[nodiscard]] float float_value(const JsonValue& object,
                                std::string_view name, float fallback) {
    const double value = number_value(object, name, fallback);
    if (!std::isfinite(value)
        || value < -static_cast<double>(std::numeric_limits<float>::max())
        || value > static_cast<double>(std::numeric_limits<float>::max())) {
        throw std::runtime_error("map JSON field " + std::string(name)
                                 + " must be finite");
    }
    return static_cast<float>(value);
}

[[nodiscard]] bool bool_value(const JsonValue& object, std::string_view name,
                              bool fallback) {
    const JsonValue* value = member(object, name);
    if (value == nullptr || value->kind == JsonValue::Kind::Null) {
        return fallback;
    }
    if (value->kind != JsonValue::Kind::Boolean) {
        throw std::runtime_error("map JSON field " + std::string(name)
                                 + " must be a boolean");
    }
    return value->boolean;
}

[[nodiscard]] Vec3 vector_value(const JsonValue& object, std::string_view name,
                                Vec3 fallback) {
    const JsonValue* value = member(object, name);
    if (value == nullptr || value->kind == JsonValue::Kind::Null) {
        return fallback;
    }
    const auto& array = require_array(*value, name).array;
    if (array.size() < 3) {
        throw std::runtime_error("map JSON field " + std::string(name)
                                 + " must contain three numbers");
    }
    const auto number = [&](std::size_t index) {
        if (array[index].kind != JsonValue::Kind::Number
            || !std::isfinite(array[index].number)) {
            throw std::runtime_error("map JSON vector field "
                                     + std::string(name)
                                     + " contains a non-number");
        }
        return static_cast<float>(array[index].number);
    };
    return {number(0), number(1), number(2)};
}

[[nodiscard]] std::optional<Vec3> optional_vector(const JsonValue& object,
                                                  std::string_view name) {
    if (member(object, name) == nullptr) {
        return std::nullopt;
    }
    return vector_value(object, name, {});
}

template <typename Value>
[[nodiscard]] std::array<Value, 3> array3_value(
    const JsonValue& object, std::string_view name,
    std::array<Value, 3> fallback) {
    const JsonValue* value = member(object, name);
    if (value == nullptr || value->kind == JsonValue::Kind::Null) {
        return fallback;
    }
    const auto& array = require_array(*value, name).array;
    if (array.size() < 3) {
        throw std::runtime_error("map JSON field " + std::string(name)
                                 + " must contain three numbers");
    }
    std::array<Value, 3> result{};
    for (std::size_t i = 0; i < result.size(); ++i) {
        if (array[i].kind != JsonValue::Kind::Number
            || !std::isfinite(array[i].number)) {
            throw std::runtime_error("map JSON field " + std::string(name)
                                     + " contains a non-number");
        }
        if constexpr (std::is_integral_v<Value>) {
            if (std::floor(array[i].number) != array[i].number) {
                throw std::runtime_error("map JSON field " + std::string(name)
                                         + " contains a non-integer");
            }
            result[i] = static_cast<Value>(array[i].number);
        } else {
            result[i] = static_cast<Value>(array[i].number);
        }
    }
    return result;
}

[[nodiscard]] std::string read_text(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("could not open map definition "
                                 + path.string());
    }
    const std::streamoff size = input.tellg();
    if (size < 0 || static_cast<std::uintmax_t>(size)
        > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("map definition is too large: "
                                 + path.string());
    }
    std::string text(static_cast<std::size_t>(size), '\0');
    input.seekg(0);
    if (!text.empty()) {
        input.read(text.data(), static_cast<std::streamsize>(text.size()));
        if (!input) {
            throw std::runtime_error("could not read map definition "
                                     + path.string());
        }
    }
    return text;
}

template <typename Callback>
void for_each_object(const JsonValue& object, std::string_view name,
                     Callback&& callback) {
    const JsonValue* value = member(object, name);
    if (value == nullptr || value->kind == JsonValue::Kind::Null) {
        return;
    }
    for (const JsonValue& item : require_array(*value, name).array) {
        callback(require_object(item, name));
    }
}

[[nodiscard]] std::vector<Material> parse_materials(const JsonValue& root) {
    std::vector<Material> result;
    for_each_object(root, "materials", [&](const JsonValue& object) {
        Material material;
        material.name = string_value(object, "name", "mat");
        material.source_material = int_value(object, "sourceMaterial", 0);
        material.tex_scale = float_value(object, "texScale", 16.0F);
        result.push_back(std::move(material));
    });
    return result;
}

[[nodiscard]] std::vector<Brush> parse_brushes(const JsonValue& root) {
    std::vector<Brush> result;
    for_each_object(root, "brushes", [&](const JsonValue& object) {
        Brush brush;
        brush.min = vector_value(object, "min", {});
        brush.max = vector_value(object, "max", {});
        brush.material = int_value(object, "material", 0);
        brush.shade = float_value(object, "shade", 1.0F);
        brush.solid = bool_value(object, "solid", true);
        brush.damaging = bool_value(object, "damaging", false);
        brush.terrain = string_value(object, "terrain", "Metal");
        result.push_back(std::move(brush));
    });
    return result;
}

[[nodiscard]] std::vector<Spawn> parse_spawns(const JsonValue& root) {
    std::vector<Spawn> result;
    for_each_object(root, "spawns", [&](const JsonValue& object) {
        Spawn spawn;
        spawn.position = vector_value(object, "position", {});
        spawn.yaw = float_value(object, "yaw", 0.0F);
        result.push_back(spawn);
    });
    return result;
}

[[nodiscard]] std::vector<JumpPad> parse_jump_pads(const JsonValue& root) {
    std::vector<JumpPad> result;
    for_each_object(root, "jumpPads", [&](const JsonValue& object) {
        JumpPad pad;
        pad.position = vector_value(object, "position", {});
        pad.target = optional_vector(object, "target");
        pad.vector = optional_vector(object, "vector");
        pad.speed = float_value(object, "speed", 0.0F);
        pad.size = vector_value(object, "size", pad.size);
        pad.model_id = uint_value(object, "modelId", 0);
        pad.cooldown_time = static_cast<std::uint16_t>(
            std::clamp(int_value(object, "cooldownTime", 20), 0, 65'535));
        pad.control_lock_time = static_cast<std::uint16_t>(
            std::clamp(int_value(object, "controlLockTime", 30), 0, 65'535));
        result.push_back(std::move(pad));
    });
    return result;
}

[[nodiscard]] std::vector<Item> parse_items(const JsonValue& root) {
    std::vector<Item> result;
    for_each_object(root, "items", [&](const JsonValue& object) {
        Item item;
        item.position = vector_value(object, "position", {});
        item.type = string_value(object, "type", "MissileExpansion");
        item.has_base = bool_value(object, "hasBase", true);
        item.spawn_interval = static_cast<std::uint16_t>(
            std::clamp(int_value(object, "spawnInterval", 300), 0, 65'535));
        result.push_back(std::move(item));
    });
    return result;
}

[[nodiscard]] const JsonValue* import_object(const JsonValue& root) {
    const JsonValue* value = member(root, "import");
    if (value == nullptr || value->kind == JsonValue::Kind::Null) {
        return nullptr;
    }
    return &require_object(*value, "import");
}

[[nodiscard]] std::string json_quote(std::string_view value) {
    std::string result = "\"";
    for (const unsigned char character : value) {
        switch (character) {
        case '\\': result += "\\\\"; break;
        case '"': result += "\\\""; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (character < 0x20) {
                result += "\\u00";
                constexpr char hex[] = "0123456789abcdef";
                result.push_back(hex[character >> 4]);
                result.push_back(hex[character & 0xf]);
            } else {
                result.push_back(static_cast<char>(character));
            }
            break;
        }
    }
    result.push_back('"');
    return result;
}

[[nodiscard]] std::string json_vec3(const Vec3& value) {
    std::ostringstream output;
    output << '[' << value.x << ", " << value.y << ", " << value.z << ']';
    return output.str();
}

template <typename Value>
[[nodiscard]] std::string json_array3(const std::array<Value, 3>& value) {
    std::ostringstream output;
    output << '[' << value[0] << ", " << value[1] << ", " << value[2] << ']';
    return output.str();
}

[[nodiscard]] std::string json_materials(
    const std::vector<Material>& materials) {
    std::ostringstream output;
    output << "[\n";
    for (std::size_t index = 0; index < materials.size(); ++index) {
        const Material& material = materials[index];
        output << "    { \"name\": " << json_quote(material.name)
               << ", \"sourceMaterial\": " << material.source_material
               << ", \"texScale\": " << material.tex_scale << " }"
               << (index + 1 == materials.size() ? '\n' : ',');
        if (index + 1 != materials.size()) {
            output << '\n';
        }
    }
    output << "  ]";
    return output.str();
}

[[nodiscard]] std::string json_brushes(const std::vector<Brush>& brushes) {
    std::ostringstream output;
    output << "[\n";
    for (std::size_t index = 0; index < brushes.size(); ++index) {
        const Brush& brush = brushes[index];
        output << "    { \"min\": " << json_vec3(brush.min)
               << ", \"max\": " << json_vec3(brush.max)
               << ", \"material\": " << brush.material
               << ", \"shade\": " << brush.shade
               << ", \"solid\": " << (brush.solid ? "true" : "false")
               << ", \"damaging\": " << (brush.damaging ? "true" : "false");
        if (!brush.terrain.empty()) {
            output << ", \"terrain\": " << json_quote(brush.terrain);
        }
        output << " }" << (index + 1 == brushes.size() ? '\n' : ',');
        if (index + 1 != brushes.size()) {
            output << '\n';
        }
    }
    output << "  ]";
    return output.str();
}

[[nodiscard]] std::string json_spawns(const std::vector<Spawn>& spawns) {
    std::ostringstream output;
    output << "[\n";
    for (std::size_t index = 0; index < spawns.size(); ++index) {
        const Spawn& spawn = spawns[index];
        output << "    { \"position\": " << json_vec3(spawn.position)
               << ", \"yaw\": " << spawn.yaw << " }"
               << (index + 1 == spawns.size() ? '\n' : ',');
        if (index + 1 != spawns.size()) {
            output << '\n';
        }
    }
    output << "  ]";
    return output.str();
}

[[nodiscard]] std::string json_jump_pads(
    const std::vector<JumpPad>& pads) {
    std::ostringstream output;
    output << "[\n";
    for (std::size_t index = 0; index < pads.size(); ++index) {
        const JumpPad& pad = pads[index];
        output << "    { \"position\": " << json_vec3(pad.position);
        if (pad.target.has_value()) {
            output << ", \"target\": " << json_vec3(*pad.target);
        }
        if (pad.vector.has_value()) {
            output << ", \"vector\": " << json_vec3(*pad.vector);
        }
        output << ", \"speed\": " << pad.speed
               << ", \"size\": " << json_vec3(pad.size)
               << ", \"modelId\": " << pad.model_id
               << ", \"cooldownTime\": " << pad.cooldown_time
               << ", \"controlLockTime\": " << pad.control_lock_time
               << " }" << (index + 1 == pads.size() ? '\n' : ',');
        if (index + 1 != pads.size()) {
            output << '\n';
        }
    }
    output << "  ]";
    return output.str();
}

[[nodiscard]] std::string json_items(const std::vector<Item>& items) {
    std::ostringstream output;
    output << "[\n";
    for (std::size_t index = 0; index < items.size(); ++index) {
        const Item& item = items[index];
        output << "    { \"position\": " << json_vec3(item.position)
               << ", \"type\": " << json_quote(item.type)
               << ", \"hasBase\": " << (item.has_base ? "true" : "false")
               << ", \"spawnInterval\": " << item.spawn_interval << " }"
               << (index + 1 == items.size() ? '\n' : ',');
        if (index + 1 != items.size()) {
            output << '\n';
        }
    }
    output << "  ]";
    return output.str();
}

[[nodiscard]] std::string json_import(const MapDefinition& definition) {
    std::ostringstream output;
    output << "{\n"
           << "    \"source\": " << json_quote(definition.import_source);
    if (!definition.import_map_name.empty()) {
        output << ",\n    \"mapName\": "
               << json_quote(definition.import_map_name);
    }
    output << ",\n    \"unitsPerUnit\": "
           << definition.import_units_per_unit
           << ",\n    \"textures\": ";
    if (definition.import_textures.empty()) {
        output << "null";
    } else {
        output << json_quote(definition.import_textures);
    }
    output << ",\n    \"defaultMaterial\": "
           << definition.import_default_material;
    if (!definition.import_shader_materials.empty()) {
        output << ",\n    \"shaderMaterials\": {";
        for (std::size_t index = 0; index < definition.import_shader_materials.size();
             ++index) {
            const auto& [shader, material] = definition.import_shader_materials[index];
            output << (index == 0 ? "\n      " : ",\n      ")
                   << json_quote(shader) << ": " << material;
        }
        output << "\n    }";
    }
    output << ",\n    \"texScale\": " << definition.import_tex_scale
           << ",\n    \"keepSky\": "
           << (definition.import_keep_sky ? "true" : "false")
           << ",\n    \"keepClip\": "
           << (definition.import_keep_clip ? "true" : "false")
           << ",\n    \"patchLevel\": " << definition.import_patch_level
           << ",\n    \"keepSpawns\": "
           << (definition.import_keep_spawns ? "true" : "false")
           << "\n  }";
    return output.str();
}

} // namespace

MapDefinition load_definition(const std::filesystem::path& path) {
    const std::filesystem::path absolute = std::filesystem::absolute(path);
    std::string text;
    if (bundle::is_bundle(absolute)) {
        const auto recipe = bundle::read_recipe(absolute);
        if (!recipe.has_value()) {
            throw std::runtime_error(absolute.filename().string()
                                     + " has no map in it");
        }
        text = *recipe;
    } else {
        text = read_text(path);
    }
    const JsonValue root = JsonParser(std::move(text)).parse();
    static_cast<void>(require_object(root, "root"));
    MapDefinition result;
    result.source_path = absolute;
    result.name = string_value(root, "name", result.name);
    result.in_game_name = string_value(root, "inGameName", result.in_game_name);
    result.texture_source = string_value(root, "textureSource", result.texture_source);
    result.scale_factor = int_value(root, "scaleFactor", result.scale_factor);
    result.kill_height = float_value(root, "killHeight", result.kill_height);
    result.far_clip = float_value(root, "farClip", result.far_clip);
    result.fog_enabled = bool_value(root, "fogEnabled", result.fog_enabled);
    result.fog_color = array3_value(root, "fogColor", result.fog_color);
    result.fog_slope = int_value(root, "fogSlope", result.fog_slope);
    result.fog_offset = int_value(root, "fogOffset", result.fog_offset);
    result.light1_color = array3_value(root, "light1Color", result.light1_color);
    result.light1_vector = vector_value(root, "light1Vector", result.light1_vector);
    result.light2_color = array3_value(root, "light2Color", result.light2_color);
    result.light2_vector = vector_value(root, "light2Vector", result.light2_vector);
    result.battle_time_limit = uint_value(root, "battleTimeLimit",
                                          result.battle_time_limit);
    result.point_limit = static_cast<std::int16_t>(std::clamp(
        int_value(root, "pointLimit", result.point_limit), -32'768, 32'767));
    if (const JsonValue* preview = member(root, "preview");
        preview != nullptr && preview->kind != JsonValue::Kind::Null) {
        const JsonValue& object = require_object(*preview, "preview");
        result.preview = MapPreview{
            vector_value(object, "position", {}),
            vector_value(object, "target", {})};
    }
    if (const JsonValue* import = import_object(root); import != nullptr) {
        result.import_source = string_value(*import, "source");
        result.import_map_name = string_value(*import, "mapName");
        result.import_units_per_unit = float_value(
            *import, "unitsPerUnit", result.import_units_per_unit);
        result.import_textures = string_value(*import, "textures");
        result.import_default_material = int_value(
            *import, "defaultMaterial", result.import_default_material);
        result.import_tex_scale = float_value(
            *import, "texScale", result.import_tex_scale);
        result.import_keep_sky = bool_value(
            *import, "keepSky", result.import_keep_sky);
        result.import_keep_clip = bool_value(
            *import, "keepClip", result.import_keep_clip);
        result.import_patch_level = int_value(
            *import, "patchLevel", result.import_patch_level);
        result.import_keep_spawns = bool_value(
            *import, "keepSpawns", result.import_keep_spawns);
        if (const JsonValue* mappings = member(*import, "shaderMaterials");
            mappings != nullptr && mappings->kind != JsonValue::Kind::Null) {
            const JsonValue& object = require_object(*mappings, "shaderMaterials");
            for (const auto& [shader, material] : object.object) {
                if (material.kind != JsonValue::Kind::Number
                    || !std::isfinite(material.number)
                    || std::floor(material.number) != material.number
                    || material.number < static_cast<double>(std::numeric_limits<int>::min())
                    || material.number > static_cast<double>(std::numeric_limits<int>::max())) {
                    throw std::runtime_error(
                        "map JSON field shaderMaterials must contain integer values");
                }
                result.import_shader_materials.emplace_back(
                    shader, static_cast<int>(material.number));
            }
        }
    }
    result.materials = parse_materials(root);
    result.brushes = parse_brushes(root);
    result.spawns = parse_spawns(root);
    result.jump_pads = parse_jump_pads(root);
    result.items = parse_items(root);
    if (result.name.empty()) {
        throw std::runtime_error("map name must not be empty");
    }
    return result;
}

std::string serialize_definition(const MapDefinition& definition) {
    std::ostringstream output;
    output << std::setprecision(9) << std::defaultfloat;
    output << "{\n";
    bool first = true;
    const auto field = [&](std::string_view name, const std::string& value) {
        if (!first) {
            output << ",\n";
        }
        output << "  " << json_quote(name) << ": " << value;
        first = false;
    };
    field("name", json_quote(definition.name));
    if (!definition.in_game_name.empty()) {
        field("inGameName", json_quote(definition.in_game_name));
    }
    field("textureSource", json_quote(definition.texture_source));
    field("scaleFactor", std::to_string(definition.scale_factor));
    field("killHeight", std::to_string(definition.kill_height));
    field("farClip", std::to_string(definition.far_clip));
    field("fogEnabled", definition.fog_enabled ? "true" : "false");
    field("fogColor", json_array3(definition.fog_color));
    field("fogSlope", std::to_string(definition.fog_slope));
    field("fogOffset", std::to_string(definition.fog_offset));
    field("light1Color", json_array3(definition.light1_color));
    field("light1Vector", json_vec3(definition.light1_vector));
    field("light2Color", json_array3(definition.light2_color));
    field("light2Vector", json_vec3(definition.light2_vector));
    field("battleTimeLimit", std::to_string(definition.battle_time_limit));
    field("pointLimit", std::to_string(definition.point_limit));
    if (definition.preview.has_value()) {
        field("preview", "{\n    \"position\": "
              + json_vec3(definition.preview->position)
              + ",\n    \"target\": "
              + json_vec3(definition.preview->target) + "\n  }");
    }
    if (!definition.import_source.empty()) {
        field("import", json_import(definition));
    }
    field("materials", json_materials(definition.materials));
    field("brushes", json_brushes(definition.brushes));
    field("spawns", json_spawns(definition.spawns));
    field("jumpPads", json_jump_pads(definition.jump_pads));
    field("items", json_items(definition.items));
    output << "\n}\n";
    return output.str();
}

int item_type_from_name(const std::string& name) {
    const std::string value = lower(name);
    static constexpr std::array<std::pair<std::string_view, int>, 19> items{{
        {"healthmedium", 0}, {"healthsmall", 1}, {"healthbig", 2},
        {"doubledamage", 3}, {"voltdriver", 5}, {"battlehammer", 7},
        {"imperialist", 8}, {"judicator", 9}, {"magmaul", 10},
        {"shockcoil", 11}, {"omegacannon", 12}, {"uasmall", 13},
        {"uabig", 14}, {"missilesmall", 15}, {"missilebig", 16},
        {"cloak", 17}, {"deathalt", 20}, {"affinityweapon", 21},
        {"pickwpnmissile", 22}
    }};
    for (const auto& [candidate, type] : items) {
        if (value == candidate) {
            return type;
        }
    }
    return -1;
}

std::string file_prefix(const MapDefinition& definition) {
    // MapPacker.cs, CustomRooms.cs and Q3Convert.cs all use
    // Name.ToLowerInvariant() verbatim.  Spaces and punctuation therefore
    // remain part of the generated filename; replacing them was a native-only
    // convention and made the two implementations address different files.
    return lower(definition.name);
}

} // namespace fruityprime::mapgen
