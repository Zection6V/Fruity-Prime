#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace System
{
    class Version final
    {
    public:
        Version(std::int32_t major, std::int32_t minor);
        Version(std::int32_t major, std::int32_t minor, std::int32_t build);
        Version(std::int32_t major, std::int32_t minor, std::int32_t build, std::int32_t revision);

        [[nodiscard]] static std::optional<Version> TryParse(std::string_view text) noexcept;

        friend bool operator>=(const Version& left, const Version& right) noexcept;

    private:
        std::int32_t _major;
        std::int32_t _minor;
        std::int32_t _build;
        std::int32_t _revision;
    };
}

namespace MphRead
{
    class Program final
    {
    public:
        static const System::Version Version;

        // Language/runtime entry seam: C# can designate a private method as the
        // CLR entry point; native startup code must be able to call this symbol.
        static void Main(const std::vector<std::string>& args);

        Program() = delete;
        Program(const Program&) = delete;
        Program& operator=(const Program&) = delete;
    };

    class ProgramException : public std::runtime_error
    {
    public:
        explicit ProgramException(const std::string& message);
    };
}
