#include "Movie.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <new>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace MphRead::Formats::MovieNativeRuntime
{
    struct TaskState
    {
        std::mutex Mutex;
        std::condition_variable Condition;
        bool Done = false;
        std::exception_ptr Exception{};
    };

    namespace
    {
        constexpr std::uint32_t Prime2 = 2246822519U;
        constexpr std::uint32_t Prime3 = 3266489917U;
        constexpr std::uint32_t Prime4 = 668265263U;
        constexpr std::uint32_t Prime5 = 374761393U;

        [[nodiscard]] std::uint32_t GlobalHashSeed() noexcept
        {
            static const std::uint32_t seed = []() noexcept
            {
                try
                {
                    std::random_device device;
                    return (static_cast<std::uint32_t>(device()) << 16)
                        ^ static_cast<std::uint32_t>(device());
                }
                catch (...)
                {
                    return 0U;
                }
            }();
            return seed;
        }

        [[nodiscard]] constexpr std::uint32_t RotateLeft(std::uint32_t value, int offset) noexcept
        {
            return std::rotl(value, offset);
        }

        [[nodiscard]] std::uint32_t QueueRound(std::uint32_t hash, std::uint32_t queued) noexcept
        {
            return RotateLeft(hash + queued * Prime3, 17) * Prime4;
        }

        [[nodiscard]] std::uint32_t MixFinal(std::uint32_t hash) noexcept
        {
            hash ^= hash >> 15;
            hash *= Prime2;
            hash ^= hash >> 13;
            hash *= Prime3;
            hash ^= hash >> 16;
            return hash;
        }
    }

    std::int32_t HashCombine(std::int32_t first, std::int32_t second) noexcept
    {
        std::uint32_t hash = GlobalHashSeed() + Prime5;
        hash += 8U;
        hash = QueueRound(hash, std::bit_cast<std::uint32_t>(first));
        hash = QueueRound(hash, std::bit_cast<std::uint32_t>(second));
        return std::bit_cast<std::int32_t>(MixFinal(hash));
    }

    void BinaryReader::ReadExact(void* destination, std::size_t size)
    {
        _stream->read(static_cast<char*>(destination), static_cast<std::streamsize>(size));
        if (_stream->gcount() != static_cast<std::streamsize>(size))
        {
            throw std::runtime_error("Unable to read beyond the end of the stream.");
        }
    }

    std::uint8_t BinaryReader::ReadByte()
    {
        std::uint8_t value = 0;
        ReadExact(&value, sizeof(value));
        return value;
    }

    char16_t BinaryReader::ReadChar()
    {
        return static_cast<char16_t>(ReadByte());
    }

    std::int16_t BinaryReader::ReadInt16()
    {
        const std::uint16_t value = ReadUInt16();
        return std::bit_cast<std::int16_t>(value);
    }

    std::uint16_t BinaryReader::ReadUInt16()
    {
        const std::uint16_t b0 = ReadByte();
        const std::uint16_t b1 = ReadByte();
        return static_cast<std::uint16_t>(b0 | (b1 << 8));
    }

    std::int32_t BinaryReader::ReadInt32()
    {
        std::uint32_t value = ReadByte();
        value |= static_cast<std::uint32_t>(ReadByte()) << 8;
        value |= static_cast<std::uint32_t>(ReadByte()) << 16;
        value |= static_cast<std::uint32_t>(ReadByte()) << 24;
        return std::bit_cast<std::int32_t>(value);
    }

    std::int64_t BinaryReader::Position()
    {
        const std::streampos position = _stream->tellg();
        if (position == std::streampos(-1))
        {
            throw std::runtime_error("Stream position is unavailable.");
        }
        return static_cast<std::int64_t>(position);
    }

    void BinaryReader::Position(std::int64_t value)
    {
        _stream->clear();
        _stream->seekg(static_cast<std::streamoff>(value), std::ios::beg);
        if (!_stream->good())
        {
            throw std::runtime_error("An attempt was made to move the position before the beginning of the stream.");
        }
    }

    MovieTask::Awaiter::Awaiter(std::shared_ptr<TaskState> state) noexcept
        : _state(std::move(state))
    {
    }

    void MovieTask::Awaiter::GetResult()
    {
        std::unique_lock<std::mutex> lock(_state->Mutex);
        _state->Condition.wait(lock, [this]() { return _state->Done; });
        std::exception_ptr exception = _state->Exception;
        lock.unlock();
        if (exception)
        {
            std::rethrow_exception(exception);
        }
    }

    MovieTask::MovieTask()
        : _state(std::make_shared<TaskState>())
    {
        _state->Done = true;
    }

    MovieTask::MovieTask(std::function<void()> action, bool asynchronous)
        : MovieTask(std::move(action), asynchronous, {})
    {
    }

    MovieTask::MovieTask(std::function<void()> action, bool asynchronous,
        std::function<bool()> synchronousPrefixDone)
        : _state(std::make_shared<TaskState>())
    {
        auto run = [state = _state, action = std::move(action)]() mutable
        {
            std::exception_ptr exception{};
            try
            {
                action();
            }
            catch (...)
            {
                exception = std::current_exception();
            }
            {
                std::lock_guard<std::mutex> lock(state->Mutex);
                state->Exception = exception;
                state->Done = true;
            }
            state->Condition.notify_all();
        };

        if (asynchronous)
        {
            std::thread(std::move(run)).detach();
            if (synchronousPrefixDone)
            {
                while (!synchronousPrefixDone())
                {
                    {
                        std::lock_guard<std::mutex> lock(_state->Mutex);
                        if (_state->Done)
                        {
                            break;
                        }
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
        }
        else
        {
            run();
        }
    }

    MovieTask::Awaiter MovieTask::GetAwaiter() const noexcept
    {
        return Awaiter(_state);
    }
}

namespace MphRead::Formats
{
    namespace
    {
        [[nodiscard]] std::int32_t ClampInt(std::int32_t value, std::int32_t minimum, std::int32_t maximum) noexcept
        {
            return std::max(minimum, std::min(maximum, value));
        }

        [[nodiscard]] std::int32_t WrapInt32Add(std::int32_t left, std::int32_t right) noexcept
        {
            const std::uint32_t result = std::bit_cast<std::uint32_t>(left)
                + std::bit_cast<std::uint32_t>(right);
            return std::bit_cast<std::int32_t>(result);
        }

        [[nodiscard]] std::int32_t WrapInt32Multiply(std::int32_t left, std::int32_t right) noexcept
        {
            const std::uint32_t result = std::bit_cast<std::uint32_t>(left)
                * std::bit_cast<std::uint32_t>(right);
            return std::bit_cast<std::int32_t>(result);
        }

        [[nodiscard]] std::filesystem::path PathFromUtf8(std::string_view value)
        {
#if defined(__cpp_char8_t)
            std::u8string converted;
            converted.reserve(value.size());
            for (unsigned char ch : value)
            {
                converted.push_back(static_cast<char8_t>(ch));
            }
            return std::filesystem::path(converted);
#else
            return std::filesystem::u8path(value.begin(), value.end());
#endif
        }

        [[nodiscard]] std::string Extension(const std::filesystem::path& path)
        {
#if defined(__cpp_char8_t)
            const std::u8string value = path.extension().u8string();
            return std::string(reinterpret_cast<const char*>(value.data()), value.size());
#else
            return path.extension().u8string();
#endif
        }

        [[nodiscard]] std::string FileName(const std::filesystem::path& path)
        {
#if defined(__cpp_char8_t)
            const std::u8string value = path.filename().u8string();
            return std::string(reinterpret_cast<const char*>(value.data()), value.size());
#else
            return path.filename().u8string();
#endif
        }

        [[nodiscard]] std::string PathUtf8(const std::filesystem::path& path)
        {
#if defined(__cpp_char8_t)
            const std::u8string value = path.u8string();
            return std::string(reinterpret_cast<const char*>(value.data()), value.size());
#else
            return path.u8string();
#endif
        }

        [[nodiscard]] std::string Stem(const std::filesystem::path& path)
        {
#if defined(__cpp_char8_t)
            const std::u8string value = path.stem().u8string();
            return std::string(reinterpret_cast<const char*>(value.data()), value.size());
#else
            return path.stem().u8string();
#endif
        }

        [[nodiscard]] bool FileExists(const std::string& path) noexcept
        {
            try
            {
                return std::filesystem::is_regular_file(PathFromUtf8(path));
            }
            catch (...)
            {
                return false;
            }
        }

        extern "C"
        {
            using StbiWriteFunc = void (*)(void* context, void* data, int size);
            void stbi_flip_vertically_on_write(int flag);
            int stbi_write_png_to_func(StbiWriteFunc func, void* context, int w, int h,
                int comp, const void* data, int stride_in_bytes);
        }

        void StbiStreamWrite(void* context, void* data, int size)
        {
            auto* stream = static_cast<std::ostream*>(context);
            stream->write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
        }

        void WriteRgbPng(const std::string& path, std::span<const std::uint8_t> pixels,
            std::int32_t width, std::int32_t height)
        {
            std::ofstream stream(PathFromUtf8(path), std::ios::binary | std::ios::trunc);
            if (!stream)
            {
                throw std::runtime_error("Could not create PNG output file.");
            }
            stbi_flip_vertically_on_write(0);
            const int result = stbi_write_png_to_func(
                &StbiStreamWrite, &stream, width, height, 3, pixels.data(), width * 3);
            if (result == 0 || !stream)
            {
                throw std::runtime_error("Failed to write PNG output file.");
            }
        }

        template <typename T>
        [[nodiscard]] T WrapFromUnsigned(std::make_unsigned_t<T> value) noexcept
        {
            return std::bit_cast<T>(value);
        }
    }

    SeekTableEntry& SeekTableEntry::operator=(const SeekTableEntry& other) noexcept
    {
        if (this != std::addressof(other))
        {
            this->~SeekTableEntry();
            ::new (static_cast<void*>(this)) SeekTableEntry(other);
        }
        return *this;
    }

    Vector2ir& Vector2ir::operator=(const Vector2ir& other) noexcept
    {
        if (this != std::addressof(other))
        {
            this->~Vector2ir();
            ::new (static_cast<void*>(this)) Vector2ir(other);
        }
        return *this;
    }

    VxBuffers::VxBuffers(
        std::shared_ptr<std::array<std::shared_ptr<VideoFrame>, 3>> prevVideoFrames,
        std::shared_ptr<std::array<std::int32_t, 3>> quantizerTable,
        std::shared_ptr<ByteArray2D> planeBufferY,
        std::shared_ptr<ByteArray2D> planeBufferU,
        std::shared_ptr<ByteArray2D> planeBufferV,
        std::shared_ptr<ByteArray2D> coeffBufferY,
        std::shared_ptr<ByteArray2D> coeffBufferUV,
        std::shared_ptr<Vector2irArray2D> vectors,
        std::shared_ptr<std::array<std::int16_t, 8>> prevSampleBuffer,
        std::shared_ptr<std::array<std::int32_t, 256>> prevPulseBuffer,
        std::shared_ptr<std::array<std::int32_t, 8>> lpcFilterBuffer,
        std::shared_ptr<std::array<std::int32_t, 8>> influenceBuffer,
        std::shared_ptr<std::vector<std::int16_t>> sampleBuffer)
        : PrevVideoFrames(std::move(prevVideoFrames)),
          QuantizerTable(std::move(quantizerTable)),
          PlaneBufferY(std::move(planeBufferY)),
          PlaneBufferU(std::move(planeBufferU)),
          PlaneBufferV(std::move(planeBufferV)),
          CoeffBufferY(std::move(coeffBufferY)),
          CoeffBufferUV(std::move(coeffBufferUV)),
          Vectors(std::move(vectors)),
          PrevSampleBuffer(std::move(prevSampleBuffer)),
          PrevPulseBuffer(std::move(prevPulseBuffer)),
          LpcFilterBuffer(std::move(lpcFilterBuffer)),
          InfluenceBuffer(std::move(influenceBuffer)),
          SampleBuffer(std::move(sampleBuffer))
    {
    }

    VxBuffers& VxBuffers::operator=(const VxBuffers& other) noexcept
    {
        if (this != std::addressof(other))
        {
            this->~VxBuffers();
            ::new (static_cast<void*>(this)) VxBuffers(other);
        }
        return *this;
    }

    Block& Block::operator=(const Block& other) noexcept
    {
        if (this != std::addressof(other))
        {
            this->~Block();
            ::new (static_cast<void*>(this)) Block(other);
        }
        return *this;
    }

    Block Block::HalfLeft() const noexcept
    {
        return Block(X, Y, W / 2, H);
    }

    Block Block::HalfRight() const noexcept
    {
        return Block(X + W / 2, Y, W / 2, H);
    }

    Block Block::HalfUp() const noexcept
    {
        return Block(X, Y, W, H / 2);
    }

    Block Block::HalfDown() const noexcept
    {
        return Block(X, Y + H / 2, W, H / 2);
    }

    const std::array<std::array<std::int32_t, 3>, 6> VxDecoder::_quantizer4x4Table{{
        {{0x0A, 0x0D, 0x10}},
        {{0x0B, 0x0E, 0x12}},
        {{0x0D, 0x10, 0x14}},
        {{0x0E, 0x12, 0x17}},
        {{0x10, 0x14, 0x19}},
        {{0x12, 0x17, 0x1D}}
    }};

    bool VxDecoder::UseStaticBuffers = true;

    VxDecoder::VxDecoder()
    {
        for (std::int32_t frame = 0; frame < 4; ++frame)
        {
            const std::size_t base = static_cast<std::size_t>(frame) * 3U;
            _planeBuffers[base] = std::make_shared<ByteArray2D>(_mphFrameH, _mphFrameW);
            _planeBuffers[base + 1U] = std::make_shared<ByteArray2D>(_mphFrameH / 2, _mphFrameW / 2);
            _planeBuffers[base + 2U] = std::make_shared<ByteArray2D>(_mphFrameH / 2, _mphFrameW / 2);
        }
    }

    namespace
    {
        struct DecoderInstances
        {
            VxDecoder One;
            VxDecoder Two;
        };

        DecoderInstances& GetDecoderInstances()
        {
            static DecoderInstances instances;
            return instances;
        }
    }

    VxDecoder& VxDecoder::Instance1()
    {
        return GetDecoderInstances().One;
    }

    VxDecoder& VxDecoder::Instance2()
    {
        return GetDecoderInstances().Two;
    }

    void VxDecoder::Reset()
    {
        {
            std::lock_guard<std::mutex> lock(_vxFramesMutex);
            if (_vxFrames)
            {
                _vxFrames->clear();
            }
        }
        _framesQueued.store(0, std::memory_order_relaxed);
        UseStaticBuffers = true;
        std::fill(_sampleBuffer->begin(), _sampleBuffer->end(), 0);
    }

    MovieNativeRuntime::MovieTask VxDecoder::ExportAll()
    {
        return MovieNativeRuntime::MovieTask([this]()
        {
            Reset();
            UseStaticBuffers = false;
            std::int32_t i = 0;
            const std::string movieFolder = Paths::Combine(Paths::FileSystem(), "movies");
            std::vector<std::filesystem::path> files;
            for (const std::filesystem::directory_entry& entry
                : std::filesystem::directory_iterator(PathFromUtf8(movieFolder)))
            {
                if (entry.is_regular_file())
                {
                    files.push_back(entry.path());
                }
            }
            for (const std::filesystem::path& path : files)
            {
                if (Extension(path) == ".vx")
                {
                    std::cout << "Exporting " << ++i << " of " << files.size()
                        << ": " << FileName(path) << '\n';
                    Decode(PathUtf8(path), true).GetAwaiter().GetResult();
                }
            }
            std::cout << "Done." << '\n';
        }, false);
    }

    MovieNativeRuntime::MovieTask VxDecoder::Export(const std::string& filePath)
    {
        return MovieNativeRuntime::MovieTask([this, filePath]()
        {
            Reset();
            std::string path = Paths::Combine(Paths::FileSystem(), "movies", filePath);
            if (!FileExists(path))
            {
                path = Paths::Combine(Paths::FileSystem(), filePath);
                if (!FileExists(path))
                {
                    path = filePath;
                }
            }
            std::cout << "Exporting..." << '\n';
            UseStaticBuffers = false;
            Decode(path, true).GetAwaiter().GetResult();
            std::cout << "Done." << '\n';
        }, false);
    }

    MovieNativeRuntime::MovieTask VxDecoder::Decode(
        const std::string& filePath, bool writeFiles, std::stop_token token)
    {
        const std::uint64_t generation = _decodeGeneration.load(std::memory_order_acquire);
        return MovieNativeRuntime::MovieTask([this, filePath, writeFiles, token]()
        {
            std::ifstream stream(PathFromUtf8(filePath), std::ios::binary);
            if (!stream)
            {
                throw std::runtime_error("Could not find file '" + filePath + "'.");
            }
            DecodeCore(stream, FileName(PathFromUtf8(filePath)), writeFiles, token);
        }, !writeFiles, [this, generation]()
        {
            return _decodeGeneration.load(std::memory_order_acquire) != generation
                && _framesQueued.load(std::memory_order_relaxed) >= 4;
        });
    }

    MovieNativeRuntime::MovieTask VxDecoder::Decode(
        std::shared_ptr<std::vector<std::uint8_t>> data, const std::string& filename,
        bool writeFiles, std::stop_token token)
    {
        if (!data)
        {
            throw System::NullReferenceException();
        }
        const std::uint64_t generation = _decodeGeneration.load(std::memory_order_acquire);
        return MovieNativeRuntime::MovieTask(
            [this, data = std::move(data), filename, writeFiles, token]()
            {
                std::string bytes(reinterpret_cast<const char*>(data->data()), data->size());
                std::istringstream stream(bytes, std::ios::binary);
                DecodeCore(stream, filename, writeFiles, token);
            }, !writeFiles, [this, generation]()
            {
                return _decodeGeneration.load(std::memory_order_acquire) != generation
                    && _framesQueued.load(std::memory_order_relaxed) >= 4;
            });
    }

    MovieNativeRuntime::MovieTask VxDecoder::Decode(
        std::shared_ptr<std::istream> stream, const std::string& filename,
        bool writeFiles, std::stop_token token)
    {
        if (!stream)
        {
            throw System::NullReferenceException();
        }
        const std::uint64_t generation = _decodeGeneration.load(std::memory_order_acquire);
        return MovieNativeRuntime::MovieTask(
            [this, stream = std::move(stream), filename, writeFiles, token]()
            {
                DecodeCore(*stream, filename, writeFiles, token);
            }, !writeFiles, [this, generation]()
            {
                return _decodeGeneration.load(std::memory_order_acquire) != generation
                    && _framesQueued.load(std::memory_order_relaxed) >= 4;
            });
    }

    void VxDecoder::DecodeCore(
        std::istream& stream, const std::string& filename, bool writeFiles, std::stop_token token)
    {
        const std::string folder = Paths::Combine(Paths::Export(), Stem(PathFromUtf8(filename)));
        if (writeFiles)
        {
            std::filesystem::create_directories(PathFromUtf8(folder));
        }

        MovieNativeRuntime::BinaryReader reader(stream);
        _nextPlaneBufferIndex = 0;
        _nextSampleBufferIndex = 0;
        _framesQueued.store(0, std::memory_order_relaxed);
        _decodeGeneration.fetch_add(1, std::memory_order_release);

        (*Magic)[0] = reader.ReadChar();
        (*Magic)[1] = reader.ReadChar();
        (*Magic)[2] = reader.ReadChar();
        (*Magic)[3] = reader.ReadChar();
        FrameCount = reader.ReadInt32();
        FrameWidth = reader.ReadInt32();
        FrameHeight = reader.ReadInt32();
        FrameRate = static_cast<double>(reader.ReadInt32()) / 65536.0;
        Quantizer = reader.ReadInt32();
        AudioSampleRate = reader.ReadInt32();
        AudioStreamCount = reader.ReadInt32();
        if (AudioStreamCount > 1)
        {
            throw ProgramException("VX decoding error 022: " + std::to_string(AudioStreamCount));
        }
        MaxDataSize = reader.ReadInt32();
        MaxDataSize = WrapInt32Add(MaxDataSize, -2);
        assert(MaxDataSize % 2 == 0);
        ExtradataOffset = reader.ReadInt32();
        SeekTableOffset = reader.ReadInt32();
        SeekTableCount = reader.ReadInt32();

        if (FrameWidth % 16 != 0 || FrameHeight % 16 != 0)
        {
            throw ProgramException("VX decoding error 001: "
                + std::to_string(FrameWidth) + " x " + std::to_string(FrameHeight));
        }

        const std::int64_t prevPosition = reader.Position();
        reader.Position(ExtradataOffset);

        for (std::int32_t i = 0; i < 3; ++i)
        {
            for (std::int32_t j = 0; j < 64; ++j)
            {
                for (std::int32_t k = 0; k < 8; ++k)
                {
                    (*Extradata->LpcCodebooks)(i, j, k) = reader.ReadInt16();
                }
            }
        }

        for (std::int32_t i = 0; i < 8; ++i)
        {
            (*Extradata->ScaleModifiers)[static_cast<std::size_t>(i)] = reader.ReadUInt16();
        }
        for (std::int32_t i = 0; i < 8; ++i)
        {
            (*Extradata->LpcBase)[static_cast<std::size_t>(i)] = reader.ReadInt32();
        }
        Extradata->ScaleInitial = reader.ReadInt32();

        reader.Position(SeekTableOffset);
        for (std::int32_t i = 0; i < SeekTableCount; ++i)
        {
            (*SeekTable)[0] = SeekTableEntry(reader.ReadInt32(), reader.ReadInt32());
        }

        if (Quantizer < 12 || Quantizer > 161)
        {
            throw ProgramException("VX decoding error 002: " + std::to_string(Quantizer));
        }
        const std::int32_t qy = Quantizer / 6;
        const std::int32_t qx = Quantizer % 6;
        const std::array<std::int32_t, 3>& table = _quantizer4x4Table.at(static_cast<std::size_t>(qx));
        for (std::size_t i = 0; i < table.size(); ++i)
        {
            (*QuantizerTable)[i] = table[i] << qy;
        }

        reader.Position(prevPosition);

        (void)VLC::Temp();

        std::shared_ptr<std::vector<std::uint8_t>> buffer = GetDataBuffer();
        std::fill(buffer->begin(), buffer->end(), 0);

        std::shared_ptr<ByteArray2D> coeffBufferY;
        std::shared_ptr<ByteArray2D> coeffBufferUV;
        std::shared_ptr<Vector2irArray2D> vectors;
        if (UseStaticBuffers)
        {
            coeffBufferY = _coeffBufferY;
            coeffBufferUV = _coeffBufferUV;
            vectors = _vectors;
        }
        else
        {
            coeffBufferY = std::make_shared<ByteArray2D>(FrameHeight / 4 + 1, FrameWidth / 4 + 1);
            coeffBufferUV = std::make_shared<ByteArray2D>(FrameHeight / 8 + 1, FrameWidth / 8 + 1);
            vectors = std::make_shared<Vector2irArray2D>(FrameHeight / 16 + 1, FrameWidth / 16 + 2);
        }

        if (FrameCount < 0)
        {
            throw std::out_of_range("Non-negative number required. (Parameter 'capacity')");
        }
        {
            std::lock_guard<std::mutex> lock(_vxFramesMutex);
            _vxFrames = std::make_shared<std::vector<std::shared_ptr<VxFrame>>>();
            _vxFrames->reserve(static_cast<std::size_t>(FrameCount));
        }
        (*_prevVideoFrames)[0].reset();
        (*_prevVideoFrames)[1].reset();
        (*_prevVideoFrames)[2].reset();
        _prevSampleBuffer->fill(0);
        _prevPulseBuffer->fill(0);
        _lpcFilterBuffer->fill(0);
        _influenceBuffer->fill(0);
        std::shared_ptr<AudioFrame> prevAudioFrame{};
        _audioFrameTotal.store(0, std::memory_order_release);

        std::ofstream waveFile;
        std::ostringstream nullFile;
        std::ostream* output = &nullFile;
        if (writeFiles && AudioStreamCount >= 1)
        {
            waveFile.open(PathFromUtf8(Paths::Combine(folder, "audio.wav")),
                std::ios::binary | std::ios::trunc);
            if (!waveFile)
            {
                throw std::runtime_error("Could not create audio.wav.");
            }
            output = &waveFile;
        }
        Sound::BinaryWriter writer(*output);
        std::vector<std::uint8_t> fileOutputBuffer;
        if (writeFiles)
        {
            const std::size_t size = static_cast<std::size_t>(FrameWidth)
                * static_cast<std::size_t>(FrameHeight) * 3U;
            fileOutputBuffer.resize(size);
        }

        for (std::int32_t i = 0; i < 11; ++i)
        {
            writer.Write(static_cast<std::int32_t>(0));
        }

        for (std::int32_t i = 0; i < FrameCount; ++i)
        {
            while (!writeFiles && _framesQueued.load(std::memory_order_relaxed) >= 4 && !token.stop_requested())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            if (token.stop_requested())
            {
                return;
            }

            std::int32_t dataSize = reader.ReadUInt16();
            dataSize -= 2;
            assert(dataSize % 2 == 0);
            assert(dataSize <= MaxDataSize);
            const std::int32_t audioFrameCount = reader.ReadUInt16();

            std::shared_ptr<ByteArray2D> planeBufferY{};
            std::shared_ptr<ByteArray2D> planeBufferU{};
            std::shared_ptr<ByteArray2D> planeBufferV{};
            if (UseStaticBuffers)
            {
                auto planeBuffers = GetPlaneBuffers();
                planeBufferY = std::move(planeBuffers[0]);
                planeBufferU = std::move(planeBuffers[1]);
                planeBufferV = std::move(planeBuffers[2]);
            }

            coeffBufferY->Clear();
            coeffBufferUV->Clear();
            vectors->Clear();

            VxBuffers buffers(
                _prevVideoFrames, QuantizerTable,
                planeBufferY, planeBufferU, planeBufferV,
                coeffBufferY, coeffBufferUV, vectors,
                _prevSampleBuffer, _prevPulseBuffer,
                _lpcFilterBuffer, _influenceBuffer, _sampleBuffer);

            auto vxFrame = std::make_shared<VxFrame>(
                FrameWidth, FrameHeight, audioFrameCount,
                *Extradata, prevAudioFrame, buffers, _nextSampleBufferIndex);
            vxFrame->Decode(reader, buffer, dataSize);

            if (audioFrameCount > 0)
            {
                prevAudioFrame = vxFrame->AudioFrames->back();
                _nextSampleBufferIndex = WrapInt32Add(_nextSampleBufferIndex, audioFrameCount);
                _nextSampleBufferIndex %= SampleBufferCount();
            }

            const std::int32_t audioFrameTotal = WrapInt32Add(
                _audioFrameTotal.load(std::memory_order_relaxed), audioFrameCount);
            _audioFrameTotal.store(audioFrameTotal, std::memory_order_release);
            {
                std::lock_guard<std::mutex> lock(_vxFramesMutex);
                _vxFrames->push_back(vxFrame);
            }
            const std::int32_t queued = _framesQueued.load(std::memory_order_relaxed);
            _framesQueued.store(WrapInt32Add(queued, 1), std::memory_order_relaxed);
            (*_prevVideoFrames)[2] = (*_prevVideoFrames)[1];
            (*_prevVideoFrames)[1] = (*_prevVideoFrames)[0];
            (*_prevVideoFrames)[0] = vxFrame->VideoFrame;

            if (writeFiles && UseStaticBuffers)
            {
                WriteFile(fileOutputBuffer, *vxFrame, folder, i);
            }

            if (writeFiles && audioFrameCount > 0)
            {
                for (std::int32_t j = 0; j < audioFrameCount; ++j)
                {
                    const std::shared_ptr<AudioFrame>& audioFrame
                        = vxFrame->AudioFrames->at(static_cast<std::size_t>(j));
                    const std::span<const std::int16_t> samples = audioFrame->SampleBuffer();
                    for (std::int32_t k = 0; k < 128; ++k)
                    {
                        const std::uint16_t sample
                            = std::bit_cast<std::uint16_t>(samples[static_cast<std::size_t>(k)]);
                        writer.Write(static_cast<std::uint8_t>(sample & 0xFFU));
                        writer.Write(static_cast<std::uint8_t>((sample >> 8) & 0xFFU));
                    }
                }
            }
        }

        if (writeFiles && _audioFrameTotal.load(std::memory_order_acquire) > 0)
        {
            output->clear();
            output->seekp(0, std::ios::beg);
            Sound::SoundRead::WriteWavHeader(
                writer,
                static_cast<std::uint32_t>(_audioFrameTotal.load(std::memory_order_relaxed)) * 128U,
                static_cast<std::uint16_t>(AudioSampleRate),
                WaveFormat::PCM16);
        }

        if (writeFiles && !UseStaticBuffers)
        {
            std::shared_ptr<std::vector<std::shared_ptr<VxFrame>>> frames;
            {
                std::lock_guard<std::mutex> lock(_vxFramesMutex);
                frames = _vxFrames;
            }
            std::int32_t frame = 0;
            for (const std::shared_ptr<VxFrame>& vxFrame : *frames)
            {
                WriteFile(fileOutputBuffer, *vxFrame, folder, frame++);
            }
        }
    }

    void VxDecoder::WriteFile(
        std::span<std::uint8_t> pixelBuffer, const VxFrame& vxFrame,
        const std::string& folder, std::int32_t frameIndex)
    {
        const VideoFrame& videoFrame = *vxFrame.VideoFrame;
        for (std::int32_t y = 0; y < FrameHeight; ++y)
        {
            for (std::int32_t x = 0; x < FrameWidth; ++x)
            {
                const std::int32_t cy = (*videoFrame.PlaneBufferY())(y, x);
                const std::int32_t cu = (*videoFrame.PlaneBufferU())(y / 2, x / 2);
                const std::int32_t cv = (*videoFrame.PlaneBufferV())(y / 2, x / 2);
                const ColorRgb rgb = YuvToRgb(cy, cu, cv);
                std::size_t index = (static_cast<std::size_t>(y)
                    * static_cast<std::size_t>(FrameWidth)
                    + static_cast<std::size_t>(x)) * 3U;
                pixelBuffer[index++] = rgb.Red;
                pixelBuffer[index++] = rgb.Green;
                pixelBuffer[index] = rgb.Blue;
            }
        }

        std::ostringstream name;
        name << std::setw(4) << std::setfill('0') << frameIndex << ".png";
        WriteRgbPng(Paths::Combine(folder, name.str()), pixelBuffer, FrameWidth, FrameHeight);
    }

    bool VxDecoder::GetImage(std::int32_t frameIndex, std::span<std::uint8_t> texture)
    {
        std::shared_ptr<VxFrame> vxFrame;
        {
            std::lock_guard<std::mutex> lock(_vxFramesMutex);
            if (!_vxFrames || frameIndex >= static_cast<std::int32_t>(_vxFrames->size()))
            {
                return false;
            }
            vxFrame = _vxFrames->at(static_cast<std::size_t>(frameIndex));
        }
        for (std::int32_t y = 0; y < FrameHeight; ++y)
        {
            for (std::int32_t x = 0; x < FrameWidth; ++x)
            {
                const std::int32_t cy = (*vxFrame->VideoFrame->PlaneBufferY())(y, x);
                const std::int32_t cu = (*vxFrame->VideoFrame->PlaneBufferU())(y / 2, x / 2);
                const std::int32_t cv = (*vxFrame->VideoFrame->PlaneBufferV())(y / 2, x / 2);
                const ColorRgb rgb = YuvToRgb(cy, cu, cv);
                const std::size_t index = static_cast<std::size_t>(y) * 256U * 3U
                    + static_cast<std::size_t>(x) * 3U;
                if (index + 2U >= texture.size())
                {
                    throw std::out_of_range("Index was outside the bounds of the array.");
                }
                texture[index] = rgb.Red;
                texture[index + 1U] = rgb.Green;
                texture[index + 2U] = rgb.Blue;
            }
        }
        const std::int32_t queued = _framesQueued.load(std::memory_order_relaxed);
        _framesQueued.store(WrapInt32Add(queued, -1), std::memory_order_relaxed);
        return true;
    }

    std::span<const std::int16_t> VxDecoder::GetAudioBuffer(std::int32_t index) const
    {
        index %= SampleBufferCount();
        const std::int64_t start = static_cast<std::int64_t>(128) * index;
        if (start < 0 || start + 128 > static_cast<std::int64_t>(_sampleBuffer->size()))
        {
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        }
        return std::span<const std::int16_t>(
            _sampleBuffer->data() + static_cast<std::ptrdiff_t>(start), 128);
    }

    std::shared_ptr<std::vector<std::uint8_t>> VxDecoder::GetDataBuffer()
    {
        if (UseStaticBuffers || MaxDataSize <= _mphMaxDataSize)
        {
            return _dataBuffer;
        }
        return std::make_shared<std::vector<std::uint8_t>>(static_cast<std::size_t>(MaxDataSize));
    }

    std::array<std::shared_ptr<ByteArray2D>, 3> VxDecoder::GetPlaneBuffers()
    {
        std::shared_ptr<ByteArray2D> bufferY
            = _planeBuffers.at(static_cast<std::size_t>(_nextPlaneBufferIndex++));
        std::shared_ptr<ByteArray2D> bufferU
            = _planeBuffers.at(static_cast<std::size_t>(_nextPlaneBufferIndex++));
        std::shared_ptr<ByteArray2D> bufferV
            = _planeBuffers.at(static_cast<std::size_t>(_nextPlaneBufferIndex++));
        _nextPlaneBufferIndex %= static_cast<std::int32_t>(_planeBuffers.size());

        // Preserve the source exactly: V is cleared twice; Y is not cleared here.
        bufferV->Clear();
        bufferU->Clear();
        bufferV->Clear();
        return {std::move(bufferY), std::move(bufferU), std::move(bufferV)};
    }

    ColorRgb VxDecoder::YuvToRgb(std::int32_t y, std::int32_t u, std::int32_t v) noexcept
    {
        u -= 128;
        v -= 128;
        const std::int32_t r = y + 2 * v;
        const std::int32_t g = y - u / 2 - v;
        const std::int32_t b = y + 2 * u;
        return ColorRgb(
            static_cast<std::uint8_t>(ClampInt(r, 0, 255)),
            static_cast<std::uint8_t>(ClampInt(g, 0, 255)),
            static_cast<std::uint8_t>(ClampInt(b, 0, 255)));
    }

    VxFrame::VxFrame(
        std::int32_t frameWidth, std::int32_t frameHeight, std::int32_t audioFrameCount,
        AudioExtradata& extradata, std::shared_ptr<AudioFrame> prevAudioFrame,
        const VxBuffers& buffers, std::int32_t sampleBufferIndex)
        : VideoFrame(std::make_shared<::MphRead::Formats::VideoFrame>(
            frameWidth, frameHeight, buffers)),
          AudioFrameCount(audioFrameCount),
          AudioFrames([&extradata, prevAudioFrame = std::move(prevAudioFrame),
              &buffers, sampleBufferIndex, audioFrameCount]() mutable
          {
              if (audioFrameCount < 0)
              {
                  throw std::overflow_error("Array dimensions exceeded supported range.");
              }
              auto frames = std::make_shared<
                  std::vector<std::shared_ptr<::MphRead::Formats::AudioFrame>>>(
                      static_cast<std::size_t>(audioFrameCount));
              for (std::int32_t i = 0; i < audioFrameCount; ++i)
              {
                  auto audioFrame = std::make_shared<::MphRead::Formats::AudioFrame>(
                      extradata, prevAudioFrame, buffers, sampleBufferIndex);
                  sampleBufferIndex = WrapInt32Add(sampleBufferIndex, 1);
                  (*frames)[static_cast<std::size_t>(i)] = audioFrame;
                  prevAudioFrame = std::move(audioFrame);
                  sampleBufferIndex %= VxDecoder::SampleBufferCount();
              }
              return frames;
          }())
    {
    }

    void VxFrame::Decode(
        MovieNativeRuntime::BinaryReader& reader,
        const std::shared_ptr<std::vector<std::uint8_t>>& buffer,
        std::int32_t length)
    {
        for (std::int32_t i = 0; i < length; i += 2)
        {
            buffer->at(static_cast<std::size_t>(i + 1)) = reader.ReadByte();
            buffer->at(static_cast<std::size_t>(i)) = reader.ReadByte();
        }
        BitStreamReader bitReader(buffer, length);
        VideoFrame->Decode(bitReader);
        for (std::int32_t i = 0; i < AudioFrameCount; ++i)
        {
            AudioFrames->at(static_cast<std::size_t>(i))->Decode(bitReader);
        }
    }

    BitStreamReader::BitStreamReader(
        std::shared_ptr<std::vector<std::uint8_t>> buffer, std::int32_t length)
        : _buffer(std::move(buffer)), _length(length)
    {
    }

    std::int32_t BitStreamReader::ReadBit()
    {
        if (!_buffer)
        {
            throw System::NullReferenceException();
        }
        const std::int32_t bytePosition = _bitPosition / 8;
        const std::int32_t bitPosition = _bitPosition % 8;
        _bitPosition = WrapInt32Add(_bitPosition, 1);
        return (_buffer->at(static_cast<std::size_t>(bytePosition))
            >> (7 - bitPosition)) & 1;
    }

    std::int32_t BitStreamReader::ConsumeUntilNotZero()
    {
        std::int32_t count = 0;
        while (ReadBit() == 0)
        {
            count = WrapInt32Add(count, 1);
        }
        return count;
    }

    std::int32_t BitStreamReader::ReadUnsignedExpGolomb()
    {
        const std::int32_t zeroCount = ConsumeUntilNotZero();
        assert(zeroCount <= 30);
        std::int32_t value = 1 << zeroCount;
        for (std::int32_t i = 0; i < zeroCount; ++i)
        {
            value |= ReadBit() << (zeroCount - i - 1);
        }
        return value - 1;
    }

    std::int32_t BitStreamReader::ReadSignedExpGolomb()
    {
        const std::int32_t value = ReadUnsignedExpGolomb() + 1;
        return (value >> 1) * ((value & 1) * -2 + 1);
    }

    std::int32_t BitStreamReader::ReadInt(std::int32_t bitCount)
    {
        assert(bitCount >= 0 && bitCount <= 32);
        std::uint32_t value = 0;
        for (std::int32_t i = 0; i < bitCount; ++i)
        {
            value |= static_cast<std::uint32_t>(ReadBit()) << (bitCount - i - 1);
        }
        return std::bit_cast<std::int32_t>(value);
    }

    std::int32_t BitStreamReader::ReadVLC2(const VLCData& vlc)
    {
        std::int32_t bitCount = 1;
        std::int32_t hashCode = MovieNativeRuntime::HashCombine(0, ReadBit());
        std::int32_t index = vlc.FindBitPattern(hashCode);
        while (index == -1)
        {
            assert(bitCount < vlc.MaxBitCount());
            ++bitCount;
            hashCode = MovieNativeRuntime::HashCombine(hashCode, ReadBit());
            index = vlc.FindBitPattern(hashCode);
        }
        return index;
    }

    void BitStreamReader::EnsureWordAlignment()
    {
        while (_bitPosition % 16 != 0)
        {
            _bitPosition = WrapInt32Add(_bitPosition, 1);
        }
    }

    const std::array<std::int32_t, 32> VideoFrame::_residueMaskTable{
        0x00, 0x08, 0x04, 0x02, 0x01, 0x1F, 0x0F, 0x0A,
        0x05, 0x0C, 0x03, 0x10, 0x0E, 0x0D, 0x0B, 0x07,
        0x09, 0x06, 0x1E, 0x1B, 0x1A, 0x1D, 0x17, 0x15,
        0x18, 0x12, 0x11, 0x1C, 0x14, 0x13, 0x16, 0x19
    };

    const std::array<std::int32_t, 17> VideoFrame::_tokenIndexTable{
        0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3
    };

    const std::array<std::int32_t, 7> VideoFrame::_suffixLimits{
        0, 3, 6, 12, 24, 48, 0x8000
    };

    const std::array<std::int32_t, 16> VideoFrame::_zigzagScanTable{
        0 * 4 + 0, 1 * 4 + 0, 0 * 4 + 1, 0 * 4 + 2,
        1 * 4 + 1, 2 * 4 + 0, 3 * 4 + 0, 2 * 4 + 1,
        1 * 4 + 2, 0 * 4 + 3, 1 * 4 + 3, 2 * 4 + 2,
        3 * 4 + 1, 3 * 4 + 2, 2 * 4 + 3, 3 * 4 + 3
    };

    VideoFrame::VideoFrame(
        std::int32_t frameWidth, std::int32_t frameHeight, const VxBuffers& buffers)
        : FrameWidth(frameWidth),
          FrameHeight(frameHeight),
          _planeBufferY(buffers.PlaneBufferY),
          _planeBufferU(buffers.PlaneBufferU),
          _planeBufferV(buffers.PlaneBufferV),
          _coeffBufferY(buffers.CoeffBufferY),
          _coeffBufferUV(buffers.CoeffBufferUV),
          _vectors(buffers.Vectors),
          _prevVideoFrames(buffers.PrevVideoFrames),
          _quantizerTable(buffers.QuantizerTable)
    {
    }

    void VideoFrame::Decode(BitStreamReader& reader)
    {
        _reader = &reader;

        if (!_planeBufferY)
        {
            _planeBufferY = std::make_shared<ByteArray2D>(FrameHeight, FrameWidth);
            _planeBufferU = std::make_shared<ByteArray2D>(FrameHeight / 2, FrameWidth / 2);
            _planeBufferV = std::make_shared<ByteArray2D>(FrameHeight / 2, FrameWidth / 2);
        }

        for (std::int32_t y = 0; y < FrameHeight; y += 16)
        {
            for (std::int32_t x = 0; x < FrameWidth; x += 16)
            {
                const Vector2ir predictionVector(
                    GetMiddleValue(
                        (*_vectors)((y / 16) + 1, (x / 16) + 0).X,
                        (*_vectors)((y / 16) + 0, (x / 16) + 1).X,
                        (*_vectors)((y / 16) + 0, (x / 16) + 2).X),
                    GetMiddleValue(
                        (*_vectors)((y / 16) + 1, (x / 16) + 0).Y,
                        (*_vectors)((y / 16) + 0, (x / 16) + 1).Y,
                        (*_vectors)((y / 16) + 0, (x / 16) + 2).Y));
                DecodeBlock(Block(x, y, 16, 16), predictionVector);
            }
        }

        _reader->EnsureWordAlignment();
        _reader = nullptr;
    }

    std::int32_t VideoFrame::GetMiddleValue(
        std::int32_t a, std::int32_t b, std::int32_t c) noexcept
    {
        std::array<std::int32_t, 3> array{a, b, c};
        std::sort(array.begin(), array.end());
        return array[1];
    }

    std::uint8_t VideoFrame::PlaneBufferGetter(
        const ByteArray2D& planeBuffer, std::int32_t step, std::int32_t x, std::int32_t y)
    {
        return planeBuffer(y / step, x / step);
    }

    void VideoFrame::DecodeBlock(Block block, Vector2ir predictionVector)
    {
        const std::int32_t mode = _reader->ReadUnsignedExpGolomb();

        if (mode == 0)
        {
            if (block.W == 2)
            {
                throw ProgramException("VX decoding error 003");
            }
            DecodeBlock(block.HalfLeft(), predictionVector);
            DecodeBlock(block.HalfRight(), predictionVector);
            if (block.W == 8 && block.H >= 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 1)
        {
            PredictInter(block, predictionVector, false, (*_prevVideoFrames)[0]);
            if (block.W >= 8 && block.H >= 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 2)
        {
            if (block.H == 2)
            {
                throw ProgramException("VX decoding error 004");
            }
            DecodeBlock(block.HalfUp(), predictionVector);
            DecodeBlock(block.HalfDown(), predictionVector);
            if (block.W >= 8 && block.H == 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 3)
        {
            PredictInterDC(block);
            if (block.W >= 8 && block.H >= 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 4)
        {
            PredictInter(block, predictionVector, true, (*_prevVideoFrames)[0]);
            if (block.W >= 8 && block.H >= 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 5)
        {
            PredictInter(block, predictionVector, true, (*_prevVideoFrames)[1]);
            if (block.W >= 8 && block.H >= 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 6)
        {
            PredictInter(block, predictionVector, true, (*_prevVideoFrames)[2]);
            if (block.W >= 8 && block.H >= 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 7)
        {
            PredictMBPlane(block);
            if (block.W >= 8 && block.H >= 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 8)
        {
            if (block.W == 2)
            {
                throw ProgramException("VX decoding error 005");
            }
            DecodeBlock(block.HalfLeft(), predictionVector);
            DecodeBlock(block.HalfRight(), predictionVector);
            DecodeResidueBlocks(block);
        }
        else if (mode == 9)
        {
            PredictInter(block, predictionVector, false, (*_prevVideoFrames)[1]);
            if (block.W >= 8 && block.H >= 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 10)
        {
            PredictInterDC(block);
            DecodeResidueBlocks(block);
        }
        else if (mode == 11)
        {
            PredictNoTile(block);
            if (block.W >= 8 && block.H >= 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 12)
        {
            PredictInter(block, predictionVector, false, (*_prevVideoFrames)[0]);
            DecodeResidueBlocks(block);
        }
        else if (mode == 13)
        {
            if (block.H == 2)
            {
                throw ProgramException("VX decoding error 006");
            }
            DecodeBlock(block.HalfUp(), predictionVector);
            DecodeBlock(block.HalfDown(), predictionVector);
            DecodeResidueBlocks(block);
        }
        else if (mode == 14)
        {
            PredictInter(block, predictionVector, false, (*_prevVideoFrames)[2]);
            if (block.W >= 8 && block.H >= 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 15)
        {
            Predict4(block);
            if (block.W >= 8 && block.H >= 8)
            {
                ClearTotalCoeff(block);
            }
        }
        else if (mode == 16)
        {
            PredictInter(block, predictionVector, true, (*_prevVideoFrames)[0]);
            DecodeResidueBlocks(block);
        }
        else if (mode == 17)
        {
            PredictInter(block, predictionVector, true, (*_prevVideoFrames)[1]);
            DecodeResidueBlocks(block);
        }
        else if (mode == 18)
        {
            PredictInter(block, predictionVector, true, (*_prevVideoFrames)[2]);
            DecodeResidueBlocks(block);
        }
        else if (mode == 19)
        {
            Predict4(block);
            DecodeResidueBlocks(block);
        }
        else if (mode == 20)
        {
            PredictInter(block, predictionVector, false, (*_prevVideoFrames)[1]);
            DecodeResidueBlocks(block);
        }
        else if (mode == 21)
        {
            PredictInter(block, predictionVector, false, (*_prevVideoFrames)[2]);
            DecodeResidueBlocks(block);
        }
        else if (mode == 22)
        {
            PredictNoTile(block);
            DecodeResidueBlocks(block);
        }
        else if (mode == 23)
        {
            PredictMBPlane(block);
            DecodeResidueBlocks(block);
        }
        else
        {
            throw ProgramException("VX decoding error 007: " + std::to_string(mode));
        }
    }

    void VideoFrame::PredictInter(
        Block block, Vector2ir predictionVector, bool hasDelta,
        const std::shared_ptr<VideoFrame>& prevVideoFrame)
    {
        assert(prevVideoFrame);
        if (!prevVideoFrame)
        {
            throw System::NullReferenceException();
        }

        if (hasDelta)
        {
            predictionVector = Vector2ir(
                predictionVector.X + _reader->ReadSignedExpGolomb(),
                predictionVector.Y + _reader->ReadSignedExpGolomb());
        }

        (*_vectors)((block.Y / 16) + 1, (block.X / 16) + 1) = predictionVector;

        for (std::int32_t y = block.Y; y < block.Y + block.H; ++y)
        {
            for (std::int32_t x = block.X; x < block.X + block.W; ++x)
            {
                (*_planeBufferY)(y, x) = PlaneBufferGetter(
                    *prevVideoFrame->_planeBufferY, 1,
                    x + predictionVector.X, y + predictionVector.Y);
            }
        }
        for (std::int32_t y = block.Y; y < block.Y + block.H; y += 2)
        {
            for (std::int32_t x = block.X; x < block.X + block.W; x += 2)
            {
                (*_planeBufferU)(y / 2, x / 2) = PlaneBufferGetter(
                    *prevVideoFrame->_planeBufferU, 2,
                    x + predictionVector.X, y + predictionVector.Y);
            }
        }
        for (std::int32_t y = block.Y; y < block.Y + block.H; y += 2)
        {
            for (std::int32_t x = block.X; x < block.X + block.W; x += 2)
            {
                (*_planeBufferV)(y / 2, x / 2) = PlaneBufferGetter(
                    *prevVideoFrame->_planeBufferV, 2,
                    x + predictionVector.X, y + predictionVector.Y);
            }
        }
    }

    void VideoFrame::PredictInterDC(Block block)
    {
        const Vector2ir vec(_reader->ReadSignedExpGolomb(), _reader->ReadSignedExpGolomb());

        if (block.X + vec.X < 0 || block.X + vec.X + block.W > FrameWidth
            || block.Y + vec.Y < 0 || block.Y + vec.Y + block.H > FrameHeight)
        {
            throw ProgramException("VX decoding error 008");
        }

        std::int32_t dcY = _reader->ReadSignedExpGolomb();
        if (dcY < -(1 << 16) || dcY >= (1 << 16))
        {
            throw ProgramException("VX decoding error 009");
        }
        dcY *= 2;

        std::int32_t dcU = _reader->ReadSignedExpGolomb();
        if (dcU < -(1 << 16) || dcU >= (1 << 16))
        {
            throw ProgramException("VX decoding error 010");
        }
        dcU *= 2;

        std::int32_t dcV = _reader->ReadSignedExpGolomb();
        if (dcV < -(1 << 16) || dcV >= (1 << 16))
        {
            throw ProgramException("VX decoding error 011");
        }
        dcV *= 2;

        const std::shared_ptr<VideoFrame>& prevVideoFrame = (*_prevVideoFrames)[0];
        assert(prevVideoFrame);

        for (std::int32_t y = block.Y; y < block.Y + block.H; ++y)
        {
            for (std::int32_t x = block.X; x < block.X + block.W; ++x)
            {
                const std::int32_t pixel = PlaneBufferGetter(
                    *prevVideoFrame->_planeBufferY, 1, x + vec.X, y + vec.Y) + dcY;
                (*_planeBufferY)(y, x) = static_cast<std::uint8_t>(ClampInt(pixel, 0, 255));
            }
        }
        for (std::int32_t y = block.Y; y < block.Y + block.H; y += 2)
        {
            for (std::int32_t x = block.X; x < block.X + block.W; x += 2)
            {
                const std::int32_t pixel = PlaneBufferGetter(
                    *prevVideoFrame->_planeBufferU, 2, x + vec.X, y + vec.Y) + dcU;
                (*_planeBufferU)(y / 2, x / 2) = static_cast<std::uint8_t>(ClampInt(pixel, 0, 255));
            }
        }
        for (std::int32_t y = block.Y; y < block.Y + block.H; y += 2)
        {
            for (std::int32_t x = block.X; x < block.X + block.W; x += 2)
            {
                const std::int32_t pixel = PlaneBufferGetter(
                    *prevVideoFrame->_planeBufferV, 2, x + vec.X, y + vec.Y) + dcV;
                (*_planeBufferV)(y / 2, x / 2) = static_cast<std::uint8_t>(ClampInt(pixel, 0, 255));
            }
        }
    }

    void VideoFrame::PredictMBPlane(Block block)
    {
        std::int32_t value = _reader->ReadSignedExpGolomb();
        if (value < -(1 << 16) || value >= (1 << 16))
        {
            throw ProgramException("VX decoding error 012: " + std::to_string(value));
        }
        PredictPlane(block, *_planeBufferY, 1, value * 2);

        value = _reader->ReadSignedExpGolomb();
        if (value < -(1 << 16) || value >= (1 << 16))
        {
            throw ProgramException("VX decoding error 013: " + std::to_string(value));
        }
        PredictPlane(block, *_planeBufferU, 2, value * 2);

        value = _reader->ReadSignedExpGolomb();
        if (value < -(1 << 16) || value >= (1 << 16))
        {
            throw ProgramException("VX decoding error 014: " + std::to_string(value));
        }
        PredictPlane(block, *_planeBufferV, 2, value * 2);
    }

    void VideoFrame::DecodeResidueBlocks(Block block)
    {
        for (std::int32_t y = 0; y < block.H; y += 8)
        {
            for (std::int32_t x = 0; x < block.W; x += 8)
            {
                const std::int32_t index = _reader->ReadUnsignedExpGolomb();
                if (index > 31)
                {
                    throw ProgramException("VX decoding error 015: " + std::to_string(index));
                }
                const std::int32_t residueMask = _residueMaskTable.at(static_cast<std::size_t>(index));

                if ((residueMask & 1) != 0)
                {
                    const std::int32_t coeffLeft = GetCoeffBuffer(
                        *_coeffBufferY, 1, block.X + x - 1, block.Y + y);
                    const std::int32_t coeffTop = GetCoeffBuffer(
                        *_coeffBufferY, 1, block.X + x, block.Y + y - 1);
                    const std::int32_t nc = (coeffLeft + coeffTop + 1) / 2;
                    const std::int32_t outTotalCoeff = DecodeResidueCAVLC(
                        block.X + x, block.Y + y, nc, *_planeBufferY, 1);
                    SetCoeffBuffer(*_coeffBufferY, 1, block.X + x, block.Y + y,
                        static_cast<std::uint8_t>(outTotalCoeff));
                }
                else
                {
                    SetCoeffBuffer(*_coeffBufferY, 1, block.X + x, block.Y + y, 0);
                }

                if ((residueMask & 2) != 0)
                {
                    const std::int32_t coeffLeft = GetCoeffBuffer(
                        *_coeffBufferY, 1, block.X + x + 4 - 1, block.Y + y);
                    const std::int32_t coeffTop = GetCoeffBuffer(
                        *_coeffBufferY, 1, block.X + x + 4, block.Y + y - 1);
                    const std::int32_t nc = (coeffLeft + coeffTop + 1) / 2;
                    const std::int32_t outTotalCoeff = DecodeResidueCAVLC(
                        block.X + x + 4, block.Y + y, nc, *_planeBufferY, 1);
                    SetCoeffBuffer(*_coeffBufferY, 1, block.X + x + 4, block.Y + y,
                        static_cast<std::uint8_t>(outTotalCoeff));
                }
                else
                {
                    SetCoeffBuffer(*_coeffBufferY, 1, block.X + x + 4, block.Y + y, 0);
                }

                if ((residueMask & 4) != 0)
                {
                    const std::int32_t coeffLeft = GetCoeffBuffer(
                        *_coeffBufferY, 1, block.X + x - 1, block.Y + y + 4);
                    const std::int32_t coeffTop = GetCoeffBuffer(
                        *_coeffBufferY, 1, block.X + x, block.Y + y + 4 - 1);
                    const std::int32_t nc = (coeffLeft + coeffTop + 1) / 2;
                    const std::int32_t outTotalCoeff = DecodeResidueCAVLC(
                        block.X + x, block.Y + y + 4, nc, *_planeBufferY, 1);
                    SetCoeffBuffer(*_coeffBufferY, 1, block.X + x, block.Y + y + 4,
                        static_cast<std::uint8_t>(outTotalCoeff));
                }
                else
                {
                    SetCoeffBuffer(*_coeffBufferY, 1, block.X + x, block.Y + y + 4, 0);
                }

                if ((residueMask & 8) != 0)
                {
                    const std::int32_t coeffLeft = GetCoeffBuffer(
                        *_coeffBufferY, 1, block.X + x + 4 - 1, block.Y + y + 4);
                    const std::int32_t coeffTop = GetCoeffBuffer(
                        *_coeffBufferY, 1, block.X + x + 4, block.Y + y + 4 - 1);
                    const std::int32_t nc = (coeffLeft + coeffTop + 1) / 2;
                    const std::int32_t outTotalCoeff = DecodeResidueCAVLC(
                        block.X + x + 4, block.Y + y + 4, nc, *_planeBufferY, 1);
                    SetCoeffBuffer(*_coeffBufferY, 1, block.X + x + 4, block.Y + y + 4,
                        static_cast<std::uint8_t>(outTotalCoeff));
                }
                else
                {
                    SetCoeffBuffer(*_coeffBufferY, 1, block.X + x + 4, block.Y + y + 4, 0);
                }

                if ((residueMask & 16) != 0)
                {
                    const std::int32_t coeffLeft = GetCoeffBuffer(
                        *_coeffBufferUV, 2, block.X + x - 1, block.Y + y);
                    const std::int32_t coeffTop = GetCoeffBuffer(
                        *_coeffBufferUV, 2, block.X + x, block.Y + y - 1);
                    const std::int32_t nc = (coeffLeft + coeffTop + 1) / 2;
                    const std::int32_t totalCoeffU = DecodeResidueCAVLC(
                        block.X + x, block.Y + y, nc, *_planeBufferU, 2);
                    const std::int32_t totalCoeffV = DecodeResidueCAVLC(
                        block.X + x, block.Y + y, nc, *_planeBufferV, 2);
                    const std::int32_t outTotalCoeff = (totalCoeffU + totalCoeffV + 1) / 2;
                    SetCoeffBuffer(*_coeffBufferUV, 2, block.X + x, block.Y + y,
                        static_cast<std::uint8_t>(outTotalCoeff));
                }
                else
                {
                    SetCoeffBuffer(*_coeffBufferUV, 2, block.X + x, block.Y + y, 0);
                }
            }
        }
    }

    std::int32_t VideoFrame::DecodeResidueCAVLC(
        std::int32_t x, std::int32_t y, std::int32_t nc,
        ByteArray2D& planeBuffer, std::int32_t step)
    {
        const std::int32_t coeffToken = _reader->ReadVLC2(
            VLC::CoeffTokenVlc().at(
                static_cast<std::size_t>(_tokenIndexTable.at(static_cast<std::size_t>(nc)))));
        if (coeffToken == -1)
        {
            throw ProgramException("VX decoding error 016");
        }

        std::int32_t trailingOnes = coeffToken & 3;
        std::int32_t totalCoeff = coeffToken >> 2;
        const std::int32_t outTotalCoeff = totalCoeff;
        if (totalCoeff == 0)
        {
            return outTotalCoeff;
        }

        std::array<std::int32_t, 16> level;
        std::int32_t levelPos = 0;
        std::int32_t zeroesRemaining;
        if (totalCoeff == 16)
        {
            zeroesRemaining = 0;
        }
        else
        {
            zeroesRemaining = _reader->ReadVLC2(
                VLC::TotalZeroesVlc().at(static_cast<std::size_t>(totalCoeff)));
            for (std::int32_t i = 0; i < 16 - (totalCoeff + zeroesRemaining); ++i)
            {
                level.at(static_cast<std::size_t>(levelPos++)) = 0;
            }
        }

        std::int32_t suffixLength = 0;
        while (true)
        {
            if (trailingOnes > 0)
            {
                --trailingOnes;
                level.at(static_cast<std::size_t>(levelPos++))
                    = _reader->ReadBit() == 0 ? 1 : -1;
            }
            else
            {
                std::int32_t levelPrefix = 0;
                while (_reader->ReadBit() == 0)
                {
                    ++levelPrefix;
                }

                std::int32_t levelSuffix;
                if (levelPrefix == 15)
                {
                    levelSuffix = _reader->ReadInt(11);
                }
                else
                {
                    levelSuffix = _reader->ReadInt(suffixLength);
                }

                std::int32_t levelCode = (levelPrefix << suffixLength) + levelSuffix + 1;
                if (levelCode > _suffixLimits.at(static_cast<std::size_t>(suffixLength + 1)))
                {
                    ++suffixLength;
                }
                if (_reader->ReadBit() == 1)
                {
                    levelCode = -levelCode;
                }
                level.at(static_cast<std::size_t>(levelPos++)) = levelCode;
            }

            --totalCoeff;
            if (totalCoeff == 0)
            {
                break;
            }
            if (zeroesRemaining == 0)
            {
                continue;
            }

            std::int32_t runBefore;
            if (zeroesRemaining < 7)
            {
                runBefore = _reader->ReadVLC2(
                    VLC::RunVlc().at(static_cast<std::size_t>(zeroesRemaining)));
            }
            else
            {
                runBefore = _reader->ReadVLC2(VLC::Run7Vlc());
            }

            zeroesRemaining -= runBefore;
            for (std::int32_t i = 0; i < runBefore; ++i)
            {
                level.at(static_cast<std::size_t>(levelPos++)) = 0;
            }
        }

        for (std::int32_t i = 0; i < zeroesRemaining; ++i)
        {
            level.at(static_cast<std::size_t>(levelPos++)) = 0;
        }

        assert(levelPos == 16);
        DecodeDct(x, y, planeBuffer, step, level);
        return outTotalCoeff;
    }

    void VideoFrame::DecodeDct(
        std::int32_t x, std::int32_t y, ByteArray2D& planeBuffer,
        std::int32_t step, std::span<const std::int32_t> level)
    {
        std::array<std::int32_t, 16> dct;

        for (std::size_t i = 0; i < _zigzagScanTable.size(); ++i)
        {
            const std::int32_t z = _zigzagScanTable[i];
            dct.at(static_cast<std::size_t>(z))
                = level[15U - i]
                * _quantizerTable->at(static_cast<std::size_t>((z & 1) + ((z >> 2) & 1)));
        }

        dct[0] += 1 << 5;

        for (std::int32_t i = 0; i < 4; ++i)
        {
            const std::size_t si = static_cast<std::size_t>(i);
            const std::int32_t z0 = dct[si + 4U * 0U] + dct[si + 4U * 2U];
            const std::int32_t z1 = dct[si + 4U * 0U] - dct[si + 4U * 2U];
            const std::int32_t z2 = (dct[si + 4U * 1U] / 2) - dct[si + 4U * 3U];
            const std::int32_t z3 = dct[si + 4U * 1U] + (dct[si + 4U * 3U] / 2);

            dct[si + 4U * 0U] = z0 + z3;
            dct[si + 4U * 1U] = z1 + z2;
            dct[si + 4U * 2U] = z1 - z2;
            dct[si + 4U * 3U] = z0 - z3;
        }

        for (std::int32_t i = 0; i < 4; ++i)
        {
            const std::size_t base = 4U * static_cast<std::size_t>(i);
            const std::int32_t z0 = dct[0U + base] + dct[2U + base];
            const std::int32_t z1 = dct[0U + base] - dct[2U + base];
            const std::int32_t z2 = (dct[1U + base] / 2) - dct[3U + base];
            const std::int32_t z3 = dct[1U + base] + (dct[3U + base] / 2);

            const std::int32_t bx = x + step * i;
            std::int32_t by = y + step * 0;
            std::int32_t pixel = PlaneBufferGetter(planeBuffer, step, bx, by)
                + ((z0 + z3) >> 6);
            planeBuffer(by / step, bx / step)
                = static_cast<std::uint8_t>(ClampInt(pixel, 0, 255));

            by = y + step * 1;
            pixel = PlaneBufferGetter(planeBuffer, step, bx, by)
                + ((z1 + z2) >> 6);
            planeBuffer(by / step, bx / step)
                = static_cast<std::uint8_t>(ClampInt(pixel, 0, 255));

            by = y + step * 2;
            pixel = PlaneBufferGetter(planeBuffer, step, bx, by)
                + ((z1 - z2) >> 6);
            planeBuffer(by / step, bx / step)
                = static_cast<std::uint8_t>(ClampInt(pixel, 0, 255));

            by = y + step * 3;
            pixel = PlaneBufferGetter(planeBuffer, step, bx, by)
                + ((z0 - z3) >> 6);
            planeBuffer(by / step, bx / step)
                = static_cast<std::uint8_t>(ClampInt(pixel, 0, 255));
        }
    }

    void VideoFrame::PredictNoTile(Block block)
    {
        const std::int32_t mode = _reader->ReadUnsignedExpGolomb();
        if (mode == 0)
        {
            PredictVertical(block, *_planeBufferY, 1);
        }
        else if (mode == 1)
        {
            PredictHorizontal(block, *_planeBufferY, 1);
        }
        else if (mode == 2)
        {
            PredictDC(block, *_planeBufferY, 1);
        }
        else if (mode == 3)
        {
            PredictPlane(block, *_planeBufferY, 1, 0);
        }
        else
        {
            throw ProgramException("VX decoding error 017: " + std::to_string(mode));
        }
        PredictNoTileUV(block);
    }

    void VideoFrame::PredictVertical(Block block, ByteArray2D& planeBuffer, std::int32_t step)
    {
        for (std::int32_t y = block.Y; y < block.Y + block.H; y += step)
        {
            for (std::int32_t x = block.X; x < block.X + block.W; x += step)
            {
                planeBuffer(y / step, x / step)
                    = PlaneBufferGetter(planeBuffer, step, x, block.Y - 1);
            }
        }
    }

    void VideoFrame::PredictHorizontal(Block block, ByteArray2D& planeBuffer, std::int32_t step)
    {
        for (std::int32_t y = block.Y; y < block.Y + block.H; y += step)
        {
            for (std::int32_t x = block.X; x < block.X + block.W; x += step)
            {
                planeBuffer(y / step, x / step)
                    = PlaneBufferGetter(planeBuffer, step, block.X - 1, y);
            }
        }
    }

    void VideoFrame::PredictDC(Block block, ByteArray2D& planeBuffer, std::int32_t step)
    {
        std::uint8_t dc = 128;
        if (block.X != 0 && block.Y != 0)
        {
            std::int32_t sumX = block.W / 2;
            for (std::int32_t x = 0; x < block.W; ++x)
            {
                sumX += PlaneBufferGetter(planeBuffer, step, block.X + x, block.Y - 1);
            }
            std::int32_t sumY = block.H / 2;
            for (std::int32_t y = 0; y < block.H; ++y)
            {
                sumY += PlaneBufferGetter(planeBuffer, step, block.X - 1, block.Y + y);
            }
            dc = static_cast<std::uint8_t>(((sumX / block.W) + (sumY / block.H) + 1) / 2);
        }
        else if (block.X == 0 && block.Y != 0)
        {
            std::int32_t sumX = block.W / 2;
            for (std::int32_t x = 0; x < block.W; ++x)
            {
                sumX += PlaneBufferGetter(planeBuffer, step, block.X + x, block.Y - 1);
            }
            dc = static_cast<std::uint8_t>(sumX / block.W);
        }
        else if (block.X != 0 && block.Y == 0)
        {
            std::int32_t sumY = block.H / 2;
            for (std::int32_t y = 0; y < block.H; ++y)
            {
                sumY += PlaneBufferGetter(planeBuffer, step, block.X - 1, block.Y + y);
            }
            dc = static_cast<std::uint8_t>(sumY / block.H);
        }

        for (std::int32_t y = block.Y; y < block.Y + block.H; y += step)
        {
            for (std::int32_t x = block.X; x < block.X + block.W; x += step)
            {
                planeBuffer(y / step, x / step) = dc;
            }
        }
    }

    void VideoFrame::PredictPlane(
        Block block, ByteArray2D& planeBuffer, std::int32_t step, std::int32_t value)
    {
        const std::int32_t bottomLeft
            = PlaneBufferGetter(planeBuffer, step, block.X - 1, block.Y + block.H - 1);
        const std::int32_t topRight
            = PlaneBufferGetter(planeBuffer, step, block.X + block.W - 1, block.Y - 1);
        const std::int32_t pixel = (bottomLeft + topRight + 1) / 2 + value;
        const std::int32_t x = block.X + block.W - 1;
        const std::int32_t y = block.Y + block.H - 1;
        planeBuffer(y / step, x / step) = static_cast<std::uint8_t>(pixel);
        PredictPlaneRecursive(block, planeBuffer, step);
    }

    void VideoFrame::PredictPlaneRecursive(
        Block block, ByteArray2D& planeBuffer, std::int32_t step)
    {
        if (block.W == step && block.H == step)
        {
            return;
        }
        if (block.W == step && block.H > step)
        {
            const std::int32_t top
                = PlaneBufferGetter(planeBuffer, step, block.X, block.Y - 1);
            const std::int32_t bottom
                = PlaneBufferGetter(planeBuffer, step, block.X, block.Y + block.H - 1);
            const std::int32_t pixel = (top + bottom) / 2;
            const std::int32_t x = block.X;
            const std::int32_t y = block.Y + (block.H / 2) - 1;
            planeBuffer(y / step, x / step) = static_cast<std::uint8_t>(pixel);
            PredictPlaneRecursive(block.HalfUp(), planeBuffer, step);
            PredictPlaneRecursive(block.HalfDown(), planeBuffer, step);
        }
        else if (block.W > step && block.H == step)
        {
            const std::int32_t left
                = PlaneBufferGetter(planeBuffer, step, block.X - 1, block.Y);
            const std::int32_t right
                = PlaneBufferGetter(planeBuffer, step, block.X + block.W - 1, block.Y);
            const std::int32_t pixel = (left + right) / 2;
            const std::int32_t x = block.X + (block.W / 2) - 1;
            const std::int32_t y = block.Y;
            planeBuffer(y / step, x / step) = static_cast<std::uint8_t>(pixel);
            PredictPlaneRecursive(block.HalfLeft(), planeBuffer, step);
            PredictPlaneRecursive(block.HalfRight(), planeBuffer, step);
        }
        else
        {
            const std::int32_t bottomLeft
                = PlaneBufferGetter(planeBuffer, step, block.X - 1, block.Y + block.H - 1);
            const std::int32_t topRight
                = PlaneBufferGetter(planeBuffer, step, block.X + block.W - 1, block.Y - 1);
            const std::int32_t bottomRight
                = PlaneBufferGetter(planeBuffer, step,
                    block.X + block.W - 1, block.Y + block.H - 1);
            const std::int32_t bottomCenter = (bottomLeft + bottomRight) / 2;
            const std::int32_t centerRight = (topRight + bottomRight) / 2;
            std::int32_t pixel;
            std::int32_t x = block.X + (block.W / 2) - 1;
            std::int32_t y = block.Y + block.H - 1;
            planeBuffer(y / step, x / step) = static_cast<std::uint8_t>(bottomCenter);
            x = block.X + block.W - 1;
            y = block.Y + (block.H / 2) - 1;
            planeBuffer(y / step, x / step) = static_cast<std::uint8_t>(centerRight);

            if ((block.W == 4 * step || block.W == 16 * step)
                != (block.H == 4 * step || block.H == 16 * step))
            {
                const std::int32_t centerLeft
                    = PlaneBufferGetter(planeBuffer, step,
                        block.X - 1, block.Y + (block.H / 2) - 1);
                pixel = (centerLeft + centerRight) / 2;
            }
            else
            {
                const std::int32_t topCenter
                    = PlaneBufferGetter(planeBuffer, step,
                        block.X + (block.W / 2) - 1, block.Y - 1);
                pixel = (topCenter + bottomCenter) / 2;
            }
            x = block.X + (block.W / 2) - 1;
            y = block.Y + (block.H / 2) - 1;
            planeBuffer(y / step, x / step) = static_cast<std::uint8_t>(pixel);
            PredictPlaneRecursive(block.HalfLeft().HalfUp(), planeBuffer, step);
            PredictPlaneRecursive(block.HalfRight().HalfUp(), planeBuffer, step);
            PredictPlaneRecursive(block.HalfLeft().HalfDown(), planeBuffer, step);
            PredictPlaneRecursive(block.HalfRight().HalfDown(), planeBuffer, step);
        }
    }

    void VideoFrame::PredictNoTileUV(Block block)
    {
        const std::int32_t mode = _reader->ReadUnsignedExpGolomb();
        if (mode == 0)
        {
            PredictDC(block, *_planeBufferU, 2);
            PredictDC(block, *_planeBufferV, 2);
        }
        else if (mode == 1)
        {
            PredictHorizontal(block, *_planeBufferU, 2);
            PredictHorizontal(block, *_planeBufferV, 2);
        }
        else if (mode == 2)
        {
            PredictVertical(block, *_planeBufferU, 2);
            PredictVertical(block, *_planeBufferV, 2);
        }
        else if (mode == 3)
        {
            PredictPlane(block, *_planeBufferU, 2, 0);
            PredictPlane(block, *_planeBufferV, 2, 0);
        }
        else
        {
            throw ProgramException("VX decoding error 018: " + std::to_string(mode));
        }
    }

    void VideoFrame::Predict4(Block block)
    {
        std::array<std::int32_t, 25> cache{};
        cache.fill(9);

        for (std::int32_t y2 = 0; y2 < block.H / 4; ++y2)
        {
            for (std::int32_t x2 = 0; x2 < block.W / 4; ++x2)
            {
                std::int32_t mode = std::min(
                    cache[static_cast<std::size_t>((1 + y2 - 1) * 5 + 1 + x2)],
                    cache[static_cast<std::size_t>((1 + y2) * 5 + 1 + x2 - 1)]);
                if (mode == 9)
                {
                    mode = 2;
                }

                if (_reader->ReadBit() == 0)
                {
                    const std::int32_t val = _reader->ReadInt(3);
                    mode = val + (val >= mode ? 1 : 0);
                }

                cache[static_cast<std::size_t>((1 + y2) * 5 + 1 + x2)] = mode;

                const Vector2ir vec(block.X + x2 * 4, block.Y + y2 * 4);
                if (mode == 0)
                {
                    Predict4x4Vertical(*_planeBufferY, vec);
                }
                else if (mode == 1)
                {
                    Predict4x4Horizontal(*_planeBufferY, vec);
                }
                else if (mode == 2)
                {
                    if (vec.X != 0 && vec.Y != 0)
                    {
                        Predict4x4Dc(*_planeBufferY, vec);
                    }
                    else if (vec.X != 0)
                    {
                        Predict4x4LeftDc(*_planeBufferY, vec);
                    }
                    else if (vec.Y != 0)
                    {
                        Predict4x4TopDc(*_planeBufferY, vec);
                    }
                    else
                    {
                        Predict4x4Dc128(*_planeBufferY, vec);
                    }
                }
                else if (mode == 3)
                {
                    Predict4x4DownLeft(*_planeBufferY, vec);
                }
                else if (mode == 4)
                {
                    Predict4x4DownRight(*_planeBufferY, vec);
                }
                else if (mode == 5)
                {
                    Predict4x4VerticalRight(*_planeBufferY, vec);
                }
                else if (mode == 6)
                {
                    Predict4x4HorizontalDown(*_planeBufferY, vec);
                }
                else if (mode == 7)
                {
                    Predict4x4VerticalLeft(*_planeBufferY, vec);
                }
                else if (mode == 8)
                {
                    Predict4x4HorizontalUp(*_planeBufferY, vec);
                }
                else
                {
                    throw ProgramException("VX decoding error 019: " + std::to_string(mode));
                }
            }
        }

        PredictNoTileUV(block);
    }

    void VideoFrame::Predict4x4Vertical(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        for (std::int32_t y = 0; y < 4; ++y)
        {
            for (std::int32_t x = 0; x < 4; ++x)
            {
                planeBuffer(vec.Y + y, vec.X + x) = planeBuffer(vec.Y - 1, vec.X + x);
            }
        }
    }

    void VideoFrame::Predict4x4Horizontal(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        for (std::int32_t y = 0; y < 4; ++y)
        {
            const std::uint8_t value = planeBuffer(vec.Y + y, vec.X - 1);
            for (std::int32_t x = 0; x < 4; ++x)
            {
                planeBuffer(vec.Y + y, vec.X + x) = value;
            }
        }
    }

    void VideoFrame::Predict4x4Dc(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        const std::uint8_t value = static_cast<std::uint8_t>(
            (planeBuffer(vec.Y - 1, vec.X + 0)
                + planeBuffer(vec.Y - 1, vec.X + 1)
                + planeBuffer(vec.Y - 1, vec.X + 2)
                + planeBuffer(vec.Y - 1, vec.X + 3)
                + planeBuffer(vec.Y + 0, vec.X - 1)
                + planeBuffer(vec.Y + 1, vec.X - 1)
                + planeBuffer(vec.Y + 2, vec.X - 1)
                + planeBuffer(vec.Y + 3, vec.X - 1) + 4) / 8);
        for (std::int32_t y = 0; y < 4; ++y)
        {
            for (std::int32_t x = 0; x < 4; ++x)
            {
                planeBuffer(vec.Y + y, vec.X + x) = value;
            }
        }
    }

    void VideoFrame::Predict4x4LeftDc(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        const std::uint8_t value = static_cast<std::uint8_t>(
            (planeBuffer(vec.Y + 0, vec.X - 1)
                + planeBuffer(vec.Y + 1, vec.X - 1)
                + planeBuffer(vec.Y + 2, vec.X - 1)
                + planeBuffer(vec.Y + 3, vec.X - 1) + 2) / 4);
        for (std::int32_t y = 0; y < 4; ++y)
        {
            for (std::int32_t x = 0; x < 4; ++x)
            {
                planeBuffer(vec.Y + y, vec.X + x) = value;
            }
        }
    }

    void VideoFrame::Predict4x4TopDc(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        const std::uint8_t value = static_cast<std::uint8_t>(
            (planeBuffer(vec.Y - 1, vec.X + 0)
                + planeBuffer(vec.Y - 1, vec.X + 1)
                + planeBuffer(vec.Y - 1, vec.X + 2)
                + planeBuffer(vec.Y - 1, vec.X + 3) + 2) / 4);
        for (std::int32_t y = 0; y < 4; ++y)
        {
            for (std::int32_t x = 0; x < 4; ++x)
            {
                planeBuffer(vec.Y + y, vec.X + x) = value;
            }
        }
    }

    void VideoFrame::Predict4x4Dc128(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        for (std::int32_t y = 0; y < 4; ++y)
        {
            for (std::int32_t x = 0; x < 4; ++x)
            {
                planeBuffer(vec.Y + y, vec.X + x) = 128;
            }
        }
    }

    void VideoFrame::Predict4x4DownLeft(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        const std::int32_t t0 = planeBuffer(vec.Y - 1, vec.X + 0);
        const std::int32_t t1 = planeBuffer(vec.Y - 1, vec.X + 1);
        const std::int32_t t2 = planeBuffer(vec.Y - 1, vec.X + 2);
        const std::int32_t t3 = planeBuffer(vec.Y - 1, vec.X + 3);
        const std::int32_t t4 = planeBuffer(vec.Y - 1, vec.X + 4);
        const std::int32_t t5 = planeBuffer(vec.Y - 1, vec.X + 5);
        const std::int32_t t6 = planeBuffer(vec.Y - 1, vec.X + 6);
        const std::int32_t t7 = planeBuffer(vec.Y - 1, vec.X + 7);

        const std::array<std::int32_t, 7> pixels{
            (t0 + 2 * t1 + t2 + 2) / 4,
            (t1 + 2 * t2 + t3 + 2) / 4,
            (t2 + 2 * t3 + t4 + 2) / 4,
            (t3 + 2 * t4 + t5 + 2) / 4,
            (t4 + 2 * t5 + t6 + 2) / 4,
            (t5 + 2 * t6 + t7 + 2) / 4,
            (t6 + 3 * t7 + 2) / 4
        };

        for (std::int32_t y = 0; y < 4; ++y)
        {
            for (std::int32_t x = 0; x < 4; ++x)
            {
                planeBuffer(vec.Y + y, vec.X + x)
                    = static_cast<std::uint8_t>(pixels[static_cast<std::size_t>(x + y)]);
            }
        }
    }

    void VideoFrame::Predict4x4DownRight(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        const std::int32_t lt = planeBuffer(vec.Y - 1, vec.X - 1);
        const std::int32_t t0 = planeBuffer(vec.Y - 1, vec.X + 0);
        const std::int32_t t1 = planeBuffer(vec.Y - 1, vec.X + 1);
        const std::int32_t t2 = planeBuffer(vec.Y - 1, vec.X + 2);
        const std::int32_t t3 = planeBuffer(vec.Y - 1, vec.X + 3);
        const std::int32_t l0 = planeBuffer(vec.Y + 0, vec.X - 1);
        const std::int32_t l1 = planeBuffer(vec.Y + 1, vec.X - 1);
        const std::int32_t l2 = planeBuffer(vec.Y + 2, vec.X - 1);
        const std::int32_t l3 = planeBuffer(vec.Y + 3, vec.X - 1);

        const std::array<std::int32_t, 7> pixels{
            (l3 + 2 * l2 + l1 + 2) / 4,
            (l2 + 2 * l1 + l0 + 2) / 4,
            (l1 + 2 * l0 + lt + 2) / 4,
            (l0 + 2 * lt + t0 + 2) / 4,
            (lt + 2 * t0 + t1 + 2) / 4,
            (t0 + 2 * t1 + t2 + 2) / 4,
            (t1 + 2 * t2 + t3 + 2) / 4
        };

        for (std::int32_t y = 0; y < 4; ++y)
        {
            for (std::int32_t x = 0; x < 4; ++x)
            {
                planeBuffer(vec.Y + y, vec.X + x)
                    = static_cast<std::uint8_t>(
                        pixels[static_cast<std::size_t>(3 + x - y)]);
            }
        }
    }

    void VideoFrame::Predict4x4VerticalRight(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        const std::int32_t lt = planeBuffer(vec.Y - 1, vec.X - 1);
        const std::int32_t t0 = planeBuffer(vec.Y - 1, vec.X + 0);
        const std::int32_t t1 = planeBuffer(vec.Y - 1, vec.X + 1);
        const std::int32_t t2 = planeBuffer(vec.Y - 1, vec.X + 2);
        const std::int32_t t3 = planeBuffer(vec.Y - 1, vec.X + 3);
        const std::int32_t l0 = planeBuffer(vec.Y + 0, vec.X - 1);
        const std::int32_t l1 = planeBuffer(vec.Y + 1, vec.X - 1);
        const std::int32_t l2 = planeBuffer(vec.Y + 2, vec.X - 1);

        const std::array<std::int32_t, 10> pixels{
            (l0 + 2 * l1 + l2 + 2) / 4,
            (lt + 2 * l0 + l1 + 2) / 4,
            (l0 + 2 * lt + t0 + 2) / 4,
            (lt + t0 + 1) / 2,
            (lt + 2 * t0 + t1 + 2) / 4,
            (t0 + t1 + 1) / 2,
            (t0 + 2 * t1 + t2 + 2) / 4,
            (t1 + t2 + 1) / 2,
            (t1 + 2 * t2 + t3 + 2) / 4,
            (t2 + t3 + 1) / 2
        };

        for (std::int32_t y = 0; y < 4; ++y)
        {
            for (std::int32_t x = 0; x < 4; ++x)
            {
                planeBuffer(vec.Y + y, vec.X + x)
                    = static_cast<std::uint8_t>(
                        pixels[static_cast<std::size_t>(3 + 2 * x - y)]);
            }
        }
    }

    void VideoFrame::Predict4x4HorizontalDown(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        const std::int32_t lt = planeBuffer(vec.Y - 1, vec.X - 1);
        const std::int32_t t0 = planeBuffer(vec.Y - 1, vec.X + 0);
        const std::int32_t t1 = planeBuffer(vec.Y - 1, vec.X + 1);
        const std::int32_t t2 = planeBuffer(vec.Y - 1, vec.X + 2);
        const std::int32_t l0 = planeBuffer(vec.Y + 0, vec.X - 1);
        const std::int32_t l1 = planeBuffer(vec.Y + 1, vec.X - 1);
        const std::int32_t l2 = planeBuffer(vec.Y + 2, vec.X - 1);
        const std::int32_t l3 = planeBuffer(vec.Y + 3, vec.X - 1);

        const std::array<std::int32_t, 10> pixels{
            (t0 + 2 * t1 + t2 + 2) / 4,
            (lt + 2 * t0 + t1 + 2) / 4,
            (l0 + 2 * lt + t0 + 2) / 4,
            (lt + l0 + 1) / 2,
            (lt + 2 * l0 + l1 + 2) / 4,
            (l0 + l1 + 1) / 2,
            (l0 + 2 * l1 + l2 + 2) / 4,
            (l1 + l2 + 1) / 2,
            (l1 + 2 * l2 + l3 + 2) / 4,
            (l2 + l3 + 1) / 2
        };

        for (std::int32_t y = 0; y < 4; ++y)
        {
            for (std::int32_t x = 0; x < 4; ++x)
            {
                planeBuffer(vec.Y + y, vec.X + x)
                    = static_cast<std::uint8_t>(
                        pixels[static_cast<std::size_t>(3 - x + 2 * y)]);
            }
        }
    }

    void VideoFrame::Predict4x4VerticalLeft(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        const std::int32_t t0 = planeBuffer(vec.Y - 1, vec.X + 0);
        const std::int32_t t1 = planeBuffer(vec.Y - 1, vec.X + 1);
        const std::int32_t t2 = planeBuffer(vec.Y - 1, vec.X + 2);
        const std::int32_t t3 = planeBuffer(vec.Y - 1, vec.X + 3);
        const std::int32_t t4 = planeBuffer(vec.Y - 1, vec.X + 4);
        const std::int32_t t5 = planeBuffer(vec.Y - 1, vec.X + 5);
        const std::int32_t t6 = planeBuffer(vec.Y - 1, vec.X + 6);

        const std::array<std::int32_t, 10> pixels{
            (t0 + t1 + 1) / 2,
            (t0 + 2 * t1 + t2 + 2) / 4,
            (t1 + t2 + 1) / 2,
            (t1 + 2 * t2 + t3 + 2) / 4,
            (t2 + t3 + 1) / 2,
            (t2 + 2 * t3 + t4 + 2) / 4,
            (t3 + t4 + 1) / 2,
            (t3 + 2 * t4 + t5 + 2) / 4,
            (t4 + t5 + 1) / 2,
            (t4 + 2 * t5 + t6 + 2) / 4
        };

        for (std::int32_t y = 0; y < 4; ++y)
        {
            for (std::int32_t x = 0; x < 4; ++x)
            {
                planeBuffer(vec.Y + y, vec.X + x)
                    = static_cast<std::uint8_t>(
                        pixels[static_cast<std::size_t>(2 * x + y)]);
            }
        }
    }

    void VideoFrame::Predict4x4HorizontalUp(ByteArray2D& planeBuffer, Vector2ir vec)
    {
        const std::int32_t l0 = planeBuffer(vec.Y + 0, vec.X - 1);
        const std::int32_t l1 = planeBuffer(vec.Y + 1, vec.X - 1);
        const std::int32_t l2 = planeBuffer(vec.Y + 2, vec.X - 1);
        const std::int32_t l3 = planeBuffer(vec.Y + 3, vec.X - 1);

        const std::array<std::int32_t, 7> pixels{
            (l0 + l1 + 1) / 2,
            (l0 + 2 * l1 + l2 + 2) / 4,
            (l1 + l2 + 1) / 2,
            (l1 + 2 * l2 + l3 + 2) / 4,
            (l2 + l3 + 1) / 2,
            (l2 + 2 * l3 + l3 + 2) / 4,
            l3
        };

        for (std::int32_t y = 0; y < 4; ++y)
        {
            for (std::int32_t x = 0; x < 4; ++x)
            {
                planeBuffer(vec.Y + y, vec.X + x)
                    = static_cast<std::uint8_t>(
                        pixels[static_cast<std::size_t>(std::min(x + 2 * y, 6))]);
            }
        }
    }

    std::uint8_t VideoFrame::GetCoeffBuffer(
        const ByteArray2D& buffer, std::int32_t step, std::int32_t x, std::int32_t y)
    {
        return buffer(y / (step * 4) + 1, x / (step * 4) + 1);
    }

    void VideoFrame::SetCoeffBuffer(
        ByteArray2D& buffer, std::int32_t step, std::int32_t x, std::int32_t y,
        std::uint8_t value)
    {
        buffer(y / (step * 4) + 1, x / (step * 4) + 1) = value;
    }

    void VideoFrame::ClearTotalCoeff(Block block)
    {
        for (std::int32_t y = 0; y < block.H; y += 8)
        {
            for (std::int32_t x = 0; x < block.W; x += 8)
            {
                SetCoeffBuffer(*_coeffBufferY, 1, block.X + x, block.Y + y, 0);
                SetCoeffBuffer(*_coeffBufferY, 1, block.X + x, block.Y + y + 4, 0);
                SetCoeffBuffer(*_coeffBufferY, 1, block.X + x + 4, block.Y + y, 0);
                SetCoeffBuffer(*_coeffBufferY, 1, block.X + x + 4, block.Y + y + 4, 0);
                SetCoeffBuffer(*_coeffBufferUV, 2, block.X + x, block.Y + y, 0);
            }
        }
    }

    const std::array<std::int32_t, 4> AudioFrame::_pulseDataLengths{8, 5, 4, 3};
    const std::array<std::int32_t, 4> AudioFrame::_pulseDistances{3, 3, 4, 5};

    AudioFrame::AudioFrame(
        AudioExtradata& extradata,
        std::shared_ptr<AudioFrame> prevAudioFrame,
        const VxBuffers& buffers,
        std::int32_t sampleBufferIndex)
        : _extradata(&extradata),
          _prevAudioFrame(std::move(prevAudioFrame)),
          _prevSampleBuffer(buffers.PrevSampleBuffer),
          _prevPulseBuffer(buffers.PrevPulseBuffer),
          _lpcFilterBuffer(buffers.LpcFilterBuffer),
          _influenceBuffer(buffers.InfluenceBuffer),
          _sampleBuffer(buffers.SampleBuffer),
          _sampleBufferIndex(sampleBufferIndex)
    {
    }

    std::span<std::int16_t> AudioFrame::SampleBuffer()
    {
        if (!_sampleBuffer)
        {
            throw System::NullReferenceException();
        }
        const std::int32_t start = WrapInt32Multiply(128, _sampleBufferIndex);
        if (start < 0 || static_cast<std::size_t>(start) + 128U > _sampleBuffer->size())
        {
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        }
        return std::span<std::int16_t>(_sampleBuffer->data() + start, 128);
    }

    std::span<const std::int16_t> AudioFrame::SampleBuffer() const
    {
        if (!_sampleBuffer)
        {
            throw System::NullReferenceException();
        }
        const std::int32_t start = WrapInt32Multiply(128, _sampleBufferIndex);
        if (start < 0 || static_cast<std::size_t>(start) + 128U > _sampleBuffer->size())
        {
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        }
        return std::span<const std::int16_t>(_sampleBuffer->data() + start, 128);
    }

    void AudioFrame::Decode(BitStreamReader& reader)
    {
        _reader = &reader;
        std::array<std::int32_t, 3> lpcCodebookIndices{};
        const std::int32_t header1 = reader.ReadInt(16);
        const std::int32_t header2 = reader.ReadInt(16);
        lpcCodebookIndices[0] = header1 & 0x3F;
        const std::int32_t scaleModifierIndex = (header1 >> 6) & 7;
        const std::int32_t prevFrameOffset = (header1 >> 9) & 0x7F;
        lpcCodebookIndices[2] = header2 & 0x3F;
        lpcCodebookIndices[1] = (header2 >> 6) & 0x3F;
        const std::int32_t pulsePackingMode = (header2 >> 12) & 3;
        const std::int32_t pulseStartPosition = (header2 >> 14) & 3;

        if (pulsePackingMode >= static_cast<std::int32_t>(_pulseDataLengths.size()))
        {
            throw ProgramException(
                "VX decoding error 020: " + std::to_string(pulsePackingMode));
        }
        if (scaleModifierIndex >= static_cast<std::int32_t>(_extradata->ScaleModifiers->size()))
        {
            throw ProgramException(
                "VX decoding error 021: " + std::to_string(scaleModifierIndex)
                + ", " + std::to_string(_extradata->ScaleModifiers->size()));
        }

        const std::int32_t pulseDataLength
            = _pulseDataLengths[static_cast<std::size_t>(pulsePackingMode)];
        std::array<std::uint16_t, 8> pulseData{};
        for (std::int32_t i = 0; i < pulseDataLength; ++i)
        {
            pulseData[static_cast<std::size_t>(i)]
                = static_cast<std::uint16_t>(reader.ReadInt(16));
        }

        std::array<std::int32_t, 42> pulseValues{};
        std::int32_t pulseValueLength
            = pulsePackingMode == 0 ? 42 : pulseDataLength * 8;
        if (pulsePackingMode == 0)
        {
            std::int32_t v = 0;
            for (std::int32_t i = 0; i < pulseDataLength; ++i)
            {
                for (std::int32_t j = 13; j >= 0; j -= 3)
                {
                    pulseValues[static_cast<std::size_t>(v++)]
                        = ((pulseData[static_cast<std::size_t>(i)] >> j) & 7) * 2 - 7;
                }
            }
            pulseValues[static_cast<std::size_t>(v++)]
                = ((pulseData[0] & 1) * 4 + (pulseData[1] & 1) * 2
                    + (pulseData[2] & 1)) * 2 - 7;
            pulseValues[static_cast<std::size_t>(v++)]
                = ((pulseData[3] & 1) * 4 + (pulseData[4] & 1) * 2
                    + (pulseData[5] & 1)) * 2 - 7;
        }
        else
        {
            std::int32_t v = 0;
            for (std::int32_t i = 0; i < pulseDataLength; ++i)
            {
                for (std::int32_t j = 14; j >= 0; j -= 2)
                {
                    pulseValues[static_cast<std::size_t>(v++)]
                        = ((pulseData[static_cast<std::size_t>(i)] >> j) & 3) * 2 - 3;
                }
            }
        }

        _scale = _extradata->ScaleInitial;
        if (prevFrameOffset != 127)
        {
            assert(_prevAudioFrame != nullptr);
            if (!_prevAudioFrame)
            {
                throw System::NullReferenceException();
            }
            _scale = _prevAudioFrame->Scale();
        }
        _scale = _scale
            * (*_extradata->ScaleModifiers)[static_cast<std::size_t>(scaleModifierIndex)] / 8192;

        const std::int32_t pulseDistance
            = _pulseDistances[static_cast<std::size_t>(pulsePackingMode)];

        std::array<std::int32_t, 128> pulseBuffer{};
        if (prevFrameOffset < 126)
        {
            for (std::int32_t i = 0; i < 128; ++i)
            {
                const std::int32_t volume = std::min(8, std::min(i + 1, 128 - i));
                pulseBuffer[static_cast<std::size_t>(i)]
                    = (*_prevPulseBuffer)[static_cast<std::size_t>(i + 127 - prevFrameOffset)]
                    * volume / 16;
            }
        }

        for (std::int32_t i = 0; i < 128; ++i)
        {
            const std::int32_t dividend = i - pulseStartPosition;
            const std::int32_t index = dividend / pulseDistance;
            const std::int32_t remainder = dividend % pulseDistance;
            if (remainder == 0 && index >= 0 && index < pulseValueLength)
            {
                pulseBuffer[static_cast<std::size_t>(i)]
                    += pulseValues[static_cast<std::size_t>(index)] * _scale;
            }
        }

        if (prevFrameOffset == 127)
        {
            std::copy(_extradata->LpcBase->begin(), _extradata->LpcBase->end(),
                _lpcFilterBuffer->begin());
        }

        for (std::int32_t i = 0; i < 8; ++i)
        {
            std::int32_t coeffSum = 0;
            for (std::int32_t j = 0; j < 3; ++j)
            {
                const std::int32_t index = lpcCodebookIndices[static_cast<std::size_t>(j)];
                coeffSum += (*_extradata->LpcCodebooks)(
                    j, index, i);
            }
            (*_lpcFilterBuffer)[static_cast<std::size_t>(i)] += coeffSum;
        }

        std::array<std::int32_t, 8> influenceValues{};
        std::array<std::int32_t, 8> influenceTemp{};
        for (std::int32_t i = 0; i < 8; ++i)
        {
            std::copy(influenceValues.begin(), influenceValues.end(), influenceTemp.begin());
            const std::int32_t coeff
                = (*_lpcFilterBuffer)[static_cast<std::size_t>(i)];
            for (std::int32_t j = 0; j < i; ++j)
            {
                influenceValues[static_cast<std::size_t>(j)]
                    += influenceTemp[static_cast<std::size_t>(i - j - 1)]
                    * coeff / 32768;
            }
            influenceValues[static_cast<std::size_t>(i)] = coeff;
        }
        for (std::int32_t i = 0; i < 8; ++i)
        {
            influenceValues[static_cast<std::size_t>(i)] /= -2;
        }

        std::array<std::int32_t, 32> influenceQuarters{};
        if (prevFrameOffset != 127)
        {
            assert(_prevAudioFrame != nullptr);
            std::copy(influenceValues.begin(), influenceValues.end(),
                influenceQuarters.begin() + 24);
            for (std::int32_t i = 0; i < 8; ++i)
            {
                influenceQuarters[static_cast<std::size_t>(8 + i)]
                    = ((*_influenceBuffer)[static_cast<std::size_t>(i)]
                        + influenceQuarters[static_cast<std::size_t>(24 + i)]) / 2;
            }
            for (std::int32_t i = 0; i < 8; ++i)
            {
                influenceQuarters[static_cast<std::size_t>(i)]
                    = ((*_influenceBuffer)[static_cast<std::size_t>(i)]
                        + influenceQuarters[static_cast<std::size_t>(8 + i)]) / 2;
            }
            for (std::int32_t i = 0; i < 8; ++i)
            {
                influenceQuarters[static_cast<std::size_t>(16 + i)]
                    = (influenceQuarters[static_cast<std::size_t>(8 + i)]
                        + influenceQuarters[static_cast<std::size_t>(24 + i)]) / 2;
            }
        }
        else
        {
            for (std::int32_t q = 0; q < 4; ++q)
            {
                std::copy(influenceValues.begin(), influenceValues.end(),
                    influenceQuarters.begin() + q * 8);
            }
        }

        std::span<std::int16_t> sampleBuffer = SampleBuffer();
        std::fill(sampleBuffer.begin(), sampleBuffer.end(), 0);
        for (std::int32_t i = 0; i < 128; ++i)
        {
            const std::int32_t quarterStart = i * 4 / 128 * 8;
            std::int32_t sample = pulseBuffer[static_cast<std::size_t>(i)] * 16384;
            for (std::int32_t j = 0; j < 8; ++j)
            {
                const std::int32_t sampleIndex = i - j - 1;
                const std::int32_t prevSample = sampleIndex >= 0
                    ? sampleBuffer[static_cast<std::size_t>(sampleIndex)]
                    : (*_prevSampleBuffer)[static_cast<std::size_t>(sampleIndex + 8)];
                sample += prevSample
                    * influenceQuarters[static_cast<std::size_t>(quarterStart + j)];
            }
            sample /= 16384;
            sampleBuffer[static_cast<std::size_t>(i)]
                = static_cast<std::int16_t>(
                    ClampInt(sample, std::numeric_limits<std::int16_t>::min(),
                        std::numeric_limits<std::int16_t>::max()));
        }

        std::copy(_prevPulseBuffer->begin() + 128, _prevPulseBuffer->end(),
            _prevPulseBuffer->begin());
        std::copy(pulseBuffer.begin(), pulseBuffer.end(), _prevPulseBuffer->begin() + 128);
        std::copy(sampleBuffer.begin() + 120, sampleBuffer.end(), _prevSampleBuffer->begin());
        std::copy(influenceValues.begin(), influenceValues.end(), _influenceBuffer->begin());
        _reader = nullptr;
    }

    VLCData::VLCData(
        std::span<const std::int32_t> lengthList, std::span<const std::int32_t> bitList)
    {
        for (std::size_t i = 0; i < lengthList.size(); ++i)
        {
            const std::int32_t length = lengthList[i];
            const std::int32_t value = bitList[i];
            if (length != 0)
            {
                std::string bitString;
                if (value == 0)
                {
                    bitString = "0";
                }
                else
                {
                    const auto unsignedValue = static_cast<std::uint32_t>(value);
                    const int width = 32 - std::countl_zero(unsignedValue);
                    bitString.reserve(static_cast<std::size_t>(width));
                    for (int bit = width - 1; bit >= 0; --bit)
                    {
                        bitString.push_back(
                            ((unsignedValue >> bit) & 1U) != 0U ? '1' : '0');
                    }
                }
                if (bitString.size() < static_cast<std::size_t>(length))
                {
                    bitString.insert(bitString.begin(),
                        static_cast<std::size_t>(length) - bitString.size(), '0');
                }

                std::int32_t hashCode = 0;
                for (char c : bitString)
                {
                    hashCode = MovieNativeRuntime::HashCombine(hashCode, c - '0');
                }
                const auto [it, inserted] = _bitDict.emplace(
                    hashCode, static_cast<std::int32_t>(i));
                if (!inserted)
                {
                    throw std::invalid_argument(
                        "An item with the same key has already been added.");
                }
                _maxBitCount = std::max(
                    _maxBitCount, static_cast<std::int32_t>(bitString.size()));
            }
        }
    }

    std::int32_t VLCData::FindBitPattern(std::int32_t hashCode) const noexcept
    {
        const auto item = _bitDict.find(hashCode);
        return item == _bitDict.end() ? -1 : item->second;
    }

    struct VLC::State
    {
        std::vector<VLCData> CoeffTokenVlc;
        std::vector<VLCData> TotalZeroesVlc;
        std::vector<VLCData> RunVlc;
        VLCData Run7Vlc;
        std::int32_t Temp = 0;

        State()
            : Run7Vlc(
                std::array<std::int32_t, 15>{3, 3, 3, 3, 3, 3, 3, 4, 5, 6, 7, 8, 9, 10, 11},
                std::array<std::int32_t, 15>{7, 6, 5, 4, 3, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1})
        {
            static const std::array<std::array<std::int32_t, 68>, 4> coeffTokenLengths{{
                {{
                    1, 0, 0, 0,
                    6, 2, 0, 0, 8, 6, 3, 0, 9, 8, 7, 5, 10, 9, 8, 6,
                    11, 10, 9, 7, 13, 11, 10, 8, 13, 13, 11, 9, 13, 13, 13, 10,
                    14, 14, 13, 11, 14, 14, 14, 13, 15, 15, 14, 14, 15, 15, 15, 14,
                    16, 15, 15, 15, 16, 16, 16, 15, 16, 16, 16, 16, 16, 16, 16, 16
                }},
                {{
                    2, 0, 0, 0,
                    6, 2, 0, 0, 6, 5, 3, 0, 7, 6, 6, 4, 8, 6, 6, 4,
                    8, 7, 7, 5, 9, 8, 8, 6, 11, 9, 9, 6, 11, 11, 11, 7,
                    12, 11, 11, 9, 12, 12, 12, 11, 12, 12, 12, 11, 13, 13, 13, 12,
                    13, 13, 13, 13, 13, 14, 13, 13, 14, 14, 14, 13, 14, 14, 14, 14
                }},
                {{
                    4, 0, 0, 0,
                    6, 4, 0, 0, 6, 5, 4, 0, 6, 5, 5, 4, 7, 5, 5, 4,
                    7, 5, 5, 4, 7, 6, 6, 4, 7, 6, 6, 4, 8, 7, 7, 5,
                    8, 8, 7, 6, 9, 8, 8, 7, 9, 9, 8, 8, 9, 9, 9, 8,
                    10, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10
                }},
                {{
                    6, 0, 0, 0,
                    6, 6, 0, 0, 6, 6, 6, 0, 6, 6, 6, 6, 6, 6, 6, 6,
                    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
                    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
                    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6
                }}
            }};
            static const std::array<std::array<std::int32_t, 68>, 4> coeffTokenBits{{
                {{
                    1, 0, 0, 0,
                    5, 1, 0, 0, 7, 4, 1, 0, 7, 6, 5, 3, 7, 6, 5, 3,
                    7, 6, 5, 4, 15, 6, 5, 4, 11, 14, 5, 4, 8, 10, 13, 4,
                    15, 14, 9, 4, 11, 10, 13, 12, 15, 14, 9, 12, 11, 10, 13, 8,
                    15, 1, 9, 12, 11, 14, 13, 8, 7, 10, 9, 12, 4, 6, 5, 8
                }},
                {{
                    3, 0, 0, 0,
                    11, 2, 0, 0, 7, 7, 3, 0, 7, 10, 9, 5, 7, 6, 5, 4,
                    4, 6, 5, 6, 7, 6, 5, 8, 15, 6, 5, 4, 11, 14, 13, 4,
                    15, 10, 9, 4, 11, 14, 13, 12, 8, 10, 9, 8, 15, 14, 13, 12,
                    11, 10, 9, 12, 7, 11, 6, 8, 9, 8, 10, 1, 7, 6, 5, 4
                }},
                {{
                    15, 0, 0, 0,
                    15, 14, 0, 0, 11, 15, 13, 0, 8, 12, 14, 12, 15, 10, 11, 11,
                    11, 8, 9, 10, 9, 14, 13, 9, 8, 10, 9, 8, 15, 14, 13, 13,
                    11, 14, 10, 12, 15, 10, 13, 12, 11, 14, 9, 12, 8, 10, 13, 8,
                    13, 7, 9, 12, 9, 12, 11, 10, 5, 8, 7, 6, 1, 4, 3, 2
                }},
                {{
                    3, 0, 0, 0,
                    0, 1, 0, 0, 4, 5, 6, 0, 8, 9, 10, 11, 12, 13, 14, 15,
                    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
                    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
                    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63
                }}
            }};

            static const std::array<std::vector<std::int32_t>, 16> totalZeroesLengths{{
                {},
                {1, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 9},
                {3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 6, 6, 6, 6},
                {4, 3, 3, 3, 4, 4, 3, 3, 4, 5, 5, 6, 5, 6},
                {5, 3, 4, 4, 3, 3, 3, 4, 3, 4, 5, 5, 5},
                {4, 4, 4, 3, 3, 3, 3, 3, 4, 5, 4, 5},
                {6, 5, 3, 3, 3, 3, 3, 3, 4, 3, 6},
                {6, 5, 3, 3, 3, 2, 3, 4, 3, 6},
                {6, 4, 5, 3, 2, 2, 3, 3, 6},
                {6, 6, 4, 2, 2, 3, 2, 5},
                {5, 5, 3, 2, 2, 2, 4},
                {4, 4, 3, 3, 1, 3},
                {4, 4, 2, 1, 3},
                {3, 3, 1, 2},
                {2, 2, 1},
                {1, 1}
            }};
            static const std::array<std::vector<std::int32_t>, 16> totalZeroesBits{{
                {},
                {1, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 1},
                {7, 6, 5, 4, 3, 5, 4, 3, 2, 3, 2, 3, 2, 1, 0},
                {5, 7, 6, 5, 4, 3, 4, 3, 2, 3, 2, 1, 1, 0},
                {3, 7, 5, 4, 6, 5, 4, 3, 3, 2, 2, 1, 0},
                {5, 4, 3, 7, 6, 5, 4, 3, 2, 1, 1, 0},
                {1, 1, 7, 6, 5, 4, 3, 2, 1, 1, 0},
                {1, 1, 5, 4, 3, 3, 2, 1, 1, 0},
                {1, 1, 1, 3, 3, 2, 2, 1, 0},
                {1, 0, 1, 3, 2, 1, 1, 1},
                {1, 0, 1, 3, 2, 1, 1},
                {0, 1, 1, 2, 1, 3},
                {0, 1, 1, 1, 1},
                {0, 1, 1, 1},
                {0, 1, 1},
                {0, 1}
            }};

            static const std::array<std::vector<std::int32_t>, 7> runLengths{{
                {}, {1, 1}, {1, 2, 2}, {2, 2, 2, 2},
                {2, 2, 2, 3, 3}, {2, 2, 3, 3, 3, 3}, {2, 3, 3, 3, 3, 3, 3}
            }};
            static const std::array<std::vector<std::int32_t>, 7> runBits{{
                {}, {1, 0}, {1, 1, 0}, {3, 2, 1, 0},
                {3, 2, 1, 1, 0}, {3, 2, 3, 2, 1, 0}, {3, 0, 1, 3, 2, 5, 4}
            }};

            CoeffTokenVlc.reserve(coeffTokenLengths.size());
            for (std::size_t i = 0; i < coeffTokenLengths.size(); ++i)
            {
                CoeffTokenVlc.emplace_back(coeffTokenLengths[i], coeffTokenBits[i]);
            }

            TotalZeroesVlc.reserve(totalZeroesLengths.size());
            for (std::size_t i = 0; i < totalZeroesLengths.size(); ++i)
            {
                TotalZeroesVlc.emplace_back(totalZeroesLengths[i], totalZeroesBits[i]);
            }

            RunVlc.reserve(runLengths.size());
            for (std::size_t i = 0; i < runLengths.size(); ++i)
            {
                RunVlc.emplace_back(runLengths[i], runBits[i]);
            }

            Temp = 1;
        }
    };

    const VLC::State& VLC::GetState()
    {
        static const State state;
        return state;
    }

    std::int32_t VLC::Temp()
    {
        return GetState().Temp;
    }

    const std::vector<VLCData>& VLC::CoeffTokenVlc()
    {
        return GetState().CoeffTokenVlc;
    }

    const std::vector<VLCData>& VLC::TotalZeroesVlc()
    {
        return GetState().TotalZeroesVlc;
    }

    const std::vector<VLCData>& VLC::RunVlc()
    {
        return GetState().RunVlc;
    }

    const VLCData& VLC::Run7Vlc()
    {
        return GetState().Run7Vlc;
    }
}
