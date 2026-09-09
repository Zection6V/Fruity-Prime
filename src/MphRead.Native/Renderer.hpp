#pragma once

#include "Formats/culling.hpp"
#include "Formats/Types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>
#include <span>
#include <unordered_map>

namespace fruityprime::renderer {

struct TextureBinding {
    int binding_id = 0;
    bool only_opaque = false;
};

class TextureMap {
public:
    [[nodiscard]] static std::uint32_t key(int texture_id, int palette_id, int recolor_id);
    [[nodiscard]] TextureBinding get(int texture_id, int palette_id, int recolor_id) const;
    void add(int texture_id, int palette_id, int recolor_id, int binding_id, bool only_opaque);
    void clear() noexcept { bindings_.clear(); }
private:
    std::unordered_map<std::uint32_t, TextureBinding> bindings_;
};

[[nodiscard]] formats::Vector3 average_color(std::span<const formats::ColorRgba> pixels) noexcept;
[[nodiscard]] formats::RenderMode update_material_mode(formats::RenderMode current,
    float alpha, bool only_opaque) noexcept;

struct Viewport {
    int width = 0;
    int height = 0;
    [[nodiscard]] float aspect() const noexcept;
};

enum class Layer : std::uint8_t {
    Room,
    Player,
    Effects,
    Hud,
    Debug
};

struct DrawItem {
    std::uint32_t resource_id = 0;
    Layer layer = Layer::Room;
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
    float scale = 1.0F;
    bool textured = false;
};

// Renderer.cs value enums.  The native renderer keeps these independent from
// any windowing or graphics API so debug tools and future backends can share
// the same state vocabulary.
enum class VolumeDisplay : std::uint8_t {
    None,
    LightColor1,
    LightColor2,
    TriggerParent,
    TriggerChild,
    AreaInside,
    AreaExit,
    MorphCamera,
    JumpPad,
    Teleporter,
    EnemyHurt,
    Object,
    FlagBase,
    DefenseNode,
    KillPlane,
    PlayerLimit,
    CameraLimit,
    NodeBounds,
    NodeData,
    Portal
};

enum class CollisionType : std::uint8_t {
    Any,
    Player,
    Beam,
    Both
};

enum class CollisionColor : std::uint8_t {
    None,
    Entity,
    Terrain,
    Type
};

enum class CameraMode : std::uint8_t {
    Pivot,
    Roam,
    Player
};

enum class AfterFade : std::uint8_t {
    None,
    Exit,
    LoadRoom,
    PlayMovie,
    StopMovie,
    EnterShip
};

struct Projection {
    float fov_radians = 78.0F * 0.01745329251994329577F;
    float near_clip = 0.0625F;
    float far_clip = 10'000.0F;
    bool use_clip = false;

    [[nodiscard]] formats::Matrix4 matrix(Viewport viewport) const;
};

// Renderer.UpdateProjection's camera-space plane construction. The distance
// must be computed after normalization, in the same coordinate basis as XYZ.
[[nodiscard]] culling::FrustumInfo build_frustum(const formats::Matrix4& view,
    formats::Vector3 camera_position, float fov, Viewport viewport, float near_clip) noexcept;

enum class ScenePass : std::uint8_t {
    Opaque, Decal, MarkTranslucent, RebuildDepth, TranslucentBehind, TranslucentBefore
};

enum class AlphaTest : std::uint8_t { Disabled, EqualOne, LessThanOne };
enum class DepthTest : std::uint8_t { Less, LessOrEqual };
enum class StencilTest : std::uint8_t { Always, Greater, NotEqual, Equal };
enum class StencilAction : std::uint8_t { Zero, Keep, Replace };
struct ScenePassState {
    AlphaTest alpha = AlphaTest::EqualOne;
    DepthTest depth = DepthTest::Less;
    StencilTest stencil = StencilTest::Always;
    std::array<StencilAction, 3> stencil_action{StencilAction::Zero, StencilAction::Zero, StencilAction::Zero};
    bool color_write = true;
    bool depth_write = true;
    bool blend = false;
    bool polygon_offset = false;
    bool clear_depth = false;
};

class ScenePassBackend {
public:
    virtual ~ScenePassBackend() = default;
    // All scene passes use an enabled stencil test and an 0xFF mask.
    // blend is SrcAlpha/OneMinusSrcAlpha; polygon_offset is (-1,-1).
    virtual void begin_scene() = 0;
    virtual void begin_pass(ScenePass pass, const ScenePassState& state) = 0;
    virtual void draw(const formats::RenderItem& item, int stencil_reference) = 0;
    // Restore depth writes, disable alpha/stencil tests and use filled polygons
    // before HUD models, cel outlines, and window composition.
    virtual void end_scene() = 0;
};

// Submission order is significant for DS polygon IDs. Items appear in several
// passes; translucent is a second index over the same items, not a partition.
class RenderQueue {
public:
    void clear() noexcept;
    void submit(formats::RenderItem item);
    void add_mesh(formats::RenderItem item, float alpha_scale, float scale_factor,
                  std::span<const float> matrix_stack,
                  std::optional<int> binding_override = std::nullopt,
                  std::optional<formats::Vector4> selection_color = std::nullopt);
    void add_volume(formats::CullingMode culling, int polygon_id,
                    formats::Vector4 color, formats::RenderItemType type,
                    std::span<const formats::Vector3> points, int vertex_count = 0,
                    bool no_lines = false);
    void add_effect(formats::RenderItemType type, float alpha, int polygon_id,
                    formats::Vector3 color, formats::RepeatMode x_repeat,
                    formats::RepeatMode y_repeat, float scale_s, float scale_t,
                    formats::Matrix4 transform, std::span<const formats::Vector3> points,
                    int binding_id, formats::BillboardMode billboard = formats::BillboardMode::None,
                    int trail_count = 8);
    void add_trail(formats::RenderItemType type, int polygon_id, formats::Vector3 color,
                   formats::RepeatMode x_repeat, formats::RepeatMode y_repeat,
                   float scale_s, float scale_t, int matrix_count,
                   std::span<const float> matrix_stack,
                   std::span<const formats::Vector3> points, int segments, int binding_id);
    [[nodiscard]] int next_polygon_id() noexcept;
    [[nodiscard]] std::span<const std::size_t> pass_items(ScenePass pass) const noexcept;
    [[nodiscard]] const formats::RenderItem& item(std::size_t index) const { return items_.at(index); }
    [[nodiscard]] std::size_t size() const noexcept { return items_.size(); }
    void render(ScenePassBackend& backend) const;
private:
    std::vector<formats::RenderItem> items_;
    std::vector<std::size_t> decals_, non_decals_, translucent_;
    std::uint32_t next_polygon_id_ = 1;
};

// Portable camera state extracted from the managed Scene/Renderer partial.
// A frontend can use the resulting matrices directly or translate the pose
// to its own API.  All setters rebuild immediately, which makes the class
// safe for a pause-menu setting to change the viewport while a match is live.
class Camera final {
public:
    Camera() noexcept;

    void set_viewport(Viewport viewport) noexcept;
    void set_mode(CameraMode mode) noexcept;
    void set_pivot(float angle_x_degrees, float angle_y_degrees,
                   float distance) noexcept;
    void set_pose(formats::Vector3 position, formats::Vector3 facing,
                  formats::Vector3 up) noexcept;
    void set_fov_degrees(float degrees) noexcept;
    void set_clip(float far_clip, bool enabled) noexcept;
    void update() noexcept;
    void reset() noexcept;
    void rotate(float horizontal_radians, float vertical_radians) noexcept;

    [[nodiscard]] Viewport viewport() const noexcept { return viewport_; }
    [[nodiscard]] CameraMode mode() const noexcept { return mode_; }
    [[nodiscard]] const formats::Vector3& position() const noexcept {
        return position_;
    }
    [[nodiscard]] const formats::Vector3& facing() const noexcept {
        return facing_;
    }
    [[nodiscard]] const formats::Vector3& up() const noexcept { return up_; }
    [[nodiscard]] const formats::Vector3& right() const noexcept {
        return right_;
    }
    [[nodiscard]] float fov_radians() const noexcept { return fov_radians_; }
    [[nodiscard]] const formats::Matrix4& view_matrix() const noexcept {
        return view_matrix_;
    }
    [[nodiscard]] const formats::Matrix4& view_inverse_rotation()
        const noexcept {
        return view_inverse_rotation_;
    }
    [[nodiscard]] const formats::Matrix4& view_inverse_rotation_y()
        const noexcept {
        return view_inverse_rotation_y_;
    }
    [[nodiscard]] const formats::Matrix4& perspective_matrix()
        const noexcept {
        return perspective_matrix_;
    }
    [[nodiscard]] const culling::FrustumInfo& frustum() const noexcept {
        return frustum_;
    }

    [[nodiscard]] static culling::FrustumPlane set_bounds_indices(
        formats::Vector4 plane) noexcept;

private:
    void rebuild() noexcept;

    Viewport viewport_;
    CameraMode mode_ = CameraMode::Pivot;
    float pivot_angle_x_degrees_ = 0.0F;
    float pivot_angle_y_degrees_ = 0.0F;
    float pivot_distance_ = 5.0F;
    formats::Vector3 position_{};
    formats::Vector3 facing_{0.0F, 0.0F, -1.0F};
    formats::Vector3 up_{0.0F, 1.0F, 0.0F};
    formats::Vector3 right_{1.0F, 0.0F, 0.0F};
    float fov_radians_ = 78.0F * 0.01745329251994329577F;
    float near_clip_ = 0.0625F;
    float far_clip_ = 10'000.0F;
    bool use_clip_ = false;
    formats::Matrix4 view_matrix_{};
    formats::Matrix4 view_inverse_rotation_{};
    formats::Matrix4 view_inverse_rotation_y_{};
    formats::Matrix4 perspective_matrix_{};
    culling::FrustumInfo frustum_{};
};

struct FadeState {
    formats::FadeType type = formats::FadeType::None;
    AfterFade after = AfterFade::None;
    float percent = 0.0F;
    float color = 0.0F;
    float opacity = 0.0F;
    bool active = false;
    bool ended = false;
};

// Renderer.cs SetFade/UpdateFade equivalent.  It exposes a compositing value
// instead of touching GL, so a platform renderer only has to draw one solid
// overlay using color (0 black, 1 white) and opacity.
class FadeController final {
public:
    void set(formats::FadeType type, float length_seconds, bool overwrite,
             AfterFade after = AfterFade::None,
             float delay_seconds = 0.0F) noexcept;
    void update(float seconds) noexcept;
    void clear() noexcept;
    [[nodiscard]] const FadeState& state() const noexcept { return state_; }
    [[nodiscard]] bool consume_ended() noexcept;
    // Platform/gameplay side effects (exit, room load, movie, ship) are queued
    // for the scene owner, never executed by the portable renderer.
    [[nodiscard]] std::optional<AfterFade> consume_action() noexcept;

private:
    void begin_type(formats::FadeType type) noexcept;

    FadeState state_;
    float length_seconds_ = 0.0F;
    float delay_seconds_ = 0.0F;
    float elapsed_seconds_ = 0.0F;
    float start_seconds_ = 0.0F;
    std::optional<AfterFade> pending_action_;
    bool ended_pending_ = false;
};

// Renderer.cs equivalent at the portable boundary. A frontend can translate
// this collected frame to OpenGL, Vulkan, Direct3D, or an Android surface
// without making scene/gameplay code depend on a graphics API.
class Frame {
public:
    void begin(Viewport viewport) noexcept;
    void submit(DrawItem item);
    void clear() noexcept;
    [[nodiscard]] Viewport viewport() const noexcept { return viewport_; }
    [[nodiscard]] const std::vector<DrawItem>& items() const noexcept {
        return items_;
    }

private:
    Viewport viewport_;
    std::vector<DrawItem> items_;
};

class Renderer {
public:
    virtual ~Renderer() = default;
    [[nodiscard]] virtual bool begin(Viewport viewport) noexcept = 0;
    virtual void draw(DrawItem item) = 0;
    virtual void end() noexcept = 0;
};

class NullRenderer final : public Renderer {
public:
    [[nodiscard]] bool begin(Viewport viewport) noexcept override;
    void draw(DrawItem item) override;
    void end() noexcept override;
    [[nodiscard]] const Frame& frame() const noexcept { return frame_; }

private:
    Frame frame_;
};

} // namespace fruityprime::renderer

namespace MphReadNative {
namespace Renderer = ::fruityprime::renderer;
}
