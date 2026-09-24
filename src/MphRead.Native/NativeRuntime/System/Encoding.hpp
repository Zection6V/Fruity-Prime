#pragma once

// Encoding.UTF8 and Encoding.Unicode, the two directions the game converts
// between. Unpaired surrogates and malformed sequences become U+FFFD, which is
// what .NET's decoders do by default.

#include <string>
#include <string_view>

namespace MphRead::NativeRuntime
{
    // Encoding.UTF8.GetString(Encoding.Unicode.GetBytes(value)).
    [[nodiscard]] std::string Utf16ToUtf8(std::u16string_view value);
    // Encoding.Unicode.GetString(Encoding.UTF8.GetBytes(value)).
    [[nodiscard]] std::u16string Utf8ToUtf16(std::string_view value);
}
