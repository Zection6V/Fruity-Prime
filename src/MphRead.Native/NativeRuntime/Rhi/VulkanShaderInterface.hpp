#pragma once

#include "ShaderConstants.hpp"
#include "VertexSemantics.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

// The shader interface the Vulkan backend is built against: vertex input
// locations, descriptor sets and bindings, and the std140 layout of every
// constant block. Nothing here talks to Vulkan -- it is the contract, fixed
// before there is a backend, so that the OpenGL frontend's constant groups
// (ShaderConstants.hpp) and the SPIR-V the Vulkan shaders compile to cannot
// drift apart. Phase 5 section 5.5 stage A: OpenGL keeps GLSL, Vulkan gets
// SPIR-V, and this interface is what both share.
namespace MphRead::NativeRuntime::Rhi::Vulkan
{
    // Descriptor sets by update frequency, so a draw rebinds only what changed.
    inline constexpr std::uint32_t FrameSet = 0;     // FrameConstants, scene lights and fog
    inline constexpr std::uint32_t MaterialSet = 1;  // MaterialConstants, the texture
    inline constexpr std::uint32_t DrawSet = 2;      // DrawConstants (dynamic offset)
    inline constexpr std::uint32_t PostSet = 3;      // post-process passes

    struct BlockBinding final
    {
        std::uint32_t Set;
        std::uint32_t Binding;
    };

    inline constexpr BlockBinding FrameBlock{FrameSet, 0};
    inline constexpr BlockBinding SceneLightBlock{FrameSet, 1};
    inline constexpr BlockBinding SceneFogBlock{FrameSet, 2};
    inline constexpr BlockBinding MaterialBlock{MaterialSet, 0};
    inline constexpr BlockBinding MaterialTexture{MaterialSet, 1};
    inline constexpr BlockBinding DrawBlock{DrawSet, 0};
    inline constexpr BlockBinding CelPostBlock{PostSet, 0};

    // std140 mirrors. A vec3 occupies sixteen bytes, bools are 32-bit, and a
    // mat4 is four vec4 columns; the static_asserts below are the layout the
    // GLSL blocks in VertexInterfaceGlsl/ConstantBlocksGlsl declare.
    struct alignas(16) Std140Vec3 final
    {
        float X = 0.0F;
        float Y = 0.0F;
        float Z = 0.0F;
        float Pad = 0.0F;
    };

    struct alignas(16) FrameBlockData final
    {
        float View[16]{};
        float Projection[16]{};
    };

    struct alignas(16) SceneLightBlockData final
    {
        Std140Vec3 Light1Vector{};
        Std140Vec3 Light1Color{};
        Std140Vec3 Light2Vector{};
        Std140Vec3 Light2Color{};
    };

    struct alignas(16) SceneFogBlockData final
    {
        float Color[4]{};
        float MinDistance = 0.0F;
        float MaxDistance = 0.0F;
        float Pad[2]{};
    };

    struct alignas(16) MaterialBlockData final
    {
        Std140Vec3 Diffuse{};
        Std140Vec3 Ambient{};
        Std140Vec3 Specular{};
        Std140Vec3 Emission{};
        float Alpha = 1.0F;
        std::int32_t PolygonMode = 0;
        std::uint32_t UseLight = 0;
        float Pad = 0.0F;
    };

    struct alignas(16) DrawBlockData final
    {
        float MatrixStack[MatrixStackCapacity * 16U]{};
    };

    struct alignas(16) CelPostBlockData final
    {
        float TexelWidth = 0.0F;
        float TexelHeight = 0.0F;
        float Outline = 0.0F;
        float NearPlane = 0.0F;
        float FarPlane = 0.0F;
        float DepthQuantum = 0.0F;
        std::uint32_t Probe = 0;
        float Pad = 0.0F;
    };

    static_assert(sizeof(Std140Vec3) == 16);
    static_assert(sizeof(FrameBlockData) == 128);
    static_assert(offsetof(FrameBlockData, Projection) == 64);
    static_assert(sizeof(SceneLightBlockData) == 64);
    static_assert(sizeof(SceneFogBlockData) == 32);
    static_assert(offsetof(SceneFogBlockData, MinDistance) == 16);
    static_assert(sizeof(MaterialBlockData) == 80);
    static_assert(offsetof(MaterialBlockData, Alpha) == 64);
    static_assert(offsetof(MaterialBlockData, UseLight) == 72);
    static_assert(sizeof(DrawBlockData) == MatrixStackCapacity * 64U);
    static_assert(sizeof(CelPostBlockData) == 32);

    [[nodiscard]] inline Std140Vec3 Pack(const ::OpenTK::Mathematics::Vector3& v)
    {
        return Std140Vec3{v.X, v.Y, v.Z, 0.0F};
    }

    [[nodiscard]] inline FrameBlockData Pack(const FrameConstants& c)
    {
        FrameBlockData d{};
        std::memcpy(d.View, &c.View.M11, sizeof(d.View));
        std::memcpy(d.Projection, &c.Projection.M11, sizeof(d.Projection));
        return d;
    }

    [[nodiscard]] inline SceneLightBlockData Pack(const SceneLightConstants& c)
    {
        return SceneLightBlockData{Pack(c.Lights[0].Vector), Pack(c.Lights[0].Color),
            Pack(c.Lights[1].Vector), Pack(c.Lights[1].Color)};
    }

    [[nodiscard]] inline SceneFogBlockData Pack(const SceneFogConstants& c)
    {
        return SceneFogBlockData{{c.Color.X, c.Color.Y, c.Color.Z, c.Color.W},
            c.MinDistance, c.MaxDistance, {}};
    }

    [[nodiscard]] inline MaterialBlockData Pack(const MaterialConstants& c)
    {
        return MaterialBlockData{Pack(c.Diffuse), Pack(c.Ambient), Pack(c.Specular),
            Pack(c.Emission), c.Alpha, c.PolygonMode, c.UseLight ? 1U : 0U, 0.0F};
    }

    [[nodiscard]] inline DrawBlockData Pack(const DrawConstants& c)
    {
        DrawBlockData d{};
        const std::size_t count = c.MatrixStack.size() < std::size(d.MatrixStack)
            ? c.MatrixStack.size() : std::size(d.MatrixStack);
        if (count != 0)
        {
            std::memcpy(d.MatrixStack, c.MatrixStack.data(), count * sizeof(float));
        }
        return d;
    }

    [[nodiscard]] inline CelPostBlockData Pack(const CelPostConstants& c)
    {
        return CelPostBlockData{c.TexelWidth, c.TexelHeight, c.Outline, c.NearPlane,
            c.FarPlane, c.DepthQuantum, c.Probe ? 1U : 0U, 0.0F};
    }

    // The vertex inputs, as the Vulkan vertex shaders declare them. The
    // locations are VulkanLocations; the names are VertexSemanticNames.
    inline constexpr std::string_view VertexInterfaceGlsl = R"glsl(
layout(location = 0) in vec4 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec4 a_color;
layout(location = 3) in vec3 a_texcoord;
layout(location = 4) in vec2 a_texcoord1;
)glsl";

    // The constant blocks, std140, in the sets and bindings above. The main
    // program's matrices are read untransposed, as the OpenGL uniforms are.
    inline constexpr std::string_view ConstantBlocksGlsl = R"glsl(
layout(std140, set = 0, binding = 0) uniform FrameBlock {
    mat4 view_mtx;
    mat4 proj_mtx;
};
layout(std140, set = 0, binding = 1) uniform SceneLightBlock {
    vec3 light1vec;
    vec3 light1col;
    vec3 light2vec;
    vec3 light2col;
};
layout(std140, set = 0, binding = 2) uniform SceneFogBlock {
    vec4 fog_color;
    float fog_min;
    float fog_max;
};
layout(std140, set = 1, binding = 0) uniform MaterialBlock {
    vec3 diffuse;
    vec3 ambient;
    vec3 specular;
    vec3 emission;
    float mat_alpha;
    int mat_mode;
    bool use_light;
};
layout(set = 1, binding = 1) uniform sampler2D tex;
layout(std140, set = 2, binding = 0) uniform DrawBlock {
    mat4 mtx_stack[32];
};
layout(std140, set = 3, binding = 0) uniform CelPostBlock {
    float texel_w;
    float texel_h;
    float outline;
    float near_plane;
    float far_plane;
    float depth_quantum;
    bool probe;
};
)glsl";
}
