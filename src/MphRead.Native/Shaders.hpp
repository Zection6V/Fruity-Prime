#pragma once

#include <cstdint>
#include <string>

namespace MphRead
{
    class Shaders final
    {
    public:
        Shaders() = delete;
        Shaders(const Shaders&) = delete;
        Shaders(Shaders&&) = delete;
        Shaders& operator=(const Shaders&) = delete;
        Shaders& operator=(Shaders&&) = delete;

        static const std::string VertexShader;
        static const std::string FragmentShader;
        static const std::string RttVertexShader;
        static const std::string RttFragmentShader;
        static const std::string CelFragmentShader;
        static const std::string ShiftFragmentShader;
    };

    class ShaderLocations
    {
    public:
        std::int32_t UseLight = 0;
        std::int32_t ShowColors = 0;
        std::int32_t UseTexture = 0;
        std::int32_t Light1Color = 0;
        std::int32_t Light1Vector = 0;
        std::int32_t Light2Color = 0;
        std::int32_t Light2Vector = 0;
        std::int32_t Diffuse = 0;
        std::int32_t Ambient = 0;
        std::int32_t Specular = 0;
        std::int32_t Emission = 0;
        std::int32_t UseFog = 0;
        std::int32_t CelBands = 0;
        std::int32_t UseFlat = 0;
        std::int32_t FlatColor = 0;
        std::int32_t CelOutline = 0;
        std::int32_t CelTexelWidth = 0;
        std::int32_t CelTexelHeight = 0;
        std::int32_t CelNearPlane = 0;
        std::int32_t CelFarPlane = 0;
        std::int32_t CelDepthQuantum = 0;
        std::int32_t CelProbe = 0;
        std::int32_t FogColor = 0;
        std::int32_t FogMinDistance = 0;
        std::int32_t FogMaxDistance = 0;
        std::int32_t UseOverride = 0;
        std::int32_t OverrideColor = 0;
        std::int32_t UsePaletteOverride = 0;
        std::int32_t PaletteOverrideColor = 0;
        std::int32_t MaterialAlpha = 0;
        std::int32_t MaterialMode = 0;
        std::int32_t ViewMatrix = 0;
        std::int32_t ViewInvMatrix = 0;
        std::int32_t ProjectionMatrix = 0;
        std::int32_t TextureMatrix = 0;
        std::int32_t TexgenMode = 0;
        std::int32_t MatrixStack = 0;
        std::int32_t ToonTable = 0;
        std::int32_t FadeColor = 0;
        std::int32_t LayerAlpha = 0;
        std::int32_t UseMask = 0;
        std::int32_t ViewWidth = 0;
        std::int32_t ViewHeight = 0;
        std::int32_t ShiftTable = 0;
        std::int32_t ShiftIndex = 0;
        std::int32_t ShiftFactor = 0;
        std::int32_t LerpFactor = 0;
        std::int32_t WhiteoutTable = 0;
        std::int32_t WhiteoutFactor = 0;
    };
}
