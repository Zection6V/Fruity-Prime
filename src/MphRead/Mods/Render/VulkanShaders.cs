#if !ANDROID
namespace MphRead.Mods.Render
{
    internal static class VulkanShaders
    {
        private const string Common = @"#version 450
layout(set=0,binding=0,std140) uniform CompatUniforms {
    mat4 proj_mtx;
    mat4 view_mtx;
    mat4 view_inv_mtx;
    mat4 tex_mtx;
    mat4 mtx_stack[32];
    vec4 imm_color;
    vec4 override_color;
    vec4 fade_color;
    vec4 params0;
    vec4 params1;
    vec4 params2;
    vec4 light1vec;
    vec4 light2vec;
    vec4 light1col;
    vec4 light2col;
    vec4 diffuse;
    vec4 ambient;
    vec4 specular;
    vec4 emission;
    vec4 fog_color;
    vec4 pal_override_color;
    vec4 flat_color;
    vec4 scene0;
    vec4 scene1;
    vec4 scene2;
    vec4 toon_table[32];
    vec4 rtt0;
    vec4 cel0;
    vec4 cel1;
    vec4 shift0;
    vec4 shift_values[16];
    vec4 white_values[48];
} u;
";

        private const string FragmentResources = @"
layout(set=0,binding=1) uniform texture2D Tex0;
layout(set=0,binding=2) uniform sampler Samp0;
layout(set=0,binding=3) uniform texture2D Tex1;
layout(set=0,binding=4) uniform sampler Samp1;
";

        private const string VertexInputs = @"
layout(location=0) in vec3 a_position;
layout(location=1) in vec4 a_color;
layout(location=2) in vec3 a_normal;
layout(location=3) in vec3 a_tex0;
layout(location=4) in vec2 a_tex1;
layout(location=5) in float a_color_set;
";

        public static string SceneVertex { get; } = Common + VertexInputs + @"
layout(location=0) out vec2 fs_tex;
layout(location=1) out vec4 fs_color;

vec3 light_calc(vec3 light_vec, vec3 light_col, vec3 normal_vec,
    vec3 dif_col, vec3 amb_col, vec3 spe_col)
{
    vec3 sight_vec = vec3(0.0, 0.0, -1.0);
    float dif_factor = max(0.0, -dot(light_vec, normal_vec));
    vec3 half_vec = (light_vec + sight_vec) / 2.0;
    float spe_factor = max(0.0, dot(-half_vec, normal_vec));
    spe_factor *= spe_factor;
    return spe_col * light_col * spe_factor
        + dif_col * light_col * dif_factor
        + amb_col * light_col;
}

void main()
{
    int mid = clamp(int(a_tex0.z), 0, 31);
    mat4 stack_mtx = u.mtx_stack[mid];
    mat4 model_mtx = stack_mtx * u.view_inv_mtx;
    gl_Position = u.proj_mtx * u.view_mtx * model_mtx * vec4(a_position, 1.0);
    // OpenTK supplies OpenGL projection matrices with clip-space Z in [-w,+w].
    // Vulkan clips Z to [0,+w], so preserve the same near/far planes explicitly.
    gl_Position.z = (gl_Position.z + gl_Position.w) * 0.5;

    bool show_colors = u.params0.z > 0.5;
    bool use_light = u.scene0.x > 0.5;
    bool use_texture = u.params0.y > 0.5;
    // OpenGL display lists capture Color calls that occur inside the list,
    // but a vertex with no list-local Color uses the current color at
    // glCallList time. a_color_set distinguishes those two cases.
    vec4 effective_color = a_color_set > 0.5 ? a_color : u.imm_color;
    vec4 vtx_color = show_colors ? effective_color : vec4(1.0);
    vec3 normal = normalize(mat3(model_mtx) * a_normal);

    if (use_light) {
        vec3 dif_current = u.diffuse.rgb;
        vec3 amb_current = u.ambient.rgb;
        if (effective_color.a == 0.0) {
            dif_current = vtx_color.rgb;
            amb_current = vec3(0.0);
        }
        vec3 col1 = light_calc(u.light1vec.rgb, u.light1col.rgb, normal,
            dif_current, amb_current, u.specular.rgb);
        vec3 col2 = light_calc(u.light2vec.rgb, u.light2col.rgb, normal,
            dif_current, amb_current, u.specular.rgb);
        fs_color = vec4(min(col1 + col2 + u.emission.rgb, vec3(1.0)), 1.0);
    }
    else {
        fs_color = vec4(vtx_color.rgb, 1.0);
    }

    int texgen_mode = int(u.scene1.x + 0.5);
    if (use_texture) {
        if (texgen_mode == 0 || texgen_mode == 1) {
            fs_tex = (u.tex_mtx * vec4(a_tex0.xy, 0.0, 1.0)).xy;
        }
        else {
            mat4 tex_mul = u.tex_mtx;
            if (texgen_mode == 2) {
                tex_mul = transpose(u.tex_mtx
                    * (use_light ? u.view_mtx : mat4(1.0))
                    * mat4(mat3(stack_mtx)));
            }
            mat2x4 texgen_mtx = mat2x4(
                vec4(tex_mul[0][0], tex_mul[0][1], tex_mul[0][2], a_tex0.x),
                vec4(tex_mul[1][0], tex_mul[1][1], tex_mul[1][2], a_tex0.y));
            fs_tex = texgen_mode == 2
                ? vec4(a_normal, 1.0) * texgen_mtx
                : vec4(a_position, 1.0) * texgen_mtx;
        }
    }
    else {
        fs_tex = vec2(0.0);
    }
}";

        public static string SceneFragment { get; } = Common + FragmentResources + @"
layout(location=0) in vec2 fs_tex;
layout(location=1) in vec4 fs_color;
layout(location=0) out vec4 out_color;

vec4 toon_color(vec4 vtx_color)
{
    int index = clamp(int(vtx_color.r * 31.0), 0, 31);
    return vec4(u.toon_table[index].rgb, vtx_color.a);
}

vec3 cel_shade(vec3 c, float steps)
{
    float lum = max(max(c.r, c.g), c.b);
    if (lum <= 0.0) return c;
    float scaled = lum * steps - 0.5;
    float lower = floor(scaled);
    float level = (lower + 0.5 + smoothstep(0.46, 0.54, scaled - lower)) / steps;
    vec3 banded = c * (level / lum);
    float grey = dot(banded, vec3(0.299, 0.587, 0.114));
    return clamp(mix(vec3(grey), banded, 1.35), 0.0, 1.0);
}

void main()
{
    bool use_texture = u.params0.y > 0.5;
    bool use_override = u.params0.w > 0.5;
    bool use_pal_override = u.scene1.z > 0.5;
    bool use_flat = u.scene2.x > 0.5;
    int mat_mode = int(u.scene1.y + 0.5);
    int cel_bands = int(u.scene1.w + 0.5);
    float mat_alpha = u.params0.x;

    vec4 col;
    if (use_texture) {
        vec4 sampled = texture(sampler2D(Tex0, Samp0), fs_tex);
        // Veldrid stores OpenGL RGB textures in RGBA storage. RGB has no
        // alpha component in OpenGL, so sampling it must return alpha 1.
        if (u.params2.w > 0.5) sampled.a = 1.0;
        vec4 texcolor = use_pal_override
            ? vec4(u.pal_override_color.rgb, sampled.a)
            : sampled;
        if (use_flat && !use_pal_override) texcolor.rgb = u.flat_color.rgb;

        if (mat_mode == 1) {
            col = vec4(
                texcolor.r * texcolor.a + fs_color.r * (1.0 - texcolor.a),
                texcolor.g * texcolor.a + fs_color.g * (1.0 - texcolor.a),
                texcolor.b * texcolor.a + fs_color.b * (1.0 - texcolor.a),
                mat_alpha * fs_color.a);
        }
        else if (mat_mode == 2) {
            vec4 toon = toon_color(fs_color);
            col = vec4(texcolor.rgb * fs_color.r + toon.rgb,
                mat_alpha * texcolor.a * fs_color.a);
        }
        else {
            col = fs_color * vec4(texcolor.rgb, mat_alpha * texcolor.a);
        }

        if (use_override) {
            col.rgb = u.override_color.rgb;
            col.a *= u.override_color.a;
        }
    }
    else if (use_override) {
        col = u.override_color;
    }
    else {
        col = mat_mode == 2 ? toon_color(fs_color) : fs_color;
        col.a *= mat_alpha;
    }

    if (cel_bands > 0) col.rgb = cel_shade(col.rgb, float(cel_bands));

    if (u.scene0.y > 0.5) {
        float depth = gl_FragCoord.z;
        float density = 0.0;
        if (depth >= u.scene0.w) density = 1.0;
        else if (depth > u.scene0.z)
            density = (depth - u.scene0.z) / (u.scene0.w - u.scene0.z) * 124.0 / 128.0;
        col = vec4((col * (1.0 - density) + u.fog_color * density).xyz, col.a);
    }

    int alpha_mode = int(u.params1.x + 0.5);
    if (alpha_mode == 1 && col.a < 0.9999) discard;
    if (alpha_mode == 2 && col.a >= 0.9999) discard;

    if (u.params2.z > 0.5) {
        float slope = max(abs(dFdx(gl_FragCoord.z)), abs(dFdy(gl_FragCoord.z)));
        float bias = u.params2.x * slope + u.params2.y * (1.0 / 16777216.0);
        gl_FragDepth = clamp(gl_FragCoord.z + bias, 0.0, 1.0);
    }
    out_color = col;
}";

        public static string ScreenVertex { get; } = Common + VertexInputs + @"
layout(location=0) out vec2 fs_tex;
layout(location=1) out vec2 fs_tex1;
layout(location=2) out vec4 fs_color;
void main()
{
    // OpenGL NDC z=0 maps to window depth 0.5.
    gl_Position = vec4(a_position.xy, 0.5, 1.0);
    fs_tex = a_tex0.xy;
    fs_tex1 = a_tex1;
    fs_color = a_color_set > 0.5 ? a_color : u.imm_color;
}";

        public static string ScreenFragment { get; } = Common + FragmentResources + @"
layout(location=0) in vec2 fs_tex;
layout(location=1) in vec2 fs_tex1;
layout(location=2) in vec4 fs_color;
layout(location=0) out vec4 out_color;
void main()
{
    vec4 c = fs_color;
    if (u.params0.y > 0.5) {
        vec4 sampled = texture(sampler2D(Tex0, Samp0), fs_tex);
        if (u.params2.w > 0.5) sampled.a = 1.0;
        c = u.params1.w > 0.5 ? sampled : c * sampled;
    }
    if (u.fade_color.a > 0.0) c = u.fade_color;
    c.a *= u.params0.x;
    int alpha_mode = int(u.params1.x + 0.5);
    if (alpha_mode == 1 && c.a < 0.9999) discard;
    if (alpha_mode == 2 && c.a >= 0.9999) discard;
    out_color = c;
}";

        public static string RttFragment { get; } = Common + FragmentResources + @"
layout(location=0) in vec2 fs_tex;
layout(location=1) in vec2 fs_tex1;
layout(location=2) in vec4 fs_color;
layout(location=0) out vec4 out_color;
void main()
{
    vec4 c;
    if (u.fade_color.a > 0.0) {
        c = u.fade_color;
    }
    else {
        vec2 texUv = fs_tex;
        if (u.params1.y > 0.5) texUv.y = 1.0 - texUv.y;
        c = texture(sampler2D(Tex0, Samp0), texUv);
        if (u.params2.w > 0.5) c.a = 1.0;
        if (u.rtt0.y > 0.5) {
            // Vulkan FragCoord has an upper-left origin. The original RTT
            // shader's mask math is defined in OpenGL window coordinates.
            float glY = u.rtt0.w - gl_FragCoord.y;
            float maskY = glY + (u.rtt0.z - u.rtt0.w) / 2.0;
            vec2 maskUv = vec2(gl_FragCoord.x / u.rtt0.z, 1.0 - maskY / u.rtt0.z);
            if (u.params1.z > 0.5) maskUv.y = 1.0 - maskUv.y;
            vec4 maskSample = texture(sampler2D(Tex1, Samp1), maskUv);
            if (u.scene2.z > 0.5) maskSample.a = 1.0;
            if (maskSample.a > 0.0) c.a = 0.0;
        }
        c.a *= u.rtt0.x;
    }
    out_color = c;
}";

        public static string BackdropFragment { get; } = Common + FragmentResources + @"
layout(location=0) in vec2 fs_tex;
layout(location=1) in vec2 fs_tex1;
layout(location=2) in vec4 fs_color;
layout(location=0) out vec4 out_color;
void main()
{
    vec3 b = texture(sampler2D(Tex0, Samp0), fs_tex).rgb;
    vec3 s = texture(sampler2D(Tex1, Samp1), fs_tex1).rgb;
    vec3 lo = 2.0 * b * s;
    vec3 hi = 1.0 - 2.0 * (1.0 - b) * (1.0 - s);
    vec3 over = mix(lo, hi, step(vec3(0.5), b));
    out_color = vec4(mix(b, over, u.scene2.y), 1.0);
}";

        public static string ShiftFragment { get; } = Common + FragmentResources + @"
layout(location=0) in vec2 fs_tex;
layout(location=1) in vec2 fs_tex1;
layout(location=2) in vec4 fs_color;
layout(location=0) out vec4 out_color;

float shift_value(int index)
{
    int i = (index % 64 + 64) % 64;
    vec4 v = u.shift_values[i / 4];
    return v[i % 4];
}

float white_value(int index)
{
    int i = clamp(index, 0, 191);
    vec4 v = u.white_values[i / 4];
    return v[i % 4];
}

void main()
{
    int band = clamp(int((1.0 - fs_tex.y) * 192.0), 0, 191);
    int shift_idx = int(u.shift0.x);
    int index = (band + shift_idx + (band % 2) * 32) % 64;
    if (index < 0) index += 64;
    float value1 = shift_value(index);
    float value2 = shift_value(index + 1);
    float value = mix(value1, value2, u.shift0.z) * u.shift0.y;
    vec2 shifted = vec2(fs_tex.x + value, fs_tex.y);
    vec2 sampleUv = shifted;
    if (u.params1.y > 0.5) sampleUv.y = 1.0 - sampleUv.y;
    vec4 c;
    if (shifted.x < 0.0 || shifted.x > 1.0) c = vec4(0.0, 0.0, 0.0, 1.0);
    else {
        c = texture(sampler2D(Tex0, Samp0), sampleUv);
        if (u.params2.w > 0.5) c.a = 1.0;
    }

    float white_fac = u.shift0.w;
    if (white_fac != 0.0) {
        float factor = white_value(band);
        if (white_fac < 0.0) {
            c = vec4(factor, factor, factor, 1.0);
        }
        else {
            factor *= white_fac;
            if (factor >= 0.0) c = vec4(c.rgb + (vec3(1.0) - c.rgb) * factor, 1.0);
            else {
                factor = -factor;
                c = vec4(c.rgb - c.rgb * factor, 1.0);
            }
        }
    }
    out_color = c;
}";

        public static string CelFragment { get; } = Common + FragmentResources + @"
layout(location=0) in vec2 fs_tex;
layout(location=1) in vec2 fs_tex1;
layout(location=2) in vec4 fs_color;
layout(location=0) out vec4 out_color;

vec2 pixel_uv()
{
    return gl_FragCoord.xy * u.cel0.xy;
}

float raw_depth(float dx, float dy)
{
    return texture(sampler2D(Tex1, Samp1),
        pixel_uv() + vec2(dx * u.cel0.x, dy * u.cel0.y)).x;
}

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
    float quantised = u.cel1.y * 4.0 / r / unit;
    float lo = max(1.1, quantised * 1.5);
    float hi = max(3.5, quantised * 4.0);
    return smoothstep(lo, hi, kink_rel(d, r, unit));
}

void main()
{
    vec3 base = texture(sampler2D(Tex0, Samp0), pixel_uv()).rgb;
    float d = raw_depth(0.0, 0.0);
    float ink = 0.0;
    int probe = int(u.cel1.z + 0.5);
    if (d < 0.9999995) {
        float scale = u.cel1.x / (u.cel1.x - u.cel0.w) - d;
        float unit = max(scale, 1e-9) * u.cel0.x;
        if (probe == 1) {
            float shown = clamp(log2(max(kink_abs(d, 2.0), 1e-10)) / 32.0 + 1.0,
                0.004, 1.0);
            out_color = vec4(shown, shown, shown, 1.0);
            return;
        }
        ink = max(edge_at(d, 2.0, unit), edge_at(d, 3.0, unit)) * u.cel0.z;
    }
    else if (probe == 1) {
        out_color = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    out_color = vec4(base * (1.0 - ink), 1.0);
}";
        public static string ClearVertex { get; } = @"#version 450
void main()
{
    float x = -1.0 + float((gl_VertexIndex & 1) << 2);
    float y = -1.0 + float((gl_VertexIndex & 2) << 1);
    gl_Position = vec4(x, y, 1.0, 1.0);
}";

        public static string ClearFragment { get; } = @"#version 450
layout(set=0, binding=0) uniform ClearUniforms
{
    vec4 color;
} clear_u;
layout(location=0) out vec4 out_color;
void main()
{
    out_color = clear_u.color;
}";

    }
}
#endif
