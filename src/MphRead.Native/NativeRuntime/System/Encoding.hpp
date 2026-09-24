#pragma once

// Encoding.UTF8, Encoding.Unicode, Encoding.UTF32 and Rune, the conversions
// the game makes between them. A C# string is UTF-16; here text is carried as
// UTF-8, so everything below is the .NET conversion on either side of that.
//
// Every decoder follows .NET Core's rule for ill-formed input: the *maximal
// subpart* of an invalid sequence -- the longest prefix that could still have
// started a valid one -- becomes a single U+FFFD. "E2 82 41" is therefore
// U+FFFD 'A', not U+FFFD U+FFFD 'A'. Unpaired surrogates in UTF-16 or UTF-32
// input become U+FFFD as well.

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace MphRead::NativeRuntime
{
    // System.Buffers.OperationStatus, as Rune.DecodeFromUtf8 reports it.
    enum class OperationStatus
    {
        Done,
        DestinationTooSmall,
        NeedMoreData,
        InvalidData
    };

    struct Utf8Scalar final
    {
        // The scalar, or U+FFFD when Status is not Done.
        char32_t Value = 0xFFFDU;
        // Bytes consumed: the sequence, or the maximal ill-formed subpart.
        std::size_t Length = 0;
        OperationStatus Status = OperationStatus::InvalidData;

        [[nodiscard]] constexpr bool Valid() const noexcept
        {
            return Status == OperationStatus::Done;
        }
    };

    // Rune.DecodeFromUtf8(bytes). NeedMoreData means the input ends inside a
    // sequence that could still be valid; Length is then the whole input.
    [[nodiscard]] Utf8Scalar RuneDecodeFromUtf8(std::string_view bytes) noexcept;
    // The scalar that starts at `offset` of `text`, decoded the way
    // Encoding.UTF8.GetString would: a truncated sequence at the end is an
    // ill-formed subpart like any other. `offset` must be < text.size().
    [[nodiscard]] Utf8Scalar DecodeUtf8Scalar(std::string_view text, std::size_t offset) noexcept;
    // The scalar that ends at `end` of `text` (end > 0), decoded the same way:
    // what the forward decoder would have produced for the bytes that end
    // there. Its start is `end - Length`.
    [[nodiscard]] Utf8Scalar DecodeLastUtf8Scalar(std::string_view text, std::size_t end) noexcept;

    // Rune.EncodeToUtf8 onto the end of `output`. A value that is not a
    // Unicode scalar (a surrogate, or past U+10FFFF) is written as U+FFFD.
    void AppendUtf8(std::string& output, char32_t scalar);

    // Encoding.UTF8.GetString(bytes): well-formed UTF-8 out, whatever came in.
    // A byte-order mark is kept, as GetString keeps it.
    [[nodiscard]] std::string Utf8GetString(std::string_view bytes);
    [[nodiscard]] std::string Utf8GetString(std::span<const std::uint8_t> bytes);
    // new StreamReader(stream, Encoding.UTF8,
    // detectEncodingFromByteOrderMarks: true).ReadToEnd(), which is what
    // File.ReadAllText and File.ReadAllLines read with: a UTF-32, UTF-8 or
    // UTF-16 byte-order mark picks that encoding and is dropped, and anything
    // else is UTF-8.
    [[nodiscard]] std::string StreamReaderDecode(std::string_view bytes);
    [[nodiscard]] std::string StreamReaderDecode(std::span<const std::uint8_t> bytes);
    // (Encoding.Unicode | Encoding.BigEndianUnicode).GetString(bytes).
    [[nodiscard]] std::string Utf16GetString(std::string_view bytes, bool bigEndian);
    // (Encoding.UTF32 | new UTF32Encoding(bigEndian: true)).GetString(bytes).
    [[nodiscard]] std::string Utf32GetString(std::string_view bytes, bool bigEndian);

    // The string a C# string of this text is: UTF-16 and UTF-32 code units.
    [[nodiscard]] std::string Utf16ToUtf8(std::u16string_view value);
    [[nodiscard]] std::u16string Utf8ToUtf16(std::string_view value);
    [[nodiscard]] std::u32string Utf8ToUtf32(std::string_view value);
    [[nodiscard]] std::string Utf32ToUtf8(std::u32string_view value);
    // string.Length of this text: its UTF-16 code units.
    [[nodiscard]] std::size_t Utf16Length(std::string_view value) noexcept;

    // wchar_t text: UTF-16 on Windows, UTF-32 elsewhere.
    [[nodiscard]] std::string WideToUtf8(std::wstring_view value);
    [[nodiscard]] std::wstring Utf8ToWide(std::string_view value);

    // A C# string is any sequence of UTF-16 code units, lone surrogates
    // included, and one that came from the operating system -- a path, an
    // argument, an environment variable -- goes back to it unchanged. WTF-8
    // carries that: UTF-8 in which a lone surrogate is written as its own
    // three-byte sequence instead of being replaced. For those strings only;
    // anything encoded for a file or the wire is UTF-8.
    [[nodiscard]] std::string WideToWtf8(std::wstring_view value);
    [[nodiscard]] std::wstring Wtf8ToWide(std::string_view value);
}
