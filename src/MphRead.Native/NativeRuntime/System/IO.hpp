#pragma once

// System.IO.File, Directory and Path: the members the game calls. Paths are
// UTF-8 strings.

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
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
    // On Windows the string is WTF-8 (see Encoding.hpp), so a file name with
    // a lone surrogate in it, which a C# string holds, survives the trip.
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
    // File.ReadAllLines(path): decoded as StreamReaderDecode does, with CR, LF
    // and CRLF all ending a line.
    [[nodiscard]] std::vector<std::string> FileReadAllLines(const std::string& path);
    // File.WriteAllLines(path, lines): every line followed by Environment.NewLine.
    void FileWriteAllLines(const std::string& path, const std::vector<std::string>& lines);
    // File.ReadAllText(path): decoded as StreamReaderDecode does -- a byte-order
    // mark picks UTF-32, UTF-8 or UTF-16 and is dropped, and anything else is
    // UTF-8, with ill-formed bytes as U+FFFD.
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
    // Path.Combine(path1, path2, path3, path4): a rooted later part replaces
    // what came before it, an empty part contributes nothing, and a separator
    // is inserted only where one is missing.
    [[nodiscard]] std::string PathCombine(std::string_view path1, std::string_view path2,
        std::string_view path3, std::string_view path4);
    // Path.GetExtension(path).
    [[nodiscard]] std::string PathGetExtension(std::string_view path);
    // Path.GetDirectoryName(path): empty for a path with no directory, as
    // .NET returns null there.
    [[nodiscard]] std::string PathGetDirectoryName(std::string_view path);
    // Path.GetTempPath(), ending in a separator.
    [[nodiscard]] std::string PathGetTempPath();
    // File.AppendAllText(path, contents).
    void FileAppendAllText(const std::string& path, std::string_view contents);
    // File.Delete(path): no error when it is not there.
    void FileDelete(const std::string& path);

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
