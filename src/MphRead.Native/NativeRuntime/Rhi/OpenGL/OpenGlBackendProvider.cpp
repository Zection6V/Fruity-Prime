#include "../BackendSession.hpp"
#include "../BackendFactory.hpp"
#include "OpenGlDevice.hpp"
#include "OpenGlGeometry.hpp"
#include "OpenGlShaderInterface.hpp"

namespace MphRead::NativeRuntime::Rhi::OpenGL
{
    namespace
    {
        class Session final : public BackendSession
        {
        public:
            // GL teardown requires a current context; the window owner calls
            // Shutdown before destroying that context, never at process exit.
            ~Session() override = default;
            GraphicsBackend Backend() const noexcept override { return GraphicsBackend::OpenGl; }
            GraphicsDevice& Device() override { _active = true; return ContextDevice(); }
            bool PresentsWindow() const noexcept override { return false; }
            unsigned ValidationErrors() const noexcept override { return 0; }
            std::string Describe() override { return Device().AdapterDescription(); }
            std::unique_ptr<Swapchain> CreateSwapchain(
                ::MphRead::RendererPlatform::Window& window, const SwapchainDesc& desc) override
            { _active = true; return BackendFactory::CreateSwapchain(Backend(), window, desc); }
            PresentResult Present(Swapchain& swapchain) override { return swapchain.TryPresent(); }
            void ResetViewport(std::int32_t width, std::int32_t height) override { ResetWindowViewport(width, height); }
            void Shutdown() noexcept override
            {
                if (!_active) return;
                try { FinishContextDevice(); } catch (...) {}
                ResetContextDevice();
                _active = false;
            }
            std::unique_ptr<SceneShaderSet> CreateShaders(GraphicsDevice& device, CommandList&,
                const SceneShaderSources& sources) override { return OpenGL::CreateSceneShaderSet(device, sources); }
            std::shared_ptr<MphRead::GpuMeshResource> CreateMesh(GraphicsDevice&, CommandList&,
                const MphRead::RendererGeometry& geometry) override { return CreateGpuMeshResource(geometry); }
            std::shared_ptr<MphRead::TransientGeometryResource> CreateTransient(GraphicsDevice&, CommandList&) override
            { return CreateTransientGeometryResource(); }
        private:
            bool _active = false;
        };

        class Provider final : public BackendProvider
        {
        public:
            GraphicsBackend Backend() const noexcept override { return GraphicsBackend::OpenGl; }
            std::string ProbePassive(bool) const override { return {}; }
            std::unique_ptr<BackendSession> CreateSession(BackendSessionOptions) const override
            { return std::make_unique<Session>(); }
        };
    }
    const BackendProvider& ProviderInstance() noexcept { static Provider provider; return provider; }
}
