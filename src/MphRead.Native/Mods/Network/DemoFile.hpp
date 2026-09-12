#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace MphRead::Mods::Network
{
    namespace Detail
    {
        // Pair-local runtime closure for the System.IO / System.IO.Compression
        // services used by DemoFile.cs. The eventual runtime owner must preserve
        // the .NET semantics documented by these operation names, including the
        // exception categories observed by DemoReader.
        struct DemoFileStreamHandle;
        struct DemoDeflateStreamHandle;

        void DemoFilePrepareDirectory(const std::string& path);
        [[nodiscard]] std::shared_ptr<DemoFileStreamHandle>
            DemoFileOpenCreateWriteShareRead(const std::string& path);
        [[nodiscard]] std::shared_ptr<DemoFileStreamHandle>
            DemoFileOpenReadShareRead(const std::string& path);

        void DemoFileStreamWrite(const std::shared_ptr<DemoFileStreamHandle>& stream,
            std::span<const std::uint8_t> data);
        void DemoFileStreamWriteByte(const std::shared_ptr<DemoFileStreamHandle>& stream,
            std::uint8_t value);
        [[nodiscard]] std::size_t DemoFileStreamReadAtLeast(
            const std::shared_ptr<DemoFileStreamHandle>& stream,
            std::span<std::uint8_t> destination, bool throwOnEndOfStream);
        void DemoFileStreamFlush(const std::shared_ptr<DemoFileStreamHandle>& stream);
        void DemoFileStreamDispose(const std::shared_ptr<DemoFileStreamHandle>& stream);

        [[nodiscard]] std::shared_ptr<DemoDeflateStreamHandle>
            DemoFileCreateDeflateFastest(
                const std::shared_ptr<DemoFileStreamHandle>& stream, bool leaveOpen);
        [[nodiscard]] std::shared_ptr<DemoDeflateStreamHandle>
            DemoFileCreateDeflateDecompress(
                const std::shared_ptr<DemoFileStreamHandle>& stream, bool leaveOpen);
        void DemoDeflateStreamWrite(const std::shared_ptr<DemoDeflateStreamHandle>& stream,
            std::span<const std::uint8_t> data);
        [[nodiscard]] std::size_t DemoDeflateStreamReadAtLeast(
            const std::shared_ptr<DemoDeflateStreamHandle>& stream,
            std::span<std::uint8_t> destination, bool throwOnEndOfStream);
        void DemoDeflateStreamFlush(const std::shared_ptr<DemoDeflateStreamHandle>& stream);
        void DemoDeflateStreamDispose(const std::shared_ptr<DemoDeflateStreamHandle>& stream);

        // NetConfig.ProtocolVersion is owned by NetProtocol.cs and has no Native
        // owner on develop2 yet. Keep the dependency explicit rather than
        // duplicating that protocol constant here.
        [[nodiscard]] std::int32_t DemoFileProtocolVersion();
    }

    class DemoFile final
    {
    public:
        static std::array<std::uint8_t, 4> Magic;
        static constexpr std::uint8_t FormatVersion = 2;
        static constexpr std::string_view Extension = ".fpdemo";
        static constexpr std::int32_t HeaderSize = 4 + 1 + 1;
        static constexpr std::uint8_t LongGap = 0xFF;

        DemoFile() = delete;
        DemoFile(const DemoFile&) = delete;
        DemoFile& operator=(const DemoFile&) = delete;
    };

    class DemoWriter final
    {
    public:
        explicit DemoWriter(const std::string& path);

        void WriteRecord(std::uint32_t frame, std::span<const std::uint8_t> data);
        void Dispose();

        DemoWriter(const DemoWriter&) = delete;
        DemoWriter& operator=(const DemoWriter&) = delete;
        DemoWriter(DemoWriter&&) = delete;
        DemoWriter& operator=(DemoWriter&&) = delete;

    private:
        static constexpr std::uint32_t FlushIntervalFrames = 15;

        std::shared_ptr<Detail::DemoFileStreamHandle> _stream;
        std::shared_ptr<Detail::DemoDeflateStreamHandle> _deflate;
        std::uint32_t _lastFrame = 0;
        std::uint32_t _lastFlushFrame = 0;
        std::array<std::uint8_t, 7> _header{};
    };

    struct DemoRecord final
    {
        const std::uint32_t Frame;
        const std::shared_ptr<std::vector<std::uint8_t>> Data;

        DemoRecord();
        DemoRecord(std::uint32_t frame, std::shared_ptr<std::vector<std::uint8_t>> data);
        DemoRecord(const DemoRecord&) = default;
        DemoRecord(DemoRecord&&) noexcept = default;
        DemoRecord& operator=(const DemoRecord& other);
        DemoRecord& operator=(DemoRecord&& other) noexcept;
    };

    class DemoReader final
    {
    public:
        [[nodiscard]] static std::unique_ptr<DemoReader> Open(const std::string& path);
        [[nodiscard]] std::optional<DemoRecord> ReadNext();
        [[nodiscard]] std::uint8_t ProtocolVersion() const;
        void Dispose();

        DemoReader(const DemoReader&) = delete;
        DemoReader& operator=(const DemoReader&) = delete;
        DemoReader(DemoReader&&) = delete;
        DemoReader& operator=(DemoReader&&) = delete;

    private:
        DemoReader(std::shared_ptr<Detail::DemoFileStreamHandle> stream,
            std::uint8_t protocolVersion);

        [[nodiscard]] bool Fill(std::span<std::uint8_t> destination);

        std::shared_ptr<Detail::DemoFileStreamHandle> _stream;
        std::shared_ptr<Detail::DemoDeflateStreamHandle> _deflate;
        std::array<std::uint8_t, 7> _header{};
        std::uint32_t _frame = 0;
        std::uint8_t _protocolVersion = 0;
    };
}
