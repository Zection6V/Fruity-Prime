#pragma once

// std::from_chars for float and double -- the parse double.Parse and
// float.Parse reach for with the invariant culture.
//
// libstdc++ and MSVC's STL have it. libc++ before 20 (the NDK's and Apple's)
// has only the integer overloads, so there the same contract is met by
// matching exactly what from_chars accepts and then converting that text in
// the "C" locale, which is correctly rounded just as from_chars is.

#include <charconv>
#include <version>

namespace MphRead::NativeRuntime
{
#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
    inline std::from_chars_result FromChars(const char* first, const char* last, float& value,
        std::chars_format format = std::chars_format::general)
    {
        return std::from_chars(first, last, value, format);
    }

    inline std::from_chars_result FromChars(const char* first, const char* last, double& value,
        std::chars_format format = std::chars_format::general)
    {
        return std::from_chars(first, last, value, format);
    }

    inline std::from_chars_result FromChars(const char* first, const char* last, long double& value,
        std::chars_format format = std::chars_format::general)
    {
        return std::from_chars(first, last, value, format);
    }
#else
    std::from_chars_result FromChars(const char* first, const char* last, float& value,
        std::chars_format format = std::chars_format::general);
    std::from_chars_result FromChars(const char* first, const char* last, double& value,
        std::chars_format format = std::chars_format::general);
    std::from_chars_result FromChars(const char* first, const char* last, long double& value,
        std::chars_format format = std::chars_format::general);
#endif
}
