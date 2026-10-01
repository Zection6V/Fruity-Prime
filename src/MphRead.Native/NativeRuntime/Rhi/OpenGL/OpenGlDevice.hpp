#pragma once

#include "../GraphicsDevice.hpp"

#include <cstdint>
#include <memory>
#include <string>

// The OpenGL implementation of the RHI device and command list.
//
// One device per GL context, because texture names are context state and the
// renderer's scenes share one context: the launcher's side scene, the match
// and the map thumbnails all bind each other's textures by handle. Command
// lists are per scene, because framebuffer objects are *not* shared between
// contexts and each command list keeps the framebuffers it built.
//
// Every operation issues the GL calls the renderer used to issue itself, in
// the same order, so moving a call site behind this changes no pixel.
namespace MphRead::NativeRuntime::Rhi::OpenGL
{
    // Drain retirement and context-owned fences before destroying a context.
    void FinishContextDevice();
    // The device for the current GL context, created on first use.
    [[nodiscard]] GraphicsDevice& ContextDevice();

    // Destroy the current device before its context goes away, after releasing
    // the scene and UI resources. The outgoing GL context must still be current.
    void ResetContextDevice() noexcept;
    // The current context's viewport, over the whole window.
    void ResetWindowViewport(std::int32_t width, std::int32_t height);

    // OpenGL only: a GLSL shader compiled from source the caller keeps. The
    // source is passed on by reference because the Android head recognises
    // the desktop shaders by the identity of their strings and substitutes
    // its own ES versions. Throws with the compiler's log on failure.
    [[nodiscard]] std::unique_ptr<Shader> CreateGlslShader(
        GraphicsDevice& device, ShaderStage stage, const std::string& source);
    // OpenGL only: the program linking these two, linked once per device
    // and shared by every pipeline and shader set that names the pair.
    [[nodiscard]] std::int32_t ProgramFor(GraphicsDevice& device, const Shader& vertex, const Shader& fragment);
    // OpenGL only: hand a buffer object to the current device's retirement
    // queue instead of deleting it (the mesh and transient geometry own
    // their buffers directly). Deleted at once when there is no device.
    void RetireBuffer(std::int32_t buffer) noexcept;
    [[nodiscard]] std::int32_t CreateGeometryBuffer();
}
