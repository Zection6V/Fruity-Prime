#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace MphRead
{
    class Extract final
    {
    private:
        template <std::size_t N>
        class ByValAnsiCharArray final
        {
        public:
            using ManagedStorage = std::array<char16_t, N>;

            ByValAnsiCharArray() noexcept = default;

            explicit ByValAnsiCharArray(const std::uint8_t* bytes)
            {
                SetMarshaledBytes(bytes);
            }

            ByValAnsiCharArray(const ByValAnsiCharArray& other)
                : _wire(other.WireBytes())
            {
                if (auto storage = other.TryGetStorage())
                {
                    SetStorage(std::move(storage));
                }
            }

            ByValAnsiCharArray& operator=(const ByValAnsiCharArray& other)
            {
                if (this != std::addressof(other))
                {
                    const auto wire = other.WireBytes();
                    auto storage = other.TryGetStorage();
                    _wire = wire;
                    if (storage)
                    {
                        SetStorage(std::move(storage));
                    }
                    else
                    {
                        ClearStorage();
                    }
                }
                return *this;
            }

            ~ByValAnsiCharArray() noexcept
            {
                ClearStorageNoThrow();
            }

            [[nodiscard]] bool IsNull() const
            {
                return !TryGetStorage();
            }

            [[nodiscard]] constexpr std::size_t Length() const noexcept
            {
                return N;
            }

            [[nodiscard]] char16_t& operator[](std::size_t index) const
            {
                return RequireStorage()->at(index);
            }

            [[nodiscard]] std::string MarshalString() const
            {
                const auto storage = RequireStorage();
                std::string result;
                for (char16_t value : *storage)
                {
                    if (value == u'\0')
                    {
                        break;
                    }
                    const std::uint32_t codePoint = static_cast<std::uint32_t>(value);
                    if (codePoint <= 0x7FU)
                    {
                        result.push_back(static_cast<char>(codePoint));
                    }
                    else if (codePoint <= 0x7FFU)
                    {
                        result.push_back(static_cast<char>(0xC0U | (codePoint >> 6)));
                        result.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
                    }
                    else
                    {
                        result.push_back(static_cast<char>(0xE0U | (codePoint >> 12)));
                        result.push_back(static_cast<char>(0x80U | ((codePoint >> 6) & 0x3FU)));
                        result.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
                    }
                }
                return result;
            }

            void SetMarshaledBytes(const std::uint8_t* bytes) const
            {
                if (bytes == nullptr)
                {
                    throw std::invalid_argument("Value cannot be null. (Parameter 'bytes')");
                }
                auto storage = std::make_shared<ManagedStorage>();
                for (std::size_t i = 0; i < N; ++i)
                {
                    _wire[i] = bytes[i];
                    (*storage)[i] = static_cast<char16_t>(bytes[i]);
                }
                SetStorage(std::move(storage));
            }

            [[nodiscard]] const std::array<std::uint8_t, N>& WireBytes() const
            {
                if (auto storage = TryGetStorage())
                {
                    for (std::size_t i = 0; i < N; ++i)
                    {
                        _wire[i] = static_cast<std::uint8_t>((*storage)[i] & 0x00FFU);
                    }
                }
                return _wire;
            }

        private:
            struct Registry final
            {
                std::mutex Mutex;
                std::unordered_map<const ByValAnsiCharArray<N>*, std::shared_ptr<ManagedStorage>> Storage;
            };

            [[nodiscard]] static Registry& GetRegistry()
            {
                static Registry* registry = new Registry();
                return *registry;
            }

            [[nodiscard]] std::shared_ptr<ManagedStorage> TryGetStorage() const
            {
                Registry& registry = GetRegistry();
                std::lock_guard<std::mutex> lock(registry.Mutex);
                const auto iterator = registry.Storage.find(this);
                return iterator == registry.Storage.end() ? nullptr : iterator->second;
            }

            [[nodiscard]] std::shared_ptr<ManagedStorage> RequireStorage() const
            {
                auto storage = TryGetStorage();
                if (!storage)
                {
                    throw std::invalid_argument("Value cannot be null. (Parameter 'array')");
                }
                return storage;
            }

            void SetStorage(std::shared_ptr<ManagedStorage> storage) const
            {
                Registry& registry = GetRegistry();
                std::lock_guard<std::mutex> lock(registry.Mutex);
                registry.Storage[this] = std::move(storage);
            }

            void ClearStorage() const
            {
                Registry& registry = GetRegistry();
                std::lock_guard<std::mutex> lock(registry.Mutex);
                registry.Storage.erase(this);
            }

            void ClearStorageNoThrow() const noexcept
            {
                try
                {
                    ClearStorage();
                }
                catch (...)
                {
                }
            }

            mutable std::array<std::uint8_t, N> _wire{};
        };

        template <typename T, std::size_t N>
        [[nodiscard]] static T LoadNative(const std::array<std::uint8_t, N>& raw, std::size_t offset) noexcept
        {
            T value{};
            std::memcpy(static_cast<void*>(std::addressof(value)), raw.data() + offset, sizeof(T));
            return value;
        }

    public:
        class FileInfo;

        class DirInfo
        {
        public:
            const std::string Name;
            const std::uint32_t Index;
            std::shared_ptr<std::vector<std::shared_ptr<DirInfo>>> Subdirectories
                = std::make_shared<std::vector<std::shared_ptr<DirInfo>>>();
            std::shared_ptr<std::vector<std::shared_ptr<FileInfo>>> Files
                = std::make_shared<std::vector<std::shared_ptr<FileInfo>>>();

            DirInfo(std::string name, std::uint32_t index)
                : Name(std::move(name)), Index(index)
            {
            }
        };

        class FileInfo
        {
        public:
            const std::string Name;
            const std::uint32_t Index;

            FileInfo(std::string name, std::uint32_t index)
                : Name(std::move(name)), Index(index)
            {
            }
        };

        struct DirTableEntry
        {
            const std::uint32_t Offset = 0;
            const std::uint16_t FirstFileIndex = 0;
            const std::uint16_t DirNum = 0;

            constexpr DirTableEntry() noexcept = default;

            DirTableEntry(const DirTableEntry&) noexcept = default;

            DirTableEntry& operator=(const DirTableEntry& other) noexcept
            {
                if (this != std::addressof(other))
                {
                    this->~DirTableEntry();
                    std::construct_at(this, other);
                }
                return *this;
            }

            [[nodiscard]] static DirTableEntry FromMarshaledBytes(
                const std::array<std::uint8_t, 8>& raw) noexcept
            {
                return DirTableEntry(
                    LoadNative<std::uint32_t>(raw, 0),
                    LoadNative<std::uint16_t>(raw, 4),
                    LoadNative<std::uint16_t>(raw, 6));
            }

        private:
            constexpr DirTableEntry(
                std::uint32_t offset, std::uint16_t firstFileIndex, std::uint16_t dirNum) noexcept
                : Offset(offset), FirstFileIndex(firstFileIndex), DirNum(dirNum)
            {
            }
        };

#pragma pack(push, 1)
        struct RomHeader
        {
            const ByValAnsiCharArray<12> Title{};
            const ByValAnsiCharArray<4> GameCode{};
            const ByValAnsiCharArray<2> MakerCode{};
            const std::uint8_t UnitCode = 0;
            const std::uint8_t Seed = 0;
            const std::uint8_t Capacity = 0;
            const ByValAnsiCharArray<7> Reserved15{};
            const std::uint8_t Reserved16 = 0;
            const std::uint8_t Region = 0;
            const std::uint8_t Version = 0;
            const std::uint8_t AutoStart = 0;
            const std::int32_t ARM9Offset = 0;
            const std::int32_t ARM9EntryAddress = 0;
            const std::int32_t ARM9RamAddress = 0;
            const std::int32_t ARM9Size = 0;
            const std::int32_t ARM7Offset = 0;
            const std::int32_t ARM7EntryAddress = 0;
            const std::int32_t ARM7RamAddress = 0;
            const std::int32_t ARM7Size = 0;
            const std::uint32_t FntOffset = 0;
            const std::uint32_t FntSize = 0;
            const std::uint32_t FatOffset = 0;
            const std::uint32_t FatSize = 0;
            const std::int32_t Overlay9Offset = 0;
            const std::int32_t Overlay9Size = 0;
            const std::int32_t Overlay7Offset = 0;
            const std::int32_t Overlay7Size = 0;
            const std::uint32_t ReadFlags = 0;
            const std::uint32_t InitFlags = 0;
            const std::int32_t BannerOffset = 0;

            RomHeader() noexcept = default;
            RomHeader(const RomHeader&) = default;

            RomHeader& operator=(const RomHeader& other)
            {
                if (this != std::addressof(other))
                {
                    this->~RomHeader();
                    std::construct_at(this, other);
                }
                return *this;
            }

            [[nodiscard]] static RomHeader FromMarshaledBytes(
                const std::array<std::uint8_t, 108>& raw)
            {
                return RomHeader(
                    ByValAnsiCharArray<12>(raw.data() + 0x00),
                    ByValAnsiCharArray<4>(raw.data() + 0x0C),
                    ByValAnsiCharArray<2>(raw.data() + 0x10),
                    raw[0x12],
                    raw[0x13],
                    raw[0x14],
                    ByValAnsiCharArray<7>(raw.data() + 0x15),
                    raw[0x1C],
                    raw[0x1D],
                    raw[0x1E],
                    raw[0x1F],
                    LoadNative<std::int32_t>(raw, 0x20),
                    LoadNative<std::int32_t>(raw, 0x24),
                    LoadNative<std::int32_t>(raw, 0x28),
                    LoadNative<std::int32_t>(raw, 0x2C),
                    LoadNative<std::int32_t>(raw, 0x30),
                    LoadNative<std::int32_t>(raw, 0x34),
                    LoadNative<std::int32_t>(raw, 0x38),
                    LoadNative<std::int32_t>(raw, 0x3C),
                    LoadNative<std::uint32_t>(raw, 0x40),
                    LoadNative<std::uint32_t>(raw, 0x44),
                    LoadNative<std::uint32_t>(raw, 0x48),
                    LoadNative<std::uint32_t>(raw, 0x4C),
                    LoadNative<std::int32_t>(raw, 0x50),
                    LoadNative<std::int32_t>(raw, 0x54),
                    LoadNative<std::int32_t>(raw, 0x58),
                    LoadNative<std::int32_t>(raw, 0x5C),
                    LoadNative<std::uint32_t>(raw, 0x60),
                    LoadNative<std::uint32_t>(raw, 0x64),
                    LoadNative<std::int32_t>(raw, 0x68));
            }

        private:
            RomHeader(
                ByValAnsiCharArray<12> title,
                ByValAnsiCharArray<4> gameCode,
                ByValAnsiCharArray<2> makerCode,
                std::uint8_t unitCode,
                std::uint8_t seed,
                std::uint8_t capacity,
                ByValAnsiCharArray<7> reserved15,
                std::uint8_t reserved16,
                std::uint8_t region,
                std::uint8_t version,
                std::uint8_t autoStart,
                std::int32_t arm9Offset,
                std::int32_t arm9EntryAddress,
                std::int32_t arm9RamAddress,
                std::int32_t arm9Size,
                std::int32_t arm7Offset,
                std::int32_t arm7EntryAddress,
                std::int32_t arm7RamAddress,
                std::int32_t arm7Size,
                std::uint32_t fntOffset,
                std::uint32_t fntSize,
                std::uint32_t fatOffset,
                std::uint32_t fatSize,
                std::int32_t overlay9Offset,
                std::int32_t overlay9Size,
                std::int32_t overlay7Offset,
                std::int32_t overlay7Size,
                std::uint32_t readFlags,
                std::uint32_t initFlags,
                std::int32_t bannerOffset)
                : Title(std::move(title)),
                  GameCode(std::move(gameCode)),
                  MakerCode(std::move(makerCode)),
                  UnitCode(unitCode),
                  Seed(seed),
                  Capacity(capacity),
                  Reserved15(std::move(reserved15)),
                  Reserved16(reserved16),
                  Region(region),
                  Version(version),
                  AutoStart(autoStart),
                  ARM9Offset(arm9Offset),
                  ARM9EntryAddress(arm9EntryAddress),
                  ARM9RamAddress(arm9RamAddress),
                  ARM9Size(arm9Size),
                  ARM7Offset(arm7Offset),
                  ARM7EntryAddress(arm7EntryAddress),
                  ARM7RamAddress(arm7RamAddress),
                  ARM7Size(arm7Size),
                  FntOffset(fntOffset),
                  FntSize(fntSize),
                  FatOffset(fatOffset),
                  FatSize(fatSize),
                  Overlay9Offset(overlay9Offset),
                  Overlay9Size(overlay9Size),
                  Overlay7Offset(overlay7Offset),
                  Overlay7Size(overlay7Size),
                  ReadFlags(readFlags),
                  InitFlags(initFlags),
                  BannerOffset(bannerOffset)
            {
            }
        };
#pragma pack(pop)

        static void Setup(const std::string& path);
        static void LoadRuntimeData();

        Extract() = delete;
        Extract(const Extract&) = delete;
        Extract& operator=(const Extract&) = delete;
    };

    static_assert(std::is_standard_layout_v<Extract::DirTableEntry>);
    static_assert(sizeof(Extract::DirTableEntry) == 8);
    static_assert(offsetof(Extract::DirTableEntry, Offset) == 0);
    static_assert(offsetof(Extract::DirTableEntry, FirstFileIndex) == 4);
    static_assert(offsetof(Extract::DirTableEntry, DirNum) == 6);

    static_assert(std::is_standard_layout_v<Extract::RomHeader>);
    static_assert(sizeof(Extract::RomHeader) == 0x6C);
    static_assert(offsetof(Extract::RomHeader, GameCode) == 0x0C);
    static_assert(offsetof(Extract::RomHeader, Version) == 0x1E);
    static_assert(offsetof(Extract::RomHeader, ARM9Offset) == 0x20);
    static_assert(offsetof(Extract::RomHeader, FntOffset) == 0x40);
    static_assert(offsetof(Extract::RomHeader, FatOffset) == 0x48);
    static_assert(offsetof(Extract::RomHeader, Overlay9Offset) == 0x50);
    static_assert(offsetof(Extract::RomHeader, BannerOffset) == 0x68);
}
