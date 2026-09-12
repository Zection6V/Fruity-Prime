#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace MphRead::Mods::Network
{
    struct DemoLibraryDateTime final
    {
        std::int64_t Ticks = 0;

        constexpr DemoLibraryDateTime() noexcept = default;
        explicit constexpr DemoLibraryDateTime(std::int64_t ticks) noexcept
            : Ticks(ticks)
        {
        }

        [[nodiscard]] constexpr std::int32_t CompareTo(DemoLibraryDateTime other) const noexcept
        {
            return Ticks < other.Ticks ? -1 : (Ticks > other.Ticks ? 1 : 0);
        }

        friend constexpr bool operator==(DemoLibraryDateTime, DemoLibraryDateTime) noexcept = default;
    };

    namespace Detail
    {
        class DemoLibraryIOException : public std::runtime_error
        {
        public:
            using std::runtime_error::runtime_error;
        };

        class DemoLibraryUnauthorizedAccessException : public std::runtime_error
        {
        public:
            using std::runtime_error::runtime_error;
        };

        // Pair-local runtime closure for the System.IO and current-culture
        // services used by DemoLibrary.cs. These operations intentionally keep
        // enumeration and FileInfo metadata lazy so the C# evaluation order,
        // exception filtering, and partial-list behavior remain observable.
        struct DemoLibraryFileEnumeratorHandle;
        struct DemoLibraryFileInfoHandle;

        [[nodiscard]] std::string DemoLibraryGetFullPath(const std::string& path);
        [[nodiscard]] std::string DemoLibraryGetFileName(const std::string& path);
        [[nodiscard]] std::string DemoLibraryGetFileNameWithoutExtension(const std::string& path);
        [[nodiscard]] bool DemoLibraryDirectoryExists(const std::string& path);

        [[nodiscard]] std::shared_ptr<DemoLibraryFileEnumeratorHandle>
            DemoLibraryEnumerateFiles(const std::string& directory, const std::string& pattern);
        [[nodiscard]] bool DemoLibraryFileEnumeratorMoveNext(
            const std::shared_ptr<DemoLibraryFileEnumeratorHandle>& enumerator);
        [[nodiscard]] std::string DemoLibraryFileEnumeratorCurrent(
            const std::shared_ptr<DemoLibraryFileEnumeratorHandle>& enumerator);
        void DemoLibraryFileEnumeratorDispose(
            const std::shared_ptr<DemoLibraryFileEnumeratorHandle>& enumerator);

        [[nodiscard]] std::shared_ptr<DemoLibraryFileInfoHandle>
            DemoLibraryCreateFileInfo(const std::string& path);
        [[nodiscard]] std::string DemoLibraryFileInfoName(
            const std::shared_ptr<DemoLibraryFileInfoHandle>& info);
        [[nodiscard]] DemoLibraryDateTime DemoLibraryFileInfoLastWriteTime(
            const std::shared_ptr<DemoLibraryFileInfoHandle>& info);
        [[nodiscard]] std::int64_t DemoLibraryFileInfoLength(
            const std::shared_ptr<DemoLibraryFileInfoHandle>& info);

        [[nodiscard]] std::string DemoLibraryFormatCurrentCultureDateTime(
            DemoLibraryDateTime value, std::string_view format);
        [[nodiscard]] std::string DemoLibraryFormatCurrentCultureInt64(std::int64_t value);
    }

    // C# internal readonly struct DemoRecording. This remains a value type in
    // Native code; copying a DemoRecording copies its complete value and there
    // are no mutating property setters.
    struct DemoRecording final
    {
        DemoRecording() = default;
        DemoRecording(std::string path, std::string room,
            DemoLibraryDateTime recorded, std::int64_t bytes);

        [[nodiscard]] const std::string& Path() const noexcept;
        [[nodiscard]] const std::string& Room() const noexcept;
        [[nodiscard]] DemoLibraryDateTime Recorded() const noexcept;
        [[nodiscard]] std::int64_t Bytes() const noexcept;
        [[nodiscard]] std::string FileName() const;

    private:
        std::string _path{};
        std::string _room{};
        DemoLibraryDateTime _recorded{};
        std::int64_t _bytes = 0;
    };

    // C# internal static class DemoLibrary.
    class DemoLibrary final
    {
    public:
        DemoLibrary() = delete;
        DemoLibrary(const DemoLibrary&) = delete;
        DemoLibrary& operator=(const DemoLibrary&) = delete;

        [[nodiscard]] static std::string Directory();
        [[nodiscard]] static std::shared_ptr<const std::vector<DemoRecording>> List();
        [[nodiscard]] static std::string Describe(const DemoRecording& demo);

    private:
        [[nodiscard]] static std::pair<std::string, std::optional<DemoLibraryDateTime>>
            ReadName(const std::string& fileName);
        [[nodiscard]] static std::string Size(std::int64_t bytes);
    };
}
