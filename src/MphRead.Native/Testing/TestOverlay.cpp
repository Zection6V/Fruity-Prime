#include "TestOverlay.hpp"

#include "../Formats/Formats.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
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

    [[nodiscard]] std::string PathToUtf8(const std::filesystem::path& path)
    {
#if defined(__cpp_char8_t)
        const std::u8string value = path.u8string();
        std::string result;
        result.reserve(value.size());
        for (char8_t ch : value)
        {
            result.push_back(static_cast<char>(ch));
        }
        return result;
#else
        return path.u8string();
#endif
    }

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

    [[nodiscard]] std::string ReplaceAll(
        std::string value,
        const std::string& oldValue,
        const std::string& newValue)
    {
        if (oldValue.empty())
        {
            throw std::invalid_argument(
                "The value cannot be an empty string. (Parameter 'oldValue')");
        }
        std::size_t position = 0;
        while ((position = value.find(oldValue, position)) != std::string::npos)
        {
            value.replace(position, oldValue.size(), newValue);
            position += newValue.size();
        }
        return value;
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
        std::vector<std::string> result;
        for (const std::filesystem::directory_entry& entry :
            std::filesystem::recursive_directory_iterator(PathFromUtf8(root)))
        {
            if (entry.is_directory())
            {
                result.push_back(PathToUtf8(entry.path()));
            }
        }
        return result;
    }

    [[nodiscard]] std::vector<std::string> EnumerateFiles(
        const std::string& directory)
    {
        std::vector<std::string> result;
        for (const std::filesystem::directory_entry& entry :
            std::filesystem::directory_iterator(PathFromUtf8(directory)))
        {
            if (!entry.is_directory())
            {
                result.push_back(PathToUtf8(entry.path()));
            }
        }
        return result;
    }

    [[nodiscard]] std::vector<std::uint8_t> ReadAllBytes(const std::string& path)
    {
        std::ifstream stream(PathFromUtf8(path), std::ios::binary);
        if (!stream.is_open())
        {
            throw std::ios_base::failure(
                "Could not open file for reading: " + path);
        }
        stream.exceptions(std::ios::badbit);
        return std::vector<std::uint8_t>(
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>());
    }
}

namespace MphRead::Testing
{
    const std::shared_ptr<std::vector<std::int32_t>> TestOverlay::OverlayMap =
        std::make_shared<std::vector<std::int32_t>>(
            std::initializer_list<std::int32_t>{
                4,
                6,
                17,
                5,
                16,
                0,
                7,
                1,
                2,
                3,
                8,
                15,
                10,
                9,
                11,
                12,
                13,
                14});

    void TestOverlay::CompareGames(const std::string& game1, const std::string& game2)
    {
        const std::string fileSystemDirectory = GetDirectoryName(Paths::FileSystem());
        const std::string root1 = Paths::Combine(fileSystemDirectory, game1);
        const std::string root2 = Paths::Combine(fileSystemDirectory, game2);

        std::vector<std::string> dirs1 = EnumerateDirectoriesRecursive(root1);
        for (std::string& directory : dirs1)
        {
            directory = ReplaceAll(std::move(directory), root1, "");
        }
        std::vector<std::string> dirs2 = EnumerateDirectoriesRecursive(root2);
        for (std::string& directory : dirs2)
        {
            directory = ReplaceAll(std::move(directory), root2, "");
        }

        std::unordered_map<std::string, std::vector<std::string>> files1;
        std::unordered_map<std::string, std::vector<std::string>> files2;
        for (const std::string& directory : dirs1)
        {
            auto [entry, inserted] = files1.emplace(directory, std::vector<std::string>{});
            if (!inserted)
            {
                throw std::invalid_argument("An item with the same key has already been added.");
            }
            for (const std::string& file : EnumerateFiles(Paths::Combine(root1, directory)))
            {
                entry->second.push_back(GetFileName(file));
            }
        }
        for (const std::string& directory : dirs2)
        {
            auto [entry, inserted] = files2.emplace(directory, std::vector<std::string>{});
            if (!inserted)
            {
                throw std::invalid_argument("An item with the same key has already been added.");
            }
            for (const std::string& file : EnumerateFiles(Paths::Combine(root2, directory)))
            {
                entry->second.push_back(GetFileName(file));
            }
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
            std::cout << "Directories in " << game1 << " not in " << game2 << ":\n";
            for (const std::string& directory : dir1not2)
            {
                std::cout << "-- " << directory << '\n';
            }
            std::cout << '\n';
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
            std::cout << "Directories in " << game2 << " not in " << game1 << ":\n";
            for (const std::string& directory : dir2not1)
            {
                std::cout << "-- " << directory << '\n';
            }
            std::cout << '\n';
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
                std::cout << directory << '\n';
            }
            if (!file1not2.empty())
            {
                std::cout << "Files in " << game1 << " not in " << game2 << ":\n";
                for (const std::string& file : file1not2)
                {
                    std::cout << "-- " << file << '\n';
                }
            }
            if (!file2not1.empty())
            {
                std::cout << "Files in " << game2 << " not in " << game1 << ":\n";
                for (const std::string& file : file2not1)
                {
                    std::cout << "-- " << file << '\n';
                }
            }
            if (!file1not2.empty() || !file2not1.empty())
            {
                std::cout << '\n';
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
                    ReadAllBytes(Paths::Combine(root1, directory, file));
                const std::vector<std::uint8_t> bytes2 =
                    ReadAllBytes(Paths::Combine(root2, directory, file));
                if (bytes1 != bytes2)
                {
                    changes.push_back(file);
                }
            }
            if (!changes.empty())
            {
                std::cout << directory << '\n';
                std::cout << "Changed files:\n";
                for (const std::string& file : changes)
                {
                    std::cout << file << '\n';
                }
                std::cout << '\n';
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
        for (std::size_t i = 0; i < active.size(); i++)
        {
            if (i != 0)
            {
                std::cout << ", ";
            }
            std::cout << active[i];
        }
        std::cout << '\n';
        Nop();
    }

    void TestOverlay::Nop()
    {
    }
}
