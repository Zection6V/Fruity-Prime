#include "Streams.hpp"

#include "Exceptions.hpp"
#include "IO.hpp"

#include <cstdio>
#include <utility>
#include <vector>

#include <zlib.h>

namespace MphRead::NativeRuntime
{
    namespace
    {
        // FileStream over the C runtime's file handle. FileShare.Read is what
        // every caller here asks for, which is the default sharing mode of
        // fopen on both platforms.
        class FileStream final : public Stream
        {
        public:
            FileStream(const std::string& path, bool write)
                : _path(path)
            {
                _file = std::fopen(path.c_str(), write ? "wb" : "rb");
                if (_file == nullptr)
                {
                    if (write)
                    {
                        throw System::IO::IOException(
                            "The process cannot access the file '" + path + "'.");
                    }
                    throw System::IO::FileNotFoundException(
                        "Could not find file '" + path + "'.");
                }
            }

            ~FileStream() override
            {
                Dispose();
            }

            void Write(std::span<const std::uint8_t> buffer) override
            {
                if (_file == nullptr || buffer.empty())
                {
                    return;
                }
                if (std::fwrite(buffer.data(), 1, buffer.size(), _file) != buffer.size())
                {
                    throw System::IO::IOException("Unable to write data to the file.");
                }
            }

            void WriteByte(std::uint8_t value) override
            {
                Write(std::span<const std::uint8_t>(&value, 1));
            }

            std::size_t ReadAtLeast(std::span<std::uint8_t> buffer) override
            {
                if (_file == nullptr || buffer.empty())
                {
                    return 0;
                }
                // ReadAtLeast keeps reading until the buffer is full or the
                // stream ends; fread already does exactly that.
                return std::fread(buffer.data(), 1, buffer.size(), _file);
            }

            void Flush() override
            {
                if (_file != nullptr)
                {
                    std::fflush(_file);
                }
            }

            void Dispose() override
            {
                if (_file != nullptr)
                {
                    std::fclose(_file);
                    _file = nullptr;
                }
            }

        private:
            std::string _path;
            std::FILE* _file = nullptr;
        };

        // DeflateStream: raw deflate, which is what .NET writes and reads.
        class DeflateStream final : public Stream
        {
        public:
            DeflateStream(std::shared_ptr<Stream> inner, bool leaveOpen, bool compress)
                : _inner(std::move(inner)), _leaveOpen(leaveOpen), _compress(compress)
            {
                _zstream.zalloc = nullptr;
                _zstream.zfree = nullptr;
                _zstream.opaque = nullptr;
                // -MAX_WBITS selects the headerless stream; CompressionLevel.Fastest
                // is level 1.
                const int result = compress
                    ? ::deflateInit2(&_zstream, 1, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY)
                    : ::inflateInit2(&_zstream, -MAX_WBITS);
                if (result != Z_OK)
                {
                    throw System::IO::IOException("The deflate stream could not be created.");
                }
                _open = true;
            }

            ~DeflateStream() override
            {
                Dispose();
            }

            void Write(std::span<const std::uint8_t> buffer) override
            {
                if (!_open || !_compress || buffer.empty())
                {
                    return;
                }
                _zstream.next_in = const_cast<Bytef*>(
                    reinterpret_cast<const Bytef*>(buffer.data()));
                _zstream.avail_in = static_cast<uInt>(buffer.size());
                Pump(Z_NO_FLUSH);
            }

            void WriteByte(std::uint8_t value) override
            {
                Write(std::span<const std::uint8_t>(&value, 1));
            }

            std::size_t ReadAtLeast(std::span<std::uint8_t> buffer) override
            {
                if (!_open || _compress || buffer.empty())
                {
                    return 0;
                }
                std::size_t produced = 0;
                while (produced < buffer.size() && !_ended)
                {
                    if (_zstream.avail_in == 0)
                    {
                        _input.resize(BufferSize);
                        const std::size_t read = _inner->ReadAtLeast(
                            std::span<std::uint8_t>(_input.data(), _input.size()));
                        if (read == 0)
                        {
                            _ended = true;
                            break;
                        }
                        _input.resize(read);
                        _zstream.next_in = reinterpret_cast<Bytef*>(_input.data());
                        _zstream.avail_in = static_cast<uInt>(read);
                    }
                    _zstream.next_out = reinterpret_cast<Bytef*>(buffer.data() + produced);
                    _zstream.avail_out = static_cast<uInt>(buffer.size() - produced);
                    const int result = ::inflate(&_zstream, Z_NO_FLUSH);
                    produced = buffer.size() - _zstream.avail_out;
                    if (result == Z_STREAM_END)
                    {
                        _ended = true;
                        break;
                    }
                    if (result != Z_OK && result != Z_BUF_ERROR)
                    {
                        throw System::IO::InvalidDataException(
                            "The archive entry was compressed using an unsupported "
                            "compression method.");
                    }
                    if (result == Z_BUF_ERROR && _zstream.avail_in == 0)
                    {
                        continue;
                    }
                }
                return produced;
            }

            void Flush() override
            {
                if (!_open || !_compress)
                {
                    return;
                }
                _zstream.next_in = nullptr;
                _zstream.avail_in = 0;
                Pump(Z_SYNC_FLUSH);
                _inner->Flush();
            }

            void Dispose() override
            {
                if (_open)
                {
                    if (_compress)
                    {
                        _zstream.next_in = nullptr;
                        _zstream.avail_in = 0;
                        Pump(Z_FINISH);
                        ::deflateEnd(&_zstream);
                    }
                    else
                    {
                        ::inflateEnd(&_zstream);
                    }
                    _open = false;
                }
                if (_inner != nullptr && !_leaveOpen)
                {
                    _inner->Dispose();
                }
                _inner = nullptr;
            }

        private:
            static constexpr std::size_t BufferSize = 16384;

            void Pump(int flush)
            {
                std::vector<std::uint8_t> output(BufferSize);
                while (true)
                {
                    _zstream.next_out = reinterpret_cast<Bytef*>(output.data());
                    _zstream.avail_out = static_cast<uInt>(output.size());
                    const int result = ::deflate(&_zstream, flush);
                    const std::size_t produced = output.size() - _zstream.avail_out;
                    if (produced != 0)
                    {
                        _inner->Write(
                            std::span<const std::uint8_t>(output.data(), produced));
                    }
                    if (result == Z_STREAM_END)
                    {
                        return;
                    }
                    if (result != Z_OK && result != Z_BUF_ERROR)
                    {
                        throw System::IO::IOException("The deflate stream failed.");
                    }
                    if (_zstream.avail_in == 0 && produced < output.size())
                    {
                        return;
                    }
                }
            }

            std::shared_ptr<Stream> _inner;
            bool _leaveOpen = false;
            bool _compress = false;
            bool _open = false;
            bool _ended = false;
            std::vector<std::uint8_t> _input;
            z_stream _zstream{};
        };
    }

    std::shared_ptr<Stream> FileStreamOpenCreateWriteShareRead(const std::string& path)
    {
        return std::make_shared<FileStream>(path, true);
    }

    std::shared_ptr<Stream> FileStreamOpenReadShareRead(const std::string& path)
    {
        return std::make_shared<FileStream>(path, false);
    }

    std::shared_ptr<Stream> DeflateStreamCompress(std::shared_ptr<Stream> stream, bool leaveOpen)
    {
        return std::make_shared<DeflateStream>(std::move(stream), leaveOpen, true);
    }

    std::shared_ptr<Stream> DeflateStreamDecompress(std::shared_ptr<Stream> stream, bool leaveOpen)
    {
        return std::make_shared<DeflateStream>(std::move(stream), leaveOpen, false);
    }
}
