#include "Common.hpp"

#include <charconv>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <regex>
#include <string>
#include <system_error>
#include <unordered_map>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <cuchar>
#include <langinfo.h>
#include <locale.h>
#endif

namespace
{
	[[nodiscard]] constexpr bool IsParseWhiteSpace(char16_t value) noexcept
	{
		return (value >= u'\u0009' && value <= u'\u000D') || value == u'\u0020';
	}

	[[nodiscard]] constexpr bool AllowsAsciiHyphen(char16_t value) noexcept
	{
		switch (value)
		{
		case u'\u2012':
		case u'\u207B':
		case u'\u208B':
		case u'\u2212':
		case u'\u2796':
		case u'\uFE63':
		case u'\uFF0D':
			return true;
		default:
			return false;
		}
	}

	[[nodiscard]] std::u16string ToU16(std::string_view value)
	{
		std::u16string result;
		result.reserve(value.size());
		for (char chr : value)
		{
			result.push_back(static_cast<char16_t>(static_cast<unsigned char>(chr)));
		}
		return result;
	}

#if !defined(_WIN32)
	[[nodiscard]] std::u16string LocaleBytesToU16(const char* value)
	{
		if (value == nullptr || *value == '\0')
		{
			return {};
		}

		std::u16string result;
		std::mbstate_t state{};
		const char* current = value;
		std::size_t remaining = std::strlen(value);
		while (remaining > 0)
		{
			char32_t scalar = U'\0';
			const std::size_t length = std::mbrtoc32(&scalar, current, remaining, &state);
			if (length == static_cast<std::size_t>(-1)
				|| length == static_cast<std::size_t>(-2)
				|| length == static_cast<std::size_t>(-3))
			{
				return ToU16(value);
			}
			if (length == 0)
			{
				break;
			}
			if (scalar <= 0xFFFFU)
			{
				result.push_back(static_cast<char16_t>(scalar));
			}
			else if (scalar <= 0x10FFFFU)
			{
				scalar -= 0x10000U;
				result.push_back(static_cast<char16_t>(0xD800U + (scalar >> 10U)));
				result.push_back(static_cast<char16_t>(0xDC00U + (scalar & 0x3FFU)));
			}
			current += length;
			remaining -= length;
		}
		return result;
	}
#endif

#if defined(_WIN32)
	[[nodiscard]] std::u16string WideToU16(std::wstring_view value)
	{
		std::u16string result;
		result.reserve(value.size());
		for (wchar_t chr : value)
		{
			result.push_back(static_cast<char16_t>(chr));
		}
		return result;
	}

	[[nodiscard]] std::u16string GetWindowsThreadLocaleInfo(LCTYPE type)
	{
		wchar_t localeName[LOCALE_NAME_MAX_LENGTH]{};
		const LCID localeId = GetThreadLocale();
		if (LCIDToLocaleName(localeId, localeName, LOCALE_NAME_MAX_LENGTH, 0) == 0)
		{
			return {};
		}

		const int required = GetLocaleInfoEx(localeName, type, nullptr, 0);
		if (required <= 1)
		{
			return {};
		}
		std::wstring value(static_cast<std::size_t>(required), L'\0');
		if (GetLocaleInfoEx(localeName, type, value.data(), required) == 0)
		{
			return {};
		}
		value.resize(static_cast<std::size_t>(required - 1));
		return WideToU16(value);
	}
#endif

	[[nodiscard]] NCSFCommon::NumberFormatInfo LoadExecutionThreadNumberFormat()
	{
		NCSFCommon::NumberFormatInfo result{
			u"+",
			u"-",
			u".",
			u"NaN",
			u"Infinity",
			u"-Infinity"
		};

#if defined(_WIN32)
		if (std::u16string value = GetWindowsThreadLocaleInfo(LOCALE_SPOSITIVESIGN); !value.empty())
		{
			result.PositiveSign = std::move(value);
		}
		if (std::u16string value = GetWindowsThreadLocaleInfo(LOCALE_SNEGATIVESIGN); !value.empty())
		{
			result.NegativeSign = std::move(value);
		}
		if (std::u16string value = GetWindowsThreadLocaleInfo(LOCALE_SDECIMAL); !value.empty())
		{
			result.NumberDecimalSeparator = std::move(value);
		}
#if defined(LOCALE_SNAN)
		if (std::u16string value = GetWindowsThreadLocaleInfo(LOCALE_SNAN); !value.empty())
		{
			result.NaNSymbol = std::move(value);
		}
#endif
#if defined(LOCALE_SPOSINFINITY)
		if (std::u16string value = GetWindowsThreadLocaleInfo(LOCALE_SPOSINFINITY); !value.empty())
		{
			result.PositiveInfinitySymbol = std::move(value);
		}
#endif
#if defined(LOCALE_SNEGINFINITY)
		if (std::u16string value = GetWindowsThreadLocaleInfo(LOCALE_SNEGINFINITY); !value.empty())
		{
			result.NegativeInfinitySymbol = std::move(value);
		}
#endif
#else
		const locale_t locale = uselocale(static_cast<locale_t>(0));
		if (locale != static_cast<locale_t>(0) && locale != LC_GLOBAL_LOCALE)
		{
			if (std::u16string value = LocaleBytesToU16(nl_langinfo_l(RADIXCHAR, locale)); !value.empty())
			{
				result.NumberDecimalSeparator = std::move(value);
			}
#if defined(POSITIVE_SIGN)
			if (std::u16string value = LocaleBytesToU16(nl_langinfo_l(POSITIVE_SIGN, locale)); !value.empty())
			{
				result.PositiveSign = std::move(value);
			}
#endif
#if defined(NEGATIVE_SIGN)
			if (std::u16string value = LocaleBytesToU16(nl_langinfo_l(NEGATIVE_SIGN, locale)); !value.empty())
			{
				result.NegativeSign = std::move(value);
			}
#endif
		}
		else if (locale == LC_GLOBAL_LOCALE)
		{
			if (const lconv* info = localeconv(); info != nullptr)
			{
				if (std::u16string value = LocaleBytesToU16(info->decimal_point); !value.empty())
				{
					result.NumberDecimalSeparator = std::move(value);
				}
				if (std::u16string value = LocaleBytesToU16(info->positive_sign); !value.empty())
				{
					result.PositiveSign = std::move(value);
				}
				if (std::u16string value = LocaleBytesToU16(info->negative_sign); !value.empty())
				{
					result.NegativeSign = std::move(value);
				}
			}
		}
#endif
		return result;
	}

	void DebugAssert(bool condition, std::string_view message)
	{
#ifndef NDEBUG
		if (!condition)
		{
			std::clog << "Debug.Assert failed: " << message << '\n';
		}
#else
		static_cast<void>(condition);
		static_cast<void>(message);
#endif
	}

	[[nodiscard]] std::vector<std::u16string_view> SplitPreserveEmpty(
		std::u16string_view value, char16_t separator)
	{
		std::vector<std::u16string_view> parts;
		std::size_t start = 0;
		while (true)
		{
			const std::size_t pos = value.find(separator, start);
			if (pos == std::u16string_view::npos)
			{
				parts.emplace_back(value.substr(start));
				break;
			}
			parts.emplace_back(value.substr(start, pos - start));
			start = pos + 1;
		}
		return parts;
	}

	[[nodiscard]] bool StartsWith(std::u16string_view value, std::u16string_view prefix) noexcept
	{
		return prefix.size() <= value.size() && value.substr(0, prefix.size()) == prefix;
	}

	[[nodiscard]] std::int32_t ParseInt32(std::u16string_view value)
	{
		if (value.empty())
		{
			throw std::invalid_argument("Input string was not in a correct format.");
		}

		const NCSFCommon::NumberFormatInfo format = NCSFCommon::GetCurrentCultureNumberFormat();
		std::size_t index = 0;
		while (index < value.size() && IsParseWhiteSpace(value[index]))
		{
			++index;
		}
		if (index >= value.size())
		{
			throw std::invalid_argument("Input string was not in a correct format.");
		}

		bool negative = false;
		const bool invariantSigns = format.PositiveSign == u"+" && format.NegativeSign == u"-";
		if (invariantSigns)
		{
			if (value[index] == u'-')
			{
				negative = true;
				++index;
			}
			else if (value[index] == u'+')
			{
				++index;
			}
		}
		else if (format.NegativeSign.size() == 1
			&& AllowsAsciiHyphen(format.NegativeSign.front())
			&& value[index] == u'-')
		{
			negative = true;
			++index;
		}
		else
		{
			const std::u16string_view remaining = value.substr(index);
			if (!format.PositiveSign.empty() && StartsWith(remaining, format.PositiveSign))
			{
				index += format.PositiveSign.size();
			}
			else if (!format.NegativeSign.empty() && StartsWith(remaining, format.NegativeSign))
			{
				negative = true;
				index += format.NegativeSign.size();
			}
		}

		if (index >= value.size())
		{
			throw std::invalid_argument("Input string was not in a correct format.");
		}

		bool sawDigit = false;
		bool overflow = false;
		std::uint64_t magnitude = 0;
		const std::uint64_t limit = negative ? 2147483648ULL : 2147483647ULL;
		while (index < value.size() && value[index] >= u'0' && value[index] <= u'9')
		{
			sawDigit = true;
			const std::uint64_t digit = static_cast<std::uint64_t>(value[index] - u'0');
			if (!overflow)
			{
				if (magnitude > (limit - digit) / 10ULL)
				{
					overflow = true;
				}
				else
				{
					magnitude = magnitude * 10ULL + digit;
				}
			}
			++index;
		}

		if (!sawDigit)
		{
			throw std::invalid_argument("Input string was not in a correct format.");
		}

		if (index < value.size() && IsParseWhiteSpace(value[index]))
		{
			do
			{
				++index;
			} while (index < value.size() && IsParseWhiteSpace(value[index]));
		}

		while (index < value.size() && value[index] == u'\0')
		{
			++index;
		}

		if (index != value.size())
		{
			throw std::invalid_argument("Input string was not in a correct format.");
		}
		if (overflow)
		{
			throw std::out_of_range("Value was either too large or too small for an Int32.");
		}

		if (negative)
		{
			if (magnitude == 2147483648ULL)
			{
				return std::numeric_limits<std::int32_t>::min();
			}
			return -static_cast<std::int32_t>(magnitude);
		}
		return static_cast<std::int32_t>(magnitude);
	}

	[[nodiscard]] std::int32_t AddUnchecked(std::int32_t left, std::int32_t right) noexcept
	{
		const std::uint32_t result = static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right);
		return std::bit_cast<std::int32_t>(result);
	}

	[[nodiscard]] std::int32_t MultiplyUnchecked(std::int32_t left, std::int32_t right) noexcept
	{
		const std::uint32_t result = static_cast<std::uint32_t>(left) * static_cast<std::uint32_t>(right);
		return std::bit_cast<std::int32_t>(result);
	}

	[[nodiscard]] std::int32_t FloatToInt32Unchecked(float value) noexcept
	{
		if (std::isnan(value))
		{
			return 0;
		}
		if (value >= 2147483648.0F)
		{
			return std::numeric_limits<std::int32_t>::max();
		}
		if (value < static_cast<float>(std::numeric_limits<std::int32_t>::min()))
		{
			return std::numeric_limits<std::int32_t>::min();
		}
		return static_cast<std::int32_t>(value);
	}

	[[nodiscard]] std::u16string FormatIntD2(
		std::int32_t value, const NCSFCommon::NumberFormatInfo& format)
	{
		const bool negative = value < 0;
		const std::uint32_t magnitude = negative
			? static_cast<std::uint32_t>(-(static_cast<std::int64_t>(value)))
			: static_cast<std::uint32_t>(value);

		std::string digits = std::to_string(magnitude);
		if (digits.size() < 2)
		{
			digits.insert(digits.begin(), 2 - digits.size(), '0');
		}

		std::u16string result;
		if (negative)
		{
			result += format.NegativeSign;
		}
		result += ToU16(digits);
		return result;
	}

	[[nodiscard]] std::u16string FormatSeconds(
		float value, const NCSFCommon::NumberFormatInfo& format)
	{
		if (std::isnan(value))
		{
			return format.NaNSymbol;
		}
		if (std::isinf(value))
		{
			return std::signbit(value)
				? format.NegativeInfinitySymbol
				: format.PositiveInfinitySymbol;
		}

		const bool negative = std::signbit(value);
		const float magnitude = std::fabs(value);
		std::array<char, 128> buffer{};
		const auto [end, error] = std::to_chars(
			buffer.data(), buffer.data() + buffer.size(), magnitude, std::chars_format::fixed, 4);
		if (error != std::errc{})
		{
			throw std::runtime_error("Failed to format a Single value.");
		}

		std::string text(buffer.data(), end);
		const std::size_t decimal = text.find('.');
		if (decimal != std::string::npos)
		{
			while (!text.empty() && text.back() == '0')
			{
				text.pop_back();
			}
			if (!text.empty() && text.back() == '.')
			{
				text.pop_back();
			}
		}

		const std::size_t integerEnd = text.find('.');
		const std::size_t integerDigits = integerEnd == std::string::npos ? text.size() : integerEnd;
		if (integerDigits < 2)
		{
			text.insert(text.begin(), 2 - integerDigits, '0');
		}

		std::u16string result;
		if (negative)
		{
			result += format.NegativeSign;
		}
		for (char chr : text)
		{
			if (chr == '.')
			{
				result += format.NumberDecimalSeparator;
			}
			else
			{
				result.push_back(static_cast<char16_t>(static_cast<unsigned char>(chr)));
			}
		}
		return result;
	}

	[[nodiscard]] std::u16string ReplaceAll(
		std::u16string value, std::u16string_view oldValue, std::u16string_view newValue)
	{
		std::size_t index = 0;
		while ((index = value.find(oldValue, index)) != std::u16string::npos)
		{
			value.replace(index, oldValue.size(), newValue);
			index += newValue.size();
		}
		return value;
	}

	[[nodiscard]] std::u16string KeepTypeName(NCSFCommon::Common::KeepType keep)
	{
		switch (keep)
		{
		case NCSFCommon::Common::KeepType::Exclude:
			return u"Exclude";
		case NCSFCommon::Common::KeepType::Include:
			return u"Include";
		case NCSFCommon::Common::KeepType::Neither:
			return u"Neither";
		}
		return ToU16(std::to_string(static_cast<std::uint8_t>(keep)));
	}

	[[nodiscard]] std::int32_t RecordHashCombine(std::int32_t hash, std::int32_t value) noexcept
	{
		const std::uint32_t result = static_cast<std::uint32_t>(hash) * 2773833001U
			+ static_cast<std::uint32_t>(value);
		return std::bit_cast<std::int32_t>(result);
	}

	[[nodiscard]] std::uint64_t CreateRecordHashSeed() noexcept
	{
		try
		{
			std::random_device source;
			std::uint64_t seed = static_cast<std::uint64_t>(source()) << 32U;
			seed ^= static_cast<std::uint64_t>(source());
			return seed;
		}
		catch (...)
		{
			const auto ticks = std::chrono::high_resolution_clock::now().time_since_epoch().count();
			const std::uint64_t address = static_cast<std::uint64_t>(
				reinterpret_cast<std::uintptr_t>(&CreateRecordHashSeed));
			return static_cast<std::uint64_t>(ticks) ^ (address + 0x9E3779B97F4A7C15ULL);
		}
	}

	[[nodiscard]] std::uint64_t RecordHashSeed() noexcept
	{
		static const std::uint64_t seed = CreateRecordHashSeed();
		return seed;
	}

	[[nodiscard]] constexpr std::uint32_t RotateLeft32(
		std::uint32_t value, unsigned shift) noexcept
	{
		return (value << shift) | (value >> (32U - shift));
	}

	void MarvinBlock(std::uint32_t& p0, std::uint32_t& p1) noexcept
	{
		p1 ^= p0;
		p0 = RotateLeft32(p0, 20U);
		p0 += p1;
		p1 = RotateLeft32(p1, 9U);
		p1 ^= p0;
		p0 = RotateLeft32(p0, 27U);
		p0 += p1;
		p1 = RotateLeft32(p1, 19U);
	}

	[[nodiscard]] std::uint8_t Utf16ByteAt(std::u16string_view value, std::size_t index) noexcept
	{
		const std::uint16_t unit = static_cast<std::uint16_t>(value[index / 2]);
		return static_cast<std::uint8_t>((unit >> ((index & 1U) * 8U)) & 0xFFU);
	}

	[[nodiscard]] std::uint16_t ReadUtf16UInt16(
		std::u16string_view value, std::size_t byteOffset) noexcept
	{
		return static_cast<std::uint16_t>(Utf16ByteAt(value, byteOffset))
			| (static_cast<std::uint16_t>(Utf16ByteAt(value, byteOffset + 1)) << 8U);
	}

	[[nodiscard]] std::uint32_t ReadUtf16UInt32(
		std::u16string_view value, std::size_t byteOffset) noexcept
	{
		return static_cast<std::uint32_t>(Utf16ByteAt(value, byteOffset))
			| (static_cast<std::uint32_t>(Utf16ByteAt(value, byteOffset + 1)) << 8U)
			| (static_cast<std::uint32_t>(Utf16ByteAt(value, byteOffset + 2)) << 16U)
			| (static_cast<std::uint32_t>(Utf16ByteAt(value, byteOffset + 3)) << 24U);
	}

	[[nodiscard]] std::int32_t MarvinStringHash(std::u16string_view value) noexcept
	{
		const std::uint64_t seed = RecordHashSeed();
		std::uint32_t p0 = static_cast<std::uint32_t>(seed);
		std::uint32_t p1 = static_cast<std::uint32_t>(seed >> 32U);
		const std::uint32_t count = static_cast<std::uint32_t>(value.size() * 2U);
		std::size_t dataOffset = 0;
		std::uint32_t partialResult = 0;

		if (count >= 8U)
		{
			std::uint32_t loopCount = count / 8U;
			do
			{
				p0 += ReadUtf16UInt32(value, dataOffset);
				const std::uint32_t next = ReadUtf16UInt32(value, dataOffset + 4U);
				MarvinBlock(p0, p1);
				p0 += next;
				MarvinBlock(p0, p1);
				dataOffset += 8U;
			} while (--loopCount > 0U);

			if ((count & 4U) != 0U)
			{
				p0 += ReadUtf16UInt32(value, dataOffset);
				MarvinBlock(p0, p1);
			}

			const std::size_t readOffset = dataOffset
				+ static_cast<std::size_t>(count & 7U) - 4U;
			partialResult = ReadUtf16UInt32(value, readOffset);
			const std::uint32_t shift = static_cast<std::uint32_t>((~count << 3U) & 0x1FU);
			partialResult >>= 8U;
			partialResult |= 0x80000000U;
			partialResult >>= shift;
		}
		else if (count >= 4U)
		{
			p0 += ReadUtf16UInt32(value, 0);
			MarvinBlock(p0, p1);

			const std::size_t readOffset = static_cast<std::size_t>(count & 7U) - 4U;
			partialResult = ReadUtf16UInt32(value, readOffset);
			const std::uint32_t shift = static_cast<std::uint32_t>((~count << 3U) & 0x1FU);
			partialResult >>= 8U;
			partialResult |= 0x80000000U;
			partialResult >>= shift;
		}
		else
		{
			partialResult = 0x80U;
			if ((count & 1U) != 0U)
			{
				partialResult = Utf16ByteAt(value, static_cast<std::size_t>(count & 2U));
				partialResult |= 0x8000U;
			}
			if ((count & 2U) != 0U)
			{
				partialResult <<= 16U;
				partialResult |= ReadUtf16UInt16(value, 0);
			}
		}

		p0 += partialResult;
		MarvinBlock(p0, p1);
		MarvinBlock(p0, p1);
		return std::bit_cast<std::int32_t>(p1 ^ p0);
	}

	[[nodiscard]] std::int32_t TypeIdentityHash(const void* identity) noexcept
	{
		std::uint64_t value = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(identity));
		value ^= RecordHashSeed();
		value ^= value >> 30U;
		value *= 0xBF58476D1CE4E5B9ULL;
		value ^= value >> 27U;
		value *= 0x94D049BB133111EBULL;
		value ^= value >> 31U;
		return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
	}

	[[nodiscard]] std::wstring Utf16CodeUnitsToWide(std::u16string_view value)
	{
		std::wstring result;
		result.reserve(value.size());
		for (char16_t unit : value)
		{
			result.push_back(static_cast<wchar_t>(static_cast<std::uint16_t>(unit)));
		}
		return result;
	}
}

namespace NCSFCommon
{
	NumberFormatInfo GetCurrentCultureNumberFormat()
	{
		return LoadExecutionThreadNumberFormat();
	}

	void Common::ThrowNotSupported()
	{
		throw std::logic_error("Specified method is not supported.");
	}

	class Regex::Impl final
	{
	private:
		std::u16string _pattern;
		std::wregex _regex;

	public:
		explicit Impl(std::u16string pattern)
			: _pattern(std::move(pattern)),
			  _regex(Utf16CodeUnitsToWide(_pattern),
				  std::regex_constants::ECMAScript | std::regex_constants::optimize)
		{
		}

		[[nodiscard]] bool IsMatch(std::u16string_view input) const
		{
			const std::wstring converted = Utf16CodeUnitsToWide(input);
			return std::regex_search(converted, _regex);
		}

		[[nodiscard]] const std::u16string& Pattern() const noexcept
		{
			return _pattern;
		}
	};

	Regex::Regex(std::u16string pattern)
		: _impl(std::make_shared<Impl>(std::move(pattern)))
	{
	}

	bool Regex::IsMatch(std::u16string_view input) const
	{
		return _impl->IsMatch(input);
	}

	const std::u16string& Regex::ToString() const noexcept
	{
		return _impl->Pattern();
	}

	namespace
	{
		struct DataArray final
		{
			std::array<std::uint8_t, 4> Bytes = {
				static_cast<std::uint8_t>('D'),
				static_cast<std::uint8_t>('A'),
				static_cast<std::uint8_t>('T'),
				static_cast<std::uint8_t>('A')
			};
		};

		[[nodiscard]] std::shared_ptr<DataArray> DataBackingStore()
		{
			static const std::shared_ptr<DataArray> data = std::make_shared<DataArray>();
			return data;
		}
	}

	const ReadOnlyMemory<std::uint8_t> Common::DataBytes = []
	{
		std::shared_ptr<DataArray> data = DataBackingStore();
		return ReadOnlyMemory<std::uint8_t>(data, data->Bytes.data(), data->Bytes.size());
	}();

	std::u16string Common::ReadNullTerminatedString(std::span<const std::uint8_t> span)
	{
		std::u16string chars;
		std::size_t pos = 0;
		char16_t chr;
		do
		{
			if (pos >= span.size())
			{
				throw std::out_of_range("Index was outside the bounds of the array.");
			}
			chr = static_cast<char16_t>(span[pos++]);
			if (chr != u'\0')
			{
				chars.push_back(chr);
			}
		} while (chr != u'\0');
		return chars;
	}

	void Common::WriteNullTerminatedString(std::span<std::uint8_t> span, std::u16string_view str)
	{
		std::size_t pos = 0;
		for (char16_t chr : str)
		{
			if (pos >= span.size())
			{
				throw std::out_of_range("Index was outside the bounds of the array.");
			}
			span[pos++] = static_cast<std::uint8_t>(chr);
		}
		if (pos >= span.size())
		{
			throw std::out_of_range("Index was outside the bounds of the array.");
		}
		span[pos] = 0;
	}

	bool Common::VerifyHeader(
		std::span<const std::uint8_t> actual, std::span<const std::uint8_t> expected)
	{
		return actual.size() == expected.size()
			&& std::equal(actual.begin(), actual.end(), expected.begin());
	}

	Regex Common::WildcardStringToRegex(std::u16string_view wildcard)
	{
		std::u16string pattern(wildcard);
		pattern = ReplaceAll(std::move(pattern), u"?", u".");
		pattern = ReplaceAll(std::move(pattern), u"*", u".*");
		pattern.insert(pattern.begin(), u'^');
		pattern.push_back(u'$');
		return Regex(std::move(pattern));
	}

	Common::KeepInfo::KeepInfo(std::u16string filename, KeepType keep)
		: _filename(std::move(filename)),
		  _keep(keep),
		  Filename(_filename),
		  Keep(_keep)
	{
	}

	Common::KeepInfo::KeepInfo(std::nullptr_t, KeepType keep)
		: _filename(std::nullopt),
		  _keep(keep),
		  Filename(_filename),
		  Keep(_keep)
	{
	}

	Common::KeepInfo::KeepInfo(const KeepInfo& other)
		: _filename(other._filename),
		  _keep(other._keep),
		  Filename(_filename),
		  Keep(_keep)
	{
	}

	Common::KeepInfo::KeepInfo(KeepInfo&& other) noexcept
		: _filename(other._filename),
		  _keep(other._keep),
		  Filename(_filename),
		  Keep(_keep)
	{
	}

	const void* Common::KeepInfo::EqualityContract() const noexcept
	{
		static const int identity = 0;
		return std::addressof(identity);
	}

	std::int32_t Common::KeepInfo::EqualityContractHashCode() const noexcept
	{
		return TypeIdentityHash(EqualityContract());
	}

	std::u16string_view Common::KeepInfo::RecordTypeName() const noexcept
	{
		return u"KeepInfo";
	}

	bool Common::KeepInfo::Equals(const KeepInfo* other) const noexcept
	{
		return other != nullptr
			&& EqualityContract() == other->EqualityContract()
			&& Filename == other->Filename
			&& Keep == other->Keep;
	}

	bool Common::KeepInfo::operator==(const KeepInfo& other) const noexcept
	{
		return Equals(std::addressof(other));
	}

	bool Common::KeepInfo::operator!=(const KeepInfo& other) const noexcept
	{
		return !Equals(std::addressof(other));
	}

	std::int32_t Common::KeepInfo::GetHashCode() const noexcept
	{
		std::int32_t hash = EqualityContractHashCode();
		hash = RecordHashCombine(hash, Filename ? MarvinStringHash(*Filename) : 0);
		hash = RecordHashCombine(hash, static_cast<std::int32_t>(static_cast<std::uint8_t>(Keep)));
		return hash;
	}

	bool Common::KeepInfo::PrintMembers(std::u16string& result) const
	{
		result += u"Filename = ";
		if (Filename)
		{
			result += *Filename;
		}
		result += u", Keep = ";
		result += KeepTypeName(Keep);
		return true;
	}

	std::u16string Common::KeepInfo::ToString() const
	{
		std::u16string result(RecordTypeName());
		result += u" { ";
		if (PrintMembers(result))
		{
			result.push_back(u' ');
		}
		result.push_back(u'}');
		return result;
	}

	void Common::KeepInfo::Deconstruct(
		std::optional<std::u16string>& filename, KeepType& keep) const
	{
		filename = Filename;
		keep = Keep;
	}

	std::shared_ptr<Common::KeepInfo> Common::KeepInfo::Clone() const
	{
		return std::shared_ptr<KeepInfo>(new KeepInfo(*this));
	}

	std::shared_ptr<Common::KeepInfo> Common::KeepInfo::With(
		std::optional<std::optional<std::u16string>> filename,
		std::optional<KeepType> keep) const
	{
		std::shared_ptr<KeepInfo> clone = Clone();
		if (filename)
		{
			clone->_filename = *filename;
		}
		if (keep)
		{
			clone->_keep = *keep;
		}
		return clone;
	}

	Common::KeepType Common::IncludeFilename(
		std::u16string_view filename,
		std::u16string_view sdatNumber,
		const std::vector<std::shared_ptr<KeepInfo>>& includesAndExcludes)
	{
		KeepType keep = KeepType::Neither;
		for (const std::shared_ptr<KeepInfo>& info : includesAndExcludes)
		{
			if (!info || !info->Filename)
			{
				throw std::runtime_error("Object reference not set to an instance of an object.");
			}

			const std::vector<std::u16string_view> parts = SplitPreserveEmpty(*info->Filename, u'/');
			DebugAssert(parts.size() <= 2, "parts.Length <= 2");
			if (parts.size() == 2)
			{
				if (WildcardStringToRegex(parts[0]).IsMatch(sdatNumber)
					&& WildcardStringToRegex(parts[1]).IsMatch(filename))
				{
					keep = info->Keep;
				}
			}
			else if (WildcardStringToRegex(*info->Filename).IsMatch(filename))
			{
				keep = info->Keep;
			}
		}
		return keep;
	}

	std::u16string Common::SecondsToString(float seconds)
	{
		const std::int32_t minutes = FloatToInt32Unchecked(seconds / 60.0F);
		seconds -= static_cast<float>(MultiplyUnchecked(minutes, 60));

		const NumberFormatInfo format = GetCurrentCultureNumberFormat();
		std::u16string result = FormatIntD2(minutes, format);
		result.push_back(u':');
		result += FormatSeconds(seconds, format);
		return result;
	}

	std::int32_t Common::StringToMS(std::u16string_view time)
	{
		std::int32_t colons = 0;
		for (char16_t chr : time)
		{
			if (chr == u':')
			{
				++colons;
			}
		}
		DebugAssert(colons <= 2, "colons <= 2");

		std::int32_t seconds;
		if (colons == 1)
		{
			const std::vector<std::u16string_view> ranges = SplitPreserveEmpty(time, u':');
			const std::int32_t first = ParseInt32(ranges[0]);
			const std::int32_t firstSeconds = MultiplyUnchecked(first, 60);
			const std::int32_t second = ParseInt32(ranges[1]);
			seconds = AddUnchecked(firstSeconds, second);
		}
		else if (colons == 2)
		{
			const std::vector<std::u16string_view> ranges = SplitPreserveEmpty(time, u':');
			const std::int32_t first = ParseInt32(ranges[0]);
			const std::int32_t firstSeconds = MultiplyUnchecked(first, 3600);
			const std::int32_t second = ParseInt32(ranges[1]);
			const std::int32_t secondSeconds = MultiplyUnchecked(second, 60);
			const std::int32_t firstTwo = AddUnchecked(firstSeconds, secondSeconds);
			const std::int32_t third = ParseInt32(ranges[2]);
			seconds = AddUnchecked(firstTwo, third);
		}
		else
		{
			seconds = ParseInt32(time);
		}
		return MultiplyUnchecked(seconds, 1000);
	}

	std::int32_t Common::VLVLength(std::int32_t value) noexcept
	{
		if (value >= 0x10000000)
		{
			return 5;
		}
		if (value >= 0x00200000)
		{
			return 4;
		}
		if (value >= 0x00004000)
		{
			return 3;
		}
		if (value >= 0x00000080)
		{
			return 2;
		}
		return 1;
	}
}
