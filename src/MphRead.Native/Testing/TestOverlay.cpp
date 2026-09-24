#include "TestOverlay.hpp"

#include "../Formats/Formats.hpp"
#include "../NativeRuntime/System/Globalization.hpp"
#include "../NativeRuntime/System/IO.hpp"

#include <algorithm>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

using ::MphRead::NativeRuntime::FileReadAllBytes;
using ::MphRead::NativeRuntime::PathFromUtf8;
using ::MphRead::NativeRuntime::PathToUtf8;
using ::MphRead::NativeRuntime::StringReplace;

namespace
{
    [[nodiscard]] std::string GetDirectoryName(const std::string& path)
    {
        if (path.empty())
        {
            return {};
        }
        const std::filesystem::path nativePath = PathFromUtf8(path);
        const std::filesystem::path rootPath = nativePath.root_path();
        if (!rootPath.empty() && nativePath == rootPath)
        {
            return {};
        }
        return PathToUtf8(nativePath.parent_path());
    }

    [[nodiscard]] std::string GetFileName(const std::string& path)
    {
        return PathToUtf8(PathFromUtf8(path).filename());
    }

    [[nodiscard]] bool Contains(
        const std::vector<std::string>& values,
        const std::string& value)
    {
        return std::find(values.begin(), values.end(), value) != values.end();
    }

    [[nodiscard]] std::vector<std::string> EnumerateDirectoriesRecursive(
        const std::string& root)
    {
        struct PendingDirectory final
        {
            std::filesystem::path Path;
            std::int32_t RemainingDepth;
        };

        std::vector<std::string> result;
        std::deque<PendingDirectory> pending;
        pending.push_back(PendingDirectory{
            PathFromUtf8(root),
            std::numeric_limits<std::int32_t>::max()});

        while (!pending.empty())
        {
            PendingDirectory current = std::move(pending.front());
            pending.pop_front();
            for (const std::filesystem::directory_entry& entry :
                std::filesystem::directory_iterator(current.Path))
            {
                if (entry.is_directory())
                {
                    if (current.RemainingDepth > 0)
                    {
                        pending.push_back(PendingDirectory{
                            entry.path(),
                            current.RemainingDepth - 1});
                    }
                    result.push_back(StringReplace(PathToUtf8(entry.path()), root, ""));
                }
            }
        }
        return result;
    }

    void AppendFiles(
        const std::string& directory,
        std::vector<std::string>& files)
    {
        for (const std::filesystem::directory_entry& entry :
            std::filesystem::directory_iterator(PathFromUtf8(directory)))
        {
            if (!entry.is_directory())
            {
                files.push_back(GetFileName(PathToUtf8(entry.path())));
            }
        }
    }

    [[nodiscard]] std::recursive_mutex& ConsoleMutex()
    {
        static std::recursive_mutex mutex;
        return mutex;
    }

    void WriteLine(const std::string& value)
    {
        const std::lock_guard<std::recursive_mutex> lock(ConsoleMutex());
        std::cout << value << '\n';
        std::cout.flush();
    }

    void WriteLine()
    {
        const std::lock_guard<std::recursive_mutex> lock(ConsoleMutex());
        std::cout << '\n';
        std::cout.flush();
    }

    [[noreturn]] void ThrowDuplicateKey(const std::string& key)
    {
        throw std::invalid_argument(
            "An item with the same key has already been added. Key: " + key);
    }
}

namespace MphRead::Testing
{
    const std::shared_ptr<const std::vector<std::int32_t>> TestOverlay::OverlayMap = []
    {
        auto values = std::make_shared<std::vector<std::int32_t>>();
        values->push_back(4);
        values->push_back(6);
        values->push_back(17);
        values->push_back(5);
        values->push_back(16);
        values->push_back(0);
        values->push_back(7);
        values->push_back(1);
        values->push_back(2);
        values->push_back(3);
        values->push_back(8);
        values->push_back(15);
        values->push_back(10);
        values->push_back(9);
        values->push_back(11);
        values->push_back(12);
        values->push_back(13);
        values->push_back(14);
        return values;
    }();

    void TestOverlay::CompareGames(const std::string& game1, const std::string& game2)
    {
        const std::string root1 = Paths::Combine(
            GetDirectoryName(Paths::FileSystem()), game1);
        const std::string root2 = Paths::Combine(
            GetDirectoryName(Paths::FileSystem()), game2);

        const std::vector<std::string> dirs1 = EnumerateDirectoriesRecursive(root1);
        const std::vector<std::string> dirs2 = EnumerateDirectoriesRecursive(root2);

        std::unordered_map<std::string, std::vector<std::string>> files1;
        std::unordered_map<std::string, std::vector<std::string>> files2;
        for (const std::string& directory : dirs1)
        {
            auto [entry, inserted] = files1.emplace(directory, std::vector<std::string>{});
            if (!inserted)
            {
                ThrowDuplicateKey(directory);
            }
            AppendFiles(Paths::Combine(root1, directory), entry->second);
        }
        for (const std::string& directory : dirs2)
        {
            auto [entry, inserted] = files2.emplace(directory, std::vector<std::string>{});
            if (!inserted)
            {
                ThrowDuplicateKey(directory);
            }
            AppendFiles(Paths::Combine(root2, directory), entry->second);
        }

        std::vector<std::string> dir1not2;
        for (const std::string& directory : dirs1)
        {
            if (!Contains(dirs2, directory))
            {
                dir1not2.push_back(directory);
            }
        }
        if (!dir1not2.empty())
        {
            WriteLine("Directories in " + game1 + " not in " + game2 + ":");
            for (const std::string& directory : dir1not2)
            {
                WriteLine("-- " + directory);
            }
            WriteLine();
        }

        std::vector<std::string> dir2not1;
        for (const std::string& directory : dirs2)
        {
            if (!Contains(dirs1, directory))
            {
                dir2not1.push_back(directory);
            }
        }
        if (!dir2not1.empty())
        {
            WriteLine("Directories in " + game2 + " not in " + game1 + ":");
            for (const std::string& directory : dir2not1)
            {
                WriteLine("-- " + directory);
            }
            WriteLine();
        }

        for (const std::string& directory : dirs1)
        {
            if (!Contains(dirs2, directory))
            {
                continue;
            }
            const std::vector<std::string>& directoryFiles1 = files1.at(directory);
            const std::vector<std::string>& directoryFiles2 = files2.at(directory);
            std::vector<std::string> file1not2;
            for (const std::string& file : directoryFiles1)
            {
                if (!Contains(directoryFiles2, file))
                {
                    file1not2.push_back(file);
                }
            }
            std::vector<std::string> file2not1;
            for (const std::string& file : directoryFiles2)
            {
                if (!Contains(directoryFiles1, file))
                {
                    file2not1.push_back(file);
                }
            }
            if (!file1not2.empty() || !file2not1.empty())
            {
                WriteLine(directory);
            }
            if (!file1not2.empty())
            {
                WriteLine("Files in " + game1 + " not in " + game2 + ":");
                for (const std::string& file : file1not2)
                {
                    WriteLine("-- " + file);
                }
            }
            if (!file2not1.empty())
            {
                WriteLine("Files in " + game2 + " not in " + game1 + ":");
                for (const std::string& file : file2not1)
                {
                    WriteLine("-- " + file);
                }
            }
            if (!file1not2.empty() || !file2not1.empty())
            {
                WriteLine();
            }
        }

        for (const std::string& directory : dirs1)
        {
            if (!Contains(dirs2, directory))
            {
                continue;
            }
            std::vector<std::string> changes;
            const std::vector<std::string>& directoryFiles1 = files1.at(directory);
            const std::vector<std::string>& directoryFiles2 = files2.at(directory);
            for (const std::string& file : directoryFiles1)
            {
                if (!Contains(directoryFiles2, file))
                {
                    continue;
                }
                const std::vector<std::uint8_t> bytes1 =
                    FileReadAllBytes(Paths::Combine(root1, directory, file));
                const std::vector<std::uint8_t> bytes2 =
                    FileReadAllBytes(Paths::Combine(root2, directory, file));
                if (bytes1 != bytes2)
                {
                    changes.push_back(file);
                }
            }
            if (!changes.empty())
            {
                WriteLine(directory);
                WriteLine("Changed files:");
                for (const std::string& file : changes)
                {
                    WriteLine(file);
                }
                WriteLine();
            }
        }
        Nop();
    }

    void TestOverlay::Translate(std::int32_t mask)
    {
        mask = 0x21;
        std::vector<std::int32_t> active;
        for (std::int32_t i = 0; i < 18; i++)
        {
            if ((mask & (1 << i)) != 0)
            {
                active.push_back(OverlayMap->at(static_cast<std::size_t>(i)));
            }
        }
        std::sort(active.begin(), active.end());
        std::string line;
        for (std::size_t i = 0; i < active.size(); i++)
        {
            if (i != 0)
            {
                line += ", ";
            }
            line += std::to_string(active[i]);
        }
        WriteLine(line);
        Nop();
    }

    void TestOverlay::Nop()
    {
    }
}
