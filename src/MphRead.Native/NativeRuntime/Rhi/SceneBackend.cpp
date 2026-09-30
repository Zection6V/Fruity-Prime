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
#include "../Skia/VulkanInterop.hpp"
#include "../../Renderer.hpp"

#if !defined(__ANDROID__)
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif


#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace MphRead::NativeRuntime::Rhi
{
    namespace
    {
        SceneBackendKind selected = SceneBackendKind::OpenGL;
        SceneBackendRequest requested = SceneBackendRequest::OpenGL;
        bool requestExplicit = false;
        bool resolved = false;
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

    void RequestSceneBackend(SceneBackendRequest request, bool explicitRequest) noexcept
    {
        if (resolved || (requestExplicit && !explicitRequest)) return;
        requested = request;
        requestExplicit = requestExplicit || explicitRequest;
    }

    SceneBackendRequest RequestedSceneBackend() noexcept { return requested; }

    bool ParseSceneBackendRequest(std::string_view text, SceneBackendRequest& request) noexcept
    {
        if (text == "opengl" || text == "gl") { request = SceneBackendRequest::OpenGL; return true; }
        if (text == "vulkan" || text == "vk") { request = SceneBackendRequest::Vulkan; return true; }
        if (text == "auto") { request = SceneBackendRequest::Auto; return true; }
        return false;
    }

    std::string_view SceneBackendRequestName(SceneBackendRequest request) noexcept
    {
        switch (request)
        {
        case SceneBackendRequest::Vulkan: return "vulkan";
        case SceneBackendRequest::Auto: return "auto";
        case SceneBackendRequest::OpenGL:
        default: return "opengl";
        }
    }

    std::string VulkanUnavailableReason(bool forWindow)
    {
#if defined(FRUITY_HAS_VULKAN) && !defined(__ANDROID__)
        if (forWindow && !Skia::VulkanInterop::Available())
            return "this build's Skia has no Vulkan backend, so the launcher cannot draw into a Vulkan window";
        if (::glfwInit() != GLFW_TRUE) return "GLFW could not be initialised";
        if (::glfwVulkanSupported() != GLFW_TRUE) return "no Vulkan loader or driver was found";
        if (Scene().Device) return {};
        try
        {
            // A device with everything the backend needs, made and let go.
            Vulkan::Context probe(false);
        }
        catch (const std::exception& ex)
        {
            return ex.what();
        }
        return {};
#else
        (void)forWindow;
        return "this build has no Vulkan backend";
#endif
    }

    SceneBackendKind SelectedSceneBackend()
    {
        if (resolved) return selected;
        resolved = true;
        if (requested == SceneBackendRequest::OpenGL)
        {
            selected = SceneBackendKind::OpenGL;
            return selected;
        }
#if defined(MPHREAD_SHELL)
        constexpr bool window = true;
#else
        constexpr bool window = false;
#endif
        const std::string why = VulkanUnavailableReason(window);
        if (requested == SceneBackendRequest::Vulkan && !why.empty())
            throw SceneBackendUnavailable("Vulkan was asked for and cannot start: " + why + ".");
        selected = why.empty() ? SceneBackendKind::Vulkan : SceneBackendKind::OpenGL;
        if (!why.empty()) std::cout << "[render] auto: OpenGL, since " << why << std::endl;
        return selected;
    }

    void SelectSceneBackend(SceneBackendKind kind) noexcept
    {
        selected = kind;
        requested = kind == SceneBackendKind::Vulkan ? SceneBackendRequest::Vulkan : SceneBackendRequest::OpenGL;
        resolved = true;
    }
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
        if (SelectedSceneBackend() == SceneBackendKind::OpenGL) return OpenGL::ContextDevice();
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

    bool ScenePresentsWindow()
    {
        return SelectedSceneBackend() == SceneBackendKind::Vulkan;
    }

    std::string DescribeSceneBackend(const Swapchain* swapchain)
    {
        std::string line = "requested " + std::string(SceneBackendRequestName(requested))
            + (requestExplicit ? " (command line)" : "") + ", selected "
            + std::string(SceneBackendName(SelectedSceneBackend()));
#if defined(FRUITY_HAS_VULKAN) && !defined(__ANDROID__)
        if (selected == SceneBackendKind::Vulkan && Scene().Context)
        {
            line += ", " + Scene().Context->Describe();
            if (swapchain)
            {
                const SwapchainDesc& desc = swapchain->Desc();
                line += ", swapchain " + std::to_string(desc.width) + "x" + std::to_string(desc.height)
                    + (desc.format == TextureFormat::BGRA8Unorm ? " BGRA8" : desc.format == TextureFormat::RGBA8Unorm
                        ? " RGBA8" : " sRGB");
            }
            line += ", depth D24S8, frames in flight 2, validation "
                + std::string(Scene().Context->ValidationEnabled() ? "on" : "off");
            return line;
        }
#endif
        (void)swapchain;
        line += ", " + OpenGL::ContextDevice().AdapterDescription();
        return line;
    }

    std::unique_ptr<Swapchain> CreateSceneWindowSwapchain(
        ::MphRead::RendererPlatform::Window& window, const SwapchainDesc& desc)
    {
        if (SelectedSceneBackend() == SceneBackendKind::OpenGL)
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
        if (SelectedSceneBackend() == SceneBackendKind::Vulkan)
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
