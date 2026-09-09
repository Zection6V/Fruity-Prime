#include "Mods/Render/gl_es.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace fruityprime::mods::render {
namespace {

void add_triangle(EsDrawBatch& batch, std::size_t a, std::size_t b,
                  std::size_t c) {
    batch.triangle_indices.push_back(static_cast<std::uint32_t>(a));
    batch.triangle_indices.push_back(static_cast<std::uint32_t>(b));
    batch.triangle_indices.push_back(static_cast<std::uint32_t>(c));
}

void add_line(EsDrawBatch& batch, std::size_t a, std::size_t b) {
    batch.line_indices.push_back(static_cast<std::uint32_t>(a));
    batch.line_indices.push_back(static_cast<std::uint32_t>(b));
}

} // namespace

void GlEsBatch::set_draw_callback(DrawCallback callback) {
    draw_callback_ = std::move(callback);
}

void GlEsBatch::set_texture_callbacks(TextureCreateCallback create,
                                      TextureDeleteCallback destroy) {
    texture_create_callback_ = std::move(create);
    texture_delete_callback_ = std::move(destroy);
}

void GlEsBatch::reset() {
    lists_.clear();
    textures_.clear();
    next_list_id_ = 1;
    texture_high_water_ = 0;
    primitive_start_ = 0;
    record_list_id_ = 0;
    batch_.clear();
    last_draw_.clear();
    recording_ = false;
    color_set_ = false;
    alpha_test_enabled_ = false;
    alpha_function_ = EsAlphaFunction::Always;
}

void GlEsBatch::begin(EsPrimitiveType mode) {
    if (!recording_) {
        batch_.clear();
        // A non-list Begin starts with no per-vertex colour.  The draw
        // callback supplies current_color_ as the imm_color uniform.
        color_set_ = false;
    }
    primitive_mode_ = mode;
    primitive_start_ = batch_.vertex_count();
}

void GlEsBatch::end() {
    const std::size_t count = batch_.vertex_count() - primitive_start_;
    emit_indices(primitive_mode_, primitive_start_, count);
    if (!recording_) {
        submit(batch_);
        batch_.clear();
    }
}

void GlEsBatch::vertex(float x, float y, float z) {
    batch_.vertices.insert(batch_.vertices.end(), {
        x, y, z,
        current_color_[0], current_color_[1], current_color_[2], current_color_[3],
        current_normal_[0], current_normal_[1], current_normal_[2],
        current_texcoord_[0], current_texcoord_[1], current_texcoord_[2],
        color_set_ ? 1.0F : 0.0F
    });
}

void GlEsBatch::color3(float r, float g, float b) {
    color4(r, g, b, 1.0F);
}

void GlEsBatch::color4(float r, float g, float b, float a) {
    current_color_ = {r, g, b, a};
    color_set_ = true;
}

void GlEsBatch::normal3(float x, float y, float z) {
    current_normal_ = {x, y, z};
}

void GlEsBatch::texcoord3(float s, float t, float matrix_id) {
    current_texcoord_ = {s, t, matrix_id};
}

void GlEsBatch::emit_indices(EsPrimitiveType mode, std::size_t begin,
                             std::size_t count) {
    switch (mode) {
    case EsPrimitiveType::Triangles:
        for (std::size_t i = 0; i + 2 < count; i += 3) {
            add_triangle(batch_, begin + i, begin + i + 1, begin + i + 2);
        }
        break;
    case EsPrimitiveType::Quads:
        for (std::size_t i = 0; i + 3 < count; i += 4) {
            add_triangle(batch_, begin + i, begin + i + 1, begin + i + 2);
            add_triangle(batch_, begin + i, begin + i + 2, begin + i + 3);
        }
        break;
    case EsPrimitiveType::TriangleStrip:
        for (std::size_t i = 0; i + 2 < count; ++i) {
            if ((i & 1U) == 0) {
                add_triangle(batch_, begin + i, begin + i + 1, begin + i + 2);
            } else {
                add_triangle(batch_, begin + i + 1, begin + i, begin + i + 2);
            }
        }
        break;
    case EsPrimitiveType::QuadStrip:
        // Vertices arrive in pairs: quad k is (2k, 2k+1, 2k+3, 2k+2).
        for (std::size_t i = 0; i + 3 < count; i += 2) {
            add_triangle(batch_, begin + i, begin + i + 1, begin + i + 3);
            add_triangle(batch_, begin + i, begin + i + 3, begin + i + 2);
        }
        break;
    case EsPrimitiveType::TriangleFan:
        for (std::size_t i = 1; i + 1 < count; ++i) {
            add_triangle(batch_, begin, begin + i, begin + i + 1);
        }
        break;
    case EsPrimitiveType::LineLoop:
        for (std::size_t i = 0; i < count; ++i) {
            add_line(batch_, begin + i, begin + (i + 1) % count);
        }
        break;
    default:
        throw std::invalid_argument("no GLES translation for primitive type");
    }
}

int GlEsBatch::gen_lists(int range) {
    if (range < 0) {
        throw std::invalid_argument("display-list range cannot be negative");
    }
    const int first = next_list_id_;
    if (range > 0 && next_list_id_ > std::numeric_limits<int>::max() - range) {
        throw std::overflow_error("display-list id range overflow");
    }
    next_list_id_ += range;
    return first;
}

void GlEsBatch::new_list(int list, EsListMode /*mode*/) {
    batch_.clear();
    recording_ = true;
    record_list_id_ = list;
    // Display-list vertices with no explicit colour use the colour current at
    // CallList time, matching fixed-function GL and the managed GLES bridge.
    color_set_ = false;
}

void GlEsBatch::end_list() {
    if (!recording_) {
        throw std::logic_error("EndList without NewList");
    }
    recording_ = false;
    lists_[record_list_id_] = batch_;
    batch_.clear();
}

bool GlEsBatch::call_list(int list) {
    const auto found = lists_.find(list);
    if (found == lists_.end()) {
        return false;
    }
    submit(found->second);
    return true;
}

void GlEsBatch::delete_lists(int list, int range) {
    if (range < 0) {
        throw std::invalid_argument("display-list range cannot be negative");
    }
    for (int offset = 0; offset < range; ++offset) {
        lists_.erase(list + offset);
    }
}

int GlEsBatch::gen_texture() {
    if (texture_high_water_ == std::numeric_limits<int>::max()) {
        throw std::overflow_error("texture name range overflow");
    }
    const int name = ++texture_high_water_;
    (void)real_texture(name);
    return name;
}

void GlEsBatch::delete_texture(int name) {
    const auto found = textures_.find(name);
    if (found == textures_.end()) {
        return;
    }
    if (texture_delete_callback_) {
        texture_delete_callback_(found->second);
    }
    textures_.erase(found);
}

std::uint32_t GlEsBatch::real_texture(int name) {
    if (name == 0) {
        return 0;
    }
    if (name > texture_high_water_) {
        texture_high_water_ = name;
    }
    const auto found = textures_.find(name);
    if (found != textures_.end()) {
        return found->second;
    }
    const std::uint32_t real = texture_create_callback_
        ? texture_create_callback_()
        : static_cast<std::uint32_t>(name);
    textures_.emplace(name, real);
    return real;
}

void GlEsBatch::submit(const EsDrawBatch& batch) {
    last_draw_ = batch;
    if (draw_callback_) {
        draw_callback_(last_draw_, draw_state());
    }
}

EsDrawState GlEsBatch::draw_state() const noexcept {
    int mode = 0;
    if (alpha_test_enabled_) {
        if (alpha_function_ == EsAlphaFunction::Equal) {
            mode = 1;
        } else if (alpha_function_ == EsAlphaFunction::Less) {
            mode = 2;
        }
    }
    return EsDrawState{current_color_, mode};
}

} // namespace fruityprime::mods::render
