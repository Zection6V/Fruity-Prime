/*
 * Native counterpart of MphRead/Export/Scripting.cs.
 *
 * The exporter writes the same small Python control layer as the managed
 * exporter. Geometry still comes from the neighbouring native COLLADA
 * exporter; this file owns recolors, materials, node setup, vertex groups,
 * and all four animation families consumed by mph_common.py.
 */
#include "Export/export.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <locale>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace fruityprime::exporter {
namespace {

constexpr float Pi = 3.14159265358979323846F;

void append_line(std::string& output, std::string_view line) {
    output.append(line);
    output.push_back('\n');
}

void append_indent(std::string& output, std::string_view text,
                   int indent = 1) {
    std::size_t begin = 0;
    for (;;) {
        const std::size_t end = text.find('\n', begin);
        output.append(static_cast<std::size_t>(std::max(0, indent) * 4), ' ');
        if (end == std::string_view::npos) {
            output.append(text.substr(begin));
            output.push_back('\n');
            return;
        }
        output.append(text.substr(begin, end - begin));
        output.push_back('\n');
        begin = end + 1;
    }
}

[[nodiscard]] std::string python_quote(std::string_view value) {
    std::string result;
    result.reserve(value.size() + 2);
    result.push_back('\'');
    for (const char character : value) {
        switch (character) {
        case '\\': result += "\\\\"; break;
        case '\'': result += "\\'"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default: result.push_back(character); break;
        }
    }
    result.push_back('\'');
    return result;
}

[[nodiscard]] std::string format_float(float value) {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(9) << value;
    return stream.str();
}

[[nodiscard]] std::string format_rgb(const model::ColorRgb& color) {
    return format_float(static_cast<float>(color.red) / 31.0F) + ", "
        + format_float(static_cast<float>(color.green) / 31.0F) + ", "
        + format_float(static_cast<float>(color.blue) / 31.0F);
}

[[nodiscard]] std::string material_script_name(const model::Material& material) {
    return material.name.empty() ? "null" : material.name;
}

[[nodiscard]] bool has_color_instruction(const model::File& model,
                                          const model::Mesh& mesh) {
    if (mesh.display_list_id >= model.instructions().size()) {
        throw std::out_of_range("model mesh references an invalid display list");
    }
    const auto& instructions = model.instructions()[mesh.display_list_id];
    return std::any_of(instructions.begin(), instructions.end(),
                       [](const model::RenderInstruction& instruction) {
        return instruction.code == model::InstructionCode::Color;
    });
}

[[nodiscard]] bool has_alpha_pixels(const model::File& model,
                                    const model::Material& material) {
    if (material.texture_id < 0) {
        return false;
    }
    if (static_cast<std::size_t>(material.texture_id)
        >= model.textures().size()) {
        throw std::out_of_range("material references an invalid texture");
    }
    const auto pixels = model.decode_texture(
        static_cast<std::size_t>(material.texture_id));
    return std::any_of(pixels.begin(), pixels.end(),
                       [](const model::TexturePixel& pixel) {
        return pixel.alpha < 255;
    });
}

[[nodiscard]] std::vector<std::size_t> node_mesh_ids(
    const model::File& model, const model::Node& node) {
    const std::size_t first = node.mesh_id / 2;
    const std::size_t count = node.mesh_count;
    if (first > model.meshes().size()
        || count > model.meshes().size() - first) {
        throw std::out_of_range("node mesh range is outside the model");
    }
    std::vector<std::size_t> result;
    result.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        result.push_back(first + index);
    }
    return result;
}

[[nodiscard]] std::vector<model::GeometryVertex> mesh_vertices(
    const model::File& model, std::size_t mesh_index) {
    if (mesh_index >= model.meshes().size()) {
        throw std::out_of_range("mesh index is outside the model");
    }
    const auto& mesh = model.meshes()[mesh_index];
    if (mesh.display_list_id >= model.instructions().size()) {
        throw std::out_of_range("mesh display list is outside the model");
    }
    int texture_width = 0;
    int texture_height = 0;
    bool texgen = false;
    if (mesh.material_id < model.materials().size()) {
        const auto& material = model.materials()[mesh.material_id];
        texgen = material.texcoord_transform_mode == 2;
        if (material.texture_id >= 0
            && static_cast<std::size_t>(material.texture_id)
                < model.textures().size()) {
            const auto& texture = model.textures()[material.texture_id];
            texture_width = texture.width;
            texture_height = texture.height;
        }
    }
    std::vector<model::GeometryVertex> result;
    for (const auto& primitive : model.decode_geometry(
             mesh.display_list_id, texture_width, texture_height, texgen)) {
        result.insert(result.end(), primitive.vertices.begin(),
                      primitive.vertices.end());
    }
    return result;
}

void append_uv_animations(const model::File& model, std::string& output) {
    append_indent(output, "uv_anims = [");
    for (const auto& group : model.animations().texcoord_groups) {
        if (group.animations.empty()) {
            continue;
        }
        append_indent(output, "{", 2);
        for (const auto& [name, animation] : group.animations) {
            append_indent(output, python_quote(name) + ":", 3);
            append_indent(output, "[", 3);
            for (std::uint32_t frame = 0; frame < group.frame_count; ++frame) {
                const int current = static_cast<int>(frame);
                const float scale_s = model::interpolate_animation(
                    group.scales, animation.scale_lut_index_s, current,
                    animation.scale_blend_s, animation.scale_lut_length_s,
                    group.frame_count);
                const float scale_t = model::interpolate_animation(
                    group.scales, animation.scale_lut_index_t, current,
                    animation.scale_blend_t, animation.scale_lut_length_t,
                    group.frame_count);
                const float rotate = model::interpolate_animation(
                    group.rotations, animation.rotate_lut_index_z, current,
                    animation.rotate_blend_z, animation.rotate_lut_length_z,
                    group.frame_count, true);
                const float translate_s = model::interpolate_animation(
                    group.translations, animation.translate_lut_index_s,
                    current, animation.translate_blend_s,
                    animation.translate_lut_length_s, group.frame_count);
                const float translate_t = model::interpolate_animation(
                    group.translations, animation.translate_lut_index_t,
                    current, animation.translate_blend_t,
                    animation.translate_lut_length_t, group.frame_count);
                append_indent(output,
                    "[" + format_float(scale_s) + ", "
                        + format_float(scale_t) + ", "
                        + format_float(rotate) + ", "
                        + format_float(translate_s) + ", "
                        + format_float(translate_t) + "],", 4);
            }
            append_indent(output, "],", 3);
        }
        append_indent(output, "},", 2);
    }
    append_indent(output, "]");
}

void append_material_animations(const model::File& model,
                                std::string& output) {
    append_indent(output, "mat_anims = [");
    for (const auto& group : model.animations().material_groups) {
        if (group.animations.empty()) {
            continue;
        }
        append_indent(output, "{", 2);
        for (const auto& [name, animation] : group.animations) {
            append_indent(output, python_quote(name + "_mat") + ":", 3);
            append_indent(output, "[", 3);
            for (std::uint32_t frame = 0; frame < group.frame_count; ++frame) {
                const int current = static_cast<int>(frame);
                const float red = model::interpolate_animation(
                    group.colors, animation.diffuse_lut_index_r, current,
                    animation.diffuse_blend_r,
                    animation.diffuse_lut_length_r, group.frame_count);
                const float green = model::interpolate_animation(
                    group.colors, animation.diffuse_lut_index_g, current,
                    animation.diffuse_blend_g,
                    animation.diffuse_lut_length_g, group.frame_count);
                const float blue = model::interpolate_animation(
                    group.colors, animation.diffuse_lut_index_b, current,
                    animation.diffuse_blend_b,
                    animation.diffuse_lut_length_b, group.frame_count);
                const float alpha = model::interpolate_animation(
                    group.colors, animation.alpha_lut_index, current,
                    animation.alpha_blend, animation.alpha_lut_length,
                    group.frame_count);
                append_indent(output,
                    "[" + format_float(red / 31.0F) + ", "
                        + format_float(green / 31.0F) + ", "
                        + format_float(blue / 31.0F) + ", "
                        + format_float(alpha / 31.0F) + "],", 4);
            }
            append_indent(output, "],", 3);
        }
        append_indent(output, "},", 2);
    }
    append_indent(output, "]");
}

void append_texture_animations(const model::File& model,
                               std::string& output) {
    append_indent(output, "tex_anims = [");
    std::vector<std::pair<std::uint16_t, std::uint16_t>> combinations;
    for (const auto& group : model.animations().texture_groups) {
        for (const auto& [unused_name, animation] : group.animations) {
            static_cast<void>(unused_name);
            const std::size_t start = animation.start_index;
            const std::size_t count = animation.count;
            if (start > group.texture_ids.size()
                || count > group.texture_ids.size() - start
                || start > group.palette_ids.size()
                || count > group.palette_ids.size() - start) {
                throw std::out_of_range(
                    "texture animation range is outside the model");
            }
            for (std::size_t index = start; index < start + count; ++index) {
                const std::pair value{group.texture_ids[index],
                                      group.palette_ids[index]};
                if (std::find(combinations.begin(), combinations.end(), value)
                    == combinations.end()) {
                    combinations.push_back(value);
                }
            }
        }
    }

    for (const auto& group : model.animations().texture_groups) {
        if (group.animations.empty()) {
            continue;
        }
        append_indent(output, "{", 2);
        for (const auto& [name, animation] : group.animations) {
            append_indent(output, python_quote(name + "_mat") + ":", 3);
            append_indent(output, "[", 3);
            const std::size_t start = animation.start_index;
            const std::size_t count = animation.count;
            if (start > group.frame_indices.size()
                || count > group.frame_indices.size() - start
                || start > group.texture_ids.size()
                || count > group.texture_ids.size() - start
                || start > group.palette_ids.size()
                || count > group.palette_ids.size() - start) {
                throw std::out_of_range(
                    "texture animation range is outside the model");
            }
            for (std::size_t index = start; index < start + count; ++index) {
                const std::pair value{group.texture_ids[index],
                                      group.palette_ids[index]};
                const auto found = std::find(combinations.begin(),
                                             combinations.end(), value);
                if (found == combinations.end()) {
                    throw std::logic_error(
                        "texture animation combination was not indexed");
                }
                const auto combination = static_cast<std::size_t>(
                    found - combinations.begin());
                append_indent(output,
                    "[" + std::to_string(group.frame_indices[index]) + ", "
                        + std::to_string(combination) + "],", 4);
            }
            append_indent(output, "],", 3);
        }
        append_indent(output, "},", 2);
    }
    append_indent(output, "]");
}

void append_node_animations(const model::File& model, std::string& output) {
    append_indent(output, "node_anims = [");
    for (const auto& group : model.animations().node_groups) {
        if (group.animations.empty()) {
            continue;
        }
        append_indent(output, "{", 2);
        for (const auto& [name, animation] : group.animations) {
            append_indent(output, python_quote(name) + ":", 3);
            append_indent(output, "[", 3);
            for (std::uint32_t frame = 0; frame < group.frame_count; ++frame) {
                const int current = static_cast<int>(frame);
                const float scale_x = model::interpolate_animation(
                    group.scales, animation.scale_lut_index_x, current,
                    animation.scale_blend_x, animation.scale_lut_length_x,
                    group.frame_count);
                const float scale_y = model::interpolate_animation(
                    group.scales, animation.scale_lut_index_y, current,
                    animation.scale_blend_y, animation.scale_lut_length_y,
                    group.frame_count);
                const float scale_z = model::interpolate_animation(
                    group.scales, animation.scale_lut_index_z, current,
                    animation.scale_blend_z, animation.scale_lut_length_z,
                    group.frame_count);
                const float rotate_x = model::interpolate_animation(
                    group.rotations, animation.rotate_lut_index_x, current,
                    animation.rotate_blend_x, animation.rotate_lut_length_x,
                    group.frame_count, true);
                const float rotate_y = model::interpolate_animation(
                    group.rotations, animation.rotate_lut_index_y, current,
                    animation.rotate_blend_y, animation.rotate_lut_length_y,
                    group.frame_count, true);
                const float rotate_z = model::interpolate_animation(
                    group.rotations, animation.rotate_lut_index_z, current,
                    animation.rotate_blend_z, animation.rotate_lut_length_z,
                    group.frame_count, true);
                const float translate_x = model::interpolate_animation(
                    group.translations, animation.translate_lut_index_x,
                    current, animation.translate_blend_x,
                    animation.translate_lut_length_x, group.frame_count);
                const float translate_y = model::interpolate_animation(
                    group.translations, animation.translate_lut_index_y,
                    current, animation.translate_blend_y,
                    animation.translate_lut_length_y, group.frame_count);
                const float translate_z = model::interpolate_animation(
                    group.translations, animation.translate_lut_index_z,
                    current, animation.translate_blend_z,
                    animation.translate_lut_length_z, group.frame_count);
                append_indent(output,
                    "[" + format_float(scale_x) + ", "
                        + format_float(scale_y) + ", "
                        + format_float(scale_z) + ", "
                        + format_float(rotate_x) + ", "
                        + format_float(rotate_y) + ", "
                        + format_float(rotate_z) + ", "
                        + format_float(translate_x) + ", "
                        + format_float(translate_y) + ", "
                        + format_float(translate_z) + "],", 4);
            }
            append_indent(output, "],", 3);
        }
        append_indent(output, "},", 2);
    }
    append_indent(output, "]");
}

[[nodiscard]] std::string dae_path(const ScriptingOptions& options) {
    const auto relative = options.export_root / options.model_name
        / (options.model_name + "_{suffix}.dae");
    std::error_code error;
    const auto absolute = std::filesystem::absolute(relative, error);
    return (error ? relative : absolute).generic_string();
}

void append_bone_setup(const model::File& model, std::string& output) {
    append_line(output, "def bone_setup():");
    append_indent(output, "bpy.ops.object.mode_set(mode = 'OBJECT')");
    append_indent(output,
        "bpy.ops.armature_add(enter_editmode=True, align='WORLD', "
        "location=(0, 0, 0))");
    append_indent(output, "bpy.ops.armature.select_all(action='SELECT')");
    append_indent(output, "bpy.ops.armature.delete()");
    for (const auto& node : model.nodes()) {
        append_indent(output,
            "bpy.ops.armature.bone_primitive_add(name="
                + python_quote(node.name) + ")");
    }
    append_indent(output, "bpy.ops.armature.select_all(action='DESELECT')");
    append_indent(output, "bones = bpy.data.armatures[0].edit_bones");
    for (const auto& node : model.nodes()) {
        if (node.parent_id < 0) {
            continue;
        }
        if (static_cast<std::size_t>(node.parent_id) >= model.nodes().size()) {
            throw std::out_of_range("node parent is outside the model");
        }
        const auto& parent = model.nodes()[node.parent_id];
        append_indent(output,
            "bones.get(" + python_quote(node.name) + ").parent = "
                "bones.get(" + python_quote(parent.name) + ")");
    }
    append_indent(output, "bpy.ops.object.editmode_toggle()");
    append_indent(output, "bpy.ops.object.select_all(action='DESELECT')");
    append_indent(output,
        "for obj in bpy.data.objects:\n"
        "    if obj.type == 'MESH':\n"
        "        obj.select_set(True)");
    append_indent(output, "bpy.data.objects['Armature'].select_set(True)");
    append_indent(output, "bpy.ops.object.parent_set(type='ARMATURE_NAME')");

    const auto& weights = model.node_weights();
    for (std::size_t mesh_index = 0; mesh_index < model.meshes().size();
         ++mesh_index) {
        const auto vertices = mesh_vertices(model, mesh_index);
        std::map<std::size_t, std::vector<std::size_t>> groups;
        for (std::size_t vertex_index = 0; vertex_index < vertices.size();
             ++vertex_index) {
            const auto matrix_id = vertices[vertex_index].matrix_id;
            if (static_cast<std::size_t>(matrix_id) >= weights.size()) {
                throw std::out_of_range(
                    "model vertex references an invalid matrix weight");
            }
            const auto node_id = weights[matrix_id];
            if (node_id < 0
                || static_cast<std::size_t>(node_id) >= model.nodes().size()) {
                throw std::out_of_range(
                    "model matrix weight references an invalid node");
            }
            groups[static_cast<std::size_t>(node_id)].push_back(vertex_index);
        }
        append_indent(output, "bpy.ops.object.select_all(action='DESELECT')");
        append_indent(output,
            "obj = bpy.data.objects["
                + python_quote("geom" + std::to_string(mesh_index + 1)
                              + "_obj")
                + "]");
        append_indent(output, "obj.select_set(True)");
        for (const auto& [node_id, indices] : groups) {
            const auto& node = model.nodes()[node_id];
            std::string values;
            for (std::size_t index = 0; index < indices.size(); ++index) {
                if (index != 0) {
                    values += ", ";
                }
                values += std::to_string(indices[index]);
            }
            append_indent(output,
                "group = obj.vertex_groups[" + python_quote(node.name)
                    + "]");
            append_indent(output,
                "group.add([" + values + "], 1.0, 'ADD')");
        }
    }

    for (const auto& node : model.nodes()) {
        append_indent(output,
            "bone = bpy.data.objects['Armature'].pose.bones["
                + python_quote(node.name) + "]");
        append_indent(output, "bone.rotation_mode = 'XYZ'");
        const auto scale = formats::Vector3{
            node.scale.x.to_float(), node.scale.y.to_float(),
            node.scale.z.to_float()};
        if (scale.x != 1.0F || scale.y != 1.0F || scale.z != 1.0F) {
            append_indent(output,
                "bone.scale = mathutils.Vector((" + format_float(scale.x)
                    + ", " + format_float(scale.y) + ", "
                    + format_float(scale.z) + "))");
        }
        const auto angle = formats::Vector3{
            static_cast<float>(node.angle_x) / 65536.0F * 2.0F * Pi,
            static_cast<float>(node.angle_y) / 65536.0F * 2.0F * Pi,
            static_cast<float>(node.angle_z) / 65536.0F * 2.0F * Pi};
        if (angle.x != 0.0F || angle.y != 0.0F || angle.z != 0.0F) {
            append_indent(output,
                "bone.rotation_euler = mathutils.Vector(("
                    + format_float(angle.x) + ", " + format_float(angle.y)
                    + ", " + format_float(angle.z) + "))");
        }
        const auto position = formats::Vector3{
            node.position.x.to_float(), node.position.y.to_float(),
            node.position.z.to_float()};
        if (position.x != 0.0F || position.y != 0.0F || position.z != 0.0F) {
            append_indent(output,
                "bone.location = mathutils.Vector(("
                    + format_float(position.x) + ", "
                    + format_float(position.y) + ", "
                    + format_float(position.z) + "))");
        }
    }
}

} // namespace

std::string generate_script(const model::File& model,
                            const ScriptingOptions& options) {
    ScriptingOptions effective = options;
    effective.model_name = options.model_name.empty() ? "model"
                                                       : options.model_name;
    const std::vector<std::string> recolors = options.recolor_names.empty()
        ? std::vector<std::string>{"default"} : options.recolor_names;

    std::string output;
    output.reserve(32 * 1024);
    append_line(output, "import bpy");
    append_line(output, "import math");
    append_line(output, "import mathutils");
    append_line(output, "from mph_common import *");
    append_line(output, "");
    append_line(output, "export_version = "
        + python_quote(options.export_version));
    std::string recolor_comment = "# recolors: ";
    for (std::size_t index = 0; index < recolors.size(); ++index) {
        if (index != 0) {
            recolor_comment += ", ";
        }
        recolor_comment += recolors[index];
    }
    append_line(output, recolor_comment);
    append_line(output, "recolor = " + python_quote(recolors.front()));
    const auto active_count = [](const auto& groups) {
        return static_cast<std::size_t>(std::count_if(
            groups.begin(), groups.end(), [](const auto& group) {
                return !group.animations.empty();
            }));
    };
    const std::size_t uv_count = active_count(model.animations().texcoord_groups);
    const std::size_t mat_count = active_count(model.animations().material_groups);
    const std::size_t node_count = active_count(model.animations().node_groups);
    const std::size_t tex_count = active_count(model.animations().texture_groups);
    append_line(output, "# uv anims: " + std::to_string(uv_count)
        + ", mat anims: " + std::to_string(mat_count)
        + ", node anims: " + std::to_string(node_count)
        + ", tex anims: " + std::to_string(tex_count));
    append_line(output, "uv_index = " + std::to_string(uv_count > 0 ? 0 : -1));
    append_line(output, "mat_index = " + std::to_string(mat_count > 0 ? 0 : -1));
    append_line(output, "node_index = " + std::to_string(node_count > 0 ? 0 : -1));
    append_line(output, "tex_index = " + std::to_string(tex_count > 0 ? 0 : -1));
    append_line(output, "");

    append_line(output, "def import_dae(suffix):");
    append_indent(output, "cleanup()");
    append_indent(output, "bpy.ops.wm.collada_import(filepath =");
    append_indent(output, "fr\"" + dae_path(effective) + "\"");
    append_indent(output, "set_common()");

    std::vector<std::size_t> invert_mesh_ids;
    for (std::size_t material_index = 0;
         material_index < model.materials().size(); ++material_index) {
        const auto& material = model.materials()[material_index];
        const std::string name = material_script_name(material);
        if (material.texture_id != -1) {
            append_indent(output,
                "set_texture_alpha(" + python_quote(name + "_mat") + ", "
                    + std::to_string(material.alpha) + ", "
                    + (has_alpha_pixels(model, material) ? "True" : "False")
                    + ")");
            const bool mirror_x = material.x_repeat == 2;
            const bool mirror_y = material.y_repeat == 2;
            if (mirror_x || mirror_y) {
                append_indent(output,
                    "set_mirror(" + python_quote(name + "_mat") + ", "
                        + (mirror_x ? "True" : "False") + ", "
                        + (mirror_y ? "True" : "False") + ")");
            }
        } else {
            append_indent(output,
                "set_material_alpha(" + python_quote(name + "_mat") + ", "
                    + std::to_string(material.alpha) + ")");
        }
        if (material.culling == 1 || material.culling == 2) {
            append_indent(output,
                "set_back_culling(" + python_quote(name + "_mat") + ")");
            if (material.culling == 1) {
                for (std::size_t mesh_index = 0;
                     mesh_index < model.meshes().size(); ++mesh_index) {
                    if (model.meshes()[mesh_index].material_id
                        == material_index) {
                        invert_mesh_ids.push_back(mesh_index);
                    }
                }
            }
        }

        std::vector<std::size_t> with_color;
        std::vector<std::size_t> without_color;
        for (std::size_t mesh_index = 0;
             mesh_index < model.meshes().size(); ++mesh_index) {
            if (model.meshes()[mesh_index].material_id != material_index) {
                continue;
            }
            (has_color_instruction(model, model.meshes()[mesh_index])
                 ? with_color : without_color).push_back(mesh_index);
        }
        if (!without_color.empty()) {
            const std::string color = format_rgb(material.diffuse);
            if (!with_color.empty()) {
                std::string objects = "[";
                for (std::size_t index = 0; index < without_color.size();
                     ++index) {
                    if (index != 0) {
                        objects += ", ";
                    }
                    // Preserve the managed exporter’s zero-based lookup in
                    // this branch; the other object names are one-based.
                    objects += python_quote(
                        "geom" + std::to_string(without_color[index])
                        + "_obj");
                }
                objects += "]";
                append_indent(output,
                    "set_mat_color(" + python_quote(name + "_mat") + ", "
                        + color + ", True, " + objects + ")");
            } else {
                append_indent(output,
                    "set_mat_color(" + python_quote(name + "_mat") + ", "
                        + color + ", False, [])");
            }
        }
    }

    for (const auto& node : model.nodes()) {
        for (const std::size_t mesh_index : node_mesh_ids(model, node)) {
            if (std::find(invert_mesh_ids.begin(), invert_mesh_ids.end(),
                          mesh_index) != invert_mesh_ids.end()) {
                append_indent(output,
                    "invert_normals(" + python_quote(
                        "geom" + std::to_string(mesh_index + 1) + "_obj")
                        + ")");
            }
        }
    }
    if (!model.node_weights().empty()) {
        append_indent(output, "bone_setup()");
    }
    append_indent(output, "anim_setup()");
    for (const auto& node : model.nodes()) {
        if (node.billboard_mode == 0) {
            continue;
        }
        for (const std::size_t mesh_index : node_mesh_ids(model, node)) {
            append_indent(output,
                "set_billboard(" + python_quote(
                    "geom" + std::to_string(mesh_index + 1) + "_obj")
                    + ", " + std::to_string(node.billboard_mode) + ")");
        }
    }

    if (!model.node_weights().empty()) {
        append_line(output, "");
        append_bone_setup(model, output);
    }
    append_line(output, "");
    // The tables are module globals so import_dae can select one before it
    // installs the corresponding Blender keyframes.
    append_uv_animations(model, output);
    append_material_animations(model, output);
    append_texture_animations(model, output);
    append_node_animations(model, output);
    append_line(output, "");
    append_line(output, "def anim_setup():");
    append_indent(output, "bpy.context.scene.render.fps = 30");
    append_indent(output, "if uv_index >= 0:");
    append_indent(output, "set_uv_anims(uv_anims[uv_index])", 2);
    append_indent(output, "if tex_index >= 0:");
    append_indent(output, "set_tex_anims(tex_anims[tex_index])", 2);
    append_indent(output, "if node_index >= 0:");
    append_indent(output, "set_node_anims(node_anims[node_index])", 2);
    append_indent(output, "if mat_index >= 0:");
    append_indent(output, "set_mat_anims(mat_anims[mat_index])", 2);
    append_line(output, "");
    append_line(output, "if __name__ == '__main__':");
    append_indent(output, "validate_version(export_version)");
    append_indent(output, "import_dae(recolor)");
    return output;
}

void write_script(const model::File& model,
                  const std::filesystem::path& output_path,
                  const ScriptingOptions& options) {
    if (output_path.empty()) {
        throw std::invalid_argument("script output path is empty");
    }
    if (output_path.has_parent_path()) {
        std::error_code error;
        std::filesystem::create_directories(output_path.parent_path(), error);
        if (error) {
            throw std::runtime_error(
                "could not create script output directory: "
                + error.message());
        }
    }
    const std::string script = generate_script(model, options);
    std::ofstream stream(output_path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        throw std::runtime_error("could not create script file: "
                                 + output_path.string());
    }
    stream.write(script.data(), static_cast<std::streamsize>(script.size()));
    if (!stream) {
        throw std::runtime_error("could not write script file: "
                                 + output_path.string());
    }
}

} // namespace fruityprime::exporter
