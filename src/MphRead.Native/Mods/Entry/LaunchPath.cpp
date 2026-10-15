#include "LaunchPath.hpp"

#include "../../NativeRuntime/System/IO.hpp"

#include <filesystem>

namespace MphRead::Mods::Entry
{
    std::string FullPathCombine(std::string_view directory, std::string_view value)
    {
        using ::MphRead::NativeRuntime::PathFromUtf8;
        using ::MphRead::NativeRuntime::PathToUtf8;
        const std::filesystem::path combined = PathFromUtf8(directory) / PathFromUtf8(value);
        return PathToUtf8(std::filesystem::absolute(combined).lexically_normal());
    }
}
