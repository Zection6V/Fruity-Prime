#include "Icu.hpp"

#include "Console.hpp"

#include <array>
#include <optional>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace MphRead::NativeRuntime::Icu
{
    namespace
    {
        // GlobalizationMode's switches: "true" or "1" turns one on, "false"
        // or "0" off, anything else leaves the default.
        [[nodiscard]] bool Switch(const char* name) noexcept
        {
            try
            {
                const std::optional<std::string> value = EnvironmentGetVariable(name);
                if (!value.has_value())
                {
                    return false;
                }
                std::string lowered = *value;
                for (char& ch : lowered)
                {
                    if (ch >= 'A' && ch <= 'Z')
                    {
                        ch = static_cast<char>(ch - 'A' + 'a');
                    }
                }
                return lowered == "1" || lowered == "true";
            }
            catch (...)
            {
                return false;
            }
        }

#if defined(_WIN32)
        using Library = HMODULE;

        [[nodiscard]] void* Lookup(Library library, const char* name) noexcept
        {
            return reinterpret_cast<void*>(GetProcAddress(library, name));
        }

        // Windows 10 1903 and later carry ICU as icu.dll (one library for
        // both halves); an application-local ICU is named by
        // DOTNET_SYSTEM_GLOBALIZATION_APPLOCALICU as "<version>[:<suffix>]".
        [[nodiscard]] std::vector<Library> Open()
        {
            std::vector<std::string> candidates;
            if (const std::optional<std::string> appLocal
                = EnvironmentGetVariable("DOTNET_SYSTEM_GLOBALIZATION_APPLOCALICU");
                appLocal.has_value() && !appLocal->empty())
            {
                std::string version = *appLocal;
                if (const std::size_t colon = version.find(':'); colon != std::string::npos)
                {
                    version.erase(0, colon + 1);
                }
                std::erase(version, '.');
                candidates.push_back("icuuc" + version + ".dll");
                candidates.push_back("icuin" + version + ".dll");
            }
            candidates.emplace_back("icu.dll");
            candidates.emplace_back("icuuc.dll");
            candidates.emplace_back("icuin.dll");
            std::vector<Library> libraries;
            for (const std::string& candidate : candidates)
            {
                if (Library library = LoadLibraryA(candidate.c_str()); library != nullptr)
                {
                    libraries.push_back(library);
                }
            }
            return libraries;
        }
#else
        using Library = void*;

        [[nodiscard]] void* Lookup(Library library, const char* name) noexcept
        {
            return dlsym(library, name);
        }

        // Linux distributions ship ICU with its major version in the soname
        // and often without the unversioned link; macOS ships it as
        // libicucore, with plain symbol names.
        [[nodiscard]] std::vector<Library> Open()
        {
            std::vector<Library> libraries;
            const auto tryOpen = [&libraries](const std::string& file)
            {
                if (Library library = dlopen(file.c_str(), RTLD_LAZY | RTLD_LOCAL); library != nullptr)
                {
                    libraries.push_back(library);
                    return true;
                }
                return false;
            };
#if defined(__APPLE__)
            if (tryOpen("/usr/lib/libicucore.A.dylib") || tryOpen("libicucore.dylib"))
            {
                return libraries;
            }
#endif
            for (const char* stem : { "libicuuc", "libicui18n" })
            {
                if (tryOpen(std::string(stem) + ".so"))
                {
                    continue;
                }
                for (int major = 100; major >= 50; --major)
                {
                    if (tryOpen(std::string(stem) + ".so." + std::to_string(major)))
                    {
                        break;
                    }
                }
            }
            return libraries;
        }
#endif

        [[nodiscard]] const std::vector<Library>& Libraries()
        {
            static const std::vector<Library> libraries = []
            {
                return InvariantMode() ? std::vector<Library>{} : Open();
            }();
            return libraries;
        }
    }

    bool InvariantMode() noexcept
    {
        static const bool invariant = Switch("DOTNET_SYSTEM_GLOBALIZATION_INVARIANT");
        return invariant;
    }

#if defined(_WIN32)
    bool UseNlsRequested() noexcept
    {
        static const bool useNls = Switch("DOTNET_SYSTEM_GLOBALIZATION_USENLS");
        return useNls;
    }
#endif

    bool Available() noexcept
    {
        try
        {
            return !Libraries().empty();
        }
        catch (...)
        {
            return false;
        }
    }

    void* Function(std::string_view name) noexcept
    {
        try
        {
            const std::vector<Library>& libraries = Libraries();
            const std::string plain(name);
            for (const Library library : libraries)
            {
                if (void* symbol = Lookup(library, plain.c_str()); symbol != nullptr)
                {
                    return symbol;
                }
            }
            for (const Library library : libraries)
            {
                for (int major = 100; major >= 50; --major)
                {
                    const std::string renamed = plain + "_" + std::to_string(major);
                    if (void* symbol = Lookup(library, renamed.c_str()); symbol != nullptr)
                    {
                        return symbol;
                    }
                }
            }
        }
        catch (...)
        {
        }
        return nullptr;
    }
}
