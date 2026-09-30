#pragma once

#include "../../RendererGpuMesh.hpp"
#include "CommandList.hpp"
#include "GraphicsDevice.hpp"
#include "OpenGL/OpenGlShaderInterface.hpp"
#include "SceneShaders.hpp"
#include "Swapchain.hpp"

namespace MphRead::RendererPlatform
{
    class Window;
}

#include <memory>
#include <string_view>

// Which backend the scene renderer draws with, and the four things the Scene
// asks a backend for: its device, its programs, its meshes and its transient
// geometry. The Scene names no backend; this is the only place that does.
namespace MphRead::NativeRuntime::Rhi
{
    enum class SceneBackendKind : std::uint8_t
    {
        OpenGL,
        Vulkan
    };

    [[nodiscard]] SceneBackendKind SelectedSceneBackend() noexcept;
    void SelectSceneBackend(SceneBackendKind kind) noexcept;
    // "opengl" / "gl" / "vulkan" / "vk"; false for anything else.
    [[nodiscard]] bool ParseSceneBackend(std::string_view text, SceneBackendKind& kind) noexcept;
    [[nodiscard]] std::string_view SceneBackendName(SceneBackendKind kind) noexcept;
    // Vulkan's validation layers for the scene device, when it is created.
    void SetSceneValidation(bool enabled) noexcept;

    // The device scenes draw with. OpenGL's is the current context's; Vulkan's
    // is one headless device for the process, created on first use.
    [[nodiscard]] GraphicsDevice& SceneDevice();
    // Validation errors the Vulkan scene device has reported (0 for OpenGL).
    [[nodiscard]] unsigned SceneValidationErrors() noexcept;

    // The game window, when the scene backend presents it itself: Vulkan
    // makes the scene device on this window's surface, and the swapchain on
    // that same device. OpenGL's window is its context and needs neither.
    [[nodiscard]] bool ScenePresentsWindow() noexcept;
    [[nodiscard]] std::unique_ptr<Swapchain> CreateSceneWindowSwapchain(
        ::MphRead::RendererPlatform::Window& window, const SwapchainDesc& desc);
    // End the frame: submit, and show the scene device's window target.
    void PresentSceneWindow(Swapchain& swapchain);
    // Before the window goes: every scene is gone, and the device follows.
    void DetachSceneWindow() noexcept;

    [[nodiscard]] std::unique_ptr<SceneShaderSet> CreateSceneShaderSet(
        GraphicsDevice& device, CommandList& commands, const OpenGL::SceneShaderSources& sources);
    [[nodiscard]] std::shared_ptr<MphRead::GpuMeshResource> CreateSceneGpuMesh(
        GraphicsDevice& device, CommandList& commands, const MphRead::RendererGeometry& geometry);
    [[nodiscard]] std::shared_ptr<MphRead::TransientGeometryResource> CreateSceneTransientGeometry(
        GraphicsDevice& device, CommandList& commands);
}
