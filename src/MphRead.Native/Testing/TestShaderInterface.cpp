#include "../NativeRuntime/Rhi/VertexSemantics.hpp"
#include "../NativeRuntime/Rhi/VulkanShaderInterface.hpp"
#include "../Shaders.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Phase 5's contract, checked without a GL context: one semantic table, every
// backend's numbering distinct within itself, desktop shaders that read only
// the semantic inputs, and a Vulkan interface whose locations and std140
// packing agree with the frontend's constant groups.
namespace
{
    using namespace MphRead::NativeRuntime::Rhi;

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

    void ExpectDistinct(const std::array<std::uint32_t, VertexSemanticCount>& table, std::string_view name)
    {
        const std::set<std::uint32_t> unique(table.begin(), table.end());
        Expect(unique.size() == table.size(), std::string(name) + " reuses a location");
    }

    void TestTablesAreDistinct()
    {
        ExpectDistinct(OpenGlDesktopLocations, "OpenGlDesktopLocations");
        ExpectDistinct(OpenGlEsLocations, "OpenGlEsLocations");
        ExpectDistinct(VulkanLocations, "VulkanLocations");
        const std::set<std::string_view> names(VertexSemanticNames.begin(), VertexSemanticNames.end());
        Expect(names.size() == VertexSemanticCount, "semantic names are not unique");
    }

    void TestDesktopUsesTheAliasTable()
    {
        // NV_vertex_program: vertex 0, normal 2, colour 3, texcoord0 8, texcoord1 9.
        Expect(Location(OpenGlDesktopLocations, VertexSemantic::Position) == 0U, "desktop position");
        Expect(Location(OpenGlDesktopLocations, VertexSemantic::Normal) == 2U, "desktop normal");
        Expect(Location(OpenGlDesktopLocations, VertexSemantic::Color) == 3U, "desktop colour");
        Expect(Location(OpenGlDesktopLocations, VertexSemantic::TexCoord) == 8U, "desktop texcoord");
        Expect(Location(OpenGlDesktopLocations, VertexSemantic::TexCoord1) == 9U, "desktop texcoord1");
    }

    void TestVulkanMatchesThePlan()
    {
        Expect(Location(VulkanLocations, VertexSemantic::Position) == 0U, "vulkan position");
        Expect(Location(VulkanLocations, VertexSemantic::Normal) == 1U, "vulkan normal");
        Expect(Location(VulkanLocations, VertexSemantic::Color) == 2U, "vulkan colour");
        Expect(Location(VulkanLocations, VertexSemantic::TexCoord) == 3U, "vulkan texcoord");
        for (std::size_t i = 0; i < VertexSemanticCount; ++i)
        {
            const std::string declared = "layout(location = " + std::to_string(VulkanLocations[i])
                + ") in ";
            const std::size_t at = Vulkan::VertexInterfaceGlsl.find(declared);
            Expect(at != std::string_view::npos, "Vulkan GLSL does not declare location "
                + std::to_string(VulkanLocations[i]));
            const std::size_t end = Vulkan::VertexInterfaceGlsl.find(';', at);
            const std::string_view line = Vulkan::VertexInterfaceGlsl.substr(at, end - at);
            Expect(line.ends_with(VertexSemanticNames[i]),
                "Vulkan GLSL location " + std::to_string(VulkanLocations[i]) + " is not "
                + std::string(VertexSemanticNames[i]));
        }
    }

    void ExpectNoBuiltinInputs(const std::string& source, std::string_view name)
    {
        for (const char* builtin : {"gl_Vertex", "gl_Normal", "gl_Color", "gl_MultiTexCoord",
                 "gl_SecondaryColor", "gl_FogCoord"})
        {
            Expect(source.find(builtin) == std::string::npos,
                std::string(name) + " still reads " + builtin);
        }
    }

    void ExpectOnlySemanticAttributes(const std::string& source, std::string_view name)
    {
        std::size_t at = 0;
        while ((at = source.find("attribute ", at)) != std::string::npos)
        {
            const std::size_t end = source.find(';', at);
            const std::string line = source.substr(at, end - at);
            const std::string input = line.substr(line.find_last_of(' ') + 1U);
            Expect(std::find(VertexSemanticNames.begin(), VertexSemanticNames.end(), input)
                    != VertexSemanticNames.end(),
                std::string(name) + " declares a non-semantic input " + input);
            at = end;
        }
    }

    void TestDesktopShadersUseExplicitInputs()
    {
        using MphRead::Shaders;
        const std::pair<const std::string*, std::string_view> sources[]{
            {&Shaders::VertexShader, "VertexShader"},
            {&Shaders::FragmentShader, "FragmentShader"},
            {&Shaders::BackdropVertexShader, "BackdropVertexShader"},
            {&Shaders::BackdropFragmentShader, "BackdropFragmentShader"},
            {&Shaders::RttVertexShader, "RttVertexShader"},
            {&Shaders::RttFragmentShader, "RttFragmentShader"},
            {&Shaders::CelFragmentShader, "CelFragmentShader"},
            {&Shaders::ShiftFragmentShader, "ShiftFragmentShader"},
        };
        for (const auto& [source, name] : sources)
        {
            ExpectNoBuiltinInputs(*source, name);
            ExpectOnlySemanticAttributes(*source, name);
        }
        Expect(Shaders::VertexShader.find("attribute vec4 a_position;") != std::string::npos,
            "main vertex shader has no position input");
    }

    void TestVulkanPacking()
    {
        using ::OpenTK::Mathematics::Vector3;
        using ::OpenTK::Mathematics::Vector4;
        const MaterialConstants material{true, Vector3(1, 2, 3), Vector3(4, 5, 6),
            Vector3(7, 8, 9), Vector3(10, 11, 12), 0.5F, 3};
        const Vulkan::MaterialBlockData packed = Vulkan::Pack(material);
        Expect(packed.Diffuse.X == 1.0F && packed.Diffuse.Z == 3.0F, "material diffuse");
        Expect(packed.Emission.Y == 11.0F, "material emission");
        Expect(packed.Alpha == 0.5F && packed.PolygonMode == 3 && packed.UseLight == 1U,
            "material scalars");

        const SceneFogConstants fog{Vector4(0.1F, 0.2F, 0.3F, 0.4F), 0.25F, 0.75F};
        const Vulkan::SceneFogBlockData fogData = Vulkan::Pack(fog);
        Expect(fogData.Color[3] == 0.4F && fogData.MinDistance == 0.25F
            && fogData.MaxDistance == 0.75F, "fog packing");

        std::vector<float> stack(40U * 16U);
        for (std::size_t i = 0; i < stack.size(); ++i)
        {
            stack[i] = static_cast<float>(i);
        }
        const Vulkan::DrawBlockData draw = Vulkan::Pack(DrawConstants{stack});
        Expect(draw.MatrixStack[0] == 0.0F
            && draw.MatrixStack[MatrixStackCapacity * 16U - 1U]
                == static_cast<float>(MatrixStackCapacity * 16U - 1U),
            "a matrix stack longer than 32 is cut at 32");

        const CelPostConstants cel{1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, true};
        const Vulkan::CelPostBlockData celData = Vulkan::Pack(cel);
        Expect(celData.DepthQuantum == 6.0F && celData.Probe == 1U, "cel packing");

        const HudPostConstants hud{Vector4(1.0F, 0.5F, 0.25F, 0.125F), 0.75F, true, 640.0F, 480.0F};
        const Vulkan::HudPostBlockData hudData = Vulkan::Pack(hud);
        Expect(hudData.FadeColor[3] == 0.125F && hudData.LayerAlpha == 0.75F
            && hudData.UseMask == 1U && hudData.ViewHeight == 480.0F, "HUD post packing");

        const Vulkan::DisruptionPostBlockData disruption
            = Vulkan::Pack(DisruptionPostConstants{0.5F, 7, 0.25F, -1.0F});
        Expect(disruption.ShiftIndex == 7 && disruption.WhiteoutFactor == -1.0F,
            "disruption packing");
        Expect(sizeof(Vulkan::DisruptionTablesBlockData) / 16U == 16U + 48U,
            "disruption tables are 16 + 48 vec4");
    }
}

int main()
{
    try
    {
        TestTablesAreDistinct();
        TestDesktopUsesTheAliasTable();
        TestVulkanMatchesThePlan();
        TestDesktopShadersUseExplicitInputs();
        TestVulkanPacking();
        std::cout << "ShaderInterface tests passed.\n";
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "ShaderInterface test failure: " << ex.what() << '\n';
        return 1;
    }
}
