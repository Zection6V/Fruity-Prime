#pragma once

// System.IO.File, Directory and Path: the members the game calls. Paths are
// UTF-8 strings.

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace MphRead::NativeRuntime
{
    // File.ReadAllBytes(path).
    [[nodiscard]] std::vector<std::uint8_t> FileReadAllBytes(const std::string& path);
    // foreach (string file in Directory.EnumerateFiles(path)) visitor(file);
    void DirectoryEnumerateFiles(
        const std::string& path,
        const std::function<void(const std::string&)>& visitor);
    // Path.GetFileName(path).
    [[nodiscard]] std::string PathGetFileName(const std::string& path);
    // Path.GetPathRoot(path).Length.
    [[nodiscard]] std::size_t PathRootLength(std::string_view path) noexcept;
    // Path.GetFullPath(path).
    [[nodiscard]] std::string PathGetFullPath(const std::string& path);
    // File.Exists(path): false for a directory and for anything unreadable.
    [[nodiscard]] bool FileExists(const std::string& path) noexcept;
    // Directory.Exists(path).
    [[nodiscard]] bool DirectoryExists(const std::string& path) noexcept;
    // File.ReadAllLines(path): UTF-8, with CR, LF and CRLF all ending a line
    // and the byte-order mark stripped.
    [[nodiscard]] std::vector<std::string> FileReadAllLines(const std::string& path);
    // File.WriteAllLines(path, lines): every line followed by Environment.NewLine.
    void FileWriteAllLines(const std::string& path, const std::vector<std::string>& lines);
    // File.ReadAllText(path): UTF-8, byte-order mark stripped.
    [[nodiscard]] std::string FileReadAllText(const std::string& path);
    // File.WriteAllText(path, text).
    void FileWriteAllText(const std::string& path, std::string_view text);
    // Directory.CreateDirectory(path), parents included.
    void DirectoryCreateDirectory(const std::string& path);
    // new FileInfo(path): the members the demo library reads.
    struct FileInfo final
    {
        std::string Name;
        std::int64_t Length = 0;
        // LastWriteTime in DateTime ticks, local.
        std::int64_t LastWriteTimeTicks = 0;
    };

    [[nodiscard]] FileInfo CreateFileInfo(const std::string& path);
    // Directory.EnumerateFiles(path, searchPattern) with the "*suffix" form.
    [[nodiscard]] std::vector<std::string> DirectoryEnumerateFilesWithSuffix(
        const std::string& path, const std::string& suffix);
    // Path.GetFileNameWithoutExtension(path).
    [[nodiscard]] std::string PathGetFileNameWithoutExtension(const std::string& path);
    // Path.GetInvalidFileNameChars().
    [[nodiscard]] std::vector<char> PathGetInvalidFileNameChars();
    // Path.Combine(...): a rooted later part replaces what came before it, an
    // empty part contributes nothing, and a separator is inserted only where
    // one is missing.
    [[nodiscard]] std::string PathCombine(std::string_view path1, std::string_view path2);
    [[nodiscard]] std::string PathCombine(
        std::string_view path1, std::string_view path2, std::string_view path3);
    [[nodiscard]] std::string PathCombine(std::string_view path1, std::string_view path2,
        std::string_view path3, std::string_view path4);
    // Path.GetExtension(path).
    [[nodiscard]] std::string PathGetExtension(std::string_view path);

    // new DirectoryInfo(path): the members the game reads. A trailing
    // separator is not part of the name, as it is not in .NET.
    class DirectoryInfo final
    {
    public:
        explicit DirectoryInfo(std::string_view path);

        [[nodiscard]] const std::string& Name() const noexcept;
        [[nodiscard]] const std::string& FullName() const noexcept;
        [[nodiscard]] std::string Extension() const;
        // DirectoryInfo.Parent: null at a root.
        [[nodiscard]] std::shared_ptr<DirectoryInfo> Parent() const;

    private:
        std::string _fullName;
        std::string _name;
    };
}
