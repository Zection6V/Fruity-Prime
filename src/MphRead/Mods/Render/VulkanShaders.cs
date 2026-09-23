#if !ANDROID
namespace MphRead.Mods.Render
{
    internal static class VulkanShaders
    {
        public const string SceneVertex = @"#version 450
layout(location=0) in vec3 a_position;
layout(location=1) in vec4 a_color;
layout(location=2) in vec3 a_normal;
layout(location=3) in vec3 a_tex0;
layout(location=4) in vec2 a_tex1;
layout(location=5) in float a_color_set;
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
} u;
layout(location=0) out vec2 fs_tex;
layout(location=1) out vec4 fs_color;
void main() {
    int mid = clamp(int(a_tex0.z), 0, 31);
    mat4 model_mtx = u.mtx_stack[mid] * u.view_inv_mtx;
    gl_Position = u.proj_mtx * u.view_mtx * model_mtx * vec4(a_position, 1.0);
    vec4 vc = a_color_set > 0.5 ? a_color : u.imm_color;
    fs_color = u.params0.z > 0.5 ? vc : vec4(1.0);
    fs_tex = (u.tex_mtx * vec4(a_tex0.xy, 0.0, 1.0)).xy;
}";
        public const string SceneFragment = @"#version 450
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
} u;
layout(set=0,binding=1) uniform texture2D Tex0;
layout(set=0,binding=2) uniform sampler Samp0;
layout(set=0,binding=3) uniform texture2D Tex1;
layout(set=0,binding=4) uniform sampler Samp1;
layout(location=0) in vec2 fs_tex;
layout(location=1) in vec4 fs_color;
layout(location=0) out vec4 out_color;
void main() {
    vec4 c = fs_color;
    if (u.params0.y > 0.5) c *= texture(sampler2D(Tex0, Samp0), fs_tex);
    c.a *= u.params0.x;
    if (u.params0.w > 0.5) {
        c.rgb = u.override_color.rgb;
        c.a *= u.override_color.a;
    }
    int alpha_mode = int(u.params1.x + 0.5);
    if (alpha_mode == 1 && c.a < 0.9999) discard;
    if (alpha_mode == 2 && c.a >= 0.9999) discard;
    if (u.params2.z > 0.5) {
        float slope = max(abs(dFdx(gl_FragCoord.z)), abs(dFdy(gl_FragCoord.z)));
        float bias = u.params2.x * slope + u.params2.y * (1.0 / 16777216.0);
        gl_FragDepth = clamp(gl_FragCoord.z + bias, 0.0, 1.0);
    }
    out_color = c;
}";
        public const string ScreenVertex = @"#version 450
layout(location=0) in vec3 a_position;
layout(location=1) in vec4 a_color;
layout(location=2) in vec3 a_normal;
layout(location=3) in vec3 a_tex0;
layout(location=4) in vec2 a_tex1;
layout(location=5) in float a_color_set;
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
} u;
layout(location=0) out vec2 fs_tex;
layout(location=1) out vec2 fs_tex1;
layout(location=2) out vec4 fs_color;
void main() {
    gl_Position = vec4(a_position.xy, 0.0, 1.0);
    fs_tex = a_tex0.xy;
    fs_tex1 = a_tex1;
    fs_color = a_color_set > 0.5 ? a_color : u.imm_color;
}";
        public const string ScreenFragment = @"#version 450
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
} u;
layout(set=0,binding=1) uniform texture2D Tex0;
layout(set=0,binding=2) uniform sampler Samp0;
layout(set=0,binding=3) uniform texture2D Tex1;
layout(set=0,binding=4) uniform sampler Samp1;
layout(location=0) in vec2 fs_tex;
layout(location=1) in vec2 fs_tex1;
layout(location=2) in vec4 fs_color;
layout(location=0) out vec4 out_color;
void main() {
    vec4 c = fs_color;
    if (u.params0.y > 0.5) c *= texture(sampler2D(Tex0, Samp0), fs_tex);
    if (u.fade_color.a > 0.0) c = u.fade_color;
    c.a *= u.params0.x;
    int alpha_mode = int(u.params1.x + 0.5);
    if (alpha_mode == 1 && c.a < 0.9999) discard;
    if (alpha_mode == 2 && c.a >= 0.9999) discard;
    out_color = c;
}";
    }
}
#endif
