#include "Streams.hpp"

#include "Console.hpp"
#include "Encoding.hpp"
#include "Exceptions.hpp"
#include "IO.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <limits>
#include <optional>
#include <utility>

#include <zlib.h>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace MphRead::NativeRuntime
{
    // ==== Stream ====================================================================

    std::int64_t Stream::Seek(std::int64_t, SeekOrigin)
    {
        throw System::NotSupportedException("Stream does not support seeking.");
    }

    std::int64_t Stream::Length() const
    {
        throw System::NotSupportedException("Stream does not support seeking.");
    }

    void Stream::SetLength(std::int64_t)
    {
        throw System::NotSupportedException("Stream does not support seeking.");
    }

    std::int64_t Stream::Position() const
    {
        throw System::NotSupportedException("Stream does not support seeking.");
    }

    void Stream::Position(std::int64_t value)
    {
        if (value < 0)
        {
            throw System::ArgumentOutOfRangeException("value");
        }
        (void)Seek(value, SeekOrigin::Begin);
    }

    void Stream::WriteByte(std::uint8_t value)
    {
        Write(std::span<const std::uint8_t>(&value, 1));
    }

    std::int32_t Stream::ReadByte()
    {
        std::uint8_t value = 0;
        return Read(std::span<std::uint8_t>(&value, 1)) == 0 ? -1 : value;
    }

    std::size_t Stream::ReadAtLeast(std::span<std::uint8_t> buffer)
    {
        std::size_t total = 0;
        while (total < buffer.size())
        {
            const std::size_t read = Read(buffer.subspan(total));
            if (read == 0)
            {
                break;
            }
            total += read;
        }
        return total;
    }

    void Stream::ReadExactly(std::span<std::uint8_t> buffer)
    {
        if (ReadAtLeast(buffer) != buffer.size())
        {
            throw System::IO::EndOfStreamException();
        }
    }

    void Stream::CopyTo(Stream& destination)
    {
        std::vector<std::uint8_t> buffer(81920);
        while (true)
        {
            const std::size_t read = Read(buffer);
            if (read == 0)
            {
                return;
            }
            destination.Write(std::span<const std::uint8_t>(buffer.data(), read));
        }
    }

    std::vector<std::uint8_t> Stream::ReadToEnd()
    {
        std::vector<std::uint8_t> result;
        std::array<std::uint8_t, 81920> buffer{};
        while (true)
        {
            const std::size_t read = Read(buffer);
            if (read == 0)
            {
                return result;
            }
            result.insert(result.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(read));
        }
    }

    // ==== FileStream ================================================================

    namespace
    {
        [[nodiscard]] bool HasAccess(FileAccess access, FileAccess flag) noexcept
        {
            return (static_cast<std::int32_t>(access) & static_cast<std::int32_t>(flag)) != 0;
        }

        // FileStream's argument checks, in the order .NET makes them.
        void ValidateFileStreamArguments(const std::string& path, FileMode mode, FileAccess access)
        {
            if (path.empty())
            {
                throw System::ArgumentException("Empty path name is not legal. (Parameter 'path')");
            }
            const bool write = HasAccess(access, FileAccess::Write);
            if (!write && (mode == FileMode::Truncate || mode == FileMode::CreateNew
                    || mode == FileMode::Create || mode == FileMode::Append))
            {
                throw System::ArgumentException("Combining FileMode: " + std::string(
                    mode == FileMode::Truncate ? "Truncate" : mode == FileMode::CreateNew ? "CreateNew"
                    : mode == FileMode::Create ? "Create" : "Append")
                    + " with FileAccess: Read is invalid. (Parameter 'access')");
            }
            if (mode == FileMode::Append && access != FileAccess::Write)
            {
                throw System::ArgumentException(
                    "Append access can be requested only in write-only mode. (Parameter 'access')");
            }
        }

        [[nodiscard]] bool FileLockingDisabled()
        {
            // DOTNET_SYSTEM_IO_DISABLEFILELOCKING turns the advisory lock off.
            static const bool disabled = []
            {
                const std::optional<std::string> value = EnvironmentGetVariable("DOTNET_SYSTEM_IO_DISABLEFILELOCKING");
                return value.has_value() && (*value == "1" || *value == "true" || *value == "True");
            }();
            return disabled;
        }
    }

    FileStream::FileStream(const std::string& path, FileMode mode)
        : FileStream(path, mode, mode == FileMode::Append ? FileAccess::Write : FileAccess::ReadWrite,
            FileShare::Read)
    {
    }

    FileStream::FileStream(const std::string& path, FileMode mode, FileAccess access, FileShare share)
        : _access(access)
    {
        ValidateFileStreamArguments(path, mode, access);
        _path = PathGetFullPath(path);
#if defined(_WIN32)
        DWORD desired = 0;
        if (HasAccess(access, FileAccess::Read))
        {
            desired |= GENERIC_READ;
        }
        if (HasAccess(access, FileAccess::Write))
        {
            desired |= GENERIC_WRITE;
        }
        DWORD disposition = OPEN_EXISTING;
        switch (mode)
        {
        case FileMode::CreateNew:
            disposition = CREATE_NEW;
            break;
        case FileMode::Create:
            disposition = CREATE_ALWAYS;
            break;
        case FileMode::Open:
            disposition = OPEN_EXISTING;
            break;
        case FileMode::OpenOrCreate:
        case FileMode::Append:
            disposition = OPEN_ALWAYS;
            break;
        case FileMode::Truncate:
            disposition = TRUNCATE_EXISTING;
            break;
        }
        DWORD shareMode = 0;
        if ((static_cast<std::int32_t>(share) & static_cast<std::int32_t>(FileShare::Read)) != 0)
        {
            shareMode |= FILE_SHARE_READ;
        }
        if ((static_cast<std::int32_t>(share) & static_cast<std::int32_t>(FileShare::Write)) != 0)
        {
            shareMode |= FILE_SHARE_WRITE;
        }
        if ((static_cast<std::int32_t>(share) & static_cast<std::int32_t>(FileShare::Delete)) != 0)
        {
            shareMode |= FILE_SHARE_DELETE;
        }
        const HANDLE handle = ::CreateFileW(Wtf8ToWide(_path).c_str(), desired, shareMode, nullptr,
            disposition, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
        {
            const DWORD error = ::GetLastError();
            ThrowForLastIOError(error, _path);
        }
        _handle = handle;
        if (mode == FileMode::Append)
        {
            _appendStart = Seek(0, SeekOrigin::End);
        }
#else
        int flags = O_CLOEXEC;
        switch (access)
        {
        case FileAccess::Read:
            flags |= O_RDONLY;
            break;
        case FileAccess::Write:
            flags |= O_WRONLY;
            break;
        case FileAccess::ReadWrite:
            flags |= O_RDWR;
            break;
        }
        switch (mode)
        {
        case FileMode::CreateNew:
            flags |= O_CREAT | O_EXCL;
            break;
        case FileMode::Create:
        case FileMode::OpenOrCreate:
        case FileMode::Append:
            flags |= O_CREAT;
            break;
        default:
            break;
        }
        const std::filesystem::path native = PathFromUtf8(_path);
        int fd = -1;
        do
        {
            fd = ::open(native.c_str(), flags, 0666);
        }
        while (fd < 0 && errno == EINTR);
        if (fd < 0)
        {
            const int error = errno;
            ThrowForLastIOError(error, _path, error == ENOENT && mode != FileMode::Open && mode != FileMode::Truncate);
        }
        _fd = fd;
        try
        {
            struct stat status{};
            if (::fstat(_fd, &status) == 0 && S_ISDIR(status.st_mode))
            {
                ThrowForLastIOError(EACCES, _path);
            }
            // SafeFileHandle.Init: FileShare.None is an exclusive lock, any
            // other share a shared one, and a file someone else holds the
            // other kind of is "being used by another process".
            if (!FileLockingDisabled())
            {
                int result = 0;
                do
                {
                    result = ::flock(_fd, (share == FileShare::None ? LOCK_EX : LOCK_SH) | LOCK_NB);
                }
                while (result < 0 && errno == EINTR);
                if (result == 0)
                {
                    _locked = true;
                }
                else if (errno == EWOULDBLOCK)
                {
                    ThrowForLastIOError(EWOULDBLOCK, _path);
                }
            }
            // Truncation after the lock, so a file somebody else has open is
            // not emptied under them.
            if (mode == FileMode::Create || mode == FileMode::Truncate)
            {
                int result = 0;
                do
                {
                    result = ::ftruncate(_fd, 0);
                }
                while (result < 0 && errno == EINTR);
                if (result != 0 && errno != EBADF && errno != EINVAL)
                {
                    ThrowForLastIOError(errno, _path);
                }
            }
            if (mode == FileMode::Append)
            {
                _appendStart = Seek(0, SeekOrigin::End);
            }
        }
        catch (...)
        {
            Dispose();
            throw;
        }
#endif
    }

    FileStream::~FileStream()
    {
        try
        {
            Dispose();
        }
        catch (...)
        {
        }
    }

    void FileStream::ThrowIfDisposed() const
    {
#if defined(_WIN32)
        if (_handle == nullptr)
#else
        if (_fd < 0)
#endif
        {
            throw System::ObjectDisposedException("file");
        }
    }

    bool FileStream::CanRead() const noexcept
    {
#if defined(_WIN32)
        return _handle != nullptr && HasAccess(_access, FileAccess::Read);
#else
        return _fd >= 0 && HasAccess(_access, FileAccess::Read);
#endif
    }

    bool FileStream::CanWrite() const noexcept
    {
#if defined(_WIN32)
        return _handle != nullptr && HasAccess(_access, FileAccess::Write);
#else
        return _fd >= 0 && HasAccess(_access, FileAccess::Write);
#endif
    }

    bool FileStream::CanSeek() const noexcept
    {
#if defined(_WIN32)
        return _handle != nullptr;
#else
        return _fd >= 0;
#endif
    }

    std::size_t FileStream::Read(std::span<std::uint8_t> buffer)
    {
        ThrowIfDisposed();
        if (!HasAccess(_access, FileAccess::Read))
        {
            throw System::NotSupportedException("Stream does not support reading.");
        }
        if (buffer.empty())
        {
            return 0;
        }
#if defined(_WIN32)
        DWORD read = 0;
        const DWORD request = static_cast<DWORD>(std::min<std::size_t>(buffer.size(), 0x7FFFFFFFU));
        if (::ReadFile(static_cast<HANDLE>(_handle), buffer.data(), request, &read, nullptr) == 0)
        {
            const DWORD error = ::GetLastError();
            if (error == ERROR_BROKEN_PIPE || error == ERROR_HANDLE_EOF)
            {
                return 0;
            }
            ThrowForLastIOError(error, _path);
        }
        _position += read;
        return read;
#else
        while (true)
        {
            const ssize_t read = ::read(_fd, buffer.data(), buffer.size());
            if (read >= 0)
            {
                _position += read;
                return static_cast<std::size_t>(read);
            }
            if (errno != EINTR)
            {
                ThrowForLastIOError(errno, _path);
            }
        }
#endif
    }

    void FileStream::Write(std::span<const std::uint8_t> buffer)
    {
        ThrowIfDisposed();
        if (!HasAccess(_access, FileAccess::Write))
        {
            throw System::NotSupportedException("Stream does not support writing.");
        }
        while (!buffer.empty())
        {
#if defined(_WIN32)
            DWORD written = 0;
            const DWORD request = static_cast<DWORD>(std::min<std::size_t>(buffer.size(), 0x7FFFFFFFU));
            if (::WriteFile(static_cast<HANDLE>(_handle), buffer.data(), request, &written, nullptr) == 0)
            {
                ThrowForLastIOError(::GetLastError(), _path);
            }
#else
            const ssize_t result = ::write(_fd, buffer.data(), buffer.size());
            if (result < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                ThrowForLastIOError(errno, _path);
            }
            const std::size_t written = static_cast<std::size_t>(result);
#endif
            if (written == 0)
            {
                throw System::IO::IOException("Unable to write data to the transport connection.");
            }
            _position += static_cast<std::int64_t>(written);
            buffer = buffer.subspan(written);
        }
    }

    void FileStream::Flush()
    {
        ThrowIfDisposed();
    }

    std::int64_t FileStream::Seek(std::int64_t offset, SeekOrigin origin)
    {
        ThrowIfDisposed();
        std::int64_t target = 0;
        switch (origin)
        {
        case SeekOrigin::Begin:
            target = offset;
            break;
        case SeekOrigin::Current:
            target = _position + offset;
            break;
        case SeekOrigin::End:
            target = Length() + offset;
            break;
        }
        if (target < 0)
        {
            throw System::IO::IOException(
                "An attempt was made to move the position before the beginning of the stream.");
        }
        if (_appendStart >= 0 && target < _appendStart)
        {
            throw System::IO::IOException(
                "Unable seek backward to overwrite data that previously existed in a file opened in Append mode.");
        }
#if defined(_WIN32)
        LARGE_INTEGER distance{};
        distance.QuadPart = target;
        if (::SetFilePointerEx(static_cast<HANDLE>(_handle), distance, nullptr, FILE_BEGIN) == 0)
        {
            ThrowForLastIOError(::GetLastError(), _path);
        }
#else
        if (::lseek(_fd, static_cast<off_t>(target), SEEK_SET) < 0)
        {
            ThrowForLastIOError(errno, _path);
        }
#endif
        _position = target;
        return target;
    }

    std::int64_t FileStream::Length() const
    {
        ThrowIfDisposed();
#if defined(_WIN32)
        LARGE_INTEGER size{};
        if (::GetFileSizeEx(static_cast<HANDLE>(_handle), &size) == 0)
        {
            ThrowForLastIOError(::GetLastError(), _path);
        }
        return size.QuadPart;
#else
        struct stat status{};
        if (::fstat(_fd, &status) != 0)
        {
            ThrowForLastIOError(errno, _path);
        }
        return static_cast<std::int64_t>(status.st_size);
#endif
    }

    void FileStream::SetLength(std::int64_t value)
    {
        ThrowIfDisposed();
        if (value < 0)
        {
            throw System::ArgumentOutOfRangeException("value");
        }
#if defined(_WIN32)
        const std::int64_t position = _position;
        (void)Seek(value, SeekOrigin::Begin);
        if (::SetEndOfFile(static_cast<HANDLE>(_handle)) == 0)
        {
            ThrowForLastIOError(::GetLastError(), _path);
        }
        (void)Seek(std::min(position, value), SeekOrigin::Begin);
#else
        if (::ftruncate(_fd, static_cast<off_t>(value)) != 0)
        {
            ThrowForLastIOError(errno, _path);
        }
        if (_position > value)
        {
            (void)Seek(value, SeekOrigin::Begin);
        }
#endif
    }

    std::int64_t FileStream::Position() const
    {
        ThrowIfDisposed();
        return _position;
    }

    void FileStream::Dispose()
    {
#if defined(_WIN32)
        if (_handle != nullptr)
        {
            const HANDLE handle = static_cast<HANDLE>(_handle);
            _handle = nullptr;
            if (::CloseHandle(handle) == 0)
            {
                ThrowForLastIOError(::GetLastError(), _path);
            }
        }
#else
        if (_fd >= 0)
        {
            const int fd = _fd;
            _fd = -1;
            if (_locked)
            {
                (void)::flock(fd, LOCK_UN);
                _locked = false;
            }
            if (::close(fd) != 0 && errno != EINTR)
            {
                ThrowForLastIOError(errno, _path);
            }
        }
#endif
    }

    // ==== MemoryStream ===============================================================

    MemoryStream::MemoryStream(std::vector<std::uint8_t> bytes, bool writable)
        : _buffer(std::move(bytes)), _expandable(false), _writable(writable)
    {
    }

    void MemoryStream::ThrowIfDisposed() const
    {
        if (_disposed)
        {
            throw System::ObjectDisposedException("Stream");
        }
    }

    std::size_t MemoryStream::Read(std::span<std::uint8_t> buffer)
    {
        ThrowIfDisposed();
        if (_position >= _buffer.size())
        {
            return 0;
        }
        const std::size_t count = std::min(buffer.size(), _buffer.size() - _position);
        std::copy_n(_buffer.begin() + static_cast<std::ptrdiff_t>(_position), count, buffer.begin());
        _position += count;
        return count;
    }

    void MemoryStream::Write(std::span<const std::uint8_t> buffer)
    {
        ThrowIfDisposed();
        if (!_writable)
        {
            throw System::NotSupportedException("Stream does not support writing.");
        }
        const std::size_t end = _position + buffer.size();
        if (end > _buffer.size())
        {
            if (!_expandable)
            {
                throw System::NotSupportedException("Memory stream is not expandable.");
            }
            _buffer.resize(end);
        }
        std::copy(buffer.begin(), buffer.end(), _buffer.begin() + static_cast<std::ptrdiff_t>(_position));
        _position = end;
    }

    std::int64_t MemoryStream::Seek(std::int64_t offset, SeekOrigin origin)
    {
        ThrowIfDisposed();
        std::int64_t target = offset;
        if (origin == SeekOrigin::Current)
        {
            target += static_cast<std::int64_t>(_position);
        }
        else if (origin == SeekOrigin::End)
        {
            target += static_cast<std::int64_t>(_buffer.size());
        }
        if (target < 0)
        {
            throw System::IO::IOException(
                "An attempt was made to move the position before the beginning of the stream.");
        }
        _position = static_cast<std::size_t>(target);
        return target;
    }

    std::int64_t MemoryStream::Length() const
    {
        ThrowIfDisposed();
        return static_cast<std::int64_t>(_buffer.size());
    }

    void MemoryStream::SetLength(std::int64_t value)
    {
        ThrowIfDisposed();
        if (!_writable || !_expandable)
        {
            throw System::NotSupportedException("Memory stream is not expandable.");
        }
        _buffer.resize(static_cast<std::size_t>(value));
        _position = std::min(_position, _buffer.size());
    }

    std::int64_t MemoryStream::Position() const
    {
        ThrowIfDisposed();
        return static_cast<std::int64_t>(_position);
    }

    // ==== DeflateStream =============================================================

    int ZLibLevel(CompressionLevel level) noexcept
    {
        switch (level)
        {
        case CompressionLevel::Fastest:
            return 1;
        case CompressionLevel::NoCompression:
            return 0;
        case CompressionLevel::SmallestSize:
            return 9;
        case CompressionLevel::Optimal:
        default:
            return 6;
        }
    }

    namespace
    {
        constexpr std::size_t DeflateBufferSize = 8192;

        [[nodiscard]] int WindowBits(DeflateStream::Format format) noexcept
        {
            // Negative is headerless deflate; 16 more asks zlib for gzip.
            return format == DeflateStream::Format::GZip ? 16 + MAX_WBITS : -MAX_WBITS;
        }
    }

    struct DeflateStream::State final
    {
        z_stream Z{};
        std::vector<std::uint8_t> Buffer = std::vector<std::uint8_t>(DeflateBufferSize);
        bool Open = false;
        bool Ended = false;
        bool Wrote = false;
    };

    DeflateStream::DeflateStream(std::shared_ptr<Stream> stream, CompressionMode mode, bool leaveOpen, Format format)
        : _stream(std::move(stream)), _state(std::make_unique<State>()), _leaveOpen(leaveOpen),
          _compress(mode == CompressionMode::Compress)
    {
        if (_stream == nullptr)
        {
            throw System::ArgumentNullException("stream");
        }
        const int result = _compress
            ? ::deflateInit2(&_state->Z, ZLibLevel(CompressionLevel::Optimal), Z_DEFLATED, WindowBits(format), 8, Z_DEFAULT_STRATEGY)
            : ::inflateInit2(&_state->Z, WindowBits(format));
        if (result != Z_OK)
        {
            throw System::IO::IOException("The underlying compression routine could not be loaded correctly.");
        }
        _state->Open = true;
    }

    DeflateStream::DeflateStream(std::shared_ptr<Stream> stream, CompressionLevel level, bool leaveOpen, Format format)
        : _stream(std::move(stream)), _state(std::make_unique<State>()), _leaveOpen(leaveOpen), _compress(true)
    {
        if (_stream == nullptr)
        {
            throw System::ArgumentNullException("stream");
        }
        if (::deflateInit2(&_state->Z, ZLibLevel(level), Z_DEFLATED, WindowBits(format), 8, Z_DEFAULT_STRATEGY) != Z_OK)
        {
            throw System::IO::IOException("The underlying compression routine could not be loaded correctly.");
        }
        _state->Open = true;
    }

    DeflateStream::~DeflateStream()
    {
        try
        {
            Dispose();
        }
        catch (...)
        {
        }
    }

    bool DeflateStream::CanRead() const noexcept
    {
        return _state->Open && !_compress;
    }

    bool DeflateStream::CanWrite() const noexcept
    {
        return _state->Open && _compress;
    }

    std::size_t DeflateStream::Read(std::span<std::uint8_t> buffer)
    {
        if (!_state->Open)
        {
            throw System::ObjectDisposedException("Stream");
        }
        if (_compress)
        {
            throw System::InvalidOperationException("Reading from the compression stream is not supported.");
        }
        z_stream& z = _state->Z;
        std::size_t produced = 0;
        while (produced < buffer.size() && !_state->Ended)
        {
            if (z.avail_in == 0)
            {
                const std::size_t read = _stream->Read(_state->Buffer);
                if (read == 0)
                {
                    break;
                }
                z.next_in = _state->Buffer.data();
                z.avail_in = static_cast<uInt>(read);
            }
            z.next_out = buffer.data() + produced;
            z.avail_out = static_cast<uInt>(buffer.size() - produced);
            const int result = ::inflate(&z, Z_NO_FLUSH);
            produced = buffer.size() - z.avail_out;
            if (result == Z_STREAM_END)
            {
                _state->Ended = true;
            }
            else if (result != Z_OK && result != Z_BUF_ERROR)
            {
                throw System::IO::InvalidDataException(
                    "The archive entry was compressed using an unsupported compression method.");
            }
            // Return what there is rather than block for more.
            if (produced > 0)
            {
                break;
            }
        }
        return produced;
    }

    void DeflateStream::Write(std::span<const std::uint8_t> buffer)
    {
        if (!_state->Open)
        {
            throw System::ObjectDisposedException("Stream");
        }
        if (!_compress)
        {
            throw System::InvalidOperationException("Writing to the compression stream is not supported.");
        }
        if (buffer.empty())
        {
            return;
        }
        z_stream& z = _state->Z;
        z.next_in = const_cast<Bytef*>(buffer.data());
        z.avail_in = static_cast<uInt>(buffer.size());
        _state->Wrote = true;
        while (z.avail_in != 0)
        {
            z.next_out = _state->Buffer.data();
            z.avail_out = static_cast<uInt>(_state->Buffer.size());
            if (::deflate(&z, Z_NO_FLUSH) == Z_STREAM_ERROR)
            {
                throw System::IO::IOException("The underlying compression routine returned an error.");
            }
            const std::size_t produced = _state->Buffer.size() - z.avail_out;
            if (produced != 0)
            {
                _stream->Write(std::span<const std::uint8_t>(_state->Buffer.data(), produced));
            }
        }
    }

    void DeflateStream::Flush()
    {
        if (!_state->Open)
        {
            throw System::ObjectDisposedException("Stream");
        }
        if (_compress && _state->Wrote)
        {
            z_stream& z = _state->Z;
            z.next_in = nullptr;
            z.avail_in = 0;
            do
            {
                z.next_out = _state->Buffer.data();
                z.avail_out = static_cast<uInt>(_state->Buffer.size());
                (void)::deflate(&z, Z_SYNC_FLUSH);
                const std::size_t produced = _state->Buffer.size() - z.avail_out;
                if (produced != 0)
                {
                    _stream->Write(std::span<const std::uint8_t>(_state->Buffer.data(), produced));
                }
            }
            while (z.avail_out == 0);
            _stream->Flush();
        }
    }

    void DeflateStream::Dispose()
    {
        if (!_state || !_state->Open)
        {
            return;
        }
        _state->Open = false;
        try
        {
            if (_compress)
            {
                // A compressor nobody wrote to still writes the empty
                // stream: two bytes of raw deflate.
                z_stream& z = _state->Z;
                z.next_in = nullptr;
                z.avail_in = 0;
                int result = Z_OK;
                do
                {
                    z.next_out = _state->Buffer.data();
                    z.avail_out = static_cast<uInt>(_state->Buffer.size());
                    result = ::deflate(&z, Z_FINISH);
                    const std::size_t produced = _state->Buffer.size() - z.avail_out;
                    if (produced != 0)
                    {
                        _stream->Write(std::span<const std::uint8_t>(_state->Buffer.data(), produced));
                    }
                }
                while (result == Z_OK);
                ::deflateEnd(&z);
                _stream->Flush();
            }
            else
            {
                ::inflateEnd(&_state->Z);
            }
        }
        catch (...)
        {
            if (!_leaveOpen)
            {
                _stream->Dispose();
            }
            throw;
        }
        if (!_leaveOpen)
        {
            _stream->Dispose();
        }
    }

    std::uint32_t Crc32(std::span<const std::uint8_t> bytes, std::uint32_t crc) noexcept
    {
        while (!bytes.empty())
        {
            const auto chunk = static_cast<uInt>(std::min<std::size_t>(bytes.size(), 0x40000000U));
            crc = static_cast<std::uint32_t>(::crc32(crc, bytes.data(), chunk));
            bytes = bytes.subspan(chunk);
        }
        return crc;
    }

    std::vector<std::uint8_t> DeflateBytes(std::span<const std::uint8_t> bytes, CompressionLevel level)
    {
        auto output = std::make_shared<MemoryStream>();
        {
            DeflateStream deflate(output, level, true);
            deflate.Write(bytes);
            deflate.Dispose();
        }
        return output->ToArray();
    }

    std::vector<std::uint8_t> InflateBytes(std::span<const std::uint8_t> bytes)
    {
        auto input = std::make_shared<MemoryStream>(std::vector<std::uint8_t>(bytes.begin(), bytes.end()), false);
        DeflateStream inflate(input, CompressionMode::Decompress, true);
        return inflate.ReadToEnd();
    }

    std::shared_ptr<Stream> FileStreamOpenCreateWriteShareRead(const std::string& path)
    {
        return std::make_shared<FileStream>(path, FileMode::Create, FileAccess::Write, FileShare::Read);
    }

    std::shared_ptr<Stream> FileStreamOpenReadShareRead(const std::string& path)
    {
        return std::make_shared<FileStream>(path, FileMode::Open, FileAccess::Read, FileShare::Read);
    }

    std::shared_ptr<Stream> DeflateStreamCompress(std::shared_ptr<Stream> stream, bool leaveOpen)
    {
        return std::make_shared<DeflateStream>(std::move(stream), CompressionLevel::Fastest, leaveOpen);
    }

    std::shared_ptr<Stream> DeflateStreamDecompress(std::shared_ptr<Stream> stream, bool leaveOpen)
    {
        return std::make_shared<DeflateStream>(std::move(stream), CompressionMode::Decompress, leaveOpen);
    }
}
