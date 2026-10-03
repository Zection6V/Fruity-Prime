#pragma once
#if !defined(__ANDROID__)

#include "GL.hpp"

#include <cstdio>
#include <string>
#include <string_view>

#if defined(_WIN32)
#define FRUITY_GL_FEATURES_CALL __stdcall
#else
#define FRUITY_GL_FEATURES_CALL
#endif

namespace OpenTK::Graphics::OpenGL
{
    // What the context current on this thread actually offers. An entry point
    // that resolves proves nothing: macOS exports every GL 4.1 symbol from its
    // one framework, so glGenSamplers resolves in the legacy 2.1 context the
    // renderer has to run on there and then fails with INVALID_OPERATION. The
    // version and the extension string are the answer; nothing else is.
    struct GlFeatures final
    {
        int Major = 0;
        int Minor = 0;
        std::string Extensions; // space-delimited, with a space at either end

        [[nodiscard]] bool AtLeast(int major, int minor) const noexcept
        {
            return Major > major || (Major == major && Minor >= minor);
        }

        [[nodiscard]] bool Has(std::string_view name) const
        {
            std::string token;
            token.reserve(name.size() + 2);
            token += ' ';
            token += name;
            token += ' ';
            return Extensions.find(token) != std::string::npos;
        }

        // Core in `major.minor`, or offered by the extension of that name.
        [[nodiscard]] bool Supports(int major, int minor, std::string_view extension) const
        {
            return AtLeast(major, minor) || Has(extension);
        }

        // Read once per context. GL_EXTENSIONS as one string is valid in the
        // compatibility and legacy contexts this renderer asks for.
        [[nodiscard]] static GlFeatures Current()
        {
            using GetStringFn = const unsigned char*(FRUITY_GL_FEATURES_CALL*)(unsigned);
            GlFeatures features;
            const auto getString = reinterpret_cast<GetStringFn>(GetEntryPoint("glGetString"));
            if (getString == nullptr) return features;
            if (const auto* version = getString(0x1F02)) // GL_VERSION
            {
                (void)std::sscanf(reinterpret_cast<const char*>(version), "%d.%d", &features.Major, &features.Minor);
            }
            features.Extensions = " ";
            if (const auto* extensions = getString(0x1F03)) // GL_EXTENSIONS
            {
                features.Extensions += reinterpret_cast<const char*>(extensions);
            }
            features.Extensions += ' ';
            return features;
        }
    };
}

#undef FRUITY_GL_FEATURES_CALL
#endif
