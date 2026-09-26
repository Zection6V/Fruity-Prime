#include "GamepadProfiles.hpp"

#include "GamepadAnalog.hpp"
#include "GamepadDeviceSnapshot.hpp"
#include "GamepadManager.hpp"
#include "GamepadOptionState.hpp"
#include "GamepadOptions.hpp"
#include "GamepadRuntimeConfig.hpp"
#include "PadBindingState.hpp"
#include "PadBindings.hpp"
#include "../Launcher/Portable/LauncherPrefs.hpp"
#include "../../NativeRuntime/System/Exceptions.hpp"
#include "../../NativeRuntime/System/Globalization.hpp"
#include "../../NativeRuntime/System/Guid.hpp"
#include "../../NativeRuntime/System/IO.hpp"
#include "../../NativeRuntime/System/Json.hpp"
#include "../../NativeRuntime/System/Number.hpp"
#include "../../NativeRuntime/System/Runtime.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <set>

namespace MphRead::Mods::Input
{
    namespace Runtime = ::MphRead::NativeRuntime;
    using System::IO::InvalidDataException;
    using System::Text::Json::JsonException;
    using Runtime::JsonPtr;
    using Runtime::JsonValue;

    namespace
    {
        constexpr std::array<std::string_view, 5> Presets{"Default", "Bumper Jumper", "Southpaw", "Classic", "Custom"};

        [[nodiscard]] bool IsPreset(std::string_view value)
        {
            return std::find(Presets.begin(), Presets.end(), value) != Presets.end();
        }

        [[nodiscard]] bool HasControl(std::string_view text)
        {
            return std::any_of(text.begin(), text.end(), [](char c)
            {
                return static_cast<unsigned char>(c) < 0x20 || c == 0x7f;
            });
        }

        [[nodiscard]] bool IsNullOrWhiteSpace(std::string_view text)
        {
            return Runtime::StringIsNullOrWhiteSpace(std::string(text));
        }

        // JsonSerializer.Deserialize<T>: a property of the wrong kind is a
        // JsonException; a missing one keeps its default.
        [[nodiscard]] std::string ReadString(const JsonPtr& value)
        {
            if (value == nullptr || value->Type() == JsonValue::Kind::Null)
            {
                return {};
            }
            if (value->Type() != JsonValue::Kind::String)
            {
                throw JsonException("The JSON value could not be converted to System.String.");
            }
            return value->Text();
        }

        [[nodiscard]] std::optional<GamepadProfile> ReadProfile(const JsonPtr& value)
        {
            if (value == nullptr || value->Type() == JsonValue::Kind::Null)
            {
                return std::nullopt;
            }
            if (value->Type() != JsonValue::Kind::Object)
            {
                throw JsonException("The JSON value could not be converted to GamepadProfile.");
            }
            GamepadProfile profile{};
            if (const JsonPtr version = value->Get("Version"))
            {
                double number = 0;
                if (version->Type() != JsonValue::Kind::Number
                    || !Runtime::DoubleTryParseInvariant(version->Text(), number) || number != std::floor(number))
                {
                    throw JsonException("The JSON value could not be converted to System.Int32.");
                }
                profile.Version = static_cast<std::int32_t>(number);
            }
            profile.Name = ReadString(value->Get("Name"));
            if (const JsonPtr settings = value->Get("Settings"); settings != nullptr && settings->Type() != JsonValue::Kind::Null)
            {
                if (settings->Type() != JsonValue::Kind::Array)
                {
                    throw JsonException("The JSON value could not be converted to System.String[].");
                }
                profile.Settings = std::make_shared<std::vector<std::string>>();
                for (const JsonPtr& item : settings->Items())
                {
                    profile.Settings->push_back(ReadString(item));
                }
            }
            return profile;
        }

        [[nodiscard]] JsonPtr WriteProfile(const GamepadProfile& profile)
        {
            JsonPtr value = JsonValue::MakeObject();
            value->Set("Version", JsonValue::MakeNumber(std::to_string(profile.Version)));
            value->Set("Name", JsonValue::MakeString(profile.Name));
            if (profile.Settings == nullptr)
            {
                value->Set("Settings", JsonValue::MakeNull());
            }
            else
            {
                JsonPtr settings = JsonValue::MakeArray();
                for (const std::string& line : *profile.Settings)
                {
                    settings->Items().push_back(JsonValue::MakeString(line));
                }
                value->Set("Settings", settings);
            }
            return value;
        }

        [[nodiscard]] JsonPtr Parse(const std::string& text)
        {
            JsonPtr root = Runtime::JsonParse(text);
            if (root == nullptr)
            {
                throw JsonException("The input is not valid JSON.");
            }
            return root;
        }

        [[nodiscard]] std::string Before(const std::string& line)
        {
            const std::size_t split = line.find('=');
            if (split == std::string::npos)
            {
                throw System::ArgumentOutOfRangeException("length");
            }
            return line.substr(0, split);
        }

        // key[4..].Split('_')[0].
        [[nodiscard]] std::string ActionName(const std::string& key)
        {
            const std::string rest = key.substr(4);
            return rest.substr(0, rest.find('_'));
        }
    }

    std::string GamepadProfiles::LibraryPath()
    {
        return Runtime::PathCombine(_directory, "controller-profiles.json");
    }

    void GamepadProfiles::Initialize()
    {
        const std::string directory = Launcher::LauncherPrefs::Directory();
        if (directory == _directory)
        {
            return;
        }
        _directory = directory;
        _library = {};
        _status = "";
        try
        {
            if (!Runtime::FileExists(LibraryPath()))
            {
                return;
            }
            if (Runtime::FileInfoLength(LibraryPath()) > 1048576)
            {
                throw InvalidDataException("Profile library is too large.");
            }
            const JsonPtr root = Parse(Runtime::FileReadAllText(LibraryPath()));
            if (root->Type() != JsonValue::Kind::Object)
            {
                throw InvalidDataException("Invalid profile library.");
            }
            GamepadProfileLibrary library{};
            bool hasProfiles = false;
            bool hasAssignments = false;
            if (const JsonPtr profiles = root->Get("Profiles"); profiles != nullptr && profiles->Type() != JsonValue::Kind::Null)
            {
                if (profiles->Type() != JsonValue::Kind::Array)
                {
                    throw JsonException("The JSON value could not be converted to List<GamepadProfile>.");
                }
                hasProfiles = true;
                for (const JsonPtr& item : profiles->Items())
                {
                    const std::optional<GamepadProfile> profile = ReadProfile(item);
                    if (!profile.has_value())
                    {
                        throw InvalidDataException("Invalid controller profile or unsupported version.");
                    }
                    library.Profiles.push_back(*profile);
                }
            }
            if (const JsonPtr assignments = root->Get("Assignments"); assignments != nullptr && assignments->Type() != JsonValue::Kind::Null)
            {
                if (assignments->Type() != JsonValue::Kind::Object)
                {
                    throw JsonException("The JSON value could not be converted to Dictionary<string, string>.");
                }
                hasAssignments = true;
                for (const auto& [key, value] : assignments->Members())
                {
                    library.Assignments[key] = ReadString(value);
                }
            }
            if (!hasProfiles || !hasAssignments || library.Profiles.size() > 32 || library.Assignments.size() > 128)
            {
                throw InvalidDataException("Invalid profile library.");
            }
            std::set<std::string> names;
            for (const GamepadProfile& profile : library.Profiles)
            {
                static_cast<void>(BuildRuntime(profile));
                if (!names.insert(profile.Name).second)
                {
                    throw InvalidDataException("Duplicate controller profile name.");
                }
            }
            for (const auto& [key, value] : library.Assignments)
            {
                if (IsNullOrWhiteSpace(key) || key.size() > 512 || !names.contains(value))
                {
                    throw InvalidDataException("Invalid controller profile assignment.");
                }
            }
            _library = std::move(library);
        }
        catch (const InvalidDataException& ex) { _status = std::string("Could not load controller profiles: ") + ex.what(); }
        catch (const System::IO::IOException& ex) { _status = std::string("Could not load controller profiles: ") + ex.what(); }
        catch (const System::UnauthorizedAccessException& ex) { _status = std::string("Could not load controller profiles: ") + ex.what(); }
        catch (const JsonException& ex) { _status = std::string("Could not load controller profiles: ") + ex.what(); }
        catch (const std::invalid_argument& ex) { _status = std::string("Could not load controller profiles: ") + ex.what(); }
        catch (const std::out_of_range& ex) { _status = std::string("Could not load controller profiles: ") + ex.what(); }
    }

    GamepadProfile GamepadProfiles::Capture(const std::string& name)
    {
        auto lines = std::make_shared<std::vector<std::string>>();
        GamepadOptions::Write(*lines);
        PadBindings::Write(*lines);
        lines->push_back("gamepad_preset=" + PadBindings::Preset());
        return GamepadProfile{1, name, lines};
    }

    void GamepadProfiles::Apply(const GamepadProfile& profile)
    {
        Validate(profile);
        std::shared_ptr<GamepadRuntimeConfig> runtime = BuildRuntime(profile);
        GamepadManager::ReplaceRuntime(runtime);
        _activeName = profile.Name;
        _revision++;
    }

    std::shared_ptr<GamepadRuntimeConfig> GamepadProfiles::BuildRuntime(const GamepadProfile& profile)
    {
        Validate(profile);
        auto runtime = std::make_shared<GamepadRuntimeConfig>();
        const std::vector<std::string>& settings = *profile.Settings;
        runtime->Options()->Load(settings);
        for (const std::string& line : settings)
        {
            const std::size_t split = line.find('=');
            if (split != std::string::npos && split > 0)
            {
                static_cast<void>(runtime->Bindings()->TryLoad(line.substr(0, split), line.substr(split + 1)));
            }
        }
        runtime->Bindings()->LoadSlots(settings);
        for (const std::string& line : settings)
        {
            const std::string key = Before(line);
            if (!key.starts_with("pad_") || !key.ends_with("_modifier"))
            {
                continue;
            }
            PadAction action{};
            if (!TryParse(ActionName(key), action))
            {
                throw System::ArgumentException("Requested value '" + ActionName(key) + "' was not found.");
            }
            const std::int32_t slot = key.ends_with("_primary_modifier") ? 0 : 1;
            GamepadButtons modifier = GamepadButtons::None;
            if (!TryParse(line.substr(key.size() + 1), modifier))
            {
                throw System::ArgumentException("Requested value was not found.");
            }
            if (runtime->Bindings()->Modifier(action, slot) != modifier)
            {
                throw InvalidDataException("A modifier requires a distinct bound button.");
            }
        }
        for (auto line = settings.rbegin(); line != settings.rend(); ++line)
        {
            if (line->starts_with("gamepad_preset="))
            {
                if (IsPreset(line->substr(15)))
                {
                    runtime->Bindings()->Preset(line->substr(15));
                }
                break;
            }
        }
        runtime->ProfileName(profile.Name);
        return runtime;
    }

    void GamepadProfiles::Validate(const GamepadProfile& profile)
    {
        if (profile.Version != 1 || IsNullOrWhiteSpace(profile.Name)
            || profile.Name.size() > 48 || HasControl(profile.Name)
            || profile.Settings == nullptr || profile.Settings->empty() || profile.Settings->size() > 256)
        {
            throw InvalidDataException("Invalid controller profile or unsupported version.");
        }
        for (const std::string& line : *profile.Settings)
        {
            if (line.size() > 256 || HasControl(line) || line.find('=') == std::string::npos
                || !(line.starts_with("pad_") || line.starts_with("gamepad_")))
            {
                throw InvalidDataException("A profile may contain only controller settings.");
            }
        }
        std::vector<std::string> defaults;
        GamepadOptionState().Write(defaults);
        std::map<std::string, std::string> types;
        for (const std::string& line : defaults)
        {
            const std::size_t split = line.find('=');
            if (!types.emplace(line.substr(0, split), line.substr(split + 1)).second)
            {
                throw System::ArgumentException("An item with the same key has already been added.");
            }
        }
        types["gamepad_deadzone"] = "0.2";
        types["gamepad_look"] = "1";
        std::set<std::string> seen;
        for (const std::string& line : *profile.Settings)
        {
            const std::size_t split = line.find('=');
            const std::string key = line.substr(0, split);
            const std::string value = line.substr(split + 1);
            if (!seen.insert(key).second)
            {
                throw InvalidDataException("Duplicate controller setting: " + key);
            }
            bool valid = false;
            if (key.starts_with("pad_"))
            {
                const std::string action = ActionName(key);
                const std::string suffix = key.substr(4 + action.size());
                PadAction parsed{};
                GamepadButtons buttons = GamepadButtons::None;
                valid = TryParse(action, parsed) && IsDefined(parsed)
                    && (suffix.empty() || suffix == "_primary" || suffix == "_secondary"
                        || suffix == "_primary_modifier" || suffix == "_secondary_modifier")
                    && TryParse(value, buttons) && (static_cast<std::int32_t>(buttons) & ~0xffff) == 0
                    && (key.find('_', 4) == std::string::npos || PadBindings::Single(buttons));
            }
            else if (key == "gamepad_preset")
            {
                valid = IsPreset(value);
            }
            else if (!types.contains(key))
            {
                valid = false;
            }
            else if (key == "gamepad_curve")
            {
                GamepadCurve curve{};
                valid = TryParse(value, curve) && IsDefined(curve);
            }
            else if (key == "gamepad_glyph_style")
            {
                GamepadFamily family{};
                valid = TryParse(value, family) && IsDefined(family);
            }
            else if (key == "gamepad_binding_modifier")
            {
                GamepadButtons modifier = GamepadButtons::None;
                valid = TryParse(value, modifier) && PadBindings::Single(modifier);
            }
            else if (key == "gamepad_wheel_order")
            {
                std::vector<std::string> parts = Runtime::StringSplit(value, ',');
                std::sort(parts.begin(), parts.end(), [](const std::string& a, const std::string& b)
                {
                    return Runtime::StringCompareCurrentCulture(a, b) < 0;
                });
                valid = parts == std::vector<std::string>{"0", "1", "2", "3", "4", "5"};
            }
            else
            {
                bool sampleFlag = false;
                bool flag = false;
                float number = 0;
                if (Runtime::BooleanTryParse(types[key], sampleFlag))
                {
                    valid = Runtime::BooleanTryParse(value, flag);
                }
                else
                {
                    valid = Runtime::SingleTryParseInvariant(value, number) && std::isfinite(number);
                }
            }
            if (!valid)
            {
                throw InvalidDataException("Invalid controller setting: " + key);
            }
        }
    }

    const GamepadProfile& GamepadProfiles::Find(const std::string& name)
    {
        for (const GamepadProfile& profile : _library.Profiles)
        {
            if (profile.Name == name)
            {
                return profile;
            }
        }
        throw InvalidDataException("Choose a saved controller profile.");
    }

    void GamepadProfiles::Save(const std::string& name)
    {
        Initialize();
        Store(Capture(Runtime::StringTrim(name)));
        _activeName = Runtime::StringTrim(name);
        GamepadRuntimeConfig::Current()->ProfileName(_activeName);
        _revision++;
    }

    void GamepadProfiles::Store(const GamepadProfile& profile)
    {
        Validate(profile);
        std::vector<GamepadProfile> profiles;
        for (const GamepadProfile& existing : _library.Profiles)
        {
            if (existing.Name != profile.Name)
            {
                profiles.push_back(existing);
            }
        }
        if (profiles.size() >= 32)
        {
            throw InvalidDataException("The library holds up to 32 profiles.");
        }
        profiles.push_back(profile);
        Commit(GamepadProfileLibrary{std::move(profiles), _library.Assignments});
    }

    void GamepadProfiles::Load(const std::string& name)
    {
        Apply(Find(name));
    }

    void GamepadProfiles::Export(const std::string& name, const std::string& path)
    {
        WriteAtomic(path, Runtime::JsonWriteIndented(WriteProfile(Find(name))));
    }

    std::string GamepadProfiles::Import(const std::string& path)
    {
        Initialize();
        if (Runtime::FileInfoLength(path) > 65536)
        {
            throw InvalidDataException("Profile file is too large.");
        }
        const std::optional<GamepadProfile> read = ReadProfile(Parse(Runtime::FileReadAllText(path)));
        if (!read.has_value())
        {
            throw InvalidDataException("Invalid controller profile.");
        }
        GamepadProfile profile = *read;
        Validate(profile);
        std::string name = profile.Name;
        std::int32_t suffix = 2;
        const auto taken = [](const std::string& candidate)
        {
            return std::any_of(_library.Profiles.begin(), _library.Profiles.end(),
                [&candidate](const GamepadProfile& p) { return p.Name == candidate; });
        };
        while (taken(name))
        {
            name = profile.Name.substr(0, std::min<std::size_t>(40, profile.Name.size())) + " " + std::to_string(suffix++);
        }
        profile.Name = name;
        Store(profile);
        _revision++;
        return name;
    }

    void GamepadProfiles::Assign(const std::string& name, const GamepadDeviceSnapshot& device)
    {
        static_cast<void>(Find(name));
        std::map<std::string, std::string> assignments = _library.Assignments;
        assignments[device.ProfileKey] = name;
        if (assignments.size() > 128)
        {
            throw InvalidDataException("Too many controller assignments.");
        }
        Commit(GamepadProfileLibrary{_library.Profiles, std::move(assignments)});
        GamepadManager::RefreshProfiles();
    }

    void GamepadProfiles::Unassign(const GamepadDeviceSnapshot& device)
    {
        std::map<std::string, std::string> assignments = _library.Assignments;
        assignments.erase(device.ProfileKey);
        Commit(GamepadProfileLibrary{_library.Profiles, std::move(assignments)});
        GamepadManager::RefreshProfiles();
    }

    std::string GamepadProfiles::DeviceKey(const std::string& id)
    {
        const std::size_t last = id.rfind(':');
        const std::size_t before = last != std::string::npos && last > 0 ? id.rfind(':', last - 1) : std::string::npos;
        const std::string stable = before != std::string::npos && before > 0 ? id.substr(0, before) : id;
        return std::string(Runtime::IsWindows() ? "windows" : Runtime::IsMacOS() ? "macos"
            : Runtime::IsAndroid() ? "android" : "linux") + ":" + stable;
    }

    std::shared_ptr<GamepadRuntimeConfig> GamepadProfiles::Resolve(const std::string& key)
    {
        const auto found = _library.Assignments.find(key);
        if (found != _library.Assignments.end())
        {
            for (const GamepadProfile& profile : _library.Profiles)
            {
                if (profile.Name == found->second)
                {
                    return BuildRuntime(profile);
                }
            }
        }
        return GamepadRuntimeConfig::Fallback()->Clone();
    }

    void GamepadProfiles::Commit(GamepadProfileLibrary library)
    {
        Runtime::DirectoryCreateDirectory(_directory);
        JsonPtr root = JsonValue::MakeObject();
        JsonPtr profiles = JsonValue::MakeArray();
        for (const GamepadProfile& profile : library.Profiles)
        {
            profiles->Items().push_back(WriteProfile(profile));
        }
        root->Set("Profiles", profiles);
        JsonPtr assignments = JsonValue::MakeObject();
        for (const auto& [key, value] : library.Assignments)
        {
            assignments->Set(key, JsonValue::MakeString(value));
        }
        root->Set("Assignments", assignments);
        WriteAtomic(LibraryPath(), Runtime::JsonWrite(root));
        _library = std::move(library);
    }

    void GamepadProfiles::WriteAtomic(const std::string& path, const std::string& text)
    {
        const std::string full = Runtime::PathGetFullPath(path);
        const std::string temp = full + "." + Runtime::Guid::NewGuid().ToString("N") + ".tmp";
        try
        {
            Runtime::FileWriteAllText(temp, text);
            Runtime::FileMove(temp, full, true);
        }
        catch (...)
        {
            if (Runtime::FileExists(temp))
            {
                Runtime::FileDelete(temp);
            }
            throw;
        }
        if (Runtime::FileExists(temp))
        {
            Runtime::FileDelete(temp);
        }
    }
}
