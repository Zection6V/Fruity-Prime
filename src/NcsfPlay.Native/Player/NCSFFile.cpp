#include "NCSFFile.hpp"

#include "../Common.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <locale>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
    using ByteSpan = std::span<const std::uint8_t>;

    [[nodiscard]] bool IsWhiteSpace(char16_t value) noexcept
    {
        return (value >= u'\u0009' && value <= u'\u000D')
            || value == u'\u0020' || value == u'\u0085' || value == u'\u00A0'
            || value == u'\u1680' || (value >= u'\u2000' && value <= u'\u200A')
            || value == u'\u2028' || value == u'\u2029' || value == u'\u202F'
            || value == u'\u205F' || value == u'\u3000';
    }

    [[nodiscard]] std::u16string_view Trim(std::u16string_view value) noexcept
    {
        std::size_t start = 0;
        while (start < value.size() && IsWhiteSpace(value[start]))
        {
            ++start;
        }
        std::size_t end = value.size();
        while (end > start && IsWhiteSpace(value[end - 1]))
        {
            --end;
        }
        return value.substr(start, end - start);
    }

    [[nodiscard]] bool EqualIgnoreCase(std::u16string_view left, std::u16string_view right) noexcept
    {
        if (left.size() != right.size())
        {
            return false;
        }
        for (std::size_t i = 0; i < left.size(); ++i)
        {
            char16_t a = left[i];
            char16_t b = right[i];
            if (a >= u'A' && a <= u'Z') a = static_cast<char16_t>(a + (u'a' - u'A'));
            if (b >= u'A' && b <= u'Z') b = static_cast<char16_t>(b + (u'a' - u'A'));
            if (a != b)
            {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool StartsWith(std::u16string_view value, std::u16string_view prefix) noexcept
    {
        return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
    }

    [[nodiscard]] std::u16string GroupSeparator(std::u16string_view decimalSeparator)
    {
        try
        {
            const wchar_t separator = std::use_facet<std::numpunct<wchar_t>>(std::locale("")).thousands_sep();
            const std::uint32_t scalar = static_cast<std::uint32_t>(separator);
            if (separator != L'\0' && scalar <= 0xFFFFU)
            {
                return std::u16string(1, static_cast<char16_t>(scalar));
            }
        }
        catch (const std::runtime_error&)
        {
        }
        return decimalSeparator == u"," ? u"." : u",";
    }

    [[nodiscard]] float OutOfRangeSingle(std::string_view canonical, bool negative)
    {
        long double wide = 0.0L;
        const char* first = canonical.data();
        const char* last = first + canonical.size();
        const auto result = std::from_chars(first, last, wide, std::chars_format::general);
        if (result.ptr == last && result.ec == std::errc{})
        {
            return static_cast<float>(wide);
        }
        if (result.ptr != last || result.ec != std::errc::result_out_of_range)
        {
            throw std::invalid_argument("Input string was not in a correct format.");
        }

        const std::size_t exponentPos = canonical.find_first_of("eE");
        std::int64_t exponent = 0;
        if (exponentPos != std::string_view::npos)
        {
            std::size_t i = exponentPos + 1;
            bool exponentNegative = false;
            if (i < canonical.size() && (canonical[i] == '+' || canonical[i] == '-'))
            {
                exponentNegative = canonical[i++] == '-';
            }
            for (; i < canonical.size(); ++i)
            {
                if (exponent < 1000000)
                {
                    exponent = exponent * 10 + canonical[i] - '0';
                }
            }
            if (exponentNegative) exponent = -exponent;
        }

        const std::size_t mantissaEnd = exponentPos == std::string_view::npos ? canonical.size() : exponentPos;
        const std::size_t signOffset = !canonical.empty() && canonical.front() == '-' ? 1U : 0U;
        const std::size_t dot = canonical.find('.', signOffset);
        const std::size_t decimalPos = dot != std::string_view::npos && dot < mantissaEnd ? dot : mantissaEnd;
        std::size_t digit = 0;
        std::size_t firstNonZero = std::string_view::npos;
        std::size_t integerDigits = 0;
        for (std::size_t i = signOffset; i < mantissaEnd; ++i)
        {
            if (canonical[i] == '.') continue;
            if (i < decimalPos) ++integerDigits;
            if (firstNonZero == std::string_view::npos && canonical[i] != '0') firstNonZero = digit;
            ++digit;
        }
        if (firstNonZero == std::string_view::npos)
        {
            return negative ? -0.0F : 0.0F;
        }
        const std::int64_t effectiveExponent = exponent
            + static_cast<std::int64_t>(integerDigits)
            - static_cast<std::int64_t>(firstNonZero) - 1;
        return effectiveExponent < 0
            ? (negative ? -0.0F : 0.0F)
            : (negative ? -std::numeric_limits<float>::infinity() : std::numeric_limits<float>::infinity());
    }

    [[nodiscard]] float ParseSingle(std::u16string_view value)
    {
        const NCSFCommon::NumberFormatInfo format = NCSFCommon::GetCurrentCultureNumberFormat();
        if (EqualIgnoreCase(value, format.NaNSymbol)) return std::numeric_limits<float>::quiet_NaN();
        if (EqualIgnoreCase(value, format.PositiveInfinitySymbol)) return std::numeric_limits<float>::infinity();
        if (EqualIgnoreCase(value, format.NegativeInfinitySymbol)) return -std::numeric_limits<float>::infinity();
        if (!format.PositiveSign.empty() && StartsWith(value, format.PositiveSign))
        {
            const std::u16string_view unsignedValue = value.substr(format.PositiveSign.size());
            if (EqualIgnoreCase(unsignedValue, format.PositiveInfinitySymbol)) return std::numeric_limits<float>::infinity();
            if (EqualIgnoreCase(unsignedValue, format.NaNSymbol)) return std::numeric_limits<float>::quiet_NaN();
        }
        if (!format.NegativeSign.empty() && StartsWith(value, format.NegativeSign)
            && EqualIgnoreCase(value.substr(format.NegativeSign.size()), format.NaNSymbol))
        {
            return std::numeric_limits<float>::quiet_NaN();
        }

        std::size_t index = 0;
        bool negative = false;
        if (!format.PositiveSign.empty() && StartsWith(value, format.PositiveSign))
        {
            index = format.PositiveSign.size();
        }
        else if (!format.NegativeSign.empty() && StartsWith(value, format.NegativeSign))
        {
            negative = true;
            index = format.NegativeSign.size();
        }

        const std::u16string groupSeparator = GroupSeparator(format.NumberDecimalSeparator);
        std::string canonical;
        canonical.reserve(value.size() + 1);
        if (negative) canonical.push_back('-');
        bool digit = false;
        bool decimal = false;
        bool exponent = false;
        bool exponentDigit = false;
        bool exponentSign = false;
        while (index < value.size())
        {
            const char16_t chr = value[index];
            if (chr >= u'0' && chr <= u'9')
            {
                canonical.push_back(static_cast<char>(chr));
                digit = true;
                if (exponent) exponentDigit = true;
                exponentSign = false;
                ++index;
            }
            else if (!exponent && !decimal && !format.NumberDecimalSeparator.empty()
                && StartsWith(value.substr(index), format.NumberDecimalSeparator))
            {
                canonical.push_back('.');
                decimal = true;
                index += format.NumberDecimalSeparator.size();
            }
            else if (!exponent && !decimal && !groupSeparator.empty()
                && groupSeparator != format.NumberDecimalSeparator
                && StartsWith(value.substr(index), groupSeparator))
            {
                index += groupSeparator.size();
            }
            else if (!exponent && digit && (chr == u'e' || chr == u'E'))
            {
                canonical.push_back('e');
                exponent = true;
                exponentSign = true;
                ++index;
            }
            else if (exponent && exponentSign && !format.PositiveSign.empty()
                && StartsWith(value.substr(index), format.PositiveSign))
            {
                canonical.push_back('+');
                index += format.PositiveSign.size();
                exponentSign = false;
            }
            else if (exponent && exponentSign && !format.NegativeSign.empty()
                && StartsWith(value.substr(index), format.NegativeSign))
            {
                canonical.push_back('-');
                index += format.NegativeSign.size();
                exponentSign = false;
            }
            else
            {
                throw std::invalid_argument("Input string was not in a correct format.");
            }
        }
        if (!digit || (exponent && !exponentDigit))
        {
            throw std::invalid_argument("Input string was not in a correct format.");
        }

        float parsed = 0.0F;
        const char* first = canonical.data();
        const char* last = first + canonical.size();
        const auto result = std::from_chars(first, last, parsed, std::chars_format::general);
        if (result.ptr != last)
        {
            throw std::invalid_argument("Input string was not in a correct format.");
        }
        if (result.ec == std::errc{}) return parsed;
        if (result.ec != std::errc::result_out_of_range)
        {
            throw std::invalid_argument("Input string was not in a correct format.");
        }
        return OutOfRangeSingle(canonical, negative);
    }

    [[nodiscard]] float SingleMin(float x, float y) noexcept
    {
        if (x != y)
        {
            return std::isnan(x) ? x : (x < y ? x : y);
        }
        return std::signbit(x) ? x : y;
    }

    [[nodiscard]] ByteSpan Slice(ByteSpan span, std::int32_t start, std::int32_t length)
    {
        if (start < 0 || length < 0)
        {
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        }
        const std::size_t offset = static_cast<std::size_t>(start);
        const std::size_t count = static_cast<std::size_t>(length);
        if (offset > span.size() || count > span.size() - offset)
        {
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        }
        return span.subspan(offset, count);
    }

    [[nodiscard]] std::uint32_t ReadUInt32LittleEndian(ByteSpan span, std::size_t offset)
    {
        if (offset > span.size() || span.size() - offset < 4)
        {
            throw std::invalid_argument("Source was too short.");
        }
        return static_cast<std::uint32_t>(span[offset])
            | (static_cast<std::uint32_t>(span[offset + 1]) << 8U)
            | (static_cast<std::uint32_t>(span[offset + 2]) << 16U)
            | (static_cast<std::uint32_t>(span[offset + 3]) << 24U);
    }

    [[nodiscard]] constexpr std::int32_t UInt32ToInt32Unchecked(std::uint32_t value) noexcept
    {
        return std::bit_cast<std::int32_t>(value);
    }

    void SetCount(std::vector<std::uint8_t>& list, std::int32_t count)
    {
        if (count < 0) throw std::out_of_range("count");
        list.resize(static_cast<std::size_t>(count));
    }

    [[nodiscard]] std::vector<std::uint8_t> ReadAllBytes(std::u16string_view path)
    {
        if (path.find(u'\0') != std::u16string_view::npos)
        {
            throw std::invalid_argument("Null character in path.");
        }
        std::ifstream stream(std::filesystem::path(path), std::ios::binary | std::ios::ate);
        if (!stream.is_open()) throw std::ios_base::failure("Could not open file.");
        const std::streampos end = stream.tellg();
        if (end < std::streampos(0)) throw std::ios_base::failure("Could not determine file length.");
        const auto length = static_cast<std::uintmax_t>(end);
        if (length > static_cast<std::uintmax_t>(std::numeric_limits<std::int32_t>::max()))
        {
            throw std::length_error("File is too large.");
        }
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
        stream.seekg(0, std::ios::beg);
        if (!bytes.empty())
        {
            stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            if (stream.gcount() != static_cast<std::streamsize>(bytes.size()))
            {
                throw std::ios_base::failure("Could not read file.");
            }
        }
        return bytes;
    }
}

namespace NCSF123
{
    const std::u16string& NCSFFile::FilePath() const noexcept
    {
        return _filePath;
    }

    std::span<const std::uint8_t> NCSFFile::ProgramSection() const noexcept
    {
        return _programSection;
    }

    std::span<const std::uint8_t> NCSFFile::ReservedSection() const noexcept
    {
        return _reservedSection;
    }

    NCSFCommon::TagList& NCSFFile::Tags() noexcept
    {
        return _tags;
    }

    const NCSFCommon::TagList& NCSFFile::Tags() const noexcept
    {
        return _tags;
    }

    std::int32_t NCSFFile::GetFadeMS(std::int32_t defaultFade) const
    {
        std::int32_t fade = defaultFade;
        if (_tags.Contains(u"fade"))
        {
            fade = NCSFCommon::Common::StringToMS(_tags[u"fade"].Value);
        }
        return fade;
    }

    std::int32_t NCSFFile::GetLengthMS(std::int32_t defaultLength) const
    {
        std::int32_t length = 0;
        if (_tags.Contains(u"length"))
        {
            length = NCSFCommon::Common::StringToMS(_tags[u"length"].Value);
        }
        if (length == 0) length = defaultLength;
        return length;
    }

    float NCSFFile::GetVolume(VolumeType preferredVolumeType, PeakType preferredPeakType) const
    {
        if (preferredVolumeType == VolumeType::None) return 1.0F;

        std::u16string albumGainStorage;
        std::u16string albumPeakStorage;
        std::u16string trackGainStorage;
        std::u16string trackPeakStorage;
        std::u16string volumeStorage;
        std::u16string_view albumGain;
        std::u16string_view albumPeak;
        std::u16string_view trackGain;
        std::u16string_view trackPeak;
        std::u16string_view volume;
        if (_tags.Contains(u"replaygain_album_gain"))
        {
            albumGainStorage = _tags[u"replaygain_album_gain"].Value;
            albumGain = Trim(albumGainStorage);
        }
        if (_tags.Contains(u"replaygain_album_peak"))
        {
            albumPeakStorage = _tags[u"replaygain_album_peak"].Value;
            albumPeak = Trim(albumPeakStorage);
        }
        if (_tags.Contains(u"replaygain_track_gain"))
        {
            trackGainStorage = _tags[u"replaygain_track_gain"].Value;
            trackGain = Trim(trackGainStorage);
        }
        if (_tags.Contains(u"replaygain_track_peak"))
        {
            trackPeakStorage = _tags[u"replaygain_track_peak"].Value;
            trackPeak = Trim(trackPeakStorage);
        }
        if (_tags.Contains(u"volume"))
        {
            volumeStorage = _tags[u"volume"].Value;
            volume = Trim(volumeStorage);
        }

        float gain = 0.0F;
        bool hadReplayGain = false;
        if (preferredVolumeType == VolumeType::ReplayGainAlbum && !albumGain.empty())
        {
            const std::size_t space = albumGain.find(u' ');
            if (space != std::u16string_view::npos) albumGain = albumGain.substr(0, space);
            gain = ParseSingle(albumGain);
            hadReplayGain = true;
        }
        if (!hadReplayGain && preferredVolumeType != VolumeType::Volume && !trackGain.empty())
        {
            const std::size_t space = trackGain.find(u' ');
            if (space != std::u16string_view::npos) trackGain = trackGain.substr(0, space);
            gain = ParseSingle(trackGain);
            hadReplayGain = true;
        }
        if (hadReplayGain)
        {
            const float vol = std::pow(10.0F, gain / 20.0F);
            float peak = 1.0F;
            if (preferredPeakType == PeakType::ReplayGainAlbum && !albumPeak.empty())
            {
                peak = ParseSingle(albumPeak);
            }
            else if (preferredPeakType != PeakType::None && !trackPeak.empty())
            {
                peak = ParseSingle(trackPeak);
            }
            return peak != 1.0F ? SingleMin(vol, 1.0F / peak) : vol;
        }
        return volume.empty() ? 1.0F : ParseSingle(volume);
    }

    void NCSFFile::ReadNCSF(std::u16string_view path, bool readTagsOnly)
    {
        const std::vector<std::uint8_t> storage = ReadAllBytes(path);
        const ByteSpan fileBytes(storage);
        NCSFCommon::NCSF::CheckForValidPSF(fileBytes, static_cast<std::uint8_t>(VersionByte));
        const std::uint32_t reservedSize = ReadUInt32LittleEndian(fileBytes, 0x04);
        const std::uint32_t programCompressedSize = ReadUInt32LittleEndian(fileBytes, 0x08);

        SetCount(_rawData, static_cast<std::int32_t>(fileBytes.size()));
        std::copy(fileBytes.begin(), fileBytes.end(), _rawData.begin());
        if (!readTagsOnly)
        {
            if (reservedSize != 0U)
            {
                const std::int32_t count = UInt32ToInt32Unchecked(reservedSize);
                SetCount(_reservedSection, count);
                const ByteSpan reserved = Slice(fileBytes, 0x10, count);
                std::copy(reserved.begin(), reserved.end(), _reservedSection.begin());
            }
            if (programCompressedSize != 0U)
            {
                const std::vector<std::uint8_t> program = NCSFCommon::NCSF::GetProgramSectionFromPSF(
                    fileBytes,
                    static_cast<std::uint8_t>(VersionByte),
                    static_cast<std::uint32_t>(ProgramHeaderSize),
                    static_cast<std::uint32_t>(ProgramSizeOffset));
                if (program.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
                {
                    throw std::length_error("Managed span length exceeded Int32.MaxValue.");
                }
                SetCount(_programSection, static_cast<std::int32_t>(program.size()));
                std::copy(program.begin(), program.end(), _programSection.begin());
            }
        }
        _tags = NCSFCommon::NCSF::GetTagsFromPSF(fileBytes, static_cast<std::uint8_t>(VersionByte));
    }

    NCSFFile::NCSFFile(std::u16string path)
        : _filePath(std::move(path))
    {
        ReadNCSF(_filePath);
    }
}
