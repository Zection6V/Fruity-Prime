#include "RendererGeometry.hpp"

#include "NativeRuntime/System/Exceptions.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace
{
    using MphRead::RendererGeometryException;
    using MphRead::RendererGeometryInstruction;
    using MphRead::ScenePrimitiveTopology;

    [[nodiscard]] std::int32_t Signed10(std::uint32_t value) noexcept
    {
        std::int32_t result = static_cast<std::int32_t>(value & 0x3FFU);
        if ((result & 0x200) != 0)
        {
            result -= 0x400;
        }
        return result;
    }

    [[nodiscard]] std::int32_t Signed16(std::uint32_t value) noexcept
    {
        std::int32_t result = static_cast<std::int32_t>(value & 0xFFFFU);
        if ((result & 0x8000) != 0)
        {
            result -= 0x10000;
        }
        return result;
    }

    // Fixed.ToFloat(int) is exactly value / 4096.0f in the native and C#
    // implementations. Keep the two-step TEXCOORD division separately below
    // because that rounding order is part of the current renderer behavior.
    [[nodiscard]] float FixedToFloat(std::int32_t value) noexcept
    {
        return static_cast<float>(value) / static_cast<float>(1 << 12);
    }

    [[nodiscard]] std::uint32_t Argument(
        const RendererGeometryInstruction& instruction, std::size_t index)
    {
        if (index >= instruction.Arguments.size())
        {
            throw RendererGeometryException("Incorrect number of render instruction arguments");
        }
        return instruction.Arguments[index];
    }

    [[nodiscard]] ScenePrimitiveTopology DecodeTopology(std::uint32_t value)
    {
        switch (value)
        {
        case 0: return ScenePrimitiveTopology::Triangles;
        case 1: return ScenePrimitiveTopology::Quads;
        case 2: return ScenePrimitiveTopology::TriangleStrip;
        case 3: return ScenePrimitiveTopology::QuadStrip;
        default: throw RendererGeometryException("Invalid geometry type");
        }
    }
}

namespace MphRead
{
    RendererGeometry DecodeRendererGeometry(
        std::span<const RendererGeometryInstruction> instructions,
        std::int32_t textureWidth, std::int32_t textureHeight,
        bool texgen, bool isRoom, std::size_t matrixCount)
    {
        RendererGeometry result;
        SceneVertex current;
        current.TexCoord = OpenTK::Mathematics::Vector2(
            texgen ? 0.5F : 0.0F, texgen ? 0.5F : 0.0F);

        SceneVertexAttributeState attributeState = SceneVertexAttributeState::None;
        std::optional<std::size_t> activeRange;

        auto emitVertex = [&]()
        {
            if (!activeRange)
            {
                throw RendererGeometryException("Vertex outside geometry range");
            }

            const std::uint32_t vertexIndex
                = static_cast<std::uint32_t>(result.Vertices.size());
            result.Vertices.push_back(current);
            result.Indices.push_back(vertexIndex);
            result.VertexAttributeStates.push_back(attributeState);
            ++result.Ranges[*activeRange].IndexCount;
        };

        for (const RendererGeometryInstruction& instruction : instructions)
        {
            switch (instruction.Code)
            {
            case InstructionCode::BEGIN_VTXS:
                if (activeRange)
                {
                    throw RendererGeometryException("Nested geometry range");
                }
                result.Ranges.push_back(ScenePrimitiveRange{
                    DecodeTopology(Argument(instruction, 0)),
                    static_cast<std::uint32_t>(result.Indices.size()),
                    0
                });
                activeRange = result.Ranges.size() - 1U;
                break;

            case InstructionCode::COLOR:
            {
                const std::uint32_t rgb = Argument(instruction, 0);
                current.Color = OpenTK::Mathematics::Vector4(
                    ((rgb >> 0U) & 0x1FU) / 31.0F,
                    ((rgb >> 5U) & 0x1FU) / 31.0F,
                    ((rgb >> 10U) & 0x1FU) / 31.0F,
                    1.0F);
                attributeState = attributeState | SceneVertexAttributeState::Color;
                break;
            }

            case InstructionCode::DIF_AMB:
            {
                const std::uint32_t rgb = Argument(instruction, 0);
                const std::uint32_t dr = (rgb >> 0U) & 0x1FU;
                const std::uint32_t dg = (rgb >> 5U) & 0x1FU;
                const std::uint32_t db = (rgb >> 10U) & 0x1FU;
                const std::uint32_t set = (rgb >> 15U) & 1U;

                // The current GLSL path uses alpha 0 as a sentinel: use this
                // per-vertex diffuse RGB and force ambient to zero. If bit 15
                // is set, the legacy Color3 call immediately restores alpha 1.
                // Ambient bits are deliberately not folded into SceneVertex;
                // current dlist semantics assert they are zero and the shader
                // sentinel path ignores them.
                current.Color = OpenTK::Mathematics::Vector4(
                    dr / 31.0F, dg / 31.0F, db / 31.0F,
                    set != 0U ? 1.0F : 0.0F);
                attributeState = attributeState | SceneVertexAttributeState::Color;
                break;
            }

            case InstructionCode::NORMAL:
            {
                const std::uint32_t xyz = Argument(instruction, 0);
                current.Normal = OpenTK::Mathematics::Vector3(
                    Signed10(xyz >> 0U) / 512.0F,
                    Signed10(xyz >> 10U) / 512.0F,
                    Signed10(xyz >> 20U) / 512.0F);
                attributeState = attributeState | SceneVertexAttributeState::Normal;
                break;
            }

            case InstructionCode::TEXCOORD:
            {
                const std::uint32_t st = Argument(instruction, 0);
                current.TexCoord = OpenTK::Mathematics::Vector2(
                    Signed16(st) / 16.0F / textureWidth,
                    Signed16(st >> 16U) / 16.0F / textureHeight);
                break;
            }

            case InstructionCode::VTX_16:
            {
                const std::uint32_t xy = Argument(instruction, 0);
                current.Position = OpenTK::Mathematics::Vector3(
                    FixedToFloat(Signed16(xy)),
                    FixedToFloat(Signed16(xy >> 16U)),
                    FixedToFloat(Signed16(Argument(instruction, 1))));
                emitVertex();
                break;
            }

            case InstructionCode::VTX_10:
            {
                const std::uint32_t xyz = Argument(instruction, 0);
                current.Position = OpenTK::Mathematics::Vector3(
                    Signed10(xyz >> 0U) / 64.0F,
                    Signed10(xyz >> 10U) / 64.0F,
                    Signed10(xyz >> 20U) / 64.0F);
                emitVertex();
                break;
            }

            case InstructionCode::VTX_XY:
            case InstructionCode::VTX_XZ:
            case InstructionCode::VTX_YZ:
            {
                const std::uint32_t pair = Argument(instruction, 0);
                if (instruction.Code == InstructionCode::VTX_XY)
                {
                    current.Position.X = FixedToFloat(Signed16(pair));
                    current.Position.Y = FixedToFloat(Signed16(pair >> 16U));
                }
                else if (instruction.Code == InstructionCode::VTX_XZ)
                {
                    current.Position.X = FixedToFloat(Signed16(pair));
                    current.Position.Z = FixedToFloat(Signed16(pair >> 16U));
                }
                else
                {
                    current.Position.Y = FixedToFloat(Signed16(pair));
                    current.Position.Z = FixedToFloat(Signed16(pair >> 16U));
                }
                emitVertex();
                break;
            }

            case InstructionCode::VTX_DIFF:
            {
                const std::uint32_t xyz = Argument(instruction, 0);
                current.Position.X += FixedToFloat(Signed10(xyz >> 0U));
                current.Position.Y += FixedToFloat(Signed10(xyz >> 10U));
                current.Position.Z += FixedToFloat(Signed10(xyz >> 20U));
                emitVertex();
                break;
            }

            case InstructionCode::END_VTXS:
                if (!activeRange)
                {
                    throw RendererGeometryException("Geometry end without begin");
                }
                activeRange.reset();
                break;

            case InstructionCode::MTX_RESTORE:
                if (!isRoom)
                {
                    const std::uint32_t requested = Argument(instruction, 0) & 0x1FU;
                    current.MatrixIndex = matrixCount == 0
                        ? 0U
                        : static_cast<std::uint32_t>(
                            std::min<std::size_t>(requested, matrixCount - 1U));
                }
                break;

            case InstructionCode::NOP:
                break;

            default:
                throw RendererGeometryException("Unknown opcode");
            }
        }

        if (activeRange)
        {
            throw RendererGeometryException("Unterminated geometry range");
        }

        result.TerminalState = current;
        result.TerminalAttributeState = attributeState;
        return result;
    }

    RendererGeometry DecodeRendererGeometry(
        const std::vector<std::shared_ptr<RenderInstruction>>& instructions,
        std::int32_t textureWidth, std::int32_t textureHeight,
        bool texgen, bool isRoom, std::size_t matrixCount)
    {
        std::vector<RendererGeometryInstruction> views;
        views.reserve(instructions.size());
        for (const std::shared_ptr<RenderInstruction>& instruction : instructions)
        {
            if (!instruction || !instruction->Arguments)
            {
                throw System::NullReferenceException();
            }
            views.push_back(RendererGeometryInstruction{
                instruction->Code,
                std::span<const std::uint32_t>(
                    instruction->Arguments->data(), instruction->Arguments->size())
            });
        }
        return DecodeRendererGeometry(
            std::span<const RendererGeometryInstruction>(views.data(), views.size()),
            textureWidth, textureHeight, texgen, isRoom, matrixCount);
    }
}
