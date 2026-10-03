#pragma once

#include "Formats/Formats.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace MphRead
{
    enum class ScenePrimitiveTopology : std::uint8_t
    {
        Triangles,
        Quads,
        TriangleStrip,
        QuadStrip
    };

    enum class SceneVertexAttributeState : std::uint8_t
    {
        None = 0,
        Color = 1U << 0U,
        Normal = 1U << 1U
    };

    [[nodiscard]] constexpr SceneVertexAttributeState operator|(
        SceneVertexAttributeState left, SceneVertexAttributeState right) noexcept
    {
        return static_cast<SceneVertexAttributeState>(
            static_cast<std::uint8_t>(left) | static_cast<std::uint8_t>(right));
    }

    [[nodiscard]] constexpr bool HasSceneVertexAttributeState(
        SceneVertexAttributeState state, SceneVertexAttributeState value) noexcept
    {
        return (static_cast<std::uint8_t>(state) & static_cast<std::uint8_t>(value)) != 0;
    }

    struct SceneVertex final
    {
        OpenTK::Mathematics::Vector3 Position{};
        OpenTK::Mathematics::Vector3 Normal{0.0F, 0.0F, 1.0F};
        OpenTK::Mathematics::Vector4 Color{1.0F, 1.0F, 1.0F, 1.0F};
        OpenTK::Mathematics::Vector2 TexCoord{};
        std::uint32_t MatrixIndex = 0;
    };

    struct ScenePrimitiveRange final
    {
        ScenePrimitiveTopology Topology = ScenePrimitiveTopology::Triangles;
        std::uint32_t FirstIndex = 0;
        std::uint32_t IndexCount = 0;
    };

    // A non-owning instruction view keeps the decoder core independently
    // testable without constructing a Model or an OpenGL context. The public
    // overload below still takes the existing RenderInstruction list directly.
    struct RendererGeometryInstruction final
    {
        InstructionCode Code = InstructionCode::NOP;
        std::span<const std::uint32_t> Arguments{};
    };

    struct RendererGeometry final
    {
        std::vector<SceneVertex> Vertices{};
        std::vector<std::uint32_t> Indices{};
        std::vector<ScenePrimitiveRange> Ranges{};

        // OpenGL color and normal are current-state attributes. Before a list's
        // first COLOR/NORMAL instruction, the value comes from the caller at
        // display-list execution time and cannot honestly be baked into a CPU
        // vertex. These flags distinguish that inherited state from an explicit
        // value while keeping SceneVertex itself straightforward and unpacked.
        std::vector<SceneVertexAttributeState> VertexAttributeStates{};

        // COLOR/NORMAL can legally occur after the final vertex. The legacy
        // display-list path exposes those current-state changes to the next
        // draw, so retain the terminal state for the compatibility boundary.
        SceneVertex TerminalState{};
        SceneVertexAttributeState TerminalAttributeState = SceneVertexAttributeState::None;
    };

    class RendererGeometryException final : public std::runtime_error
    {
    public:
        explicit RendererGeometryException(std::string message)
            : std::runtime_error(std::move(message))
        {
        }
    };

    [[nodiscard]] RendererGeometry DecodeRendererGeometry(
        std::span<const RendererGeometryInstruction> instructions,
        std::int32_t textureWidth, std::int32_t textureHeight,
        bool texgen, bool isRoom, std::size_t matrixCount);

    [[nodiscard]] RendererGeometry DecodeRendererGeometry(
        const std::vector<std::shared_ptr<RenderInstruction>>& instructions,
        std::int32_t textureWidth, std::int32_t textureHeight,
        bool texgen, bool isRoom, std::size_t matrixCount);
}
