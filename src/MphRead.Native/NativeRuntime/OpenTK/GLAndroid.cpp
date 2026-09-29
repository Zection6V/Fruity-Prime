#include "GL.hpp"

#if defined(__ANDROID__)

#include "../../Mods/Render/GlEs.hpp"

#include <GLES3/gl3.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    namespace Compat = ::MphRead::Mods::Render;
    namespace GLApi = ::OpenTK::Graphics::OpenGL::GL;

    template <typename T>
    [[nodiscard]] constexpr std::int32_t Int(T value) noexcept
    {
        return static_cast<std::int32_t>(value);
    }

    [[nodiscard]] GLuint Name(std::int32_t value) noexcept
    {
        return static_cast<GLuint>(std::bit_cast<std::uint32_t>(value));
    }

    [[nodiscard]] std::int32_t ManagedName(GLuint value) noexcept
    {
        return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
    }

    struct AndroidGlState final
    {
        std::array<float, 4> CurrentColor{1.0F, 1.0F, 1.0F, 1.0F};
        std::array<float, 3> CurrentNormal{0.0F, 0.0F, 1.0F};
        std::array<float, 3> CurrentTexCoord{0.0F, 0.0F, 0.0F};
        bool AlphaTestEnabled = false;
        std::int32_t AlphaFunc = 0x0207;
        std::int32_t CurrentProgram = 0;
        GLuint ArrayBuffer = 0;
        GLuint ElementArrayBuffer = 0;
        bool VertexArray = false;
        bool NormalArray = false;
        bool ColorArray = false;
        bool TexCoordArray = false;
        std::unordered_map<GLuint, std::vector<std::uint32_t>> ElementData{};
    };

    AndroidGlState State{};

    void ApplyConstantAttributes()
    {
        if (!State.NormalArray)
        {
            glDisableVertexAttribArray(2);
            glVertexAttrib3f(
                2,
                State.CurrentNormal[0],
                State.CurrentNormal[1],
                State.CurrentNormal[2]);
        }
        if (!State.TexCoordArray)
        {
            glDisableVertexAttribArray(3);
            glVertexAttrib3f(
                3,
                State.CurrentTexCoord[0],
                State.CurrentTexCoord[1],
                State.CurrentTexCoord[2]);
        }

        // a_color_set is always a constant. The color data itself is either
        // vertex attribute 1 or the shader's imm_color uniform.
        glDisableVertexAttribArray(4);
        glVertexAttrib1f(4, State.ColorArray ? 1.0F : 0.0F);
    }

    void ApplyLegacyShaderState()
    {
        if (State.CurrentProgram != 0)
        {
            const GLuint program = Name(State.CurrentProgram);
            const GLint immColor = glGetUniformLocation(program, "imm_color");
            if (immColor >= 0)
            {
                glUniform4f(
                    immColor,
                    State.CurrentColor[0],
                    State.CurrentColor[1],
                    State.CurrentColor[2],
                    State.CurrentColor[3]);
            }

            const GLint alphaTest = glGetUniformLocation(program, "alpha_test");
            if (alphaTest >= 0)
            {
                std::int32_t mode = 0;
                if (State.AlphaTestEnabled)
                {
                    mode = State.AlphaFunc == 0x0202 ? 1
                        : State.AlphaFunc == 0x0201 ? 2
                        : 0;
                }
                glUniform1i(alphaTest, mode);
            }
        }
        ApplyConstantAttributes();
    }

    [[nodiscard]] std::vector<std::uint32_t> ReadIndices(
        std::int32_t count,
        const void* indices)
    {
        if (count <= 0)
        {
            return {};
        }

        const std::size_t countSize = static_cast<std::size_t>(count);
        if (State.ElementArrayBuffer == 0)
        {
            if (indices == nullptr)
            {
                throw std::runtime_error(
                    "Android GLES quad translation requires index data.");
            }
            const auto* source = static_cast<const std::uint32_t*>(indices);
            return std::vector<std::uint32_t>(source, source + countSize);
        }

        const auto found = State.ElementData.find(State.ElementArrayBuffer);
        if (found == State.ElementData.end())
        {
            throw std::runtime_error(
                "Android GLES quad translation has no CPU index-buffer shadow.");
        }

        const std::uintptr_t byteOffset =
            reinterpret_cast<std::uintptr_t>(indices);
        if ((byteOffset % sizeof(std::uint32_t)) != 0)
        {
            throw std::runtime_error(
                "Android GLES index offset is not uint32 aligned.");
        }
        const std::size_t first = byteOffset / sizeof(std::uint32_t);
        if (first > found->second.size()
            || countSize > found->second.size() - first)
        {
            throw std::out_of_range(
                "Android GLES index range exceeds the uploaded index buffer.");
        }
        return std::vector<std::uint32_t>(
            found->second.begin() + static_cast<std::ptrdiff_t>(first),
            found->second.begin()
                + static_cast<std::ptrdiff_t>(first + countSize));
    }

    [[nodiscard]] std::vector<std::uint32_t> Triangulate(
        GLApi::PrimitiveType mode,
        const std::vector<std::uint32_t>& source)
    {
        std::vector<std::uint32_t> result;
        if (mode == GLApi::PrimitiveType::Quads)
        {
            result.reserve(source.size() / 4U * 6U);
            for (std::size_t i = 0; i + 3U < source.size(); i += 4U)
            {
                result.push_back(source[i]);
                result.push_back(source[i + 1U]);
                result.push_back(source[i + 2U]);
                result.push_back(source[i]);
                result.push_back(source[i + 2U]);
                result.push_back(source[i + 3U]);
            }
            return result;
        }
        if (mode == GLApi::PrimitiveType::QuadStrip)
        {
            if (source.size() >= 4U)
            {
                result.reserve((source.size() - 2U) / 2U * 6U);
            }
            for (std::size_t i = 0; i + 3U < source.size(); i += 2U)
            {
                result.push_back(source[i]);
                result.push_back(source[i + 1U]);
                result.push_back(source[i + 3U]);
                result.push_back(source[i]);
                result.push_back(source[i + 3U]);
                result.push_back(source[i + 2U]);
            }
            return result;
        }
        return source;
    }

    void DrawConverted(
        GLApi::PrimitiveType mode,
        std::int32_t count,
        const void* indices)
    {
        const std::vector<std::uint32_t> source = ReadIndices(count, indices);
        const std::vector<std::uint32_t> triangles =
            Triangulate(mode, source);
        if (triangles.empty())
        {
            return;
        }

        GLuint temporary = 0;
        glGenBuffers(1, &temporary);
        if (temporary == 0)
        {
            throw std::runtime_error(
                "Android GLES could not allocate a translated index buffer.");
        }

        const GLuint restore = State.ElementArrayBuffer;
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, temporary);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(
                triangles.size() * sizeof(std::uint32_t)),
            triangles.data(),
            GL_STREAM_DRAW);
        glDrawElements(
            GL_TRIANGLES,
            static_cast<GLsizei>(triangles.size()),
            GL_UNSIGNED_INT,
            nullptr);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, restore);
        glDeleteBuffers(1, &temporary);
    }

    [[nodiscard]] std::string ProgramInfoLog(GLuint program)
    {
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        if (length <= 0)
        {
            return {};
        }
        std::vector<GLchar> buffer(static_cast<std::size_t>(length));
        GLsizei written = 0;
        glGetProgramInfoLog(
            program,
            length,
            &written,
            buffer.data());
        if (written <= 0)
        {
            return {};
        }
        return std::string(
            buffer.data(),
            static_cast<std::size_t>(written));
    }
}

namespace OpenTK::Graphics::OpenGL::GL
{
    void ResetAndroidState()
    {
        State = AndroidGlState{};
    }

    void ActiveTexture(TextureUnit texture)
    {
        Compat::GlEs::ActiveTexture(Int(texture));
    }

    void AlphaFunc(AlphaFunction func, float reference)
    {
        State.AlphaFunc = Int(func);
        Compat::GlEs::AlphaFunc(Int(func), reference);
    }

    void AttachShader(std::int32_t program, std::int32_t shader)
    {
        Compat::GlEs::AttachShader(program, shader);
    }

    void Begin(PrimitiveType mode)
    {
        Compat::GlEs::Begin(Int(mode));
    }

    void BindBuffer(BufferTarget target, std::int32_t buffer)
    {
        const GLuint name = Name(buffer);
        glBindBuffer(static_cast<GLenum>(Int(target)), name);
        if (target == BufferTarget::ArrayBuffer)
        {
            State.ArrayBuffer = name;
        }
        else if (target == BufferTarget::ElementArrayBuffer)
        {
            State.ElementArrayBuffer = name;
        }
    }

    void BufferData(
        BufferTarget target,
        std::size_t size,
        const void* data,
        BufferUsageHint usage)
    {
        glBufferData(
            static_cast<GLenum>(Int(target)),
            static_cast<GLsizeiptr>(size),
            data,
            static_cast<GLenum>(Int(usage)));

        if (target == BufferTarget::ElementArrayBuffer
            && State.ElementArrayBuffer != 0)
        {
            std::vector<std::uint32_t>& shadow =
                State.ElementData[State.ElementArrayBuffer];
            if (data == nullptr || size == 0)
            {
                shadow.clear();
            }
            else
            {
                const std::size_t count =
                    size / sizeof(std::uint32_t);
                const auto* values =
                    static_cast<const std::uint32_t*>(data);
                shadow.assign(values, values + count);
            }
        }
    }

    void BindFramebuffer(
        FramebufferTarget target,
        std::int32_t framebuffer)
    {
        Compat::GlEs::BindFramebuffer(Int(target), framebuffer);
    }

    void BindRenderbuffer(
        RenderbufferTarget target,
        std::int32_t renderbuffer)
    {
        Compat::GlEs::BindRenderbuffer(Int(target), renderbuffer);
    }

    void BindTexture(TextureTarget target, std::int32_t texture)
    {
        Compat::GlEs::BindTexture(Int(target), texture);
    }

    void BlendFunc(
        BlendingFactor sfactor,
        BlendingFactor dfactor)
    {
        Compat::GlEs::BlendFunc(Int(sfactor), Int(dfactor));
    }

    FramebufferErrorCode CheckFramebufferStatus(
        FramebufferTarget target)
    {
        return static_cast<FramebufferErrorCode>(
            Compat::GlEs::CheckFramebufferStatus(Int(target)));
    }

    void Clear(ClearBufferMask mask)
    {
        Compat::GlEs::Clear(Int(mask));
    }

    void ClearColor(::OpenTK::Mathematics::Vector4 color)
    {
        Compat::GlEs::ClearColor(
            std::array<float, 4>{
                color.X, color.Y, color.Z, color.W});
    }

    void ClearColor(float red, float green, float blue, float alpha)
    {
        Compat::GlEs::ClearColor(red, green, blue, alpha);
    }

    void ClearStencil(std::int32_t s)
    {
        Compat::GlEs::ClearStencil(s);
    }

    void ClientActiveTexture(TextureUnit)
    {
        // The GLES shader has one explicit texture-coordinate attribute.
    }

    void Color3(float red, float green, float blue)
    {
        State.CurrentColor = {red, green, blue, 1.0F};
        Compat::GlEs::Color3(red, green, blue);
    }

    void Color3(::OpenTK::Mathematics::Vector3 color)
    {
        Color3(color.X, color.Y, color.Z);
    }

    void Color4(float red, float green, float blue, float alpha)
    {
        State.CurrentColor = {red, green, blue, alpha};
        Compat::GlEs::Color4(red, green, blue, alpha);
    }

    void ColorMask(bool red, bool green, bool blue, bool alpha)
    {
        Compat::GlEs::ColorMask(red, green, blue, alpha);
    }

    void CompileShader(std::int32_t shader)
    {
        Compat::GlEs::CompileShader(shader);
    }

    void CopyTexSubImage2D(
        TextureTarget target,
        std::int32_t level,
        std::int32_t xoffset,
        std::int32_t yoffset,
        std::int32_t x,
        std::int32_t y,
        std::int32_t width,
        std::int32_t height)
    {
        Compat::GlEs::CopyTexSubImage2D(
            Int(target), level, xoffset, yoffset, x, y, width, height);
    }

    std::int32_t CreateProgram()
    {
        return Compat::GlEs::CreateProgram();
    }

    std::int32_t CreateShader(ShaderType type)
    {
        return Compat::GlEs::CreateShader(Int(type));
    }

    void CullFace(TriangleFace mode)
    {
        Compat::GlEs::CullFace(Int(mode));
    }

    void DeleteBuffer(std::int32_t buffer)
    {
        const GLuint name = Name(buffer);
        if (name == 0)
        {
            return;
        }
        State.ElementData.erase(name);
        if (State.ArrayBuffer == name)
        {
            State.ArrayBuffer = 0;
        }
        if (State.ElementArrayBuffer == name)
        {
            State.ElementArrayBuffer = 0;
        }
        glDeleteBuffers(1, &name);
    }

    void DeleteProgram(std::int32_t program)
    {
        if (State.CurrentProgram == program)
        {
            State.CurrentProgram = 0;
        }
        Compat::GlEs::DeleteProgram(program);
    }

    void DeleteShader(std::int32_t shader)
    {
        Compat::GlEs::DeleteShader(shader);
    }

    void DeleteTexture(std::int32_t texture)
    {
        Compat::GlEs::DeleteTexture(texture);
    }

    void DepthFunc(DepthFunction func)
    {
        Compat::GlEs::DepthFunc(Int(func));
    }

    void DepthMask(bool flag)
    {
        Compat::GlEs::DepthMask(flag);
    }

    void DetachShader(std::int32_t program, std::int32_t shader)
    {
        Compat::GlEs::DetachShader(program, shader);
    }

    void Disable(EnableCap cap)
    {
        if (cap == EnableCap::AlphaTest)
        {
            State.AlphaTestEnabled = false;
        }
        Compat::GlEs::Disable(Int(cap));
    }

    void DisableClientState(ClientState array)
    {
        switch (array)
        {
        case ClientState::VertexArray:
            State.VertexArray = false;
            glDisableVertexAttribArray(0);
            break;
        case ClientState::ColorArray:
            State.ColorArray = false;
            glDisableVertexAttribArray(1);
            glDisableVertexAttribArray(4);
            glVertexAttrib1f(4, 0.0F);
            break;
        case ClientState::NormalArray:
            State.NormalArray = false;
            glDisableVertexAttribArray(2);
            glVertexAttrib3f(
                2,
                State.CurrentNormal[0],
                State.CurrentNormal[1],
                State.CurrentNormal[2]);
            break;
        case ClientState::TextureCoordArray:
            State.TexCoordArray = false;
            glDisableVertexAttribArray(3);
            glVertexAttrib3f(
                3,
                State.CurrentTexCoord[0],
                State.CurrentTexCoord[1],
                State.CurrentTexCoord[2]);
            break;
        }
    }

    void DrawBuffer(DrawBufferMode mode)
    {
        const GLenum buffer = static_cast<GLenum>(Int(mode));
        glDrawBuffers(1, &buffer);
    }

    void DrawElements(
        PrimitiveType mode,
        std::int32_t count,
        DrawElementsType type,
        const void* indices)
    {
        ApplyLegacyShaderState();
        if (mode == PrimitiveType::Quads
            || mode == PrimitiveType::QuadStrip)
        {
            if (type != DrawElementsType::UnsignedInt)
            {
                throw std::runtime_error(
                    "Android GLES quad translation requires uint32 indices.");
            }
            DrawConverted(mode, count, indices);
            return;
        }
        glDrawElements(
            static_cast<GLenum>(Int(mode)),
            count,
            static_cast<GLenum>(Int(type)),
            indices);
    }

    void Enable(EnableCap cap)
    {
        if (cap == EnableCap::AlphaTest)
        {
            State.AlphaTestEnabled = true;
        }
        Compat::GlEs::Enable(Int(cap));
    }

    void EnableClientState(ClientState array)
    {
        switch (array)
        {
        case ClientState::VertexArray:
            State.VertexArray = true;
            glEnableVertexAttribArray(0);
            break;
        case ClientState::ColorArray:
            State.ColorArray = true;
            glEnableVertexAttribArray(1);
            glDisableVertexAttribArray(4);
            glVertexAttrib1f(4, 1.0F);
            break;
        case ClientState::NormalArray:
            State.NormalArray = true;
            glEnableVertexAttribArray(2);
            break;
        case ClientState::TextureCoordArray:
            State.TexCoordArray = true;
            glEnableVertexAttribArray(3);
            break;
        }
    }

    void End()
    {
        Compat::GlEs::End();
    }

    void FramebufferRenderbuffer(
        FramebufferTarget target,
        FramebufferAttachment attachment,
        RenderbufferTarget renderbuffertarget,
        std::int32_t renderbuffer)
    {
        Compat::GlEs::FramebufferRenderbuffer(
            Int(target),
            Int(attachment),
            Int(renderbuffertarget),
            renderbuffer);
    }

    void FramebufferTexture2D(
        FramebufferTarget target,
        FramebufferAttachment attachment,
        TextureTarget textarget,
        std::int32_t texture,
        std::int32_t level)
    {
        Compat::GlEs::FramebufferTexture2D(
            Int(target),
            Int(attachment),
            Int(textarget),
            texture,
            level);
    }

    void DeleteFramebuffer(std::int32_t framebuffer)
    {
        Compat::GlEs::DeleteFramebuffer(framebuffer);
    }

    void DeleteRenderbuffer(std::int32_t renderbuffer)
    {
        Compat::GlEs::DeleteRenderbuffer(renderbuffer);
    }

    std::int32_t GenBuffer()
    {
        GLuint buffer = 0;
        glGenBuffers(1, &buffer);
        return ManagedName(buffer);
    }

    std::int32_t GenFramebuffer()
    {
        return Compat::GlEs::GenFramebuffer();
    }

    std::int32_t GenRenderbuffer()
    {
        return Compat::GlEs::GenRenderbuffer();
    }

    std::int32_t GenTexture()
    {
        return Compat::GlEs::GenTexture();
    }

    ErrorCode GetError()
    {
        return static_cast<ErrorCode>(Compat::GlEs::GetError());
    }

    std::int32_t GetInteger(std::int32_t pname)
    {
        return Compat::GlEs::GetInteger(pname);
    }

    void GetIntegers(std::int32_t pname, std::int32_t* values)
    {
        glGetIntegerv(
            static_cast<GLenum>(pname),
            reinterpret_cast<GLint*>(values));
    }

    void GetFloat(GetPName pname, float* values)
    {
        if (values == nullptr)
        {
            return;
        }
        if (pname == GetPName::CurrentColor)
        {
            std::copy(
                State.CurrentColor.begin(),
                State.CurrentColor.end(),
                values);
            return;
        }
        std::copy(
            State.CurrentNormal.begin(),
            State.CurrentNormal.end(),
            values);
    }

    bool IsEnabled(EnableCap cap)
    {
        if (cap == EnableCap::AlphaTest)
        {
            return State.AlphaTestEnabled;
        }
        if (cap == EnableCap::Texture2D)
        {
            return false;
        }
        return glIsEnabled(
            static_cast<GLenum>(Int(cap))) == GL_TRUE;
    }

    void DebugMessageCallback(void* callback, const void* userParam)
    {
        Compat::GlEs::DebugMessageCallback(
            callback,
            const_cast<void*>(userParam));
    }

    void GetFramebufferAttachmentParameter(
        FramebufferTarget target,
        FramebufferAttachment attachment,
        FramebufferParameterName pname,
        std::int32_t& params)
    {
        Compat::GlEs::GetFramebufferAttachmentParameter(
            Int(target), Int(attachment), Int(pname), params);
    }

    void GetProgram(
        std::int32_t program,
        GetProgramParameterName pname,
        std::int32_t& params)
    {
        GLint value = 0;
        glGetProgramiv(
            Name(program),
            static_cast<GLenum>(Int(pname)),
            &value);
        params = value;
    }

    std::string GetProgramInfoLog(std::int32_t program)
    {
        return ProgramInfoLog(Name(program));
    }

    void GetShader(
        std::int32_t shader,
        ShaderParameter pname,
        std::int32_t& params)
    {
        Compat::GlEs::GetShader(shader, Int(pname), params);
    }

    std::string GetShaderInfoLog(std::int32_t shader)
    {
        return Compat::GlEs::GetShaderInfoLog(shader);
    }

    std::string GetString(StringName name)
    {
        return Compat::GlEs::GetString(Int(name));
    }

    std::int32_t GetUniformLocation(
        std::int32_t program,
        const std::string& name)
    {
        return Compat::GlEs::GetUniformLocation(program, &name);
    }

    void LinkProgram(std::int32_t program)
    {
        Compat::GlEs::LinkProgram(program);
    }

    void LoadIdentity()
    {
        // Fixed-function matrices are shader uniforms on Android.
    }

    void MatrixMode(enum MatrixMode)
    {
        // Fixed-function matrices are shader uniforms on Android.
    }

    void MultiTexCoord2(TextureUnit, float s, float t)
    {
        TexCoord3(s, t, State.CurrentTexCoord[2]);
    }

    void Normal3(float nx, float ny, float nz)
    {
        State.CurrentNormal = {nx, ny, nz};
        Compat::GlEs::Normal3(nx, ny, nz);
        if (!State.NormalArray)
        {
            glVertexAttrib3f(2, nx, ny, nz);
        }
    }

    void NormalPointer(
        PointerType type,
        std::int32_t stride,
        const void* pointer)
    {
        glVertexAttribPointer(
            2,
            3,
            static_cast<GLenum>(Int(type)),
            GL_FALSE,
            stride,
            pointer);
    }

    void ColorPointer(
        std::int32_t size,
        PointerType type,
        std::int32_t stride,
        const void* pointer)
    {
        glVertexAttribPointer(
            1,
            size,
            static_cast<GLenum>(Int(type)),
            GL_FALSE,
            stride,
            pointer);
    }

    void PixelStore(PixelStoreParameter pname, std::int32_t param)
    {
        Compat::GlEs::PixelStore(Int(pname), param);
    }

    void LineWidth(float width)
    {
        Compat::GlEs::LineWidth(width);
    }

    void PolygonMode(TriangleFace face, enum PolygonMode mode)
    {
        Compat::GlEs::PolygonMode(Int(face), Int(mode));
    }

    void PolygonOffset(float factor, float units)
    {
        Compat::GlEs::PolygonOffset(factor, units);
    }

    void ReadBuffer(ReadBufferMode src)
    {
        Compat::GlEs::ReadBuffer(Int(src));
    }

    void ReadPixels(
        std::int32_t x,
        std::int32_t y,
        std::int32_t width,
        std::int32_t height,
        PixelFormat format,
        PixelType type,
        void* pixels)
    {
        glReadPixels(
            x,
            y,
            width,
            height,
            static_cast<GLenum>(Int(format)),
            static_cast<GLenum>(Int(type)),
            pixels);
    }

    void RenderbufferStorage(
        RenderbufferTarget target,
        enum RenderbufferStorage internalformat,
        std::int32_t width,
        std::int32_t height)
    {
        Compat::GlEs::RenderbufferStorage(
            Int(target), Int(internalformat), width, height);
    }

    void ShaderSource(std::int32_t shader, const std::string& source)
    {
        Compat::GlEs::ShaderSource(shader, &source);
    }

    void StencilFunc(
        StencilFunction func,
        std::int32_t ref,
        std::int32_t mask)
    {
        Compat::GlEs::StencilFunc(Int(func), ref, mask);
    }

    void StencilMask(std::int32_t mask)
    {
        Compat::GlEs::StencilMask(mask);
    }

    void StencilOp(
        enum StencilOp sfail,
        enum StencilOp dpfail,
        enum StencilOp dppass)
    {
        Compat::GlEs::StencilOp(
            Int(sfail), Int(dpfail), Int(dppass));
    }

    void TexEnv(TextureEnvTarget, TextureEnvParameter, std::int32_t)
    {
        // The GLES shader implements texture modulation explicitly.
    }

    void TexCoordPointer(
        std::int32_t size,
        PointerType type,
        std::int32_t stride,
        const void* pointer)
    {
        glVertexAttribPointer(
            3,
            size,
            static_cast<GLenum>(Int(type)),
            GL_FALSE,
            stride,
            pointer);
    }

    void TexCoord2(float s, float t)
    {
        TexCoord3(s, t, State.CurrentTexCoord[2]);
    }

    void TexCoord3(float s, float t, float r)
    {
        State.CurrentTexCoord = {s, t, r};
        Compat::GlEs::TexCoord3(s, t, r);
        if (!State.TexCoordArray)
        {
            glVertexAttrib3f(3, s, t, r);
        }
    }

    void TexCoord3(::OpenTK::Mathematics::Vector3 coord)
    {
        TexCoord3(coord.X, coord.Y, coord.Z);
    }

    void TexSubImage2D(
        TextureTarget target,
        std::int32_t level,
        std::int32_t xoffset,
        std::int32_t yoffset,
        std::int32_t width,
        std::int32_t height,
        PixelFormat format,
        PixelType type,
        const void* pixels)
    {
        glTexSubImage2D(
            static_cast<GLenum>(Int(target)),
            level,
            xoffset,
            yoffset,
            width,
            height,
            static_cast<GLenum>(Int(format)),
            static_cast<GLenum>(Int(type)),
            pixels);
    }

    void TexImage2D(
        TextureTarget target,
        std::int32_t level,
        PixelInternalFormat internalformat,
        std::int32_t width,
        std::int32_t height,
        std::int32_t border,
        PixelFormat format,
        PixelType type,
        const void* pixels)
    {
        Compat::GlEs::TexImage2D(
            Int(target),
            level,
            Int(internalformat),
            width,
            height,
            border,
            Int(format),
            Int(type),
            pixels);
    }

    void TexParameter(
        TextureTarget target,
        TextureParameterName pname,
        std::int32_t param)
    {
        Compat::GlEs::TexParameter(Int(target), Int(pname), param);
    }

    void Uniform1(std::int32_t location, float v0)
    {
        Compat::GlEs::Uniform1(location, v0);
    }

    void Uniform1(std::int32_t location, std::int32_t v0)
    {
        Compat::GlEs::Uniform1(location, v0);
    }

    void Uniform1(
        std::int32_t location,
        std::int32_t count,
        const float* value)
    {
        Compat::GlEs::Uniform1(
            location,
            count,
            std::span<const float>(
                value,
                static_cast<std::size_t>(std::max(count, 0))));
    }

    void Uniform3(
        std::int32_t location,
        ::OpenTK::Mathematics::Vector3 data)
    {
        Compat::GlEs::Uniform3(
            location,
            std::array<float, 3>{data.X, data.Y, data.Z});
    }

    void Uniform3(
        std::int32_t location,
        std::int32_t count,
        const float* value)
    {
        Compat::GlEs::Uniform3(
            location,
            count,
            std::span<const float>(
                value,
                static_cast<std::size_t>(
                    std::max(count, 0) * 3)));
    }

    void Uniform4(
        std::int32_t location,
        ::OpenTK::Mathematics::Vector4 data)
    {
        Compat::GlEs::Uniform4(
            location,
            std::array<float, 4>{
                data.X, data.Y, data.Z, data.W});
    }

    void Uniform4(
        std::int32_t location,
        float v0,
        float v1,
        float v2,
        float v3)
    {
        Compat::GlEs::Uniform4(location, v0, v1, v2, v3);
    }

    void Uniform4(
        std::int32_t location,
        std::int32_t v0,
        std::int32_t v1,
        std::int32_t v2,
        std::int32_t v3)
    {
        Compat::GlEs::Uniform4(location, v0, v1, v2, v3);
    }

    void UniformMatrix4(
        std::int32_t location,
        bool transpose,
        const ::OpenTK::Mathematics::Matrix4& matrix)
    {
        if (!transpose)
        {
            glUniformMatrix4fv(location, 1, GL_FALSE, &matrix.M11);
            return;
        }
        const float transposed[16]{
            matrix.M11, matrix.M21, matrix.M31, matrix.M41,
            matrix.M12, matrix.M22, matrix.M32, matrix.M42,
            matrix.M13, matrix.M23, matrix.M33, matrix.M43,
            matrix.M14, matrix.M24, matrix.M34, matrix.M44
        };
        glUniformMatrix4fv(location, 1, GL_FALSE, transposed);
    }

    void UniformMatrix4(
        std::int32_t location,
        std::int32_t count,
        bool transpose,
        const float* value)
    {
        if (!transpose)
        {
            glUniformMatrix4fv(
                location, count, GL_FALSE, value);
            return;
        }

        std::vector<float> transposed(
            static_cast<std::size_t>(std::max(count, 0)) * 16U);
        for (std::int32_t i = 0; i < count; ++i)
        {
            const float* src = value + static_cast<std::size_t>(i) * 16U;
            float* dst = transposed.data() + static_cast<std::size_t>(i) * 16U;
            for (std::size_t row = 0; row < 4U; ++row)
            {
                for (std::size_t column = 0; column < 4U; ++column)
                {
                    dst[column * 4U + row] = src[row * 4U + column];
                }
            }
        }
        glUniformMatrix4fv(
            location, count, GL_FALSE, transposed.data());
    }

    void UseProgram(std::int32_t program)
    {
        State.CurrentProgram = program;
        Compat::GlEs::UseProgram(program);
    }

    void VertexPointer(
        std::int32_t size,
        PointerType type,
        std::int32_t stride,
        const void* pointer)
    {
        glVertexAttribPointer(
            0,
            size,
            static_cast<GLenum>(Int(type)),
            GL_FALSE,
            stride,
            pointer);
    }

    void Vertex2(float x, float y)
    {
        Compat::GlEs::Vertex3(x, y, 0.0F);
    }

    void Vertex3(float x, float y, float z)
    {
        Compat::GlEs::Vertex3(x, y, z);
    }

    void Vertex3(::OpenTK::Mathematics::Vector3 vector)
    {
        Compat::GlEs::Vertex3(
            std::array<float, 3>{vector.X, vector.Y, vector.Z});
    }

    void PopMatrix()
    {
        // Fixed-function matrices are shader uniforms on Android.
    }

    void PushMatrix()
    {
        // Fixed-function matrices are shader uniforms on Android.
    }

    void Scissor(
        std::int32_t x,
        std::int32_t y,
        std::int32_t width,
        std::int32_t height)
    {
        Compat::GlEs::Scissor(x, y, width, height);
    }

    void Viewport(
        std::int32_t x,
        std::int32_t y,
        std::int32_t width,
        std::int32_t height)
    {
        Compat::GlEs::Viewport(x, y, width, height);
    }
}

namespace OpenTK::Graphics::OpenGL
{
    std::string ToString(FramebufferErrorCode value)
    {
        switch (value)
        {
        case FramebufferErrorCode::FramebufferUndefined:
            return "FramebufferUndefined";
        case FramebufferErrorCode::FramebufferComplete:
            return "FramebufferComplete";
        case FramebufferErrorCode::FramebufferIncompleteAttachment:
            return "FramebufferIncompleteAttachment";
        case FramebufferErrorCode::FramebufferIncompleteMissingAttachment:
            return "FramebufferIncompleteMissingAttachment";
        case FramebufferErrorCode::FramebufferIncompleteDrawBuffer:
            return "FramebufferIncompleteDrawBuffer";
        case FramebufferErrorCode::FramebufferIncompleteReadBuffer:
            return "FramebufferIncompleteReadBuffer";
        case FramebufferErrorCode::FramebufferUnsupported:
            return "FramebufferUnsupported";
        case FramebufferErrorCode::FramebufferIncompleteMultisample:
            return "FramebufferIncompleteMultisample";
        case FramebufferErrorCode::FramebufferIncompleteLayerTargets:
            return "FramebufferIncompleteLayerTargets";
        }
        return std::to_string(static_cast<std::int32_t>(value));
    }

    std::string ToString(ErrorCode value)
    {
        switch (value)
        {
        case ErrorCode::NoError: return "NoError";
        case ErrorCode::InvalidEnum: return "InvalidEnum";
        case ErrorCode::InvalidValue: return "InvalidValue";
        case ErrorCode::InvalidOperation: return "InvalidOperation";
        case ErrorCode::StackOverflow: return "StackOverflow";
        case ErrorCode::StackUnderflow: return "StackUnderflow";
        case ErrorCode::OutOfMemory: return "OutOfMemory";
        case ErrorCode::InvalidFramebufferOperation:
            return "InvalidFramebufferOperation";
        case ErrorCode::ContextLost: return "ContextLost";
        case ErrorCode::TableTooLarge: return "TableTooLarge";
        }
        return std::to_string(static_cast<std::int32_t>(value));
    }
}

#endif
