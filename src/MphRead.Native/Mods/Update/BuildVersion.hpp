#pragma once

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace MphRead::Mods::Update
{
    // C++20 value equivalent used for the System.Version values exposed by
    // BuildVersion.cs. Build and Revision use -1 for the .NET "unspecified"
    // state, and comparison is lexicographic across all four components.
    class Version final
    {
    public:
        Version() noexcept;
        Version(std::int32_t major, std::int32_t minor);
        Version(std::int32_t major, std::int32_t minor, std::int32_t build);
        Version(std::int32_t major, std::int32_t minor, std::int32_t build,
            std::int32_t revision);

        [[nodiscard]] std::int32_t Major() const noexcept;
        [[nodiscard]] std::int32_t Minor() const noexcept;
        [[nodiscard]] std::int32_t Build() const noexcept;
        [[nodiscard]] std::int32_t Revision() const noexcept;

        [[nodiscard]] std::string ToString() const;
        [[nodiscard]] std::string ToString(std::int32_t fieldCount) const;

        [[nodiscard]] friend bool operator==(const Version& left,
            const Version& right) noexcept = default;
        friend std::strong_ordering operator<=>(const Version& left,
            const Version& right) noexcept;

    private:
        std::int32_t _major = 0;
        std::int32_t _minor = 0;
        std::int32_t _build = -1;
        std::int32_t _revision = -1;
    };

    class BuildVersion final
    {
    public:
        // The release this binary is, or nullopt for a local build. The value is
        // evaluated once, on first access, matching Lazy<Version?>.
        [[nodiscard]] static const std::optional<Version>& Current();

        // True when this build came out of the release workflow.
        [[nodiscard]] static bool IsRelease();

        // "v1.2.0", or "a local build" when there is no stamp.
        [[nodiscard]] static std::string Display();

        // "v1.2.0", "1.2.0", "1.2" -> a Version. Anything else, including
        // the SDK's unstamped 1.0.0, is not a release.
        [[nodiscard]] static std::optional<Version> Parse(
            std::optional<std::string_view> text);

        // Compare on three parts; the fourth is never in a tag.
        [[nodiscard]] static Version Normalise(const Version& version);

        BuildVersion() = delete;
        BuildVersion(const BuildVersion&) = delete;
        BuildVersion& operator=(const BuildVersion&) = delete;

    private:
        struct LazyCurrent;

        [[nodiscard]] static std::optional<Version> Read();

        static LazyCurrent _current;
    };
}
