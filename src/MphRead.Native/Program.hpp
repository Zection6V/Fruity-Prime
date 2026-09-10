#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace System
{
    // C++20 language/runtime seam for System.Version.
    //
    // This is intentionally limited to the System.Version behavior that is
    // observable through Program.cs:
    //   * two/four-component storage semantics,
    //   * TryParse,
    //   * comparison,
    //   * ToString.
    //
    // It is not an application-level replacement API and contains no
    // Fruity-Prime-specific behavior.
    class Version final
    {
    public:
        Version(int major, int minor);
        Version(int major, int minor, int build);
        Version(int major, int minor, int build, int revision);

        [[nodiscard]] int Major() const noexcept;
        [[nodiscard]] int Minor() const noexcept;
        [[nodiscard]] int Build() const noexcept;
        [[nodiscard]] int Revision() const noexcept;

        [[nodiscard]] std::string ToString() const;

        [[nodiscard]] static std::optional<Version> TryParse(
            std::string_view text) noexcept;

        friend bool operator==(const Version& left, const Version& right) noexcept;
        friend bool operator!=(const Version& left, const Version& right) noexcept;
        friend bool operator<(const Version& left, const Version& right) noexcept;
        friend bool operator<=(const Version& left, const Version& right) noexcept;
        friend bool operator>(const Version& left, const Version& right) noexcept;
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
        // C#:
        // public static Version Version { get; } = new Version(0, 35, 1, 0);
        static const System::Version Version;

        // C# Main is private because the CLR can designate a private method as
        // the assembly entry point. ISO C++ requires an externally callable
        // startup/ABI seam. This method is therefore exposed solely for that
        // seam; it remains the one and only application dispatcher.
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
