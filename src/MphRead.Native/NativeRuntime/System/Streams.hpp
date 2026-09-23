#pragma once

// System.IO.FileStream and System.IO.Compression.DeflateStream: the members
// the demo files use. The deflate format is the raw one .NET writes, which is
// zlib with no header or trailer.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace MphRead::NativeRuntime
{
    class Stream
    {
    public:
        Stream() = default;
        Stream(const Stream&) = delete;
        Stream& operator=(const Stream&) = delete;
        virtual ~Stream() = default;

        virtual void Write(std::span<const std::uint8_t> buffer) = 0;
        virtual void WriteByte(std::uint8_t value) = 0;
        // Stream.ReadAtLeast(buffer, buffer.Length, throwOnEndOfStream: false):
        // the bytes actually read, which is short only at the end.
        [[nodiscard]] virtual std::size_t ReadAtLeast(std::span<std::uint8_t> buffer) = 0;
        virtual void Flush() = 0;
        virtual void Dispose() = 0;
    };

    // new FileStream(path, FileMode.Create, FileAccess.Write, FileShare.Read).
    [[nodiscard]] std::shared_ptr<Stream> FileStreamOpenCreateWriteShareRead(
        const std::string& path);
    // new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read).
    [[nodiscard]] std::shared_ptr<Stream> FileStreamOpenReadShareRead(const std::string& path);

    // new DeflateStream(stream, CompressionLevel.Fastest, leaveOpen).
    [[nodiscard]] std::shared_ptr<Stream> DeflateStreamCompress(
        std::shared_ptr<Stream> stream, bool leaveOpen);
    // new DeflateStream(stream, CompressionMode.Decompress, leaveOpen).
    [[nodiscard]] std::shared_ptr<Stream> DeflateStreamDecompress(
        std::shared_ptr<Stream> stream, bool leaveOpen);
}
