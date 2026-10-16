#include "../Mods/Render/DesktopGlContext.hpp"
#include "../Mods/Render/PreviewAnimation.hpp"
#include "../Mods/Render/AnimationBlend.hpp"
#include "../Mods/RenderOptions.hpp"
#include "../Mods/ScreenCapture.hpp"
#include "../NativeRuntime/Rhi/BackendFactory.hpp"
#include "../Entities/EntityBase.hpp"
#include "../Scene.hpp"
#include "../Read.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

// Optional ROM/GPU probe, separate from the asset-free CTest suite:
// fruity_animation_loop_capture ASSET_DIRECTORY OUTPUT_DIRECTORY [MODEL]
namespace
{
    using namespace MphRead;
    using namespace MphRead::Mods::Render;

    class Capture final : public RendererPlatform::WindowEvents
    {
    public:
        Capture(std::filesystem::path output, const std::string& model) : _output(std::move(output))
        {
            auto settings = DesktopGlContext::Settings(true);
            settings.ClientSize = OpenTK::Mathematics::Vector2i(512, 512);
            settings.UpdateFrequency = 0;
            _window = RendererPlatform::CreateWindow(settings);
            NativeRuntime::Rhi::SwapchainDesc swapchain{};
            swapchain.width = swapchain.height = 512;
            _swapchain = NativeRuntime::Rhi::BackendFactory::CreateSwapchain(
                NativeRuntime::Rhi::GraphicsBackend::OpenGl, *_window, swapchain);
            _scene = std::make_unique<Scene>(_window->Size(), _window->Keyboard(), _window->Mouse(),
                [](std::string) {}, [this] { _window->Close(); });
            auto entity = _scene->AddModel(model);
            entity->Scale = OpenTK::Mathematics::Vector3(3, 3, 3);
            _instance = entity->GetModels().at(0);
            _instance->SetAnimation(0);
            std::filesystem::create_directories(_output);
        }

        void Run() { _window->Run(*this); }

        void OnLoad() override
        {
            _scene->OnLoad();
            _window->BaseOnLoad();
            _scene->OnResize();
        }

        void OnRenderFrame(const RendererPlatform::FrameEventArgs&) override
        {
            // Fixed source time, independent of wall time and GPU speed. Two
            // complete loops at four pictures per authored animation frame.
            const int pictures = (*_instance->AnimInfo->FrameCount)[0] * 8;
            for (int picture = 0; picture < pictures; ++picture)
            {
                _clock.Advance(*_instance, 1.0 / 120);
                _scene->OnDrawFrame();
                if (!_scene->OnRenderFrame()) throw std::runtime_error("scene refused animation capture");
                const auto path = _output / (std::to_string(picture + 1000) + ".png");
                if (!Mods::ScreenCapture::Save(_scene.get(), path.string()))
                    throw std::runtime_error("animation capture readback failed");
                _scene->AfterRenderFrame();
            }
            std::cout << "PASS rendered " << pictures << " pictures over two model loops\n";
            _window->Close();
        }

        void OnClosing() override
        {
            _scene->DoCleanup();
            _window->BaseOnClosing();
        }

    private:
        std::filesystem::path _output;
        std::shared_ptr<RendererPlatform::Window> _window;
        std::unique_ptr<NativeRuntime::Rhi::Swapchain> _swapchain;
        std::unique_ptr<Scene> _scene;
        std::shared_ptr<ModelInstance> _instance;
        PreviewAnimation _clock;
    };
}

int main(int argc, char** argv)
{
    try
    {
        if (argc < 3) throw std::runtime_error("expected asset directory and output directory");
        MphRead::Paths::MphKey = "AMHP1";
        MphRead::Paths::SetPath("AMHP1", argv[1]);
        MphRead::Mods::RenderOptions::CelShading(false);
        MphRead::Mods::RenderOptions::Fog(false);
        MphRead::Mods::RenderOptions::ShowFps(false);
        Capture capture(argv[2], argc > 3 ? argv[3] : "AlimbicDoorLock");
        capture.Run();
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL animation loop capture: " << error.what() << '\n';
        return 1;
    }
}
