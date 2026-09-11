#pragma once

#if defined(__ANDROID__)
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace MphRead::Mods::Render
{
    class GlEs final
    {
    public:
        GlEs() = delete;
        GlEs(const GlEs&) = delete;
        GlEs(GlEs&&) = delete;
        GlEs& operator=(const GlEs&) = delete;
        GlEs& operator=(GlEs&&) = delete;

        static void Reset();

        static void Begin(std::int32_t mode);
        static void End();
        static void Vertex3(float x, float y, float z);
        static void Vertex3(const std::array<float, 3>& vector);
        static void Color3(float r, float g, float b);
        static void Color3(const std::array<float, 3>& color);
        static void Color4(float r, float g, float b, float a);
        static void Normal3(float x, float y, float z);
        static void TexCoord3(float s, float t, float matrixId);
        static void TexCoord3(const std::array<float, 3>& texcoord);

        static std::int32_t GenLists(std::int32_t range);
        static void NewList(std::int32_t list, std::int32_t mode);
        static void EndList();
        static void CallList(std::int32_t list);
        static void DeleteLists(std::int32_t list, std::int32_t range);

        static std::int32_t GenTexture();
        static void DeleteTexture(std::int32_t name);
        static void BindTexture(std::int32_t target, std::int32_t name);

        static std::int32_t CreateShader(std::int32_t type);
        static void ShaderSource(std::int32_t shader, const std::string* source);
        static void CompileShader(std::int32_t shader);
        static void GetShader(std::int32_t shader, std::int32_t pname, std::int32_t& value);
        static std::string GetShaderInfoLog(std::int32_t shader);
        static void DeleteShader(std::int32_t shader);
        static std::int32_t CreateProgram();
        static void AttachShader(std::int32_t program, std::int32_t shader);
        static void DetachShader(std::int32_t program, std::int32_t shader);
        static void LinkProgram(std::int32_t program);
        static void UseProgram(std::int32_t program);
        static std::int32_t GetUniformLocation(std::int32_t program, const std::string* name);

        static void Enable(std::int32_t cap);
        static void Disable(std::int32_t cap);
        static void AlphaFunc(std::int32_t func, float reference);
        static void PolygonMode(std::int32_t face, std::int32_t mode);
        static void DebugMessageCallback(void* callback, void* userParam);
        static void Clear(std::int32_t mask);
        static void ClearColor(const std::array<float, 4>& color);
        static void ClearStencil(std::int32_t value);
        static void ColorMask(bool red, bool green, bool blue, bool alpha);
        static void DepthMask(bool flag);
        static void DepthFunc(std::int32_t func);
        static void CullFace(std::int32_t mode);
        static void BlendFunc(std::int32_t src, std::int32_t dst);
        static void StencilFunc(std::int32_t func, std::int32_t reference, std::int32_t mask);
        static void StencilOp(std::int32_t fail, std::int32_t zfail, std::int32_t zpass);
        static void StencilMask(std::int32_t mask);
        static void PolygonOffset(float factor, float units);
        static void Viewport(std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height);
        static void Scissor(std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height);
        static void ClearColor(float red, float green, float blue, float alpha);
        static void PixelStore(std::int32_t pname, std::int32_t param);
        static void ReadBuffer(std::int32_t mode);
        static void ActiveTexture(std::int32_t texture);
        static std::int32_t GetError();
        static std::string GetString(std::int32_t name);
        static std::int32_t GetInteger(std::int32_t pname);

        static void TexParameter(std::int32_t target, std::int32_t pname, std::int32_t param);
        static void TexImage2D(std::int32_t target, std::int32_t level, std::int32_t internalFormat,
            std::int32_t width, std::int32_t height, std::int32_t border, std::int32_t format,
            std::int32_t type, const void* pixels);

        template <typename T>
        static void TexImage2D(std::int32_t target, std::int32_t level, std::int32_t internalFormat,
            std::int32_t width, std::int32_t height, std::int32_t border, std::int32_t format,
            std::int32_t type, std::span<const T> pixels)
        {
            TexImage2D(target, level, internalFormat, width, height, border, format, type,
                pixels.empty() ? nullptr : static_cast<const void*>(pixels.data()));
        }

        template <typename T>
        static void TexSubImage2D(std::int32_t target, std::int32_t level,
            std::int32_t xoffset, std::int32_t yoffset, std::int32_t width, std::int32_t height,
            std::int32_t format, std::int32_t type, std::span<const T> pixels)
        {
            TexSubImage2DRaw(target, level, xoffset, yoffset, width, height, format, type,
                pixels.empty() ? nullptr : static_cast<const void*>(pixels.data()));
        }

        static void CopyTexSubImage2D(std::int32_t target, std::int32_t level,
            std::int32_t xoffset, std::int32_t yoffset, std::int32_t x, std::int32_t y,
            std::int32_t width, std::int32_t height);

        template <typename T>
        static void ReadPixels(std::int32_t x, std::int32_t y, std::int32_t width,
            std::int32_t height, std::int32_t format, std::int32_t type, std::span<T> pixels)
        {
            ReadPixelsRaw(x, y, width, height, format, type,
                pixels.empty() ? nullptr : static_cast<void*>(pixels.data()));
        }

        static std::int32_t GenFramebuffer();
        static void BindFramebuffer(std::int32_t target, std::int32_t framebuffer);
        static void FramebufferTexture2D(std::int32_t target, std::int32_t attachment,
            std::int32_t textarget, std::int32_t texture, std::int32_t level);
        static std::int32_t GenRenderbuffer();
        static void BindRenderbuffer(std::int32_t target, std::int32_t renderbuffer);
        static void RenderbufferStorage(std::int32_t target, std::int32_t internalFormat,
            std::int32_t width, std::int32_t height);
        static void FramebufferRenderbuffer(std::int32_t target, std::int32_t attachment,
            std::int32_t renderbufferTarget, std::int32_t renderbuffer);
        static void GetFramebufferAttachmentParameter(std::int32_t target,
            std::int32_t attachment, std::int32_t pname, std::int32_t& result);
        static std::int32_t CheckFramebufferStatus(std::int32_t target);

        static void Uniform1(std::int32_t location, std::int32_t value);
        static void Uniform1(std::int32_t location, float value);
        static void Uniform1(std::int32_t location, std::int32_t count, std::span<const float> value);
        static void Uniform3(std::int32_t location, const std::array<float, 3>& vector);
        static void Uniform3(std::int32_t location, std::int32_t count, std::span<const float> value);
        static void Uniform4(std::int32_t location, std::array<float, 4> vector);
        static void Uniform4(std::int32_t location, std::array<float, 4>* vector);
        static void Uniform4(std::int32_t location, float v0, float v1, float v2, float v3);
        static void Uniform4(std::int32_t location, std::int32_t v0, std::int32_t v1,
            std::int32_t v2, std::int32_t v3);
        static void UniformMatrix4(std::int32_t location, bool transpose,
            std::array<float, 16>* matrix);
        static void UniformMatrix4(std::int32_t location, std::int32_t count, bool transpose,
            std::span<const float> value);

    private:
        static constexpr std::int32_t FloatsPerVertex = 14;
        static constexpr std::int32_t Stride = FloatsPerVertex * static_cast<std::int32_t>(sizeof(float));

        struct Batch final
        {
            std::vector<float> Vertices;
            std::vector<std::int32_t> TriIndices;
            std::vector<std::int32_t> LineIndices;
            std::int32_t VertexCount = 0;

            Batch();
            void Clear();
        };

        struct CompiledList final
        {
            std::int32_t Vao = 0;
            std::int32_t Vbo = 0;
            std::int32_t Ibo = 0;
            std::int32_t TriCount = 0;
            std::int32_t LineCount = 0;
        };

        struct ProgramLocations final
        {
            std::int32_t ImmColor;
            std::int32_t AlphaTest;
        };

        static std::array<float, 4> _curColor;
        static std::array<float, 3> _curNormal;
        static std::array<float, 3> _curTexCoord;
        static bool _colorSet;

        static std::int32_t _primMode;
        static std::int32_t _primStart;

        static Batch _batch;
        static bool _recording;
        static std::int32_t _recordListId;

        static std::unordered_map<std::int32_t, CompiledList> _lists;
        static std::int32_t _nextListId;

        static std::int32_t _dynVao;
        static std::int32_t _dynVbo;
        static std::int32_t _dynIbo;
        static std::int32_t _dynVboSize;
        static std::int32_t _dynIboSize;

        static bool _alphaTestEnabled;
        static std::int32_t _alphaFunc;
        static std::int32_t _program;
        static std::int32_t _immColorLoc;
        static std::int32_t _alphaTestLoc;
        static std::unordered_map<std::int32_t, ProgramLocations> _programLocs;

        static std::unordered_map<std::int32_t, std::int32_t> _textures;
        static std::int32_t _textureHighWater;

        static void EmitIndices(std::int32_t mode, std::int32_t base, std::int32_t count);
        static std::vector<std::int32_t> BuildIndexArray();
        static void FlushDynamic();
        static void SetupAttributes();
        static void ApplyDrawState();
        static std::int32_t RealTexture(std::int32_t name);
        static bool IgnoredCap(std::int32_t cap);
        static void TexSubImage2DRaw(std::int32_t target, std::int32_t level,
            std::int32_t xoffset, std::int32_t yoffset, std::int32_t width, std::int32_t height,
            std::int32_t format, std::int32_t type, const void* pixels);
        static void ReadPixelsRaw(std::int32_t x, std::int32_t y, std::int32_t width,
            std::int32_t height, std::int32_t format, std::int32_t type, void* pixels);
    };
}
#endif
