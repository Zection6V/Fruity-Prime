#pragma once

#include "../GraphicsDevice.hpp"
#include "../VertexSemantics.hpp"
#include "OpenGlUniformState.hpp"

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
    // Sessions own devices. This lookup borrows the current context's device;
    // if needed, the scene session creates it. It never owns native objects.
    [[nodiscard]] GraphicsDevice& ContextDevice();
    [[nodiscard]] std::unique_ptr<GraphicsDevice> CreateGraphicsDevice();
    [[nodiscard]] bool HasNativeContext(const GraphicsDevice& device) noexcept;

    // Defensively release native state before its context goes away. The
    // session keeps ownership of the inert wrapper. Its context is current.
    void ReleaseContextDevice() noexcept;
    // Explicit ownership boundary around external GL consumers. Invalidates
    // remembered native bindings and program values without creating a device.
    void InvalidateContextState() noexcept;
    void BeginExternalGlInterop();
    void EndExternalGlInterop() noexcept;
    // ARRAY_BUFFER is context state, independent of the retained VAO. Wrapper
    // binds/deletions report mutations; an external ownership boundary makes
    // the next upload query once before preserving the caller's binding.
    [[nodiscard]] std::int32_t ArrayBufferBinding();
    void ArrayBufferBound(std::int32_t name) noexcept;
    void ArrayBufferDeleted(std::int32_t name) noexcept;
    // Backend shader initialization and constants share the program owner's
    // linked locations and cached values with command-list small constants.
    void UseProgram(GraphicsDevice& device, std::int32_t program);
    void SetMaterialAlpha(GraphicsDevice& device, float alpha);
    [[nodiscard]] bool UpdateShaderUniform(GraphicsDevice& device, ShaderUniformLocation uniform,
        OpenGlUniformState::Kind kind, std::span<const std::byte> bytes);
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
    // Scene shader inputs remain compatible while scene geometry uses the
    // same RHI vertex/index binding, VAO cache and DrawIndexed implementation.
    void DrawSceneGeometry(CommandList& commands,
        std::span<const VertexBufferLayoutDesc> buffers, std::span<const VertexAttributeDesc> attributes,
        PrimitiveTopology topology, std::uint32_t count, std::uint32_t first = 0);

    // Backend-owned current inputs, independent of conventional GL attribute
    // aliases and of the undefined current values after an array draw.
    void SetCurrentAttribute(VertexSemantic semantic, float x, float y, float z, float w);
    [[nodiscard]] std::array<float, 4> CurrentAttribute(VertexSemantic semantic);
    // Skia interop: borrow a native texture in this GL context for one draw.
    void BindInteropTexture(CommandList& commands, std::int32_t texture, const Sampler& sampler);
    // Optional driver budget before external GL/Skia target allocations. These
    // callers own their storage; it is not included in RHI backing estimates.
    void AdmitInteropTextureStorage(TextureFormat format, std::uint32_t width, std::uint32_t height);
    void CheckInteropStorageResult(const char* operation);
#if defined(__ANDROID__)
    // GLES still uses its emulated fixed-function draw wrapper.
    void RetireAndroidGeometryBuffer(std::int32_t buffer) noexcept;
    [[nodiscard]] std::int32_t CreateAndroidGeometryBuffer();
#endif
}
