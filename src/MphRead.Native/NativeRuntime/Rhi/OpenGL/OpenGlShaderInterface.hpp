#pragma once

#include "../GraphicsDevice.hpp"
#include "../SceneShaders.hpp"

#include <memory>
#include <span>
#include <string>

namespace MphRead::NativeRuntime::Rhi::OpenGL
{
    // The GLSL the renderer's programs are built from, and the two tables
    // they are initialised with. The sources are held by reference, not
    // copied: the Android head swaps in its ES versions by the identity of
    // these strings.
    struct SceneShaderSources final
    {
        const std::string* MainVertex = nullptr;
        const std::string* MainFragment = nullptr;
        const std::string* CompositeVertex = nullptr;
        const std::string* CompositeFragment = nullptr;
        const std::string* ShiftFragment = nullptr;
        const std::string* CelFragment = nullptr;
        // Metadata::ToonTable as xyz triples, and the disruption shift table.
        std::span<const float> ToonTable{};
        std::span<const float> ShiftTable{};
    };

    // Compile and link the four programs, look up every uniform the
    // renderer sets (the location cache stays in here), and set the
    // uniforms that never change: the texture units and the two tables.
    // Leaves the main program current, as the renderer always has. Throws
    // with the compiler's log when a shader does not compile.
    [[nodiscard]] std::unique_ptr<SceneShaderSet> CreateSceneShaderSet(
        GraphicsDevice& device, const SceneShaderSources& sources);
}
