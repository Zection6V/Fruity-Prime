#include "SceneBackend.hpp"

#include "OpenGL/OpenGlDevice.hpp"
#include "OpenGL/OpenGlGeometry.hpp"
#include "OpenGL/OpenGlShaderInterface.hpp"

#if defined(FRUITY_HAS_VULKAN) && !defined(__ANDROID__)
#include "Vulkan/VulkanContext.hpp"
#include "Vulkan/VulkanGraphicsDevice.hpp"
#include "Vulkan/VulkanScene.hpp"
#include "Vulkan/VulkanSwapchain.hpp"
#endif
#include "BackendFactory.hpp"
#include "../../Renderer.hpp"


#include <cstdlib>
#include <stdexcept>

namespace MphRead::NativeRuntime::Rhi
{
    namespace
    {
        SceneBackendKind selected = SceneBackendKind::OpenGL;
        bool validation = false;

#if defined(FRUITY_HAS_VULKAN) && !defined(__ANDROID__)
        struct VulkanScene final
        {
            std::unique_ptr<Vulkan::Context> Context;
            std::unique_ptr<GraphicsDevice> Device;
            ::MphRead::RendererPlatform::Window* Window = nullptr;
        };

        VulkanScene& Scene()
        {
            // Never destroyed: scenes and their resources may outlive any
            // point a destructor here could run at.
            static auto* scene = new VulkanScene();
            return *scene;
        }
#endif

        bool IsVulkan(const GraphicsDevice& device) noexcept
        {
            return device.GetBackend() == GraphicsBackend::Vulkan;
        }
    }

    SceneBackendKind SelectedSceneBackend() noexcept { return selected; }
    void SelectSceneBackend(SceneBackendKind kind) noexcept { selected = kind; }
    void SetSceneValidation(bool enabled) noexcept { validation = enabled; }

    bool ParseSceneBackend(std::string_view text, SceneBackendKind& kind) noexcept
    {
        if (text == "opengl" || text == "gl") { kind = SceneBackendKind::OpenGL; return true; }
        if (text == "vulkan" || text == "vk") { kind = SceneBackendKind::Vulkan; return true; }
        return false;
    }

    std::string_view SceneBackendName(SceneBackendKind kind) noexcept
    {
        return kind == SceneBackendKind::Vulkan ? "vulkan" : "opengl";
    }

    GraphicsDevice& SceneDevice()
    {
        if (selected == SceneBackendKind::OpenGL) return OpenGL::ContextDevice();
#if defined(FRUITY_HAS_VULKAN) && !defined(__ANDROID__)
        auto& scene = Scene();
        if (!scene.Device)
        {
            scene.Context = std::make_unique<Vulkan::Context>(validation);
            scene.Device = Vulkan::CreateGraphicsDevice(*scene.Context);
        }
        return *scene.Device;
#else
        throw std::runtime_error("The Vulkan scene backend was not built.");
#endif
    }

    unsigned SceneValidationErrors() noexcept
    {
#if defined(FRUITY_HAS_VULKAN) && !defined(__ANDROID__)
        auto& scene = Scene();
        return scene.Context ? scene.Context->ValidationErrors() : 0U;
#else
        return 0U;
#endif
    }

    bool ScenePresentsWindow() noexcept
    {
        return selected == SceneBackendKind::Vulkan;
    }

    std::unique_ptr<Swapchain> CreateSceneWindowSwapchain(
        ::MphRead::RendererPlatform::Window& window, const SwapchainDesc& desc)
    {
        if (selected == SceneBackendKind::OpenGL)
            return BackendFactory::CreateSwapchain(GraphicsBackend::OpenGl, window, desc);
#if defined(FRUITY_HAS_VULKAN) && !defined(__ANDROID__)
        auto& scene = Scene();
        if (scene.Device && scene.Window != &window)
            throw std::logic_error("The Vulkan scene device already belongs to another surface.");
        if (!scene.Device)
        {
            scene.Context = std::make_unique<Vulkan::Context>(validation, window);
            scene.Device = Vulkan::CreateGraphicsDevice(*scene.Context);
            scene.Window = &window;
        }
        return Vulkan::CreateSwapchain(*scene.Context, window, desc);
#else
        throw std::runtime_error("The Vulkan scene backend was not built.");
#endif
    }

    void PresentSceneWindow(Swapchain& swapchain)
    {
#if defined(FRUITY_HAS_VULKAN) && !defined(__ANDROID__)
        if (selected == SceneBackendKind::Vulkan)
        {
            Vulkan::PresentWindow(SceneDevice(), swapchain);
            return;
        }
#endif
        swapchain.Present();
    }

    void DetachSceneWindow() noexcept
    {
#if defined(FRUITY_HAS_VULKAN) && !defined(__ANDROID__)
        auto& scene = Scene();
        if (!scene.Window) return;
        // The device stays for the process, as OpenGL's context device does:
        // the launcher's Skia surface, its overlay and its side scene are
        // statics that go at exit, after the window, and each still holds
        // Vulkan objects. Everything is finished with before the window goes.
        try
        {
            scene.Device->WaitIdle();
        }
        catch (...)
        {
        }
#endif
    }

    std::unique_ptr<SceneShaderSet> CreateSceneShaderSet(
        GraphicsDevice& device, CommandList& commands, const OpenGL::SceneShaderSources& sources)
    {
        (void)commands;
#if defined(FRUITY_HAS_VULKAN) && !defined(__ANDROID__)
        if (IsVulkan(device)) return Vulkan::CreateSceneShaderSet(device, sources.ToonTable, sources.ShiftTable);
#endif
        return OpenGL::CreateSceneShaderSet(device, sources);
    }

    std::shared_ptr<MphRead::GpuMeshResource> CreateSceneGpuMesh(
        GraphicsDevice& device, CommandList& commands, const MphRead::RendererGeometry& geometry)
    {
        (void)commands;
#if defined(FRUITY_HAS_VULKAN) && !defined(__ANDROID__)
        if (IsVulkan(device)) return Vulkan::CreateGpuMeshResource(device, commands, geometry);
#endif
        (void)device;
        return OpenGL::CreateGpuMeshResource(geometry);
    }

    std::shared_ptr<MphRead::TransientGeometryResource> CreateSceneTransientGeometry(
        GraphicsDevice& device, CommandList& commands)
    {
        (void)commands;
#if defined(FRUITY_HAS_VULKAN) && !defined(__ANDROID__)
        if (IsVulkan(device)) return Vulkan::CreateTransientGeometryResource(device, commands);
#endif
        (void)device;
        return OpenGL::CreateTransientGeometryResource();
    }
}
