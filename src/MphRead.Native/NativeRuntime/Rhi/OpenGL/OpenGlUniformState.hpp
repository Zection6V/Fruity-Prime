#pragma once

#include "../SceneShaderAbi.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace MphRead::NativeRuntime::Rhi::OpenGL
{
    struct ShaderUniformLocation final
    {
        std::int32_t Program = 0, Location = -1;
        std::size_t Semantic = SceneShaderAbi::Constants.size();
        operator std::int32_t() const noexcept { return Location; }
    };

    // Each linked native program owns its values. Slots are generated semantic
    // indices, not strings or a content-derived hash. External GL invalidates
    // validity only; storage remains bounded across scene and UI switches.
    class OpenGlUniformState final
    {
    public:
        enum class Kind : std::uint8_t { Int, Float, FloatArray, Vec3, Vec4, Matrix4, Matrix4Transposed };
        static constexpr std::size_t Capacity = MatrixStackCapacity * 16 * sizeof(float);
        bool Update(std::size_t semantic, std::int32_t location, Kind kind,
            std::span<const std::byte> bytes) noexcept
        {
            if (location == -1) return false;
            if (semantic >= _values.size()) return true;
            auto& value = _values[semantic];
            if (bytes.empty() || bytes.size() > Capacity)
            { value.Current = false; return true; }
            if (value.Current && value.Location == location && value.Type == kind && value.Size == bytes.size()
                && std::memcmp(value.Bytes.data(), bytes.data(), bytes.size()) == 0) return false;
            std::memcpy(value.Bytes.data(), bytes.data(), bytes.size());
            value.Location = location; value.Type = kind; value.Size = bytes.size(); value.Current = true;
            return true;
        }
        void Invalidate() noexcept { for (auto& value : _values) value.Current = false; }
    private:
        struct Value final
        {
            std::array<std::byte, Capacity> Bytes{};
            std::size_t Size = 0;
            std::int32_t Location = -1;
            Kind Type{};
            bool Current = false;
        };
        std::array<Value, SceneShaderAbi::Constants.size()> _values{};
    };
}
