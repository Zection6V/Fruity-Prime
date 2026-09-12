#include "Strings.hpp"
#include "Formats/Formats.hpp"
#include "Formats/RawFormats.hpp"
#include "Read.hpp"
#include "Scene.hpp"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <ios>
#include <sstream>
#include <span>
#include <utility>
namespace MphRead::Text
{
std::unordered_map<Language, std::unordered_map<std::string, Strings::StringTable>> Strings::_cache{};
const std::unordered_map<char, std::int32_t> Strings::_categoryMap{
{'L', 0},
{'l', 0},
{'B', 1},
{'b', 1},
{'O', 2},
{'o', 2},
{'E', 3},
{'e', 3},
{'X', 4},
{'x', 4}
};
const std::shared_ptr<StringTableEntry> Strings::EmptyScanEntry = std::make_shared<StringTableEntry>(
"000", '\0', "INVALID LOG ENTRY", "This object has no entry in the log book.", 0, 'S');
const std::vector<std::string> Strings::_nonAscii{
"\xE2\x82\xAC", " ", "\xE2\x80\x9A", "\xC6\x92", "\xE2\x80\x9E", "\xE2\x80\xA6", "\xE2\x80\xA0", "\xE2\x80\xA1", "\xCB\x86", "\xE2\x80\xB0", "\xC5\xA0", "\xE2\x80\xB9", "\xC5\x92", " ", "\xC5\xBD", " ", " ", "\xE2\x80\x98", "\xE2\x80\x99", "\xE2\x80\x9C", "\xE2\x80\x9D", "\xE2\x80\xA2", "\xE2\x80\x93", "\xE2\x80\x94", "\xCB\x9C", "\xE2\x84\xA2",
"\xC5\xA1", "\xE2\x80\xBA", "\xC5\x93", " ", "\xC5\xBE", "\xC5\xB8", " ", "\xC2\xA1", "\xC2\xA2", "\xC2\xA3", "\xC2\xA4", "\xC2\xA5", "\xC2\xA6", "\xC2\xA7", "\xC2\xA8", "\xC2\xA9", "\xC2\xAA", "\xC2\xAB", "\xC2\xAC", "\xE2\x80\x93", "\xC2\xAE", "\xC2\xAF", "\xC2\xB0", "\xC2\xB1", "\xC2\xB2", "\xC2\xB3",
"\xC2\xB4", "\xC2\xB5", "\xC2\xB6", "\xC2\xB7", "\xC2\xB8", "\xC2\xB9", "\xC2\xBA", "\xC2\xBB", "\xC2\xBC", "\xC2\xBD", "\xC2\xBE", "\xC2\xBF", "\xC3\x80", "\xC3\x81", "\xC3\x82", "\xC3\x83", "\xC3\x84", "\xC3\x85", "\xC3\x86", "\xC3\x87", "\xC3\x88", "\xC3\x89", "\xC3\x8A", "\xC3\x8B", "\xC3\x8C", "\xC3\x8D",
"\xC3\x8E", "\xC3\x8F", "\xC3\x90", "\xC3\x91", "\xC3\x92", "\xC3\x93", "\xC3\x94", "\xC3\x95", "\xC3\x96", "\xC3\x97", "\xC3\x98", "\xC3\x99", "\xC3\x9A", "\xC3\x9B", "\xC3\x9C", "\xC3\x9D", "\xC3\x9E", "\xC3\x9F", "\xC3\xA0", "\xC3\xA1", "\xC3\xA2", "\xC3\xA3", "\xC3\xA4", "\xC3\xA5", "\xC3\xA6", "\xC3\xA7",
"\xC3\xA8", "\xC3\xA9", "\xC3\xAA", "\xC3\xAB", "\xC3\xAC", "\xC3\xAD", "\xC3\xAE", "\xC3\xAF", "\xC3\xB0", "\xC3\xB1", "\xC3\xB2", "\xC3\xB3", "\xC3\xB4", "\xC3\xB5", "\xC3\xB6", "\xC3\xB7", "\xC3\xB8", "\xC3\xB9", "\xC3\xBA", "\xC3\xBB", "\xC3\xBC", "\xC3\xBD", "\xC3\xBE", "\xC3\xBF", " ",
"\xE3\x81\x81", "\xE3\x81\x82", "\xE3\x81\x83", "\xE3\x81\x84", "\xE3\x81\x85", "\xE3\x81\x86", "\xE3\x81\x87", "\xE3\x81\x88", "\xE3\x81\x89", "\xE3\x81\x8A", "\xE3\x81\x8B", "\xE3\x81\x8C", "\xE3\x81\x8D", "\xE3\x81\x8E", "\xE3\x81\x8F", "\xE3\x81\x90", "\xE3\x81\x91", "\xE3\x81\x92", "\xE3\x81\x93", "\xE3\x81\x94", "\xE3\x81\x95", "\xE3\x81\x96",
"\xE3\x81\x97", "\xE3\x81\x98", "\xE3\x81\x99", "\xE3\x81\x9A", "\xE3\x81\x9B", "\xE3\x81\x9C", "\xE3\x81\x9D", "\xE3\x81\x9E", "\xE3\x81\x9F", "\xE3\x81\xA0", "\xE3\x81\xA1", "\xE3\x81\xA2", "\xE3\x81\xA3", "\xE3\x81\xA4", "\xE3\x81\xA5", "\xE3\x81\xA6", "\xE3\x81\xA7", "\xE3\x81\xA8", "\xE3\x81\xA9", "\xE3\x81\xAA", "\xE3\x81\xAB", "\xE3\x81\xAC",
"\xE3\x81\xAD", "\xE3\x81\xAE", "\xE3\x81\xAF", "\xE3\x81\xB0", "\xE3\x81\xB1", "\xE3\x81\xB2", "\xE3\x81\xB3", "\xE3\x81\xB4", "\xE3\x81\xB5", "\xE3\x81\xB6", "\xE3\x81\xB7", "\xE3\x81\xB8", "\xE3\x81\xB9", "\xE3\x81\xBA", "\xE3\x81\xBB", "\xE3\x81\xBC", "\xE3\x81\xBD", "\xE3\x81\xBE", "\xE3\x81\xBF", "\xE3\x82\x80", "\xE3\x82\x81", "\xE3\x82\x82",
"\xE3\x82\x83", "\xE3\x82\x84", "\xE3\x82\x85", "\xE3\x82\x86", "\xE3\x82\x87", "\xE3\x82\x88", "\xE3\x82\x89", "\xE3\x82\x8A", "\xE3\x82\x8B", "\xE3\x82\x8C", "\xE3\x82\x8D", "\xE3\x82\x8E", "\xE3\x82\x8F", "\xE3\x82\x90", "\xE3\x82\x91", "\xE3\x82\x92", "\xE3\x82\x93", "\xE3\x82\xA1", "\xE3\x82\xA2", "\xE3\x82\xA3", "\xE3\x82\xA4", "\xE3\x82\xA5",
"\xE3\x82\xA6", "\xE3\x82\xA7", "\xE3\x82\xA8", "\xE3\x82\xA9", "\xE3\x82\xAA", "\xE3\x82\xAB", "\xE3\x82\xAC", "\xE3\x82\xAD", "\xE3\x82\xAE", "\xE3\x82\xAF", "\xE3\x82\xB0", "\xE3\x82\xB1", "\xE3\x82\xB2", "\xE3\x82\xB3", "\xE3\x82\xB4", "\xE3\x82\xB5", "\xE3\x82\xB6", "\xE3\x82\xB7", "\xE3\x82\xB8", "\xE3\x82\xB9", "\xE3\x82\xBA", "\xE3\x82\xBB",
"\xE3\x82\xBC", "\xE3\x82\xBD", "\xE3\x82\xBE", "\xE3\x82\xBF", "\xE3\x83\x80", "\xE3\x83\x81", "\xE3\x83\x82", "\xE3\x83\x83", "\xE3\x83\x84", "\xE3\x83\x85", "\xE3\x83\x86", "\xE3\x83\x87", "\xE3\x83\x88", "\xE3\x83\x89", "\xE3\x83\x8A", "\xE3\x83\x8B", "\xE3\x83\x8C", "\xE3\x83\x8D", "\xE3\x83\x8E", "\xE3\x83\x8F", "\xE3\x83\x90", "\xE3\x83\x91",
"\xE3\x83\x92", "\xE3\x83\x93", "\xE3\x83\x94", "\xE3\x83\x95", "\xE3\x83\x96", "\xE3\x83\x97", "\xE3\x83\x98", "\xE3\x83\x99", "\xE3\x83\x9A", "\xE3\x83\x9B", "\xE3\x83\x9C", "\xE3\x83\x9D", "\xE3\x83\x9E", "\xE3\x83\x9F", "\xE3\x83\xA0", "\xE3\x83\xA1", "\xE3\x83\xA2", "\xE3\x83\xA3", "\xE3\x83\xA4", "\xE3\x83\xA5", "\xE3\x83\xA6", "\xE3\x83\xA7",
"\xE3\x83\xA8", "\xE3\x83\xA9", "\xE3\x83\xAA", "\xE3\x83\xAB", "\xE3\x83\xAC", "\xE3\x83\xAD", "\xE3\x83\xAE", "\xE3\x83\xAF", "\xE3\x83\xB0", "\xE3\x83\xB1", "\xE3\x83\xB2", "\xE3\x83\xB3", "\xE3\x83\xB4", "\xE3\x83\xB5", "\xE3\x87\xB0", " ", " ", " ", " ", " ", " ", " ",
"\xE3\x80\x81", "\xE3\x80\x82", "'", "\xE3\x83\xBB", "\xE3\x83\xBB", ":", ";", "?", "!", "\xE3\x82\x9B", "\xE3\x82\x9C", "\xC2\xB4", "`", "\xC2\xA8", "^", "\xE2\x80\xBE", "_", " ", " ", "\xE3\x82\x9D", "\xE3\x82\x9E", " ", " ",
"\xE3\x80\x85", " ", " ", "\xE2\x80\x93", "\xE2\x80\x94", "\xE2\x88\x92", "\xEF\xBC\x8F", "\xEF\xBC\xBC", "\xCB\x9C", " ", "|", "\xE2\x80\xA6", " ", "'", "'", "\"", "\"", "(", ")", "(", ")", "[", "]", "{", "}",
"<", ">", " ", " ", "\xE3\x80\x8C", "\xE3\x80\x8D", " ", " ", " ", " ", "+", "-", "\xC2\xB1", "\xC3\x97", "\xC3\xB7", "=", " ", " ", " ", " ", " ", "\xE2\x88\x9E", "\xE2\x88\xB4", " ", " ",
"\xC2\xB0", "\xE1\x90\x9F", "\xE1\x90\xA5 ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ",
" ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " "
};
void Strings::ClearCache()
{
_cache.clear();
}
Strings::StringTable Strings::ReadStringTable(const std::string& name)
{
Language language = Scene::Language();
auto languageIt = _cache.find(language);
if (languageIt != _cache.end())
{
auto tableIt = languageIt->second.find(name);
if (tableIt != languageIt->second.end())
{
return tableIt->second;
}
}
else
{
languageIt = _cache.emplace(language, std::unordered_map<std::string, StringTable>{}).first;
}
auto entries = std::make_shared<std::vector<std::shared_ptr<StringTableEntry>>>();
const std::string filename = name == StringTables::ScanLog && Paths::MphKey() == Ver::AMHK0
? StringTables::ScanLogSorted
: name;
const std::string path = Paths::Combine(Paths::FileSystem(), GetFolder(), filename);
std::ifstream stream;
stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);
stream.open(path, std::ios::binary);
stream.seekg(0, std::ios::end);
const std::streamoff length = stream.tellg();
stream.seekg(0, std::ios::beg);
std::vector<std::uint8_t> data(static_cast<std::size_t>(length));
if (!data.empty())
{
stream.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
}
stream.close();
const std::span<const std::uint8_t> bytes(data.data(), data.size());
const std::uint32_t count = Read::SpanReadUint(bytes, static_cast<std::int32_t>(0));
const std::int32_t offset = name == StringTables::ScanLog ? 8 : 4;
for (const RawStringTableEntry& entry : Read::DoOffsets<RawStringTableEntry>(bytes, offset, count))
{
if (entry.Offset < bytes.size())
{
std::string value = Read::ReadStringTable(bytes, entry.Offset, entry.Length);
value.erase(std::remove(value.begin(), value.end(), '$'), value.end());
char prefix = '\0';
if (name == StringTables::GameMessages)
{
prefix = value.at(0);
value = value.substr(1);
}
std::string value1 = value;
std::string value2;
const std::size_t slashCount = static_cast<std::size_t>(std::count(value.begin(), value.end(), '\\'));
if (slashCount == 1)
{
const std::size_t slash = value.find('\\');
value1 = value.substr(0, slash);
value2 = value.substr(slash + 1);
}
entries->push_back(std::make_shared<StringTableEntry>(entry, prefix, value1, value2));
}
}
StringTable table = entries;
languageIt->second.emplace(name, table);
return table;
}
std::string Strings::GetHudMessage(std::int32_t id)
{
return GetHudMessage(static_cast<std::uint32_t>(id));
}
std::string Strings::GetHudMessage(std::uint32_t id)
{
if (id >= 1 && id <= 11)
{
return GetMessage('H', id, StringTables::HudMsgsCommon);
}
if (id >= 101 && id <= 122)
{
return GetMessage('H', id, StringTables::HudMessagesSP);
}
if (id >= 201 && id <= 257)
{
return GetMessage('H', id, StringTables::HudMessagesMP);
}
if (id >= 301 && id <= 305)
{
return GetMessage('W', id - 300, StringTables::HudMessagesMP);
}
return " ";
}
std::string Strings::GetMessage(char type, std::int32_t id, const std::string& table)
{
return GetMessage(type, static_cast<std::uint32_t>(id), table);
}
std::string Strings::GetMessage(char type, std::uint32_t id, const std::string& table)
{
std::shared_ptr<StringTableEntry> entry = GetEntry(type, id, table);
return entry ? entry->Value1() : " ";
}
std::shared_ptr<StringTableEntry> Strings::GetEntry(char type, std::int32_t id, const std::string& table)
{
return GetEntry(type, static_cast<std::uint32_t>(id), table);
}
std::shared_ptr<StringTableEntry> Strings::GetEntry(char type, std::uint32_t id, const std::string& table)
{
std::ostringstream builder;
builder << type << std::setfill('0') << std::setw(3) << id;
const std::string fullId = builder.str();
StringTable list = ReadStringTable(table);
for (const std::shared_ptr<StringTableEntry>& entry : *list)
{
if (entry->Id() == fullId)
{
return entry;
}
}
return nullptr;
}
std::shared_ptr<StringTableEntry> Strings::GetScanEntry(std::int32_t scanId)
{
return GetEntry('L', static_cast<std::uint32_t>(scanId), StringTables::ScanLog);
}
std::int32_t Strings::GetScanEntryCategory(std::int32_t scanId)
{
std::shared_ptr<StringTableEntry> entry = GetEntry('L', static_cast<std::uint32_t>(scanId), StringTables::ScanLog);
if (!entry)
{
return 0;
}
const auto found = _categoryMap.find(entry->Category());
if (found != _categoryMap.end())
{
return found->second;
}
return 5;
}
float Strings::GetScanEntryTime(std::int32_t scanId)
{
std::shared_ptr<StringTableEntry> entry = GetEntry('L', static_cast<std::uint32_t>(scanId), StringTables::ScanLog);
if (!entry)
{
return 60 / 30.0F;
}
return 10 * (entry->Speed() & 7) / 30.0F;
}
std::string Strings::GetFolder()
{
std::string folder = "stringTables";
if (Scene::Language() == Language::French)
{
folder += "_fr";
}
else if (Scene::Language() == Language::German)
{
folder += "_gr";
}
else if (Scene::Language() == Language::Italian)
{
folder += "_it";
}
else if (Scene::Language() == Language::Japanese)
{
folder += "_jp";
}
else if (Scene::Language() == Language::Spanish)
{
folder += "_sp";
}
return folder;
}
std::shared_ptr<const std::vector<std::string>> Strings::ReadTextFile(bool downloadPlay)
{
std::string suffix = Paths::IsMphEurope() ? "en-gb" : "en";
if (Scene::Language() == Language::French)
{
suffix = "fr";
}
else if (Scene::Language() == Language::German)
{
suffix = "de";
}
else if (Scene::Language() == Language::Italian)
{
suffix = "it";
}
else if (Scene::Language() == Language::Japanese)
{
suffix = "jp";
}
else if (Scene::Language() == Language::Spanish)
{
suffix = "es";
}
const std::string prefix = downloadPlay ? "single_" : "";
const std::string name = prefix + "metroidhunters_text_" + suffix + ".bin";
std::string path = Paths::Combine(Paths::FileSystem(), "frontend", name);
if (suffix == "en-gb")
{
std::size_t position = 0;
while ((position = path.find("amhe0", position)) != std::string::npos)
{
path.replace(position, 5, "amhp1");
position += 5;
}
}
std::ifstream stream;
stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);
stream.open(path, std::ios::binary);
stream.seekg(0, std::ios::end);
const std::streamoff length = stream.tellg();
stream.seekg(0, std::ios::beg);
std::vector<std::uint8_t> data(static_cast<std::size_t>(length));
if (!data.empty())
{
stream.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
}
stream.close();
const std::span<const std::uint8_t> bytes(data.data(), data.size());
std::int32_t offset = 0;
std::vector<std::uint32_t> list;
while (true)
{
const std::uint32_t item = Read::SpanReadUint(bytes, static_cast<std::int32_t>(offset));
offset += 4;
if (item == 0)
{
break;
}
list.push_back(item);
}
auto strings = std::make_shared<std::vector<std::string>>();
for (std::uint32_t item : list)
{
const TextFileEntry entry = Read::DoOffset<TextFileEntry>(bytes, item);
assert(entry.Offset1 == entry.Offset2);
assert(entry.Length1 == entry.Length2);
std::string text = Read::ReadString(bytes, entry.Offset1, entry.Length1);
while (text.size() != entry.Length1)
{
text.push_back('\0');
}
strings->push_back(std::move(text));
}
return strings;
}
std::string Strings::ReplaceNonAscii(const std::string& value)
{
std::string result;
for (std::size_t i = 0; i < value.size(); i++)
{
const std::uint8_t c = static_cast<std::uint8_t>(value[i]);
if ((c & 0xA0U) == 0xA0U)
{
return "<kanji>";
}
if ((c & 0x80U) == 0)
{
result.push_back(static_cast<char>(c));
}
else
{
const std::uint8_t next = static_cast<std::uint8_t>(value.at(++i));
const std::int32_t index = static_cast<std::int32_t>(next & 0x3FU)
| (static_cast<std::int32_t>(c & 0x1FU) << 6);
if (index >= 128 && static_cast<std::size_t>(index - 128) <= _nonAscii.size())
{
result += _nonAscii.at(static_cast<std::size_t>(index - 128));
}
else if (index <= 0x7F)
{
result.push_back(static_cast<char>(index));
}
else if (index <= 0x7FF)
{
result.push_back(static_cast<char>(0xC0 | (index >> 6)));
result.push_back(static_cast<char>(0x80 | (index & 0x3F)));
}
else
{
result.push_back(static_cast<char>(0xE0 | (index >> 12)));
result.push_back(static_cast<char>(0x80 | ((index >> 6) & 0x3F)));
result.push_back(static_cast<char>(0x80 | (index & 0x3F)));
}
}
}
return result;
}
const std::string StringTables::GameMessages = "GameMessages.bin";
const std::string StringTables::HudMessagesMP = "HudMessagesMP.bin";
const std::string StringTables::HudMessagesSP = "HudMessagesSP.bin";
const std::string StringTables::HudMsgsCommon = "HudMsgsCommon.bin";
const std::string StringTables::LocationNames = "LocationNames.bin";
const std::string StringTables::MBBanner = "MBBanner.bin";
const std::string StringTables::ScanLog = "ScanLog.bin";
const std::string StringTables::ScanLogSorted = "ScanLogSorted.bin";
const std::string StringTables::ShipInSpace = "ShipInSpace.bin";
const std::string StringTables::ShipOnGround = "ShipOnGround.bin";
const std::string StringTables::WeaponNames = "WeaponNames.bin";
const std::shared_ptr<const std::vector<std::string>> StringTables::_all
= std::make_shared<const std::vector<std::string>>(std::vector<std::string>{
GameMessages, HudMessagesMP, HudMessagesSP, HudMsgsCommon, LocationNames,
MBBanner, ScanLog, ShipInSpace, ShipOnGround, WeaponNames
});
const std::shared_ptr<Font> Font::_normal = std::make_shared<Font>();
const std::shared_ptr<Font> Font::_kanji = std::make_shared<Font>();
}
