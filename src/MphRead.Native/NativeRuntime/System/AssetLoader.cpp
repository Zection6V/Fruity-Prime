#include "AssetLoader.hpp"

#include "Exceptions.hpp"
#include "IO.hpp"
#include "Runtime.hpp"

namespace MphRead::NativeRuntime
{
    std::optional<std::string> AssetLoader::Resolve(std::string_view uri)
    {
        std::string path(uri);
        const std::string scheme = "avares://";
        if (path.rfind(scheme, 0) == 0)
        {
            path = path.substr(scheme.size());
            // The assembly name is the first segment.
            const std::size_t slash = path.find('/');
            path = slash == std::string::npos ? std::string() : path.substr(slash + 1);
        }
        const std::size_t hash = path.find('#');
        if (hash != std::string::npos)
        {
            path = path.substr(0, hash);
        }
#if defined(__ANDROID__)
        // The native Android host copies the packaged Assets/ tree into the
        // app's writable game root and makes that root the current directory.
        // /proc/self/exe points into the package runtime, not at those files.
        const std::string current = EnvironmentCurrentDirectory();
        const std::string androidDirect = PathCombine(current, path);
        if (FileExists(androidDirect))
        {
            return androidDirect;
        }
#endif
        const std::string base = AppContextBaseDirectory();
        const std::string direct = PathCombine(base, path);
        if (FileExists(direct))
        {
            return direct;
        }
        // Older layouts carried the assets flat beside the executable.
        const std::size_t name = path.rfind('/');
        const std::string flat = PathCombine(base, name == std::string::npos ? path : path.substr(name + 1));
        if (FileExists(flat))
        {
            return flat;
        }
        return std::nullopt;
    }

    std::vector<std::uint8_t> AssetLoader::Open(std::string_view uri)
    {
        const std::optional<std::string> path = Resolve(uri);
        if (!path.has_value())
        {
            throw System::IO::FileNotFoundException("The resource " + std::string(uri) + " could not be found.");
        }
        return FileReadAllBytes(*path);
    }

    bool AssetLoader::Exists(std::string_view uri)
    {
        return Resolve(uri).has_value();
    }
}
