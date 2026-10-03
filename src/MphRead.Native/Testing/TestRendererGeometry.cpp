#include "../RendererGeometry.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    using namespace MphRead;

    struct TestInstruction final
    {
        InstructionCode Code = InstructionCode::NOP;
        std::vector<std::uint32_t> Arguments{};
    };

    [[nodiscard]] TestInstruction Op(
        InstructionCode code, std::initializer_list<std::uint32_t> arguments = {})
    {
        return TestInstruction{code, std::vector<std::uint32_t>(arguments)};
    }

    [[nodiscard]] std::uint32_t Pack16(std::int32_t low, std::int32_t high) noexcept
    {
        return static_cast<std::uint32_t>(static_cast<std::uint16_t>(low))
            | (static_cast<std::uint32_t>(static_cast<std::uint16_t>(high)) << 16U);
    }

    [[nodiscard]] std::uint32_t Pack10(
        std::int32_t x, std::int32_t y, std::int32_t z) noexcept
    {
        return (static_cast<std::uint32_t>(x) & 0x3FFU)
            | ((static_cast<std::uint32_t>(y) & 0x3FFU) << 10U)
            | ((static_cast<std::uint32_t>(z) & 0x3FFU) << 20U);
    }

    [[nodiscard]] std::uint32_t PackColor5(
        std::uint32_t r, std::uint32_t g, std::uint32_t b) noexcept
    {
        return (r & 0x1FU) | ((g & 0x1FU) << 5U) | ((b & 0x1FU) << 10U);
    }

    [[nodiscard]] RendererGeometry Decode(
        const std::vector<TestInstruction>& instructions,
        std::int32_t textureWidth = 8, std::int32_t textureHeight = 8,
        bool texgen = false, bool isRoom = false, std::size_t matrixCount = 32)
    {
        std::vector<RendererGeometryInstruction> views;
        views.reserve(instructions.size());
        for (const TestInstruction& instruction : instructions)
        {
            views.push_back(RendererGeometryInstruction{
                instruction.Code,
                std::span<const std::uint32_t>(
                    instruction.Arguments.data(), instruction.Arguments.size())
            });
        }
        return DecodeRendererGeometry(
            std::span<const RendererGeometryInstruction>(views.data(), views.size()),
            textureWidth, textureHeight, texgen, isRoom, matrixCount);
    }

    [[noreturn]] void Fail(std::string_view message)
    {
        throw std::runtime_error(std::string(message));
    }

    void Expect(bool value, std::string_view message)
    {
        if (!value)
        {
            Fail(message);
        }
    }

    void ExpectFloat(float actual, float expected, std::string_view message)
    {
        if (std::fabs(actual - expected) > 0.000001F)
        {
            Fail(message);
        }
    }

    void ExpectVector3(
        OpenTK::Mathematics::Vector3 actual,
        OpenTK::Mathematics::Vector3 expected,
        std::string_view message)
    {
        ExpectFloat(actual.X, expected.X, message);
        ExpectFloat(actual.Y, expected.Y, message);
        ExpectFloat(actual.Z, expected.Z, message);
    }

    void ExpectVector2(
        OpenTK::Mathematics::Vector2 actual,
        OpenTK::Mathematics::Vector2 expected,
        std::string_view message)
    {
        ExpectFloat(actual.X, expected.X, message);
        ExpectFloat(actual.Y, expected.Y, message);
    }

    void ExpectVector4(
        OpenTK::Mathematics::Vector4 actual,
        OpenTK::Mathematics::Vector4 expected,
        std::string_view message)
    {
        ExpectFloat(actual.X, expected.X, message);
        ExpectFloat(actual.Y, expected.Y, message);
        ExpectFloat(actual.Z, expected.Z, message);
        ExpectFloat(actual.W, expected.W, message);
    }

    void ExpectIndices(
        const RendererGeometry& geometry, std::initializer_list<std::uint32_t> expected)
    {
        Expect(geometry.Indices.size() == expected.size(), "index count");
        std::size_t index = 0;
        for (std::uint32_t value : expected)
        {
            if (geometry.Indices[index++] != value)
            {
                Fail("index order");
            }
        }
    }

    void ExpectRange(
        const RendererGeometry& geometry, std::size_t rangeIndex,
        ScenePrimitiveTopology topology, std::uint32_t first, std::uint32_t count)
    {
        Expect(rangeIndex < geometry.Ranges.size(), "range index");
        const ScenePrimitiveRange& range = geometry.Ranges[rangeIndex];
        Expect(range.Topology == topology, "range topology");
        Expect(range.FirstIndex == first, "range first index");
        Expect(range.IndexCount == count, "range index count");
    }

    void TestTriangle()
    {
        const RendererGeometry geometry = Decode({
            Op(InstructionCode::BEGIN_VTXS, {0}),
            Op(InstructionCode::VTX_10, {Pack10(0, 0, 0)}),
            Op(InstructionCode::VTX_10, {Pack10(64, 0, 0)}),
            Op(InstructionCode::VTX_10, {Pack10(0, 64, 0)}),
            Op(InstructionCode::END_VTXS)
        });
        Expect(geometry.Vertices.size() == 3, "triangle vertex count");
        ExpectIndices(geometry, {0, 1, 2});
        Expect(geometry.Ranges.size() == 1, "triangle range count");
        ExpectRange(geometry, 0, ScenePrimitiveTopology::Triangles, 0, 3);
        ExpectVector3(geometry.Vertices[1].Position, {1.0F, 0.0F, 0.0F},
            "triangle position");
    }

    void TestQuadPreservesSourceTopology()
    {
        const RendererGeometry geometry = Decode({
            Op(InstructionCode::BEGIN_VTXS, {1}),
            Op(InstructionCode::VTX_10, {Pack10(0, 0, 0)}),
            Op(InstructionCode::VTX_10, {Pack10(64, 0, 0)}),
            Op(InstructionCode::VTX_10, {Pack10(64, 64, 0)}),
            Op(InstructionCode::VTX_10, {Pack10(0, 64, 0)}),
            Op(InstructionCode::END_VTXS)
        });
        ExpectIndices(geometry, {0, 1, 2, 3});
        ExpectRange(geometry, 0, ScenePrimitiveTopology::Quads, 0, 4);
    }

    void TestTriangleStripBothParityCases()
    {
        const RendererGeometry geometry = Decode({
            Op(InstructionCode::BEGIN_VTXS, {2}),
            Op(InstructionCode::VTX_10, {Pack10(0, 0, 0)}),
            Op(InstructionCode::VTX_10, {Pack10(64, 0, 0)}),
            Op(InstructionCode::VTX_10, {Pack10(0, 64, 0)}),
            Op(InstructionCode::VTX_10, {Pack10(64, 64, 0)}),
            Op(InstructionCode::VTX_10, {Pack10(0, 128, 0)}),
            Op(InstructionCode::END_VTXS)
        });
        ExpectIndices(geometry, {0, 1, 2, 3, 4});
        ExpectRange(geometry, 0, ScenePrimitiveTopology::TriangleStrip, 0, 5);

        // The decoder preserves the source strip rather than expanding it.
        // These are OpenGL's first even and odd strip triangles respectively:
        // even: (0,1,2), odd: (2,1,3). A source-order change would change
        // either parity when the legacy consumer submits this range.
        const auto& indices = geometry.Indices;
        const std::array<std::uint32_t, 3> even{
            indices[0], indices[1], indices[2]
        };
        const std::array<std::uint32_t, 3> odd{
            indices[2], indices[1], indices[3]
        };
        Expect(even == std::array<std::uint32_t, 3>{0, 1, 2},
            "triangle strip even parity");
        Expect(odd == std::array<std::uint32_t, 3>{2, 1, 3},
            "triangle strip odd parity");
    }

    void TestQuadStrip()
    {
        const RendererGeometry geometry = Decode({
            Op(InstructionCode::BEGIN_VTXS, {3}),
            Op(InstructionCode::VTX_10, {Pack10(0, 0, 0)}),
            Op(InstructionCode::VTX_10, {Pack10(0, 64, 0)}),
            Op(InstructionCode::VTX_10, {Pack10(64, 0, 0)}),
            Op(InstructionCode::VTX_10, {Pack10(64, 64, 0)}),
            Op(InstructionCode::VTX_10, {Pack10(128, 0, 0)}),
            Op(InstructionCode::VTX_10, {Pack10(128, 64, 0)}),
            Op(InstructionCode::END_VTXS)
        });
        ExpectIndices(geometry, {0, 1, 2, 3, 4, 5});
        ExpectRange(geometry, 0, ScenePrimitiveTopology::QuadStrip, 0, 6);
    }

    void TestAttributeInheritanceAndCapture()
    {
        const std::uint32_t colorA = PackColor5(31, 5, 1);
        const std::uint32_t colorB = PackColor5(1, 31, 2);
        const RendererGeometry geometry = Decode({
            Op(InstructionCode::COLOR, {colorA}),
            Op(InstructionCode::NORMAL, {Pack10(256, -256, 0)}),
            Op(InstructionCode::TEXCOORD, {Pack16(32, -16)}),
            Op(InstructionCode::NOP),
            Op(InstructionCode::BEGIN_VTXS, {0}),
            Op(InstructionCode::VTX_10, {Pack10(0, 0, 0)}),
            Op(InstructionCode::VTX_10, {Pack10(64, 0, 0)}),
            Op(InstructionCode::COLOR, {colorB}),
            Op(InstructionCode::VTX_10, {Pack10(0, 64, 0)}),
            Op(InstructionCode::END_VTXS)
        }, 4, 2);

        ExpectVector4(geometry.Vertices[0].Color,
            {31.0F / 31.0F, 5.0F / 31.0F, 1.0F / 31.0F, 1.0F},
            "captured color");
        ExpectVector4(geometry.Vertices[1].Color, geometry.Vertices[0].Color,
            "inherited color");
        ExpectVector4(geometry.Vertices[2].Color,
            {1.0F / 31.0F, 31.0F / 31.0F, 2.0F / 31.0F, 1.0F},
            "replacement color");
        for (const SceneVertex& vertex : geometry.Vertices)
        {
            ExpectVector3(vertex.Normal, {0.5F, -0.5F, 0.0F},
                "inherited normal");
            ExpectVector2(vertex.TexCoord, {0.5F, -0.5F},
                "inherited texcoord");
        }
        const SceneVertexAttributeState both
            = SceneVertexAttributeState::Color | SceneVertexAttributeState::Normal;
        for (SceneVertexAttributeState state : geometry.VertexAttributeStates)
        {
            Expect(state == both, "attribute state capture");
        }
    }

    void TestExternalInheritanceAndTerminalState()
    {
        const RendererGeometry geometry = Decode({
            Op(InstructionCode::BEGIN_VTXS, {0}),
            Op(InstructionCode::VTX_10, {Pack10(0, 0, 0)}),
            Op(InstructionCode::VTX_10, {Pack10(64, 0, 0)}),
            Op(InstructionCode::VTX_10, {Pack10(0, 64, 0)}),
            Op(InstructionCode::END_VTXS),
            Op(InstructionCode::COLOR, {PackColor5(4, 5, 6)}),
            Op(InstructionCode::NORMAL, {Pack10(0, 0, 256)})
        });

        for (SceneVertexAttributeState state : geometry.VertexAttributeStates)
        {
            Expect(state == SceneVertexAttributeState::None,
                "pre-COLOR/NORMAL state must stay externally inherited");
        }
        Expect(HasSceneVertexAttributeState(
            geometry.TerminalAttributeState, SceneVertexAttributeState::Color),
            "terminal color state");
        Expect(HasSceneVertexAttributeState(
            geometry.TerminalAttributeState, SceneVertexAttributeState::Normal),
            "terminal normal state");
        ExpectVector4(geometry.TerminalState.Color,
            {4.0F / 31.0F, 5.0F / 31.0F, 6.0F / 31.0F, 1.0F},
            "terminal color value");
        ExpectVector3(geometry.TerminalState.Normal, {0.0F, 0.0F, 0.5F},
            "terminal normal value");
    }

    void TestTexgenInitialTexcoord()
    {
        const RendererGeometry geometry = Decode({
            Op(InstructionCode::BEGIN_VTXS, {0}),
            Op(InstructionCode::VTX_10, {Pack10(0, 0, 0)}),
            Op(InstructionCode::VTX_10, {Pack10(64, 0, 0)}),
            Op(InstructionCode::VTX_10, {Pack10(0, 64, 0)}),
            Op(InstructionCode::END_VTXS)
        }, 8, 8, true);

        for (const SceneVertex& vertex : geometry.Vertices)
        {
            ExpectVector2(vertex.TexCoord, {0.5F, 0.5F}, "texgen initial texcoord");
        }
    }

    void TestEveryVertexEncoding()
    {
        const RendererGeometry geometry = Decode({
            Op(InstructionCode::BEGIN_VTXS, {0}),
            Op(InstructionCode::VTX_16, {
                Pack16(4096, -4096),
                static_cast<std::uint32_t>(static_cast<std::uint16_t>(2048))
            }),
            Op(InstructionCode::VTX_XY, {Pack16(8192, 12288)}),
            Op(InstructionCode::VTX_XZ, {Pack16(-4096, 4096)}),
            Op(InstructionCode::VTX_YZ, {Pack16(-8192, -4096)}),
            Op(InstructionCode::VTX_10, {Pack10(64, -128, 192)}),
            Op(InstructionCode::VTX_DIFF, {Pack10(64, -64, 32)}),
            Op(InstructionCode::END_VTXS)
        });

        Expect(geometry.Vertices.size() == 6, "all VTX forms vertex count");
        ExpectVector3(geometry.Vertices[0].Position, {1.0F, -1.0F, 0.5F}, "VTX_16");
        ExpectVector3(geometry.Vertices[1].Position, {2.0F, 3.0F, 0.5F}, "VTX_XY");
        ExpectVector3(geometry.Vertices[2].Position, {-1.0F, 3.0F, 1.0F}, "VTX_XZ");
        ExpectVector3(geometry.Vertices[3].Position, {-1.0F, -2.0F, -1.0F}, "VTX_YZ");
        ExpectVector3(geometry.Vertices[4].Position, {1.0F, -2.0F, 3.0F}, "VTX_10");
        ExpectVector3(geometry.Vertices[5].Position,
            {1.015625F, -2.015625F, 3.0078125F}, "VTX_DIFF");
    }

    void TestMatrixRestoreMaskClampAndRoomRule()
    {
        const std::vector<TestInstruction> instructions{
            Op(InstructionCode::BEGIN_VTXS, {0}),
            Op(InstructionCode::MTX_RESTORE, {0xFFFFFFE2U}),
            Op(InstructionCode::VTX_10, {Pack10(0, 0, 0)}),
            Op(InstructionCode::MTX_RESTORE, {0x0000003FU}),
            Op(InstructionCode::VTX_10, {Pack10(64, 0, 0)}),
            Op(InstructionCode::MTX_RESTORE, {0x00000020U}),
            Op(InstructionCode::VTX_10, {Pack10(0, 64, 0)}),
            Op(InstructionCode::END_VTXS)
        };

        const RendererGeometry geometry = Decode(instructions, 8, 8, false, false, 4);
        Expect(geometry.Vertices[0].MatrixIndex == 2, "MTX_RESTORE 0x1F mask");
        Expect(geometry.Vertices[1].MatrixIndex == 3, "MTX_RESTORE clamp");
        Expect(geometry.Vertices[2].MatrixIndex == 0, "MTX_RESTORE masked zero");

        const RendererGeometry room = Decode(instructions, 8, 8, false, true, 4);
        for (const SceneVertex& vertex : room.Vertices)
        {
            Expect(vertex.MatrixIndex == 0, "room MTX_RESTORE must remain zero");
        }

        const RendererGeometry emptyStack = Decode(instructions, 8, 8, false, false, 0);
        for (const SceneVertex& vertex : emptyStack.Vertices)
        {
            Expect(vertex.MatrixIndex == 0, "empty matrix stack clamp");
        }
    }

    void TestDifAmbSentinel()
    {
        const std::uint32_t diffuse = PackColor5(31, 16, 1);
        std::vector<TestInstruction> instructions{
            Op(InstructionCode::BEGIN_VTXS, {0}),
            Op(InstructionCode::DIF_AMB, {diffuse}),
            Op(InstructionCode::VTX_10, {Pack10(0, 0, 0)})
        };
#if !defined(DEBUG)
        // In DEBUG the legacy code asserts before the bit-15 Color3 path.
        // Release behavior is still required to restore alpha to 1.
        instructions.push_back(Op(InstructionCode::DIF_AMB, {diffuse | (1U << 15U)}));
        instructions.push_back(Op(InstructionCode::VTX_10, {Pack10(64, 0, 0)}));
#endif
        instructions.push_back(Op(InstructionCode::DIF_AMB, {diffuse}));
        instructions.push_back(Op(InstructionCode::VTX_10, {Pack10(0, 64, 0)}));
        instructions.push_back(Op(InstructionCode::END_VTXS));

        const RendererGeometry geometry = Decode(instructions);

        ExpectVector4(geometry.Vertices[0].Color,
            {1.0F, 16.0F / 31.0F, 1.0F / 31.0F, 0.0F},
            "DIF_AMB alpha sentinel");
#if !defined(DEBUG)
        ExpectFloat(geometry.Vertices[1].Color.W, 1.0F, "DIF_AMB set bit Color3 alpha");
        ExpectFloat(geometry.Vertices[2].Color.W, 0.0F, "DIF_AMB sentinel restored");
#else
        ExpectFloat(geometry.Vertices[1].Color.W, 0.0F, "DIF_AMB sentinel retained");
#endif
        for (SceneVertexAttributeState state : geometry.VertexAttributeStates)
        {
            Expect(HasSceneVertexAttributeState(state, SceneVertexAttributeState::Color),
                "DIF_AMB must mark color explicit");
        }
    }
}

int main()
{
    try
    {
        TestTriangle();
        TestQuadPreservesSourceTopology();
        TestTriangleStripBothParityCases();
        TestQuadStrip();
        TestAttributeInheritanceAndCapture();
        TestExternalInheritanceAndTerminalState();
        TestTexgenInitialTexcoord();
        TestEveryVertexEncoding();
        TestMatrixRestoreMaskClampAndRoomRule();
        TestDifAmbSentinel();
        std::cout << "RendererGeometry tests passed\n";
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "RendererGeometry test failure: " << ex.what() << '\n';
        return 1;
    }
}
