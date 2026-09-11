#include "Program.hpp"
#include "Export/Images.hpp"
#include "Formats/Formats.hpp"
#include "Formats/Movie.hpp"
#include "Formats/Sound.hpp"
#include "Menu.hpp"
#include "Metadata/Metadata.hpp"
#include "Metadata/Rooms.hpp"
#include "Mods/Branding.hpp"
#include "Mods/ConsoleWindow.hpp"
#include "Mods/ModEntry.hpp"
#include "Read.hpp"
#include "Renderer.hpp"
#include "Utility/Console.hpp"
#include "Utility/Extract.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#if defined(_WIN32)
#include <conio.h>
#include <cstdio>
#include <io.h>
#include <windows.h>
#else
#include <poll.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#endif
namespace
{
const System::Version MinExtractVersion(0, 19, 0, 0);
struct Argument final
{
std::string Name;
std::optional<std::string> ValueOne;
std::optional<std::string> ValueTwo;
Argument(std::string name, std::optional<std::string> valueOne, std::optional<std::string> valueTwo = std::nullopt)
: Name(std::move(name)), ValueOne(std::move(valueOne)), ValueTwo(std::move(valueTwo))
{
}
};
[[nodiscard]] bool IsDotNetTrimWhitespace(std::uint32_t codePoint) noexcept
{
if (codePoint >= 0x0009 && codePoint <= 0x000D) return true;
switch (codePoint)
{
case 0x0020: case 0x0085: case 0x00A0: case 0x1680:
case 0x2000: case 0x2001: case 0x2002: case 0x2003:
case 0x2004: case 0x2005: case 0x2006: case 0x2007:
case 0x2008: case 0x2009: case 0x200A: case 0x2028:
case 0x2029: case 0x202F: case 0x205F: case 0x3000:
return true;
default: return false;
}
}
[[nodiscard]] bool IsNumberWhitespace(unsigned char ch) noexcept
{
return ch == 0x20 || (ch >= 0x09 && ch <= 0x0D);
}
struct Utf8CodePoint final
{
std::uint32_t Value;
std::size_t Length;
};
[[nodiscard]] std::optional<Utf8CodePoint> DecodeUtf8Forward(std::string_view text, std::size_t position) noexcept
{
if (position >= text.size()) return std::nullopt;
const auto first = static_cast<unsigned char>(text[position]);
if (first <= 0x7F) return Utf8CodePoint{first, 1};
std::uint32_t value = 0;
std::size_t length = 0;
std::uint32_t minimum = 0;
if ((first & 0xE0U) == 0xC0U) { value = first & 0x1FU; length = 2; minimum = 0x80; }
else if ((first & 0xF0U) == 0xE0U) { value = first & 0x0FU; length = 3; minimum = 0x800; }
else if ((first & 0xF8U) == 0xF0U) { value = first & 0x07U; length = 4; minimum = 0x10000; }
else return std::nullopt;
if (position + length > text.size()) return std::nullopt;
for (std::size_t i = 1; i < length; ++i)
{
const auto next = static_cast<unsigned char>(text[position + i]);
if ((next & 0xC0U) != 0x80U) return std::nullopt;
value = (value << 6) | (next & 0x3FU);
}
if (value < minimum || value > 0x10FFFFU || (value >= 0xD800U && value <= 0xDFFFU)) return std::nullopt;
return Utf8CodePoint{value, length};
}
[[nodiscard]] std::optional<std::pair<Utf8CodePoint, std::size_t>> DecodeUtf8Backward(std::string_view text, std::size_t end) noexcept
{
if (end == 0 || end > text.size()) return std::nullopt;
std::size_t start = end - 1;
while (start > 0 && (static_cast<unsigned char>(text[start]) & 0xC0U) == 0x80U) --start;
const auto decoded = DecodeUtf8Forward(text, start);
if (!decoded.has_value() || start + decoded->Length != end) return std::nullopt;
return std::make_pair(*decoded, start);
}
[[nodiscard]] std::string_view TrimDotNetWhitespace(std::string_view text) noexcept
{
std::size_t first = 0;
std::size_t last = text.size();
while (first < last)
{
const auto decoded = DecodeUtf8Forward(text.substr(0, last), first);
if (!decoded.has_value() || !IsDotNetTrimWhitespace(decoded->Value)) break;
first += decoded->Length;
}
while (last > first)
{
const auto decoded = DecodeUtf8Backward(text, last);
if (!decoded.has_value() || !IsDotNetTrimWhitespace(decoded->first.Value)) break;
last = decoded->second;
}
return text.substr(first, last - first);
}
[[nodiscard]] bool TryParseInt32(std::string_view input, std::int32_t& result) noexcept
{
result = 0;
if (input.empty()) return false;
std::size_t index = 0;
while (index < input.size() && IsNumberWhitespace(static_cast<unsigned char>(input[index]))) ++index;
if (index == input.size()) return false;
bool negative = false;
if (input[index] == '+' || input[index] == '-') { negative = input[index] == '-'; ++index; }
if (index == input.size() || input[index] < '0' || input[index] > '9') return false;
constexpr std::uint64_t PositiveLimit = static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max());
constexpr std::uint64_t NegativeLimit = PositiveLimit + 1ULL;
const std::uint64_t limit = negative ? NegativeLimit : PositiveLimit;
std::uint64_t value = 0;
bool overflow = false;
while (index < input.size() && input[index] >= '0' && input[index] <= '9')
{
const std::uint64_t digit = static_cast<std::uint64_t>(input[index] - '0');
if (value > (limit - digit) / 10ULL) overflow = true;
else if (!overflow) value = value * 10ULL + digit;
++index;
}
while (index < input.size() && IsNumberWhitespace(static_cast<unsigned char>(input[index]))) ++index;
while (index < input.size() && input[index] == '\0') ++index;
if (index != input.size() || overflow) return false;
if (!negative) result = static_cast<std::int32_t>(value);
else if (value == NegativeLimit) result = std::numeric_limits<std::int32_t>::min();
else result = -static_cast<std::int32_t>(value);
return true;
}
[[nodiscard]] bool StartsWithDash(std::string_view text) noexcept
{
return !text.empty() && text.front() == '-';
}
[[nodiscard]] std::string ToLowerInvariantForProgram(std::string_view text)
{
std::string result(text);
for (char& ch : result) if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
return result;
}
[[nodiscard]] std::filesystem::path PathFromUtf8(std::string_view value)
{
#if defined(__cpp_char8_t)
std::u8string converted;
converted.reserve(value.size());
for (unsigned char ch : value) converted.push_back(static_cast<char8_t>(ch));
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
for (char8_t ch : value) result.push_back(static_cast<char>(ch));
return result;
#else
return path.u8string();
#endif
}
[[nodiscard]] bool FileExists(std::string_view path) noexcept
{
if (path.empty() || path.find('\0') != std::string_view::npos) return false;
#if defined(_WIN32)
if (path.back() == '/' || path.back() == '\\') return false;
#else
if (path.back() == '/') return false;
#endif
try
{
const std::filesystem::path nativePath = PathFromUtf8(path);
#if defined(_WIN32)
const DWORD attributes = GetFileAttributesW(nativePath.c_str());
return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
struct stat info{};
if (::lstat(nativePath.c_str(), &info) != 0) return false;
if (S_ISLNK(info.st_mode))
{
struct stat target{};
if (::stat(nativePath.c_str(), &target) != 0) return true;
return !S_ISDIR(target.st_mode);
}
return !S_ISDIR(info.st_mode);
#endif
}
catch (...) { return false; }
}
void AppendUtf8(std::string& output, std::uint32_t codePoint)
{
if (codePoint > 0x10FFFFU || (codePoint >= 0xD800U && codePoint <= 0xDFFFU)) codePoint = 0xFFFDU;
if (codePoint <= 0x7FU) output.push_back(static_cast<char>(codePoint));
else if (codePoint <= 0x7FFU)
{
output.push_back(static_cast<char>(0xC0U | (codePoint >> 6)));
output.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
}
else if (codePoint <= 0xFFFFU)
{
output.push_back(static_cast<char>(0xE0U | (codePoint >> 12)));
output.push_back(static_cast<char>(0x80U | ((codePoint >> 6) & 0x3FU)));
output.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
}
else
{
output.push_back(static_cast<char>(0xF0U | (codePoint >> 18)));
output.push_back(static_cast<char>(0x80U | ((codePoint >> 12) & 0x3FU)));
output.push_back(static_cast<char>(0x80U | ((codePoint >> 6) & 0x3FU)));
output.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
}
}
[[nodiscard]] std::string DecodeUtf8Text(std::string_view bytes)
{
std::string output;
output.reserve(bytes.size());
for (std::size_t index = 0; index < bytes.size();)
{
const auto decoded = DecodeUtf8Forward(bytes, index);
if (decoded.has_value()) { output.append(bytes.substr(index, decoded->Length)); index += decoded->Length; }
else { AppendUtf8(output, 0xFFFDU); ++index; }
}
return output;
}
[[nodiscard]] std::string DecodeUtf16Text(std::string_view bytes, bool bigEndian)
{
std::string output;
output.reserve(bytes.size());
auto readUnit = [&](std::size_t index) -> std::uint16_t
{
const auto first = static_cast<unsigned char>(bytes[index]);
const auto second = static_cast<unsigned char>(bytes[index + 1]);
return bigEndian ? static_cast<std::uint16_t>((first << 8) | second) : static_cast<std::uint16_t>(first | (second << 8));
};
std::size_t index = 0;
while (index + 1 < bytes.size())
{
const std::uint16_t first = readUnit(index);
index += 2;
if (first >= 0xD800U && first <= 0xDBFFU)
{
if (index + 1 < bytes.size())
{
const std::uint16_t second = readUnit(index);
if (second >= 0xDC00U && second <= 0xDFFFU)
{
index += 2;
const std::uint32_t codePoint = 0x10000U + ((static_cast<std::uint32_t>(first) - 0xD800U) << 10) + (static_cast<std::uint32_t>(second) - 0xDC00U);
AppendUtf8(output, codePoint);
continue;
}
}
AppendUtf8(output, 0xFFFDU);
}
else if (first >= 0xDC00U && first <= 0xDFFFU) AppendUtf8(output, 0xFFFDU);
else AppendUtf8(output, first);
}
if (index < bytes.size()) AppendUtf8(output, 0xFFFDU);
return output;
}
[[nodiscard]] std::string DecodeUtf32Text(std::string_view bytes, bool bigEndian)
{
std::string output;
output.reserve(bytes.size());
std::size_t index = 0;
while (index + 3 < bytes.size())
{
const auto b0 = static_cast<unsigned char>(bytes[index]);
const auto b1 = static_cast<unsigned char>(bytes[index + 1]);
const auto b2 = static_cast<unsigned char>(bytes[index + 2]);
const auto b3 = static_cast<unsigned char>(bytes[index + 3]);
index += 4;
const std::uint32_t codePoint = bigEndian
? (static_cast<std::uint32_t>(b0) << 24) | (static_cast<std::uint32_t>(b1) << 16) | (static_cast<std::uint32_t>(b2) << 8) | static_cast<std::uint32_t>(b3)
: static_cast<std::uint32_t>(b0) | (static_cast<std::uint32_t>(b1) << 8) | (static_cast<std::uint32_t>(b2) << 16) | (static_cast<std::uint32_t>(b3) << 24);
AppendUtf8(output, codePoint);
}
if (index < bytes.size()) AppendUtf8(output, 0xFFFDU);
return output;
}
[[nodiscard]] std::string ReadAllText(std::string_view path)
{
std::ifstream stream(PathFromUtf8(path), std::ios::in | std::ios::binary);
if (!stream.is_open()) throw std::ios_base::failure("Could not open file for reading: " + std::string(path));
stream.exceptions(std::ios::badbit);
const std::string bytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
auto byteAt = [&](std::size_t index) -> unsigned char { return static_cast<unsigned char>(bytes[index]); };
if (bytes.size() >= 4 && byteAt(0) == 0xFF && byteAt(1) == 0xFE && byteAt(2) == 0x00 && byteAt(3) == 0x00) return DecodeUtf32Text(std::string_view(bytes).substr(4), false);
if (bytes.size() >= 4 && byteAt(0) == 0x00 && byteAt(1) == 0x00 && byteAt(2) == 0xFE && byteAt(3) == 0xFF) return DecodeUtf32Text(std::string_view(bytes).substr(4), true);
if (bytes.size() >= 3 && byteAt(0) == 0xEF && byteAt(1) == 0xBB && byteAt(2) == 0xBF) return DecodeUtf8Text(std::string_view(bytes).substr(3));
if (bytes.size() >= 2 && byteAt(0) == 0xFF && byteAt(1) == 0xFE) return DecodeUtf16Text(std::string_view(bytes).substr(2), false);
if (bytes.size() >= 2 && byteAt(0) == 0xFE && byteAt(1) == 0xFF) return DecodeUtf16Text(std::string_view(bytes).substr(2), true);
return DecodeUtf8Text(bytes);
}
[[nodiscard]] std::string GetFileNameWithoutExtension(std::string_view path)
{
#if defined(_WIN32)
const std::size_t separator = path.find_last_of("/\\");
#else
const std::size_t separator = path.find_last_of('/');
#endif
const std::string_view fileName = separator == std::string_view::npos ? path : path.substr(separator + 1);
const std::size_t period = fileName.find_last_of('.');
return period == std::string_view::npos ? std::string(fileName) : std::string(fileName.substr(0, period));
}
void WriteLine() { std::cout << '\n'; }
void WriteLine(std::string_view value) { std::cout << value << '\n'; }
#if !defined(_WIN32)
std::deque<unsigned char> PendingConsoleBytes;
[[nodiscard]] std::optional<unsigned char> ReadConsoleByte(bool wait, int timeoutMilliseconds)
{
if (!PendingConsoleBytes.empty())
{
const unsigned char value = PendingConsoleBytes.front();
PendingConsoleBytes.pop_front();
return value;
}
if (!wait)
{
pollfd descriptor{STDIN_FILENO, POLLIN, 0};
const int ready = ::poll(&descriptor, 1, timeoutMilliseconds);
if (ready <= 0 || (descriptor.revents & POLLIN) == 0) return std::nullopt;
}
unsigned char value = 0;
while (true)
{
const ssize_t count = ::read(STDIN_FILENO, &value, 1);
if (count == 1) return value;
if (count < 0 && errno == EINTR) continue;
throw std::runtime_error("Could not read a key from the console.");
}
}
void EchoConsoleBytes(const std::vector<unsigned char>& bytes)
{
if (bytes.empty()) return;
std::cout.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
std::cout.flush();
}
#endif
void ReadKey()
{
std::cout.flush();
#if defined(_WIN32)
if (_isatty(_fileno(stdin)) == 0) throw std::runtime_error("Console.ReadKey cannot be used when input is redirected.");
const wint_t first = _getwche();
if (first == 0 || first == 0xE0) (void)_getwch();
#else
if (::isatty(STDIN_FILENO) == 0) throw std::runtime_error("Console.ReadKey cannot be used when input is redirected.");
termios original{};
if (::tcgetattr(STDIN_FILENO, &original) != 0) throw std::runtime_error("Could not read console mode for Console.ReadKey.");
termios current = original;
current.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
current.c_cc[VMIN] = 1;
current.c_cc[VTIME] = 0;
if (::tcsetattr(STDIN_FILENO, TCSANOW, &current) != 0) throw std::runtime_error("Could not configure the console for Console.ReadKey.");
struct TerminalRestore final { termios Value; ~TerminalRestore() { (void)::tcsetattr(STDIN_FILENO, TCSANOW, &Value); } } restore{original};
const unsigned char first = *ReadConsoleByte(true, 0);
std::vector<unsigned char> keyBytes{first};
bool echo = true;
if (first == 0x1B)
{
const auto second = ReadConsoleByte(false, 30);
if (second.has_value())
{
keyBytes.push_back(*second);
if (*second == '[' || *second == 'O')
{
echo = false;
while (true)
{
const auto next = ReadConsoleByte(false, 30);
if (!next.has_value()) break;
keyBytes.push_back(*next);
if (*next >= 0x40 && *next <= 0x7E) break;
}
}
else keyBytes.erase(keyBytes.begin());
}
}
else if ((first & 0x80U) != 0)
{
std::size_t expected = 1;
if ((first & 0xE0U) == 0xC0U) expected = 2;
else if ((first & 0xF0U) == 0xE0U) expected = 3;
else if ((first & 0xF8U) == 0xF0U) expected = 4;
while (keyBytes.size() < expected)
{
const unsigned char next = *ReadConsoleByte(true, 0);
if ((next & 0xC0U) != 0x80U) { PendingConsoleBytes.push_front(next); break; }
keyBytes.push_back(next);
}
}
if (echo) EchoConsoleBytes(keyBytes);
#endif
}
[[nodiscard]] bool AnyName(const std::vector<Argument>& arguments, std::string_view name) noexcept
{
for (const Argument& argument : arguments) if (argument.Name == name) return true;
return false;
}
[[nodiscard]] bool TryGetArgument(const std::vector<Argument>& arguments, std::string_view fullName, std::string_view shortName, const Argument*& argument) noexcept
{
for (const Argument& item : arguments)
{
if (item.Name == fullName || item.Name == shortName) { argument = &item; return true; }
}
argument = nullptr;
return false;
}
[[nodiscard]] bool TryGetString(const std::vector<Argument>& arguments, std::string_view fullName, std::string_view shortName, std::optional<std::string>& value)
{
const Argument* argument = nullptr;
if (TryGetArgument(arguments, fullName, shortName, argument) && argument->ValueOne.has_value()) { value = argument->ValueOne; return true; }
value = std::nullopt;
return false;
}
[[nodiscard]] bool TryGetInt(const std::vector<Argument>& arguments, std::string_view fullName, std::string_view shortName, std::int32_t& value)
{
std::optional<std::string> stringValue;
if (TryGetString(arguments, fullName, shortName, stringValue))
{
std::int32_t intValue = 0;
if (TryParseInt32(*stringValue, intValue)) { value = intValue; return true; }
}
value = 0;
return false;
}
class PairRange final
{
public:
class Iterator final
{
public:
using value_type = std::pair<std::string, std::int32_t>;
using difference_type = std::ptrdiff_t;
using iterator_category = std::input_iterator_tag;
Iterator(const std::vector<Argument>* arguments, std::string_view fullName, std::string_view shortName, std::size_t index)
: _arguments(arguments), _fullName(fullName), _shortName(shortName), _index(index) { AdvanceToMatch(); }
[[nodiscard]] value_type operator*() const
{
const Argument& argument = (*_arguments)[_index];
std::int32_t valueTwo = 0;
if (argument.ValueTwo.has_value()) (void)TryParseInt32(*argument.ValueTwo, valueTwo);
return std::make_pair(*argument.ValueOne, valueTwo);
}
Iterator& operator++() { ++_index; AdvanceToMatch(); return *this; }
void operator++(int) { ++(*this); }
friend bool operator==(const Iterator& left, const Iterator& right) noexcept { return left._arguments == right._arguments && left._index == right._index; }
friend bool operator!=(const Iterator& left, const Iterator& right) noexcept { return !(left == right); }
private:
void AdvanceToMatch()
{
while (_arguments != nullptr && _index < _arguments->size())
{
const Argument& argument = (*_arguments)[_index];
if ((argument.Name == _fullName || argument.Name == _shortName) && argument.ValueOne.has_value()) return;
++_index;
}
}
const std::vector<Argument>* _arguments;
std::string_view _fullName;
std::string_view _shortName;
std::size_t _index;
};
PairRange(const std::vector<Argument>& arguments, std::string_view fullName, std::string_view shortName)
: _arguments(arguments), _fullName(fullName), _shortName(shortName) {}
[[nodiscard]] Iterator begin() const { return Iterator(&_arguments, _fullName, _shortName, 0); }
[[nodiscard]] Iterator end() const { return Iterator(&_arguments, _fullName, _shortName, _arguments.size()); }
private:
const std::vector<Argument>& _arguments;
std::string_view _fullName;
std::string_view _shortName;
};
[[nodiscard]] PairRange GetPairs(const std::vector<Argument>& arguments, std::string_view fullName, std::string_view shortName)
{
return PairRange(arguments, fullName, shortName);
}
[[nodiscard]] std::vector<Argument> ParseArguments(const std::vector<std::string>& args)
{
std::vector<Argument> arguments;
for (std::size_t i = 0; i < args.size(); ++i)
{
std::string arg = args[i];
if (StartsWithDash(arg) && arg.size() > 1)
{
arg.erase(0, 1);
if (i == args.size() - 1) arguments.emplace_back(std::move(arg), std::nullopt);
else
{
const std::string& valueOne = args[i + 1];
if (StartsWithDash(valueOne)) arguments.emplace_back(std::move(arg), std::nullopt);
else
{
std::optional<std::string> valueTwo;
if (i < args.size() - 2 && !StartsWithDash(args[i + 2])) { valueTwo = args[i + 2]; ++i; }
arguments.emplace_back(std::move(arg), valueOne, std::move(valueTwo));
++i;
}
}
}
}
return arguments;
}
[[nodiscard]] bool CheckVersion()
{
std::string text = ReadAllText("paths.txt");
const std::size_t newline = text.find('\n');
if (newline != std::string::npos) text.resize(newline);
const std::string_view trimmed = TrimDotNetWhitespace(text);
const auto extractVersion = System::Version::TryParse(trimmed);
return extractVersion.has_value() && *extractVersion >= MinExtractVersion;
}
[[nodiscard]] bool CheckSetup(const std::vector<std::string>& args)
{
if (FileExists("paths.txt") && !CheckVersion())
{
WriteLine("Your paths.txt file is not compatible with this version of " + std::string(MphRead::Mods::Branding::Name) + " and needs to be recreated.");
WriteLine("It is recommended that you delete the file as well as any extracted game files, then perform setup again.");
WriteLine();
WriteLine("Press any key to exit...");
ReadKey();
return true;
}
if (args.size() == 1 && !StartsWithDash(args[0]) && FileExists(args[0])) { MphRead::Extract::Setup(args[0]); return true; }
if (!FileExists("paths.txt"))
{
WriteLine("Could not find the paths.txt file.");
WriteLine("You may need to perform first-time setup by dragging a ROM onto the " + MphRead::Mods::Branding::Executable() + " executable.");
WriteLine();
WriteLine("Press any key to exit...");
ReadKey();
return true;
}
MphRead::Paths::UpdatePaths();
MphRead::Paths::ChooseMphPath();
MphRead::Paths::ChooseFhPath();
return false;
}
void Nop() {}
[[noreturn]] void Exit()
{
Nop();
WriteLine(MphRead::Mods::Branding::Executable() + " usage:");
WriteLine("    -room <room_name -or- room_id>");
WriteLine("    -model <model_name> [recolor_index]");
WriteLine("At most one room may be specified. Any number of models may be specified.");
WriteLine("To load First Hunt models, include -fh in the argument list.");
WriteLine("Available room options: -mode, -players, -boss, -node, -entity");
WriteLine("- or -");
WriteLine("    -extract <archive_path>");
WriteLine("If the target archive is LZ10-compressed, it will be decompressed.");
WriteLine("- or -");
WriteLine("    -export <target_name>");
WriteLine("The export target may be a model or room name.");
std::cout.flush();
std::exit(1);
}
}
namespace System
{
Version::Version(std::int32_t major, std::int32_t minor) : _major(major), _minor(minor), _build(-1), _revision(-1)
{
if (major < 0) throw std::out_of_range("major");
if (minor < 0) throw std::out_of_range("minor");
}
Version::Version(std::int32_t major, std::int32_t minor, std::int32_t build) : _major(major), _minor(minor), _build(build), _revision(-1)
{
if (major < 0) throw std::out_of_range("major");
if (minor < 0) throw std::out_of_range("minor");
if (build < 0) throw std::out_of_range("build");
}
Version::Version(std::int32_t major, std::int32_t minor, std::int32_t build, std::int32_t revision) : _major(major), _minor(minor), _build(build), _revision(revision)
{
if (major < 0) throw std::out_of_range("major");
if (minor < 0) throw std::out_of_range("minor");
if (build < 0) throw std::out_of_range("build");
if (revision < 0) throw std::out_of_range("revision");
}
std::optional<Version> Version::TryParse(std::string_view text) noexcept
{
std::array<std::int32_t, 4> components{};
std::size_t componentCount = 0;
std::size_t start = 0;
while (true)
{
if (componentCount == components.size()) return std::nullopt;
const std::size_t separator = text.find('.', start);
const std::string_view component = separator == std::string_view::npos ? text.substr(start) : text.substr(start, separator - start);
std::int32_t value = 0;
if (!TryParseInt32(component, value) || value < 0) return std::nullopt;
components[componentCount++] = value;
if (separator == std::string_view::npos) break;
start = separator + 1;
}
if (componentCount < 2 || componentCount > 4) return std::nullopt;
try
{
if (componentCount == 2) return Version(components[0], components[1]);
if (componentCount == 3) return Version(components[0], components[1], components[2]);
return Version(components[0], components[1], components[2], components[3]);
}
catch (...) { return std::nullopt; }
}
bool operator>=(const Version& left, const Version& right) noexcept
{
if (left._major != right._major) return left._major > right._major;
if (left._minor != right._minor) return left._minor > right._minor;
if (left._build != right._build) return left._build > right._build;
return left._revision >= right._revision;
}
}
namespace MphRead
{
const System::Version Program::Version(0, 35, 1, 0);
ProgramException::ProgramException(const std::string& message) : std::runtime_error(message) {}
void Program::Main(const std::vector<std::string>& args)
{
ConsoleSetup::Run();
#if defined(_WIN32)
Mods::ConsoleWindow::Prepare(args);
#endif
if (Mods::ModEntry::TryHandleHeadless(args)) return;
if (CheckSetup(args)) return;
const std::vector<Argument> arguments = ParseArguments(args);
if (Mods::ModEntry::TryHandle(args)) return;
if (arguments.empty()) Menu::ShowMenuPrompts();
else if (AnyName(arguments, "setup"))
{
const std::string archivesPath = Paths::Combine(Paths::FileSystem(), "archives");
for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(PathFromUtf8(archivesPath)))
{
std::error_code statusError;
const bool isDirectory = entry.is_directory(statusError);
if (!isDirectory)
{
const std::string path = PathToUtf8(entry.path());
Read::ExtractArchive(GetFileNameWithoutExtension(path));
}
}
}
else
{
std::optional<std::string> exportValue;
if (TryGetString(arguments, "export", "e", exportValue))
{
if (ToLowerInvariantForProgram(*exportValue) == "layer2d") Export::Images::ExportHudLayers();
else if (ToLowerInvariantForProgram(*exportValue) == "object2d") Export::Images::ExportHudObjects();
else if (ToLowerInvariantForProgram(*exportValue) == "sfx") Formats::Sound::SoundRead::ExportSamples();
else if (ToLowerInvariantForProgram(*exportValue) == "wfs") Formats::Sound::SoundRead::ExportWfsSamples();
else if (ToLowerInvariantForProgram(*exportValue) == "strm") Formats::Sound::SoundRead::ExportStreams();
else if (ToLowerInvariantForProgram(*exportValue) == "fhsfx") Formats::Sound::SoundRead::ExportAllFh();
else if (ToLowerInvariantForProgram(*exportValue) == "movie")
{
const Argument* exportArgument = nullptr;
(void)TryGetArgument(arguments, "export", "e", exportArgument);
if (exportArgument->ValueTwo.has_value()) Formats::VxDecoder::Instance1().Export(*exportArgument->ValueTwo).GetAwaiter().GetResult();
else Formats::VxDecoder::Instance1().ExportAll().GetAwaiter().GetResult();
}
else
{
const bool firstHunt = AnyName(arguments, "fh");
Read::ReadAndExport(*exportValue, firstHunt);
}
return;
}
std::optional<std::string> extractValue;
if (TryGetString(arguments, "extract", "x", extractValue)) { Read::ExtractArchive(*extractValue); return; }
std::vector<std::string> rooms;
std::vector<std::pair<std::string, std::int32_t>> models;
GameMode mode = GameMode::None;
std::int32_t playerCount = 0;
BossFlags bossFlags = BossFlags::None;
std::int32_t nodeLayerMask = 0;
std::int32_t entityLayerId = -1;
std::int32_t roomId = 0;
if (TryGetInt(arguments, "room", "r", roomId))
{
const RoomMetadata* meta = Metadata::GetRoomById(roomId);
if (meta == nullptr) Exit();
rooms.push_back(meta->Name());
}
else
{
std::optional<std::string> roomName;
if (TryGetString(arguments, "room", "r", roomName)) rooms.push_back(*roomName);
}
std::int32_t modeValue = 0;
if (TryGetInt(arguments, "mode", "g", modeValue)) mode = static_cast<GameMode>(modeValue);
std::int32_t playerValue = 0;
if (TryGetInt(arguments, "players", "p", playerValue)) playerCount = playerValue;
std::int32_t bossValue = 0;
if (TryGetInt(arguments, "boss", "b", bossValue)) bossFlags = static_cast<BossFlags>(bossValue);
std::int32_t nodeValue = 0;
if (TryGetInt(arguments, "node", "n", nodeValue)) nodeLayerMask = nodeValue;
std::int32_t entityValue = 0;
if (TryGetInt(arguments, "entity", "l", entityValue)) entityLayerId = entityValue;
for (const auto& pair : GetPairs(arguments, "model", "m")) models.push_back(pair);
if (rooms.size() > 1 || (rooms.empty() && models.empty())) Exit();
RenderWindow renderer;
for (const std::string& room : rooms) renderer.AddRoom(room, mode, playerCount, bossFlags, nodeLayerMask, entityLayerId);
const bool firstHunt = AnyName(arguments, "fh");
for (const auto& [model, recolor] : models) renderer.AddModel(model, recolor, firstHunt);
renderer.Run();
}
}
}
