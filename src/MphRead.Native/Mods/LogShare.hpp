#pragma once

#include "NativeRuntime/System/AtomicSharedPtr.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace MphRead::Mods
{
    class LogFileInfo final
    {
    public:
        [[nodiscard]] const std::u16string& FullName() const noexcept;
        [[nodiscard]] const std::u16string& Name() const noexcept;
        [[nodiscard]] std::chrono::system_clock::time_point LastWriteTimeUtc() const noexcept;

    private:
        friend class LogArchive;

        LogFileInfo(std::filesystem::path path, std::u16string fullName,
            std::u16string name, std::int64_t lastWriteTicks);

        std::filesystem::path _path;
        std::u16string _fullName;
        std::u16string _name;
        std::int64_t _lastWriteTicks = 0;
    };

    class LogArchive final
    {
    public:
        LogArchive() = delete;
        ~LogArchive() = delete;
        LogArchive(const LogArchive&) = delete;
        LogArchive& operator=(const LogArchive&) = delete;
        LogArchive(LogArchive&&) = delete;
        LogArchive& operator=(LogArchive&&) = delete;

        [[nodiscard]] static std::u16string Directory();
        [[nodiscard]] static std::vector<LogFileInfo> Files();
        [[nodiscard]] static bool Any();
        [[nodiscard]] static bool Create(std::u16string_view zipPath, std::u16string& error);
        [[nodiscard]] static std::u16string FileName();
    };

    class ILogShare
    {
    public:
        virtual ~ILogShare() = default;

        [[nodiscard]] virtual std::u16string StagingPath(std::u16string_view fileName) = 0;
        [[nodiscard]] virtual bool Share(std::u16string_view path,
            std::u16string_view subject, std::u16string& error) = 0;
    };

    class LogShare final
    {
    public:
        LogShare() = delete;
        ~LogShare() = delete;
        LogShare(const LogShare&) = delete;
        LogShare& operator=(const LogShare&) = delete;
        LogShare(LogShare&&) = delete;
        LogShare& operator=(LogShare&&) = delete;

        [[nodiscard]] static std::shared_ptr<ILogShare> Current();
        static void Current(std::shared_ptr<ILogShare> value);
        [[nodiscard]] static bool Available();

    private:
        static ::MphRead::NativeRuntime::AtomicSharedPtr<ILogShare> _current;
    };
}
