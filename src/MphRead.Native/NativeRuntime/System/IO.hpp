#pragma once

// System.IO.File, Directory and Path: the members the game calls. Paths are
// UTF-8 strings.

#include <cstdint>
#include <functional>
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
    // Path.GetInvalidFileNameChars().
    [[nodiscard]] std::vector<char> PathGetInvalidFileNameChars();
}
