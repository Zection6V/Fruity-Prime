#include "OpenGlShaderInterface.hpp"

#include "../../OpenTK/GL.hpp"

#include <algorithm>
#include <utility>

namespace MphRead::NativeRuntime::Rhi::OpenGL
{
    namespace
    {
        namespace GL = ::OpenTK::Graphics::OpenGL::GL;

        class OpenGlShaderConstants final : public ShaderConstantSink
        {
        public:
            explicit OpenGlShaderConstants(std::shared_ptr<const ShaderLocations> locations)
                : _locations(std::move(locations))
            {
            }

            void Set(const FrameConstants& constants) override
            {
                GL::UniformMatrix4(_locations->ViewMatrix, false, constants.View);
                GL::UniformMatrix4(_locations->ProjectionMatrix, false, constants.Projection);
            }

            void Set(const SceneLightConstants& constants) override
            {
                for (std::size_t i = 0; i < SceneLightCount; ++i)
                {
                    SetLight(i, constants.Lights[i]);
                }
            }

            void SetLight(std::size_t index, const LightConstants& constants) override
            {
                if (index == 0)
                {
                    GL::Uniform3(_locations->Light1Vector, constants.Vector);
                    GL::Uniform3(_locations->Light1Color, constants.Color);
                }
                else if (index == 1)
                {
                    GL::Uniform3(_locations->Light2Vector, constants.Vector);
                    GL::Uniform3(_locations->Light2Color, constants.Color);
                }
            }

            void Set(const SceneFogConstants& constants) override
            {
                GL::Uniform4(_locations->FogColor, constants.Color);
                GL::Uniform1(_locations->FogMinDistance, constants.MinDistance);
                GL::Uniform1(_locations->FogMaxDistance, constants.MaxDistance);
            }

            void Set(const MaterialConstants& constants) override
            {
                GL::Uniform1(_locations->UseLight, constants.UseLight ? 1 : 0);
                GL::Uniform3(_locations->Diffuse, constants.Diffuse);
                GL::Uniform3(_locations->Ambient, constants.Ambient);
                GL::Uniform3(_locations->Specular, constants.Specular);
                GL::Uniform3(_locations->Emission, constants.Emission);
                GL::Uniform1(_locations->MaterialAlpha, constants.Alpha);
                GL::Uniform1(_locations->MaterialMode, constants.PolygonMode);
            }

            void Set(const DrawConstants& constants) override
            {
                const std::size_t count = std::min(
                    constants.MatrixStack.size() / 16U, MatrixStackCapacity);
                if (count == 0)
                {
                    return;
                }
                GL::UniformMatrix4(_locations->MatrixStack, static_cast<std::int32_t>(count),
                    false, constants.MatrixStack.data());
            }

            void Set(const CelPostConstants& constants) override
            {
                GL::Uniform1(_locations->CelTexelWidth, constants.TexelWidth);
                GL::Uniform1(_locations->CelTexelHeight, constants.TexelHeight);
                GL::Uniform1(_locations->CelOutline, constants.Outline);
                GL::Uniform1(_locations->CelNearPlane, constants.NearPlane);
                GL::Uniform1(_locations->CelFarPlane, constants.FarPlane);
                GL::Uniform1(_locations->CelDepthQuantum, constants.DepthQuantum);
                GL::Uniform1(_locations->CelProbe, constants.Probe ? 1 : 0);
            }

        private:
            std::shared_ptr<const ShaderLocations> _locations;
        };
    }

    std::shared_ptr<ShaderConstantSink> CreateShaderConstantSink(
        std::shared_ptr<const ShaderLocations> locations)
    {
        return std::make_shared<OpenGlShaderConstants>(std::move(locations));
    }
}
