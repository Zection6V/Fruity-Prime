#pragma once

#include "../../OpenTK/GLFW.hpp"
#if !defined(__ANDROID__)
#include "../../OpenTK/GL.hpp"
#include "../../OpenTK/GlFeatures.hpp"
#endif
#if defined(__ANDROID__)
#include <EGL/egl.h>
#endif
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

#if defined(_WIN32)
#define FRUITY_GL_CALL __stdcall
#else
#define FRUITY_GL_CALL
#endif

namespace MphRead::NativeRuntime::Rhi::OpenGL
{
    // Native entry points used only by this backend, loaded with its current
    // context. No process-lifetime cache ties a recreated session to old GL.
    struct OpenGlNative final
    {
        template <class T> static T Load(const char* name)
        {
#if defined(__ANDROID__)
            return reinterpret_cast<T>(eglGetProcAddress(name));
#else
            return reinterpret_cast<T>(::OpenTK::Graphics::OpenGL::GetEntryPoint(name));
#endif
        }
        template <class T> static T Require(T entry, const char* name)
        {
            if (!entry) throw std::runtime_error(std::string("OpenGL entry point unavailable: ") + name);
            return entry;
        }

#define FRUITY_GL_ENTRY(name, result, ...) \
        using name##Type = result(FRUITY_GL_CALL*)(__VA_ARGS__); \
        name##Type name = Load<name##Type>("gl" #name)
        FRUITY_GL_ENTRY(GenSamplers, void, int, unsigned*);
        FRUITY_GL_ENTRY(DeleteSamplers, void, int, const unsigned*);
        FRUITY_GL_ENTRY(SamplerParameteri, void, unsigned, unsigned, int);
        FRUITY_GL_ENTRY(SamplerParameterf, void, unsigned, unsigned, float);
        FRUITY_GL_ENTRY(SamplerParameterfv, void, unsigned, unsigned, const float*);
        FRUITY_GL_ENTRY(BindSampler, void, unsigned, unsigned);
        FRUITY_GL_ENTRY(GenVertexArrays, void, int, unsigned*);
        FRUITY_GL_ENTRY(DeleteVertexArrays, void, int, const unsigned*);
        FRUITY_GL_ENTRY(BindVertexArray, void, unsigned);
        FRUITY_GL_ENTRY(VertexAttribIPointer, void, unsigned, int, unsigned, int, const void*);
        FRUITY_GL_ENTRY(VertexAttribPointer, void, unsigned, int, unsigned, unsigned char, int, const void*);
        FRUITY_GL_ENTRY(EnableVertexAttribArray, void, unsigned);
        FRUITY_GL_ENTRY(VertexAttribDivisor, void, unsigned, unsigned);
        FRUITY_GL_ENTRY(BufferSubData, void, unsigned, std::ptrdiff_t, std::ptrdiff_t, const void*);
        FRUITY_GL_ENTRY(MapBufferRange, void*, unsigned, std::ptrdiff_t, std::ptrdiff_t, unsigned);
        FRUITY_GL_ENTRY(MapBuffer, void*, unsigned, unsigned);
        FRUITY_GL_ENTRY(GetBufferSubData, void, unsigned, std::ptrdiff_t, std::ptrdiff_t, void*);
        FRUITY_GL_ENTRY(TexParameteri, void, unsigned, unsigned, int);
        FRUITY_GL_ENTRY(TexParameterf, void, unsigned, unsigned, float);
        FRUITY_GL_ENTRY(TexParameterfv, void, unsigned, unsigned, const float*);
        FRUITY_GL_ENTRY(UnmapBuffer, unsigned char, unsigned);
        FRUITY_GL_ENTRY(CopyBufferSubData, void, unsigned, unsigned, std::ptrdiff_t, std::ptrdiff_t, std::ptrdiff_t);
        FRUITY_GL_ENTRY(BindBufferRange, void, unsigned, unsigned, unsigned, std::ptrdiff_t, std::ptrdiff_t);
        FRUITY_GL_ENTRY(BindImageTexture, void, unsigned, unsigned, int, unsigned char, int, unsigned, unsigned);
        FRUITY_GL_ENTRY(MemoryBarrier, void, unsigned);
        FRUITY_GL_ENTRY(FenceSync, void*, unsigned, unsigned);
        FRUITY_GL_ENTRY(ClientWaitSync, unsigned, void*, unsigned, std::uint64_t);
        FRUITY_GL_ENTRY(DeleteSync, void, void*);
        FRUITY_GL_ENTRY(Flush, void);
        FRUITY_GL_ENTRY(IsBuffer, unsigned char, unsigned);
        FRUITY_GL_ENTRY(IsTexture, unsigned char, unsigned);
        FRUITY_GL_ENTRY(IsRenderbuffer, unsigned char, unsigned);
        FRUITY_GL_ENTRY(IsShader, unsigned char, unsigned);
        FRUITY_GL_ENTRY(IsProgram, unsigned char, unsigned);
        FRUITY_GL_ENTRY(IsSampler, unsigned char, unsigned);
        FRUITY_GL_ENTRY(IsFramebuffer, unsigned char, unsigned);
        FRUITY_GL_ENTRY(IsVertexArray, unsigned char, unsigned);
        FRUITY_GL_ENTRY(DrawArraysInstanced, void, unsigned, int, int, int);
        FRUITY_GL_ENTRY(DrawArraysInstancedBaseInstance, void, unsigned, int, int, int, unsigned);
        FRUITY_GL_ENTRY(DrawElementsInstanced, void, unsigned, int, unsigned, const void*, int);
        FRUITY_GL_ENTRY(DrawElementsInstancedBaseVertex, void, unsigned, int, unsigned, const void*, int, int);
        FRUITY_GL_ENTRY(DrawElementsInstancedBaseVertexBaseInstance, void, unsigned, int, unsigned, const void*, int, int, unsigned);
        FRUITY_GL_ENTRY(GetTexImage, void, unsigned, int, unsigned, unsigned, void*);
        FRUITY_GL_ENTRY(FrontFace, void, unsigned);
        FRUITY_GL_ENTRY(BlendFuncSeparate, void, unsigned, unsigned, unsigned, unsigned);
        FRUITY_GL_ENTRY(BlendEquationSeparate, void, unsigned, unsigned);
        FRUITY_GL_ENTRY(StencilFuncSeparate, void, unsigned, unsigned, int, unsigned);
        FRUITY_GL_ENTRY(StencilOpSeparate, void, unsigned, unsigned, unsigned, unsigned);
        FRUITY_GL_ENTRY(DepthRange, void, double, double);
        FRUITY_GL_ENTRY(DepthRangef, void, float, float);
        FRUITY_GL_ENTRY(GenQueries, void, int, unsigned*);
        FRUITY_GL_ENTRY(DeleteQueries, void, int, const unsigned*);
        FRUITY_GL_ENTRY(QueryCounter, void, unsigned, unsigned);
        FRUITY_GL_ENTRY(GetQueryiv, void, unsigned, unsigned, int*);
        FRUITY_GL_ENTRY(GetQueryObjectiv, void, unsigned, unsigned, int*);
        FRUITY_GL_ENTRY(GetQueryObjectui64v, void, unsigned, unsigned, std::uint64_t*);
        FRUITY_GL_ENTRY(PushDebugGroup, void, unsigned, unsigned, int, const char*);
        FRUITY_GL_ENTRY(PopDebugGroup, void);
        FRUITY_GL_ENTRY(DebugMessageInsert, void, unsigned, unsigned, unsigned, unsigned, int, const char*);
        FRUITY_GL_ENTRY(ObjectLabel, void, unsigned, unsigned, int, const char*);
#undef FRUITY_GL_ENTRY

        // What the context can do beyond an entry point resolving. The OpenGL
        // RHI is written against GL 3.3; on a context below that (macOS's legacy
        // 2.1, the only one with immediate mode there) these say which parts
        // run on an extension, which on a fallback, and which not at all.
        struct Features final
        {
            bool samplerObjects = true; // else sampler state lives on the bound texture
            bool mapBufferRange = true; // else glMapBuffer / glGetBufferSubData
            bool copyBuffer = true;     // else a read back and a re-upload
            bool uniformBuffers = true;
        } features;

        OpenGlNative()
        {
#if !defined(__ANDROID__)
            ApplyContextFeatures(::OpenTK::Graphics::OpenGL::GlFeatures::Current());
#endif
        }

    private:
#if !defined(__ANDROID__)
        template <class T> static void Prefer(T& entry, bool core, bool extension, const char* suffixed)
        {
            if (core) return;
            entry = extension && suffixed ? Load<T>(suffixed) : nullptr;
        }

        // Every entry point above is nulled unless the version or an
        // extension offers it, so the backend's existing null checks decide
        // -- macOS resolves all of them whatever the context can do.
        void ApplyContextFeatures(const ::OpenTK::Graphics::OpenGL::GlFeatures& gl)
        {
            if (!gl.Major) return; // no version string: leave the 3.3 contract as it was
            features.samplerObjects = gl.Supports(3, 3, "GL_ARB_sampler_objects");
            if (!features.samplerObjects)
            {
                GenSamplers = nullptr; DeleteSamplers = nullptr; BindSampler = nullptr; IsSampler = nullptr;
                SamplerParameteri = nullptr; SamplerParameterf = nullptr; SamplerParameterfv = nullptr;
            }
            const bool appleVao = gl.Has("GL_APPLE_vertex_array_object");
            const bool vao = gl.Supports(3, 0, "GL_ARB_vertex_array_object");
            Prefer(GenVertexArrays, vao, appleVao, "glGenVertexArraysAPPLE");
            Prefer(DeleteVertexArrays, vao, appleVao, "glDeleteVertexArraysAPPLE");
            Prefer(BindVertexArray, vao, appleVao, "glBindVertexArrayAPPLE");
            Prefer(IsVertexArray, vao, appleVao, "glIsVertexArrayAPPLE");
            Prefer(VertexAttribIPointer, gl.AtLeast(3, 0), gl.Has("GL_EXT_gpu_shader4"), "glVertexAttribIPointerEXT");
            Prefer(VertexAttribDivisor, gl.AtLeast(3, 3), gl.Has("GL_ARB_instanced_arrays"), "glVertexAttribDivisorARB");
            const bool drawInstanced = gl.Has("GL_ARB_draw_instanced");
            Prefer(DrawArraysInstanced, gl.AtLeast(3, 1), drawInstanced, "glDrawArraysInstancedARB");
            Prefer(DrawElementsInstanced, gl.AtLeast(3, 1), drawInstanced, "glDrawElementsInstancedARB");
            // ARB_draw_elements_base_vertex keeps the core name.
            Prefer(DrawElementsInstancedBaseVertex, gl.AtLeast(3, 2),
                gl.Has("GL_ARB_draw_elements_base_vertex") && DrawElementsInstanced, "glDrawElementsInstancedBaseVertex");
            const bool baseInstance = gl.Supports(4, 2, "GL_ARB_base_instance");
            Prefer(DrawArraysInstancedBaseInstance, baseInstance, false, nullptr);
            Prefer(DrawElementsInstancedBaseVertexBaseInstance, baseInstance, false, nullptr);
            features.mapBufferRange = gl.Supports(3, 0, "GL_ARB_map_buffer_range");
            Prefer(MapBufferRange, features.mapBufferRange, false, nullptr);
            features.copyBuffer = gl.Supports(3, 1, "GL_ARB_copy_buffer");
            Prefer(CopyBufferSubData, features.copyBuffer, false, nullptr);
            features.uniformBuffers = gl.Supports(3, 1, "GL_ARB_uniform_buffer_object");
            Prefer(BindBufferRange, features.uniformBuffers, false, nullptr);
            const bool images = gl.Supports(4, 2, "GL_ARB_shader_image_load_store");
            Prefer(BindImageTexture, images, false, nullptr);
            Prefer(MemoryBarrier, images, false, nullptr);
            const bool sync = gl.Supports(3, 2, "GL_ARB_sync");
            Prefer(FenceSync, sync, false, nullptr);
            Prefer(ClientWaitSync, sync, false, nullptr);
            Prefer(DeleteSync, sync, false, nullptr);
            const bool timer = gl.Supports(3, 3, "GL_ARB_timer_query");
            Prefer(QueryCounter, timer, false, nullptr);
            Prefer(GetQueryObjectui64v, timer, false, nullptr);
            const bool debug = gl.Supports(4, 3, "GL_KHR_debug");
            Prefer(PushDebugGroup, debug, false, nullptr);
            Prefer(PopDebugGroup, debug, false, nullptr);
            Prefer(DebugMessageInsert, debug, false, nullptr);
            Prefer(ObjectLabel, debug, false, nullptr);
            Prefer(DepthRangef, gl.Supports(4, 1, "GL_ARB_ES2_compatibility"), false, nullptr);
        }
#endif
    };
}
#undef FRUITY_GL_CALL
