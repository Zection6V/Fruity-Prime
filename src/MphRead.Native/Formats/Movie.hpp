#pragma once

#include "Formats.hpp"
#include "Sound.hpp"
#include "Types.hpp"
#include "../Metadata/FrontendMeta.hpp"
#include "../Program.hpp"

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <istream>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace MphRead::Formats::MovieNativeRuntime
{
    class MovieTask;
}

namespace MphRead
{
    enum class AfterMovie : std::int32_t
    {
        StartGame = 0,
        LoadRoom = 1,
        EndGame = 2
    };

    // Scene is a C# partial type. Scene.hpp is its canonical C++ declaration owner.
    // This contributor exposes only its declaration fragment; the canonical owner
    // expands contributor declaration fragments when that aggregate is closed.
#define MPHREAD_SCENE_MOVIE_MEMBERS \
private: \
    static constexpr std::int32_t _frameWidth = 256; \
    static constexpr std::int32_t _frameHeight = 192; \
    std::int32_t _topMovieBinding = -1; \
    std::int32_t _botMovieBinding = -1; \
    std::int32_t _movieFrameCount = 0; \
    std::int32_t _movieFrameIndex = -1; \
    std::int32_t _lastRenderedMovieFrameIndex = 0; \
    std::int32_t _movieFrameTotal = 0; \
public: \
    [[nodiscard]] bool MoviePlaying() const noexcept; \
private: \
    bool _skipMovie = false; \
    bool _playingLandingMovie = false; \
    bool _dualScreenMovie = true; \
    std::array<std::uint8_t, static_cast<std::size_t>(_frameWidth) * _frameHeight * 3> _topImageBuffer{}; \
    std::array<std::uint8_t, static_cast<std::size_t>(_frameWidth) * _frameHeight * 3> _botImageBuffer{}; \
    std::shared_ptr<std::stop_source> _decoderCts{}; \
public: \
    void StartMovies(Movie movieId, Movie afterMovieId, FadeType fadeToMovieType, float fadeToMovieLength, \
        FadeType fadeFromMovieType, float fadeFromMovieLength, AfterMovie afterMovieAction = AfterMovie::LoadRoom); \
    void StartMovie(Movie movieId, FadeType fadeToMovieType, float fadeToMovieLength, FadeType fadeFromMovieType, \
        float fadeFromMovieLength, std::optional<OpenTK::Mathematics::Vector3> afterPosition = std::nullopt, \
        std::optional<OpenTK::Mathematics::Vector3> afterFacing = std::nullopt, \
        std::optional<Movie> afterMovieId = std::nullopt, AfterMovie afterMovieAction = AfterMovie::LoadRoom); \
private: \
    void PlayMovie(Movie movieId); \
    std::int32_t _audioHandle = -1; \
public: \
    [[nodiscard]] std::int32_t MovieAudioHandle() const noexcept; \
private: \
    static constexpr std::int32_t _audioBufferCount = 16; \
    std::int32_t _audioBufferIndex = 0; \
    std::array<std::int32_t, _audioBufferCount> _audioBufferIds{}; \
    std::array<bool, _audioBufferCount> _audioBuffersAvailable{}; \
    [[nodiscard]] Formats::MovieNativeRuntime::MovieTask UpdateMovieAudio(std::stop_token token); \
    void StopMovie(); \
public: \
    void SkipMovie(); \
private: \
    void UpdateMovie(); \
    [[nodiscard]] Formats::MovieNativeRuntime::MovieTask UpdateMovieImage(std::stop_token token); \
    void DrawMovieFrame();

}

namespace MphRead::Formats
{
    namespace MovieNativeRuntime
    {
        template <typename T>
        class RectArray2D final
        {
        public:
            RectArray2D() = default;
            RectArray2D(std::int32_t rows, std::int32_t columns)
                : _rows(rows), _columns(columns),
                  _data(CheckedLength(rows, columns))
            {
            }

            [[nodiscard]] std::int32_t Rows() const noexcept { return _rows; }
            [[nodiscard]] std::int32_t Columns() const noexcept { return _columns; }
            [[nodiscard]] std::size_t Length() const noexcept { return _data.size(); }

            [[nodiscard]] T& operator()(std::int32_t row, std::int32_t column)
            {
                return _data.at(Index(row, column));
            }
            [[nodiscard]] const T& operator()(std::int32_t row, std::int32_t column) const
            {
                return _data.at(Index(row, column));
            }

            void Clear()
            {
                std::fill(_data.begin(), _data.end(), T{});
            }

            [[nodiscard]] std::vector<T>& Data() noexcept { return _data; }
            [[nodiscard]] const std::vector<T>& Data() const noexcept { return _data; }

        private:
            std::int32_t _rows = 0;
            std::int32_t _columns = 0;
            std::vector<T> _data{};

            [[nodiscard]] static std::size_t CheckedLength(std::int32_t rows, std::int32_t columns)
            {
                if (rows < 0 || columns < 0)
                {
                    throw std::overflow_error("Array dimensions exceeded supported range.");
                }
                return static_cast<std::size_t>(rows) * static_cast<std::size_t>(columns);
            }
            [[nodiscard]] std::size_t Index(std::int32_t row, std::int32_t column) const
            {
                if (row < 0 || column < 0 || row >= _rows || column >= _columns)
                {
                    throw std::out_of_range("Index was outside the bounds of the array.");
                }
                return static_cast<std::size_t>(row) * static_cast<std::size_t>(_columns)
                    + static_cast<std::size_t>(column);
            }
        };

        template <typename T>
        class RectArray3D final
        {
        public:
            RectArray3D() = default;
            RectArray3D(std::int32_t a, std::int32_t b, std::int32_t c)
                : _a(a), _b(b), _c(c), _data(CheckedLength(a, b, c))
            {
            }

            [[nodiscard]] T& operator()(std::int32_t a, std::int32_t b, std::int32_t c)
            {
                return _data.at(Index(a, b, c));
            }
            [[nodiscard]] const T& operator()(std::int32_t a, std::int32_t b, std::int32_t c) const
            {
                return _data.at(Index(a, b, c));
            }
            [[nodiscard]] std::size_t Length() const noexcept { return _data.size(); }

        private:
            std::int32_t _a = 0;
            std::int32_t _b = 0;
            std::int32_t _c = 0;
            std::vector<T> _data{};

            [[nodiscard]] static std::size_t CheckedLength(std::int32_t a, std::int32_t b, std::int32_t c)
            {
                if (a < 0 || b < 0 || c < 0)
                {
                    throw std::overflow_error("Array dimensions exceeded supported range.");
                }
                return static_cast<std::size_t>(a) * static_cast<std::size_t>(b) * static_cast<std::size_t>(c);
            }
            [[nodiscard]] std::size_t Index(std::int32_t a, std::int32_t b, std::int32_t c) const
            {
                if (a < 0 || b < 0 || c < 0 || a >= _a || b >= _b || c >= _c)
                {
                    throw std::out_of_range("Index was outside the bounds of the array.");
                }
                return (static_cast<std::size_t>(a) * static_cast<std::size_t>(_b)
                    + static_cast<std::size_t>(b)) * static_cast<std::size_t>(_c)
                    + static_cast<std::size_t>(c);
            }
        };

        class BinaryReader final
        {
        public:
            explicit BinaryReader(std::istream& stream) noexcept : _stream(&stream) {}
            [[nodiscard]] std::uint8_t ReadByte();
            [[nodiscard]] char16_t ReadChar();
            [[nodiscard]] std::int16_t ReadInt16();
            [[nodiscard]] std::uint16_t ReadUInt16();
            [[nodiscard]] std::int32_t ReadInt32();
            [[nodiscard]] std::int64_t Position();
            void Position(std::int64_t value);

        private:
            std::istream* _stream;
            void ReadExact(void* destination, std::size_t size);
        };

        struct TaskState;

        class MovieTask final
        {
        public:
            class Awaiter final
            {
            public:
                explicit Awaiter(std::shared_ptr<TaskState> state) noexcept;
                void GetResult();
            private:
                std::shared_ptr<TaskState> _state;
            };

            MovieTask();
            explicit MovieTask(std::function<void()> action, bool asynchronous);
            MovieTask(std::function<void()> action, bool asynchronous,
                std::function<bool()> synchronousPrefixDone);
            MovieTask(const MovieTask&) = delete;
            MovieTask& operator=(const MovieTask&) = delete;
            MovieTask(MovieTask&&) noexcept = default;
            MovieTask& operator=(MovieTask&&) noexcept = default;
            ~MovieTask() = default;

            [[nodiscard]] Awaiter GetAwaiter() const noexcept;

        private:
            std::shared_ptr<TaskState> _state;
        };

        [[nodiscard]] std::int32_t HashCombine(std::int32_t first, std::int32_t second) noexcept;
    }

    using ByteArray2D = MovieNativeRuntime::RectArray2D<std::uint8_t>;
    struct Vector2ir;
    using Vector2irArray2D = MovieNativeRuntime::RectArray2D<Vector2ir>;
    using IntArray3D = MovieNativeRuntime::RectArray3D<std::int32_t>;

    struct SeekTableEntry
    {
        const std::int32_t FrameId = 0;
        const std::int32_t FrameOffset = 0;

        SeekTableEntry() noexcept = default;
        SeekTableEntry(std::int32_t frameId, std::int32_t frameOffset) noexcept
            : FrameId(frameId), FrameOffset(frameOffset)
        {
        }
        SeekTableEntry(const SeekTableEntry&) noexcept = default;
        SeekTableEntry& operator=(const SeekTableEntry& other) noexcept;
    };

    class AudioExtradata
    {
    public:
        const std::shared_ptr<IntArray3D> LpcCodebooks
            = std::make_shared<IntArray3D>(3, 64, 8);
        const std::shared_ptr<std::array<std::int32_t, 8>> ScaleModifiers
            = std::make_shared<std::array<std::int32_t, 8>>();
        const std::shared_ptr<std::array<std::int32_t, 8>> LpcBase
            = std::make_shared<std::array<std::int32_t, 8>>();
        std::int32_t ScaleInitial = 0;
    };

    class VideoFrame;
    class AudioFrame;
    class VxFrame;
    class VLCData;
    class BitStreamReader;

    struct VxBuffers
    {
        const std::shared_ptr<std::array<std::shared_ptr<VideoFrame>, 3>> PrevVideoFrames;
        const std::shared_ptr<std::array<std::int32_t, 3>> QuantizerTable;
        const std::shared_ptr<ByteArray2D> PlaneBufferY;
        const std::shared_ptr<ByteArray2D> PlaneBufferU;
        const std::shared_ptr<ByteArray2D> PlaneBufferV;
        const std::shared_ptr<ByteArray2D> CoeffBufferY;
        const std::shared_ptr<ByteArray2D> CoeffBufferUV;
        const std::shared_ptr<Vector2irArray2D> Vectors;
        const std::shared_ptr<std::array<std::int16_t, 8>> PrevSampleBuffer;
        const std::shared_ptr<std::array<std::int32_t, 256>> PrevPulseBuffer;
        const std::shared_ptr<std::array<std::int32_t, 8>> LpcFilterBuffer;
        const std::shared_ptr<std::array<std::int32_t, 8>> InfluenceBuffer;
        const std::shared_ptr<std::vector<std::int16_t>> SampleBuffer;

        VxBuffers() noexcept = default;
        VxBuffers(
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
            std::shared_ptr<std::vector<std::int16_t>> sampleBuffer);
        VxBuffers(const VxBuffers&) noexcept = default;
        VxBuffers& operator=(const VxBuffers& other) noexcept;
    };

    struct Vector2ir
    {
        const std::int32_t X = 0;
        const std::int32_t Y = 0;

        Vector2ir() noexcept = default;
        Vector2ir(std::int32_t x, std::int32_t y) noexcept : X(x), Y(y) {}
        Vector2ir(const Vector2ir&) noexcept = default;
        Vector2ir& operator=(const Vector2ir& other) noexcept;
    };

    struct Block
    {
        const std::int32_t X;
        const std::int32_t Y;
        const std::int32_t W;
        const std::int32_t H;

        Block() noexcept : X(0), Y(0), W(0), H(0) {}
        Block(std::int32_t x, std::int32_t y, std::int32_t w, std::int32_t h) noexcept
            : X(x), Y(y), W(w), H(h)
        {
        }
        Block(const Block&) noexcept = default;
        Block& operator=(const Block& other) noexcept;

        [[nodiscard]] Block HalfLeft() const noexcept;
        [[nodiscard]] Block HalfRight() const noexcept;
        [[nodiscard]] Block HalfUp() const noexcept;
        [[nodiscard]] Block HalfDown() const noexcept;
    };

    class VxDecoder
    {
    public:
        static VxDecoder& Instance1();
        static VxDecoder& Instance2();

        const std::shared_ptr<std::array<char16_t, 4>> Magic
            = std::make_shared<std::array<char16_t, 4>>();
        std::int32_t FrameCount = 0;
        std::int32_t FrameWidth = 0;
        std::int32_t FrameHeight = 0;
        double FrameRate = 0.0;
        std::int32_t Quantizer = 0;
        std::int32_t AudioSampleRate = 0;
        std::int32_t AudioStreamCount = 0;
        std::int32_t MaxDataSize = 0;
        std::int32_t ExtradataOffset = 0;
        std::int32_t SeekTableOffset = 0;
        std::int32_t SeekTableCount = 0;
        const std::shared_ptr<AudioExtradata> Extradata
            = std::make_shared<AudioExtradata>();
        const std::shared_ptr<std::array<SeekTableEntry, 1>> SeekTable
            = std::make_shared<std::array<SeekTableEntry, 1>>();
        const std::shared_ptr<std::array<std::int32_t, 3>> QuantizerTable
            = std::make_shared<std::array<std::int32_t, 3>>();

        VxDecoder();

        void Reset();
        [[nodiscard]] MovieNativeRuntime::MovieTask ExportAll();
        [[nodiscard]] MovieNativeRuntime::MovieTask Export(const std::string& filePath);
        [[nodiscard]] MovieNativeRuntime::MovieTask Decode(
            const std::string& filePath, bool writeFiles = false, std::stop_token token = {});
        [[nodiscard]] MovieNativeRuntime::MovieTask Decode(
            std::shared_ptr<std::vector<std::uint8_t>> data, const std::string& filename,
            bool writeFiles = false, std::stop_token token = {});
        [[nodiscard]] MovieNativeRuntime::MovieTask Decode(
            std::shared_ptr<std::istream> stream, const std::string& filename,
            bool writeFiles = false, std::stop_token token = {});

        [[nodiscard]] bool GetImage(std::int32_t frameIndex, std::span<std::uint8_t> texture);
        [[nodiscard]] std::int32_t AudioFrameTotal() const noexcept
        {
            return _audioFrameTotal.load(std::memory_order_acquire);
        }
        [[nodiscard]] std::span<const std::int16_t> GetAudioBuffer(std::int32_t index) const;
        [[nodiscard]] std::int32_t FramesQueued() const noexcept
        {
            return _framesQueued.load(std::memory_order_relaxed);
        }

        [[nodiscard]] static constexpr std::int32_t SampleBufferCount() noexcept { return 5 * 12; }

        static bool UseStaticBuffers;

    private:
        std::shared_ptr<std::array<std::shared_ptr<VideoFrame>, 3>> _prevVideoFrames
            = std::make_shared<std::array<std::shared_ptr<VideoFrame>, 3>>();
        std::atomic<std::int32_t> _framesQueued{0};
        std::atomic<std::int32_t> _audioFrameTotal{0};
        std::atomic<std::uint64_t> _decodeGeneration{0};
        std::shared_ptr<std::vector<std::shared_ptr<VxFrame>>> _vxFrames{};
        mutable std::mutex _vxFramesMutex{};

        static const std::array<std::array<std::int32_t, 3>, 6> _quantizer4x4Table;

        std::shared_ptr<std::array<std::int16_t, 8>> _prevSampleBuffer
            = std::make_shared<std::array<std::int16_t, 8>>();
        std::shared_ptr<std::array<std::int32_t, 256>> _prevPulseBuffer
            = std::make_shared<std::array<std::int32_t, 256>>();
        std::shared_ptr<std::array<std::int32_t, 8>> _lpcFilterBuffer
            = std::make_shared<std::array<std::int32_t, 8>>();
        std::shared_ptr<std::array<std::int32_t, 8>> _influenceBuffer
            = std::make_shared<std::array<std::int32_t, 8>>();
        std::int32_t _nextSampleBufferIndex = 0;
        std::shared_ptr<std::vector<std::int16_t>> _sampleBuffer
            = std::make_shared<std::vector<std::int16_t>>(SampleBufferCount() * 128);

        static constexpr std::int32_t _mphMaxDataSize = 7602;
        std::shared_ptr<std::vector<std::uint8_t>> _dataBuffer
            = std::make_shared<std::vector<std::uint8_t>>(_mphMaxDataSize);

        static constexpr std::int32_t _mphFrameW = 256;
        static constexpr std::int32_t _mphFrameH = 192;
        std::int32_t _nextPlaneBufferIndex = 0;
        std::array<std::shared_ptr<ByteArray2D>, 12> _planeBuffers{};

        std::shared_ptr<ByteArray2D> _coeffBufferY
            = std::make_shared<ByteArray2D>(_mphFrameH / 4 + 1, _mphFrameW / 4 + 1);
        std::shared_ptr<ByteArray2D> _coeffBufferUV
            = std::make_shared<ByteArray2D>(_mphFrameH / 8 + 1, _mphFrameW / 8 + 1);
        std::shared_ptr<Vector2irArray2D> _vectors
            = std::make_shared<Vector2irArray2D>(_mphFrameH / 16 + 1, _mphFrameW / 16 + 2);

        [[nodiscard]] std::shared_ptr<std::vector<std::uint8_t>> GetDataBuffer();
        [[nodiscard]] std::array<std::shared_ptr<ByteArray2D>, 3> GetPlaneBuffers();
        [[nodiscard]] static ColorRgb YuvToRgb(std::int32_t y, std::int32_t u, std::int32_t v) noexcept;
        void DecodeCore(std::istream& stream, const std::string& filename, bool writeFiles, std::stop_token token);
        void WriteFile(std::span<std::uint8_t> pixelBuffer, const VxFrame& vxFrame,
            const std::string& folder, std::int32_t frameIndex);
    };

    class VxFrame
    {
    public:
        const std::shared_ptr<::MphRead::Formats::VideoFrame> VideoFrame;
        const std::int32_t AudioFrameCount;
        const std::shared_ptr<std::vector<std::shared_ptr<::MphRead::Formats::AudioFrame>>> AudioFrames;

        VxFrame(std::int32_t frameWidth, std::int32_t frameHeight, std::int32_t audioFrameCount,
            AudioExtradata& extradata, std::shared_ptr<AudioFrame> prevAudioFrame,
            const VxBuffers& buffers, std::int32_t sampleBufferIndex);

        void Decode(MovieNativeRuntime::BinaryReader& reader,
            const std::shared_ptr<std::vector<std::uint8_t>>& buffer, std::int32_t length);
    };

    class BitStreamReader
    {
    public:
        BitStreamReader(std::shared_ptr<std::vector<std::uint8_t>> buffer, std::int32_t length);

        [[nodiscard]] std::int32_t ReadBit();
        [[nodiscard]] std::int32_t ConsumeUntilNotZero();
        [[nodiscard]] std::int32_t ReadUnsignedExpGolomb();
        [[nodiscard]] std::int32_t ReadSignedExpGolomb();
        [[nodiscard]] std::int32_t ReadInt(std::int32_t bitCount);
        [[nodiscard]] std::int32_t ReadVLC2(const VLCData& vlc);
        void EnsureWordAlignment();

    private:
        const std::shared_ptr<std::vector<std::uint8_t>> _buffer;
        const std::int32_t _length;
        std::int32_t _bitPosition = 0;
    };

    class VideoFrame : public std::enable_shared_from_this<VideoFrame>
    {
    public:
        const std::int32_t FrameWidth;
        const std::int32_t FrameHeight;

        [[nodiscard]] const std::shared_ptr<ByteArray2D>& PlaneBufferY() const noexcept { return _planeBufferY; }
        [[nodiscard]] const std::shared_ptr<ByteArray2D>& PlaneBufferU() const noexcept { return _planeBufferU; }
        [[nodiscard]] const std::shared_ptr<ByteArray2D>& PlaneBufferV() const noexcept { return _planeBufferV; }

        VideoFrame(std::int32_t frameWidth, std::int32_t frameHeight, const VxBuffers& buffers);
        void Decode(BitStreamReader& reader);

    private:
        std::shared_ptr<ByteArray2D> _planeBufferY;
        std::shared_ptr<ByteArray2D> _planeBufferU;
        std::shared_ptr<ByteArray2D> _planeBufferV;
        const std::shared_ptr<ByteArray2D> _coeffBufferY;
        const std::shared_ptr<ByteArray2D> _coeffBufferUV;
        const std::shared_ptr<Vector2irArray2D> _vectors;
        const std::shared_ptr<std::array<std::shared_ptr<VideoFrame>, 3>> _prevVideoFrames;
        const std::shared_ptr<std::array<std::int32_t, 3>> _quantizerTable;
        BitStreamReader* _reader = nullptr;

        [[nodiscard]] static std::int32_t GetMiddleValue(
            std::int32_t a, std::int32_t b, std::int32_t c) noexcept;
        [[nodiscard]] static std::uint8_t PlaneBufferGetter(
            const ByteArray2D& planeBuffer, std::int32_t step, std::int32_t x, std::int32_t y);
        void DecodeBlock(Block block, Vector2ir predictionVector);
        void PredictInter(Block block, Vector2ir predictionVector, bool hasDelta,
            const std::shared_ptr<VideoFrame>& prevVideoFrame);
        void PredictInterDC(Block block);
        void PredictMBPlane(Block block);
        void DecodeResidueBlocks(Block block);
        [[nodiscard]] std::int32_t DecodeResidueCAVLC(
            std::int32_t x, std::int32_t y, std::int32_t nc,
            ByteArray2D& planeBuffer, std::int32_t step);
        void DecodeDct(std::int32_t x, std::int32_t y, ByteArray2D& planeBuffer,
            std::int32_t step, std::span<const std::int32_t> level);
        void PredictNoTile(Block block);
        void PredictVertical(Block block, ByteArray2D& planeBuffer, std::int32_t step);
        void PredictHorizontal(Block block, ByteArray2D& planeBuffer, std::int32_t step);
        void PredictDC(Block block, ByteArray2D& planeBuffer, std::int32_t step);
        void PredictPlane(Block block, ByteArray2D& planeBuffer, std::int32_t step, std::int32_t value);
        void PredictPlaneRecursive(Block block, ByteArray2D& planeBuffer, std::int32_t step);
        void PredictNoTileUV(Block block);
        void Predict4(Block block);
        void Predict4x4Vertical(ByteArray2D& planeBuffer, Vector2ir vec);
        void Predict4x4Horizontal(ByteArray2D& planeBuffer, Vector2ir vec);
        void Predict4x4Dc(ByteArray2D& planeBuffer, Vector2ir vec);
        void Predict4x4LeftDc(ByteArray2D& planeBuffer, Vector2ir vec);
        void Predict4x4TopDc(ByteArray2D& planeBuffer, Vector2ir vec);
        void Predict4x4Dc128(ByteArray2D& planeBuffer, Vector2ir vec);
        void Predict4x4DownLeft(ByteArray2D& planeBuffer, Vector2ir vec);
        void Predict4x4DownRight(ByteArray2D& planeBuffer, Vector2ir vec);
        void Predict4x4VerticalRight(ByteArray2D& planeBuffer, Vector2ir vec);
        void Predict4x4HorizontalDown(ByteArray2D& planeBuffer, Vector2ir vec);
        void Predict4x4VerticalLeft(ByteArray2D& planeBuffer, Vector2ir vec);
        void Predict4x4HorizontalUp(ByteArray2D& planeBuffer, Vector2ir vec);
        [[nodiscard]] static std::uint8_t GetCoeffBuffer(
            const ByteArray2D& buffer, std::int32_t step, std::int32_t x, std::int32_t y);
        static void SetCoeffBuffer(
            ByteArray2D& buffer, std::int32_t step, std::int32_t x, std::int32_t y, std::uint8_t value);
        void ClearTotalCoeff(Block block);

        static const std::array<std::int32_t, 32> _residueMaskTable;
        static const std::array<std::int32_t, 17> _tokenIndexTable;
        static const std::array<std::int32_t, 7> _suffixLimits;
        static const std::array<std::int32_t, 16> _zigzagScanTable;
    };

    class AudioFrame
    {
    public:
        AudioFrame(AudioExtradata& extradata, std::shared_ptr<AudioFrame> prevAudioFrame,
            const VxBuffers& buffers, std::int32_t sampleBufferIndex);

        [[nodiscard]] std::span<std::int16_t> SampleBuffer();
        [[nodiscard]] std::span<const std::int16_t> SampleBuffer() const;
        [[nodiscard]] std::int32_t Scale() const noexcept { return _scale; }
        void Decode(BitStreamReader& reader);

    private:
        AudioExtradata* const _extradata;
        const std::shared_ptr<AudioFrame> _prevAudioFrame;
        const std::shared_ptr<std::array<std::int16_t, 8>> _prevSampleBuffer;
        const std::shared_ptr<std::array<std::int32_t, 256>> _prevPulseBuffer;
        const std::shared_ptr<std::array<std::int32_t, 8>> _lpcFilterBuffer;
        const std::shared_ptr<std::array<std::int32_t, 8>> _influenceBuffer;
        const std::shared_ptr<std::vector<std::int16_t>> _sampleBuffer;
        const std::int32_t _sampleBufferIndex;
        std::int32_t _scale = 0;
        BitStreamReader* _reader = nullptr;

        static const std::array<std::int32_t, 4> _pulseDataLengths;
        static const std::array<std::int32_t, 4> _pulseDistances;
    };

    class VLCData
    {
    public:
        VLCData(std::span<const std::int32_t> lengthList, std::span<const std::int32_t> bitList);

        [[nodiscard]] std::int32_t MaxBitCount() const noexcept { return _maxBitCount; }
        [[nodiscard]] std::int32_t FindBitPattern(std::int32_t hashCode) const noexcept;

    private:
        std::unordered_map<std::int32_t, std::int32_t> _bitDict;
        std::int32_t _maxBitCount = 0;
    };

    class VLC final
    {
    public:
        [[nodiscard]] static std::int32_t Temp();
        [[nodiscard]] static const std::vector<VLCData>& CoeffTokenVlc();
        [[nodiscard]] static const std::vector<VLCData>& TotalZeroesVlc();
        [[nodiscard]] static const std::vector<VLCData>& RunVlc();
        [[nodiscard]] static const VLCData& Run7Vlc();

        VLC() = delete;

    private:
        struct State;
        [[nodiscard]] static const State& GetState();
    };
}
