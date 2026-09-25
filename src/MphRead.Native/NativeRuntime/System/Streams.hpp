#pragma once

// System.IO.Stream and the streams the game opens: FileStream (with .NET's
// FileShare, which on Unix is an advisory flock), MemoryStream, and
// System.IO.Compression's DeflateStream and GZipStream over zlib.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace MphRead::NativeRuntime
{
    enum class FileMode : std::int32_t
    {
        CreateNew = 1,
        Create = 2,
        Open = 3,
        OpenOrCreate = 4,
        Truncate = 5,
        Append = 6,
    };

    enum class FileAccess : std::int32_t
    {
        Read = 1,
        Write = 2,
        ReadWrite = 3,
    };

    enum class FileShare : std::int32_t
    {
        None = 0,
        Read = 1,
        Write = 2,
        ReadWrite = 3,
        Delete = 4,
    };

    enum class SeekOrigin : std::int32_t
    {
        Begin = 0,
        Current = 1,
        End = 2,
    };

    enum class CompressionLevel : std::int32_t
    {
        Optimal = 0,
        Fastest = 1,
        NoCompression = 2,
        SmallestSize = 3,
    };

    enum class CompressionMode : std::int32_t
    {
        Decompress = 0,
        Compress = 1,
    };

    class Stream
    {
    public:
        Stream() = default;
        Stream(const Stream&) = delete;
        Stream& operator=(const Stream&) = delete;
        virtual ~Stream() = default;

        [[nodiscard]] virtual bool CanRead() const noexcept = 0;
        [[nodiscard]] virtual bool CanWrite() const noexcept = 0;
        [[nodiscard]] virtual bool CanSeek() const noexcept = 0;

        // Stream.Read(buffer): up to buffer.size() bytes, 0 only at the end.
        [[nodiscard]] virtual std::size_t Read(std::span<std::uint8_t> buffer) = 0;
        virtual void Write(std::span<const std::uint8_t> buffer) = 0;
        virtual void Flush() {}
        // NotSupportedException unless CanSeek.
        virtual std::int64_t Seek(std::int64_t offset, SeekOrigin origin);
        [[nodiscard]] virtual std::int64_t Length() const;
        virtual void SetLength(std::int64_t value);
        [[nodiscard]] virtual std::int64_t Position() const;
        void Position(std::int64_t value);
        // Stream.Dispose(): flushes and lets go of what it holds; a second
        // call does nothing.
        virtual void Dispose() {}

        void WriteByte(std::uint8_t value);
        // -1 at the end.
        [[nodiscard]] std::int32_t ReadByte();
        // Stream.ReadAtLeast(buffer, buffer.Length, throwOnEndOfStream: false):
        // the bytes read, which is short only at the end.
        [[nodiscard]] std::size_t ReadAtLeast(std::span<std::uint8_t> buffer);
        // Stream.ReadExactly(buffer): EndOfStreamException if it runs out.
        void ReadExactly(std::span<std::uint8_t> buffer);
        // Stream.CopyTo(destination).
        void CopyTo(Stream& destination);
        // Everything from the current position to the end.
        [[nodiscard]] std::vector<std::uint8_t> ReadToEnd();
    };

    // new FileStream(path, mode, access, share).
    class FileStream final : public Stream
    {
    public:
        FileStream(const std::string& path, FileMode mode, FileAccess access, FileShare share);
        // new FileStream(path, mode): read-write unless the mode is Append,
        // and FileShare.Read.
        FileStream(const std::string& path, FileMode mode);
        ~FileStream() override;

        [[nodiscard]] bool CanRead() const noexcept override;
        [[nodiscard]] bool CanWrite() const noexcept override;
        [[nodiscard]] bool CanSeek() const noexcept override;
        [[nodiscard]] std::size_t Read(std::span<std::uint8_t> buffer) override;
        void Write(std::span<const std::uint8_t> buffer) override;
        void Flush() override;
        std::int64_t Seek(std::int64_t offset, SeekOrigin origin) override;
        [[nodiscard]] std::int64_t Length() const override;
        void SetLength(std::int64_t value) override;
        [[nodiscard]] std::int64_t Position() const override;
        void Dispose() override;

        // FileStream.Name: the full path.
        [[nodiscard]] const std::string& Name() const noexcept { return _path; }

    private:
        void ThrowIfDisposed() const;

        std::string _path;
        FileAccess _access;
        std::int64_t _position = 0;
        std::int64_t _appendStart = -1;
#if defined(_WIN32)
        void* _handle = nullptr;
#else
        int _fd = -1;
        bool _locked = false;
#endif
    };

    // new MemoryStream() / new MemoryStream(bytes).
    class MemoryStream final : public Stream
    {
    public:
        MemoryStream() = default;
        // Over a copy of `bytes`, fixed in size as .NET's is over an array.
        explicit MemoryStream(std::vector<std::uint8_t> bytes, bool writable = true);

        [[nodiscard]] bool CanRead() const noexcept override { return !_disposed; }
        [[nodiscard]] bool CanWrite() const noexcept override { return !_disposed && _writable; }
        [[nodiscard]] bool CanSeek() const noexcept override { return !_disposed; }
        [[nodiscard]] std::size_t Read(std::span<std::uint8_t> buffer) override;
        void Write(std::span<const std::uint8_t> buffer) override;
        std::int64_t Seek(std::int64_t offset, SeekOrigin origin) override;
        [[nodiscard]] std::int64_t Length() const override;
        void SetLength(std::int64_t value) override;
        [[nodiscard]] std::int64_t Position() const override;
        void Dispose() override { _disposed = true; }

        // MemoryStream.ToArray(): works after Dispose too.
        [[nodiscard]] std::vector<std::uint8_t> ToArray() const { return _buffer; }
        [[nodiscard]] const std::vector<std::uint8_t>& Buffer() const noexcept { return _buffer; }

    private:
        void ThrowIfDisposed() const;

        std::vector<std::uint8_t> _buffer;
        std::size_t _position = 0;
        bool _expandable = true;
        bool _writable = true;
        bool _disposed = false;
    };

    // new DeflateStream(stream, mode | level, leaveOpen) and GZipStream: the
    // raw deflate format, or gzip around it.
    class DeflateStream final : public Stream
    {
    public:
        enum class Format
        {
            Deflate,
            GZip,
        };

        DeflateStream(std::shared_ptr<Stream> stream, CompressionMode mode, bool leaveOpen = false,
            Format format = Format::Deflate);
        DeflateStream(std::shared_ptr<Stream> stream, CompressionLevel level, bool leaveOpen = false,
            Format format = Format::Deflate);
        ~DeflateStream() override;

        [[nodiscard]] bool CanRead() const noexcept override;
        [[nodiscard]] bool CanWrite() const noexcept override;
        [[nodiscard]] bool CanSeek() const noexcept override { return false; }
        [[nodiscard]] std::size_t Read(std::span<std::uint8_t> buffer) override;
        void Write(std::span<const std::uint8_t> buffer) override;
        void Flush() override;
        void Dispose() override;

    private:
        struct State;
        std::shared_ptr<Stream> _stream;
        std::unique_ptr<State> _state;
        bool _leaveOpen;
        bool _compress;
    };

    // The level .NET hands zlib for a CompressionLevel: 6, 1, 0 and 9.
    [[nodiscard]] int ZLibLevel(CompressionLevel level) noexcept;
    // Crc32.HashToUInt32 / the CRC a zip entry carries.
    [[nodiscard]] std::uint32_t Crc32(std::span<const std::uint8_t> bytes, std::uint32_t crc = 0) noexcept;
    // Raw deflate of `bytes` in one go, as a DeflateStream with `level`
    // written and disposed would produce.
    [[nodiscard]] std::vector<std::uint8_t> DeflateBytes(std::span<const std::uint8_t> bytes, CompressionLevel level);
    // Raw inflate of a whole deflate stream.
    [[nodiscard]] std::vector<std::uint8_t> InflateBytes(std::span<const std::uint8_t> bytes);

    // The factories the demo files were written against.

    // new FileStream(path, FileMode.Create, FileAccess.Write, FileShare.Read).
    [[nodiscard]] std::shared_ptr<Stream> FileStreamOpenCreateWriteShareRead(const std::string& path);
    // new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read).
    [[nodiscard]] std::shared_ptr<Stream> FileStreamOpenReadShareRead(const std::string& path);
    // new DeflateStream(stream, CompressionLevel.Fastest, leaveOpen).
    [[nodiscard]] std::shared_ptr<Stream> DeflateStreamCompress(std::shared_ptr<Stream> stream, bool leaveOpen);
    // new DeflateStream(stream, CompressionMode.Decompress, leaveOpen).
    [[nodiscard]] std::shared_ptr<Stream> DeflateStreamDecompress(std::shared_ptr<Stream> stream, bool leaveOpen);
}
