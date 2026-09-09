#pragma once

#include "Formats/model_format.hpp"
#include "Formats/movie.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace fruityprime::exporter {

struct ModelExportStats {
    std::size_t meshes = 0;
    std::size_t primitives = 0;
    std::size_t vertices = 0;
    std::size_t faces = 0;
};

// Inputs that the managed Export.Scripting class receives from the model
// exporter but which are not part of the cartridge model record itself.  The
// native model keeps geometry and animation data; the caller supplies the
// export name, output root, and the recolor labels that the generated Blender
// script should offer.
struct ScriptingOptions {
    std::string model_name = "model";
    std::filesystem::path export_root;
    std::string export_version = "0.35.1.0";
    std::vector<std::string> recolor_names{"default"};
};

// Generate the Blender import script used by Export/Scripting.cs.  The
// returned text is independent of a UI or graphics API and can therefore be
// tested against synthetic model/animation records as well as a real ROM.
[[nodiscard]] std::string generate_script(
    const model::File& model, const ScriptingOptions& options = {});

void write_script(const model::File& model,
                  const std::filesystem::path& output,
                  const ScriptingOptions& options = {});

// Export decoded cartridge geometry to a renderer-independent Wavefront OBJ.
// The accompanying MTL is written next to it. This is the native equivalent
// of the C# Export.Collada/Images boundary for tools that do not need the
// managed OpenGL window.
[[nodiscard]] ModelExportStats write_obj(
    const model::File& model, const std::filesystem::path& output);

// Export the same decoded static geometry to COLLADA 1.4.1.  The native
// exporter writes positions, normals, UVs, per-material diffuse colors, and
// the model's node-independent mesh list.  Animated node/material tracks are
// intentionally not synthesized here; callers get a valid static scene that
// can be opened by the same DCC tools as the managed exporter.
[[nodiscard]] ModelExportStats write_collada(
    const model::File& model, const std::filesystem::path& output);

struct TextureExportStats {
    std::size_t images = 0;
    std::size_t pixels = 0;
};

// Write an 8-bit RGBA PNG without requiring an image library.  This is used
// by the native replacement for the managed Export.Images path and is also a
// small public image boundary for HUD/model tools.
void write_png_rgba(const std::filesystem::path& output, int width, int height,
                    std::span<const std::uint8_t> rgba);

// Export the indexed/direct model textures referenced by its materials.  A
// unique texture/palette pair becomes `texture-N-pM.png`; direct-color images
// use `texture-N-direct.png`.
[[nodiscard]] TextureExportStats write_model_textures(
    const model::File& model, const std::filesystem::path& output_directory);

struct MovieExportStats {
    std::size_t frames = 0;
    std::size_t audio_frames = 0;
};

// Export a decoded VXDS movie as numbered RGB PNG frames and, when present,
// a mono PCM16 WAV stream. The decoder must have completed decode().
[[nodiscard]] MovieExportStats write_movie(
    const movie::VxDecoder& decoder,
    const std::filesystem::path& output_directory);

} // namespace fruityprime::exporter

namespace MphReadNative {
namespace Export = ::fruityprime::exporter;
}
