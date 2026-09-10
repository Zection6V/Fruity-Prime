#pragma once

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
        Version(int major, int minor);
        Version(int major, int minor, int build);
        Version(int major, int minor, int build, int revision);

        [[nodiscard]] static std::optional<Version> TryParse(std::string_view text) noexcept;

        friend bool operator>=(const Version& left, const Version& right) noexcept;

    private:
        int _major;
        int _minor;
        int _build;
        int _revision;
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
