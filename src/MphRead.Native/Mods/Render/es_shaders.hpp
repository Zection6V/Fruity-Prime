#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace fruityprime::mods::render {

// Native counterpart of MphRead/Mods/Render/EsShaders.cs.  The desktop
// renderer and the Android ES renderer have different shader source strings,
// but the six ES programs are still one named source unit with one translation
// table and one drift check.
enum class EsShader {
    Vertex,
    Fragment,
    RttVertex,
    RttFragment,
    CelFragment,
    ShiftFragment
};

struct EsShaderSyncEntry final {
    std::string_view name;
    std::string_view desktop_source;
    std::string_view expected_sha256;
};

[[nodiscard]] std::string_view es_shader_source(EsShader shader) noexcept;

[[nodiscard]] std::span<const EsShader> all_es_shaders() noexcept;

// C# uses reference identity because each desktop shader is a static string.
// C++ callers pass the six corresponding desktop source views; content
// equality is the portable equivalent for a string_view-based API.
[[nodiscard]] std::optional<std::string_view> translate_es_shader(
    std::string_view desktop_source,
    std::span<const std::string_view> desktop_sources) noexcept;

// Normalize CRLF to LF, hash the UTF-8 bytes, and report the first mismatch.
// This is the native equivalent of CheckInSync and keeps Android shader drift
// visible even when a checkout changes line endings.
[[nodiscard]] bool check_es_shaders_in_sync(
    std::span<const EsShaderSyncEntry> entries, std::string& error);

} // namespace fruityprime::mods::render
