#include "GlEs.hpp"

#if defined(__ANDROID__)
#include "../../Program.hpp"
#include "EsShaders.hpp"

#include <GLES3/gl3.h>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    constexpr std::int32_t GlPoints = 0x0000;
    constexpr std::int32_t GlLines = 0x0001;
    constexpr std::int32_t GlLineLoop = 0x0002;
    constexpr std::int32_t GlLineStrip = 0x0003;
    constexpr std::int32_t GlTriangles = 0x0004;
    constexpr std::int32_t GlTriangleStrip = 0x0005;
    constexpr std::int32_t GlTriangleFan = 0x0006;
    constexpr std::int32_t GlQuads = 0x0007;
    constexpr std::int32_t GlQuadStrip = 0x0008;
    constexpr std::int32_t GlPolygon = 0x0009;
    constexpr std::int32_t GlLinesAdjacency = 0x000A;
    constexpr std::int32_t GlLineStripAdjacency = 0x000B;
    constexpr std::int32_t GlTrianglesAdjacency = 0x000C;
    constexpr std::int32_t GlTriangleStripAdjacency = 0x000D;
    constexpr std::int32_t GlPatches = 0x000E;

    constexpr std::int32_t GlAlphaTest = 0x0BC0;
    constexpr std::int32_t GlTexture2D = 0x0DE1;
    constexpr std::int32_t GlDebugOutputSynchronous = 0x8242;
    constexpr std::int32_t GlDebugOutput = 0x92E0;

    constexpr std::int32_t GlLess = 0x0201;
    constexpr std::int32_t GlEqual = 0x0202;
    constexpr std::int32_t GlAlways = 0x0207;

    constexpr std::int32_t GlContextFlags = 0x821E;
    constexpr std::int32_t GlContextProfileMask = 0x9126;

    static_assert(sizeof(GLuint) == sizeof(std::uint32_t));
    static_assert(sizeof(GLint) == sizeof(std::int32_t));

    std::uint32_t Int32Bits(std::int32_t value)
    {
        return std::bit_cast<std::uint32_t>(value);
    }

    std::int32_t UInt32Bits(std::uint32_t value)
    {
        return std::bit_cast<std::int32_t>(value);
    }

    std::int32_t WrapAdd(std::int32_t left, std::int32_t right)
    {
        return UInt32Bits(Int32Bits(left) + Int32Bits(right));
    }

    std::int32_t WrapSubtract(std::int32_t left, std::int32_t right)
    {
        return UInt32Bits(Int32Bits(left) - Int32Bits(right));
    }

    std::int32_t WrapMultiply(std::int32_t left, std::int32_t right)
    {
        return UInt32Bits(Int32Bits(left) * Int32Bits(right));
    }

    GLuint GlName(std::int32_t value)
    {
        return static_cast<GLuint>(Int32Bits(value));
    }

    std::int32_t ManagedName(GLuint value)
    {
        return UInt32Bits(static_cast<std::uint32_t>(value));
    }

    GLenum GlEnum(std::int32_t value)
    {
        return static_cast<GLenum>(Int32Bits(value));
    }

    GLbitfield GlBits(std::int32_t value)
    {
        return static_cast<GLbitfield>(Int32Bits(value));
    }

    std::string PrimitiveTypeText(std::int32_t mode)
    {
        switch (mode)
        {
        case GlPoints: return "Points";
        case GlLines: return "Lines";
        case GlLineLoop: return "LineLoop";
        case GlLineStrip: return "LineStrip";
        case GlTriangles: return "Triangles";
        case GlTriangleStrip: return "TriangleStrip";
        case GlTriangleFan: return "TriangleFan";
        case GlQuads: return "Quads";
        case GlQuadStrip: return "QuadStrip";
        case GlPolygon: return "Polygon";
        case GlLinesAdjacency: return "LinesAdjacency";
        case GlLineStripAdjacency: return "LineStripAdjacency";
        case GlTrianglesAdjacency: return "TrianglesAdjacency";
        case GlTriangleStripAdjacency: return "TriangleStripAdjacency";
        case GlPatches: return "Patches";
        default: return std::to_string(mode);
        }
    }

    std::int32_t DotNetStringLength(std::string_view value)
    {
        std::uint32_t count = 0;
        for (std::size_t i = 0; i < value.size();)
        {
            const unsigned char first = static_cast<unsigned char>(value[i]);
            std::size_t length = 1;
            std::uint32_t codePoint = first;
            if ((first & 0xE0U) == 0xC0U && i + 1 < value.size())
            {
                length = 2;
                codePoint = first & 0x1FU;
            }
            else if ((first & 0xF0U) == 0xE0U && i + 2 < value.size())
            {
                length = 3;
                codePoint = first & 0x0FU;
            }
            else if ((first & 0xF8U) == 0xF0U && i + 3 < value.size())
            {
                length = 4;
                codePoint = first & 0x07U;
            }
            if (length > 1)
            {
                bool valid = true;
                for (std::size_t j = 1; j < length; ++j)
                {
                    const unsigned char next = static_cast<unsigned char>(value[i + j]);
                    if ((next & 0xC0U) != 0x80U)
                    {
                        valid = false;
                        break;
                    }
                    codePoint = (codePoint << 6U) | (next & 0x3FU);
                }
                if (!valid)
                {
                    length = 1;
                    codePoint = first;
                }
            }
            count += codePoint > 0xFFFFU ? 2U : 1U;
            i += length;
        }
        return UInt32Bits(count);
    }

    std::string ShaderInfoLog(GLuint shader)
    {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        if (length <= 0)
        {
            return {};
        }
        std::vector<GLchar> buffer(static_cast<std::size_t>(length));
        GLsizei written = 0;
        glGetShaderInfoLog(shader, length, &written, buffer.data());
        if (written <= 0)
        {
            return {};
        }
        return std::string(buffer.data(), static_cast<std::size_t>(written));
    }

    std::string ProgramInfoLog(GLuint program)
    {
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        if (length <= 0)
        {
            return {};
        }
        std::vector<GLchar> buffer(static_cast<std::size_t>(length));
        GLsizei written = 0;
        glGetProgramInfoLog(program, length, &written, buffer.data());
        if (written <= 0)
        {
            return {};
        }
        return std::string(buffer.data(), static_cast<std::size_t>(written));
    }
}

namespace MphRead::Mods::Render
{
    std::array<float, 4> GlEs::_curColor = {1.0F, 1.0F, 1.0F, 1.0F};
    std::array<float, 3> GlEs::_curNormal = {0.0F, 0.0F, 1.0F};
    std::array<float, 3> GlEs::_curTexCoord = {0.0F, 0.0F, 0.0F};
    bool GlEs::_colorSet = false;

    std::int32_t GlEs::_primMode = 0;
    std::int32_t GlEs::_primStart = 0;

    GlEs::Batch GlEs::_batch;
    bool GlEs::_recording = false;
    std::int32_t GlEs::_recordListId = 0;

    std::unordered_map<std::int32_t, GlEs::CompiledList> GlEs::_lists;
    std::int32_t GlEs::_nextListId = 1;

    std::int32_t GlEs::_dynVao = 0;
    std::int32_t GlEs::_dynVbo = 0;
    std::int32_t GlEs::_dynIbo = 0;
    std::int32_t GlEs::_dynVboSize = 0;
    std::int32_t GlEs::_dynIboSize = 0;

    bool GlEs::_alphaTestEnabled = false;
    std::int32_t GlEs::_alphaFunc = GlAlways;
    std::int32_t GlEs::_program = 0;
    std::int32_t GlEs::_immColorLoc = -1;
    std::int32_t GlEs::_alphaTestLoc = -1;
    std::unordered_map<std::int32_t, GlEs::ProgramLocations> GlEs::_programLocs;

    std::unordered_map<std::int32_t, std::int32_t> GlEs::_textures;
    std::int32_t GlEs::_textureHighWater = 0;

    GlEs::Batch::Batch()
    {
        Vertices.reserve(4096);
        TriIndices.reserve(4096);
    }

    void GlEs::Batch::Clear()
    {
        Vertices.clear();
        TriIndices.clear();
        LineIndices.clear();
        VertexCount = 0;
    }

    void GlEs::Reset()
    {
        _lists.clear();
        _textures.clear();
        _programLocs.clear();
        _nextListId = 1;
        _textureHighWater = 0;
        _dynVao = _dynVbo = _dynIbo = 0;
        _dynVboSize = _dynIboSize = 0;
        _program = 0;
        _immColorLoc = -1;
        _alphaTestLoc = -1;
        _batch.Clear();
        _recording = false;
    }

    void GlEs::Begin(std::int32_t mode)
    {
        if (!_recording)
        {
            _batch.Clear();
            _colorSet = false;
        }
        _primMode = mode;
        _primStart = _batch.VertexCount;
    }

    void GlEs::End()
    {
        const std::int32_t count = WrapSubtract(_batch.VertexCount, _primStart);
        EmitIndices(_primMode, _primStart, count);
        if (!_recording)
        {
            FlushDynamic();
        }
    }

    void GlEs::Vertex3(float x, float y, float z)
    {
        std::vector<float>& v = _batch.Vertices;
        v.push_back(x);
        v.push_back(y);
        v.push_back(z);
        v.push_back(_curColor[0]);
        v.push_back(_curColor[1]);
        v.push_back(_curColor[2]);
        v.push_back(_curColor[3]);
        v.push_back(_curNormal[0]);
        v.push_back(_curNormal[1]);
        v.push_back(_curNormal[2]);
        v.push_back(_curTexCoord[0]);
        v.push_back(_curTexCoord[1]);
        v.push_back(_curTexCoord[2]);
        v.push_back(_colorSet ? 1.0F : 0.0F);
        _batch.VertexCount = WrapAdd(_batch.VertexCount, 1);
    }

    void GlEs::Vertex3(const std::array<float, 3>& vector)
    {
        Vertex3(vector[0], vector[1], vector[2]);
    }

    void GlEs::Color3(float r, float g, float b)
    {
        _curColor = {r, g, b, 1.0F};
        _colorSet = true;
    }

    void GlEs::Color3(const std::array<float, 3>& color)
    {
        Color3(color[0], color[1], color[2]);
    }

    void GlEs::Color4(float r, float g, float b, float a)
    {
        _curColor = {r, g, b, a};
        _colorSet = true;
    }

    void GlEs::Normal3(float x, float y, float z)
    {
        _curNormal = {x, y, z};
    }

    void GlEs::TexCoord3(float s, float t, float matrixId)
    {
        _curTexCoord = {s, t, matrixId};
    }

    void GlEs::TexCoord3(const std::array<float, 3>& texcoord)
    {
        _curTexCoord = texcoord;
    }

    void GlEs::EmitIndices(std::int32_t mode, std::int32_t base, std::int32_t count)
    {
        std::vector<std::int32_t>& tris = _batch.TriIndices;
        switch (mode)
        {
        case GlTriangles:
            for (std::int32_t i = 0; i + 2 < count; i += 3)
            {
                tris.push_back(WrapAdd(base, i));
                tris.push_back(WrapAdd(base, i + 1));
                tris.push_back(WrapAdd(base, i + 2));
            }
            break;
        case GlQuads:
            for (std::int32_t i = 0; i + 3 < count; i += 4)
            {
                tris.push_back(WrapAdd(base, i));
                tris.push_back(WrapAdd(base, i + 1));
                tris.push_back(WrapAdd(base, i + 2));
                tris.push_back(WrapAdd(base, i));
                tris.push_back(WrapAdd(base, i + 2));
                tris.push_back(WrapAdd(base, i + 3));
            }
            break;
        case GlTriangleStrip:
            for (std::int32_t i = 0; i + 2 < count; ++i)
            {
                if ((i & 1) == 0)
                {
                    tris.push_back(WrapAdd(base, i));
                    tris.push_back(WrapAdd(base, i + 1));
                    tris.push_back(WrapAdd(base, i + 2));
                }
                else
                {
                    tris.push_back(WrapAdd(base, i + 1));
                    tris.push_back(WrapAdd(base, i));
                    tris.push_back(WrapAdd(base, i + 2));
                }
            }
            break;
        case GlQuadStrip:
            for (std::int32_t i = 0; i + 3 < count; i += 2)
            {
                tris.push_back(WrapAdd(base, i));
                tris.push_back(WrapAdd(base, i + 1));
                tris.push_back(WrapAdd(base, i + 3));
                tris.push_back(WrapAdd(base, i));
                tris.push_back(WrapAdd(base, i + 3));
                tris.push_back(WrapAdd(base, i + 2));
            }
            break;
        case GlTriangleFan:
            for (std::int32_t i = 1; i + 1 < count; ++i)
            {
                tris.push_back(base);
                tris.push_back(WrapAdd(base, i));
                tris.push_back(WrapAdd(base, i + 1));
            }
            break;
        case GlLineLoop:
            {
                std::vector<std::int32_t>& lines = _batch.LineIndices;
                for (std::int32_t i = 0; i < count; ++i)
                {
                    lines.push_back(WrapAdd(base, i));
                    lines.push_back(WrapAdd(base, (i + 1) % count));
                }
            }
            break;
        default:
            throw MphRead::ProgramException(
                "No ES translation for primitive type " + PrimitiveTypeText(mode) + ".");
        }
    }

    std::int32_t GlEs::GenLists(std::int32_t range)
    {
        const std::int32_t id = _nextListId;
        _nextListId = WrapAdd(_nextListId, range);
        return id;
    }

    void GlEs::NewList(std::int32_t list, std::int32_t mode)
    {
        (void)mode;
        _batch.Clear();
        _recording = true;
        _recordListId = list;
        _colorSet = false;
    }

    void GlEs::EndList()
    {
        _recording = false;
        CompiledList compiled{};
        compiled.TriCount = static_cast<std::int32_t>(_batch.TriIndices.size());
        compiled.LineCount = static_cast<std::int32_t>(_batch.LineIndices.size());
        if (compiled.TriCount == 0 && compiled.LineCount == 0)
        {
            _lists[_recordListId] = compiled;
            _batch.Clear();
            return;
        }

        GLuint vao = 0;
        GLuint vbo = 0;
        GLuint ibo = 0;
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ibo);
        compiled.Vao = ManagedName(vao);
        compiled.Vbo = ManagedName(vbo);
        compiled.Ibo = ManagedName(ibo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        const std::int32_t vertexBytes = WrapMultiply(
            static_cast<std::int32_t>(_batch.Vertices.size()), static_cast<std::int32_t>(sizeof(float)));
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertexBytes), _batch.Vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        const std::vector<std::int32_t> indices = BuildIndexArray();
        const std::int32_t indexBytes = WrapMultiply(
            static_cast<std::int32_t>(indices.size()), static_cast<std::int32_t>(sizeof(std::int32_t)));
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indexBytes), indices.data(), GL_STATIC_DRAW);
        SetupAttributes();
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        _lists[_recordListId] = compiled;
        _batch.Clear();
    }

    void GlEs::CallList(std::int32_t list)
    {
        const auto found = _lists.find(list);
        if (found == _lists.end() || found->second.Vao == 0)
        {
            return;
        }
        const CompiledList& compiled = found->second;
        ApplyDrawState();
        glBindVertexArray(GlName(compiled.Vao));
        if (compiled.TriCount > 0)
        {
            glDrawElements(GL_TRIANGLES, compiled.TriCount, GL_UNSIGNED_INT, nullptr);
        }
        if (compiled.LineCount > 0)
        {
            const std::int32_t offset = WrapMultiply(compiled.TriCount,
                static_cast<std::int32_t>(sizeof(std::int32_t)));
            glDrawElements(GL_LINES, compiled.LineCount, GL_UNSIGNED_INT,
                reinterpret_cast<const void*>(static_cast<std::intptr_t>(offset)));
        }
        glBindVertexArray(0);
    }

    void GlEs::DeleteLists(std::int32_t list, std::int32_t range)
    {
        for (std::int32_t i = 0; i < range; ++i)
        {
            const std::int32_t id = WrapAdd(list, i);
            const auto found = _lists.find(id);
            if (found != _lists.end())
            {
                const CompiledList compiled = found->second;
                _lists.erase(found);
                if (compiled.Vao != 0)
                {
                    const GLuint vao = GlName(compiled.Vao);
                    const GLuint vbo = GlName(compiled.Vbo);
                    const GLuint ibo = GlName(compiled.Ibo);
                    glDeleteVertexArrays(1, &vao);
                    glDeleteBuffers(1, &vbo);
                    glDeleteBuffers(1, &ibo);
                }
            }
        }
    }

    std::vector<std::int32_t> GlEs::BuildIndexArray()
    {
        std::vector<std::int32_t> indices;
        indices.resize(_batch.TriIndices.size() + _batch.LineIndices.size());
        std::copy(_batch.TriIndices.begin(), _batch.TriIndices.end(), indices.begin());
        std::copy(_batch.LineIndices.begin(), _batch.LineIndices.end(),
            indices.begin() + static_cast<std::ptrdiff_t>(_batch.TriIndices.size()));
        return indices;
    }

    void GlEs::FlushDynamic()
    {
        const std::int32_t triCount = static_cast<std::int32_t>(_batch.TriIndices.size());
        const std::int32_t lineCount = static_cast<std::int32_t>(_batch.LineIndices.size());
        if (triCount == 0 && lineCount == 0)
        {
            _batch.Clear();
            return;
        }
        if (_dynVao == 0)
        {
            GLuint vao = 0;
            GLuint vbo = 0;
            GLuint ibo = 0;
            glGenVertexArrays(1, &vao);
            glGenBuffers(1, &vbo);
            glGenBuffers(1, &ibo);
            _dynVao = ManagedName(vao);
            _dynVbo = ManagedName(vbo);
            _dynIbo = ManagedName(ibo);
            glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
            SetupAttributes();
        }
        else
        {
            glBindVertexArray(GlName(_dynVao));
            glBindBuffer(GL_ARRAY_BUFFER, GlName(_dynVbo));
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GlName(_dynIbo));
        }

        const std::int32_t vertexBytes = WrapMultiply(
            static_cast<std::int32_t>(_batch.Vertices.size()), static_cast<std::int32_t>(sizeof(float)));
        if (vertexBytes > _dynVboSize)
        {
            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertexBytes),
                _batch.Vertices.data(), GL_STREAM_DRAW);
            _dynVboSize = vertexBytes;
        }
        else
        {
            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(_dynVboSize), nullptr, GL_STREAM_DRAW);
            glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(vertexBytes), _batch.Vertices.data());
        }

        const std::vector<std::int32_t> indices = BuildIndexArray();
        const std::int32_t indexBytes = WrapMultiply(
            static_cast<std::int32_t>(indices.size()), static_cast<std::int32_t>(sizeof(std::int32_t)));
        if (indexBytes > _dynIboSize)
        {
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indexBytes),
                indices.data(), GL_STREAM_DRAW);
            _dynIboSize = indexBytes;
        }
        else
        {
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(_dynIboSize), nullptr, GL_STREAM_DRAW);
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(indexBytes), indices.data());
        }

        ApplyDrawState();
        if (triCount > 0)
        {
            glDrawElements(GL_TRIANGLES, triCount, GL_UNSIGNED_INT, nullptr);
        }
        if (lineCount > 0)
        {
            const std::int32_t offset = WrapMultiply(triCount, static_cast<std::int32_t>(sizeof(std::int32_t)));
            glDrawElements(GL_LINES, lineCount, GL_UNSIGNED_INT,
                reinterpret_cast<const void*>(static_cast<std::intptr_t>(offset)));
        }
        glBindVertexArray(0);
        _batch.Clear();
    }

    void GlEs::SetupAttributes()
    {
        for (GLuint i = 0; i <= 4; ++i)
        {
            glEnableVertexAttribArray(i);
        }
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, Stride, nullptr);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, Stride,
            reinterpret_cast<const void*>(3 * sizeof(float)));
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, Stride,
            reinterpret_cast<const void*>(7 * sizeof(float)));
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, Stride,
            reinterpret_cast<const void*>(10 * sizeof(float)));
        glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, Stride,
            reinterpret_cast<const void*>(13 * sizeof(float)));
    }

    void GlEs::ApplyDrawState()
    {
        if (_immColorLoc >= 0)
        {
            glUniform4f(_immColorLoc, _curColor[0], _curColor[1], _curColor[2], _curColor[3]);
        }
        if (_alphaTestLoc >= 0)
        {
            std::int32_t mode = 0;
            if (_alphaTestEnabled)
            {
                mode = _alphaFunc == GlEqual ? 1 : _alphaFunc == GlLess ? 2 : 0;
            }
            glUniform1i(_alphaTestLoc, mode);
        }
    }

    std::int32_t GlEs::RealTexture(std::int32_t name)
    {
        if (name == 0)
        {
            return 0;
        }
        if (name > _textureHighWater)
        {
            _textureHighWater = name;
        }
        const auto found = _textures.find(name);
        if (found != _textures.end())
        {
            return found->second;
        }
        GLuint texture = 0;
        glGenTextures(1, &texture);
        const std::int32_t real = ManagedName(texture);
        _textures[name] = real;
        return real;
    }

    std::int32_t GlEs::GenTexture()
    {
        const std::int32_t name = _textureHighWater = WrapAdd(_textureHighWater, 1);
        RealTexture(name);
        return name;
    }

    void GlEs::DeleteTexture(std::int32_t name)
    {
        const auto found = _textures.find(name);
        if (found != _textures.end())
        {
            const GLuint real = GlName(found->second);
            _textures.erase(found);
            glDeleteTextures(1, &real);
        }
    }

    void GlEs::BindTexture(std::int32_t target, std::int32_t name)
    {
        glBindTexture(GlEnum(target), GlName(RealTexture(name)));
    }

    std::int32_t GlEs::CreateShader(std::int32_t type)
    {
        return ManagedName(glCreateShader(GlEnum(type)));
    }

    void GlEs::ShaderSource(std::int32_t shader, const std::string* source)
    {
        EsShaders::CheckInSync();
        const std::string* translated = EsShaders::Translate(source);
        if (translated == nullptr)
        {
            translated = source;
        }
        if (translated == nullptr)
        {
            throw std::runtime_error("Object reference not set to an instance of an object.");
        }
        const GLchar* string = translated->empty() ? nullptr : translated->c_str();
        const GLint length = DotNetStringLength(*translated);
        glShaderSource(GlName(shader), 1, &string, &length);
    }

    void GlEs::CompileShader(std::int32_t shader)
    {
        const GLuint glShader = GlName(shader);
        glCompileShader(glShader);
        GLint status = 0;
        glGetShaderiv(glShader, GL_COMPILE_STATUS, &status);
        if (status == 0)
        {
            std::cout << "[gles] shader " << shader << " failed to compile: "
                << ShaderInfoLog(glShader) << std::endl;
        }
    }

    void GlEs::GetShader(std::int32_t shader, std::int32_t pname, std::int32_t& value)
    {
        GLint result = 0;
        glGetShaderiv(GlName(shader), GlEnum(pname), &result);
        value = result;
    }

    std::string GlEs::GetShaderInfoLog(std::int32_t shader)
    {
        return ShaderInfoLog(GlName(shader));
    }

    void GlEs::DeleteShader(std::int32_t shader)
    {
        glDeleteShader(GlName(shader));
    }

    std::int32_t GlEs::CreateProgram()
    {
        return ManagedName(glCreateProgram());
    }

    void GlEs::AttachShader(std::int32_t program, std::int32_t shader)
    {
        glAttachShader(GlName(program), GlName(shader));
    }

    void GlEs::DetachShader(std::int32_t program, std::int32_t shader)
    {
        glDetachShader(GlName(program), GlName(shader));
    }

    void GlEs::LinkProgram(std::int32_t program)
    {
        const GLuint glProgram = GlName(program);
        glLinkProgram(glProgram);
        GLint status = 0;
        glGetProgramiv(glProgram, GL_LINK_STATUS, &status);
        if (status == 0)
        {
            throw MphRead::ProgramException("Failed to link program " + std::to_string(program)
                + ": " + ProgramInfoLog(glProgram));
        }
    }

    void GlEs::UseProgram(std::int32_t program)
    {
        glUseProgram(GlName(program));
        _program = program;
        const auto found = _programLocs.find(program);
        if (found == _programLocs.end())
        {
            ProgramLocations locations{
                glGetUniformLocation(GlName(program), "imm_color"),
                glGetUniformLocation(GlName(program), "alpha_test")
            };
            _programLocs[program] = locations;
            _immColorLoc = locations.ImmColor;
            _alphaTestLoc = locations.AlphaTest;
        }
        else
        {
            _immColorLoc = found->second.ImmColor;
            _alphaTestLoc = found->second.AlphaTest;
        }
    }

    std::int32_t GlEs::GetUniformLocation(std::int32_t program, const std::string* name)
    {
        return glGetUniformLocation(GlName(program),
            name == nullptr || name->empty() ? nullptr : name->c_str());
    }

    void GlEs::Enable(std::int32_t cap)
    {
        if (cap == GlAlphaTest)
        {
            _alphaTestEnabled = true;
            return;
        }
        if (IgnoredCap(cap))
        {
            return;
        }
        glEnable(GlEnum(cap));
    }

    void GlEs::Disable(std::int32_t cap)
    {
        if (cap == GlAlphaTest)
        {
            _alphaTestEnabled = false;
            return;
        }
        if (IgnoredCap(cap))
        {
            return;
        }
        glDisable(GlEnum(cap));
    }

    bool GlEs::IgnoredCap(std::int32_t cap)
    {
        return cap == GlTexture2D || cap == GlDebugOutput || cap == GlDebugOutputSynchronous;
    }

    void GlEs::AlphaFunc(std::int32_t func, float reference)
    {
        (void)reference;
        _alphaFunc = func;
    }

    void GlEs::PolygonMode(std::int32_t face, std::int32_t mode)
    {
        (void)face;
        (void)mode;
    }

    void GlEs::DebugMessageCallback(void* callback, void* userParam)
    {
        (void)callback;
        (void)userParam;
    }

    void GlEs::Clear(std::int32_t mask)
    {
        glClear(GlBits(mask));
    }

    void GlEs::ClearColor(const std::array<float, 4>& color)
    {
        glClearColor(color[0], color[1], color[2], color[3]);
    }

    void GlEs::ClearStencil(std::int32_t value)
    {
        glClearStencil(value);
    }

    void GlEs::ColorMask(bool red, bool green, bool blue, bool alpha)
    {
        glColorMask(red ? GL_TRUE : GL_FALSE, green ? GL_TRUE : GL_FALSE,
            blue ? GL_TRUE : GL_FALSE, alpha ? GL_TRUE : GL_FALSE);
    }

    void GlEs::DepthMask(bool flag)
    {
        glDepthMask(flag ? GL_TRUE : GL_FALSE);
    }

    void GlEs::DepthFunc(std::int32_t func)
    {
        glDepthFunc(GlEnum(func));
    }

    void GlEs::CullFace(std::int32_t mode)
    {
        glCullFace(GlEnum(mode));
    }

    void GlEs::BlendFunc(std::int32_t src, std::int32_t dst)
    {
        glBlendFunc(GlEnum(src), GlEnum(dst));
    }

    void GlEs::StencilFunc(std::int32_t func, std::int32_t reference, std::int32_t mask)
    {
        glStencilFunc(GlEnum(func), reference, Int32Bits(mask));
    }

    void GlEs::StencilOp(std::int32_t fail, std::int32_t zfail, std::int32_t zpass)
    {
        glStencilOp(GlEnum(fail), GlEnum(zfail), GlEnum(zpass));
    }

    void GlEs::StencilMask(std::int32_t mask)
    {
        glStencilMask(Int32Bits(mask));
    }

    void GlEs::PolygonOffset(float factor, float units)
    {
        glPolygonOffset(factor, units);
    }

    void GlEs::Viewport(std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height)
    {
        glViewport(x, y, width, height);
    }

    void GlEs::Scissor(std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height)
    {
        glScissor(x, y, width, height);
    }

    void GlEs::ClearColor(float red, float green, float blue, float alpha)
    {
        glClearColor(red, green, blue, alpha);
    }

    void GlEs::PixelStore(std::int32_t pname, std::int32_t param)
    {
        glPixelStorei(GlEnum(pname), param);
    }

    void GlEs::ReadBuffer(std::int32_t mode)
    {
        glReadBuffer(GlEnum(mode));
    }

    void GlEs::ActiveTexture(std::int32_t texture)
    {
        glActiveTexture(GlEnum(texture));
    }

    std::int32_t GlEs::GetError()
    {
        return static_cast<std::int32_t>(glGetError());
    }

    std::string GlEs::GetString(std::int32_t name)
    {
        const GLubyte* value = glGetString(GlEnum(name));
        if (value == nullptr)
        {
            return {};
        }
        return reinterpret_cast<const char*>(value);
    }

    std::int32_t GlEs::GetInteger(std::int32_t pname)
    {
        if (pname == GlContextFlags || pname == GlContextProfileMask)
        {
            return 0;
        }
        GLint result = 0;
        glGetIntegerv(GlEnum(pname), &result);
        return result;
    }

    void GlEs::TexParameter(std::int32_t target, std::int32_t pname, std::int32_t param)
    {
        glTexParameteri(GlEnum(target), GlEnum(pname), param);
    }

    void GlEs::TexImage2D(std::int32_t target, std::int32_t level, std::int32_t internalFormat,
        std::int32_t width, std::int32_t height, std::int32_t border, std::int32_t format,
        std::int32_t type, const void* pixels)
    {
        glTexImage2D(GlEnum(target), level, internalFormat, width, height, border,
            GlEnum(format), GlEnum(type), pixels);
    }

    void GlEs::TexSubImage2DRaw(std::int32_t target, std::int32_t level,
        std::int32_t xoffset, std::int32_t yoffset, std::int32_t width, std::int32_t height,
        std::int32_t format, std::int32_t type, const void* pixels)
    {
        glTexSubImage2D(GlEnum(target), level, xoffset, yoffset, width, height,
            GlEnum(format), GlEnum(type), pixels);
    }

    void GlEs::CopyTexSubImage2D(std::int32_t target, std::int32_t level,
        std::int32_t xoffset, std::int32_t yoffset, std::int32_t x, std::int32_t y,
        std::int32_t width, std::int32_t height)
    {
        glCopyTexSubImage2D(GlEnum(target), level, xoffset, yoffset, x, y, width, height);
    }

    void GlEs::ReadPixelsRaw(std::int32_t x, std::int32_t y, std::int32_t width,
        std::int32_t height, std::int32_t format, std::int32_t type, void* pixels)
    {
        glReadPixels(x, y, width, height, GlEnum(format), GlEnum(type), pixels);
    }

    std::int32_t GlEs::GenFramebuffer()
    {
        GLuint framebuffer = 0;
        glGenFramebuffers(1, &framebuffer);
        return ManagedName(framebuffer);
    }

    void GlEs::BindFramebuffer(std::int32_t target, std::int32_t framebuffer)
    {
        glBindFramebuffer(GlEnum(target), GlName(framebuffer));
    }

    void GlEs::FramebufferTexture2D(std::int32_t target, std::int32_t attachment,
        std::int32_t textarget, std::int32_t texture, std::int32_t level)
    {
        glFramebufferTexture2D(GlEnum(target), GlEnum(attachment), GlEnum(textarget),
            GlName(RealTexture(texture)), level);
    }

    std::int32_t GlEs::GenRenderbuffer()
    {
        GLuint renderbuffer = 0;
        glGenRenderbuffers(1, &renderbuffer);
        return ManagedName(renderbuffer);
    }

    void GlEs::BindRenderbuffer(std::int32_t target, std::int32_t renderbuffer)
    {
        glBindRenderbuffer(GlEnum(target), GlName(renderbuffer));
    }

    void GlEs::RenderbufferStorage(std::int32_t target, std::int32_t internalFormat,
        std::int32_t width, std::int32_t height)
    {
        glRenderbufferStorage(GlEnum(target), GlEnum(internalFormat), width, height);
    }

    void GlEs::FramebufferRenderbuffer(std::int32_t target, std::int32_t attachment,
        std::int32_t renderbufferTarget, std::int32_t renderbuffer)
    {
        glFramebufferRenderbuffer(GlEnum(target), GlEnum(attachment), GlEnum(renderbufferTarget),
            GlName(renderbuffer));
    }

    void GlEs::GetFramebufferAttachmentParameter(std::int32_t target,
        std::int32_t attachment, std::int32_t pname, std::int32_t& result)
    {
        GLint value = 0;
        glGetFramebufferAttachmentParameteriv(GlEnum(target), GlEnum(attachment), GlEnum(pname), &value);
        result = value;
    }

    std::int32_t GlEs::CheckFramebufferStatus(std::int32_t target)
    {
        return static_cast<std::int32_t>(glCheckFramebufferStatus(GlEnum(target)));
    }

    void GlEs::Uniform1(std::int32_t location, std::int32_t value)
    {
        glUniform1i(location, value);
    }

    void GlEs::Uniform1(std::int32_t location, float value)
    {
        glUniform1f(location, value);
    }

    void GlEs::Uniform1(std::int32_t location, std::int32_t count, std::span<const float> value)
    {
        glUniform1fv(location, count, value.empty() ? nullptr : value.data());
    }

    void GlEs::Uniform3(std::int32_t location, const std::array<float, 3>& vector)
    {
        glUniform3f(location, vector[0], vector[1], vector[2]);
    }

    void GlEs::Uniform3(std::int32_t location, std::int32_t count, std::span<const float> value)
    {
        glUniform3fv(location, count, value.empty() ? nullptr : value.data());
    }

    void GlEs::Uniform4(std::int32_t location, std::array<float, 4> vector)
    {
        glUniform4f(location, vector[0], vector[1], vector[2], vector[3]);
    }

    void GlEs::Uniform4(std::int32_t location, std::array<float, 4>* vector)
    {
        glUniform4f(location, (*vector)[0], (*vector)[1], (*vector)[2], (*vector)[3]);
    }

    void GlEs::Uniform4(std::int32_t location, float v0, float v1, float v2, float v3)
    {
        glUniform4f(location, v0, v1, v2, v3);
    }

    void GlEs::Uniform4(std::int32_t location, std::int32_t v0, std::int32_t v1,
        std::int32_t v2, std::int32_t v3)
    {
        glUniform4i(location, v0, v1, v2, v3);
    }

    void GlEs::UniformMatrix4(std::int32_t location, bool transpose,
        std::array<float, 16>* matrix)
    {
        glUniformMatrix4fv(location, 1, transpose ? GL_TRUE : GL_FALSE, matrix->data());
    }

    void GlEs::UniformMatrix4(std::int32_t location, std::int32_t count, bool transpose,
        std::span<const float> value)
    {
        glUniformMatrix4fv(location, count, transpose ? GL_TRUE : GL_FALSE,
            value.empty() ? nullptr : value.data());
    }
}
#endif
