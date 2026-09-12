#pragma once

#include "Formats/Enums.hpp"

#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace MphRead
{
    class StringTableEntry;
}

namespace MphRead::Text
{
    class Strings final
    {
    public:
        static void ClearCache();
        static std::shared_ptr<const std::vector<std::shared_ptr<StringTableEntry>>>
            ReadStringTable(const std::string& name);
        static std::string GetHudMessage(std::int32_t id);
        static std::string GetHudMessage(std::uint32_t id);
        static std::string GetMessage(char type, std::int32_t id, const std::string& table);
        static std::string GetMessage(char type, std::uint32_t id, const std::string& table);
        static std::shared_ptr<StringTableEntry> GetEntry(char type, std::int32_t id, const std::string& table);
        static std::shared_ptr<StringTableEntry> GetEntry(char type, std::uint32_t id, const std::string& table);

        static const std::shared_ptr<StringTableEntry> EmptyScanEntry;

        static std::shared_ptr<StringTableEntry> GetScanEntry(std::int32_t scanId);
        static std::int32_t GetScanEntryCategory(std::int32_t scanId);
        static float GetScanEntryTime(std::int32_t scanId);
        static std::shared_ptr<const std::vector<std::string>> ReadTextFile(bool downloadPlay = false);
        static std::string ReplaceNonAscii(const std::string& value);

        Strings() = delete;
        Strings(const Strings&) = delete;
        Strings& operator=(const Strings&) = delete;

    private:
        static std::string GetFolder();

        static std::unordered_map<Language,
            std::unordered_map<std::string,
                std::shared_ptr<const std::vector<std::shared_ptr<StringTableEntry>>>>> _cache;
        static const std::unordered_map<char, std::int32_t> _categoryMap;
        static const std::vector<std::string> _nonAscii;
    };

    class StringTables final
    {
    public:
        static const std::string GameMessages;
        static const std::string HudMessagesMP;
        static const std::string HudMessagesSP;
        static const std::string HudMsgsCommon;
        static const std::string LocationNames;
        static const std::string MBBanner;
        static const std::string ScanLog;
        static const std::string ScanLogSorted;
        static const std::string ShipInSpace;
        static const std::string ShipOnGround;
        static const std::string WeaponNames;

        static const std::shared_ptr<const std::vector<std::string>>& All()
        {
            return _all;
        }

        StringTables() = delete;
        StringTables(const StringTables&) = delete;
        StringTables& operator=(const StringTables&) = delete;

    private:
        static const std::shared_ptr<const std::vector<std::string>> _all;
    };

    class Font
    {
    public:
        Font() = default;
        Font(const Font&) = delete;
        Font(Font&&) = delete;
        Font& operator=(const Font&) = delete;
        Font& operator=(Font&&) = delete;

        static const std::shared_ptr<Font>& Normal()
        {
            return _normal;
        }

        static const std::shared_ptr<Font>& Kanji()
        {
            return _kanji;
        }

        const std::shared_ptr<const std::vector<std::int32_t>>& Widths() const
        {
            return _widths;
        }

        const std::shared_ptr<const std::vector<std::int32_t>>& Offsets() const
        {
            return _offsets;
        }

        const std::shared_ptr<const std::vector<std::uint8_t>>& CharacterData() const
        {
            return _characterData;
        }

        std::int32_t MinCharacter() const
        {
            return _minCharacter;
        }

        void SetData(const std::shared_ptr<std::vector<std::uint8_t>>& widths,
            const std::shared_ptr<std::vector<std::uint8_t>>& offsets,
            const std::shared_ptr<std::vector<std::uint8_t>>& chars,
            std::int32_t minChar, bool packed = true)
        {
            auto widthData = std::make_shared<std::vector<std::int32_t>>(widths->size());
            for (std::size_t i = 0; i < widths->size(); i++)
            {
                (*widthData)[i] = (*widths)[i];
            }
            _widths = widthData;

            auto offsetData = std::make_shared<std::vector<std::int32_t>>(offsets->size());
            for (std::size_t i = 0; i < offsets->size(); i++)
            {
                const std::int32_t value = (*offsets)[i];
                (*offsetData)[i] = value < 128 ? value : value - 256;
            }
            _offsets = offsetData;

            if (packed)
            {
                assert(!chars->empty() && chars->size() % 2 == 0);
                auto charData = std::make_shared<std::vector<std::uint8_t>>(chars->size() * 2);
                for (std::size_t i = 0; i < chars->size(); i++)
                {
                    const std::uint8_t data = (*chars)[i];
                    (*charData)[i * 2] = static_cast<std::uint8_t>(data & 0x0F);
                    (*charData)[i * 2 + 1] = static_cast<std::uint8_t>(data >> 4);
                }
                _characterData = charData;
            }
            else
            {
                _characterData = chars;
            }
            _minCharacter = minChar;
        }

    private:
        static const std::shared_ptr<Font> _normal;
        static const std::shared_ptr<Font> _kanji;

        std::shared_ptr<const std::vector<std::int32_t>> _widths;
        std::shared_ptr<const std::vector<std::int32_t>> _offsets;
        std::shared_ptr<const std::vector<std::uint8_t>> _characterData;
        std::int32_t _minCharacter = 0;
    };
}
