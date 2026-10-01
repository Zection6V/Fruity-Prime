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

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
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

    // What the player (launcher.txt's renderer) or the command line (-rhi)
    // asked for. Auto takes Vulkan when this build and this machine can run
    // it -- the window and its launcher included -- and OpenGL otherwise.
    enum class SceneBackendRequest : std::uint8_t
    {
        OpenGL,
        Vulkan,
        Auto
    };

    // An explicitly requested backend that cannot start. Never answered by
    // quietly starting the other one: the message says what is missing.
    class SceneBackendUnavailable final : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    // explicitRequest: the command line's, which the preference cannot
    // replace. Takes effect before the first scene device is asked for.
    void RequestSceneBackend(SceneBackendRequest request, bool explicitRequest) noexcept;
    [[nodiscard]] SceneBackendRequest RequestedSceneBackend() noexcept;
    // The launcher will draw into the window (the shell): Vulkan then also
    // needs a Skia that can draw through it. Harness windows never say so.
    void SceneBackendNeedsWindowUi(bool value) noexcept;
    // "opengl" / "gl" / "vulkan" / "vk" / "auto"; false for anything else.
    [[nodiscard]] bool ParseSceneBackendRequest(std::string_view text, SceneBackendRequest& request) noexcept;
    [[nodiscard]] std::string_view SceneBackendRequestName(SceneBackendRequest request) noexcept;

    // The backend the request resolved to, decided once. Throws
    // SceneBackendUnavailable for an explicit Vulkan request this build or
    // this machine cannot honour.
    [[nodiscard]] SceneBackendKind SelectedSceneBackend();
    void SelectSceneBackend(SceneBackendKind kind) noexcept;
    // "opengl" / "gl" / "vulkan" / "vk"; false for anything else.
    [[nodiscard]] bool ParseSceneBackend(std::string_view text, SceneBackendKind& kind) noexcept;
    [[nodiscard]] std::string_view SceneBackendName(SceneBackendKind kind) noexcept;
    // Why Vulkan cannot be used here, or empty when it can. forWindow: the
    // presented window's launcher as well (Skia with Vulkan).
    [[nodiscard]] std::string VulkanUnavailableReason(bool forWindow);
    // What this binary carries, one "key=value" a line, for CI to assert:
    // the backends, Skia's Vulkan, the embedded SPIR-V stages and the
    // scene programs. Needs no GPU and no game files.
    [[nodiscard]] std::string SceneBackendContract();
    // One line for the startup log: requested, selected, GPU, API, driver,
    // swapchain and depth formats, frames in flight, validation.
    [[nodiscard]] std::string DescribeSceneBackend(const Swapchain* swapchain);
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
    [[nodiscard]] bool ScenePresentsWindow();
    [[nodiscard]] std::unique_ptr<Swapchain> CreateSceneWindowSwapchain(
        ::MphRead::RendererPlatform::Window& window, const SwapchainDesc& desc);
    // Android's Vulkan path: the GameView's ANativeWindow*. The first surface
    // makes the device; later ones replace only the surface, so the game and
    // every GPU resource survive a pause or a rotation. Detach releases the
    // surface (the caller has released its swapchain) and keeps the device.
    void AttachSceneSurface(void* nativeWindow);
    void DetachSceneSurface() noexcept;
    [[nodiscard]] std::unique_ptr<Swapchain> CreateSceneSurfaceSwapchain(const SwapchainDesc& desc);
    // A window that has just loaded a scene: the full-window viewport.
    // OpenGL's is context state that outlives a draw; Vulkan sets it per
    // pass, so there it is nothing.
    void ResetWindowViewport(std::int32_t width, std::int32_t height);
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
