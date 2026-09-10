#include "BuildVersion.hpp"

#include <array>
#include <cstddef>
#include <exception>
#include <limits>
#include <mutex>
#include <stdexcept>

namespace MphRead::Mods::Update
{
    namespace
    {
        [[nodiscard]] constexpr bool IsNumberWhiteSpace(unsigned char value) noexcept
        {
            // NumberStyles.Integer in .NET accepts only ASCII space and U+0009..U+000D
            // as leading/trailing whitespace.
            return value == 0x20 || (value >= 0x09 && value <= 0x0D);
        }

        [[nodiscard]] constexpr bool IsDotNetWhiteSpace(char32_t value) noexcept
        {
            // Char.IsWhiteSpace / String.Trim whitespace set used by .NET 9.
            return (value >= U'\u0009' && value <= U'\u000D')
                || value == U'\u0020'
                || value == U'\u0085'
                || value == U'\u00A0'
                || value == U'\u1680'
                || (value >= U'\u2000' && value <= U'\u200A')
                || value == U'\u2028'
                || value == U'\u2029'
                || value == U'\u202F'
                || value == U'\u205F'
                || value == U'\u3000';
        }

        struct Utf8Character
        {
            char32_t Value;
            std::size_t Length;
            bool Valid;
        };

        [[nodiscard]] Utf8Character DecodeUtf8(std::string_view text,
            std::size_t offset) noexcept
        {
            const auto byte = static_cast<unsigned char>(text[offset]);
            if (byte < 0x80)
            {
                return {byte, 1, true};
            }

            std::size_t length = 0;
            char32_t value = 0;
            char32_t minimum = 0;
            if ((byte & 0xE0) == 0xC0)
            {
                length = 2;
                value = byte & 0x1F;
                minimum = 0x80;
            }
            else if ((byte & 0xF0) == 0xE0)
            {
                length = 3;
                value = byte & 0x0F;
                minimum = 0x800;
            }
            else if ((byte & 0xF8) == 0xF0)
            {
                length = 4;
                value = byte & 0x07;
                minimum = 0x10000;
            }
            else
            {
                return {byte, 1, false};
            }

            if (offset + length > text.size())
            {
                return {byte, 1, false};
            }
            for (std::size_t i = 1; i < length; ++i)
            {
                const auto continuation = static_cast<unsigned char>(text[offset + i]);
                if ((continuation & 0xC0) != 0x80)
                {
                    return {byte, 1, false};
                }
                value = (value << 6) | (continuation & 0x3F);
            }
            if (value < minimum || value > 0x10FFFF
                || (value >= 0xD800 && value <= 0xDFFF))
            {
                return {byte, 1, false};
            }
            return {value, length, true};
        }

        [[nodiscard]] std::string_view TrimDotNetWhiteSpace(
            std::string_view text) noexcept
        {
            std::size_t first = 0;
            while (first < text.size())
            {
                const Utf8Character character = DecodeUtf8(text, first);
                if (!character.Valid || !IsDotNetWhiteSpace(character.Value))
                {
                    break;
                }
                first += character.Length;
            }

            std::size_t cursor = first;
            std::size_t lastNonWhite = first;
            while (cursor < text.size())
            {
                const Utf8Character character = DecodeUtf8(text, cursor);
                if (!character.Valid || !IsDotNetWhiteSpace(character.Value))
                {
                    lastNonWhite = cursor + character.Length;
                }
                cursor += character.Length;
            }
            return text.substr(first, lastNonWhite - first);
        }

        [[nodiscard]] bool TryParseInt32Component(std::string_view component,
            std::int32_t& result) noexcept
        {
            std::size_t first = 0;
            while (first < component.size()
                && IsNumberWhiteSpace(static_cast<unsigned char>(component[first])))
            {
                ++first;
            }
            if (first == component.size())
            {
                return false;
            }

            bool negative = false;
            if (component[first] == '+' || component[first] == '-')
            {
                negative = component[first] == '-';
                ++first;
                if (first == component.size())
                {
                    return false;
                }
            }

            if (component[first] < '0' || component[first] > '9')
            {
                return false;
            }

            const std::uint64_t limit = negative ? 2147483648ULL : 2147483647ULL;
            std::uint64_t value = 0;
            std::size_t cursor = first;
            while (cursor < component.size()
                && component[cursor] >= '0' && component[cursor] <= '9')
            {
                const auto digit = static_cast<unsigned char>(component[cursor] - '0');
                if (value > limit / 10
                    || (value == limit / 10 && digit > limit % 10))
                {
                    return false;
                }
                value = value * 10 + digit;
                ++cursor;
            }

            while (cursor < component.size()
                && IsNumberWhiteSpace(static_cast<unsigned char>(component[cursor])))
            {
                ++cursor;
            }

            // CoreLib's integer parser accepts trailing U+0000 characters for
            // compatibility. They must come after any trailing whitespace.
            while (cursor < component.size() && component[cursor] == '\0')
            {
                ++cursor;
            }
            if (cursor != component.size())
            {
                return false;
            }

            if (negative)
            {
                // System.Version rejects every negative component. Keep the
                // Int32 minimum boundary correct even though BuildVersion.Parse
                // rejects '-' before Version parsing is reached.
                if (value > 2147483648ULL)
                {
                    return false;
                }
                if (value == 2147483648ULL)
                {
                    result = std::numeric_limits<std::int32_t>::min();
                }
                else
                {
                    result = -static_cast<std::int32_t>(value);
                }
                return false;
            }

            if (value > static_cast<std::uint64_t>(
                    std::numeric_limits<std::int32_t>::max()))
            {
                return false;
            }
            result = static_cast<std::int32_t>(value);
            return true;
        }

        [[nodiscard]] std::optional<Version> TryParseVersion(std::string_view text)
        {
            std::array<std::string_view, 4> parts{};
            std::size_t count = 0;
            std::size_t start = 0;
            while (true)
            {
                if (count == parts.size())
                {
                    return std::nullopt;
                }
                const std::size_t dot = text.find('.', start);
                parts[count++] = dot == std::string_view::npos
                    ? text.substr(start)
                    : text.substr(start, dot - start);
                if (dot == std::string_view::npos)
                {
                    break;
                }
                start = dot + 1;
            }

            if (count < 2 || count > 4)
            {
                return std::nullopt;
            }

            std::array<std::int32_t, 4> values{};
            for (std::size_t i = 0; i < count; ++i)
            {
                if (!TryParseInt32Component(parts[i], values[i]))
                {
                    return std::nullopt;
                }
            }

            switch (count)
            {
            case 2:
                return Version(values[0], values[1]);
            case 3:
                return Version(values[0], values[1], values[2]);
            case 4:
                return Version(values[0], values[1], values[2], values[3]);
            default:
                return std::nullopt;
            }
        }

        [[nodiscard]] std::string JoinVersion(const Version& version,
            std::int32_t fieldCount)
        {
            std::string result;
            if (fieldCount == 0)
            {
                return result;
            }

            result = std::to_string(version.Major());
            if (fieldCount >= 2)
            {
                result += "." + std::to_string(version.Minor());
            }
            if (fieldCount >= 3)
            {
                result += "." + std::to_string(version.Build());
            }
            if (fieldCount >= 4)
            {
                result += "." + std::to_string(version.Revision());
            }
            return result;
        }
    }

    struct BuildVersion::LazyCurrent
    {
        std::once_flag Once;
        std::optional<Version> Value;
        std::exception_ptr Failure;
    };

    BuildVersion::LazyCurrent BuildVersion::_current;

    Version::Version() noexcept = default;

    Version::Version(std::int32_t major, std::int32_t minor)
        : _major(major), _minor(minor)
    {
        if (major < 0 || minor < 0)
        {
            throw std::out_of_range("Version components must be non-negative");
        }
    }

    Version::Version(std::int32_t major, std::int32_t minor,
        std::int32_t build)
        : _major(major), _minor(minor), _build(build)
    {
        if (major < 0 || minor < 0 || build < 0)
        {
            throw std::out_of_range("Version components must be non-negative");
        }
    }

    Version::Version(std::int32_t major, std::int32_t minor,
        std::int32_t build, std::int32_t revision)
        : _major(major), _minor(minor), _build(build), _revision(revision)
    {
        if (major < 0 || minor < 0 || build < 0 || revision < 0)
        {
            throw std::out_of_range("Version components must be non-negative");
        }
    }

    std::int32_t Version::Major() const noexcept
    {
        return _major;
    }

    std::int32_t Version::Minor() const noexcept
    {
        return _minor;
    }

    std::int32_t Version::Build() const noexcept
    {
        return _build;
    }

    std::int32_t Version::Revision() const noexcept
    {
        return _revision;
    }

    std::string Version::ToString() const
    {
        return ToString(_build < 0 ? 2 : (_revision < 0 ? 3 : 4));
    }

    std::string Version::ToString(std::int32_t fieldCount) const
    {
        if (fieldCount < 0 || fieldCount > 4)
        {
            throw std::invalid_argument("fieldCount");
        }
        if (fieldCount >= 3 && _build < 0)
        {
            throw std::invalid_argument("fieldCount");
        }
        if (fieldCount == 4 && _revision < 0)
        {
            throw std::invalid_argument("fieldCount");
        }
        return JoinVersion(*this, fieldCount);
    }

    std::strong_ordering operator<=>(const Version& left,
        const Version& right) noexcept
    {
        if (const auto order = left._major <=> right._major; order != 0)
        {
            return order;
        }
        if (const auto order = left._minor <=> right._minor; order != 0)
        {
            return order;
        }
        if (const auto order = left._build <=> right._build; order != 0)
        {
            return order;
        }
        return left._revision <=> right._revision;
    }

    const std::optional<Version>& BuildVersion::Current()
    {
        // Lazy<T>'s default ExecutionAndPublication mode evaluates once and also
        // caches an exception. Catch inside call_once so a throwing Read is not
        // retried on the next access.
        std::call_once(_current.Once, []
        {
            try
            {
                _current.Value = Read();
            }
            catch (...)
            {
                _current.Failure = std::current_exception();
            }
        });
        if (_current.Failure)
        {
            std::rethrow_exception(_current.Failure);
        }
        return _current.Value;
    }

    bool BuildVersion::IsRelease()
    {
        return Current().has_value();
    }

    std::string BuildVersion::Display()
    {
        const std::optional<Version>& current = Current();
        if (!current)
        {
            return "a local build";
        }
        return "v" + current->ToString(3);
    }

    std::optional<Version> BuildVersion::Read()
    {
        // System.Reflection has no C++ counterpart. These private build-adapter
        // defines encode the same selection made by Assembly.GetEntryAssembly()
        // ?? typeof(BuildVersion).Assembly. An entry-assembly version always wins.
        // The fallback is enabled only when the adapter explicitly says there is
        // no entry assembly; merely lacking its version attribute must not fall
        // through to this assembly. No state is inferred from the target OS.
        std::string_view text;
#if defined(MPHREAD_ENTRY_ASSEMBLY_INFORMATIONAL_VERSION)
        constexpr char informationalVersion[] =
            MPHREAD_ENTRY_ASSEMBLY_INFORMATIONAL_VERSION;
        text = std::string_view(informationalVersion,
            sizeof(informationalVersion) - 1);
#elif defined(MPHREAD_BUILDVERSION_NO_ENTRY_ASSEMBLY)
    #if defined(MPHREAD_BUILDVERSION_ASSEMBLY_INFORMATIONAL_VERSION)
        constexpr char informationalVersion[] =
            MPHREAD_BUILDVERSION_ASSEMBLY_INFORMATIONAL_VERSION;
        text = std::string_view(informationalVersion,
            sizeof(informationalVersion) - 1);
    #else
        return std::nullopt;
    #endif
#else
        return std::nullopt;
#endif

        const std::size_t plus = text.find('+');
        if (plus != std::string_view::npos)
        {
            text = text.substr(0, plus);
        }
        return Parse(text);
    }

    std::optional<Version> BuildVersion::Parse(
        std::optional<std::string_view> text)
    {
        if (!text)
        {
            return std::nullopt;
        }

        std::string_view value = TrimDotNetWhiteSpace(*text);
        if (value.empty())
        {
            return std::nullopt;
        }
        if (value.size() > 1 && (value.front() == 'v' || value.front() == 'V'))
        {
            value.remove_prefix(1);
        }
        if (value.find('-') != std::string_view::npos)
        {
            return std::nullopt;
        }

        const std::optional<Version> version = TryParseVersion(value);
        if (!version)
        {
            return std::nullopt;
        }
        if (version->Major() == 1 && version->Minor() == 0
            && version->Build() <= 0)
        {
            return std::nullopt;
        }
        return Normalise(*version);
    }

    Version BuildVersion::Normalise(const Version& version)
    {
        return Version(version.Major(), version.Minor(),
            version.Build() < 0 ? 0 : version.Build());
    }
}
