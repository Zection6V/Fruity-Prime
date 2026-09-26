#pragma once

// Avalonia.Platform's AssetLoader: an avares: URI names a file the published
// build carries under Assets/ beside the executable.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace MphRead::NativeRuntime::Avalonia::Platform
{
    class AssetLoader final
    {
    public:
        AssetLoader() = delete;

        // The file an avares: URI names, or nothing when it is not there.
        [[nodiscard]] static std::optional<std::string> Resolve(std::string_view uri);
        // AssetLoader.Open(uri), read whole. Throws FileNotFoundException.
        [[nodiscard]] static std::vector<std::uint8_t> Open(std::string_view uri);
        [[nodiscard]] static bool Exists(std::string_view uri);
    };
}
