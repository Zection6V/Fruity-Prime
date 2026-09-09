#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace fruityprime::mods::render {

// These are the desktop OpenGL primitive values accepted by the managed
// renderer.  GLES has no Quads or immediate mode, so the native bridge keeps
// the caller-facing values and expands them before a draw is submitted.
enum class EsPrimitiveType : std::uint32_t {
    LineLoop = 0x0002,
    Triangles = 0x0004,
    TriangleStrip = 0x0005,
    TriangleFan = 0x0006,
    Quads = 0x0007,
    QuadStrip = 0x0008
};

enum class EsAlphaFunction : std::uint32_t {
    Less = 0x0201,
    Equal = 0x0202,
    Always = 0x0207
};

enum class EsListMode : std::uint32_t {
    Compile = 0x1300
};

inline constexpr std::size_t EsFloatsPerVertex = 14;

struct EsDrawBatch final {
    // Position (3), colour (4), normal (3), texture coordinate plus matrix
    // id (3), and the fixed-function "colour was explicitly set" bit (1).
    std::vector<float> vertices;
    std::vector<std::uint32_t> triangle_indices;
    std::vector<std::uint32_t> line_indices;

    [[nodiscard]] std::size_t vertex_count() const noexcept {
        return vertices.size() / EsFloatsPerVertex;
    }

    void clear() noexcept {
        vertices.clear();
        triangle_indices.clear();
        line_indices.clear();
    }
};

struct EsDrawState final {
    std::array<float, 4> current_color{1.0F, 1.0F, 1.0F, 1.0F};
    int alpha_test_mode = 0;
};

// CPU-side GLES compatibility layer.  It owns the exact state conversion
// that does not depend on a platform GL context: immediate vertices become
// indexed triangles/lines, display lists retain their baked geometry, and
// engine-owned texture names remain separate from driver names.  A platform
// head installs submit/create/delete callbacks to issue the actual GLES
// calls after it has loaded its context functions through EsBindings.
class GlEsBatch final {
public:
    using DrawCallback = std::function<void(const EsDrawBatch&, const EsDrawState&)>;
    using TextureCreateCallback = std::function<std::uint32_t()>;
    using TextureDeleteCallback = std::function<void(std::uint32_t)>;

    GlEsBatch() = default;

    void set_draw_callback(DrawCallback callback);
    void set_texture_callbacks(TextureCreateCallback create,
                               TextureDeleteCallback destroy);

    // Drop the CPU and name-map state owned by this bridge.  The context
    // owner is responsible for deleting any driver objects it still tracks.
    void reset();

    void begin(EsPrimitiveType mode);
    void end();
    void vertex(float x, float y, float z);
    void color3(float r, float g, float b);
    void color4(float r, float g, float b, float a);
    void normal3(float x, float y, float z);
    void texcoord3(float s, float t, float matrix_id);

    int gen_lists(int range);
    void new_list(int list, EsListMode mode);
    void end_list();
    bool call_list(int list);
    void delete_lists(int list, int range);

    int gen_texture();
    void delete_texture(int name);
    [[nodiscard]] std::uint32_t real_texture(int name);

    void enable_alpha_test() noexcept { alpha_test_enabled_ = true; }
    void disable_alpha_test() noexcept { alpha_test_enabled_ = false; }
    void alpha_func(EsAlphaFunction function) noexcept { alpha_function_ = function; }

    [[nodiscard]] const EsDrawBatch& last_draw() const noexcept {
        return last_draw_;
    }
    [[nodiscard]] const std::array<float, 4>& current_color() const noexcept {
        return current_color_;
    }
    [[nodiscard]] bool recording() const noexcept { return recording_; }

private:
    void emit_indices(EsPrimitiveType mode, std::size_t begin,
                      std::size_t count);
    void submit(const EsDrawBatch& batch);
    [[nodiscard]] EsDrawState draw_state() const noexcept;

    EsDrawBatch batch_;
    EsDrawBatch last_draw_;
    std::unordered_map<int, EsDrawBatch> lists_;
    std::unordered_map<int, std::uint32_t> textures_;
    std::array<float, 4> current_color_{1.0F, 1.0F, 1.0F, 1.0F};
    std::array<float, 3> current_normal_{0.0F, 0.0F, 1.0F};
    std::array<float, 3> current_texcoord_{0.0F, 0.0F, 0.0F};
    EsPrimitiveType primitive_mode_ = EsPrimitiveType::Triangles;
    EsAlphaFunction alpha_function_ = EsAlphaFunction::Always;
    std::size_t primitive_start_ = 0;
    int record_list_id_ = 0;
    int next_list_id_ = 1;
    int texture_high_water_ = 0;
    bool color_set_ = false;
    bool recording_ = false;
    bool alpha_test_enabled_ = false;
    DrawCallback draw_callback_;
    TextureCreateCallback texture_create_callback_;
    TextureDeleteCallback texture_delete_callback_;
};

} // namespace fruityprime::mods::render
