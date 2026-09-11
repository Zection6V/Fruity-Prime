#include "EsShaders.hpp"

#if defined(__ANDROID__)
#include "../../Program.hpp"
#include "../../Shaders.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace
{
    constexpr std::array<std::uint32_t, 64> Sha256K = {
        0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
        0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
        0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
        0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
        0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
        0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
        0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
        0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
        0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
        0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
        0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
        0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
        0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
        0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
        0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
        0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
    };

    std::uint32_t RotateRight(std::uint32_t value, std::uint32_t count)
    {
        return (value >> count) | (value << (32U - count));
    }

    std::string Sha256Hex(const std::string& input)
    {
        std::vector<std::uint8_t> bytes(input.begin(), input.end());
        const std::uint64_t bitLength = static_cast<std::uint64_t>(bytes.size()) * 8U;
        bytes.push_back(0x80U);
        while ((bytes.size() % 64U) != 56U)
        {
            bytes.push_back(0U);
        }
        for (int shift = 56; shift >= 0; shift -= 8)
        {
            bytes.push_back(static_cast<std::uint8_t>((bitLength >> shift) & 0xffU));
        }

        std::array<std::uint32_t, 8> hash = {
            0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
            0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U
        };

        for (std::size_t offset = 0; offset < bytes.size(); offset += 64U)
        {
            std::array<std::uint32_t, 64> words{};
            for (std::size_t i = 0; i < 16U; ++i)
            {
                const std::size_t p = offset + i * 4U;
                words[i] = (static_cast<std::uint32_t>(bytes[p]) << 24U)
                    | (static_cast<std::uint32_t>(bytes[p + 1U]) << 16U)
                    | (static_cast<std::uint32_t>(bytes[p + 2U]) << 8U)
                    | static_cast<std::uint32_t>(bytes[p + 3U]);
            }
            for (std::size_t i = 16U; i < 64U; ++i)
            {
                const std::uint32_t s0 = RotateRight(words[i - 15U], 7U)
                    ^ RotateRight(words[i - 15U], 18U) ^ (words[i - 15U] >> 3U);
                const std::uint32_t s1 = RotateRight(words[i - 2U], 17U)
                    ^ RotateRight(words[i - 2U], 19U) ^ (words[i - 2U] >> 10U);
                words[i] = words[i - 16U] + s0 + words[i - 7U] + s1;
            }

            std::uint32_t a = hash[0];
            std::uint32_t b = hash[1];
            std::uint32_t c = hash[2];
            std::uint32_t d = hash[3];
            std::uint32_t e = hash[4];
            std::uint32_t f = hash[5];
            std::uint32_t g = hash[6];
            std::uint32_t h = hash[7];

            for (std::size_t i = 0; i < 64U; ++i)
            {
                const std::uint32_t s1 = RotateRight(e, 6U) ^ RotateRight(e, 11U) ^ RotateRight(e, 25U);
                const std::uint32_t ch = (e & f) ^ ((~e) & g);
                const std::uint32_t temp1 = h + s1 + ch + Sha256K[i] + words[i];
                const std::uint32_t s0 = RotateRight(a, 2U) ^ RotateRight(a, 13U) ^ RotateRight(a, 22U);
                const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
                const std::uint32_t temp2 = s0 + maj;

                h = g;
                g = f;
                f = e;
                e = d + temp1;
                d = c;
                c = b;
                b = a;
                a = temp1 + temp2;
            }

            hash[0] += a;
            hash[1] += b;
            hash[2] += c;
            hash[3] += d;
            hash[4] += e;
            hash[5] += f;
            hash[6] += g;
            hash[7] += h;
        }

        static constexpr char Hex[] = "0123456789abcdef";
        std::string result;
        result.reserve(64U);
        for (std::uint32_t word : hash)
        {
            for (int shift = 28; shift >= 0; shift -= 4)
            {
                result.push_back(Hex[(word >> shift) & 0x0fU]);
            }
        }
        return result;
    }

    std::string NormalizeCrLf(const std::string& source)
    {
        std::string normalized;
        normalized.reserve(source.size());
        for (std::size_t i = 0; i < source.size(); ++i)
        {
            if (source[i] == '\r' && i + 1U < source.size() && source[i + 1U] == '\n')
            {
                normalized.push_back('\n');
                ++i;
            }
            else
            {
                normalized.push_back(source[i]);
            }
        }
        return normalized;
    }
}

namespace MphRead::Mods::Render
{
    const std::string EsShaders::VertexShader = R"shader(#version 300 es
precision highp float;
precision highp int;

layout(location = 0) in vec4 a_position;
layout(location = 1) in vec4 a_color;
layout(location = 2) in vec3 a_normal;
layout(location = 3) in vec3 a_texcoord;
layout(location = 4) in float a_color_set;

uniform vec4 imm_color;

uniform bool use_light;
uniform bool use_texture;
uniform bool show_colors;
uniform bool fog_enable;
uniform vec3 light1vec;
uniform vec3 light1col;
uniform vec3 light2vec;
uniform vec3 light2col;
uniform vec3 diffuse;
uniform vec3 ambient;
uniform vec3 specular;
uniform vec3 emission;
uniform vec4 fog_color;
uniform float far_plane;
uniform mat4 proj_mtx;
uniform mat4 view_mtx;
uniform mat4 view_inv_mtx;
uniform mat4 tex_mtx;
uniform int texgen_mode;
uniform mat4 mtx_stack[32];

out vec2 texcoord;
out vec4 color;

vec3 light_calc(vec3 light_vec, vec3 light_col, vec3 normal_vec, vec3 dif_col, vec3 amb_col, vec3 spe_col)
{
    vec3 sight_vec = vec3(0.0, 0.0, -1.0);
    float dif_factor = max(0.0, -dot(light_vec, normal_vec));
    vec3 half_vec = (light_vec + sight_vec) / 2.0;
    float spe_factor = max(0.0, dot(-half_vec, normal_vec));
    spe_factor = spe_factor * spe_factor;
    vec3 spe_out = spe_col * light_col * spe_factor;
    vec3 dif_out = dif_col * light_col * dif_factor;
    vec3 amb_out = amb_col * light_col;
    return spe_out + dif_out + amb_out;
}

void main()
{
    vec4 vtx_in_color = a_color_set > 0.5 ? a_color : imm_color;
    mat4 stack_mtx = mtx_stack[int(a_texcoord.z)];
    // view_inv_mtx is set for billboard transforms
    mat4 model_mtx = stack_mtx * view_inv_mtx;
    gl_Position = proj_mtx * view_mtx * model_mtx * a_position;
    vec4 vtx_color = show_colors ? vtx_in_color : vec4(1.0);
    vec3 normal = normalize(mat3(model_mtx) * a_normal);
    if (use_light) {
        vec3 dif_current = diffuse;
        vec3 amb_current = ambient;
        if (vtx_in_color.a == 0.0) {
            // see comment on DIF_AMB
            dif_current = vtx_color.rgb;
            amb_current = vec3(0.0, 0.0, 0.0);
        }
        vec3 col1 = light_calc(light1vec, light1col, normal, dif_current, amb_current, specular);
        vec3 col2 = light_calc(light2vec, light2col, normal, dif_current, amb_current, specular);
        color = vec4(min((col1 + col2 + emission), vec3(1.0, 1.0, 1.0)), 1.0);
    }
    else {
        // alpha will only be less than 1.0 here if DIF_AMB is used but lighting is disabled
        color = vec4(vtx_color.rgb, 1.0);
    }
    texcoord = vec2(0.0, 0.0);
    if (use_texture) {
        // texgen mode: 0 - none, 1 - texcoord, 2 - normal, 3 - vertex
        if (texgen_mode == 0 || texgen_mode == 1) {
            texcoord = vec2(tex_mtx * vec4(a_texcoord.xy, 0.0, 1.0));
        }
        else if (texgen_mode == 2 || texgen_mode == 3) {
            mat4 tex_mul = tex_mtx;
            if (texgen_mode == 2) {
                // texgen uses the node transform, which doesn't have billboard transform applied
                tex_mul = transpose(tex_mtx * (use_light ? view_mtx : mat4(1.0)) * mat4(mat3(stack_mtx)));
            }
            mat2x4 texgen_mtx = mat2x4(
                vec4(tex_mul[0][0], tex_mul[0][1], tex_mul[0][2], a_texcoord.x),
                vec4(tex_mul[1][0], tex_mul[1][1], tex_mul[1][2], a_texcoord.y)
            );
            if (texgen_mode == 2) {
                texcoord = vec4(a_normal, 1.0) * texgen_mtx;
            }
            else {
                texcoord = vec4(a_position.xyz, 1.0) * texgen_mtx;
            }
        }
    }
}
)shader";

    const std::string EsShaders::FragmentShader = R"shader(#version 300 es
precision highp float;
precision highp int;

uniform bool use_texture;
uniform bool fog_enable;
uniform vec4 fog_color;
uniform float fog_min;
uniform float fog_max;
uniform sampler2D tex;
uniform bool use_override;
uniform vec4 override_color;
uniform bool use_pal_override;
uniform vec4 pal_override_color;
uniform float mat_alpha;
uniform int mat_mode;
uniform vec3 toon_table[32];
// 0 - off, 1 - pass only alpha == 1, 2 - pass only alpha < 1
uniform int alpha_test;
// Cel shading: 0 bands is off. Only the scene is drawn through this program;
// the helmet and the HUD go through the RTT one afterwards and are left as
// they are without anything having to turn this off.
uniform int cel_bands;
// The one colour the bound texture averages to, and whether to use it in
// place of the texture's own. Set per render item by the renderer, which
// works the average out once when the texture is uploaded.
uniform bool use_flat;
uniform vec3 flat_color;

in vec2 texcoord;
in vec4 color;

out vec4 frag_color;

vec4 toon_color(vec4 vtx_color)
{
    return vec4(toon_table[int(vtx_color.r * 31.0)], vtx_color.a);
}

// Brightness to steps, hue left alone: a surface keeps its colour and it is
// the shading across it that goes flat.
//
// The step used to be softened over the middle third of a band, because the
// texture was still there underneath and its texel-to-texel variation sat on
// a band boundary somewhere in every wall, which a hard step turned into
// speckle. There is no texture under this any more -- use_flat has already
// replaced it with one colour -- so the softening is down to the width that
// keeps the boundary from crawling as the camera moves, and a band is a band
// rather than a gradient.
vec3 cel_shade(vec3 c)
{
    float steps = float(cel_bands);
    float lum = max(max(c.r, c.g), c.b);
    if (lum <= 0.0) {
        return c;
    }
    // Levels sit at the middle of each band, so the darkest is not black and
    // the brightest is not blown out.
    float scaled = lum * steps - 0.5;
    float lower = floor(scaled);
    float level = (lower + 0.5 + smoothstep(0.46, 0.54, scaled - lower)) / steps;
    vec3 banded = c * (level / lum);
    // A drawn frame is more saturated than a photograph of the same thing,
    // and flattening the shading takes some of the apparent colour with it.
    float grey = dot(banded, vec3(0.299, 0.587, 0.114));
    return clamp(mix(vec3(grey), banded, 1.35), 0.0, 1.0);
}

void main()
{
    // mat_mode: 0 - modulate, 1 - decal, 2 - toon
    vec4 col;
    if (use_texture) {
        vec4 texcolor = use_pal_override ? vec4(pal_override_color.xyz, texture(tex, texcoord).w) : texture(tex, texcoord);
        // Cel shading takes the picture off the texture rather than banding
        // it. The texel's alpha is kept, so a grate is still a grate and a
        // decal is still cut to shape, but its colour is replaced by the one
        // colour the whole texture averages to. Banding a photograph of
        // rubble only ever produces banded rubble; what makes a picture read
        // as drawn is that the surface is one colour and the line around it
        // carries the shape.
        if (use_flat && !use_pal_override) {
            texcolor.rgb = flat_color;
        }
        if (mat_mode == 1) {
            col = vec4(
                (texcolor.r * texcolor.a + color.r * (1.0 - texcolor.a)),
                (texcolor.g * texcolor.a + color.g * (1.0 - texcolor.a)),
                (texcolor.b * texcolor.a + color.b * (1.0 - texcolor.a)),
                mat_alpha * color.a
            );
        }
        else if (mat_mode == 2) {
            vec4 toon = toon_color(color);
            col = vec4(texcolor.rgb * color.r + toon.rgb, mat_alpha * texcolor.a * color.a);
        }
        else {
            col = color * vec4(texcolor.rgb, mat_alpha * texcolor.a);
        }
        if (use_override) {
            col.r = override_color.r;
            col.g = override_color.g;
            col.b = override_color.b;
            col.a *= override_color.a;
        }
    }
    else if (use_override) {
        col = override_color;
    }
    else {
        col = mat_mode == 2 ? toon_color(color) : color;
        col.a *= mat_alpha;
    }
    // Cel shading, on the finished surface colour -- the texture, the vertex
    // colours and the lighting together, which is the only place all three
    // are. Banding the lighting term alone left a room untouched: rooms carry
    // nearly all of their shading in vertex colours and light almost nothing
    // dynamically, so the mode was invisible exactly where it should have
    // shown most. Before the fog, which is atmosphere rather than surface and
    // reads wrong in steps.
    if (cel_bands > 0) {
        col.rgb = cel_shade(col.rgb);
    }
    if (fog_enable) {
        float depth = gl_FragCoord.z;
        float density = 0.0;
        if (depth >= fog_max) {
            density = 1.0;
        }
        else if (depth > fog_min) {
            // MPH fog table has min 0 and max 124
            density = (depth - fog_min) / (fog_max - fog_min) * 124.0 / 128.0;
        }
        col = vec4((col * (1.0 - density) + fog_color * density).xyz, col.a);
    }
    // glAlphaFunc, which ES does not have. The engine only ever asks for
    // Equal 1.0 and Less 1.0, and the test runs on the final colour.
    if (alpha_test == 1 && col.a < 1.0) {
        discard;
    }
    if (alpha_test == 2 && col.a >= 1.0) {
        discard;
    }
    frag_color = col;
}
)shader";

    const std::string EsShaders::RttVertexShader = R"shader(#version 300 es
precision highp float;

layout(location = 0) in vec4 a_position;
layout(location = 3) in vec3 a_texcoord;

out vec2 texcoord;

void main()
{
    gl_Position = vec4(a_position.xy, 0.0, 1.0);
    texcoord = a_texcoord.xy;
}
)shader";

    const std::string EsShaders::RttFragmentShader = R"shader(#version 300 es
precision highp float;

uniform float alpha;
uniform bool use_mask;
uniform float view_width;
uniform float view_height;
uniform vec4 fade_color;
uniform sampler2D tex;
uniform sampler2D mask;

in vec2 texcoord;

out vec4 frag_color;

void main()
{
    if (fade_color.a > 0.0) {
        frag_color = fade_color;
    }
    else {
        frag_color = texture(tex, texcoord);
        if (use_mask) {
            float maskY = gl_FragCoord.y + (view_width - view_height) / 2.0;
            vec2 maskTexcoord = vec2(gl_FragCoord.x / view_width, 1.0 - maskY / view_width);
            vec4 maskColor = texture(mask, maskTexcoord);
            if (maskColor.a > 0.0) {
                frag_color.a = 0.0;
            }
        }
        frag_color.a *= alpha;
    }
}
)shader";

    const std::string EsShaders::CelFragmentShader = R"shader(#version 300 es
precision highp float;

uniform sampler2D tex;
// highp, said out loud, because the fragment language does not say it for us.
// The precision line above sets the default for floats and not for
// samplers: an ES 3.0 fragment shader defaults sampler2D to lowp, so this one
// declaration is the difference between reading the depth buffer at the
// twenty-four bits it stores and reading it at about twelve.
//
// That is not a subtlety here, it is the whole pass. Measured on a Mali-G78
// the depth arrived 3964x coarser than the driver stores it -- 2.4e-4, which
// is one step of an fp16 near these values and nothing like one step of a
// D24 -- and the ink drew those steps as straight black lines across every
// floor. The desktop shader has no precision qualifiers at all, is fp32
// throughout, and never had the problem: this is the entire difference
// between the two platforms.
uniform highp sampler2D depth_tex;
uniform float texel_w;
uniform float texel_h;
uniform float outline;
uniform float near_plane;
uniform float far_plane;
// One step of the depth buffer the driver actually gave us, which is not
// always the one that was asked for. See the note in edge_at.
uniform float depth_quantum;
// What the ink must clear before it is believed, measured on this machine
// rather than assumed. Zero until the first frame has been looked at.
// 1 draws the measurement instead of the picture, for that one frame.
uniform int probe;

in vec2 texcoord;

out vec4 frag_color;

// The depth buffer's own value, not a distance in world units.
//
// That is the whole trick. Window-space depth is an affine function of 1/z,
// and 1/z is *linear across the screen* for any plane at any angle -- that is
// what makes perspective-correct interpolation work at all. So the second
// difference of this number is exactly zero on a flat surface however steeply
// it runs away from the camera. Linearising it to world units first, which is
// what this pass used to do, throws that away: z itself is not linear in
// screen space, its second difference over a floor stretching to the far wall
// is large, and every flat surface seen at an angle came out scribbled over.
// Where this pixel is, worked out from gl_FragCoord rather than from the
// interpolated texture coordinate.
//
// gl_FragCoord.xy is the pixel centre exactly, so this lands on texel centres
// exactly and the taps are exactly r texels apart. Coming through a varying
// they are only as good as the interpolator, and ES promises highp no better
// than sixteen bits of mantissa -- a fraction of a row of error, which is
// nothing to a picture and everything to a pass that compares a row with the
// two either side of it. Getting a neighbour off by a row every so often
// draws a line straight across the screen wherever it happens, which is what
// a phone was doing while this machine was not.
vec2 pixel_uv()
{
    return gl_FragCoord.xy * vec2(texel_w, texel_h);
}

float raw_depth(float dx, float dy)
{
    return texture(depth_tex, pixel_uv() + vec2(dx * texel_w, dy * texel_h)).x;
}

// How much of an edge there is at a given reach, 0 to 1.
//
// Reach does two things. It widens the line -- a pixel three away from an
// edge still sees it, so the ink comes out three or four pixels wide instead
// of the one pixel a drawn line never is -- and, because the kink is divided
// by it while whatever the depth buffer got wrong is not, it is also
// *quieter*. Reaching two and three rather than one and two is two to three
// times the margin over a noisy depth buffer for a line that looks the same,
// and each reach carries its own floor rather than the two being maxed
// together and taking the noisier one's noise with them.
//
// The neighbours are subtracted from the centre before being added to each
// other. Written as a sum of three samples it is three roundings of numbers
// close to 1, and the signal here is around a millionth of that; written as
// two differences it is exact, since a difference of two floats within a
// factor of two of each other always is. That costs nothing and is most of
// the precision this pass has -- and highp in an ES fragment shader is only
// promised sixteen bits of mantissa, so it is worth more here than it is on
// the desktop.
// The kink at one reach, in the units everything below is measured in.
// The kink in the depth buffer's own units -- what the pass actually looks
// at, before anything is made of it. Per unit of reach, so the reaches are
// comparable with each other and with a measurement taken at one of them.
float kink_abs(float d, float r)
{
    vec2 h = vec2(raw_depth(-r, 0.0) - d, raw_depth(r, 0.0) - d);
    vec2 v = vec2(raw_depth(0.0, -r) - d, raw_depth(0.0, r) - d);
    return max(abs(h.x + h.y), abs(v.x + v.y)) / r;
}

float kink_rel(float d, float r, float unit)
{
    return kink_abs(d, r) / unit;
}

float edge_at(float d, float r, float unit)
{
    // What this machine's depth is worth here, in this pixel's units.
    //
    // depth_quantum is an error in the *depth buffer's* units -- one step of
    // it, or worse where the driver interpolates worse than it stores, which
    // Renderer.CalibrateInk measures rather than takes on trust. Dividing it
    // by the same unit the kink is divided by is the whole point: unit shrinks
    // with distance and with grazing angle, so a fixed error is worth more and
    // more in these units the further away and the flatter-on the surface is.
    // A floor that did not do this was the bug -- it was a constant, measured
    // once in the middle of a frame where unit was large, and it protected
    // exactly the surfaces that never needed protecting. A floor stretching
    // away from the camera got no protection at all, and drew the depth
    // buffer's own steps as regular black stripes across itself.
    float quantised = depth_quantum * 4.0 / r / unit;
    float lo = max(1.1, quantised * 1.5);
    float hi = max(3.5, quantised * 4.0);
    return smoothstep(lo, hi, kink_rel(d, r, unit));
}

void main()
{
    vec3 base = texture(tex, pixel_uv()).rgb;
    float d = raw_depth(0.0, 0.0);
    float ink = 0.0;
    // Nothing was drawn here: the cleared far plane has no shape to draw
    // around, and the normalisation below divides by nearly zero on it.
    // The silhouette against it is still found, from the geometry's side.
    if (d < 0.9999995) {
        // far/(far-near) - d is (far*near/(far-near))/z, so dividing by it
        // takes the distance out and leaves a pure change of slope; dividing
        // by the texel width takes the resolution out, so the same threshold
        // means the same corner at 640x360 and at 4K. What is left is about
        // 2 for a right-angled crease and hundreds for a silhouette.
        float scale = far_plane / (far_plane - near_plane) - d;
        float unit = max(scale, 1e-9) * texel_w;
        if (probe == 1) {
            // log2 of the kink in depth units, -32..0 into 0..1, and black
            // where nothing was drawn so the reader can leave those pixels
            // out. Depth units, not this pixel's units, because what is being
            // measured is a property of the machine rather than of wherever
            // the camera happened to be pointing: a flat surface's kink in
            // these units is the depth error itself, and it is the same number
            // across the frame. Thirty-two powers of two below one covers a
            // buffer from eight bits to well past the twenty-four this asks
            // for.
            float shown = clamp(log2(max(kink_abs(d, 2.0), 1e-10)) / 32.0 + 1.0, 0.004, 1.0);
            frag_color = vec4(shown, shown, shown, 1.0);
            return;
        }
        ink = max(edge_at(d, 2.0, unit), edge_at(d, 3.0, unit)) * outline;
    }
    else if (probe == 1) {
        frag_color = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    frag_color = vec4(base * (1.0 - ink), 1.0);
}
)shader";

    const std::string EsShaders::ShiftFragmentShader = R"shader(#version 300 es
precision highp float;
precision highp int;

uniform float shift_table[64];
uniform int shift_idx;
uniform float shift_fac;
uniform float lerp_fac;
uniform float white_table[192];
uniform float white_fac;
uniform sampler2D tex;

in vec2 texcoord;

out vec4 frag_color;

void main()
{
    int band = int((1.0 - texcoord.y) * 192.0);
    float bandf = float(band);
    int index = int(mod(bandf + float(shift_idx) + mod(bandf, 2.0) * 32.0, 64.0));
    float value1 = shift_table[index];
    float value2 = shift_table[int(mod(float(index) + 1.0, 64.0))];
    float value = mix(value1, value2, lerp_fac) * shift_fac;
    vec2 shifted = vec2(texcoord.x + value, texcoord.y);
    if (shifted.x < 0.0 || shifted.x > 1.0) {
        frag_color = vec4(0.0, 0.0, 0.0, 1.0);
    }
    else {
        frag_color = texture(tex, shifted);
    }
    if (white_fac != 0.0) {
        float factor = white_table[band];
        if (white_fac < 0.0) {
            frag_color = vec4(factor, factor, factor, 1.0);
        }
        else {
            factor *= white_fac;
            if (factor >= 0.0) {
                float r = frag_color.r + (1.0 - frag_color.r) * factor;
                float g = frag_color.g + (1.0 - frag_color.g) * factor;
                float b = frag_color.b + (1.0 - frag_color.b) * factor;
                frag_color = vec4(r, g, b, 1.0);
            }
            else {
                factor = -factor;
                float r = frag_color.r - frag_color.r * factor;
                float g = frag_color.g - frag_color.g * factor;
                float b = frag_color.b - frag_color.b * factor;
                frag_color = vec4(r, g, b, 1.0);
            }
        }
    }
}
)shader";

    bool EsShaders::_checked = false;

    const std::string* EsShaders::Translate(const std::string* desktopSource)
    {
        if (desktopSource == &Shaders::VertexShader)
        {
            return &VertexShader;
        }
        if (desktopSource == &Shaders::FragmentShader)
        {
            return &FragmentShader;
        }
        if (desktopSource == &Shaders::RttVertexShader)
        {
            return &RttVertexShader;
        }
        if (desktopSource == &Shaders::RttFragmentShader)
        {
            return &RttFragmentShader;
        }
        if (desktopSource == &Shaders::CelFragmentShader)
        {
            return &CelFragmentShader;
        }
        if (desktopSource == &Shaders::ShiftFragmentShader)
        {
            return &ShiftFragmentShader;
        }
        return nullptr;
    }

    void EsShaders::CheckInSync()
    {
        if (_checked)
        {
            return;
        }
        _checked = true;
        Check("VertexShader", Shaders::VertexShader,
            "4cf1422bddaa3ece44c9cfbf6dab1ede192ee8c3f4fbed362e7da5eebfdfc428");
        Check("FragmentShader", Shaders::FragmentShader,
            "b7d15d11622cb4ff811f36572d8d74bc30450b75e81404ff27b48dc8665d8528");
        Check("RttVertexShader", Shaders::RttVertexShader,
            "af070f447840bf1fc51d6bba88a339fab067a4e3a01e460351a2549ca9107f4f");
        Check("RttFragmentShader", Shaders::RttFragmentShader,
            "021b5992926cb3a8c714fb943b0c85e091cf3cd76d2c487950ca0fb03d27c56e");
        Check("CelFragmentShader", Shaders::CelFragmentShader,
            "0fcb40630809a0e5b2d78448ed8b9518686fb6a5fc3b1a69914a37fecf28f7d5");
        Check("ShiftFragmentShader", Shaders::ShiftFragmentShader,
            "2b2511d5506ad9a25d64005b7b9e452f56b550410f96c753a6072a743b3162fa");
    }

    void EsShaders::Check(const std::string& name, const std::string& source, const std::string& expected)
    {
        const std::string actual = Sha256Hex(NormalizeCrLf(source));
        if (actual != expected)
        {
            throw ProgramException(
                "Shaders." + name + " has changed since the OpenGL ES version of it was written "
                + "(expected " + expected + ", found " + actual + "). Update EsShaders." + name
                + " to match, then update the hash here.");
        }
    }
}
#endif
