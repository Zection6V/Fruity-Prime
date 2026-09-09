#pragma once

#include <string_view>

namespace fruityprime::shaders {

enum class Program {
    Vertex,
    Fragment,
    RttVertex,
    RttFragment,
    CelFragment,
    ShiftFragment,
    // Compatibility names retained for the first native facade.
    Textured = Fragment,
    Colored = Fragment,
    Hud = Fragment
};

// Shaders.ShaderLocations.  OpenTK fills these with the locations returned by
// the linked program; keeping the managed property names and zero defaults is
// important because a newly created instance is passed to the renderer before
// every optional uniform has been discovered.
struct ShaderLocations {
    int UseLight = 0;
    int ShowColors = 0;
    int UseTexture = 0;
    int Light1Color = 0;
    int Light1Vector = 0;
    int Light2Color = 0;
    int Light2Vector = 0;
    int Diffuse = 0;
    int Ambient = 0;
    int Specular = 0;
    int Emission = 0;
    int UseFog = 0;
    int CelBands = 0;
    int UseFlat = 0;
    int FlatColor = 0;
    int CelOutline = 0;
    int CelTexelWidth = 0;
    int CelTexelHeight = 0;
    int CelNearPlane = 0;
    int CelFarPlane = 0;
    int CelDepthQuantum = 0;
    int CelProbe = 0;
    int FogColor = 0;
    int FogMinDistance = 0;
    int FogMaxDistance = 0;
    int UseOverride = 0;
    int OverrideColor = 0;
    int UsePaletteOverride = 0;
    int PaletteOverrideColor = 0;
    int MaterialAlpha = 0;
    int MaterialMode = 0;
    int ViewMatrix = 0;
    int ViewInvMatrix = 0;
    int ProjectionMatrix = 0;
    int TextureMatrix = 0;
    int TexgenMode = 0;
    int MatrixStack = 0;
    int ToonTable = 0;
    int FadeColor = 0;
    int LayerAlpha = 0;
    int UseMask = 0;
    int ViewWidth = 0;
    int ViewHeight = 0;
    int ShiftTable = 0;
    int ShiftIndex = 0;
    int ShiftFactor = 0;
    int LerpFactor = 0;
    int WhiteoutTable = 0;
    int WhiteoutFactor = 0;
};

[[nodiscard]] std::string_view vertex_shader() noexcept;
[[nodiscard]] std::string_view fragment_shader() noexcept;
[[nodiscard]] std::string_view rtt_vertex_shader() noexcept;
[[nodiscard]] std::string_view rtt_fragment_shader() noexcept;
[[nodiscard]] std::string_view cel_fragment_shader() noexcept;
[[nodiscard]] std::string_view shift_fragment_shader() noexcept;

[[nodiscard]] std::string_view vertex_source(Program program) noexcept;
[[nodiscard]] std::string_view fragment_source(Program program) noexcept;
[[nodiscard]] bool has_required_entry_points(Program program) noexcept;

} // namespace fruityprime::shaders

namespace MphReadNative {
namespace Shaders = ::fruityprime::shaders;
}
