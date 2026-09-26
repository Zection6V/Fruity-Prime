#include "MapBundle.hpp"

#include "MapDefinition.hpp"
#include "Q3Bsp.hpp"
#include "Q3Import.hpp"
#include "../../Formats/Types.hpp"
#include "../../Program.hpp"
#include "../../NativeRuntime/System/Encoding.hpp"
#include "../../NativeRuntime/System/IO.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "NativeRuntime/System/Globalization.hpp"
#include "NativeRuntime/System/ZipArchive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cwctype>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#include <sys/types.h>
#endif

using ::MphRead::NativeRuntime::AppendUtf8;
using ::MphRead::NativeRuntime::FileInfoLength;
using ::MphRead::NativeRuntime::FileReadAllBytes;
using ::MphRead::NativeRuntime::PathCombine;
using ::MphRead::NativeRuntime::PathFromUtf8;
using ::MphRead::NativeRuntime::PathGetExtension;
using ::MphRead::NativeRuntime::PathGetFileName;
using ::MphRead::NativeRuntime::PathGetFileNameWithoutExtension;
using ::MphRead::NativeRuntime::PathToUtf8;
using ::MphRead::NativeRuntime::StreamReaderDecode;
using ::MphRead::NativeRuntime::UncheckedAdd;
using ::MphRead::NativeRuntime::Utf8GetString;
using ::MphRead::NativeRuntime::Utf8ToUtf32;

namespace MphRead::Mods::MapGen
{
    // Transitional dependency surface. CustomRooms is a later dependency-order
    // item; this declaration binds exactly the member used by MapBundle without
    // changing that file in this migration slice.
    class CustomRooms final
    {
    public:
        [[nodiscard]] static const std::string& MapDirectory();
    };
}

namespace
{
    using ByteVector = std::vector<std::uint8_t>;
    using MphRead::Mods::MapGen::MapBundle;

    void MoveOverwrite(const std::string& source, const std::string& destination)
    {
#if defined(_WIN32)
        const std::filesystem::path sourcePath = PathFromUtf8(source);
        const std::filesystem::path destinationPath = PathFromUtf8(destination);
        if (!MoveFileExW(
                sourcePath.c_str(), destinationPath.c_str(),
                MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING))
        {
            throw std::system_error(
                static_cast<int>(GetLastError()), std::system_category(),
                "Could not move temporary bundle");
        }
#else
        const std::filesystem::path sourcePath = PathFromUtf8(source);
        const std::filesystem::path destinationPath = PathFromUtf8(destination);
        if (std::rename(sourcePath.c_str(), destinationPath.c_str()) != 0)
        {
            const int error = errno;
            if (error != EXDEV)
            {
                throw std::system_error(
                    error, std::generic_category(), "Could not move temporary bundle");
            }

            // File.Move(..., overwrite: true) on Unix falls back to copy+delete
            // when rename crosses a device/mount boundary.
            std::error_code copyError;
            std::filesystem::copy_file(
                sourcePath, destinationPath,
                std::filesystem::copy_options::overwrite_existing, copyError);
            if (copyError)
            {
                throw std::system_error(
                    copyError, "Could not copy temporary bundle across devices");
            }

            std::error_code deleteError;
            (void)std::filesystem::remove(sourcePath, deleteError);
            if (deleteError)
            {
                throw std::system_error(
                    deleteError, "Could not delete temporary bundle after cross-device copy");
            }
        }
#endif
    }

}

namespace MphRead::Mods::MapGen
{
    bool MapBundle::Is(const std::string& path)
    {
        return ::MphRead::NativeRuntime::StringEqualsOrdinalIgnoreCase(PathGetExtension(path), Extension);
    }

    std::string MapBundle::Cook(
        MapDefinition* definition,
        const std::string& recipePath,
        const std::optional<std::string>& outputPath,
        bool verbose)
    {
        if (definition == nullptr)
        {
            throw System::NullReferenceException();
        }

        MapImport* import = definition->Import();
        if (import == nullptr || import->Source().empty())
        {
            throw ProgramException(
                definition->Name()
                + " builds from its own description; there is no level to bundle.");
        }

        std::optional<std::string> level = import->Resolve();
        if (!level)
        {
            throw ProgramException(
                definition->Name() + ": its source level " + import->Source()
                + " is not here, so there is nothing to cook.");
        }

        const std::optional<std::string>& requestedMapName = import->MapName();
        const std::string mapName = requestedMapName
            ? *requestedMapName
            : PathGetFileNameWithoutExtension(*level);
        const ByteVector trimmed = Q3Bsp::Trim(
            Q3Bsp::ReadLevel(*level, import->MapName()));

        std::optional<std::string> texturePath = import->ResolveTextures();
        const std::optional<std::string>& requestedTextures = import->Textures();
        if (!texturePath && requestedTextures && !requestedTextures->empty())
        {
            texturePath = Q3Import::BakeTextures(
                Q3Bsp::Load(*level, import->MapName()), import, verbose);
            if (!texturePath)
            {
                throw ProgramException(
                    definition->Name() + ": its textures (" + *import->Textures()
                    + ") are not beside its recipe and could not be baked from "
                    + PathGetFileName(*level)
                    + ". A bundle without them is a room with no materials, "
                      "so this is a failure and not a bundle.");
            }
        }

        const std::string path = outputPath
            ? *outputPath
            : PathCombine(
                CustomRooms::MapDirectory(),
                PathGetFileNameWithoutExtension(recipePath) + Extension);
        const std::string recipeName = PathGetFileName(recipePath);
        const std::string textureName = texturePath
            ? PathGetFileName(*texturePath) : std::string{};

        // A hand-edited collision mesh is the map, not a working file: the room
        // cannot be built without it, so it travels with the recipe.
        std::optional<std::string> collisionPath;
        if (const MapCollision* collision = definition->Collision();
            collision != nullptr && !collision->Source.empty())
        {
            collisionPath = collision->Resolve();
            if (!collisionPath.has_value())
            {
                throw ProgramException(definition->Name() + ": its collision mesh " + collision->Source
                    + " is not beside its recipe, and a bundle without it is a map that cannot be generated.");
            }
        }
        std::shared_ptr<MapDefinition> inside = MapDefinition::Load(recipePath);
        if (MapImport* insideImport = inside->Import(); insideImport != nullptr)
        {
            insideImport->Source(std::string(LevelDirectory) + mapName + ".bsp");
            insideImport->MapName(mapName);
            insideImport->Textures(textureName);
        }
        if (inside->Collision() != nullptr && collisionPath.has_value())
        {
            inside->Collision()->Source = PathGetFileName(*collisionPath);
        }

        const std::string temporary = path + ".tmp";
        {
            // using (var file = File.Create(temporary))
            // using (var archive = new ZipArchive(file, ZipArchiveMode.Create))
            using ::MphRead::NativeRuntime::CompressionLevel;
            using ::MphRead::NativeRuntime::ZipArchive;
            auto file = std::make_shared<::MphRead::NativeRuntime::FileStream>(temporary,
                ::MphRead::NativeRuntime::FileMode::Create, ::MphRead::NativeRuntime::FileAccess::ReadWrite,
                ::MphRead::NativeRuntime::FileShare::None);
            ZipArchive archive(file, ::MphRead::NativeRuntime::ZipArchiveMode::Create);
            const auto write = [&archive](const std::string& name, std::span<const std::uint8_t> bytes)
            {
                const auto entry = archive.CreateEntry(name, CompressionLevel::SmallestSize);
                const auto stream = entry->Open();
                stream->Write(bytes);
                stream->Dispose();
            };
            const std::string serialized = inside->Serialize();
            write(recipeName, std::span(reinterpret_cast<const std::uint8_t*>(serialized.data()), serialized.size()));
            write(std::string(LevelDirectory) + mapName + ".bsp", trimmed);
            if (texturePath)
            {
                write(textureName, FileReadAllBytes(*texturePath));
            }
            if (collisionPath.has_value())
            {
                write(PathGetFileName(*collisionPath), FileReadAllBytes(*collisionPath));
            }
            archive.Dispose();
        }

        MoveOverwrite(temporary, path);

        if (verbose)
        {
            const std::int64_t levelLength = FileInfoLength(*level);
            const std::int64_t textureLength
                = texturePath ? FileInfoLength(*texturePath) : 0;
            const std::int64_t before = UncheckedAdd(levelLength, textureLength);
            std::cout << "[mapbundle] " << definition->Name() << " -> " << path
                << " (" << FileInfoLength(path) / 1024 << " KiB, from "
                << before / 1024 << " KiB)" << std::endl;
        }
        return path;
    }

    std::optional<std::string> MapBundle::ReadRecipe(const std::string& bundlePath)
    {
        const auto archive = ::MphRead::NativeRuntime::ZipArchive::OpenRead(bundlePath);
        for (const auto& entry : archive->Entries())
        {
            if (::MphRead::NativeRuntime::StringEndsWithOrdinalIgnoreCase(entry->FullName(), ".json"))
            {
                return StreamReaderDecode(entry->ReadAllBytes());
            }
        }
        return std::nullopt;
    }

    std::optional<std::vector<std::uint8_t>> MapBundle::ReadEntry(
        const std::string& bundlePath, const std::string& name)
    {
        if (name.empty())
        {
            return std::nullopt;
        }
        const auto archive = ::MphRead::NativeRuntime::ZipArchive::OpenRead(bundlePath);
        for (const auto& entry : archive->Entries())
        {
            if (::MphRead::NativeRuntime::StringEqualsOrdinalIgnoreCase(entry->FullName(), name))
            {
                return entry->ReadAllBytes();
            }
        }
        const std::string suffix = "/" + name;
        for (const auto& entry : archive->Entries())
        {
            if (::MphRead::NativeRuntime::StringEndsWithOrdinalIgnoreCase(entry->FullName(), suffix))
            {
                return entry->ReadAllBytes();
            }
        }
        return std::nullopt;
    }
}
