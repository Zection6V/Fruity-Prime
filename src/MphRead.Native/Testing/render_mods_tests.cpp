#include "../Mods/Render/render_mods.hpp"
#include "Mods/Render/es_bindings.hpp"
#include "Mods/Render/es_shaders.hpp"
#include "Mods/Render/gl_es.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

int main() {
    using fruityprime::formats::Vector3;
    using fruityprime::mods::render::icon_bounds;
    using fruityprime::mods::render::preview_camera;
    using fruityprime::mods::render::preview_pose;

    std::vector<std::uint8_t> chars(2 * 8 * 8, 0);
    // 16x8 DS-tiled data: x=9,y=3 is in tile column 1.
    chars[64 + 3 * 8 + 1] = 1;
    chars[64 + 6 * 8 + 5] = 2;
    const auto bounds = icon_bounds(chars, 0, 16, 8);
    assert(bounds.min_x == 9 && bounds.min_y == 3);
    assert(bounds.max_x == 13 && bounds.max_y == 6);
    assert(bounds.width() == 5 && bounds.height() == 4);
    assert(std::fabs(bounds.centre_x() - 11.5F) < 0.001F);
    assert(icon_bounds({}, 0, 16, 8).width() == 16);

    const auto pose = preview_pose({0.0F, 0.0F, 0.0F},
                                   {0.0F, 0.0F, 1.0F});
    assert(std::fabs(pose.facing.z - 1.0F) < 0.001F);
    assert(std::fabs(pose.right.x + 1.0F) < 0.001F);
    assert(std::fabs(pose.up.y - 1.0F) < 0.001F);
    const auto fallback = preview_pose({1.0F, 2.0F, 3.0F},
                                       {1.0F, 2.0F, 3.0F});
    assert(std::fabs(fallback.facing.z + 1.0F) < 0.001F);
    // PreviewCamera.cs falls back for LengthSquared < 0.0001f, not merely
    // for an exactly zero direction.
    const auto near_zero = preview_pose({0.0F, 0.0F, 0.0F},
                                        {0.005F, 0.0F, 0.0F});
    assert(std::fabs(near_zero.facing.z + 1.0F) < 0.001F);
    const auto just_outside = preview_pose({0.0F, 0.0F, 0.0F},
                                           {0.011F, 0.0F, 0.0F});
    assert(just_outside.facing.x > 0.99F);
    const auto camera = preview_camera({2.0F, 3.0F, 4.0F},
                                       {2.0F, 3.0F, 8.0F}, 0.0F);
    assert(camera.position.x == 2.0F && camera.previous_position.z == 4.0F);
    assert(camera.fov == 45.0F);
    assert(camera.facing.z > 0.99F);

    auto& es = fruityprime::mods::render::EsBindings::instance();
    es.reset();
    assert(!es.loaded() && es.get_proc_address("glBindVertexArray") == nullptr);

    using fruityprime::mods::render::EsShader;
    using fruityprime::mods::render::all_es_shaders;
    using fruityprime::mods::render::check_es_shaders_in_sync;
    using fruityprime::mods::render::es_shader_source;
    using fruityprime::mods::render::translate_es_shader;
    assert(all_es_shaders().size() == 6);
    for (const EsShader shader : all_es_shaders()) {
        const auto source = es_shader_source(shader);
        assert(source.starts_with("#version 300 es\n"));
        assert(source.find("void main()") != std::string_view::npos);
    }
    const std::array<std::string_view, 6> desktop_sources{
        "desktop vertex", "desktop fragment", "desktop RTT vertex",
        "desktop RTT fragment", "desktop cel fragment", "desktop shift fragment",
    };
    const auto translated = translate_es_shader(
        desktop_sources[4], desktop_sources);
    assert(translated.has_value()
           && translated->find("depth_tex") != std::string_view::npos);
    assert(!translate_es_shader("unknown", desktop_sources).has_value());
    std::string shader_error;
    const std::array<fruityprime::mods::render::EsShaderSyncEntry, 1> sync{
        fruityprime::mods::render::EsShaderSyncEntry{
            "Test", "a\r\nb", 
            "7e18f737311b2dc3b2f269dd78396b0351f14fb66efa879f768cb23181883c78"}
    };
    assert(check_es_shaders_in_sync(sync, shader_error)
           && shader_error.empty());
    const std::array<fruityprime::mods::render::EsShaderSyncEntry, 1> bad_sync{
        fruityprime::mods::render::EsShaderSyncEntry{
            "Test", "a\r\nb", 
            "0000000000000000000000000000000000000000000000000000000000000000"}
    };
    // A bad digest must produce a diagnostic, while CRLF is normalized before
    // the hash just as the managed implementation does.
    assert(!check_es_shaders_in_sync(bad_sync, shader_error));
    assert(shader_error.find("Shaders.Test") != std::string::npos);

    using fruityprime::mods::render::EsAlphaFunction;
    using fruityprime::mods::render::EsDrawBatch;
    using fruityprime::mods::render::EsDrawState;
    using fruityprime::mods::render::EsListMode;
    using fruityprime::mods::render::EsPrimitiveType;
    using fruityprime::mods::render::GlEsBatch;

    // The native GLES bridge expands every primitive the managed renderer
    // emits.  Keep the winding explicit: independent triangles do not carry
    // the alternating winding that a strip has in the fixed-function path.
    const auto check_primitive = [](EsPrimitiveType mode, int vertices,
                                    std::vector<std::uint32_t> triangles,
                                    std::vector<std::uint32_t> lines) {
        GlEsBatch bridge;
        bridge.begin(mode);
        for (int index = 0; index < vertices; ++index) {
            bridge.vertex(static_cast<float>(index), 0.0F, 0.0F);
        }
        bridge.end();
        assert(bridge.last_draw().triangle_indices == triangles);
        assert(bridge.last_draw().line_indices == lines);
    };
    check_primitive(EsPrimitiveType::Triangles, 4,
                    {0, 1, 2}, {});
    check_primitive(EsPrimitiveType::Quads, 4,
                    {0, 1, 2, 0, 2, 3}, {});
    check_primitive(EsPrimitiveType::TriangleStrip, 4,
                    {0, 1, 2, 2, 1, 3}, {});
    check_primitive(EsPrimitiveType::QuadStrip, 4,
                    {0, 1, 3, 0, 3, 2}, {});
    check_primitive(EsPrimitiveType::TriangleFan, 4,
                    {0, 1, 2, 0, 2, 3}, {});
    check_primitive(EsPrimitiveType::LineLoop, 3, {},
                    {0, 1, 1, 2, 2, 0});

    GlEsBatch bridge;
    EsDrawBatch submitted;
    EsDrawState state;
    int submissions = 0;
    bridge.set_draw_callback([&](const EsDrawBatch& draw,
                                 const EsDrawState& draw_state) {
        submitted = draw;
        state = draw_state;
        ++submissions;
    });
    bridge.normal3(0.0F, 1.0F, 0.0F);
    bridge.texcoord3(0.25F, 0.75F, 4.0F);
    bridge.begin(EsPrimitiveType::Triangles);
    bridge.color3(0.2F, 0.4F, 0.6F);
    bridge.vertex(0.0F, 0.0F, 0.0F);
    bridge.vertex(1.0F, 0.0F, 0.0F);
    bridge.vertex(0.0F, 1.0F, 0.0F);
    bridge.end();
    assert(submissions == 1 && submitted.vertex_count() == 3);
    assert(submitted.vertices[3] == 0.2F
           && submitted.vertices[6] == 1.0F
           && submitted.vertices[8] == 1.0F
           && submitted.vertices[10] == 0.25F
           && submitted.vertices[12] == 4.0F
           && submitted.vertices[13] == 1.0F);

    bridge.enable_alpha_test();
    bridge.alpha_func(EsAlphaFunction::Equal);
    bridge.begin(EsPrimitiveType::Triangles);
    bridge.vertex(0.0F, 0.0F, 0.0F);
    bridge.vertex(1.0F, 0.0F, 0.0F);
    bridge.vertex(0.0F, 1.0F, 0.0F);
    bridge.end();
    assert(state.alpha_test_mode == 1);

    const int list = bridge.gen_lists(1);
    bridge.new_list(list, EsListMode::Compile);
    bridge.begin(EsPrimitiveType::Quads);
    bridge.vertex(0.0F, 0.0F, 0.0F);
    bridge.vertex(1.0F, 0.0F, 0.0F);
    bridge.vertex(1.0F, 1.0F, 0.0F);
    bridge.vertex(0.0F, 1.0F, 0.0F);
    bridge.end();
    bridge.end_list();
    assert(!bridge.recording() && bridge.call_list(list));
    assert(submissions == 3
           && bridge.last_draw().triangle_indices.size() == 6);
    bridge.delete_lists(list, 1);
    assert(!bridge.call_list(list));

    int texture_creates = 0;
    std::vector<std::uint32_t> deleted_textures;
    bridge.set_texture_callbacks(
        [&]() { return static_cast<std::uint32_t>(100 + ++texture_creates); },
        [&](std::uint32_t texture) { deleted_textures.push_back(texture); });
    const int texture = bridge.gen_texture();
    assert(texture == 1 && bridge.real_texture(texture) == 101);
    assert(bridge.real_texture(7) == 102);
    bridge.delete_texture(texture);
    assert(deleted_textures == std::vector<std::uint32_t>{101});

    std::cout << "native render mods tests passed\n";
    return 0;
}
