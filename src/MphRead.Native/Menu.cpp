#include "Menu.hpp"
#include "NativeRuntime/System/Decimal.hpp"

#include "Metadata/FrontendMeta.hpp"
#include "Renderer.hpp"
#include "Scene.hpp"
#include "Metadata/Metadata.hpp"
#include "Metadata/Rooms.hpp"
#include "NativeRuntime/System/Random.hpp"
#include "Sound/Sfx.hpp"

#include "Features.hpp"
#include "Formats/Formats.hpp"
#include "Formats/Sound.hpp"
#include "GameState.hpp"
#include "Metadata/Metadata.hpp"
#include "Metadata/FrontendMeta.hpp"
#include "Metadata/Rooms.hpp"
#include "Mods/Branding.hpp"
#include "SceneSetup.hpp"
#include "Sound/Music.hpp"
#include "Sound/Sfx.hpp"
#include "Strings.hpp"
#include "Utility/Rng.hpp"
#include "NativeRuntime/System/Encoding.hpp"
#include "NativeRuntime/System/Globalization.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <climits>
#include <codecvt>
#include <cstdint>
#include <cstdlib>
#include <cwchar>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <locale>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <conio.h>
#include <windows.h>
#else
#include <dlfcn.h>
#include <langinfo.h>
#include <locale.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#include <wctype.h>
#endif

using ::MphRead::NativeRuntime::CharIsWhiteSpace;
using ::MphRead::NativeRuntime::DecodeUtf8Scalar;
using ::MphRead::NativeRuntime::Int32TryParseCurrentCulture;
using ::MphRead::NativeRuntime::IsNumberWhiteSpace;
using ::MphRead::NativeRuntime::StringIsNullOrWhiteSpace;
using ::MphRead::NativeRuntime::StringReplace;
using ::MphRead::NativeRuntime::StringTrim;
using ::MphRead::NativeRuntime::Utf16ToUtf8;
using ::MphRead::NativeRuntime::Utf8Scalar;
using ::MphRead::NativeRuntime::Utf8ToWide;
using ::MphRead::NativeRuntime::WideToUtf8;

namespace
{
    // `Metadata.ModelMetadata.Keys`, in the dictionary's own order.
    [[nodiscard]] std::vector<std::string> ModelMetadataKeys()
    {
        std::vector<std::string> keys;
        keys.reserve(MphRead::Metadata::ModelMetadata.size());
        for (const auto& entry : MphRead::Metadata::ModelMetadata)
        {
            keys.push_back(entry.first);
        }
        return keys;
    }
}


namespace MphRead
{
    class RenderWindow;
}

namespace MphRead::GameStateDetail
{
    [[nodiscard]] GameMode GameModeNone();
    [[nodiscard]] GameMode GameModeSinglePlayer();
    [[nodiscard]] GameMode GameModeBattle();
    [[nodiscard]] GameMode GameModeBattleTeams();
    [[nodiscard]] GameMode GameModeSurvival();
    [[nodiscard]] GameMode GameModeSurvivalTeams();
    [[nodiscard]] GameMode GameModeCapture();
    [[nodiscard]] GameMode GameModeBounty();
    [[nodiscard]] GameMode GameModeBountyTeams();
    [[nodiscard]] GameMode GameModeNodes();
    [[nodiscard]] GameMode GameModeNodesTeams();
    [[nodiscard]] GameMode GameModeDefender();
    [[nodiscard]] GameMode GameModeDefenderTeams();
    [[nodiscard]] GameMode GameModePrimeHunter();

    [[nodiscard]] BossFlags BossFlagsUnit1B1Kill();
    [[nodiscard]] BossFlags BossFlagsUnit1B1Done();
    [[nodiscard]] BossFlags BossFlagsUnit1B2Kill();
    [[nodiscard]] BossFlags BossFlagsUnit1B2Done();
    [[nodiscard]] BossFlags BossFlagsUnit2B1Kill();
    [[nodiscard]] BossFlags BossFlagsUnit2B1Done();
    [[nodiscard]] BossFlags BossFlagsUnit2B2Kill();
    [[nodiscard]] BossFlags BossFlagsUnit2B2Done();
    [[nodiscard]] BossFlags BossFlagsUnit3B1Kill();
    [[nodiscard]] BossFlags BossFlagsUnit3B1Done();
    [[nodiscard]] BossFlags BossFlagsUnit3B2Kill();
    [[nodiscard]] BossFlags BossFlagsUnit3B2Done();
    [[nodiscard]] BossFlags BossFlagsUnit4B1Kill();
    [[nodiscard]] BossFlags BossFlagsUnit4B1Done();
    [[nodiscard]] BossFlags BossFlagsUnit4B2Kill();
    [[nodiscard]] BossFlags BossFlagsUnit4B2Done();
}

namespace
{
    using namespace MphRead;

    constexpr std::string_view A76E0 = "A76E0";
    constexpr std::string_view AMHE0 = "AMHE0";
    constexpr std::string_view AMHE1 = "AMHE1";
    constexpr std::string_view AMHP0 = "AMHP0";
    constexpr std::string_view AMHP1 = "AMHP1";
    constexpr std::string_view AMHJ0 = "AMHJ0";
    constexpr std::string_view AMHJ1 = "AMHJ1";
    constexpr std::string_view AMHK0 = "AMHK0";
    constexpr std::string_view AMFE0 = "AMFE0";
    constexpr std::string_view AMFP0 = "AMFP0";


    [[nodiscard]] std::string DotNetTrimStart(std::string_view text)
    {
        std::size_t first = 0;
        while (first < text.size())
        {
            const Utf8Scalar character = DecodeUtf8Scalar(text, first);
            if (!character.Valid() || !CharIsWhiteSpace(character.Value)) break;
            first += character.Length;
        }
        return std::string(text.substr(first));
    }

    using Decimal = ::System::Decimal;

    Decimal _sfxVolume = Decimal::ParseInvariant("0.35");
    Decimal _musicVolume = Decimal::ParseInvariant("0.50");
    std::string _mode = "auto-select";
    Language _language = Language::English;
    std::int32_t _movieId = -1;

    bool _applySettings = false;
    bool _teams = false;
    Decimal _pointGoal = Decimal(0);
    Decimal _timeGoal = Decimal(0);
    Decimal _timeLimit = Decimal(0);
    bool _octolithReset = true;
    bool _radarPlayers = false;
    std::int32_t _damageLevel = 1;
    bool _friendlyFire = false;
    bool _affinityWeapons = false;
    std::string _goalType;

    std::array<std::int32_t, 5> _planets{1, 0, 0, 0, 0};
    std::int32_t _alinos1State = 0;
    std::int32_t _alinos2State = 0;
    std::int32_t _ca1State = 0;
    std::int32_t _ca2State = 0;
    std::int32_t _vdo1State = 0;
    std::int32_t _vdo2State = 0;
    std::int32_t _arcterra1State = 0;
    std::int32_t _arcterra2State = 0;
    std::int32_t _checkpointId = -1;
    std::int32_t _healthMax = 99;
    std::int32_t _missileMax = 50;
    std::int32_t _uaMax = 400;
    std::array<std::int32_t, 9> _weapons{1, 0, 1, 0, 0, 0, 0, 0, 0};
    std::array<std::int32_t, 8> _octoliths{};

    std::array<std::string, 10> _saveInfo{
        "Alinos   : locked  : ", "CA       : locked  : ", "VDO      : locked  : ",
        "Arcterra : locked  : ", "Oubliette: locked", "Artifacts: ", "Octoliths: ",
        "Expansion: ", "Weapons  : ", "Complete : "
    };

    const std::vector<Decimal> _battlePoints = []{ std::vector<Decimal> v; for (int n : {1,5,7,10,15,20,25,30,40,50,60,70,80,90,100}) v.push_back(Decimal(n)); return v; }();
    const std::vector<Decimal> _octolithPoints = []{ std::vector<Decimal> v; for (int n : {1,2,3,4,5,6,7,8,9,10,15,20,25}) v.push_back(Decimal(n)); return v; }();
    const std::vector<Decimal> _nodePoints = []{ std::vector<Decimal> v; for (int n : {40,50,60,70,80,90,100,120,140,160,180,190,200,250}) v.push_back(Decimal(n)); return v; }();
    const std::vector<Decimal> _extraLives = []{ std::vector<Decimal> v; for (int n : {0,1,2,3,4,5,6,7,8,9,10}) v.push_back(Decimal(n)); return v; }();
    const std::vector<Decimal> _timeGoals = []{ std::vector<Decimal> v; for (int n : {60,90,120,150,180,210,240,270,300,360,420,480,540,600}) v.push_back(Decimal(n)); return v; }();
    const std::vector<Decimal> _timeLimits = []{ std::vector<Decimal> v; for (int n : {180,300,420,540,600,900,1200,1500,1800,2100,2400,2700,3000,3300,3600}) v.push_back(Decimal(n)); return v; }();

    enum class ConsoleKey
    {
        None, Escape, Enter, Spacebar, Backspace, Delete,
        UpArrow, DownArrow, LeftArrow, RightArrow, PageUp, PageDown,
        Add, Subtract, OemPlus, OemMinus,
        A, B, C, D, E, F, G, H, I, J, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y,
        D1, D2, D3, D4, D5, D6, D7, D8,
        NumPad1, NumPad2, NumPad3, NumPad4, NumPad5, NumPad6, NumPad7, NumPad8
    };

    struct ConsoleKeyInfo
    {
        ConsoleKey Key = ConsoleKey::None;
        bool Control = false;
    };

    [[nodiscard]] std::string ToLower(std::string value)
    {
        return ::MphRead::NativeRuntime::ToLowerCurrentCulture(value);
    }

    [[nodiscard]] std::string ToUpper(std::string value)
    {
        return ::MphRead::NativeRuntime::ToUpperCurrentCulture(value);
    }

    // string.StartsWith(string) / EndsWith(string): current culture.
    [[nodiscard]] bool StartsWith(std::string_view value, std::string_view prefix)
    {
        return ::MphRead::NativeRuntime::StringStartsWithCurrentCulture(value, prefix);
    }
    [[nodiscard]] bool EndsWith(std::string_view value, std::string_view suffix)
    {
        return ::MphRead::NativeRuntime::StringEndsWithCurrentCulture(value, suffix);
    }

    [[nodiscard]] std::vector<std::string> Split(const std::string& value, char separator, bool trim, bool removeEmpty)
    {
        std::vector<std::string> result;
        std::size_t start = 0;
        while (true)
        {
            const std::size_t end = value.find(separator, start);
            std::string item = value.substr(start, end == std::string::npos ? std::string::npos : end - start);
            if (trim) item = StringTrim(std::move(item));
            if (!removeEmpty || !item.empty()) result.push_back(std::move(item));
            if (end == std::string::npos) break;
            start = end + 1;
        }
        return result;
    }

    [[nodiscard]] bool TryParseByte(std::string_view input, std::uint8_t& result)
    {
        std::int32_t value = 0;
        if (!Int32TryParseCurrentCulture(input, value) || value < 0 || value > 255) { result = 0; return false; }
        result = static_cast<std::uint8_t>(value);
        return true;
    }

    template <typename E>
    [[nodiscard]] bool TryParseEnumNumeric(std::string_view input, E& result)
    {
        using U = std::underlying_type_t<E>;
        using UU = std::make_unsigned_t<U>;
        static_assert(std::is_integral_v<U>);

        const std::string text = DotNetTrimStart(input);
        if (text.empty()) return false;

        bool negative = false;
        std::size_t index = 0;
        if (text[index] == '+' || text[index] == '-')
        {
            negative = text[index] == '-';
            if (++index == text.size()) return false;
        }

        std::uint64_t limit;
        if constexpr (std::is_signed_v<U>)
        {
            limit = negative
                ? static_cast<std::uint64_t>(std::numeric_limits<U>::max()) + 1ULL
                : static_cast<std::uint64_t>(std::numeric_limits<U>::max());
        }
        else
        {
            if (negative) return false;
            limit = static_cast<std::uint64_t>(std::numeric_limits<U>::max());
        }

        std::uint64_t magnitude = 0;
        bool digitSeen = false;
        while (index < text.size() && text[index] >= '0' && text[index] <= '9')
        {
            digitSeen = true;
            const std::uint64_t digit = static_cast<std::uint64_t>(text[index] - '0');
            if (magnitude > limit / 10ULL
                || (magnitude == limit / 10ULL && digit > limit % 10ULL)) return false;
            magnitude = magnitude * 10ULL + digit;
            ++index;
        }
        if (!digitSeen) return false;
        while (index < text.size() && IsNumberWhiteSpace(static_cast<unsigned char>(text[index]))) ++index;
        while (index < text.size() && text[index] == '\0') ++index;
        if (index != text.size()) return false;

        U raw{};
        if constexpr (std::is_signed_v<U>)
        {
            if (negative)
            {
                if (magnitude == static_cast<std::uint64_t>(std::numeric_limits<U>::max()) + 1ULL)
                    raw = std::numeric_limits<U>::min();
                else raw = static_cast<U>(-static_cast<std::int64_t>(magnitude));
            }
            else raw = static_cast<U>(magnitude);
        }
        else raw = static_cast<U>(static_cast<UU>(magnitude));

        result = static_cast<E>(raw);
        return true;
    }

    [[nodiscard]] std::string Fixed2(const Decimal& value)
    {
        return value.ToString("F2");
    }

    [[nodiscard]] std::string SaveWhenString(SaveWhen value)
    {
        switch (value)
        {
        case static_cast<SaveWhen>(0): return "never";
        case static_cast<SaveWhen>(1): return "always";
        case static_cast<SaveWhen>(2): return "prompt";
        }
        return ToLower(std::to_string(static_cast<std::int32_t>(value)));
    }

    [[nodiscard]] std::string LanguageString(Language value)
    {
        switch (value)
        {
        case Language::English: return "English";
        case Language::Japanese: return "Japanese";
        case Language::French: return "French";
        case Language::Spanish: return "Spanish";
        case Language::German: return "German";
        case Language::Italian: return "Italian";
        }
        return std::to_string(static_cast<std::int32_t>(value));
    }

    [[nodiscard]] bool TryParseLanguage(std::string_view value, Language& result)
    {
        if (TryParseEnumNumeric(value, result)) return true;
        const std::string text = StringTrim(value);

        std::int32_t combined = 0;
        const std::vector<std::string> parts = Split(text, ',', false, false);
        if (parts.empty()) return false;
        for (const std::string& partValue : parts)
        {
            const std::string part = StringTrim(partValue);
            Language parsed{};
            if (part == "English") parsed = Language::English;
            else if (part == "Japanese") parsed = Language::Japanese;
            else if (part == "French") parsed = Language::French;
            else if (part == "Spanish") parsed = Language::Spanish;
            else if (part == "German") parsed = Language::German;
            else if (part == "Italian") parsed = Language::Italian;
            else return false;
            combined |= static_cast<std::int32_t>(parsed);
        }
        result = static_cast<Language>(combined);
        return true;
    }

    [[nodiscard]] SaveWhen ParseSaveWhen(std::string value, SaveWhen fallback)
    {
        if (value.size() >= 2)
        {
            const Utf8Scalar first = DecodeUtf8Scalar(value, 0);
            const std::size_t firstLength = first.Valid() ? first.Length : 1;
            value = ToUpper(value.substr(0, firstLength)) + ToLower(value.substr(firstLength));
            SaveWhen numeric{};
            if (TryParseEnumNumeric(value, numeric)) return numeric;
            const std::string text = StringTrim(value);

            std::int32_t combined = 0;
            const std::vector<std::string> parts = Split(text, ',', false, false);
            if (parts.empty()) return fallback;
            for (const std::string& partValue : parts)
            {
                const std::string part = StringTrim(partValue);
                if (part == "Never") combined |= 0;
                else if (part == "Always") combined |= 1;
                else if (part == "Prompt") combined |= 2;
                else return fallback;
            }
            return static_cast<SaveWhen>(combined);
        }
        return fallback;
    }

    [[nodiscard]] std::string HunterString(Hunter value)
    {
        switch (value)
        {
        case Hunter::Samus: return "Samus";
        case Hunter::Kanden: return "Kanden";
        case Hunter::Trace: return "Trace";
        case Hunter::Sylux: return "Sylux";
        case Hunter::Noxus: return "Noxus";
        case Hunter::Spire: return "Spire";
        case Hunter::Weavel: return "Weavel";
        case Hunter::Guardian: return "Guardian";
        case Hunter::Random: return "Random";
        }
        return std::to_string(static_cast<std::int32_t>(value));
    }

    [[nodiscard]] bool IsDefinedHunter(Hunter value) noexcept
    {
        const std::int32_t raw = static_cast<std::int32_t>(value);
        return raw >= 0 && raw <= 8;
    }

    [[nodiscard]] bool TryParseHunter(std::string_view value, Hunter& result)
    {
        if (TryParseEnumNumeric(value, result)) return true;
        const std::string text = StringTrim(value);

        std::int32_t combined = 0;
        const std::vector<std::string> parts = Split(text, ',', false, false);
        if (parts.empty()) return false;
        for (const std::string& partValue : parts)
        {
            const std::string part = StringTrim(partValue);
            bool matched = false;
            for (std::int32_t i = 0; i <= 8; ++i)
            {
                const Hunter candidate = static_cast<Hunter>(i);
                if (HunterString(candidate) == part)
                {
                    combined |= i;
                    matched = true;
                    break;
                }
            }
            if (!matched) return false;
        }
        result = static_cast<Hunter>(combined);
        return true;
    }

    [[nodiscard]] std::optional<MetaDir> TryParseMetaDir(std::string_view value)
    {
        static const std::array<std::string_view, 24> names = {
            "Models", "Hud", "Stage", "MainMenu", "Logo", "CharSelect", "CreateJoin", "GameOption",
            "GamersCard", "Keyboard", "Keypad", "MoviePlayer", "MultiMaster", "Multiplayer", "PaxControls",
            "Popup", "Results", "ScStartGame", "StartGame", "ToStart", "TouchToStart", "TouchToStart2",
            "WifiCreate", "WifiGames"
        };
        MetaDir numeric{};
        if (TryParseEnumNumeric(value, numeric)) return numeric;
        const std::string text = StringTrim(value);
        std::int32_t combined = 0;
        const std::vector<std::string> parts = Split(text, ',', false, false);
        if (parts.empty()) return std::nullopt;
        for (const std::string& partValue : parts)
        {
            const std::string part = StringTrim(partValue);
            bool matched = false;
            for (std::size_t i = 0; i < names.size(); ++i)
            {
                if (names[i] == part)
                {
                    combined |= static_cast<std::int32_t>(i);
                    matched = true;
                    break;
                }
            }
            if (!matched) return std::nullopt;
        }
        return static_cast<MetaDir>(combined);
    }

    template <typename E, E Value>
    [[nodiscard]] constexpr std::string_view EnumValueName() noexcept
    {
#if defined(_MSC_VER)
        constexpr std::string_view signature = __FUNCSIG__;
        constexpr std::string_view marker = "EnumValueName<";
        const std::size_t start0 = signature.find(marker);
        if (start0 == std::string_view::npos) return {};
        const std::size_t comma = signature.find(',', start0 + marker.size());
        const std::size_t end = signature.find(">(void)", comma);
        if (comma == std::string_view::npos || end == std::string_view::npos) return {};
        std::string_view token = signature.substr(comma + 1, end - comma - 1);
        while (!token.empty() && token.front() == ' ') token.remove_prefix(1);
#else
        constexpr std::string_view signature = __PRETTY_FUNCTION__;
        constexpr std::string_view marker = "Value = ";
        const std::size_t start0 = signature.find(marker);
        if (start0 == std::string_view::npos) return {};
        const std::size_t start = start0 + marker.size();
        std::size_t end = signature.find(';', start);
        if (end == std::string_view::npos) end = signature.find(']', start);
        if (end == std::string_view::npos) return {};
        std::string_view token = signature.substr(start, end - start);
#endif
        if (token.empty() || token.front() == '(' || token.find('(') != std::string_view::npos) return {};
        const std::size_t scope = token.rfind("::");
        if (scope != std::string_view::npos) token.remove_prefix(scope + 2);
        return token;
    }

    template <typename E, std::int32_t Min, std::size_t... I>
    [[nodiscard]] constexpr auto MakeEnumNames(std::index_sequence<I...>)
    {
        return std::array<std::pair<std::int32_t, std::string_view>, sizeof...(I)>{
            std::pair<std::int32_t, std::string_view>{Min + static_cast<std::int32_t>(I),
                EnumValueName<E, static_cast<E>(Min + static_cast<std::int32_t>(I))>()}...
        };
    }

    template <typename E, std::int32_t Min, std::int32_t Max>
    [[nodiscard]] std::string EnumStringRange(E value)
    {
        static constexpr auto names = MakeEnumNames<E, Min>(std::make_index_sequence<static_cast<std::size_t>(Max - Min + 1)>{});
        const std::int32_t raw = static_cast<std::int32_t>(value);
        if (raw >= Min && raw <= Max)
        {
            const std::string_view name = names[static_cast<std::size_t>(raw - Min)].second;
            if (!name.empty()) return std::string(name);
        }
        return std::to_string(raw);
    }

    template <typename E, std::int32_t Min, std::int32_t Max>
    [[nodiscard]] bool TryParseEnumRange(std::string_view text, E& result)
    {
        static constexpr auto names = MakeEnumNames<E, Min>(std::make_index_sequence<static_cast<std::size_t>(Max - Min + 1)>{});
        if (TryParseEnumNumeric(text, result)) return true;
        const std::string value = StringTrim(text);

        std::int32_t combined = 0;
        const std::vector<std::string> parts = Split(value, ',', false, false);
        if (parts.empty())
        {
            result = static_cast<E>(0);
            return false;
        }
        for (const std::string& partValue : parts)
        {
            const std::string part = StringTrim(partValue);
            bool matched = false;
            for (const auto& [enumRaw, name] : names)
            {
                if (!name.empty() && name == part)
                {
                    combined |= enumRaw;
                    matched = true;
                    break;
                }
            }
            if (!matched)
            {
                result = static_cast<E>(0);
                return false;
            }
        }
        result = static_cast<E>(combined);
        return true;
    }

    [[nodiscard]] std::string SeqString(SeqId value) { return EnumStringRange<SeqId, -1, 59>(value); }
    [[nodiscard]] std::string MusicString(MusicId value) { return EnumStringRange<MusicId, -1, 68>(value); }
    [[nodiscard]] std::string VoiceString(VoiceId value) { return EnumStringRange<VoiceId, -1, 11>(value); }
    [[nodiscard]] std::string SfxString(SfxId value)
    {
        const std::int32_t raw = static_cast<std::int32_t>(value);
        if (raw >= 0 && raw <= 527) return EnumStringRange<SfxId, 0, 527>(value);
        if (raw >= 0x4000 && raw <= 0x4068) return EnumStringRange<SfxId, 0x4000, 0x4068>(value);
        if (raw >= 0x8000 && raw <= 0x8030) return EnumStringRange<SfxId, 0x8000, 0x8030>(value);
        return std::to_string(raw);
    }
    [[nodiscard]] bool TryParseMusic(std::string_view text, MusicId& value) { return TryParseEnumRange<MusicId, -1, 68>(text, value); }
    [[nodiscard]] bool TryParseSeq(std::string_view text, SeqId& value) { return TryParseEnumRange<SeqId, -1, 59>(text, value); }
    [[nodiscard]] bool TryParseVoice(std::string_view text, VoiceId& value) { return TryParseEnumRange<VoiceId, -1, 11>(text, value); }
    [[nodiscard]] bool TryParseSfx(std::string_view text, SfxId& value)
    {
        if (TryParseEnumRange<SfxId, 0, 527>(text, value)) return true;
        if (TryParseEnumRange<SfxId, 0x4000, 0x4068>(text, value)) return true;
        return TryParseEnumRange<SfxId, 0x8000, 0x8030>(text, value);
    }

    [[nodiscard]] std::string OnOff(bool value) { return value ? "On" : "Off"; }
    [[nodiscard]] std::string Mark(bool selected) { return selected ? "[x]" : "[ ]"; }
    [[nodiscard]] std::string ProgramVersionBanner() { return std::string(Mods::Branding::Name) + " 0.35.1.0"; }

    void WriteLine() { std::cout << '\n'; }
    void WriteLine(std::string_view text) { std::cout << text << '\n'; }
    void ClearConsole()
    {
#if defined(_WIN32)
        HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO info{};
        if (output != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(output, &info))
        {
            const DWORD count = static_cast<DWORD>(info.dwSize.X) * static_cast<DWORD>(info.dwSize.Y);
            DWORD written = 0;
            const COORD home{0, 0};
            FillConsoleOutputCharacterW(output, L' ', count, home, &written);
            FillConsoleOutputAttribute(output, info.wAttributes, count, home, &written);
            SetConsoleCursorPosition(output, home);
            return;
        }
#endif
        std::cout << "\x1b[2J\x1b[H";
        std::cout.flush();
    }

    [[nodiscard]] std::optional<std::string> ReadLine()
    {
        std::string line;
        if (!std::getline(std::cin, line)) return std::nullopt;
        return line;
    }

    [[nodiscard]] ConsoleKey AlphaKey(char ch)
    {
        switch (static_cast<char>(std::toupper(static_cast<unsigned char>(ch))))
        {
        case 'A': return ConsoleKey::A; case 'B': return ConsoleKey::B; case 'C': return ConsoleKey::C;
        case 'D': return ConsoleKey::D; case 'E': return ConsoleKey::E; case 'F': return ConsoleKey::F;
        case 'G': return ConsoleKey::G; case 'H': return ConsoleKey::H; case 'I': return ConsoleKey::I;
        case 'J': return ConsoleKey::J; case 'L': return ConsoleKey::L; case 'M': return ConsoleKey::M;
        case 'N': return ConsoleKey::N; case 'O': return ConsoleKey::O; case 'P': return ConsoleKey::P;
        case 'Q': return ConsoleKey::Q; case 'R': return ConsoleKey::R; case 'S': return ConsoleKey::S;
        case 'T': return ConsoleKey::T; case 'U': return ConsoleKey::U; case 'V': return ConsoleKey::V;
        case 'W': return ConsoleKey::W; case 'X': return ConsoleKey::X; case 'Y': return ConsoleKey::Y;
        default: return ConsoleKey::None;
        }
    }

    [[nodiscard]] ConsoleKey DigitKey(char ch)
    {
        switch (ch)
        {
        case '1': return ConsoleKey::D1; case '2': return ConsoleKey::D2; case '3': return ConsoleKey::D3;
        case '4': return ConsoleKey::D4; case '5': return ConsoleKey::D5; case '6': return ConsoleKey::D6;
        case '7': return ConsoleKey::D7; case '8': return ConsoleKey::D8;
        default: return ConsoleKey::None;
        }
    }

    [[nodiscard]] ConsoleKeyInfo ReadKey()
    {
        std::cout.flush();
#if defined(_WIN32)
        HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
        DWORD mode = 0;
        if (input == INVALID_HANDLE_VALUE || input == nullptr || !GetConsoleMode(input, &mode))
        {
            throw System::InvalidOperationException("Cannot read keys when either application does not have a console or when console input has been redirected. Try Console.Read.");
        }
        while (true)
        {
            INPUT_RECORD record{};
            DWORD read = 0;
            if (!ReadConsoleInputW(input, &record, 1, &read) || read != 1)
            {
                throw std::runtime_error("Could not read a key from the console.");
            }
            if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown) continue;
            const KEY_EVENT_RECORD& key = record.Event.KeyEvent;
            if (key.wVirtualKeyCode == VK_SHIFT || key.wVirtualKeyCode == VK_CONTROL
                || key.wVirtualKeyCode == VK_MENU || key.wVirtualKeyCode == VK_CAPITAL
                || key.wVirtualKeyCode == VK_NUMLOCK || key.wVirtualKeyCode == VK_SCROLL)
            {
                continue;
            }

            ConsoleKeyInfo result{};
            result.Control = (key.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
            switch (key.wVirtualKeyCode)
            {
            case VK_ESCAPE: result.Key = ConsoleKey::Escape; break;
            case VK_RETURN: result.Key = ConsoleKey::Enter; break;
            case VK_SPACE: result.Key = ConsoleKey::Spacebar; break;
            case VK_BACK: result.Key = ConsoleKey::Backspace; break;
            case VK_DELETE: result.Key = ConsoleKey::Delete; break;
            case VK_UP: result.Key = ConsoleKey::UpArrow; break;
            case VK_DOWN: result.Key = ConsoleKey::DownArrow; break;
            case VK_LEFT: result.Key = ConsoleKey::LeftArrow; break;
            case VK_RIGHT: result.Key = ConsoleKey::RightArrow; break;
            case VK_PRIOR: result.Key = ConsoleKey::PageUp; break;
            case VK_NEXT: result.Key = ConsoleKey::PageDown; break;
            case VK_ADD: result.Key = ConsoleKey::Add; break;
            case VK_SUBTRACT: result.Key = ConsoleKey::Subtract; break;
            case VK_OEM_PLUS: result.Key = ConsoleKey::OemPlus; break;
            case VK_OEM_MINUS: result.Key = ConsoleKey::OemMinus; break;
            case VK_NUMPAD1: result.Key = ConsoleKey::NumPad1; break;
            case VK_NUMPAD2: result.Key = ConsoleKey::NumPad2; break;
            case VK_NUMPAD3: result.Key = ConsoleKey::NumPad3; break;
            case VK_NUMPAD4: result.Key = ConsoleKey::NumPad4; break;
            case VK_NUMPAD5: result.Key = ConsoleKey::NumPad5; break;
            case VK_NUMPAD6: result.Key = ConsoleKey::NumPad6; break;
            case VK_NUMPAD7: result.Key = ConsoleKey::NumPad7; break;
            case VK_NUMPAD8: result.Key = ConsoleKey::NumPad8; break;
            default:
                if (key.wVirtualKeyCode >= 'A' && key.wVirtualKeyCode <= 'Z')
                    result.Key = AlphaKey(static_cast<char>(key.wVirtualKeyCode));
                else if (key.wVirtualKeyCode >= '1' && key.wVirtualKeyCode <= '8')
                    result.Key = DigitKey(static_cast<char>(key.wVirtualKeyCode));
                break;
            }

            // Console.ReadKey() defaults to intercept:false, so echo KeyChar but not
            // virtual-key-only events such as arrows and page navigation keys.
            if (key.uChar.UnicodeChar != L'\0')
            {
                (void)_putwch(key.uChar.UnicodeChar);
            }
            return result;
        }
#else
        if (::isatty(STDIN_FILENO) == 0) throw System::InvalidOperationException("Cannot read keys when either application does not have a console or when console input has been redirected. Try Console.Read.");
        termios original{};
        if (::tcgetattr(STDIN_FILENO, &original) != 0) throw std::runtime_error("Could not read console mode for Console.ReadKey.");
        termios current = original;
        current.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
        current.c_cc[VMIN] = 1; current.c_cc[VTIME] = 0;
        if (::tcsetattr(STDIN_FILENO, TCSANOW, &current) != 0) throw std::runtime_error("Could not configure the console for Console.ReadKey.");
        struct Restore { termios value; ~Restore(){ (void)::tcsetattr(STDIN_FILENO, TCSANOW, &value); } } restore{original};
        unsigned char first = 0;
        if (::read(STDIN_FILENO, &first, 1) != 1) throw std::runtime_error("Could not read a key from the console.");
        ConsoleKeyInfo result{};
        if (first == 0x1B)
        {
            pollfd descriptor{STDIN_FILENO, POLLIN, 0};
            if (::poll(&descriptor, 1, 30) > 0)
            {
                unsigned char second = 0;
                if (::read(STDIN_FILENO, &second, 1) == 1 && (second == '[' || second == 'O'))
                {
                    std::array<unsigned char, 16> sequence{};
                    std::size_t count = 0;
                    while (count < sequence.size())
                    {
                        unsigned char value = 0;
                        if (::read(STDIN_FILENO, &value, 1) != 1) break;
                        sequence[count++] = value;
                        if (value >= 0x40 && value <= 0x7E) break;
                        descriptor.revents = 0;
                        if (::poll(&descriptor, 1, 10) <= 0) break;
                    }
                    if (count != 0)
                    {
                        const unsigned char final = sequence[count - 1];
                        const std::string parameters(reinterpret_cast<const char*>(sequence.data()), count - 1);
                        auto Parameter = [&](std::size_t index) -> std::optional<std::int32_t>
                        {
                            std::size_t start = 0;
                            for (std::size_t i = 0; i < index; ++i)
                            {
                                const std::size_t separator = parameters.find(';', start);
                                if (separator == std::string::npos) return std::nullopt;
                                start = separator + 1;
                            }
                            const std::size_t end = parameters.find(';', start);
                            const std::string_view part(parameters.data() + start,
                                (end == std::string::npos ? parameters.size() : end) - start);
                            std::int32_t parsed = 0;
                            if (part.empty() || !Int32TryParseCurrentCulture(part, parsed)) return std::nullopt;
                            return parsed;
                        };
                        auto Modifier = [&]() -> std::int32_t
                        {
                            const std::size_t separator = parameters.rfind(';');
                            if (separator == std::string::npos) return 1;
                            std::int32_t parsed = 1;
                            if (!Int32TryParseCurrentCulture(std::string_view(parameters).substr(separator + 1), parsed)) return 1;
                            return parsed;
                        };
                        const std::int32_t modifier = Modifier();
                        result.Control = modifier > 1 && ((modifier - 1) & 4) != 0;
                        if (final == '^') result.Control = true; // rxvt Ctrl+navigation form

                        if (final == 'A') result.Key = ConsoleKey::UpArrow;
                        else if (final == 'B') result.Key = ConsoleKey::DownArrow;
                        else if (final == 'C') result.Key = ConsoleKey::RightArrow;
                        else if (final == 'D') result.Key = ConsoleKey::LeftArrow;
                        else if (final == '~' || final == '^')
                        {
                            const std::int32_t keyCode = Parameter(0).value_or(0);
                            if (keyCode == 5) result.Key = ConsoleKey::PageUp;
                            else if (keyCode == 6) result.Key = ConsoleKey::PageDown;
                            else if (keyCode == 3) result.Key = ConsoleKey::Delete;
                        }
                        return result;
                    }
                }
            }
            (void)::write(STDOUT_FILENO, &first, 1);
            result.Key = ConsoleKey::Escape;
            return result;
        }

        if (first >= 0x80)
        {
            const std::size_t length = (first & 0xE0U) == 0xC0U ? 2
                : (first & 0xF0U) == 0xE0U ? 3
                : (first & 0xF8U) == 0xF0U ? 4 : 1;
            std::array<unsigned char, 4> bytes{};
            bytes[0] = first;
            std::size_t read = 1;
            while (read < length)
            {
                if (::read(STDIN_FILENO, &bytes[read], 1) != 1) break;
                ++read;
            }
            (void)::write(STDOUT_FILENO, bytes.data(), read);
            return result;
        }

        unsigned char echo = first == 0x7F ? static_cast<unsigned char>('\b') : first;
        (void)::write(STDOUT_FILENO, &echo, 1);
        if (first == '\r' || first == '\n') result.Key = ConsoleKey::Enter;
        else if (first == ' ') result.Key = ConsoleKey::Spacebar;
        else if (first == 0x7F || first == '\b') result.Key = ConsoleKey::Backspace;
        else if (first == '+') result.Key = ConsoleKey::OemPlus;
        else if (first == '-') result.Key = ConsoleKey::OemMinus;
        else if (first >= 1 && first <= 26) { result.Control = true; result.Key = AlphaKey(static_cast<char>('A' + first - 1)); }
        else { result.Key = AlphaKey(static_cast<char>(first)); if (result.Key == ConsoleKey::None) result.Key = DigitKey(static_cast<char>(first)); }
        return result;
#endif
    }

    [[nodiscard]] std::int32_t GetState(std::string_view state)
    {
        if (state == "escape") return 1;
        if (state == "done") return 2;
        return 0;
    }
    [[nodiscard]] std::string FormatState(std::int32_t state)
    {
        if (state == 1) return "escape";
        if (state == 2) return "done";
        return "none";
    }

    [[nodiscard]] std::string JoinNonEmpty(const std::vector<std::string>& values, std::string_view separator)
    {
        std::string result;
        bool first = true;
        for (const std::string& value : values)
        {
            if (value.empty()) continue;
            if (!first) result += separator;
            result += value; first = false;
        }
        return result;
    }

    [[nodiscard]] Decimal Advance(const Decimal& current, const std::vector<Decimal>& values, std::int32_t direction)
    {
        if (direction == 1)
        {
            std::optional<Decimal> update;
            for (const Decimal& value : values) if (value > current && (!update || value < *update)) update = value;
            return update ? *update : values.front();
        }
        std::optional<Decimal> update;
        for (auto it = values.rbegin(); it != values.rend(); ++it) if (*it < current && (!update || *it > *update)) update = *it;
        return update ? *update : values.back();
    }

    [[nodiscard]] std::int32_t AdvanceInt(std::int32_t current, std::int32_t direction, std::int32_t maxValue)
    {
        current += direction;
        if (current < 0) current = maxValue;
        else if (current > maxValue) current = 0;
        return current;
    }

    void SetDefaultLanguage()
    {
        _language = Paths::IsMphJapan() || Paths::IsMphKorea()
            ? Language::Japanese : Language::English;
    }

    [[nodiscard]] bool GetTime(std::string input, Decimal& result)
    {
        result = Decimal(0);
        const std::vector<std::string> split = Split(input, ':', false, false);
        std::int32_t a = 0, b = 0, c = 0;
        auto Wrap = [](std::uint32_t value) { return static_cast<std::int32_t>(value); };
        if (split.size() == 1)
        {
            if (Int32TryParseCurrentCulture(split[0], a)) { result = Decimal(Wrap(static_cast<std::uint32_t>(a) * 60U)); return true; }
        }
        else if (split.size() == 2)
        {
            if (Int32TryParseCurrentCulture(split[0], a) && Int32TryParseCurrentCulture(split[1], b))
            {
                result = Decimal(Wrap(static_cast<std::uint32_t>(a) * 60U + static_cast<std::uint32_t>(b))); return true;
            }
        }
        else if (split.size() == 3)
        {
            if (Int32TryParseCurrentCulture(split[0], a) && Int32TryParseCurrentCulture(split[1], b) && Int32TryParseCurrentCulture(split[2], c))
            {
                result = Decimal(Wrap(static_cast<std::uint32_t>(a) * 3600U
                    + static_cast<std::uint32_t>(b) * 60U + static_cast<std::uint32_t>(c))); return true;
            }
        }
        return false;
    }

    [[nodiscard]] std::string FormatTime(const Decimal& value)
    {
        const float secondsFloat = value.ToSingle();
        const auto ticks = static_cast<std::int64_t>(static_cast<double>(secondsFloat) * 10000000.0);
        const std::int64_t totalSeconds = ticks / 10000000LL;
        const std::int32_t hours = static_cast<std::int32_t>((totalSeconds / 3600LL) % 24LL);
        const std::int32_t minutes = static_cast<std::int32_t>((totalSeconds / 60LL) % 60LL);
        const std::int32_t seconds = static_cast<std::int32_t>(totalSeconds % 60LL);
        std::ostringstream stream;
        if (hours > 0) stream << hours << ':' << std::setfill('0') << std::setw(2) << minutes << ':' << std::setw(2) << seconds;
        else if (minutes > 0) stream << minutes << ':' << std::setfill('0') << std::setw(2) << seconds;
        else stream << "0:" << std::setfill('0') << std::setw(2) << seconds;
        return stream.str();
    }

    void ResetGoal()
    {
        if (_mode == "auto-select" || StartsWith(_mode, "Battle")) _pointGoal = Decimal(7);
        else if (StartsWith(_mode, "Survival")) _pointGoal = Decimal(2);
        else if (_mode == "Capture") _pointGoal = Decimal(5);
        else if (StartsWith(_mode, "Bounty")) _pointGoal = Decimal(3);
        else if (StartsWith(_mode, "Nodes")) _pointGoal = Decimal(70);
        else if (StartsWith(_mode, "Defender") || _mode == "Prime Hunter") _timeGoal = Decimal(90);
    }
    void ResetTimeLimit()
    {
        _timeLimit = Decimal((_mode == "auto-select" || StartsWith(_mode, "Battle")) ? 420 : 900);
    }
    void UpdateSettings()
    {
        _teams = false;
        _pointGoal = Decimal(0); _timeGoal = Decimal(0); _timeLimit = Decimal(0);
        _octolithReset = true;
        ResetGoal(); ResetTimeLimit();
        if (_mode == "auto-select" || StartsWith(_mode, "Battle") || StartsWith(_mode, "Nodes")) _goalType = "Point Goal";
        else if (StartsWith(_mode, "Survival")) _goalType = "Extra Lives";
        else if (_mode == "Capture" || StartsWith(_mode, "Bounty")) _goalType = "Octolith Goal";
        else if (StartsWith(_mode, "Defender") || _mode == "Prime Hunter") _goalType = "Time Goal";
        if (_mode == "Capture" || EndsWith(_mode, "Teams")) _teams = true;
    }

    void ReadTimeGoal(const std::optional<std::string>& input)
    {
        if (StringIsNullOrWhiteSpace(input)) return;
        const std::string text = ToUpper(StringTrim(*input));
        if (_goalType == "Time Goal")
        {
            Decimal result;
            if (GetTime(text, result)) _timeGoal = result;
        }
        else
        {
            Decimal result;
            if (Decimal::TryParse(text, result))
            {
                const std::int32_t integer = result.ToInt32();
                _pointGoal = Decimal(std::clamp(integer, _goalType == "Extra Lives" ? 0 : 1, 99999));
            }
        }
    }

    void ReadTimeLimit(const std::optional<std::string>& input)
    {
        if (StringIsNullOrWhiteSpace(input)) return;
        Decimal result;
        if (GetTime(ToUpper(StringTrim(*input)), result)) _timeLimit = result;
    }

    [[nodiscard]] GameMode ParseGameMode(std::string_view mode)
    {
        if (mode == "Battle") return GameMode::Battle;
        if (mode == "BattleTeams") return GameMode::BattleTeams;
        if (mode == "Survival") return GameMode::Survival;
        if (mode == "SurvivalTeams") return GameMode::SurvivalTeams;
        if (mode == "Capture") return GameMode::Capture;
        if (mode == "Bounty") return GameMode::Bounty;
        if (mode == "BountyTeams") return GameMode::BountyTeams;
        if (mode == "Nodes") return GameMode::Nodes;
        if (mode == "NodesTeams") return GameMode::NodesTeams;
        if (mode == "Defender") return GameMode::Defender;
        if (mode == "DefenderTeams") return GameMode::DefenderTeams;
        if (mode == "PrimeHunter") return GameMode::PrimeHunter;
        throw std::invalid_argument("Requested value was not found.");
    }

    class RendererOwner final
    {
    public:
        RendererOwner() : _renderer(new RenderWindow())
        {
            if (_renderer == nullptr) throw System::NullReferenceException();
        }
        ~RendererOwner() { delete _renderer; }
        RendererOwner(const RendererOwner&) = delete;
        RendererOwner& operator=(const RendererOwner&) = delete;
        [[nodiscard]] RenderWindow* Get() const noexcept { return _renderer; }
    private:
        RenderWindow* _renderer;
    };
}

namespace MphRead
{
    std::uint8_t Menu::SaveSlot = 0;
    std::int32_t Menu::PreviousSaveSlot = -1;
    SaveWhen Menu::SaveFromExit = static_cast<SaveWhen>(0);
    SaveWhen Menu::SaveFromShip = static_cast<SaveWhen>(2);
    SaveWhen Menu::NeededSave = static_cast<SaveWhen>(0);

    void Menu::PrintSoundInfo(SoundCapability soundCapability)
    {
        if (soundCapability == SoundCapability::None)
        {
            WriteLine();
            WriteLine("WARNING: Audio system could not be loaded. Sound effects will not be played.");
            WriteLine("You may need to install OpenAL Soft on your system.");
            WriteLine("Music and video playback will not be affected.");
        }
        else if (soundCapability == SoundCapability::Unsupported)
        {
            WriteLine();
            WriteLine("WARNING: Audio system was loaded, but an unsupported version of OpenAL was used.");
            WriteLine("You may need to install OpenAL Soft on your system for sounds to play correctly.");
            WriteLine("Music and video playback will not be affected.");
        }
    }

    void Menu::ShowMenuPrompts()
    {
        const SoundCapability soundCapability = Sound::Sfx::CheckAudioLoad();
        std::int32_t prompt = 0;
        std::int32_t selection = 18;
        std::int32_t roomId = -1;
        std::string room;
        std::string roomKey = "MP3 PROVING GROUND";
        if (roomId >= 0)
        {
            const auto init = Metadata::GetRoomById(roomId);
            assert(init != nullptr);
            room = (*init).InGameName.value_or((*init).Name);
            roomKey = (*init).Name;
        }
        else if (!roomKey.empty())
        {
            const auto [init, id] = Metadata::GetRoomByName(roomKey);
            assert(init != nullptr);
            roomId = id;
            room = (*init).InGameName.value_or((*init).Name);
        }
        bool fhRoom = false;

        struct PlayerSetting
        {
            std::string HunterName;
            std::string Team;
            std::string Recolor;
        };
        struct ModelSetting
        {
            std::string Name;
            std::int32_t Recolor = 0;
            bool FirstHunt = false;
            MetaDir Dir = static_cast<MetaDir>(0);
        };

        std::vector<PlayerSetting> players{
            {"Samus", "orange", "0"}, {"none", "green", "0"},
            {"none", "orange", "0"}, {"none", "green", "0"}
        };
        std::vector<std::int32_t> playerIds{0, -1, -1, -1};
        const std::vector<std::string> hunters{
            "Samus", "Kanden", "Spire", "Trace", "Noxus", "Sylux", "Weavel", "Guardian"
        };
        std::int32_t modeId = 0;
        const std::unordered_map<std::string, std::string> modeOpts{
            {"adventure", "Adventure"}, {"story", "Adventure"}, {"1p", "Adventure"},
            {"battle", "Battle"}, {"battleteams", "Battle Teams"}, {"survival", "Survival"},
            {"survivalteams", "Survival Teams"}, {"capture", "Capture"}, {"bounty", "Bounty"},
            {"bountyteams", "Bounty Teams"}, {"nodes", "Nodes"}, {"nodesteams", "Nodes Teams"},
            {"defender", "Defender"}, {"defenderteams", "Defender Teams"}, {"primehunter", "Prime Hunter"}
        };
        const std::vector<std::string> modes{
            "auto-select", "Adventure", "Battle", "Battle Teams", "Survival", "Survival Teams", "Capture",
            "Bounty", "Bounty Teams", "Nodes", "Nodes Teams", "Defender", "Defender Teams", "Prime Hunter"
        };
        [[maybe_unused]] const std::vector<std::string> teamsModes{
            "Battle Teams", "Survival Teams", "Capture", "Bounty Teams", "Nodes Teams", "Defender Teams"
        };
        std::vector<ModelSetting> models;
        const std::vector<std::string> mphVersions{
            std::string(A76E0), std::string(AMHE0), std::string(AMHE1), std::string(AMHP0),
            std::string(AMHP1), std::string(AMHJ0), std::string(AMHJ1), std::string(AMHK0)
        };
        const std::vector<std::string> fhVersions{std::string(AMFE0), std::string(AMFP0)};
        const std::unordered_map<std::string, std::string> mphInfo{
            {std::string(A76E0), "Kiosk demo"}, {std::string(AMHE0), "USA rev 0"},
            {std::string(AMHE1), "USA rev 1"}, {std::string(AMHP0), "EUR rev 0"},
            {std::string(AMHP1), "EUR rev 1"}, {std::string(AMHJ0), "JPN rev 0"},
            {std::string(AMHJ1), "JPN rev 1"}, {std::string(AMHK0), "KOR rev 0"}
        };
        const std::unordered_map<std::string, std::string> fhInfo{
            {std::string(AMFE0), "USA rev 0"}, {std::string(AMFP0), "EUR rev 0"}
        };

        auto ReadRoom = [&](const std::optional<std::string>& input)
        {
            if (StringIsNullOrWhiteSpace(input)) return;
            const std::string value = ToLower(StringTrim(*input));
            std::int32_t id = 0;
            const RoomMetadata* meta = nullptr;
            if (Int32TryParseCurrentCulture(value, id))
            {
                meta = Metadata::GetRoomById(id, true);
                if (meta)
                {
                    roomId = id;
                    room = (*meta).InGameName.value_or((*meta).Name);
                    roomKey = (*meta).Name;
                    fhRoom = (*meta).FirstHunt;
                }
                return;
            }
            const auto& rooms = Metadata::RoomList;
            for (const auto& candidate : rooms)
            {
                if (candidate && ToLower((*candidate).Name) == value)
                {
                    meta = candidate.get();
                    break;
                }
            }
            if (!meta)
            {
                const bool multi = _mode != "Adventure";
                for (const auto& candidate : rooms)
                {
                    if (!candidate) continue;
                    const auto& inGame = (*candidate).InGameName;
                    if (inGame && ToLower(*inGame) == value && (*candidate).Multiplayer == multi)
                    {
                        meta = candidate.get();
                        break;
                    }
                }
                if (!meta)
                {
                    for (const auto& candidate : rooms)
                    {
                        if (!candidate) continue;
                        const auto& inGame = (*candidate).InGameName;
                        if (inGame && ToLower(*inGame) == value)
                        {
                            meta = candidate.get();
                            break;
                        }
                    }
                }
            }
            if (meta)
            {
                roomId = (*meta).Id;
                room = (*meta).InGameName.value_or((*meta).Name);
                roomKey = (*meta).Name;
                fhRoom = (*meta).FirstHunt;
            }
        };

        auto ReadMode = [&](const std::optional<std::string>& input)
        {
            if (StringIsNullOrWhiteSpace(input)) return;
            std::string value = ToLower(StringTrim(*input));
            value = StringReplace(std::move(value), " ", "");
            const auto it = modeOpts.find(value);
            if (it != modeOpts.end())
            {
                _mode = it->second;
                const auto modeIt = std::find(modes.begin(), modes.end(), _mode);
                modeId = static_cast<std::int32_t>(std::distance(modes.begin(), modeIt));
            }
            else
            {
                _mode = "auto-select";
                modeId = 0;
            }
        };

        auto ReadPlayer = [&](const std::optional<std::string>& input, std::int32_t index)
        {
            if (StringIsNullOrWhiteSpace(input)) return;
            const std::string value = ToLower(StringTrim(*input));
            const std::vector<std::string> split = Split(value, ' ', false, false);
            std::string player = "none";
            std::string name = split.at(0);
            if (!name.empty())
            {
                const Utf8Scalar first = DecodeUtf8Scalar(name, 0);
                const std::size_t firstLength = first.Valid() ? first.Length : 1;
                name = ToUpper(name.substr(0, firstLength)) + name.substr(firstLength);
                Hunter hunter{};
                if (TryParseHunter(name, hunter) && IsDefinedHunter(hunter) && hunter != Hunter::Random)
                {
                    player = HunterString(hunter);
                }
            }
            std::string team = players.at(static_cast<std::size_t>(index)).Team;
            std::string recolor = players.at(static_cast<std::size_t>(index)).Recolor;
            if (split.size() > 1)
            {
                if (_teams)
                {
                    const std::string teamValue = ToLower(split[1]);
                    if (teamValue == "orange" || teamValue == "red") team = "orange";
                    else if (teamValue == "green") team = "green";
                    else
                    {
                        std::int32_t parsed = 0;
                        if (Int32TryParseCurrentCulture(split[1], parsed)) team = std::clamp(parsed, 0, 1) == 0 ? "orange" : "green";
                    }
                }
                else
                {
                    std::int32_t parsed = 0;
                    if (Int32TryParseCurrentCulture(split[1], parsed)) recolor = std::to_string(std::clamp(parsed, 0, 5));
                }
            }
            players.at(static_cast<std::size_t>(index)) = PlayerSetting{player, team, recolor};
            if (player == "none") playerIds.at(static_cast<std::size_t>(index)) = -1;
            else
            {
                const auto it = std::find(hunters.begin(), hunters.end(), player);
                playerIds.at(static_cast<std::size_t>(index)) = static_cast<std::int32_t>(std::distance(hunters.begin(), it));
            }
        };

        auto ReadModels = [&](const std::optional<std::string>& input)
        {
            if (StringIsNullOrWhiteSpace(input)) return;
            models.clear();
            const auto split = Split(*input, ',', false, false);
            for (const std::string& raw : split)
            {
                const auto parts = Split(StringTrim(raw), ' ', true, true);
                std::int32_t recolor = 0;
                bool firstHunt = false;
                MetaDir dir = static_cast<MetaDir>(0);
                for (std::size_t i = 1; i < parts.size(); ++i)
                {
                    const std::string& part = parts[i];
                    if (ToLower(part) == "fh") firstHunt = true;
                    else
                    {
                        std::int32_t parsed = 0;
                        if (Int32TryParseCurrentCulture(part, parsed)) recolor = parsed;
                        else
                        {
                            recolor = 0;
                            const auto parsedDir = TryParseMetaDir(part);
                            if (parsedDir) dir = *parsedDir;
                            else dir = static_cast<MetaDir>(0);
                        }
                    }
                }
                const std::string& modelName = parts.at(0);
                const auto meta = firstHunt
                    ? Metadata::GetFirstHuntModelByName(modelName)
                    : Metadata::GetModelByName(modelName, dir);
                if (meta)
                {
                    recolor = std::clamp(recolor, 0, static_cast<std::int32_t>((*meta).Recolors.size()) - 1);
                    models.push_back(ModelSetting{(*meta).Name, recolor, firstHunt, dir});
                }
            }
        };

        auto ReadMphVersion = [&](const std::optional<std::string>& input)
        {
            if (StringIsNullOrWhiteSpace(input)) return;
            const std::string value = ToUpper(StringTrim(*input));
            if (std::find(mphVersions.begin(), mphVersions.end(), value) != mphVersions.end()
                && !Paths::AllPaths().at(value).empty())
            {
                Paths::MphKey = value;
                SetDefaultLanguage();
            }
        };
        auto ReadFhVersion = [&](const std::optional<std::string>& input)
        {
            if (StringIsNullOrWhiteSpace(input)) return;
            const std::string value = ToUpper(StringTrim(*input));
            if (std::find(fhVersions.begin(), fhVersions.end(), value) != fhVersions.end()
                && !Paths::AllPaths().at(value).empty())
            {
                Paths::FhKey = value;
                SetDefaultLanguage();
            }
        };

        auto LoadSettings = [&](const MenuSettings& settings)
        {
            ReadRoom(settings.RoomKey);
            ReadMode(settings.Mode);
            ReadPlayer(settings.Player1, 0);
            ReadPlayer(settings.Player2, 1);
            ReadPlayer(settings.Player3, 2);
            ReadPlayer(settings.Player4, 3);
            ReadModels(settings.Models);
            ReadMphVersion(settings.MphVersion);
            ReadFhVersion(settings.MphVersion);
            ReadTimeLimit(settings.TimeLimit);
            ReadTimeGoal(settings.TimeGoal);
            Language language{};
            if (TryParseLanguage(settings.Language, language)) _language = language;
            Decimal decimalValue;
            if (Decimal::TryParse(settings.SfxVolume, decimalValue))
            {
                _sfxVolume = decimalValue;
                Sound::Sfx::Volume = _sfxVolume.ToSingle();
            }
            if (Decimal::TryParse(settings.MusicVolume, decimalValue))
            {
                _musicVolume = decimalValue;
                Music::UserVolume(_musicVolume.ToSingle());
            }
            std::int32_t integer = 0;
            if (Int32TryParseCurrentCulture(settings.PointGoal, integer)) _pointGoal = Decimal(integer);
            _octolithReset = settings.PointGoal != "off";
            _teams = settings.TeamPlay != "off";
            _radarPlayers = settings.HunterRadar != "off";
            if (settings.DamageLevel == "low") _damageLevel = 0;
            else if (settings.DamageLevel == "medium") _damageLevel = 1;
            else if (settings.DamageLevel == "high") _damageLevel = 2;
            _friendlyFire = settings.FriendlyFire != "off";
            _affinityWeapons = settings.AffinityWeapons != "off";
            if (settings.SaveSlot == "none") SaveSlot = 0;
            else
            {
                std::uint8_t slot = 0;
                if (TryParseByte(settings.SaveSlot, slot)) SaveSlot = slot;
            }
            UpdateSaveInfo();
            SaveFromExit = ParseSaveWhen(settings.SaveFromExit, static_cast<SaveWhen>(0));
            SaveFromShip = ParseSaveWhen(settings.SaveFromShip, static_cast<SaveWhen>(2));
            const auto planets = Split(ToLower(settings.Planets), ',', true, true);
            if (!planets.empty())
            {
                auto Has = [&](std::string_view value) { return std::find(planets.begin(), planets.end(), value) != planets.end(); };
                _planets[0] = Has("ca") ? 1 : 0; _planets[1] = Has("alinos") ? 1 : 0;
                _planets[2] = Has("vdo") ? 1 : 0; _planets[3] = Has("arcterra") ? 1 : 0; _planets[4] = Has("oubliette") ? 1 : 0;
            }
            _ca1State = GetState(settings.Ca1State); _ca2State = GetState(settings.Ca2State);
            _alinos1State = GetState(settings.Alinos1State); _alinos2State = GetState(settings.Alinos2State);
            _vdo1State = GetState(settings.Vdo1State); _vdo2State = GetState(settings.Vdo2State);
            _arcterra1State = GetState(settings.Arcterra1State); _arcterra2State = GetState(settings.Arcterra2State);
            if (settings.CheckpointId == "none") _checkpointId = -1;
            else if (Int32TryParseCurrentCulture(settings.CheckpointId, integer) && integer >= 0) _checkpointId = integer;
            if (Int32TryParseCurrentCulture(settings.HealthMax, integer)) _healthMax = std::max(integer, 1);
            if (Int32TryParseCurrentCulture(settings.MissileMax, integer)) _missileMax = std::max(integer, 0);
            if (Int32TryParseCurrentCulture(settings.UaMax, integer)) _uaMax = std::max(integer, 0);
            const auto weapons = Split(ToLower(settings.Weapons), ',', true, true);
            if (!weapons.empty())
            {
                auto Has = [&](std::string_view value) { return std::find(weapons.begin(), weapons.end(), value) != weapons.end(); };
                _weapons[0] = Has("power beam") ? 1 : 0; _weapons[2] = Has("missiles") ? 1 : 0;
                _weapons[1] = Has("volt driver") ? 1 : 0; _weapons[3] = Has("battlehammer") ? 1 : 0;
                _weapons[4] = Has("imperialist") ? 1 : 0; _weapons[5] = Has("judicator") ? 1 : 0;
                _weapons[6] = Has("magmaul") ? 1 : 0; _weapons[7] = Has("shock coil") ? 1 : 0;
                _weapons[8] = Has("omega cannon") ? 1 : 0;
            }
            const auto octoliths = Split(ToLower(settings.Octoliths), ',', true, true);
            if (!octoliths.empty())
            {
                auto Has = [&](std::string_view value) { return std::find(octoliths.begin(), octoliths.end(), value) != octoliths.end(); };
                _octoliths[0] = Has("ca1") ? 1 : 0; _octoliths[1] = Has("ca2") ? 1 : 0;
                _octoliths[2] = Has("alinos1") ? 1 : 0; _octoliths[3] = Has("alinos2") ? 1 : 0;
                _octoliths[4] = Has("vdo1") ? 1 : 0; _octoliths[5] = Has("vdo2") ? 1 : 0;
                _octoliths[6] = Has("arcterra1") ? 1 : 0; _octoliths[7] = Has("arcterra2") ? 1 : 0;
            }
        };

        auto CommitSettings = [&]()
        {
            auto FormatHunter = [&](std::int32_t index)
            {
                const std::int32_t id = playerIds.at(static_cast<std::size_t>(index));
                if (id < 0) return std::string("none");
                const auto& player = players.at(static_cast<std::size_t>(index));
                return hunters.at(static_cast<std::size_t>(id)) + " " + (_teams ? player.Team : player.Recolor);
            };
            std::vector<std::string> planetValues{
                _planets[0] != 0 ? "CA" : "", _planets[1] != 0 ? "Alinos" : "", _planets[2] != 0 ? "VDO" : "",
                _planets[3] != 0 ? "Arcterra" : "", _planets[4] != 0 ? "Oubliette" : ""
            };
            std::vector<std::string> weaponValues{
                _weapons[0] != 0 ? "Power Beam" : "", _weapons[2] != 0 ? "Missiles" : "", _weapons[1] != 0 ? "Volt Driver" : "",
                _weapons[3] != 0 ? "Battlehammer" : "", _weapons[4] != 0 ? "Imperialist" : "", _weapons[5] != 0 ? "Judicator" : "",
                _weapons[6] != 0 ? "Magmaul" : "", _weapons[7] != 0 ? "Shock Coil" : "", _weapons[8] != 0 ? "Omega Cannon" : ""
            };
            std::vector<std::string> octolithValues{
                _octoliths[0] != 0 ? "CA1" : "", _octoliths[1] != 0 ? "CA2" : "", _octoliths[2] != 0 ? "Alinos1" : "",
                _octoliths[3] != 0 ? "Alinos2" : "", _octoliths[4] != 0 ? "VDO1" : "", _octoliths[5] != 0 ? "VDO2" : "",
                _octoliths[6] != 0 ? "Arcterra1" : "", _octoliths[7] != 0 ? "Arcterra2" : ""
            };
            std::vector<std::string> modelValues;
            modelValues.reserve(models.size());
            for (const auto& model : models) modelValues.push_back(model.Name + " " + std::to_string(model.Recolor));

            auto settings = std::make_shared<MenuSettings>();
            settings->RoomKey = roomKey; settings->Mode = _mode;
            settings->Player1 = FormatHunter(0); settings->Player2 = FormatHunter(1);
            settings->Player3 = FormatHunter(2); settings->Player4 = FormatHunter(3);
            settings->Models = JoinNonEmpty(modelValues, ", ");
            settings->MphVersion = Paths::MphKey; settings->FhVersion = Paths::FhKey;
            settings->Language = LanguageString(_language); settings->SfxVolume = _sfxVolume.ToString(); settings->MusicVolume = _musicVolume.ToString();
            settings->PointGoal = _pointGoal.ToString(); settings->TimeLimit = ::FormatTime(_timeLimit); settings->TimeGoal = ::FormatTime(_timeGoal);
            settings->AutoReset = _octolithReset ? "on" : "off"; settings->TeamPlay = _teams ? "on" : "off";
            settings->HunterRadar = _radarPlayers ? "on" : "off";
            settings->DamageLevel = _damageLevel == 0 ? "low" : _damageLevel == 1 ? "medium" : _damageLevel == 2 ? "high" : "medium";
            settings->FriendlyFire = _friendlyFire ? "on" : "off"; settings->AffinityWeapons = _affinityWeapons ? "on" : "off";
            settings->SaveSlot = SaveSlot == 0 ? "none" : std::to_string(SaveSlot);
            settings->SaveFromExit = SaveWhenString(SaveFromExit); settings->SaveFromShip = SaveWhenString(SaveFromShip);
            settings->Planets = JoinNonEmpty(planetValues, ",");
            settings->Alinos1State = FormatState(_alinos1State); settings->Alinos2State = FormatState(_alinos2State);
            settings->Ca1State = FormatState(_ca1State); settings->Ca2State = FormatState(_ca2State);
            settings->Vdo1State = FormatState(_vdo1State); settings->Vdo2State = FormatState(_vdo2State);
            settings->Arcterra1State = FormatState(_arcterra1State); settings->Arcterra2State = FormatState(_arcterra2State);
            settings->CheckpointId = _checkpointId == -1 ? "none" : std::to_string(_checkpointId);
            settings->HealthMax = std::to_string(_healthMax); settings->MissileMax = std::to_string(_missileMax); settings->UaMax = std::to_string(_uaMax);
            settings->Weapons = JoinNonEmpty(weaponValues, ","); settings->Octoliths = JoinNonEmpty(octolithValues, ",");
            GameState::CommitSettings(settings);
        };

        auto PrintPlayer = [&](std::int32_t index)
        {
            const auto& player = players.at(static_cast<std::size_t>(index));
            if (player.HunterName == "none") return std::string("none");
            if (_teams) return player.HunterName + ", " + player.Team + " team";
            return player.HunterName + ", suit color " + player.Recolor;
        };
        auto PrintModels = [&]()
        {
            if (models.empty()) return std::string("none");
            std::vector<std::string> values;
            values.reserve(models.size());
            for (const auto& model : models) values.push_back(model.Name + " " + std::to_string(model.Recolor));
            return JoinNonEmpty(values, ", ");
        };
        auto X = [&](std::int32_t index) { return Mark(selection == index); };

        const auto loadedSettings = GameState::LoadSettings();
        LoadSettings(*loadedSettings);
        ::UpdateSettings();

        while (true)
        {
            if (NeededSave != static_cast<SaveWhen>(0) && SaveSlot != 0)
            {
                if (NeededSave == static_cast<SaveWhen>(1)) GameState::CommitSave();
                else if (NeededSave == static_cast<SaveWhen>(2))
                {
                    ConsoleKey input = ConsoleKey::None;
                    while (input != ConsoleKey::Y && input != ConsoleKey::N && input != ConsoleKey::Escape)
                    {
                        ClearConsole(); WriteLine(ProgramVersionBanner()); WriteLine();
                        WriteLine("Save game to slot " + std::to_string(SaveSlot) + "? (y/n)");
                        input = ReadKey().Key;
                        if (input == ConsoleKey::Y) GameState::CommitSave();
                    }
                }
            }
            UpdateSaveInfo();
            NeededSave = static_cast<SaveWhen>(0);
            while (true)
            {
                std::int32_t s = 0;
                if (prompt == -1) { if (!ShowSettingsPrompts()) return; prompt = 0; }
                else if (prompt == -2) { if (!ShowStoryModePrompts()) return; prompt = 0; }
                else if (prompt == -3) { if (!ShowFeaturePrompts()) return; prompt = 0; }
                else if (prompt == -4) { if (!ShowSoundTest(soundCapability)) return; prompt = 0; }

                const std::string lastMode = _mode;
                const std::string mphKey = Paths::MphKey;
                const std::string fhKey = Paths::FhKey;
                std::string roomString = roomKey == "AD1 TRANSFER LOCK BT" ? "Transfer Lock (Expanded)" : room;
                roomString += " [" + roomKey + "] - " + std::to_string(roomId);
                const std::string modeString = _mode == "auto-select" ? "auto-select (Adventure or Battle)" : _mode;
                const std::string languageString = mphKey == AMHK0 ? "Korean" : LanguageString(_language);
                const std::string movieString = _movieId == -1 ? "none" : Metadata::MovieDisplayInfo[static_cast<std::size_t>(_movieId)];
                ClearConsole(); WriteLine(ProgramVersionBanner()); WriteLine();
                WriteLine("Choose an option using up/down or with the key indicated.");
                WriteLine("Press Space to specify, Backspace to clear, or left/right to advance the option.");
                WriteLine("When finished, press Enter or use the last option to launch. Press Escape to exit.");
                WriteLine();
                WriteLine(X(s++) + " (R) Room: " + roomString);
                WriteLine(X(s++) + " (G) Game mode: " + modeString);
                WriteLine(X(s++) + " (1) Player 1: " + PrintPlayer(0));
                WriteLine(X(s++) + " (2) Player 2: " + PrintPlayer(1));
                WriteLine(X(s++) + " (3) Player 3: " + PrintPlayer(2));
                WriteLine(X(s++) + " (4) Player 4: " + PrintPlayer(3));
                WriteLine(X(s++) + " (M) Models: " + PrintModels());
                WriteLine(X(s++) + " (H) MPH Version: " + mphKey + " (" + mphInfo.at(mphKey) + ")");
                WriteLine(X(s++) + " (F) FH Version: " + fhKey + " (" + fhInfo.at(fhKey) + ")");
                WriteLine(X(s++) + " (I) Language: " + languageString);
                WriteLine(X(s++) + " (V) SFX Volume: " + Fixed2(_sfxVolume));
                WriteLine(X(s++) + " (B) Music Volume: " + Fixed2(_musicVolume));
                WriteLine(X(s++) + " (P) Movie Player: " + movieString);
                WriteLine(X(s++) + " (T) Sound Test...");
                WriteLine(X(s++) + " (A) Adventure Mode Settings...");
                WriteLine(X(s++) + " (S) Match Settings...");
                WriteLine(X(s++) + " (C) Features...");
                WriteLine(X(s++) + " (X) Reset All");
                WriteLine(X(s++) + " (L) Launch");
                --s;

                if (prompt == 0)
                {
                    PrintSoundInfo(soundCapability);
                    const ConsoleKeyInfo keyInfo = ReadKey();
                    if (keyInfo.Key == ConsoleKey::Escape) return;
                    if (keyInfo.Key == ConsoleKey::Enter || keyInfo.Key == ConsoleKey::L
                        || keyInfo.Key == ConsoleKey::Spacebar && selection == s)
                    {
                        ClearConsole(); WriteLine(ProgramVersionBanner()); WriteLine(); WriteLine("Loading...");
                        CommitSettings();
                        break;
                    }
                    if (keyInfo.Key == ConsoleKey::Spacebar)
                    {
                        if (selection == s - 5) { prompt = -4; continue; }
                        if (selection == s - 4) { prompt = -2; continue; }
                        if (selection == s - 3) { prompt = -1; continue; }
                        if (selection == s - 2) { prompt = -3; continue; }
                        if (selection == s - 1)
                        {
                            ResetFeatures();
                            MenuSettings defaults;
                            LoadSettings(defaults);
                            ::UpdateSettings();
                            continue;
                        }
                        prompt = selection + 1;
                    }
                    else if (keyInfo.Key == ConsoleKey::R) selection = 0;
                    else if (keyInfo.Key == ConsoleKey::G) selection = 1;
                    else if (keyInfo.Key == ConsoleKey::D1 || keyInfo.Key == ConsoleKey::NumPad1) selection = 2;
                    else if (keyInfo.Key == ConsoleKey::D2 || keyInfo.Key == ConsoleKey::NumPad2) selection = 3;
                    else if (keyInfo.Key == ConsoleKey::D3 || keyInfo.Key == ConsoleKey::NumPad3) selection = 4;
                    else if (keyInfo.Key == ConsoleKey::D4 || keyInfo.Key == ConsoleKey::NumPad4) selection = 5;
                    else if (keyInfo.Key == ConsoleKey::M) { selection = 6; prompt = selection + 1; }
                    else if (keyInfo.Key == ConsoleKey::H) selection = 7;
                    else if (keyInfo.Key == ConsoleKey::F) selection = 8;
                    else if (keyInfo.Key == ConsoleKey::I) selection = 9;
                    else if (keyInfo.Key == ConsoleKey::V) selection = 10;
                    else if (keyInfo.Key == ConsoleKey::B) selection = 11;
                    else if (keyInfo.Key == ConsoleKey::P) selection = 12;
                    else if (keyInfo.Key == ConsoleKey::T) { selection = s - 5; prompt = -4; continue; }
                    else if (keyInfo.Key == ConsoleKey::A) { selection = s - 4; prompt = -2; continue; }
                    else if (keyInfo.Key == ConsoleKey::S) { selection = s - 3; prompt = -1; continue; }
                    else if (keyInfo.Key == ConsoleKey::C) { selection = s - 2; prompt = -3; continue; }
                    else if (keyInfo.Key == ConsoleKey::X) selection = s - 1;
                    else if (keyInfo.Key == ConsoleKey::UpArrow || keyInfo.Key == ConsoleKey::W) { if (--selection < 0) selection = s; }
                    else if (keyInfo.Key == ConsoleKey::DownArrow || keyInfo.Key == ConsoleKey::S) { if (++selection > s) selection = 0; }
                    else if (keyInfo.Key == ConsoleKey::Backspace || keyInfo.Key == ConsoleKey::Delete)
                    {
                        if (selection == 0) { roomId = -1; room = "none"; roomKey = "none"; }
                        else if (selection == 1) _mode = "auto-select";
                        else if (selection >= 2 && selection <= 5)
                        {
                            const std::int32_t index = selection - 2;
                            const std::string team = index == 0 || index == 2 ? "orange" : "green";
                            players.at(static_cast<std::size_t>(index)) = PlayerSetting{"none", team, "0"};
                        }
                        else if (selection == 6) models.clear();
                        else if (selection == 7) Paths::ChooseMphPath();
                        else if (selection == 8) Paths::ChooseFhPath();
                        else if (selection == 9) SetDefaultLanguage();
                        else if (selection == 10) { _sfxVolume = Decimal::ParseInvariant("0.35"); Sound::Sfx::Volume = _sfxVolume.ToSingle(); }
                        else if (selection == 11) { _musicVolume = Decimal::ParseInvariant("0.50"); Music::UserVolume(_musicVolume.ToSingle()); }
                        else if (selection == 12) _movieId = -1;
                    }
                    else if (keyInfo.Key == ConsoleKey::Add || keyInfo.Key == ConsoleKey::OemPlus || keyInfo.Key == ConsoleKey::RightArrow)
                    {
                        if (selection == 0)
                        {
                            ++roomId;
                            if (roomId > 137) { roomId = -1; room = "none"; roomKey = "none"; }
                            else
                            {
                                const auto meta = Metadata::GetRoomById(roomId);
                                if (meta)
                                {
                                    room = (*meta).InGameName.value_or((*meta).Name);
                                    roomKey = (*meta).Name; fhRoom = (*meta).FirstHunt;
                                }
                                else { roomId = -1; room = "none"; roomKey = "none"; }
                            }
                        }
                        else if (selection == 1)
                        {
                            ++modeId; if (modeId >= static_cast<std::int32_t>(modes.size())) modeId = 0;
                            _mode = modes.at(static_cast<std::size_t>(modeId));
                        }
                        else if (selection >= 2 && selection <= 5)
                        {
                            const std::int32_t index = selection - 2;
                            std::int32_t id = playerIds.at(static_cast<std::size_t>(index));
                            if (++id > 7) id = -1;
                            playerIds.at(static_cast<std::size_t>(index)) = id;
                            auto& player = players.at(static_cast<std::size_t>(index));
                            player.HunterName = id == -1 ? "none" : hunters.at(static_cast<std::size_t>(id));
                        }
                        else if (selection == 6)
                        {
                            if (models.empty()) models.push_back(ModelSetting{"Crate01", 0, false, static_cast<MetaDir>(0)});
                            else
                            {
                                auto keys = ModelMetadataKeys();
                                const std::string model = models[0].Name;
                                auto it = std::find(keys.begin(), keys.end(), model);
                                std::int32_t index = static_cast<std::int32_t>(std::distance(keys.begin(), it));
                                ++index; if (index >= static_cast<std::int32_t>(keys.size())) index = 0;
                                const auto meta = &Metadata::ModelMetadata.at(keys.at(static_cast<std::size_t>(index)));
                                models[0] = ModelSetting{(*meta).Name, models[0].Recolor, false, static_cast<MetaDir>(0)};
                            }
                        }
                        else if (selection == 7)
                        {
                            std::string current = Paths::MphKey;
                            std::string next = Paths::MphKey;
                            do
                            {
                                auto it = std::find(mphVersions.begin(), mphVersions.end(), next);
                                std::int32_t index = static_cast<std::int32_t>(std::distance(mphVersions.begin(), it)) + 1;
                                if (index >= static_cast<std::int32_t>(mphVersions.size())) index = 0;
                                next = mphVersions.at(static_cast<std::size_t>(index));
                                if (!Paths::AllPaths().at(next).empty()) current = next;
                            } while (current != next);
                            Paths::MphKey = current; SetDefaultLanguage();
                        }
                        else if (selection == 8)
                        {
                            std::string current = Paths::FhKey;
                            std::string next = Paths::FhKey;
                            do
                            {
                                auto it = std::find(fhVersions.begin(), fhVersions.end(), next);
                                std::int32_t index = static_cast<std::int32_t>(std::distance(fhVersions.begin(), it)) + 1;
                                if (index >= static_cast<std::int32_t>(fhVersions.size())) index = 0;
                                next = fhVersions.at(static_cast<std::size_t>(index));
                                if (!Paths::AllPaths().at(next).empty()) current = next;
                            } while (current != next);
                            Paths::FhKey = current; SetDefaultLanguage();
                        }
                        else if (selection == 9)
                        {
                            std::int32_t language = static_cast<std::int32_t>(_language) + 1;
                            if (language > 5) language = 0; _language = static_cast<Language>(language);
                        }
                        else if (selection == 10)
                        {
                            _sfxVolume = std::min(_sfxVolume + Decimal::ParseInvariant("0.05"), Decimal::ParseInvariant("1.5"));
                            Sound::Sfx::Volume = _sfxVolume.ToSingle();
                        }
                        else if (selection == 11)
                        {
                            _musicVolume = std::min(_musicVolume + Decimal::ParseInvariant("0.05"), Decimal::ParseInvariant("1.0"));
                            Music::UserVolume(_musicVolume.ToSingle());
                        }
                        else if (selection == 12)
                        {
                            ++_movieId; if (_movieId == 13 || _movieId == 34) ++_movieId; if (_movieId > 35) _movieId = -1;
                        }
                    }
                    else if (keyInfo.Key == ConsoleKey::Subtract || keyInfo.Key == ConsoleKey::OemMinus || keyInfo.Key == ConsoleKey::LeftArrow)
                    {
                        if (selection == 0)
                        {
                            --roomId; if (roomId < -1) roomId = 137;
                            if (roomId == -1) { room = "none"; roomKey = "none"; }
                            else
                            {
                                const auto meta = Metadata::GetRoomById(roomId);
                                if (meta)
                                {
                                    room = (*meta).InGameName.value_or((*meta).Name);
                                    roomKey = (*meta).Name; fhRoom = (*meta).FirstHunt;
                                }
                                else { roomId = -1; room = "none"; roomKey = "none"; }
                            }
                        }
                        else if (selection == 1)
                        {
                            --modeId; if (modeId < 0) modeId = static_cast<std::int32_t>(modes.size()) - 1;
                            _mode = modes.at(static_cast<std::size_t>(modeId));
                        }
                        else if (selection >= 2 && selection <= 5)
                        {
                            const std::int32_t index = selection - 2;
                            std::int32_t id = playerIds.at(static_cast<std::size_t>(index));
                            if (--id < -1) id = 7;
                            playerIds.at(static_cast<std::size_t>(index)) = id;
                            auto& player = players.at(static_cast<std::size_t>(index));
                            player.HunterName = id == -1 ? "none" : hunters.at(static_cast<std::size_t>(id));
                        }
                        else if (selection == 6)
                        {
                            if (models.empty()) models.push_back(ModelSetting{"Crate01", 0, false, static_cast<MetaDir>(0)});
                            else
                            {
                                auto keys = ModelMetadataKeys();
                                const std::string model = models[0].Name;
                                auto it = std::find(keys.begin(), keys.end(), model);
                                std::int32_t index = static_cast<std::int32_t>(std::distance(keys.begin(), it)) - 1;
                                if (index < 0) index = static_cast<std::int32_t>(keys.size()) - 1;
                                const auto meta = &Metadata::ModelMetadata.at(keys.at(static_cast<std::size_t>(index)));
                                models[0] = ModelSetting{(*meta).Name, models[0].Recolor, false, static_cast<MetaDir>(0)};
                            }
                        }
                        else if (selection == 7)
                        {
                            std::string current = Paths::MphKey;
                            std::string next = Paths::MphKey;
                            do
                            {
                                auto it = std::find(mphVersions.begin(), mphVersions.end(), next);
                                std::int32_t index = static_cast<std::int32_t>(std::distance(mphVersions.begin(), it)) - 1;
                                if (index < 0) index = static_cast<std::int32_t>(mphVersions.size()) - 1;
                                next = mphVersions.at(static_cast<std::size_t>(index));
                                if (!Paths::AllPaths().at(next).empty()) current = next;
                            } while (current != next);
                            Paths::MphKey = current; SetDefaultLanguage();
                        }
                        else if (selection == 8)
                        {
                            std::string current = Paths::FhKey;
                            std::string next = Paths::FhKey;
                            do
                            {
                                auto it = std::find(fhVersions.begin(), fhVersions.end(), next);
                                std::int32_t index = static_cast<std::int32_t>(std::distance(fhVersions.begin(), it)) - 1;
                                if (index < 0) index = static_cast<std::int32_t>(fhVersions.size()) - 1;
                                next = fhVersions.at(static_cast<std::size_t>(index));
                                if (!Paths::AllPaths().at(next).empty()) current = next;
                            } while (current != next);
                            Paths::FhKey = current; SetDefaultLanguage();
                        }
                        else if (selection == 9)
                        {
                            std::int32_t language = static_cast<std::int32_t>(_language) - 1;
                            if (language < 0) language = 5; _language = static_cast<Language>(language);
                        }
                        else if (selection == 10)
                        {
                            _sfxVolume = std::max(_sfxVolume - Decimal::ParseInvariant("0.05"), Decimal(0));
                            Sound::Sfx::Volume = _sfxVolume.ToSingle();
                        }
                        else if (selection == 11)
                        {
                            _musicVolume = std::max(_musicVolume - Decimal::ParseInvariant("0.05"), Decimal(0));
                            Music::UserVolume(_musicVolume.ToSingle());
                        }
                        else if (selection == 12)
                        {
                            --_movieId; if (_movieId == 13 || _movieId == 34) --_movieId; if (_movieId < -1) _movieId = 35;
                        }
                    }
                }
                else
                {
                    WriteLine();
                    if (prompt == 1) { WriteLine("Enter room ID, internal name, or in-game name."); WriteLine("Examples: 95, MP3 PROVING GROUND, Combat Hall"); ReadRoom(ReadLine()); }
                    else if (prompt == 2) { WriteLine("Enter game mode."); WriteLine("Examples: Adventure, Battle, Survival Teams"); ReadMode(ReadLine()); }
                    else if (prompt >= 3 && prompt <= 6)
                    {
                        if (_teams) { WriteLine("Enter hunter and (optionally) team."); WriteLine("Examples: Samus, Trace 0, Sylux 1, Guardian"); }
                        else { WriteLine("Enter hunter and (optionally) recolor."); WriteLine("Examples: Samus, Trace 2, Sylux 5, Guardian"); }
                        ReadPlayer(ReadLine(), prompt - 3);
                    }
                    else if (prompt == 7) { WriteLine("Enter comma-separated list of models and (optionally) recolors."); WriteLine("Examples: Crate01, blastcap, LavaDemon 1, KandenGun 4"); ReadModels(ReadLine()); }
                    else if (prompt == 8) { WriteLine("Enter MPH version."); WriteLine("Examples: AMHE0, AMHP1, A76E0"); ReadMphVersion(ReadLine()); }
                    else if (prompt == 9) { WriteLine("Enter FH version."); WriteLine("Examples: AMFE0, AMFP0"); ReadFhVersion(ReadLine()); }
                    prompt = 0;
                }
                if (_mode != lastMode) ::UpdateSettings();
            }

            _applySettings = true;
            RendererOwner renderer;
            if (_movieId != -1) renderer.Get()->QueueMovie(_movieId);
            else if (room != "none")
            {
                if (!fhRoom)
                {
                    for (const auto& player : players)
                    {
                        if (player.HunterName == "none") continue;
                        std::int32_t teamId = -1;
                        if (_teams) teamId = player.Team == "orange" ? 0 : 1;
                        Hunter hunter{};
                        if (!TryParseHunter(player.HunterName, hunter)) throw std::invalid_argument("Requested value was not found.");
                        renderer.Get()->AddPlayer(hunter, std::stoi(player.Recolor), teamId);
                    }
                }
                Scene::Language(Paths::MphKey == "AMHK0" ? Language::Japanese : _language);
                GameMode gameMode = GameMode::None;
                if (_mode == "Adventure") gameMode = GameMode::SinglePlayer;
                else if (_mode != "auto-select")
                {
                    std::string modeName = _mode; modeName = StringReplace(std::move(modeName), " ", "");
                    gameMode = ParseGameMode(modeName);
                }
                renderer.Get()->AddRoom(roomKey, gameMode);
            }
            for (const auto& model : models)
            {
                renderer.Get()->AddModel(model.Name, model.Recolor, model.FirstHunt, model.Dir);
            }
            renderer.Get()->Run();
        }
    }

    void Menu::ApplyMultiplayerSettings()
    {
        if (_applySettings)
        {
            GameState::Teams(_teams);
            GameState::PointGoal(_pointGoal.ToInt32());
            GameState::TimeGoal(_timeGoal.ToSingle());
            GameState::MatchTime(_timeLimit.ToSingle());
            GameState::OctolithReset(_octolithReset);
            GameState::RadarPlayers(_radarPlayers);
            GameState::DamageLevel(_damageLevel);
            GameState::FriendlyFire(_friendlyFire);
            GameState::AffinityWeapons(_affinityWeapons);
        }
    }

    void Menu::ApplyAdventureSettings()
    {
        if (_applySettings && SaveSlot == 0)
        {
            const std::int32_t areas = (_planets[0] == 0 ? 0 : 0xC)
                | (_planets[1] == 0 ? 0 : 0x3)
                | (_planets[2] == 0 ? 0 : 0x30)
                | (_planets[3] == 0 ? 0 : 0xC0)
                | (_planets[4] == 0 ? 0 : 0x100);
            GameState::StorySave->Areas = static_cast<std::uint16_t>(areas);
            BossFlags bossFlags = static_cast<BossFlags>(0);
            const std::array<BossFlags, 3> ca1{static_cast<BossFlags>(0), BossFlags::Unit2B1Kill, BossFlags::Unit2B1Done};
            const std::array<BossFlags, 3> ca2{static_cast<BossFlags>(0), BossFlags::Unit2B2Kill, BossFlags::Unit2B2Done};
            const std::array<BossFlags, 3> alinos1{static_cast<BossFlags>(0), BossFlags::Unit1B1Kill, BossFlags::Unit1B1Done};
            const std::array<BossFlags, 3> alinos2{static_cast<BossFlags>(0), BossFlags::Unit1B2Kill, BossFlags::Unit1B2Done};
            const std::array<BossFlags, 3> vdo1{static_cast<BossFlags>(0), BossFlags::Unit3B1Kill, BossFlags::Unit3B1Done};
            const std::array<BossFlags, 3> vdo2{static_cast<BossFlags>(0), BossFlags::Unit3B2Kill, BossFlags::Unit3B2Done};
            const std::array<BossFlags, 3> arcterra1{static_cast<BossFlags>(0), BossFlags::Unit4B1Kill, BossFlags::Unit4B1Done};
            const std::array<BossFlags, 3> arcterra2{static_cast<BossFlags>(0), BossFlags::Unit4B2Kill, BossFlags::Unit4B2Done};
            bossFlags = bossFlags | ca1.at(static_cast<std::size_t>(_ca1State));
            bossFlags = bossFlags | ca2.at(static_cast<std::size_t>(_ca2State));
            bossFlags = bossFlags | alinos1.at(static_cast<std::size_t>(_alinos1State));
            bossFlags = bossFlags | alinos2.at(static_cast<std::size_t>(_alinos2State));
            bossFlags = bossFlags | vdo1.at(static_cast<std::size_t>(_vdo1State));
            bossFlags = bossFlags | vdo2.at(static_cast<std::size_t>(_vdo2State));
            bossFlags = bossFlags | arcterra1.at(static_cast<std::size_t>(_arcterra1State));
            bossFlags = bossFlags | arcterra2.at(static_cast<std::size_t>(_arcterra2State));
            GameState::StorySave->BossFlags = bossFlags;
            GameState::StorySave->CheckpointEntityId = _checkpointId;
            GameState::StorySave->HealthMax = _healthMax;
            GameState::StorySave->Health = _healthMax;
            (*GameState::StorySave->AmmoMax)[1] = _missileMax;
            (*GameState::StorySave->Ammo)[1] = _missileMax;
            (*GameState::StorySave->AmmoMax)[0] = _uaMax;
            (*GameState::StorySave->Ammo)[0] = _uaMax;
            std::int32_t weapons = 0;
            for (std::size_t i = 0; i < _weapons.size(); ++i) if (_weapons[i] != 0) weapons |= 1 << static_cast<std::int32_t>(i);
            GameState::StorySave->Weapons = static_cast<std::uint16_t>(weapons);
            std::int32_t octoliths = 0;
            constexpr std::array<std::int32_t, 8> bits{2, 3, 0, 1, 4, 5, 6, 7};
            for (std::size_t i = 0; i < _octoliths.size(); ++i) if (_octoliths[i] != 0) octoliths |= 1 << bits[i];
            GameState::StorySave->CurrentOctoliths = GameState::StorySave->FoundOctoliths = static_cast<std::uint16_t>(octoliths);
        }
    }
}

namespace MphRead
{
    bool Menu::ShowSettingsPrompts()
    {
        std::int32_t prompt = 0;
        std::int32_t selection = 0;
        const std::array<std::string, 3> damageLevels{"Low", "Medium", "High"};
        auto X = [&](std::int32_t index) { return Mark(selection == index); };
        while (true)
        {
            std::int32_t s = 0;
            std::string modeString = _mode == "auto-select" ? "Battle" : _mode;
            modeString = StringReplace(std::move(modeString), " Teams", "");
            modeString += " Mode Settings";
            const std::string goalString = StartsWith(_mode, "Defender") || _mode == "Prime Hunter"
                ? ::FormatTime(_timeGoal) : _pointGoal.ToString();
            const std::string timeString = ::FormatTime(_timeLimit);
            std::string resetString = "N/A";
            if (_mode == "Capture" || StartsWith(_mode, "Bounty")) resetString = OnOff(_octolithReset);
            const std::string weaponsString = _affinityWeapons ? "Affinity Weapons" : "Default Weapons";

            ClearConsole();
            WriteLine(ProgramVersionBanner()); WriteLine();
            WriteLine("Choose a setting using up/down or with the key indicated.");
            WriteLine("Press Space to specify, Backspace to clear, or left/right to advance the setting.");
            WriteLine("When finished, press Enter or use the last option to return. Press Escape to exit.");
            WriteLine(); WriteLine(modeString); WriteLine();
            WriteLine(X(s++) + " (P) " + _goalType + ": " + goalString);
            WriteLine(X(s++) + " (L) Time Limit: " + timeString);
            WriteLine(X(s++) + " (A) Auto Reset: " + resetString);
            WriteLine(X(s++) + " (T) Team Play: " + OnOff(_teams));
            WriteLine(X(s++) + " (S) Show Hunters On Radar: " + OnOff(_radarPlayers));
            WriteLine(X(s++) + " (D) Damage Level: " + damageLevels.at(static_cast<std::size_t>(_damageLevel)));
            WriteLine(X(s++) + " (F) Friendly Fire: " + OnOff(_friendlyFire));
            WriteLine(X(s++) + " (W) Available Weapons: " + weaponsString);
            WriteLine(X(s++) + " (X) Reset Match Settings");
            WriteLine(X(s++) + " (B) Go Back");
            --s;

            if (prompt == 0)
            {
                ConsoleKeyInfo keyInfo = ReadKey();
                if (keyInfo.Key == ConsoleKey::Escape) return false;
                if (keyInfo.Key == ConsoleKey::Enter || keyInfo.Key == ConsoleKey::B
                    || keyInfo.Key == ConsoleKey::Spacebar && selection == s) break;
                if (keyInfo.Key == ConsoleKey::Spacebar)
                {
                    if (selection == s - 1)
                    {
                        _radarPlayers = false;
                        _damageLevel = 1;
                        _friendlyFire = false;
                        _affinityWeapons = false;
                        ::UpdateSettings();
                        continue;
                    }
                    prompt = selection + 1;
                }
                else if (keyInfo.Key == ConsoleKey::P) selection = 0;
                else if (keyInfo.Key == ConsoleKey::L) selection = 1;
                else if (keyInfo.Key == ConsoleKey::A) selection = 2;
                else if (keyInfo.Key == ConsoleKey::T) selection = 3;
                else if (keyInfo.Key == ConsoleKey::S) selection = 4;
                else if (keyInfo.Key == ConsoleKey::D) selection = 5;
                else if (keyInfo.Key == ConsoleKey::F) selection = 6;
                else if (keyInfo.Key == ConsoleKey::W) selection = 7;
                else if (keyInfo.Key == ConsoleKey::X) selection = 8;
                else if (keyInfo.Key == ConsoleKey::UpArrow || keyInfo.Key == ConsoleKey::W)
                {
                    --selection;
                    if (selection < 0) selection = s;
                }
                else if (keyInfo.Key == ConsoleKey::DownArrow || keyInfo.Key == ConsoleKey::S)
                {
                    ++selection;
                    if (selection > s) selection = 0;
                }
                else if (keyInfo.Key == ConsoleKey::Backspace || keyInfo.Key == ConsoleKey::Delete)
                {
                    if (selection == 0) ::ResetGoal();
                    else if (selection == 1) ::ResetTimeLimit();
                    else if (selection == 2) _octolithReset = true;
                    else if (selection == 3) _teams = _mode == "Capture";
                    else if (selection == 4) _radarPlayers = false;
                    else if (selection == 5) _damageLevel = 1;
                    else if (selection == 6) _friendlyFire = false;
                    else if (selection == 7) _affinityWeapons = false;
                }
                else if (keyInfo.Key == ConsoleKey::Add || keyInfo.Key == ConsoleKey::OemPlus
                    || keyInfo.Key == ConsoleKey::RightArrow || keyInfo.Key == ConsoleKey::Subtract
                    || keyInfo.Key == ConsoleKey::OemMinus || keyInfo.Key == ConsoleKey::LeftArrow)
                {
                    const std::int32_t direction = keyInfo.Key == ConsoleKey::Add || keyInfo.Key == ConsoleKey::OemPlus
                        || keyInfo.Key == ConsoleKey::RightArrow ? 1 : -1;
                    if (selection == 0)
                    {
                        if (_mode == "auto-select" || StartsWith(_mode, "Battle")) _pointGoal = Advance(_pointGoal, _battlePoints, direction);
                        else if (StartsWith(_mode, "Survival")) _pointGoal = Advance(_pointGoal, _extraLives, direction);
                        else if (_mode == "Capture" || StartsWith(_mode, "Bounty")) _pointGoal = Advance(_pointGoal, _octolithPoints, direction);
                        else if (StartsWith(_mode, "Nodes")) _pointGoal = Advance(_pointGoal, _nodePoints, direction);
                        else if (StartsWith(_mode, "Defender") || _mode == "Prime Hunter") _timeGoal = Advance(_timeGoal, _timeGoals, direction);
                    }
                    else if (selection == 1) _timeLimit = Advance(_timeLimit, _timeLimits, direction);
                    else if (selection == 2) _octolithReset = !_octolithReset;
                    else if (selection == 3)
                    {
                        if (_mode == "Capture") _teams = true;
                        else if (_mode == "Prime Hunter") _teams = false;
                        else _teams = !_teams;
                    }
                    else if (selection == 4) _radarPlayers = !_radarPlayers;
                    else if (selection == 5)
                    {
                        _damageLevel += direction;
                        if (_damageLevel >= static_cast<std::int32_t>(damageLevels.size())) _damageLevel = 0;
                        else if (_damageLevel < 0) _damageLevel = static_cast<std::int32_t>(damageLevels.size()) - 1;
                    }
                    else if (selection == 6) _friendlyFire = !_friendlyFire;
                    else if (selection == 7) _affinityWeapons = !_affinityWeapons;
                    if (_teams && _mode != "Capture" && !EndsWith(_mode, "Teams"))
                    {
                        if (_mode == "auto-select") _mode = "Battle";
                        _mode += " Teams";
                    }
                    else if (!_teams && EndsWith(_mode, "Teams")) _mode = StringReplace(std::move(_mode), " Teams", "");
                }
            }
            else
            {
                if (prompt == 1)
                {
                    WriteLine("Enter " + ToLower(_goalType) + ".");
                    if (_goalType == "Time Goal") WriteLine("Examples: 7, 2:30, 0:45");
                    else WriteLine("Examples: 5, 66, 100");
                    ::ReadTimeGoal(ReadLine());
                }
                else if (prompt == 2)
                {
                    WriteLine("Enter time limit.");
                    WriteLine("Examples: 7, 2:30, 0:45");
                    ::ReadTimeLimit(ReadLine());
                }
                prompt = 0;
            }
        }
        return true;
    }

    void Menu::ResetFeatures()
    {
        Features::NoRepeatEncounters(false);
        Features::AllowInvalidTeams(true);
        Features::TopScreenTargetInfo(true);
        Features::HelmetOpacity(1.0F);
        Features::VisorOpacity(0.5F);
        Features::HudOpacity(1.0F);
        Features::ReticleOpacity(1.0F);
        Features::HudSway(true);
        Features::TargetInfoSway(false);
        Features::DelayedIdleSway(true);
        Features::NoIdleSway(false);
        Features::NoMapCentering(false);
        Features::MaxRoomDetail(false);
        Features::MaxPlayerDetail(true);
        Features::LogSpatialAudio(false);
        Features::HalfSecondAlarm(false);
        Features::FullBoostCharge(false);
        Features::BoostOpensDoors(false);
        Features::AlternateHunters1P(true);
        Cheats::FreeWeaponSelect(false);
        Cheats::UnlimitedJumps(false);
        Cheats::NoRandomEncounters(false);
        Cheats::UnlockAllDoors(false);
        Cheats::ContinueFromCurrentRoom(false);
        Cheats::SkipPlanetIntros(false);
        Cheats::StartWithAllUpgrades(false);
        Cheats::StartWithAllOctoliths(false);
        Cheats::WalkThroughWalls(false);
        Cheats::AlwaysFightGorea2(false);
        Cheats::QuadrupleDamage(false);
        Bugfixes::SmoothCamSeqHandoff(false);
        Bugfixes::BetterCamSeqNodeRef(true);
        Bugfixes::NoStrayRespawnText(false);
        Bugfixes::CorrectBountySfx(true);
        Bugfixes::NoDoubleEnemyDeath(true);
        Bugfixes::NoSlenchRollTimerUnderflow(true);
    }

    bool Menu::ShowFeaturePrompts()
    {
        std::int32_t screen = 0;
        std::int32_t selection = 0;
        auto X = [&](std::int32_t index) { return Mark(selection == index); };
        auto PrintOpacity = [](float value)
        {
            if (value == 0.0F) return std::string("zero");
            if (value >= 1.0F) return std::string("full");
            return std::string("partial");
        };
        while (true)
        {
            std::int32_t s = 0;
            ClearConsole(); WriteLine(ProgramVersionBanner()); WriteLine();
            WriteLine("Choose a setting using up/down or with the key indicated.");
            WriteLine("Press Space to specify, Backspace to clear, or left/right to advance the setting.");
            WriteLine("When finished, press Enter or use the last option to return. Press Escape to exit.");
            WriteLine();
            if (screen == 0)
            {
                WriteLine("Features, Cheats, and Bugfixes"); WriteLine();
                WriteLine(X(s++) + " (F) Features...");
                WriteLine(X(s++) + " (C) Cheats...");
                WriteLine(X(s++) + " (G) Bugfixes...");
                WriteLine(X(s++) + " (X) Reset Features, Cheats, and Bugfixes");
            }
            else if (screen == 1)
            {
                WriteLine("Features"); WriteLine();
                WriteLine(X(s++) + " (E) No Repeat Encounters: " + OnOff(Features::NoRepeatEncounters()));
                WriteLine(X(s++) + " (T) Allow Invalid Teams: " + OnOff(Features::AllowInvalidTeams()));
                WriteLine(X(s++) + " (I) Target Info On Top Screen: " + OnOff(Features::TopScreenTargetInfo()));
                WriteLine(X(s++) + " (H) Helmet Opacity: " + PrintOpacity(Features::HelmetOpacity()));
                WriteLine(X(s++) + " (V) Visor Opacity: " + PrintOpacity(Features::VisorOpacity()));
                WriteLine(X(s++) + " (D) HUD Opacity: " + PrintOpacity(Features::HudOpacity()));
                WriteLine(X(s++) + " (C) Reticle Opacity: " + PrintOpacity(Features::ReticleOpacity()));
                WriteLine(X(s++) + " (S) HUD Sway: " + OnOff(Features::HudSway()));
                WriteLine(X(s++) + " (F) Target Info Sway: " + OnOff(Features::TargetInfoSway()));
                WriteLine(X(s++) + " (W) Delayed Idle Sway: " + OnOff(Features::DelayedIdleSway()));
                WriteLine(X(s++) + " (N) No Idle Sway: " + OnOff(Features::NoIdleSway()));
                WriteLine(X(s++) + " (M) No Map Centering: " + OnOff(Features::NoMapCentering()));
                WriteLine(X(s++) + " (R) Maximum Room Detail: " + OnOff(Features::MaxRoomDetail()));
                WriteLine(X(s++) + " (P) Maximum Player Detail: " + OnOff(Features::MaxPlayerDetail()));
                WriteLine(X(s++) + " (L) Logarithmic Spatial Audio: " + OnOff(Features::LogSpatialAudio()));
                WriteLine(X(s++) + " (A) Consistent Alarm Interval: " + OnOff(Features::HalfSecondAlarm()));
                WriteLine(X(s++) + " (G) Full Boost Charge: " + OnOff(Features::FullBoostCharge()));
                WriteLine(X(s++) + " (B) Boost Opens Doors: " + OnOff(Features::BoostOpensDoors()));
                WriteLine(X(s++) + " (1) Update Adventure Mode For Other Hunters: " + OnOff(Features::AlternateHunters1P()));
            }
            else if (screen == 2)
            {
                WriteLine("Cheats"); WriteLine();
                WriteLine(X(s++) + " (W) Free Weapon Selection: " + OnOff(Cheats::FreeWeaponSelect()));
                WriteLine(X(s++) + " (J) Unlimited Jumps: " + OnOff(Cheats::UnlimitedJumps()));
                WriteLine(X(s++) + " (E) No Random Encounters: " + OnOff(Cheats::NoRandomEncounters()));
                WriteLine(X(s++) + " (D) All Doors Unlocked: " + OnOff(Cheats::UnlockAllDoors()));
                WriteLine(X(s++) + " (R) Retry From Current Room: " + OnOff(Cheats::ContinueFromCurrentRoom()));
                WriteLine(X(s++) + " (I) Skip Planet Intros: " + OnOff(Cheats::SkipPlanetIntros()));
                WriteLine(X(s++) + " (U) Start With All Upgrades: " + OnOff(Cheats::StartWithAllUpgrades()));
                WriteLine(X(s++) + " (O) Start With All Octoliths: " + OnOff(Cheats::StartWithAllOctoliths()));
                WriteLine(X(s++) + " (G) Walk Through Walls: " + OnOff(Cheats::WalkThroughWalls()));
                WriteLine(X(s++) + " (2) Always Fight Gorea 2: " + OnOff(Cheats::AlwaysFightGorea2()));
                WriteLine(X(s++) + " (Q) Quadruple Damage: " + OnOff(Cheats::QuadrupleDamage()));
            }
            else if (screen == 3)
            {
                WriteLine("Bugfixes"); WriteLine();
                WriteLine(X(s++) + " (C) Smooth Camera Sequence Handoff: " + OnOff(Bugfixes::SmoothCamSeqHandoff()));
                WriteLine(X(s++) + " (N) Better Camera Sequence Node Refs: " + OnOff(Bugfixes::BetterCamSeqNodeRef()));
                WriteLine(X(s++) + " (R) No Stray Respawn Text: " + OnOff(Bugfixes::NoStrayRespawnText()));
                WriteLine(X(s++) + " (S) Correct Bounty SFX: " + OnOff(Bugfixes::CorrectBountySfx()));
                WriteLine(X(s++) + " (E) Fix Double Enemy Death: " + OnOff(Bugfixes::NoDoubleEnemyDeath()));
                WriteLine(X(s++) + " (T) Fix Slench Roll Timer Underflow: " + OnOff(Bugfixes::NoSlenchRollTimerUnderflow()));
            }
            WriteLine(X(s++) + " (B) Go Back");
            --s;
            ConsoleKeyInfo keyInfo = ReadKey();
            if (keyInfo.Key == ConsoleKey::UpArrow) { if (--selection < 0) selection = s; }
            else if (keyInfo.Key == ConsoleKey::DownArrow) { if (++selection > s) selection = 0; }
            else if (keyInfo.Key == ConsoleKey::Escape) return false;
            else if (screen == 0)
            {
                if (keyInfo.Key == ConsoleKey::Enter || keyInfo.Key == ConsoleKey::B
                    || keyInfo.Key == ConsoleKey::Spacebar && selection == s) break;
                if (keyInfo.Key == ConsoleKey::F || keyInfo.Key == ConsoleKey::Spacebar && selection == 0) { screen = 1; selection = 0; }
                else if (keyInfo.Key == ConsoleKey::C || keyInfo.Key == ConsoleKey::Spacebar && selection == 1) { screen = 2; selection = 0; }
                else if (keyInfo.Key == ConsoleKey::G || keyInfo.Key == ConsoleKey::Spacebar && selection == 2) { screen = 3; selection = 0; }
                else if (keyInfo.Key == ConsoleKey::X) selection = 3;
                else if (keyInfo.Key == ConsoleKey::Spacebar && selection == 3) ResetFeatures();
            }
            else if (screen == 1)
            {
                if (keyInfo.Key == ConsoleKey::Enter || keyInfo.Key == ConsoleKey::B
                    || keyInfo.Key == ConsoleKey::Spacebar && selection == s) { screen = 0; selection = 0; }
                else if (keyInfo.Key == ConsoleKey::E) selection = 0;
                else if (keyInfo.Key == ConsoleKey::T) selection = 1;
                else if (keyInfo.Key == ConsoleKey::I) selection = 2;
                else if (keyInfo.Key == ConsoleKey::H) selection = 3;
                else if (keyInfo.Key == ConsoleKey::V) selection = 4;
                else if (keyInfo.Key == ConsoleKey::D) selection = 5;
                else if (keyInfo.Key == ConsoleKey::C) selection = 6;
                else if (keyInfo.Key == ConsoleKey::S) selection = 7;
                else if (keyInfo.Key == ConsoleKey::F) selection = 8;
                else if (keyInfo.Key == ConsoleKey::W) selection = 9;
                else if (keyInfo.Key == ConsoleKey::N) selection = 10;
                // These offsets intentionally match Menu.cs, including its shortcut/display mismatch.
                else if (keyInfo.Key == ConsoleKey::R) selection = 11;
                else if (keyInfo.Key == ConsoleKey::P) selection = 12;
                else if (keyInfo.Key == ConsoleKey::L) selection = 13;
                else if (keyInfo.Key == ConsoleKey::A) selection = 14;
                else if (keyInfo.Key == ConsoleKey::G) selection = 15;
                else if (keyInfo.Key == ConsoleKey::D1 || keyInfo.Key == ConsoleKey::NumPad1) selection = 16;
                else if (keyInfo.Key == ConsoleKey::Backspace || keyInfo.Key == ConsoleKey::Delete)
                {
                    if (selection == 0) Features::NoRepeatEncounters(true);
                    else if (selection == 1) Features::AllowInvalidTeams(true);
                    else if (selection == 2) Features::TopScreenTargetInfo(true);
                    else if (selection == 3) Features::HelmetOpacity(1.0F);
                    else if (selection == 4) Features::VisorOpacity(0.5F);
                    else if (selection == 5) Features::HudOpacity(1.0F);
                    else if (selection == 6) Features::ReticleOpacity(1.0F);
                    else if (selection == 7) Features::HudSway(true);
                    else if (selection == 8) Features::TargetInfoSway(false);
                    else if (selection == 9) Features::DelayedIdleSway(true);
                    else if (selection == 10) Features::NoIdleSway(false);
                    else if (selection == 11) Features::NoMapCentering(false);
                    else if (selection == 12) Features::MaxRoomDetail(false);
                    else if (selection == 13) Features::MaxPlayerDetail(true);
                    else if (selection == 14) Features::LogSpatialAudio(false);
                    else if (selection == 15) Features::HalfSecondAlarm(false);
                    else if (selection == 16) Features::FullBoostCharge(false);
                    else if (selection == 17) Features::BoostOpensDoors(false);
                    else if (selection == 18) Features::AlternateHunters1P(true);
                }
                else if (keyInfo.Key == ConsoleKey::Add || keyInfo.Key == ConsoleKey::OemPlus || keyInfo.Key == ConsoleKey::RightArrow
                    || keyInfo.Key == ConsoleKey::Subtract || keyInfo.Key == ConsoleKey::OemMinus || keyInfo.Key == ConsoleKey::LeftArrow)
                {
                    const std::int32_t direction = keyInfo.Key == ConsoleKey::Add || keyInfo.Key == ConsoleKey::OemPlus
                        || keyInfo.Key == ConsoleKey::RightArrow ? 1 : -1;
                    auto UpdateOpacity = [&](float value)
                    {
                        if (direction == 1)
                        {
                            if (value <= 0) return 0.5F;
                            if (value >= 1) return 0.0F;
                            return 1.0F;
                        }
                        if (value <= 0) return 1.0F;
                        if (value >= 1) return 0.5F;
                        return 0.0F;
                    };
                    if (selection == 0) Features::NoRepeatEncounters(!Features::NoRepeatEncounters());
                    else if (selection == 1) Features::AllowInvalidTeams(!Features::AllowInvalidTeams());
                    else if (selection == 2) Features::TopScreenTargetInfo(!Features::TopScreenTargetInfo());
                    else if (selection == 3) Features::HelmetOpacity(UpdateOpacity(Features::HelmetOpacity()));
                    else if (selection == 4) Features::VisorOpacity(UpdateOpacity(Features::VisorOpacity()));
                    else if (selection == 5) Features::HudOpacity(UpdateOpacity(Features::HudOpacity()));
                    else if (selection == 6) Features::ReticleOpacity(UpdateOpacity(Features::ReticleOpacity()));
                    else if (selection == 7) Features::HudSway(!Features::HudSway());
                    else if (selection == 8) Features::TargetInfoSway(!Features::TargetInfoSway());
                    else if (selection == 9) Features::DelayedIdleSway(!Features::DelayedIdleSway());
                    else if (selection == 10) Features::NoIdleSway(!Features::NoIdleSway());
                    else if (selection == 11) Features::NoMapCentering(!Features::NoMapCentering());
                    else if (selection == 12) Features::MaxRoomDetail(!Features::MaxRoomDetail());
                    else if (selection == 13) Features::MaxPlayerDetail(!Features::MaxPlayerDetail());
                    else if (selection == 14) Features::LogSpatialAudio(!Features::LogSpatialAudio());
                    else if (selection == 15) Features::HalfSecondAlarm(!Features::HalfSecondAlarm());
                    else if (selection == 16) Features::FullBoostCharge(!Features::FullBoostCharge());
                    else if (selection == 17) Features::BoostOpensDoors(!Features::BoostOpensDoors());
                    else if (selection == 18) Features::AlternateHunters1P(!Features::AlternateHunters1P());
                }
            }
            else if (screen == 2)
            {
                if (keyInfo.Key == ConsoleKey::Enter || keyInfo.Key == ConsoleKey::B
                    || keyInfo.Key == ConsoleKey::Spacebar && selection == s) { screen = 0; selection = 1; }
                else if (keyInfo.Key == ConsoleKey::W) selection = 0;
                else if (keyInfo.Key == ConsoleKey::J) selection = 1;
                else if (keyInfo.Key == ConsoleKey::E) selection = 2;
                else if (keyInfo.Key == ConsoleKey::D) selection = 3;
                else if (keyInfo.Key == ConsoleKey::R) selection = 4;
                else if (keyInfo.Key == ConsoleKey::I) selection = 5;
                else if (keyInfo.Key == ConsoleKey::U) selection = 6;
                else if (keyInfo.Key == ConsoleKey::O) selection = 7;
                else if (keyInfo.Key == ConsoleKey::G) selection = 8;
                else if (keyInfo.Key == ConsoleKey::D2 || keyInfo.Key == ConsoleKey::NumPad2) selection = 9;
                else if (keyInfo.Key == ConsoleKey::Q) selection = 10;
                else if (keyInfo.Key == ConsoleKey::Backspace || keyInfo.Key == ConsoleKey::Delete)
                {
                    if (selection == 0) Cheats::FreeWeaponSelect(false);
                    else if (selection == 1) Cheats::UnlimitedJumps(false);
                    else if (selection == 2) Cheats::NoRandomEncounters(false);
                    else if (selection == 3) Cheats::UnlockAllDoors(false);
                    else if (selection == 4) Cheats::ContinueFromCurrentRoom(false);
                    else if (selection == 5) Cheats::SkipPlanetIntros(false);
                    else if (selection == 6) Cheats::StartWithAllUpgrades(false);
                    else if (selection == 7) Cheats::StartWithAllOctoliths(false);
                    else if (selection == 8) Cheats::WalkThroughWalls(false);
                    else if (selection == 9) Cheats::AlwaysFightGorea2(false);
                    else if (selection == 10) Cheats::QuadrupleDamage(false);
                }
                else if (keyInfo.Key == ConsoleKey::Add || keyInfo.Key == ConsoleKey::OemPlus || keyInfo.Key == ConsoleKey::RightArrow
                    || keyInfo.Key == ConsoleKey::Subtract || keyInfo.Key == ConsoleKey::OemMinus || keyInfo.Key == ConsoleKey::LeftArrow)
                {
                    if (selection == 0) Cheats::FreeWeaponSelect(!Cheats::FreeWeaponSelect());
                    else if (selection == 1) Cheats::UnlimitedJumps(!Cheats::UnlimitedJumps());
                    else if (selection == 2) Cheats::NoRandomEncounters(!Cheats::NoRandomEncounters());
                    else if (selection == 3) Cheats::UnlockAllDoors(!Cheats::UnlockAllDoors());
                    else if (selection == 4) Cheats::ContinueFromCurrentRoom(!Cheats::ContinueFromCurrentRoom());
                    else if (selection == 5) Cheats::SkipPlanetIntros(!Cheats::SkipPlanetIntros());
                    else if (selection == 6) Cheats::StartWithAllUpgrades(!Cheats::StartWithAllUpgrades());
                    else if (selection == 7) Cheats::StartWithAllOctoliths(!Cheats::StartWithAllOctoliths());
                    else if (selection == 8) Cheats::WalkThroughWalls(!Cheats::WalkThroughWalls());
                    else if (selection == 9) Cheats::AlwaysFightGorea2(!Cheats::AlwaysFightGorea2());
                    else if (selection == 10) Cheats::QuadrupleDamage(!Cheats::QuadrupleDamage());
                }
            }
            else if (screen == 3)
            {
                if (keyInfo.Key == ConsoleKey::Enter || keyInfo.Key == ConsoleKey::B
                    || keyInfo.Key == ConsoleKey::Spacebar && selection == s) { screen = 0; selection = 2; }
                else if (keyInfo.Key == ConsoleKey::C) selection = 0;
                else if (keyInfo.Key == ConsoleKey::N) selection = 1;
                else if (keyInfo.Key == ConsoleKey::R) selection = 2;
                else if (keyInfo.Key == ConsoleKey::S) selection = 3;
                else if (keyInfo.Key == ConsoleKey::E) selection = 4;
                else if (keyInfo.Key == ConsoleKey::T) selection = 5;
                else if (keyInfo.Key == ConsoleKey::Backspace || keyInfo.Key == ConsoleKey::Delete)
                {
                    if (selection == 0) Bugfixes::SmoothCamSeqHandoff(false);
                    else if (selection == 1) Bugfixes::BetterCamSeqNodeRef(true);
                    else if (selection == 2) Bugfixes::NoStrayRespawnText(false);
                    else if (selection == 3) Bugfixes::CorrectBountySfx(true);
                    else if (selection == 4) Bugfixes::NoDoubleEnemyDeath(true);
                    else if (selection == 5) Bugfixes::NoSlenchRollTimerUnderflow(true);
                }
                else if (keyInfo.Key == ConsoleKey::Add || keyInfo.Key == ConsoleKey::OemPlus || keyInfo.Key == ConsoleKey::RightArrow
                    || keyInfo.Key == ConsoleKey::Subtract || keyInfo.Key == ConsoleKey::OemMinus || keyInfo.Key == ConsoleKey::LeftArrow)
                {
                    if (selection == 0) Bugfixes::SmoothCamSeqHandoff(!Bugfixes::SmoothCamSeqHandoff());
                    else if (selection == 1) Bugfixes::BetterCamSeqNodeRef(!Bugfixes::BetterCamSeqNodeRef());
                    else if (selection == 2) Bugfixes::NoStrayRespawnText(!Bugfixes::NoStrayRespawnText());
                    else if (selection == 3) Bugfixes::CorrectBountySfx(!Bugfixes::CorrectBountySfx());
                    else if (selection == 4) Bugfixes::NoDoubleEnemyDeath(!Bugfixes::NoDoubleEnemyDeath());
                    else if (selection == 5) Bugfixes::NoSlenchRollTimerUnderflow(!Bugfixes::NoSlenchRollTimerUnderflow());
                }
            }
        }
        return true;
    }
}

namespace MphRead
{
    bool Menu::ShowSoundTest(SoundCapability soundCapability)
    {
        struct MusicListItem { MusicType Type; std::int32_t Id; const char* Name; };
        static const std::vector<MusicListItem> musicList{
            {MusicType::Seq, (std::int32_t)SeqId::DRONE, "Intro"},
            {MusicType::Stream, (std::int32_t)VoiceId::STRM_TITLE_SCREEN, "Title"},
            {MusicType::Seq, (std::int32_t)SeqId::CHUTNEY, "Menu"},
            {MusicType::Seq, (std::int32_t)SeqId::MENU1, "Menu (Unused)"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_MP2_M15, "Celestial Archives VS."},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_MP2_M40, "Celestial Archives VS. (Octolith)"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_MP2_M6, "Celestial Archives VS. (Node)"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_MP1_M12, "Alinos VS."},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_MP1_M11, "Alinos VS. (Octolith)"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_MP1_M43, "Alinos VS. (Node)"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_DILL_M33, "VDO VS."},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_DILL_M42, "VDO VS. (Octolith)"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_DILL_M47, "VDO VS. (Node)"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_PEPPER_M36, "Arcterra VS."},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_PEPPER_M38, "Arcterra VS. (Octolith)"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_PEPPER_M45, "Arcterra VS. (Node)"},
            {MusicType::Seq, (std::int32_t)SeqId::RESULTS, "Results"},
            {MusicType::Seq, (std::int32_t)SeqId::NEW_GAME, "Story"},
            {MusicType::Seq, (std::int32_t)SeqId::SHIP, "Tetra Galaxy"},
            {MusicType::Seq, (std::int32_t)SeqId::FLY_IN_2, "Landing (Celestial Archives)"},
            {MusicType::Seq, (std::int32_t)SeqId::SHIP_LAND2, "Ship Cockpit (Celestial Archives)"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_GREY_M17, "Shadows"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_ENEMY_1_M28, "Enemies"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_YELLOW_M1, "The Archives"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_INTRO_KANDEN_M30, "Pursuit"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_GUMBO_M3, "Kanden"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_AMBIENT_1_M2, "Foreboding"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_GARLIC_M4, "Cretaphid"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_TELEPORT_M5, "Aftermath"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_OREGANO_M55, "Escape"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_OREGANO_M56, "Escape (Alarm)"},
            {MusicType::Seq, (std::int32_t)SeqId::FLY_IN_1, "Landing (Alinos)"},
            {MusicType::Seq, (std::int32_t)SeqId::SHIP_LAND1, "Ship Cockpit (Alinos)"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_RED_M13, "Alinos"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_GUMBO_M37, "Spire"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_SAFFRON_M29, "Slench"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_GUMBO_M10, "Weavel"},
            {MusicType::Seq, (std::int32_t)SeqId::FLY_IN_3, "Landing (VDO)"},
            {MusicType::Seq, (std::int32_t)SeqId::SHIP_LAND3, "Ship Cockpit (VDO)"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_GREEN_M19, "The Outpost"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_GREEN_M50, "The Outpost (Race)"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_GUARDIAN_M18, "Guardians"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_GUMBO_M39, "Sylux"},
            {MusicType::Seq, (std::int32_t)SeqId::FLY_IN_4, "Landing (Arcterra)"},
            {MusicType::Seq, (std::int32_t)SeqId::SHIP_LAND4, "Ship Cockpit (Arcterra)"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_WHITE_M48, "Desolation"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_WHITE_M54, "Desolation (Maze)"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_BLUE_M14, "Arcterra"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_BLUE_M44, "Arcterra (Puzzle)"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_GUMBO_M49, "Noxus & Trace"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_GUMBO_M7, "Noxus"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_GUMBO_M41, "Trace"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_RED_M60, "The Elders"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_BRINSTAR_M67, "Magma Drop"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_PARASITE_M16, "Demon Spawn"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_INDIGO_M59, "Space Decay"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_BLACK_M53, "Watching"},
            {MusicType::Seq, (std::int32_t)SeqId::FLY_IN_GOREA, "Landing (Oubliette)"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_GOREA_1_M20, "Gorea"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_GOREA_1_M22, "Gorea (Battlehammer)"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_GOREA_1_M26, "Gorea (Volt Driver)"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_GOREA_1_M27, "Gorea (Mamgmaul)"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_GOREA_1_M24, "Gorea (Judicator)"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_GOREA_1_M23, "Gorea (Imperialist)"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_GOREA_1_M25, "Gorea (Shock Coil)"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_GOREA_1_M21, "Gorea (Seal Sphere)"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_CREDITS_M65, "Hunters (Credits)"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_GOREA_2_M34, "Oubliette"},
            {MusicType::Music, (std::int32_t)MusicId::SEQ_GOREA_2_M35, "Oubliette (Node)"},
            {MusicType::Seq, (std::int32_t)SeqId::GET_WEAPON, "Get Weapon"},
            {MusicType::Seq, (std::int32_t)SeqId::GET_OCTOLITH, "Get Octolith"}
        };

        std::int32_t selection = 0;
        auto info = Formats::Sound::SoundRead::ReadInterMusicInfo();
        if (soundCapability != SoundCapability::None) Sound::Sfx::Load(*static_cast<Scene*>(nullptr));
        std::int32_t playlist = 0;
        MusicId music = MusicId::SEQ_YELLOW_M1;
        auto track = info->at(static_cast<std::size_t>(static_cast<std::int32_t>(music)));
        std::string musicStr = MusicString(music) + " [SEQ_" + SeqString(track->SeqId) + "]";
        SeqId seq = SeqId::BRINSTAR;
        std::uint16_t tracks = std::numeric_limits<std::uint16_t>::max();
        SfxId sfx = SfxId::LID_CLOSE;
        VoiceId stream = VoiceId::VOICE_CONSECUTIVE_KILLS;

        auto UpdatePlaylist = [&](std::int32_t dir)
        {
            playlist += dir;
            if (playlist < 0) playlist = static_cast<std::int32_t>(musicList.size()) - 1;
            else if (playlist >= static_cast<std::int32_t>(musicList.size())) playlist = 0;
        };
        auto GetPlaylistString = [&]()
        {
            const MusicListItem& item = musicList.at(static_cast<std::size_t>(playlist));
            std::string id;
            if (item.Type == MusicType::Stream) id = VoiceString(static_cast<VoiceId>(item.Id));
            else if (item.Type == MusicType::Seq) id = "SEQ_" + SeqString(static_cast<SeqId>(item.Id));
            else id = MusicString(static_cast<MusicId>(item.Id));
            return std::string(item.Name) + " [" + id + "]";
        };
        auto UpdateMusic = [&](std::int32_t dir)
        {
            std::int32_t id = static_cast<std::int32_t>(music) + dir;
            if (id < 1) id = 68;
            else if (id > 68) id = 1;
            music = static_cast<MusicId>(id);
            auto item = info->at(static_cast<std::size_t>(id));
            musicStr = MusicString(music) + " [SEQ_" + SeqString(item->SeqId) + "]";
        };
        auto UpdateSeq = [&](std::int32_t dir)
        {
            std::int32_t id = static_cast<std::int32_t>(seq) + dir;
            if (id < 0) id = 59;
            else if (id > 59) id = 0;
            seq = static_cast<SeqId>(id);
        };
        auto UpdateSfx = [&](std::int32_t dir)
        {
            std::int32_t id = static_cast<std::int32_t>(sfx) + dir;
            if (id < 0) id = 48 | 0x8000;
            else if (id > (48 | 0x8000)) id = 0;
            else if (id == 528) id = 0x8000;
            else if (id == 0x8000 - 1) id = 527;
            sfx = static_cast<SfxId>(id);
        };
        auto GetSfxString = [&]()
        {
            const std::int32_t id = static_cast<std::int32_t>(sfx);
            std::ostringstream out;
            if ((id & 0x4000) != 0) out << "Script " << std::left << std::setw(3) << (id & ~0x4000) << " - " << SfxString(sfx);
            else if ((id & 0x8000) != 0) out << "DGN " << std::left << std::setw(2) << (id & ~0x8000) << " - " << SfxString(sfx);
            else out << "SFX " << std::right << std::setw(3) << id << " - SFX_" << SfxString(sfx);
            return out.str();
        };
        auto UpdateStream = [&](std::int32_t dir)
        {
            std::int32_t id = static_cast<std::int32_t>(stream) + dir;
            if (id < 0) id = 11;
            else if (id > 11) id = 0;
            stream = static_cast<VoiceId>(id);
        };
        auto Stop = [&]()
        {
            MusicPlayer::Stop();
            if (auto instance = Sound::Sfx::Instance()) instance->StopAllSound(true);
        };
        auto PlayPlaylist = [&]()
        {
            MusicPlayer::Stop();
            if (auto instance = Sound::Sfx::Instance()) instance->StopAllSound(true);
            const MusicListItem& item = musicList.at(static_cast<std::size_t>(playlist));
            SeqId seqToPlay;
            if (item.Type == MusicType::Stream)
            {
                if (auto instance = Sound::Sfx::Instance()) instance->PlayFreeStream(item.Id);
                return;
            }
            if (item.Type == MusicType::Seq)
            {
                seqToPlay = static_cast<SeqId>(item.Id);
                tracks = std::numeric_limits<std::uint16_t>::max();
            }
            else
            {
                auto m = info->at(static_cast<std::size_t>(item.Id));
                tracks = m->Tracks;
                seqToPlay = m->SeqId;
            }
            MusicPlayer::Load(seqToPlay, tracks);
            MusicPlayer::WaitForLoad();
            MusicPlayer::Play(_musicVolume.ToSingle());
        };
        auto PlayMusic = [&]()
        {
            MusicPlayer::Stop();
            if (auto instance = Sound::Sfx::Instance()) instance->StopAllSound(true);
            auto item = info->at(static_cast<std::size_t>(static_cast<std::int32_t>(music)));
            tracks = item->Tracks;
            MusicPlayer::Load(item->SeqId, tracks);
            MusicPlayer::WaitForLoad();
            MusicPlayer::Play(_musicVolume.ToSingle());
        };
        auto PlaySeq = [&]()
        {
            MusicPlayer::Stop();
            if (auto instance = Sound::Sfx::Instance()) instance->StopAllSound(true);
            tracks = std::numeric_limits<std::uint16_t>::max();
            MusicPlayer::Load(seq);
            MusicPlayer::WaitForLoad();
            MusicPlayer::Play(_musicVolume.ToSingle());
        };
        auto PlaySfx = [&]()
        {
            MusicPlayer::Stop();
            if (auto instance = Sound::Sfx::Instance()) instance->StopAllSound(true);
            const std::int32_t id = static_cast<std::int32_t>(sfx);
            if ((id & 0x4000) != 0) return;
            if ((id & 0x8000) != 0)
            {
                const std::int32_t amountA = NativeRuntime::RandomSharedNext(0xFFFF);
                const std::int32_t amountB = NativeRuntime::RandomSharedNext(0xFFFF);
                if (auto instance = Sound::Sfx::Instance())
                {
                    instance->PlayDgn(id, nullptr, false, false, -1.0F, false,
                        static_cast<float>(amountA), static_cast<float>(amountB));
                }
            }
            else if (auto instance = Sound::Sfx::Instance())
            {
                instance->PlaySample(id, nullptr, std::nullopt, false, -1.0F, false, false);
            }
        };
        auto PlayStream = [&]()
        {
            MusicPlayer::Stop();
            if (auto instance = Sound::Sfx::Instance()) instance->StopAllSound(true);
            if (auto instance = Sound::Sfx::Instance()) instance->PlayFreeStream(static_cast<std::int32_t>(stream));
        };
        auto X = [&](std::int32_t index) { return Mark(selection == index); };

        while (true)
        {
            std::int32_t s = 0;
            ClearConsole(); WriteLine(ProgramVersionBanner()); WriteLine();
            WriteLine("Choose a setting using up/down or with the key indicated.");
            WriteLine("Press Space to specify, Backspace to clear, or left/right to advance the setting.");
            WriteLine("When finished, press Enter or use the last option to return. Press Escape to exit.");
            WriteLine(); WriteLine("Sound Test"); WriteLine();
            { std::ostringstream o; o << X(s++) << " (P) MPH Playlist: " << std::setw(2) << playlist << " - " << GetPlaylistString(); WriteLine(o.str()); }
            { std::ostringstream o; o << X(s++) << " (M) Music Tracks: " << std::setw(2) << static_cast<std::int32_t>(music) << " - " << musicStr; WriteLine(o.str()); }
            { std::ostringstream o; o << X(s++) << " (S) Raw Sequence: " << std::setw(2) << static_cast<std::int32_t>(seq) << " - SEQ_" << SeqString(seq); WriteLine(o.str()); }
            { std::ostringstream o; o << X(s++) << " (V) Voice/Stream: " << std::setw(2) << static_cast<std::int32_t>(stream) << " - " << VoiceString(stream); WriteLine(o.str()); }
            WriteLine(X(s++) + " (X) Sound Effect: " + GetSfxString());
            WriteLine(X(s++) + " (C) Stop");
            WriteLine(X(s++) + " (B) Go Back");
            --s;
            PrintSoundInfo(soundCapability);
            ConsoleKeyInfo keyInfo = ReadKey();
            if (keyInfo.Key == ConsoleKey::UpArrow) { if (--selection < 0) selection = s; }
            else if (keyInfo.Key == ConsoleKey::DownArrow) { if (++selection > s) selection = 0; }
            else if (keyInfo.Key == ConsoleKey::Escape)
            {
                MusicPlayer::Stop(); Sound::Sfx::ShutDown(); return false;
            }
            else if (keyInfo.Key == ConsoleKey::B || selection == s
                && (keyInfo.Key == ConsoleKey::Enter || keyInfo.Key == ConsoleKey::Spacebar)) break;
            if (keyInfo.Key == ConsoleKey::P) selection = 0;
            else if (keyInfo.Key == ConsoleKey::M) selection = 1;
            else if (keyInfo.Key == ConsoleKey::S) selection = 2;
            else if (keyInfo.Key == ConsoleKey::V) selection = 3;
            else if (keyInfo.Key == ConsoleKey::X) selection = 4;
            else if (keyInfo.Key == ConsoleKey::C || selection == 5 && keyInfo.Key == ConsoleKey::Spacebar) Stop();
            else if (keyInfo.Key == ConsoleKey::Backspace)
            {
                if (selection == 0) { playlist = 0; UpdatePlaylist(0); }
                else if (selection == 1) { music = MusicId::SEQ_YELLOW_M1; UpdateMusic(0); }
                else if (selection == 2) { seq = SeqId::BRINSTAR; UpdateSeq(0); }
                else if (selection == 3) stream = VoiceId::VOICE_CONSECUTIVE_KILLS;
                else if (selection == 4) sfx = SfxId::LID_CLOSE;
            }
            else if (keyInfo.Key == ConsoleKey::LeftArrow)
            {
                if (selection == 0) UpdatePlaylist(-1); else if (selection == 1) UpdateMusic(-1);
                else if (selection == 2) UpdateSeq(-1); else if (selection == 3) UpdateStream(-1); else if (selection == 4) UpdateSfx(-1);
            }
            else if (keyInfo.Key == ConsoleKey::RightArrow)
            {
                if (selection == 0) UpdatePlaylist(1); else if (selection == 1) UpdateMusic(1);
                else if (selection == 2) UpdateSeq(1); else if (selection == 3) UpdateStream(1); else if (selection == 4) UpdateSfx(1);
            }
            else if (keyInfo.Key == ConsoleKey::Spacebar)
            {
                if (selection == 0)
                {
                    WriteLine(); WriteLine("Enter playlist index.");
                    auto entry = ReadLine(); std::int32_t id = 0;
                    if (entry && Int32TryParseCurrentCulture(*entry, id) && id >= 0 && id < static_cast<std::int32_t>(musicList.size()))
                    { playlist = id; UpdatePlaylist(0); PlayPlaylist(); }
                }
                else if (selection == 1)
                {
                    WriteLine(); WriteLine("Enter music name or ID.");
                    auto entry = ReadLine(); std::int32_t id = 0;
                    if (entry && Int32TryParseCurrentCulture(*entry, id))
                    {
                        if (id >= 1 && id <= 68) { music = static_cast<MusicId>(id); UpdateMusic(0); PlayMusic(); }
                    }
                    else if (entry)
                    {
                        MusicId parsed{};
                        if (TryParseMusic(ToUpper(*entry), parsed) && parsed != MusicId::Invalid && parsed != MusicId::None)
                        { music = parsed; UpdateMusic(0); PlayMusic(); }
                    }
                }
                else if (selection == 2)
                {
                    WriteLine(); WriteLine("Enter sequence name or ID.");
                    auto entry = ReadLine(); std::int32_t id = 0;
                    if (entry && Int32TryParseCurrentCulture(*entry, id))
                    {
                        if (id >= 0 && id <= 59) { seq = static_cast<SeqId>(id); PlaySeq(); }
                    }
                    else if (entry)
                    {
                        std::string name = ToUpper(*entry); name = StringReplace(std::move(name), "SEQ_", "");
                        SeqId parsed{}; if (TryParseSeq(name, parsed) && parsed != SeqId::None) { seq = parsed; PlaySeq(); }
                    }
                }
                else if (selection == 3)
                {
                    WriteLine(); WriteLine("Enter voice/stream name or ID.");
                    auto entry = ReadLine(); std::int32_t id = 0;
                    if (entry && Int32TryParseCurrentCulture(*entry, id))
                    {
                        if (id >= 0 && id <= 11) { stream = static_cast<VoiceId>(id); PlayStream(); }
                    }
                    else if (entry)
                    {
                        VoiceId parsed{}; if (TryParseVoice(ToUpper(*entry), parsed) && parsed != VoiceId::None) { stream = parsed; PlayStream(); }
                    }
                }
                else if (selection == 4)
                {
                    WriteLine(); WriteLine("Enter SFX name or ID.");
                    auto entry = ReadLine(); std::int32_t id = 0;
                    if (entry && Int32TryParseCurrentCulture(*entry, id))
                    {
                        if (id >= 0 && id <= 527) { sfx = static_cast<SfxId>(id); PlaySfx(); }
                    }
                    else if (entry)
                    {
                        std::string name = ToUpper(*entry); name = StringReplace(std::move(name), "SFX_", "");
                        SfxId parsed{};
                        if (TryParseSfx(name, parsed) && parsed != SfxId::None)
                        {
                            const std::int32_t value = static_cast<std::int32_t>(parsed);
                            if (value < 0x4000 || value > (104 | 0x4000)) { sfx = parsed; PlaySfx(); }
                        }
                    }
                }
            }
            else if (keyInfo.Key == ConsoleKey::Enter)
            {
                if (selection == 0) PlayPlaylist(); else if (selection == 1) PlayMusic();
                else if (selection == 2) PlaySeq(); else if (selection == 3) PlayStream(); else if (selection == 4) PlaySfx();
            }
        }
        MusicPlayer::Stop(); Sound::Sfx::ShutDown(); return true;
    }
}

namespace MphRead
{
    void Menu::UpdateSaveInfo()
    {
        auto save = GameState::ReadSave();
        auto ArtifactDisplay = [&](std::int32_t area, std::int32_t artifact)
        { return save->CheckFoundArtifact(artifact, area) ? std::string("1") : std::string("0"); };
        auto OctolithDisplay = [&](std::int32_t area)
        { return save->CheckFoundOctolith(area) ? std::string("1") : std::string("0"); };
        auto WeaponDisplay = [&](BeamType weapon, std::string_view name)
        {
            if ((save->Weapons & (1 << static_cast<std::int32_t>(weapon))) == 0) return std::string();
            return std::string(EndsWith(_saveInfo[8], " ") ? "" : ", ") + std::string(name);
        };
        _saveInfo[0] = "Alinos   : " + std::string((save->Areas & 1) == 0 ? "locked  " : "unlocked") + ": ";
        _saveInfo[1] = "CA       : " + std::string((save->Areas & 4) == 0 ? "locked  " : "unlocked") + ": ";
        _saveInfo[2] = "VDO      : " + std::string((save->Areas & 0x10) == 0 ? "locked  " : "unlocked") + ": ";
        _saveInfo[3] = "Arcterra : " + std::string((save->Areas & 0x40) == 0 ? "locked  " : "unlocked") + ": ";
        _saveInfo[4] = "Oubliette: " + std::string((save->Areas & 0x100) == 0 ? "locked  " : "unlocked");
        _saveInfo[5] = "Artifacts:"
            " CA " + ArtifactDisplay(2,0) + "/" + ArtifactDisplay(2,1) + "/" + ArtifactDisplay(2,2)
            + " " + ArtifactDisplay(3,0) + "/" + ArtifactDisplay(3,1) + "/" + ArtifactDisplay(3,2) + ","
            + " Alinos " + ArtifactDisplay(0,0) + "/" + ArtifactDisplay(0,1) + "/" + ArtifactDisplay(0,2)
            + " " + ArtifactDisplay(1,0) + "/" + ArtifactDisplay(1,1) + "/" + ArtifactDisplay(1,2) + ","
            + " VDO " + ArtifactDisplay(4,0) + "/" + ArtifactDisplay(4,1) + "/" + ArtifactDisplay(4,2)
            + " " + ArtifactDisplay(5,0) + "/" + ArtifactDisplay(5,1) + "/" + ArtifactDisplay(5,2) + ","
            + " Arcterra " + ArtifactDisplay(6,0) + "/" + ArtifactDisplay(6,1) + "/" + ArtifactDisplay(6,2)
            + " " + ArtifactDisplay(7,0) + "/" + ArtifactDisplay(7,1) + "/" + ArtifactDisplay(7,2);
        _saveInfo[6] = "Octoliths: CA " + OctolithDisplay(2) + "     " + OctolithDisplay(3) + ","
            + "     Alinos " + OctolithDisplay(0) + "     " + OctolithDisplay(1) + ","
            + "     VDO " + OctolithDisplay(4) + "     " + OctolithDisplay(5) + ","
            + "     Arcterra " + OctolithDisplay(6) + "     " + OctolithDisplay(7);
        _saveInfo[7] = "Expansion: Health " + std::to_string(save->HealthMax)
            + ", Missiles " + std::to_string((*save->AmmoMax)[1] / 10)
            + ", UA " + std::to_string((*save->AmmoMax)[0] / 10);
        _saveInfo[8] = "Weapons  : ";
        _saveInfo[8] += WeaponDisplay(BeamType::Battlehammer, "Battlehammer");
        _saveInfo[8] += WeaponDisplay(BeamType::Judicator, "Judicator");
        _saveInfo[8] += WeaponDisplay(BeamType::VoltDriver, "Volt Driver");
        _saveInfo[8] += WeaponDisplay(BeamType::Magmaul, "Magmaul");
        _saveInfo[8] += WeaponDisplay(BeamType::ShockCoil, "Shock Coil");
        _saveInfo[8] += WeaponDisplay(BeamType::Imperialist, "Imperialist");
        _saveInfo[8] += WeaponDisplay(BeamType::OmegaCannon, "Omega Cannon");
        _saveInfo[9] = "Complete : " + std::to_string(save->GetCompletionPercentage()) + "%";
        const std::uint32_t rng2 = Rng::Rng2();
        SceneSetup::UpdateAreaHunters(save.get());
        Rng::SetRng2(rng2);
        for (std::int32_t i = 0; i < 4; ++i)
        {
            for (std::int32_t j = 1; j < 7; ++j)
            {
                if (((*save->AreaHunters)[static_cast<std::size_t>(i)] & (1 << j)) != 0)
                {
                    if (!EndsWith(_saveInfo[static_cast<std::size_t>(i)], " ")) _saveInfo[static_cast<std::size_t>(i)] += ", ";
                    _saveInfo[static_cast<std::size_t>(i)] += HunterString(static_cast<Hunter>(j));
                    std::int32_t octoliths = 0;
                    for (std::int32_t k = 0; k < 8; ++k)
                    {
                        if (((save->LostOctoliths >> (4 * k)) & 15U) == static_cast<std::uint32_t>(j)) ++octoliths;
                    }
                    if (octoliths > 0) _saveInfo[static_cast<std::size_t>(i)] += " (x" + std::to_string(octoliths) + ")";
                }
            }
        }
    }

    bool Menu::ShowLogbook(StorySave& save)
    {
        std::int32_t category = -1;
        std::int32_t viewId = -1;
        std::int32_t selection = 0;
        std::int32_t listPos = -1;
        Scene::Language(Paths::MphKey == "AMHK0" ? Language::Japanese : _language);
        auto entries = Text::Strings::ReadStringTable(Text::StringTables::ScanLog);
        auto Category = [](char value)
        {
            auto categories = std::make_shared<ManagedArray<char>>(1);
            (*categories)[0] = value;
            return categories;
        };
        const std::int32_t loreMax = save.GetLogbookCount(false, Category('L'));
        const std::int32_t bioformMax = save.GetLogbookCount(false, Category('B'));
        const std::int32_t objectMax = save.GetLogbookCount(false, Category('O'));
        const std::int32_t equipMax = save.GetLogbookCount(false, Category('E'));
        const std::int32_t scanPct = static_cast<std::int32_t>(save.ScanCount / static_cast<float>(save.GetMaxScanCount()) * 100.0F);
        const std::int32_t lorePct = static_cast<std::int32_t>(save.GetLogbookCount(true, Category('L')) / static_cast<float>(loreMax) * 100.0F);
        const std::int32_t bioformPct = static_cast<std::int32_t>(save.GetLogbookCount(true, Category('B')) / static_cast<float>(bioformMax) * 100.0F);
        const std::int32_t objectPct = static_cast<std::int32_t>(save.GetLogbookCount(true, Category('O')) / static_cast<float>(objectMax) * 100.0F);
        const std::int32_t equipPct = static_cast<std::int32_t>(save.EquipmentCount / static_cast<float>(equipMax) * 100.0F);
        using LogItem = std::pair<std::string, std::string>;
        std::vector<LogItem> loreList(static_cast<std::size_t>(loreMax));
        std::vector<LogItem> bioformList(static_cast<std::size_t>(bioformMax));
        std::vector<LogItem> objectList(static_cast<std::size_t>(objectMax));
        std::vector<LogItem> equipList(static_cast<std::size_t>(equipMax));
        std::int32_t loreIndex = 0, bioformIndex = 0, objectIndex = 0, equipmentIndex = 0;
        for (std::int32_t i = 0; i < static_cast<std::int32_t>(entries->size()); ++i)
        {
            const auto& entry = entries->at(static_cast<std::size_t>(i));
            auto WriteEntry = [&](std::vector<LogItem>& target, std::int32_t index)
            {
                target.at(static_cast<std::size_t>(index)) = save.CheckLogbook(i)
                    ? LogItem{entry->String1, entry->String2} : LogItem{"-", "-"};
            };
            if (entry->Category == u'L') WriteEntry(loreList, loreIndex++);
            else if (entry->Category == u'B') WriteEntry(bioformList, bioformIndex++);
            else if (entry->Category == u'O') WriteEntry(objectList, objectIndex++);
            else if (entry->Category == u'E') WriteEntry(equipList, equipmentIndex++);
        }
        std::vector<LogItem>* list = &loreList;
        while (true)
        {
            std::int32_t s = 0;
            auto X = [&](std::int32_t index)
            {
                const bool check = selection == index || (category != -1 && category + 1 == s);
                return Mark(check);
            };
            auto Y = [&](std::int32_t index) { return Mark(index == listPos); };
            ClearConsole(); WriteLine(ProgramVersionBanner()); WriteLine();
            if (category == -1) WriteLine("Select a category");
            else if (category == 0) WriteLine("Lore: " + std::to_string(lorePct) + "%");
            else if (category == 1) WriteLine("Bioform: " + std::to_string(bioformPct) + "%");
            else if (category == 2) WriteLine("Object: " + std::to_string(objectPct) + "%");
            else if (category == 3) WriteLine("Equipment: " + std::to_string(equipPct) + "%");
            WriteLine(X(s++) + " (L) Lore      \\");
            { std::ostringstream out; out << X(s++) << " (F) Bioform   } " << std::setw(3) << scanPct << '%'; WriteLine(out.str()); }
            WriteLine(X(s++) + " (O) Object    /");
            { std::ostringstream out; out << X(s++) << " (E) Equipment } " << std::setw(3) << equipPct << '%'; WriteLine(out.str()); }
            WriteLine(X(s++) + " (B) Go Back");
            bool emptyList = false;
            if (category != -1)
            {
                WriteLine();
                if (viewId == -1)
                {
                    bool any = false;
                    for (const auto& item : *list) if (item.first != "-") { any = true; break; }
                    if (!any) { WriteLine("No entries found"); emptyList = true; }
                    else
                    {
                        std::int32_t startPos = std::max(listPos - 5, 0);
                        std::int32_t endPos = std::min(std::max(listPos + 5, startPos + 10), static_cast<std::int32_t>(list->size()) - 1);
                        startPos = std::max(std::min(listPos - 5, endPos - 10), 0);
                        for (std::int32_t i = startPos; i <= endPos; ++i)
                        {
                            std::ostringstream out;
                            out << Y(i) << ' ' << std::setfill('0') << std::setw(3) << i << std::setfill(' ')
                                << ": " << list->at(static_cast<std::size_t>(i)).first;
                            WriteLine(out.str());
                        }
                    }
                }
                else
                {
                    const auto& item = list->at(static_cast<std::size_t>(viewId));
                    std::ostringstream out; out << std::setfill('0') << std::setw(3) << viewId; WriteLine(out.str());
                    WriteLine(item.first); WriteLine(); WriteLine(item.second);
                }
            }
            --s;
            ConsoleKeyInfo keyInfo = ReadKey();
            if (keyInfo.Key == ConsoleKey::Escape) return false;
            if (category == -1)
            {
                if (keyInfo.Key == ConsoleKey::B || keyInfo.Key == ConsoleKey::Spacebar && selection == s) break;
                if (keyInfo.Key == ConsoleKey::L || keyInfo.Key == ConsoleKey::Spacebar && selection == 0)
                { category = selection = 0; listPos = 0; list = &loreList; viewId = -1; }
                else if (keyInfo.Key == ConsoleKey::F || keyInfo.Key == ConsoleKey::Spacebar && selection == 1)
                { category = selection = 1; listPos = 0; list = &bioformList; viewId = -1; }
                else if (keyInfo.Key == ConsoleKey::O || keyInfo.Key == ConsoleKey::Spacebar && selection == 2)
                { category = selection = 2; listPos = 0; list = &objectList; viewId = -1; }
                else if (keyInfo.Key == ConsoleKey::E || keyInfo.Key == ConsoleKey::Spacebar && selection == 3)
                { category = selection = 3; listPos = 0; list = &equipList; viewId = -1; }
                else if (keyInfo.Key == ConsoleKey::UpArrow) { if (--selection < 0) selection = s; }
                else if (keyInfo.Key == ConsoleKey::DownArrow) { if (++selection > s) selection = 0; }
            }
            else
            {
                if (keyInfo.Key == ConsoleKey::B || keyInfo.Key == ConsoleKey::Backspace || keyInfo.Key == ConsoleKey::Enter)
                {
                    if (viewId == -1) { category = -1; listPos = -1; }
                    else viewId = -1;
                }
                else if (keyInfo.Key == ConsoleKey::Spacebar && viewId == -1)
                {
                    if (list->at(static_cast<std::size_t>(listPos)).first != "-") viewId = listPos;
                }
                else if (keyInfo.Key == ConsoleKey::UpArrow || keyInfo.Key == ConsoleKey::PageUp && viewId != -1)
                {
                    do { if (--listPos < 0) listPos = static_cast<std::int32_t>(list->size()) - 1; }
                    while ((keyInfo.Control || viewId != -1) && !emptyList && list->at(static_cast<std::size_t>(listPos)).first == "-");
                    if (viewId != -1) viewId = listPos;
                }
                else if (keyInfo.Key == ConsoleKey::DownArrow || keyInfo.Key == ConsoleKey::PageDown && viewId != -1)
                {
                    do { if (++listPos > static_cast<std::int32_t>(list->size()) - 1) listPos = 0; }
                    while ((keyInfo.Control || viewId != -1) && !emptyList && list->at(static_cast<std::size_t>(listPos)).first == "-");
                    if (viewId != -1) viewId = listPos;
                }
                else if (keyInfo.Key == ConsoleKey::PageUp && viewId == -1) listPos = (listPos - 11) % static_cast<std::int32_t>(list->size());
                else if (keyInfo.Key == ConsoleKey::PageDown && viewId == -1) listPos = (listPos + 11) % static_cast<std::int32_t>(list->size());
            }
        }
        return true;
    }

    bool Menu::ShowStoryModePrompts()
    {
        std::int32_t prompt = 0, selection = 0, planet = 0, weapon = 0, octolith = 0;
        auto X = [&](std::int32_t index) { return Mark(selection == index); };
        auto LayerName = [](std::int32_t value)
        {
            if (value == 0) return std::string("Before Boss");
            if (value == 1) return std::string("Leaving Boss");
            if (value == 2) return std::string("After Boss");
            return std::string("?");
        };
        auto Planet = [&](std::int32_t index)
        { return selection == 3 && planet == index ? "*" + std::to_string(_planets.at(static_cast<std::size_t>(index))) + "*"
            : " " + std::to_string(_planets.at(static_cast<std::size_t>(index))) + " "; };
        auto Weapon = [&](std::int32_t index)
        {
            std::int32_t highlight = index;
            if (highlight == 1) highlight = 2; else if (highlight == 2) highlight = 1;
            return selection == 16 && weapon == highlight ? "*" + std::to_string(_weapons.at(static_cast<std::size_t>(index))) + "*"
                : " " + std::to_string(_weapons.at(static_cast<std::size_t>(index))) + " ";
        };
        auto Octolith = [&](std::int32_t index)
        { return selection == 17 && octolith == index ? "*" + std::to_string(_octoliths.at(static_cast<std::size_t>(index))) + "*"
            : " " + std::to_string(_octoliths.at(static_cast<std::size_t>(index))) + " "; };

        while (true)
        {
            const std::string saveSlot = SaveSlot == 0 ? "none" : std::to_string(SaveSlot);
            const std::string planets = "CA:" + Planet(0) + ", Alinos:" + Planet(1) + ", VDO:" + Planet(2)
                + ", Arcterra:" + Planet(3) + ", Oubliette:" + Planet(4);
            const std::string weapons = "PB:" + Weapon(0) + ", MI:" + Weapon(2) + ", VD:" + Weapon(1) + ", BH:" + Weapon(3)
                + ", IM:" + Weapon(4) + ", JD:" + Weapon(5) + ", MG:" + Weapon(6) + ", SC:" + Weapon(7) + ", OC:" + Weapon(8);
            const std::string octoliths = "CA:" + Octolith(0) + Octolith(1) + ", Alinos:" + Octolith(2) + Octolith(3)
                + ", VDO:" + Octolith(4) + Octolith(5) + ", Arcterra:" + Octolith(6) + Octolith(7);
            std::int32_t s = 0;
            ClearConsole(); WriteLine(ProgramVersionBanner()); WriteLine();
            WriteLine("Choose a setting using up/down or with the key indicated.");
            WriteLine("Press Space to specify, Backspace to clear, or left/right to advance the setting.");
            WriteLine("When finished, press Enter or use the last option to return. Press Escape to exit.");
            WriteLine(); WriteLine("Adventure Mode Settings"); WriteLine();
            WriteLine(X(s++) + " (S) Save Slot: " + saveSlot);
            WriteLine(X(s++) + " (E) Save From Exit: " + SaveWhenString(SaveFromExit));
            WriteLine(X(s++) + " (G) Save From Ship: " + SaveWhenString(SaveFromShip));
            if (SaveSlot == 0)
            {
                WriteLine(X(s++) + " (A) Areas: " + planets);
                WriteLine(X(s++) + " (1) CA 1 State: " + LayerName(_ca1State));
                WriteLine(X(s++) + " (2) CA 2 State: " + LayerName(_ca2State));
                WriteLine(X(s++) + " (3) Alinos 1 State: " + LayerName(_alinos1State));
                WriteLine(X(s++) + " (4) Alinos 2 State: " + LayerName(_alinos2State));
                WriteLine(X(s++) + " (5) VDO 1 State: " + LayerName(_vdo1State));
                WriteLine(X(s++) + " (6) VDO 2 State: " + LayerName(_vdo2State));
                WriteLine(X(s++) + " (7) Arcterra 1 State: " + LayerName(_arcterra1State));
                WriteLine(X(s++) + " (8) Arcterra 2 State: " + LayerName(_arcterra2State));
                WriteLine(X(s++) + " (C) Checkpoint ID: " + std::to_string(_checkpointId));
                WriteLine(X(s++) + " (H) Health Max: " + std::to_string(_healthMax));
                WriteLine(X(s++) + " (M) Missile Max: " + std::to_string(_missileMax));
                WriteLine(X(s++) + " (U) UA Max: " + std::to_string(_uaMax));
                WriteLine(X(s++) + " (W) Weapons: " + weapons);
                WriteLine(X(s++) + " (O) Octoliths: " + octoliths);
                WriteLine(X(s++) + " (X) Reset Adventure Settings");
            }
            else WriteLine(X(s++) + " (L) View Logbook");
            WriteLine(X(s++) + " (B) Go Back");
            if (SaveSlot != 0)
            {
                WriteLine(); WriteLine("Save Info"); WriteLine(_saveInfo[1]); WriteLine(_saveInfo[0]);
                for (std::size_t i = 2; i < _saveInfo.size(); ++i) WriteLine(_saveInfo[i]);
            }
            --s;
            if (prompt == 0)
            {
                ConsoleKeyInfo keyInfo = ReadKey();
                if (keyInfo.Key == ConsoleKey::Escape) return false;
                if (keyInfo.Key == ConsoleKey::Enter || keyInfo.Key == ConsoleKey::B
                    || keyInfo.Key == ConsoleKey::Spacebar && selection == s) break;
                if (keyInfo.Key == ConsoleKey::Spacebar && selection == 0) prompt = 1;
                else if (keyInfo.Key == ConsoleKey::Spacebar && selection == s - 1 && SaveSlot != 0) prompt = 2;
                else if (keyInfo.Key == ConsoleKey::S) selection = 0;
                else if (keyInfo.Key == ConsoleKey::E) selection = 1;
                else if (keyInfo.Key == ConsoleKey::G) selection = 2;
                else if (keyInfo.Key == ConsoleKey::L && SaveSlot != 0) { selection = s - 1; prompt = 2; }
                else if (keyInfo.Key == ConsoleKey::UpArrow) { if (--selection < 0) selection = s; }
                else if (keyInfo.Key == ConsoleKey::DownArrow) { if (++selection > s) selection = 0; }
                else if (keyInfo.Key == ConsoleKey::Backspace || keyInfo.Key == ConsoleKey::Delete)
                {
                    if (selection == 0) { SaveSlot = 0; UpdateSaveInfo(); }
                    else if (selection == 1) SaveFromExit = static_cast<SaveWhen>(0);
                    else if (selection == 2) SaveFromShip = static_cast<SaveWhen>(2);
                    if (SaveSlot != 0) continue;
                    if (selection == 3) { _planets.fill(0); _planets[0] = 1; }
                    else if (selection == 12) _checkpointId = -1;
                    else if (selection == 13) _healthMax = 99;
                    else if (selection == 14) _missileMax = 50;
                    else if (selection == 15) _uaMax = 400;
                    else if (selection == 16) { _weapons.fill(0); _weapons[0] = 1; _weapons[2] = 1; }
                    else if (selection == 17) _octoliths.fill(0);
                }
                else if (keyInfo.Key == ConsoleKey::Add || keyInfo.Key == ConsoleKey::OemPlus || keyInfo.Key == ConsoleKey::RightArrow
                    || keyInfo.Key == ConsoleKey::Subtract || keyInfo.Key == ConsoleKey::OemMinus || keyInfo.Key == ConsoleKey::LeftArrow)
                {
                    const std::int32_t direction = keyInfo.Key == ConsoleKey::Add || keyInfo.Key == ConsoleKey::OemPlus
                        || keyInfo.Key == ConsoleKey::RightArrow ? 1 : -1;
                    if (selection == 0) { SaveSlot = static_cast<std::uint8_t>(AdvanceInt(SaveSlot, direction, 255)); UpdateSaveInfo(); }
                    else if (selection == 1) SaveFromExit = static_cast<SaveWhen>((static_cast<std::int32_t>(SaveFromExit) + 1) % 3);
                    else if (selection == 2) SaveFromShip = static_cast<SaveWhen>((static_cast<std::int32_t>(SaveFromShip) + 1) % 3);
                    if (SaveSlot != 0) continue;
                    if (selection == 3) planet = AdvanceInt(planet, direction, 4);
                    else if (selection == 4) _ca1State = AdvanceInt(_ca1State, direction, 2);
                    else if (selection == 5) _ca2State = AdvanceInt(_ca2State, direction, 2);
                    else if (selection == 6) _alinos1State = AdvanceInt(_alinos1State, direction, 2);
                    else if (selection == 7) _alinos2State = AdvanceInt(_alinos2State, direction, 2);
                    else if (selection == 8) _vdo1State = AdvanceInt(_vdo1State, direction, 2);
                    else if (selection == 9) _vdo2State = AdvanceInt(_vdo2State, direction, 2);
                    else if (selection == 10) _arcterra1State = AdvanceInt(_arcterra1State, direction, 2);
                    else if (selection == 11) _arcterra2State = AdvanceInt(_arcterra2State, direction, 2);
                    else if (selection == 12) { if (_checkpointId >= 0 || direction == 1) _checkpointId += direction; }
                    else if (selection == 13) { if (direction == -1 && _healthMax > 99 || direction == 1 && _healthMax < 799) _healthMax += direction * 100; }
                    else if (selection == 14) { if (direction == -1 && _missileMax > 50 || direction == 1 && _missileMax < 950) _missileMax += direction * 100; }
                    else if (selection == 15) { if (direction == -1 && _uaMax > 400 || direction == 1 && _uaMax < 4000) _uaMax += direction * 300; }
                    else if (selection == 16) weapon = AdvanceInt(weapon, direction, 8);
                    else if (selection == 17) octolith = AdvanceInt(octolith, direction, 7);
                }
                if (SaveSlot != 0) continue;
                if (keyInfo.Key == ConsoleKey::Spacebar)
                {
                    if (selection == s - 1)
                    {
                        _planets.fill(0); _planets[0] = 1;
                        _alinos1State = _alinos2State = _ca1State = _ca2State = 0;
                        _vdo1State = _vdo2State = _arcterra1State = _arcterra2State = 0;
                        _checkpointId = -1; _healthMax = 99; _missileMax = 50; _uaMax = 400;
                        _weapons.fill(0); _weapons[0] = 1; _weapons[2] = 1; _octoliths.fill(0);
                        ::UpdateSettings();
                    }
                    else if (selection == 3) _planets.at(static_cast<std::size_t>(planet)) = (_planets.at(static_cast<std::size_t>(planet)) + 1) % 2;
                    else if (selection == 16)
                    {
                        std::int32_t highlight = weapon; if (highlight == 1) highlight = 2; else if (highlight == 2) highlight = 1;
                        auto& value = _weapons.at(static_cast<std::size_t>(highlight)); value = (value + 1) % 2;
                    }
                    else if (selection == 17) { auto& value = _octoliths.at(static_cast<std::size_t>(octolith)); value = (value + 1) % 2; }
                }
                else if (keyInfo.Key == ConsoleKey::A) selection = 3;
                else if (keyInfo.Key == ConsoleKey::D1 || keyInfo.Key == ConsoleKey::NumPad1) selection = 4;
                else if (keyInfo.Key == ConsoleKey::D2 || keyInfo.Key == ConsoleKey::NumPad2) selection = 5;
                else if (keyInfo.Key == ConsoleKey::D3 || keyInfo.Key == ConsoleKey::NumPad3) selection = 6;
                else if (keyInfo.Key == ConsoleKey::D4 || keyInfo.Key == ConsoleKey::NumPad4) selection = 7;
                else if (keyInfo.Key == ConsoleKey::D5 || keyInfo.Key == ConsoleKey::NumPad5) selection = 8;
                else if (keyInfo.Key == ConsoleKey::D6 || keyInfo.Key == ConsoleKey::NumPad6) selection = 9;
                else if (keyInfo.Key == ConsoleKey::D7 || keyInfo.Key == ConsoleKey::NumPad7) selection = 10;
                else if (keyInfo.Key == ConsoleKey::D8 || keyInfo.Key == ConsoleKey::NumPad8) selection = 11;
                else if (keyInfo.Key == ConsoleKey::C) selection = 12;
                else if (keyInfo.Key == ConsoleKey::H) selection = 13;
                else if (keyInfo.Key == ConsoleKey::M) selection = 14;
                else if (keyInfo.Key == ConsoleKey::U) selection = 15;
                else if (keyInfo.Key == ConsoleKey::W) selection = 16;
                else if (keyInfo.Key == ConsoleKey::O) selection = 17;
                else if (keyInfo.Key == ConsoleKey::X) selection = 18;
            }
            else if (prompt == 1)
            {
                WriteLine(); WriteLine("Enter save slot from 1 to 255.");
                auto entry = ReadLine(); std::int32_t slot = 0;
                if (entry && Int32TryParseCurrentCulture(*entry, slot) && slot >= 0 && slot <= 255)
                { SaveSlot = static_cast<std::uint8_t>(slot); UpdateSaveInfo(); }
                prompt = 0;
            }
            else if (prompt == 2)
            {
                auto save = GameState::ReadSave();
                if (!ShowLogbook(*save)) return false;
                prompt = 0;
            }
        }
        return true;
    }
}

namespace MphRead::GameStateDetail
{
    std::uint8_t MenuSaveSlot()
    {
        return Menu::SaveSlot;
    }
}

namespace MphRead::SceneSetupInterop
{
    void ApplyAdventureSettings()
    {
        Menu::ApplyAdventureSettings();
    }

    std::int32_t SaveSlot()
    {
        return static_cast<std::int32_t>(Menu::SaveSlot);
    }

    std::int32_t PreviousSaveSlot()
    {
        return Menu::PreviousSaveSlot;
    }

    void SetPreviousSaveSlot(std::int32_t value)
    {
        Menu::PreviousSaveSlot = value;
    }
}
