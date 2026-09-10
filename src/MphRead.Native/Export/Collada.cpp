#include "Export/export.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <initializer_list>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::exporter {
namespace {

[[nodiscard]] std::string material_name(const model::Material& material,
                                        std::size_t index) {
    std::string result;
    result.reserve(material.name.size() + 16);
    for (const char value : material.name) {
        const auto character = static_cast<unsigned char>(value);
        if (std::isalnum(character) != 0 || value == '_' || value == '-') {
            result.push_back(value);
        } else {
            result.push_back('_');
        }
    }
    if (result.empty()) {
        result = "material";
    }
    result += '_' + std::to_string(index);
    return result;
}

void write_face(std::ofstream& output, std::size_t first,
                std::initializer_list<std::size_t> offsets) {
    if (offsets.size() < 3) {
        return;
    }
    output << "f";
    for (const std::size_t offset : offsets) {
        const std::size_t index = first + offset;
        output << ' ' << index << '/' << index << '/' << index;
    }
    output << '\n';
}

std::size_t write_primitive_faces(
    std::ofstream& output, model::GeometryPrimitiveType type,
    std::size_t first, std::size_t vertex_count) {
    std::size_t faces = 0;
    switch (type) {
    case model::GeometryPrimitiveType::Triangles:
        for (std::size_t i = 0; i + 2 < vertex_count; i += 3) {
            write_face(output, first, {i, i + 1, i + 2});
            ++faces;
        }
        break;
    case model::GeometryPrimitiveType::Quads:
        for (std::size_t i = 0; i + 3 < vertex_count; i += 4) {
            write_face(output, first, {i, i + 1, i + 2, i + 3});
            ++faces;
        }
        break;
    case model::GeometryPrimitiveType::TriangleStrip:
        for (std::size_t i = 0; i + 2 < vertex_count; ++i) {
            if ((i & 1u) == 0) {
                write_face(output, first, {i, i + 1, i + 2});
            } else {
                write_face(output, first, {i + 1, i, i + 2});
            }
            ++faces;
        }
        break;
    case model::GeometryPrimitiveType::QuadStrip:
        for (std::size_t i = 0; i + 3 < vertex_count; i += 2) {
            write_face(output, first, {i, i + 1, i + 3, i + 2});
            ++faces;
        }
        break;
    }
    return faces;
}

[[nodiscard]] std::string xml_escape(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const char character : value) {
        switch (character) {
        case '&': result += "&amp;"; break;
        case '<': result += "&lt;"; break;
        case '>': result += "&gt;"; break;
        case '"': result += "&quot;"; break;
        case '\'': result += "&apos;"; break;
        default: result.push_back(character); break;
        }
    }
    return result;
}

[[nodiscard]] std::string collada_material_id(
    const model::Material& material, std::size_t index) {
    return "material_" + material_name(material, index);
}

[[nodiscard]] std::string collada_effect_id(
    const model::Material& material, std::size_t index) {
    return "effect_" + material_name(material, index);
}

struct ColladaTriangle {
    std::size_t a = 0;
    std::size_t b = 0;
    std::size_t c = 0;
};

void append_collada_triangles(model::GeometryPrimitiveType type,
                              std::size_t first, std::size_t count,
                              std::vector<ColladaTriangle>& triangles) {
    switch (type) {
    case model::GeometryPrimitiveType::Triangles:
        for (std::size_t i = 0; i + 2 < count; i += 3) {
            triangles.push_back({first + i, first + i + 1, first + i + 2});
        }
        break;
    case model::GeometryPrimitiveType::Quads:
        for (std::size_t i = 0; i + 3 < count; i += 4) {
            triangles.push_back({first + i, first + i + 1, first + i + 2});
            triangles.push_back({first + i, first + i + 2, first + i + 3});
        }
        break;
    case model::GeometryPrimitiveType::TriangleStrip:
        for (std::size_t i = 0; i + 2 < count; ++i) {
            if ((i & 1u) == 0) {
                triangles.push_back({first + i, first + i + 1,
                                     first + i + 2});
            } else {
                triangles.push_back({first + i + 1, first + i,
                                     first + i + 2});
            }
        }
        break;
    case model::GeometryPrimitiveType::QuadStrip:
        for (std::size_t i = 0; i + 3 < count; i += 2) {
            triangles.push_back({first + i, first + i + 1,
                                 first + i + 3});
            triangles.push_back({first + i, first + i + 3,
                                 first + i + 2});
        }
        break;
    }
}

[[nodiscard]] std::string format_float(float value) {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::fixed << std::setprecision(6) << value;
    return stream.str();
}

void write_u16_le(std::ofstream& output, std::uint16_t value) {
    const std::array<char, 2> bytes{
        static_cast<char>(value & 0xffU),
        static_cast<char>((value >> 8) & 0xffU)
    };
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void write_u32_le(std::ofstream& output, std::uint32_t value) {
    const std::array<char, 4> bytes{
        static_cast<char>(value & 0xffU),
        static_cast<char>((value >> 8) & 0xffU),
        static_cast<char>((value >> 16) & 0xffU),
        static_cast<char>((value >> 24) & 0xffU)
    };
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void write_mtl(const model::File& model, const std::filesystem::path& path) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("could not create OBJ material file: "
                                 + path.string());
    }
    output << std::fixed << std::setprecision(6);
    for (std::size_t i = 0; i < model.materials().size(); ++i) {
        const auto& material = model.materials()[i];
        const auto name = material_name(material, i);
        output << "newmtl " << name << '\n'
               << "Kd "
               << static_cast<float>(material.diffuse.red) / 255.0F << ' '
               << static_cast<float>(material.diffuse.green) / 255.0F << ' '
               << static_cast<float>(material.diffuse.blue) / 255.0F << '\n'
               << "Ka "
               << static_cast<float>(material.ambient.red) / 255.0F << ' '
               << static_cast<float>(material.ambient.green) / 255.0F << ' '
               << static_cast<float>(material.ambient.blue) / 255.0F << '\n'
               << "Ns 1.000000\n\n";
    }
}

} // namespace

ModelExportStats write_obj(const model::File& model,
                           const std::filesystem::path& output_path) {
    if (output_path.empty()) {
        throw std::invalid_argument("OBJ output path is empty");
    }
    if (output_path.has_parent_path()) {
        std::error_code error;
        std::filesystem::create_directories(output_path.parent_path(), error);
        if (error) {
            throw std::runtime_error("could not create OBJ output directory: "
                                     + error.message());
        }
    }

    std::filesystem::path material_path = output_path;
    material_path.replace_extension(".mtl");
    write_mtl(model, material_path);

    std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("could not create OBJ file: "
                                 + output_path.string());
    }
    output << "# Fruity Prime native model export\n"
           << "mtllib " << material_path.filename().string() << '\n'
           << std::fixed << std::setprecision(9);

    ModelExportStats stats;
    stats.meshes = model.meshes().size();
    for (std::size_t mesh_index = 0; mesh_index < model.meshes().size();
         ++mesh_index) {
        const auto& mesh = model.meshes()[mesh_index];
        if (mesh.display_list_id >= model.instructions().size()) {
            throw std::runtime_error("model mesh references an invalid display list");
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
        const auto primitives = model.decode_geometry(
            mesh.display_list_id, texture_width, texture_height, texgen);
        output << "o mesh_" << mesh_index << '\n';
        if (mesh.material_id < model.materials().size()) {
            output << "usemtl "
                   << material_name(model.materials()[mesh.material_id],
                                    mesh.material_id)
                   << '\n';
        }
        for (const auto& primitive : primitives) {
            const std::size_t first_vertex = stats.vertices + 1;
            ++stats.primitives;
            stats.vertices += primitive.vertices.size();
            for (const auto& vertex : primitive.vertices) {
                output << "v " << vertex.x << ' ' << vertex.y << ' '
                       << vertex.z << '\n';
            }
            for (const auto& vertex : primitive.vertices) {
                output << "vt " << vertex.texture_s << ' '
                       << (1.0F - vertex.texture_t) << '\n';
            }
            for (const auto& vertex : primitive.vertices) {
                output << "vn " << vertex.normal_x << ' ' << vertex.normal_y
                       << ' ' << vertex.normal_z << '\n';
            }
            stats.faces += write_primitive_faces(
                output, primitive.type, first_vertex, primitive.vertices.size());
        }
    }
    return stats;
}

ModelExportStats write_collada(const model::File& model,
                               const std::filesystem::path& output_path) {
    if (output_path.empty()) {
        throw std::invalid_argument("COLLADA output path is empty");
    }
    if (output_path.has_parent_path()) {
        std::error_code error;
        std::filesystem::create_directories(output_path.parent_path(), error);
        if (error) {
            throw std::runtime_error(
                "could not create COLLADA output directory: "
                + error.message());
        }
    }

    std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("could not create COLLADA file: "
                                 + output_path.string());
    }
    output << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
           << "<COLLADA xmlns=\"http://www.collada.org/2005/11/COLLADASchema\" "
              "version=\"1.4.1\">\n"
           << "  <asset><up_axis>Y_UP</up_axis>"
              "<unit name=\"meter\" meter=\"1\"/></asset>\n";

    output << "  <library_effects>\n";
    for (std::size_t i = 0; i < model.materials().size(); ++i) {
        const auto& material = model.materials()[i];
        const auto effect_id = collada_effect_id(material, i);
        const float alpha = static_cast<float>(material.alpha) / 255.0F;
        output << "    <effect id=\"" << xml_escape(effect_id)
               << "\"><profile_COMMON><technique sid=\"common\">"
                  "<phong><diffuse><color>"
               << format_float(static_cast<float>(material.diffuse.red)
                               / 255.0F)
               << ' '
               << format_float(static_cast<float>(material.diffuse.green)
                               / 255.0F)
               << ' '
               << format_float(static_cast<float>(material.diffuse.blue)
                               / 255.0F)
               << ' ' << format_float(alpha)
               << "</color></diffuse><transparency><float>"
               << format_float(alpha)
               << "</float></transparency></phong></technique>"
                  "</profile_COMMON></effect>\n";
    }
    output << "  </library_effects>\n  <library_materials>\n";
    for (std::size_t i = 0; i < model.materials().size(); ++i) {
        const auto& material = model.materials()[i];
        const auto material_id = collada_material_id(material, i);
        output << "    <material id=\"" << xml_escape(material_id)
               << "\" name=\"" << xml_escape(material.name)
               << "\"><instance_effect url=\"#"
               << xml_escape(collada_effect_id(material, i))
               << "\"/></material>\n";
    }
    output << "  </library_materials>\n  <library_geometries>\n";

    ModelExportStats stats;
    stats.meshes = model.meshes().size();
    for (std::size_t mesh_index = 0; mesh_index < model.meshes().size();
         ++mesh_index) {
        const auto& mesh = model.meshes()[mesh_index];
        if (mesh.display_list_id >= model.instructions().size()) {
            throw std::runtime_error(
                "model mesh references an invalid display list");
        }
        const model::Material* material = nullptr;
        if (mesh.material_id < model.materials().size()) {
            material = &model.materials()[mesh.material_id];
        }
        int texture_width = 0;
        int texture_height = 0;
        bool texgen = false;
        if (material != nullptr) {
            texgen = material->texcoord_transform_mode == 2;
            if (material->texture_id >= 0
                && static_cast<std::size_t>(material->texture_id)
                    < model.textures().size()) {
                const auto& texture = model.textures()[material->texture_id];
                texture_width = texture.width;
                texture_height = texture.height;
            }
        }
        const auto primitives = model.decode_geometry(
            mesh.display_list_id, texture_width, texture_height, texgen);
        std::vector<model::GeometryVertex> vertices;
        std::vector<ColladaTriangle> triangles;
        for (const auto& primitive : primitives) {
            const std::size_t first = vertices.size();
            vertices.insert(vertices.end(), primitive.vertices.begin(),
                            primitive.vertices.end());
            append_collada_triangles(primitive.type, first,
                                     primitive.vertices.size(), triangles);
            ++stats.primitives;
        }
        stats.vertices += vertices.size();
        stats.faces += triangles.size();

        const std::string geometry_id = "geometry_" + std::to_string(mesh_index);
        const std::string positions_id = geometry_id + "_positions";
        const std::string normals_id = geometry_id + "_normals";
        const std::string uvs_id = geometry_id + "_uvs";
        const std::string colors_id = geometry_id + "_colors";
        output << "    <geometry id=\"" << geometry_id
               << "\" name=\"mesh_" << mesh_index << "\"><mesh>\n";
        const auto write_source = [&output, &vertices](
            std::string_view source_id, std::string_view array_id,
            std::string_view params, auto value_writer) {
            output << "      <source id=\"" << source_id << "\"><float_array id=\""
                   << array_id << "\" count=\"" << vertices.size() * 3
                   << "\">";
            for (const auto& vertex : vertices) {
                value_writer(vertex);
            }
            output << "</float_array><technique_common><accessor source=\"#"
                   << array_id << "\" count=\"" << vertices.size()
                   << "\" stride=\"3\">" << params
                   << "</accessor></technique_common></source>\n";
        };
        write_source(positions_id, positions_id,
                     "<param name=\"X\" type=\"float\"/><param name=\"Y\" "
                     "type=\"float\"/><param name=\"Z\" type=\"float\"/>",
                     [&output](const model::GeometryVertex& vertex) {
                         output << format_float(vertex.x) << ' '
                                << format_float(vertex.y) << ' '
                                << format_float(vertex.z) << ' ';
                     });
        write_source(normals_id, normals_id,
                     "<param name=\"X\" type=\"float\"/><param name=\"Y\" "
                     "type=\"float\"/><param name=\"Z\" type=\"float\"/>",
                     [&output](const model::GeometryVertex& vertex) {
                         output << format_float(vertex.normal_x) << ' '
                                << format_float(vertex.normal_y) << ' '
                                << format_float(vertex.normal_z) << ' ';
                     });
        write_source(uvs_id, uvs_id,
                     "<param name=\"S\" type=\"float\"/><param name=\"T\" "
                     "type=\"float\"/><param name=\"R\" type=\"float\"/>",
                     [&output](const model::GeometryVertex& vertex) {
                         output << format_float(vertex.texture_s) << ' '
                                << format_float(1.0F - vertex.texture_t)
                                << " 0 ";
                     });
        write_source(colors_id, colors_id,
                     "<param name=\"R\" type=\"float\"/><param name=\"G\" "
                     "type=\"float\"/><param name=\"B\" type=\"float\"/>",
                     [&output](const model::GeometryVertex& vertex) {
                         const auto red = static_cast<float>(vertex.color & 0xffu)
                             / 255.0F;
                         const auto green = static_cast<float>(
                             (vertex.color >> 8) & 0xffu) / 255.0F;
                         const auto blue = static_cast<float>(
                             (vertex.color >> 16) & 0xffu) / 255.0F;
                         output << format_float(red) << ' '
                                << format_float(green) << ' '
                                << format_float(blue) << ' ';
                     });
        output << "      <vertices id=\"" << geometry_id
               << "_vertices\"><input semantic=\"POSITION\" source=\"#"
               << positions_id << "\"/></vertices>\n";
        if (!triangles.empty()) {
            output << "      <triangles count=\"" << triangles.size() << "\"";
            if (material != nullptr) {
                output << " material=\""
                       << xml_escape(collada_material_id(
                              *material, mesh.material_id))
                       << "\"";
            }
            output << ">\n"
                      "        <input semantic=\"VERTEX\" source=\"#"
                   << geometry_id << "_vertices\" offset=\"0\"/>\n"
                      "        <input semantic=\"NORMAL\" source=\"#"
                   << normals_id << "\" offset=\"1\"/>\n"
                      "        <input semantic=\"TEXCOORD\" source=\"#"
                   << uvs_id << "\" offset=\"2\" set=\"0\"/>\n"
                      "        <input semantic=\"COLOR\" source=\"#"
                   << colors_id << "\" offset=\"3\" set=\"0\"/>\n"
                      "        <p>";
            for (const auto& triangle : triangles) {
                for (const std::size_t index : {triangle.a, triangle.b,
                                                triangle.c}) {
                    output << index << ' ' << index << ' ' << index << ' '
                           << index << ' ';
                }
            }
            output << "</p>\n      </triangles>\n";
        }
        output << "    </mesh></geometry>\n";
    }
    output << "  </library_geometries>\n  <library_visual_scenes>\n"
              "    <visual_scene id=\"Scene\" name=\"Scene\">\n";
    for (std::size_t mesh_index = 0; mesh_index < model.meshes().size();
         ++mesh_index) {
        const auto& mesh = model.meshes()[mesh_index];
        output << "      <node id=\"node_" << mesh_index
               << "\" name=\"mesh_" << mesh_index
               << "\"><instance_geometry url=\"#geometry_" << mesh_index
               << "\"><bind_material><technique_common>";
        if (mesh.material_id < model.materials().size()) {
            output << "<instance_material symbol=\""
                   << xml_escape(collada_material_id(
                          model.materials()[mesh.material_id],
                          mesh.material_id))
                   << "\" target=\"#"
                   << xml_escape(collada_material_id(
                          model.materials()[mesh.material_id],
                          mesh.material_id))
                   << "\"/>";
        }
        output << "</technique_common></bind_material></instance_geometry>"
                  "</node>\n";
    }
    output << "    </visual_scene>\n  </library_visual_scenes>\n"
              "  <scene><instance_visual_scene url=\"#Scene\"/></scene>\n"
              "</COLLADA>\n";
    return stats;
}

MovieExportStats write_movie(
    const movie::VxDecoder& decoder,
    const std::filesystem::path& output_directory) {
    if (output_directory.empty()) {
        throw std::invalid_argument("movie output directory is empty");
    }
    const auto& header = decoder.header();
    if (header.frame_width <= 0 || header.frame_height <= 0) {
        throw std::invalid_argument("movie dimensions are invalid");
    }
    std::error_code error;
    std::filesystem::create_directories(output_directory, error);
    if (error) {
        throw std::runtime_error("could not create movie output directory: "
                                 + error.message());
    }

    MovieExportStats stats;
    for (std::size_t frame_index = 0; frame_index < decoder.frame_count();
         ++frame_index) {
        const auto rgb = decoder.image_rgb(frame_index);
        const std::size_t pixels = static_cast<std::size_t>(header.frame_width)
            * static_cast<std::size_t>(header.frame_height);
        if (rgb.size() != pixels * 3U) {
            throw std::runtime_error("movie frame has the wrong RGB size");
        }
        std::ostringstream filename;
        filename << std::setw(4) << std::setfill('0') << frame_index << ".png";
        write_png_rgb(output_directory / filename.str(), header.frame_width,
                      header.frame_height, rgb);
        ++stats.frames;
    }

    if (decoder.audio_frame_total() == 0) {
        return stats;
    }
    if (header.audio_sample_rate <= 0) {
        throw std::runtime_error("movie audio sample rate is invalid");
    }
    const auto samples = decoder.audio_samples();
    if (samples.size() > std::numeric_limits<std::uint32_t>::max() / 2U
        || samples.size() * 2U > std::numeric_limits<std::uint32_t>::max() - 36U) {
        throw std::runtime_error("movie audio is too large for a WAV file");
    }
    const auto data_size = static_cast<std::uint32_t>(samples.size() * 2U);
    const auto riff_size = static_cast<std::uint32_t>(36U + data_size);
    const auto sample_rate = static_cast<std::uint32_t>(header.audio_sample_rate);
    if (sample_rate > std::numeric_limits<std::uint32_t>::max() / 2U) {
        throw std::runtime_error("movie audio sample rate is too large");
    }
    std::ofstream audio(output_directory / "audio.wav",
                        std::ios::binary | std::ios::trunc);
    if (!audio) {
        throw std::runtime_error("could not create movie audio file");
    }
    audio.write("RIFF", 4);
    write_u32_le(audio, riff_size);
    audio.write("WAVEfmt ", 8);
    write_u32_le(audio, 16);
    write_u16_le(audio, 1); // PCM
    write_u16_le(audio, 1); // mono
    write_u32_le(audio, sample_rate);
    write_u32_le(audio, sample_rate * 2U);
    write_u16_le(audio, 2);
    write_u16_le(audio, 16);
    audio.write("data", 4);
    write_u32_le(audio, data_size);
    for (const std::int16_t sample : samples) {
        write_u16_le(audio, static_cast<std::uint16_t>(sample));
    }
    if (!audio) {
        throw std::runtime_error("could not write movie audio file");
    }
    stats.audio_frames = decoder.audio_frame_total();
    return stats;
}

} // namespace fruityprime::exporter
