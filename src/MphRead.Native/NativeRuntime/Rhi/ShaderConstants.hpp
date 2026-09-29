#pragma once

#include "../OpenTK/Mathematics.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

// What the renderer hands a shader, grouped by how often it changes, with no
// uniform location, program id or binding number anywhere in it. A backend
// turns these into whatever it has: the OpenGL one into glUniform calls on
// the locations it looked up itself (OpenGlShaderConstants), a Vulkan one into
// uniform-buffer writes against VulkanShaderInterface.hpp's blocks.
//
// Each structure is exactly the set of values one frontend call site sets
// together, so replacing that call site's uniform calls with one Set() changes
// no GPU state the old calls did not change.
namespace MphRead::NativeRuntime::Rhi
{
    // Per camera: set when the view changes, and swapped for the HUD's
    // orthographic pair while the HUD draws.
    struct FrameConstants final
    {
        ::OpenTK::Mathematics::Matrix4 View{};
        ::OpenTK::Mathematics::Matrix4 Projection{};
    };

    // Per room (or per preview): the DS's two directional lights.
    struct LightConstants final
    {
        ::OpenTK::Mathematics::Vector3 Vector{};
        ::OpenTK::Mathematics::Vector3 Color{};
    };

    inline constexpr std::size_t SceneLightCount = 2;

    struct SceneLightConstants final
    {
        LightConstants Lights[SceneLightCount]{};
    };

    // Per room: the fog the room metadata describes, as fractions of the depth range.
    struct SceneFogConstants final
    {
        ::OpenTK::Mathematics::Vector4 Color{};
        float MinDistance = 0.0F;
        float MaxDistance = 0.0F;
    };

    // Per material: DoMaterial's set.
    struct MaterialConstants final
    {
        bool UseLight = false;
        ::OpenTK::Mathematics::Vector3 Diffuse{};
        ::OpenTK::Mathematics::Vector3 Ambient{};
        ::OpenTK::Mathematics::Vector3 Specular{};
        ::OpenTK::Mathematics::Vector3 Emission{};
        float Alpha = 1.0F;
        std::int32_t PolygonMode = 0;
    };

    inline constexpr std::size_t MatrixStackCapacity = 32;

    // Per draw: the node matrix stack the vertex shader indexes with
    // TexCoord.z. Sixteen floats a matrix in Matrix4's own order (M11, M12,
    // ...), which the shaders read untransposed; at most MatrixStackCapacity
    // of them. The span does not own the data.
    struct DrawConstants final
    {
        std::span<const float> MatrixStack{};
    };

    // Post: the cel-shading outline pass.
    struct CelPostConstants final
    {
        float TexelWidth = 0.0F;
        float TexelHeight = 0.0F;
        float Outline = 0.0F;
        float NearPlane = 0.0F;
        float FarPlane = 0.0F;
        float DepthQuantum = 0.0F;
        bool Probe = false;
    };

    class ShaderConstantSink
    {
    public:
        virtual ~ShaderConstantSink() = default;

        virtual void Set(const FrameConstants& constants) = 0;
        virtual void Set(const SceneLightConstants& constants) = 0;
        // One light on its own: an entity lit by its own light rather than the room's.
        virtual void SetLight(std::size_t index, const LightConstants& constants) = 0;
        virtual void Set(const SceneFogConstants& constants) = 0;
        virtual void Set(const MaterialConstants& constants) = 0;
        virtual void Set(const DrawConstants& constants) = 0;
        virtual void Set(const CelPostConstants& constants) = 0;
    };
}
