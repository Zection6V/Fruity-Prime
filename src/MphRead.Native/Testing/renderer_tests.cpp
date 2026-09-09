#include "Renderer.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace {

bool near(float left, float right, float epsilon = 0.001F) {
    return std::fabs(left - right) <= epsilon;
}

} // namespace

int main() {
    using fruityprime::formats::FadeType;
    using fruityprime::formats::Vector3;
    using fruityprime::renderer::AfterFade;
    using fruityprime::renderer::Camera;
    using fruityprime::renderer::CameraMode;
    using fruityprime::renderer::FadeController;
    using fruityprime::renderer::Projection;
    using fruityprime::renderer::Viewport;

    using namespace fruityprime::renderer;
    using namespace fruityprime::formats;
    TextureMap bindings;
    bindings.add(3, -1, 254, 17, true);
    bindings.add(3, 0, 254, 18, false);
    assert(bindings.get(3, -1, 254).binding_id == 17);
    assert(bindings.get(3, 0, 254).binding_id == 18);
    bindings.add(3, -1, 254, 19, false);
    assert(bindings.get(3, -1, 254).binding_id == 19);
    assert(!bindings.get(3, -1, 254).only_opaque);
    const std::array<ColorRgba, 2> pixels{{{255,0,0,255}, {0,0,255,0}}};
    assert(near(average_color(pixels).x, 1));
    assert(near(average_color(pixels).z, 0));
    assert(near(average_color({}).x, 1));
    assert(update_material_mode(RenderMode::Decal, 0.5F, true) == RenderMode::Translucent);
    assert(update_material_mode(RenderMode::Decal, 1, true) == RenderMode::Normal);
    assert(update_material_mode(RenderMode::Decal, 1, false) == RenderMode::Decal);

    RenderQueue queue;
    RenderItem opaque; opaque.alpha = 1; opaque.polygon_id = queue.next_polygon_id();
    queue.submit(opaque);
    RenderItem decal; decal.alpha = 1; decal.polygon_id = queue.next_polygon_id(); decal.render_mode = RenderMode::Decal;
    queue.submit(decal);
    decal.polygon_id = queue.next_polygon_id(); decal.alpha = 0.5F;
    queue.submit(decal); // both decal and translucent
    RenderItem translucent; translucent.alpha = 1; translucent.polygon_id = queue.next_polygon_id();
    translucent.render_mode = RenderMode::Translucent;
    queue.submit(translucent);
    assert(queue.pass_items(ScenePass::Opaque).size() == 2);
    assert(queue.pass_items(ScenePass::Decal).size() == 2);
    assert(queue.pass_items(ScenePass::TranslucentBehind).size() == 2);
    struct Backend final : ScenePassBackend {
        std::vector<ScenePassState> states;
        std::vector<std::pair<int,int>> draws;
        bool begun = false, ended = false;
        void begin_scene() override { begun = true; }
        void begin_pass(ScenePass, const ScenePassState& s) override { states.push_back(s); }
        void draw(const RenderItem& item, int reference) override { draws.emplace_back(item.polygon_id, reference); }
        void end_scene() override { ended = true; }
    } backend;
    queue.render(backend);
    assert(backend.begun && backend.ended && backend.states.size() == 6);
    const std::vector<std::pair<int,int>> expected{
        {1,0},{4,0}, {2,0},{3,0}, {3,3},{4,4}, {1,0},{4,0}, {3,3},{4,4}, {3,3},{4,4}};
    assert(backend.draws == expected);
    assert(backend.states[1].polygon_offset && backend.states[1].blend);
    assert(!backend.states[2].color_write && backend.states[2].stencil_action[2] == StencilAction::Replace);
    assert(backend.states[3].clear_depth && !backend.states[3].color_write);
    assert(!backend.states[4].depth_write && backend.states[4].stencil == StencilTest::NotEqual);
    assert(backend.states[5].stencil == StencilTest::Equal);
    queue.clear();
    assert(queue.size() == 0 && queue.next_polygon_id() == 1);
    RenderItem mesh;
    mesh.alpha = 0.75F; mesh.transform.m41 = 9; mesh.palette_override = Vector4{1,0,0,1};
    std::array<float,16> stack{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    queue.add_mesh(mesh, 0.5F, 2, stack, 21, Vector4{0,1,0,1});
    const auto& scaled = queue.item(0);
    assert(near(scaled.alpha, 0.375F) && scaled.transform.m41 == 9);
    assert(scaled.transform.m11 == 2 && scaled.matrix_stack[2] == 6);
    assert(scaled.matrix_stack[3] == 4 && scaled.matrix_stack[12] == 13);
    assert(scaled.texture_binding_id == 21 && scaled.texgen_mode == TexgenMode::Normal);
    assert(scaled.x_repeat == RepeatMode::Mirror && !scaled.palette_override);

    const auto projection = Projection{}.matrix({1280, 720});
    assert(near(projection.m11 / projection.m22, 720.0F / 1280.0F));
    assert(near(projection.m34, -1.0F));
    assert(std::isfinite(projection.m33));
    assert(std::isfinite(projection.m43));

    Camera camera;
    camera.set_viewport(Viewport{1280, 720});
    assert(camera.mode() == CameraMode::Pivot);
    assert(near(camera.position().z, 5.0F));
    assert(near(camera.facing().z, -1.0F));
    assert(camera.frustum().index == 1);
    assert(camera.frustum().count == 5);
    assert(camera.frustum().planes[0].z_index1 == 5);

    camera.set_mode(CameraMode::Roam);
    camera.set_pose(Vector3{1.0F, 2.0F, 3.0F},
                    Vector3{0.0F, 0.0F, -1.0F},
                    Vector3{0.0F, 1.0F, 0.0F});
    assert(near(camera.position().x, 1.0F));
    assert(near(camera.position().y, 2.0F));
    assert(near(camera.position().z, 3.0F));
    camera.rotate(1.57079632679F, 0.0F);
    assert(near(camera.facing().x, 1.0F));
    assert(near(camera.facing().z, 0.0F));
    camera.set_fov_degrees(90.0F);
    camera.set_clip(20.0F, true);
    assert(near(camera.perspective_matrix().m11,
                camera.perspective_matrix().m22
                    / camera.viewport().aspect()));

    FadeController fade;
    fade.set(FadeType::FadeOutBlack, 1.0F, true, AfterFade::Exit);
    assert(fade.state().active);
    assert(near(fade.state().opacity, 0.0F));
    fade.update(0.25F);
    assert(near(fade.state().percent, 0.25F));
    assert(near(fade.state().opacity, 0.25F));
    const auto prior_type = fade.state().type;
    fade.set(FadeType::FadeInWhite, 1.0F, false);
    assert(fade.state().type == prior_type);
    fade.update(0.75F);
    // Exit keeps the fully opaque overlay until the scene owner quits.
    assert(fade.state().active);
    assert(fade.state().ended);
    assert(fade.consume_action() == AfterFade::Exit);
    assert(!fade.consume_action());
    assert(fade.consume_ended());
    assert(!fade.consume_ended());

    fade.set(FadeType::FadeOutBlack, 1, true, AfterFade::None, 0.1F);
    fade.update(0.25F);
    assert(near(fade.state().percent, 0));
    fade.update(0.25F);
    assert(near(fade.state().percent, 0.25F));
    fade.set(FadeType::FadeOutBlack, 0, true);
    fade.update(0);
    assert(std::isnan(fade.state().percent));
    fade.update(0.1F);
    assert(fade.state().type == FadeType::None && fade.state().ended);
    assert(fade.consume_ended());

    fade.set(FadeType::FadeOutBlack, 1.0F, true,
             AfterFade::LoadRoom, 0.5F);
    fade.update(0.25F);
    assert(near(fade.state().percent, 0.0F));
    fade.update(0.25F);
    assert(near(fade.state().percent, 0.0F));
    fade.update(0.25F);
    assert(near(fade.state().percent, 0.25F));

    fade.set(FadeType::FadeOutInWhite, 1.0F, true);
    fade.update(1.0F);
    assert(fade.state().active);
    assert(fade.state().type == FadeType::FadeInWhite);
    assert(near(fade.state().opacity, 1.0F));
    fade.update(0.5F);
    fade.update(0.5F);
    assert(!fade.state().active);
    assert(fade.consume_ended());

    std::cout << "native renderer camera/fade tests passed\n";
    return 0;
}
