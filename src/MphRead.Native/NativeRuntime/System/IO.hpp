#pragma once

// System.IO.File, Directory and Path: the members the game calls. Paths are
// UTF-8 strings.

#include <cstdint>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace MphRead::NativeRuntime
{
    // A UTF-8 string as a std::filesystem::path, and back. Every path in the
    // program is a UTF-8 std::string, as every path in the C# is a string;
    // these are the only two places one crosses into std::filesystem.
    // std::filesystem::path(std::string) and path::string() use the ANSI code
    // page on Windows, so a path with any non-ASCII character in it -- a
    // Japanese user name is enough -- names a different file or throws.
    [[nodiscard]] std::filesystem::path PathFromUtf8(std::string_view value);
    [[nodiscard]] std::string PathToUtf8(const std::filesystem::path& value);
    // Path.IsPathRooted(path).
    [[nodiscard]] bool PathIsPathRooted(std::string_view path) noexcept;
    // Path.Combine(first, second), as .NET (Core) does it: a rooted second
    // wins, an empty side is the other side, and a separator is added only
    // when neither side has one at the join.
    [[nodiscard]] std::string PathCombine(std::string_view first, std::string_view second);
    [[nodiscard]] std::string PathCombine(
        std::string_view first, std::string_view second, std::string_view third);
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
    [[nodiscard]] bool FileExists(std::string_view path) noexcept;
    // Directory.Exists(path).
    [[nodiscard]] bool DirectoryExists(std::string_view path) noexcept;
    // File.ReadAllLines(path): UTF-8, with CR, LF and CRLF all ending a line
    // and the byte-order mark stripped.
    [[nodiscard]] std::vector<std::string> FileReadAllLines(const std::string& path);
    // File.WriteAllLines(path, lines): every line followed by Environment.NewLine.
    void FileWriteAllLines(const std::string& path, const std::vector<std::string>& lines);
    // File.ReadAllText(path): UTF-8, byte-order mark stripped.
    [[nodiscard]] std::string FileReadAllText(const std::string& path);
    // File.WriteAllText(path, text).
    void FileWriteAllText(const std::string& path, std::string_view text);
    // File.WriteAllBytes(path, bytes).
    void FileWriteAllBytes(const std::string& path, std::span<const std::uint8_t> bytes);
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
}
