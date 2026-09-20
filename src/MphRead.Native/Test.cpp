#include "Test.hpp"

#include "Entities/CamSeq/CameraSequence.hpp"
#include "Formats/EntityEnemy.hpp"
#include "Metadata/Rooms.hpp"
#include "Read.hpp"

#include <algorithm>
#include <bit>
#include <csignal>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace MphRead
{
    // Exact declaration seam for the already-owned Metadata counterpart.
    // Read.hpp brings in Formats.hpp, which still conflicts with Metadata.hpp
    // over PaletteData, so mirror the existing Read.cpp seam rather than
    // inventing another runtime surface.
    enum class MdlSuffix : std::int32_t
    {
        None,
        All,
        Model
    };

    enum class MetaDir : std::int32_t
    {
        Models,
        Hud,
        Stage,
        MainMenu,
        Logo,
        CharSelect,
        CreateJoin,
        GameOption,
        GamersCard,
        Keyboard,
        Keypad,
        MoviePlayer,
        MultiMaster,
        Multiplayer,
        PaxControls,
        Popup,
        Results,
        ScStartGame,
        StartGame,
        ToStart,
        TouchToStart,
        TouchToStart2,
        WifiCreate,
        WifiGames
    };

    class RecolorMetadata
    {
    public:
        const std::string Name;
        const std::string ModelPath;
        const std::string TexturePath;
        const std::string PalettePath;
        const std::optional<std::string> ReplacePath;
        const std::map<int, std::vector<int>> ReplaceIds;

        RecolorMetadata(std::string name, std::string modelPath);
        RecolorMetadata(std::string name, std::string modelPath, std::string texturePath);
        RecolorMetadata(std::string name, std::string modelPath, std::string texturePath,
            std::string palettePath, std::map<int, std::vector<int>> replaceIds = {},
            bool separateReplace = false);
    };

    class ModelMetadata
    {
        struct Values
        {
            std::string Name;
            std::string ModelPath;
            std::optional<std::string> AnimationPath;
            std::optional<std::string> AnimationShare;
            std::optional<std::string> CollisionPath;
            std::optional<std::string> ExtraCollisionPath;
            std::vector<RecolorMetadata> Recolors;
            bool UseLightSources = false;
            bool FirstHunt = false;
        };

        explicit ModelMetadata(Values values);

    public:
        const std::string Name;
        const std::string ModelPath;
        const std::optional<std::string> AnimationPath;
        const std::optional<std::string> AnimationShare;
        const std::optional<std::string> CollisionPath;
        const std::optional<std::string> ExtraCollisionPath;
        const std::vector<RecolorMetadata> Recolors;
        const bool UseLightSources;
        const bool FirstHunt;

        ModelMetadata(std::string name, std::string modelPath,
            std::optional<std::string> animationPath, std::optional<std::string> collisionPath,
            std::vector<RecolorMetadata> recolors,
            std::optional<std::string> animationShare = std::nullopt,
            bool useLightSources = false);
        ModelMetadata(std::string name, MetaDir dir,
            std::optional<std::string> anim = std::nullopt);
        ModelMetadata(std::string name, std::string texturePath, MetaDir dir);
        ModelMetadata(std::string name, std::optional<std::string> animationPath,
            std::optional<std::string> texturePath = std::nullopt);
        ModelMetadata(std::string name, std::string remove, bool animation = true,
            std::optional<std::string> animationPath = std::nullopt,
            bool collision = false, bool firstHunt = false);
        ModelMetadata(std::string name, std::vector<std::string> recolors,
            std::optional<std::string> remove = std::nullopt, bool animation = false,
            std::optional<std::string> animationPath = std::nullopt, bool texture = false,
            MdlSuffix mdlSuffix = MdlSuffix::None,
            std::optional<std::string> archive = std::nullopt,
            std::optional<std::string> recolorName = std::nullopt,
            std::optional<std::string> animationShare = std::nullopt,
            bool useLightSources = false, bool firstHunt = false,
            bool noUnderscore = false);
        ModelMetadata(std::string name, bool animation = true, bool collision = false,
            bool texture = false, std::optional<std::string> share = std::nullopt,
            MdlSuffix mdlSuffix = MdlSuffix::None,
            std::optional<std::string> archive = std::nullopt,
            std::optional<std::string> addToAnim = std::nullopt,
            bool firstHunt = false,
            std::optional<std::string> animationPath = std::nullopt,
            std::optional<std::string> extraCollision = std::nullopt);
        ModelMetadata(std::string name, std::string modelPath,
            std::optional<std::string> animationPath,
            std::optional<std::string> collisionPath, bool firstHunt = false);
    };

    namespace Metadata
    {
        extern const std::unordered_map<std::string, ::MphRead::ModelMetadata> ModelMetadata;
        extern const std::unordered_map<std::string, ::MphRead::ModelMetadata> FirstHuntModels;

        [[nodiscard]] const ::MphRead::RoomMetadata* GetRoomById(
            int id, bool noThrow = false);
    }
}

namespace
{
    using namespace MphRead;

    struct Vector4iValue final
    {
        std::int32_t X = 0;
        std::int32_t Y = 0;
        std::int32_t Z = 0;
        std::int32_t W = 0;
    };

    [[nodiscard]] constexpr std::string_view EnvironmentNewLine() noexcept
    {
#if defined(_WIN32)
        return "\r\n";
#else
        return "\n";
#endif
    }

    void WriteConsoleLine(std::string_view value)
    {
        static std::recursive_mutex mutex;
        const std::lock_guard<std::recursive_mutex> guard(mutex);

        std::ostream& output = std::cout;
        if (!value.empty())
        {
            output.write(value.data(), static_cast<std::streamsize>(value.size()));
        }
        const std::string_view newLine = EnvironmentNewLine();
        output.write(newLine.data(), static_cast<std::streamsize>(newLine.size()));
        output.flush();
        if (!output)
        {
            throw std::ios_base::failure("Failed to write Console output.");
        }
    }

    void WriteLine(const std::string& value)
    {
        WriteConsoleLine(value);
    }

    void WriteLine()
    {
        WriteConsoleLine({});
    }

#if defined(DEBUG)
    [[noreturn]] void DebugAssertFailedFallback() noexcept
    {
        std::abort();
    }

    void DebugAssertFallback(bool condition) noexcept
    {
        if (!condition)
        {
            DebugAssertFailedFallback();
        }
    }
#define MPH_TEST_DEBUG_ASSERT(condition) DebugAssertFallback(condition)
#else
#define MPH_TEST_DEBUG_ASSERT(condition) do { } while (false)
#endif

    void DebuggerBreakFallback()
    {
#if defined(_MSC_VER)
        __debugbreak();
#else
        std::raise(SIGTRAP);
#endif
    }

    template <typename T>
    [[nodiscard]] const T& Require(const std::shared_ptr<T>& value)
    {
        if (!value)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

    template <typename T>
    [[nodiscard]] const T& Require(const T* value)
    {
        if (value == nullptr)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

    template <typename T>
    [[nodiscard]] const T& VectorAt(const std::vector<T>& values, std::int32_t index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= values.size())
        {
            throw std::out_of_range(
                "Index was out of range. Must be non-negative and less than the size of the collection.");
        }
        return values[static_cast<std::size_t>(index)];
    }

    template <typename T>
    [[nodiscard]] const T& VectorAt(const std::vector<T>& values, std::size_t index)
    {
        if (index >= values.size())
        {
            throw std::out_of_range("Index was outside the bounds of the array.");
        }
        return values[index];
    }

    template <typename T>
    [[nodiscard]] const EntityOf<T>& CastEntity(const std::shared_ptr<Entity>& entity)
    {
        Require(entity);
        const std::shared_ptr<EntityOf<T>> cast = std::dynamic_pointer_cast<EntityOf<T>>(entity);
        if (!cast)
        {
            throw std::runtime_error("Specified cast is not valid.");
        }
        return *cast;
    }

    [[nodiscard]] constexpr std::int32_t ManagedInt32(std::uint32_t value) noexcept
    {
        return std::bit_cast<std::int32_t>(value);
    }

    [[nodiscard]] constexpr std::uint32_t ManagedUInt32(std::int32_t value) noexcept
    {
        return std::bit_cast<std::uint32_t>(value);
    }

    [[nodiscard]] constexpr std::int32_t UncheckedAdd(
        std::int32_t left, std::int32_t right) noexcept
    {
        return ManagedInt32(ManagedUInt32(left) + ManagedUInt32(right));
    }

    [[nodiscard]] constexpr std::int32_t UncheckedSubtract(
        std::int32_t left, std::int32_t right) noexcept
    {
        return ManagedInt32(ManagedUInt32(left) - ManagedUInt32(right));
    }

    [[nodiscard]] constexpr std::int32_t UncheckedMultiply(
        std::int32_t left, std::int32_t right) noexcept
    {
        return ManagedInt32(ManagedUInt32(left) * ManagedUInt32(right));
    }

    [[nodiscard]] constexpr std::int32_t UncheckedShiftLeft(
        std::int32_t value, std::int32_t count) noexcept
    {
        return ManagedInt32(
            ManagedUInt32(value) << (static_cast<std::uint32_t>(count) & 0x1FU));
    }

    [[nodiscard]] std::int32_t ManagedAbs(std::int32_t value)
    {
        if (value == std::numeric_limits<std::int32_t>::min())
        {
            throw System::OverflowException();
        }
        return value < 0 ? -value : value;
    }

    [[nodiscard]] std::int32_t SignExtend16(std::uint32_t value) noexcept
    {
        std::uint32_t result = value & 0xFFFFU;
        if ((result & 0x8000U) != 0)
        {
            result |= 0xFFFF0000U;
        }
        return ManagedInt32(result);
    }

    [[nodiscard]] std::int32_t SignExtend10(std::uint32_t value) noexcept
    {
        std::uint32_t result = value & 0x3FFU;
        if ((result & 0x200U) != 0)
        {
            result |= 0xFFFFFC00U;
        }
        return ManagedInt32(result);
    }

    [[nodiscard]] std::string MessageToString(Message value)
    {
        switch (value)
        {
        case Message::None: return "None";
        case Message::SetActive: return "SetActive";
        case Message::Destroyed: return "Destroyed";
        case Message::Damage: return "Damage";
        case Message::Trigger: return "Trigger";
        case Message::UpdateMusic: return "UpdateMusic";
        case Message::Gravity: return "Gravity";
        case Message::Unlock: return "Unlock";
        case Message::Lock: return "Lock";
        case Message::Activate: return "Activate";
        case Message::Complete: return "Complete";
        case Message::Impact: return "Impact";
        case Message::Death: return "Death";
        case Message::Unused22: return "Unused22";
        case Message::ShipHatch: return "ShipHatch";
        case Message::Unused24: return "Unused24";
        case Message::Unused25: return "Unused25";
        case Message::ShowPrompt: return "ShowPrompt";
        case Message::ShowWarning: return "ShowWarning";
        case Message::ShowOverlay: return "ShowOverlay";
        case Message::MoveItemSpawner: return "MoveItemSpawner";
        case Message::SetCamSeqAi: return "SetCamSeqAi";
        case Message::PlayerCollideWith: return "PlayerCollideWith";
        case Message::BeamCollideWith: return "BeamCollideWith";
        case Message::UnlockConnectors: return "UnlockConnectors";
        case Message::LockConnectors: return "LockConnectors";
        case Message::PreventFormSwitch: return "PreventFormSwitch";
        case Message::Gorea2Trigger: return "Gorea2Trigger";
        case Message::SetTriggerState: return "SetTriggerState";
        case Message::ClearTriggerState: return "ClearTriggerState";
        case Message::PlatformWakeup: return "PlatformWakeup";
        case Message::PlatformSleep: return "PlatformSleep";
        case Message::DripMoatPlatform: return "DripMoatPlatform";
        case Message::ActivateTurret: return "ActivateTurret";
        case Message::DecreaseTurretLights: return "DecreaseTurretLights";
        case Message::IncreaseTurretLights: return "IncreaseTurretLights";
        case Message::DeactivateTurret: return "DeactivateTurret";
        case Message::SetBeamReflection: return "SetBeamReflection";
        case Message::SetPlatformIndex: return "SetPlatformIndex";
        case Message::PlaySfxScript: return "PlaySfxScript";
        case Message::UnlockOubliette: return "UnlockOubliette";
        case Message::Checkpoint: return "Checkpoint";
        case Message::EscapeUpdate1: return "EscapeUpdate1";
        case Message::SetSeekPlayerY: return "SetSeekPlayerY";
        case Message::LoadOubliette: return "LoadOubliette";
        case Message::EscapeUpdate2: return "EscapeUpdate2";
        }
        return std::to_string(static_cast<std::uint32_t>(value));
    }

    [[nodiscard]] std::string EntityTypeToString(EntityType value)
    {
        switch (value)
        {
        case EntityType::Platform: return "Platform";
        case EntityType::Object: return "Object";
        case EntityType::PlayerSpawn: return "PlayerSpawn";
        case EntityType::Door: return "Door";
        case EntityType::ItemSpawn: return "ItemSpawn";
        case EntityType::ItemInstance: return "ItemInstance";
        case EntityType::EnemySpawn: return "EnemySpawn";
        case EntityType::TriggerVolume: return "TriggerVolume";
        case EntityType::AreaVolume: return "AreaVolume";
        case EntityType::JumpPad: return "JumpPad";
        case EntityType::PointModule: return "PointModule";
        case EntityType::MorphCamera: return "MorphCamera";
        case EntityType::OctolithFlag: return "OctolithFlag";
        case EntityType::FlagBase: return "FlagBase";
        case EntityType::Teleporter: return "Teleporter";
        case EntityType::NodeDefense: return "NodeDefense";
        case EntityType::LightSource: return "LightSource";
        case EntityType::Artifact: return "Artifact";
        case EntityType::CameraSequence: return "CameraSequence";
        case EntityType::ForceField: return "ForceField";
        case EntityType::BeamEffect: return "BeamEffect";
        case EntityType::Bomb: return "Bomb";
        case EntityType::EnemyInstance: return "EnemyInstance";
        case EntityType::Halfturret: return "Halfturret";
        case EntityType::Player: return "Player";
        case EntityType::BeamProjectile: return "BeamProjectile";
        case EntityType::ListHead: return "ListHead";
        case EntityType::FhUnknown0: return "FhUnknown0";
        case EntityType::FhPlayerSpawn: return "FhPlayerSpawn";
        case EntityType::FhUnknown2: return "FhUnknown2";
        case EntityType::FhDoor: return "FhDoor";
        case EntityType::FhItemSpawn: return "FhItemSpawn";
        case EntityType::FhItemInstance: return "FhItemInstance";
        case EntityType::FhEnemySpawn: return "FhEnemySpawn";
        case EntityType::FhEffectInstance: return "FhEffectInstance";
        case EntityType::FhBomb: return "FhBomb";
        case EntityType::FhTriggerVolume: return "FhTriggerVolume";
        case EntityType::FhAreaVolume: return "FhAreaVolume";
        case EntityType::FhPlatform: return "FhPlatform";
        case EntityType::FhJumpPad: return "FhJumpPad";
        case EntityType::FhPointModule: return "FhPointModule";
        case EntityType::FhMorphCamera: return "FhMorphCamera";
        case EntityType::FhEnemyInstance: return "FhEnemyInstance";
        case EntityType::FhPlayer: return "FhPlayer";
        case EntityType::FhBeamProjectile: return "FhBeamProjectile";
        case EntityType::Room: return "Room";
        case EntityType::Model: return "Model";
        case EntityType::All: return "All";
        }
        return std::to_string(static_cast<std::uint16_t>(value));
    }

    [[nodiscard]] bool EndsWith(std::string_view value, std::string_view suffix) noexcept
    {
        return value.size() >= suffix.size()
            && value.substr(value.size() - suffix.size()) == suffix;
    }

    [[nodiscard]] std::string GetFileName(const std::string& path)
    {
        return std::filesystem::path(path).filename().string();
    }

    [[nodiscard]] std::string ReplaceAll(
        std::string value, std::string_view from, std::string_view to)
    {
        if (from.empty())
        {
            return value;
        }

        std::size_t position = 0;
        while ((position = value.find(from, position)) != std::string::npos)
        {
            value.replace(position, from.size(), to);
            position += to.size();
        }
        return value;
    }

    [[nodiscard]] std::string ManagedToLower(std::string value)
    {
        // System.String.ToLower is culture-sensitive. This preserves the ASCII
        // filenames and suffixes used by the game data; full managed culture
        // casing remains owned by the external runtime.
        for (char& c : value)
        {
            if (c >= 'A' && c <= 'Z')
            {
                c = static_cast<char>(c - 'A' + 'a');
            }
        }
        return value;
    }

    [[nodiscard]] std::vector<std::uint8_t> FileReadAllBytes(const std::string& path)
    {
        std::ifstream stream(path, std::ios::binary | std::ios::ate);
        if (!stream)
        {
            std::error_code error;
            if (!std::filesystem::exists(path, error) && !error)
            {
                throw System::IO::FileNotFoundException(path);
            }
            throw System::IO::IOException("I/O error occurred.");
        }

        const std::streampos end = stream.tellg();
        if (end < std::streampos(0))
        {
            throw System::IO::IOException("I/O error occurred.");
        }

        const auto size = static_cast<std::uintmax_t>(end);
        if (size > static_cast<std::uintmax_t>(
                std::numeric_limits<std::size_t>::max()))
        {
            throw System::OutOfMemoryException();
        }

        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
        stream.seekg(0, std::ios::beg);
        if (!bytes.empty())
        {
            stream.read(
                reinterpret_cast<char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
        }
        if (!stream)
        {
            throw System::IO::IOException("I/O error occurred.");
        }
        return bytes;
    }

    [[nodiscard]] std::vector<std::string> EnumerateFiles(
        const std::string& path, bool recursive)
    {
        std::vector<std::string> result;
        std::error_code error;

        if (recursive)
        {
            std::filesystem::recursive_directory_iterator iterator(path, error);
            if (error)
            {
                throw System::IO::DirectoryNotFoundException(path);
            }
            const std::filesystem::recursive_directory_iterator end;
            for (; iterator != end; iterator.increment(error))
            {
                if (error)
                {
                    throw System::IO::IOException(error.message());
                }
                std::error_code typeError;
                if (iterator->is_regular_file(typeError))
                {
                    result.push_back(iterator->path().string());
                }
                else if (typeError)
                {
                    throw System::IO::IOException(typeError.message());
                }
            }
        }
        else
        {
            std::filesystem::directory_iterator iterator(path, error);
            if (error)
            {
                throw System::IO::DirectoryNotFoundException(path);
            }
            const std::filesystem::directory_iterator end;
            for (; iterator != end; iterator.increment(error))
            {
                if (error)
                {
                    throw System::IO::IOException(error.message());
                }
                std::error_code typeError;
                if (iterator->is_regular_file(typeError))
                {
                    result.push_back(iterator->path().string());
                }
                else if (typeError)
                {
                    throw System::IO::IOException(typeError.message());
                }
            }
        }
        return result;
    }

    void FileWriteAllLines(
        const std::string& path, const std::vector<std::string>& lines)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            throw System::IO::IOException("I/O error occurred.");
        }

        const std::string_view newLine = EnvironmentNewLine();
        for (const std::string& line : lines)
        {
            if (!line.empty())
            {
                stream.write(line.data(), static_cast<std::streamsize>(line.size()));
            }
            stream.write(newLine.data(), static_cast<std::streamsize>(newLine.size()));
            if (!stream)
            {
                throw System::IO::IOException("I/O error occurred.");
            }
        }
        stream.flush();
        if (!stream)
        {
            throw System::IO::IOException("I/O error occurred.");
        }
        stream.close();
        if (!stream)
        {
            throw System::IO::IOException("I/O error occurred.");
        }
    }

    [[nodiscard]] NativeRuntime::CoroutineSequence<std::shared_ptr<Model>>
        EnumerateAllModels()
    {
        for (const auto& meta : Metadata::ModelMetadata)
        {
            const std::shared_ptr<ModelInstance> instance = Read::GetModelInstance(meta.first);
            co_yield Require(instance).Model();
        }
        for (const auto& meta : Metadata::FirstHuntModels)
        {
            const std::shared_ptr<ModelInstance> instance
                = Read::GetModelInstance(meta.first, true);
            co_yield Require(instance).Model();
        }
        for (const auto& meta : Metadata::RoomMetadata)
        {
            const std::shared_ptr<ModelInstance> instance
                = Read::GetRoomModelInstance(meta.first);
            co_yield Require(instance).Model();
        }
    }

    [[nodiscard]] NativeRuntime::CoroutineSequence<std::shared_ptr<Model>>
        EnumerateAllRooms()
    {
        for (const auto& meta : Metadata::RoomMetadata)
        {
            const std::shared_ptr<ModelInstance> instance
                = Read::GetRoomModelInstance(meta.first);
            co_yield Require(instance).Model();
        }
    }
}

namespace MphRead
{
    void Test::ParseAllModels()
    {
        std::vector<std::shared_ptr<Model>> models;
        for (const std::shared_ptr<Model>& model : GetAllModels())
        {
            models.push_back(model);
        }
    }

    void Test::TestAllModels()
    {
        for (const std::shared_ptr<Model>& model : GetAllModels())
        {
            (void)model;
        }
        Nop();
    }

    void Test::TestModelFiles()
    {
        std::vector<std::string> paths;
        for (const std::string& path :
            EnumerateFiles(Paths::Combine(Paths::FileSystem(), "models"), false))
        {
            if (EndsWith(path, "odel.bin"))
            {
                paths.push_back(path);
            }
        }
        for (const std::string& path :
            EnumerateFiles(Paths::Combine(Paths::FileSystem(), "_archives"), true))
        {
            if (EndsWith(path, "odel.bin"))
            {
                paths.push_back(path);
            }
        }

        std::vector<std::pair<std::string, Header>> headers;
        for (const std::string& path : paths)
        {
            const std::vector<std::uint8_t> bytes = FileReadAllBytes(path);
            const std::string name = GetFileName(path);
            const Header header = Read::ReadStruct<Header>(
                std::span<const std::uint8_t>(bytes));

            const auto duplicate = std::find_if(
                headers.begin(), headers.end(),
                [&name](const auto& pair)
                {
                    return pair.first == name;
                });
            if (duplicate != headers.end())
            {
                throw std::invalid_argument(
                    "An item with the same key has already been added. Key: " + name);
            }
            headers.emplace_back(name, header);
        }

        for (const auto& kvp : headers)
        {
            const std::string& name = kvp.first;
            const Header& h = kvp.second;
            (void)name;
            (void)h;
        }
        Nop();
    }

    void Test::TestAllRooms()
    {
        const std::vector<std::int32_t> ignore{
            1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 23, 26
        };
        for (std::int32_t i = 0; i < 122; i = UncheckedAdd(i, 1))
        {
            if (std::find(ignore.begin(), ignore.end(), i) == ignore.end())
            {
                const RoomMetadata* meta = Metadata::GetRoomById(i);
                (void)meta;
            }
        }
    }

    void Test::TestAllNodes()
    {
        for (const auto& meta : Metadata::RoomMetadata)
        {
            const std::shared_ptr<ModelInstance> instance
                = Read::GetRoomModelInstance(meta.first);
            const std::shared_ptr<Model> roomPtr = Require(instance).Model();
            const Model& room = Require(roomPtr);

            WriteLine(meta.first);
            const auto& nodes = Require(room.Nodes);
            for (std::int32_t i = 0;
                i < static_cast<std::int32_t>(nodes.size());
                i = UncheckedAdd(i, 1))
            {
                const std::shared_ptr<Node>& node
                    = VectorAt(nodes, static_cast<std::size_t>(i));
                (void)node;
            }
            WriteLine();
        }
        Nop();
    }

    void Test::TestAllEntities()
    {
        for (const auto& meta : Metadata::RoomMetadata)
        {
            const RoomMetadata& room = Require(meta.second);
            if (room.EntityPath.has_value() && !room.FirstHunt)
            {
                const std::shared_ptr<const std::vector<std::shared_ptr<Entity>>> entities
                    = Read::GetEntities(*room.EntityPath, -1, room.FirstHunt);
                for (const std::shared_ptr<Entity>& entity : Require(entities))
                {
                    const Entity& entityRef = Require(entity);
                    if (entityRef.Type == EntityType::Object)
                    {
                        const ObjectEntityData data
                            = CastEntity<ObjectEntityData>(entity).Data;
                        (void)data;
                    }
                }
            }
        }
        Nop();
    }

    void Test::TestAllEntityMessages()
    {
        using MessageUse = std::pair<std::string, std::string>;
        std::map<Message, std::vector<MessageUse>> used;

        for (const auto& meta : Metadata::RoomMetadata)
        {
            const RoomMetadata& room = Require(meta.second);
            if (room.EntityPath.has_value() && !room.FirstHunt)
            {
                const std::shared_ptr<const std::vector<std::shared_ptr<Entity>>> entities
                    = Read::GetEntities(*room.EntityPath, -1, room.FirstHunt);
                for (const std::shared_ptr<Entity>& entity : Require(entities))
                {
                    const Entity& entityRef = Require(entity);
                    const auto add = [&](Message message)
                    {
                        used[message].emplace_back(
                            EntityTypeToString(entityRef.Type), meta.first);
                    };

                    if (entityRef.Type == EntityType::Platform)
                    {
                        const PlatformEntityData data
                            = CastEntity<PlatformEntityData>(entity).Data;
                        add(data.ScanMessage);
                        add(data.LifetimeMessage1);
                        add(data.LifetimeMessage2);
                        add(data.LifetimeMessage3);
                        add(data.LifetimeMessage4);
                        add(data.BeamHitMessage);
                        add(data.DeadMessage);
                        add(data.PlayerColMessage);
                    }
                    else if (entityRef.Type == EntityType::Object)
                    {
                        const ObjectEntityData data
                            = CastEntity<ObjectEntityData>(entity).Data;
                        add(data.ScanMessage);
                    }
                    else if (entityRef.Type == EntityType::Artifact)
                    {
                        const ArtifactEntityData data
                            = CastEntity<ArtifactEntityData>(entity).Data;
                        add(data.Message1);
                        add(data.Message2);
                        add(data.Message3);
                    }
                    else if (entityRef.Type == EntityType::EnemySpawn)
                    {
                        const EnemySpawnEntityData data
                            = CastEntity<EnemySpawnEntityData>(entity).Data;
                        add(data.Message1);
                        add(data.Message2);
                        add(data.Message3);
                    }
                    else if (entityRef.Type == EntityType::ItemSpawn)
                    {
                        const ItemSpawnEntityData data
                            = CastEntity<ItemSpawnEntityData>(entity).Data;
                        add(data.CollectedMessage);
                    }
                    else if (entityRef.Type == EntityType::CameraSequence)
                    {
                        const CameraSequenceEntityData data
                            = CastEntity<CameraSequenceEntityData>(entity).Data;
                        add(data.EndMessage);
                    }
                    else if (entityRef.Type == EntityType::AreaVolume)
                    {
                        const AreaVolumeEntityData data
                            = CastEntity<AreaVolumeEntityData>(entity).Data;
                        add(data.InsideMessage);
                        add(data.ExitMessage);
                    }
                    else if (entityRef.Type == EntityType::TriggerVolume)
                    {
                        const TriggerVolumeEntityData data
                            = CastEntity<TriggerVolumeEntityData>(entity).Data;
                        add(data.ParentMessage);
                        add(data.ChildMessage);
                    }
                }
            }
        }

        const std::int32_t filenameCount
            = static_cast<std::int32_t>(Formats::CameraSequence::Filenames.Count());
        for (std::int32_t i = 0; i < filenameCount; i = UncheckedAdd(i, 1))
        {
            if (i != 8 && i != 66 && i != 69 && i != 84
                && i != 106 && i != 122 && i != 123)
            {
                const std::shared_ptr<Formats::CameraSequence> seq
                    = Formats::CameraSequence::Load(i, nullptr);
                const Formats::CameraSequence& sequence = Require(seq);
                for (const std::shared_ptr<CameraSequenceKeyframe>& frame :
                    sequence.Keyframes())
                {
                    const Message message
                        = static_cast<Message>(Require(frame).MessageId);
                    used[message].emplace_back("Keyframe", sequence.Name());
                }
            }
        }

        WriteLine("Used:");
        for (const auto& kvp : used)
        {
            WriteLine("* " + MessageToString(kvp.first));
            for (const MessageUse& value : kvp.second)
            {
                WriteLine(value.first + " in " + value.second);
            }
            WriteLine();
        }
        WriteLine();
        WriteLine("Unused:");
        for (std::int32_t i = 0; i <= 61; i = UncheckedAdd(i, 1))
        {
            const Message message
                = static_cast<Message>(static_cast<std::uint32_t>(i));
            if (used.find(message) == used.end())
            {
                WriteLine(MessageToString(message));
            }
        }
        Nop();
    }

    void Test::TestTriggerVolumes()
    {
        for (const auto& meta : Metadata::RoomMetadata)
        {
            const RoomMetadata& room = Require(meta.second);
            if (room.EntityPath.has_value())
            {
                const std::shared_ptr<const std::vector<std::shared_ptr<Entity>>> entities
                    = Read::GetEntities(*room.EntityPath, -1, room.FirstHunt);
                for (const std::shared_ptr<Entity>& entity : Require(entities))
                {
                    const Entity& entityRef = Require(entity);
                    if (entityRef.Type == EntityType::TriggerVolume)
                    {
                        const TriggerVolumeEntityData data
                            = CastEntity<TriggerVolumeEntityData>(entity).Data;
                        (void)data;
                    }
                }
            }
        }
        Nop();
    }

    void Test::TestAreaVolumes()
    {
        for (const auto& meta : Metadata::RoomMetadata)
        {
            const RoomMetadata& room = Require(meta.second);
            if (room.EntityPath.has_value())
            {
                const std::shared_ptr<const std::vector<std::shared_ptr<Entity>>> entities
                    = Read::GetEntities(*room.EntityPath, -1, room.FirstHunt);
                for (const std::shared_ptr<Entity>& entity : Require(entities))
                {
                    const Entity& entityRef = Require(entity);
                    if (entityRef.Type == EntityType::AreaVolume)
                    {
                        const AreaVolumeEntityData data
                            = CastEntity<AreaVolumeEntityData>(entity).Data;
                        (void)data;
                    }
                }
            }
        }
        Nop();
    }

    void Test::LightColor(std::uint32_t arg)
    {
        const std::uint32_t r = arg & 0x1FU;
        const std::uint32_t g = (arg >> 5U) & 0x1FU;
        const std::uint32_t b = (arg >> 10U) & 0x1FU;
        const std::int32_t light = (arg & 0x40000000U) == 0 ? 0 : 1;
        WriteLine(
            "light: " + std::to_string(light)
            + " R " + std::to_string(r)
            + ", G " + std::to_string(g)
            + ", B " + std::to_string(b));
        WriteLine();
    }

    Enumerable<std::shared_ptr<Model>> Test::GetAllModels()
    {
        return Enumerable<std::shared_ptr<Model>>([]()
        {
            return EnumerateAllModels();
        });
    }

    Enumerable<std::shared_ptr<Model>> Test::GetAllRooms()
    {
        return Enumerable<std::shared_ptr<Model>>([]()
        {
            return EnumerateAllRooms();
        });
    }

    bool Test::TestBytes(const std::string& one, const std::string& two)
    {
        const std::vector<std::uint8_t> bone = FileReadAllBytes(one);
        const std::vector<std::uint8_t> btwo = FileReadAllBytes(two);
        return bone == btwo;
    }

    void Test::WriteAllModels()
    {
        const std::string modelPath = Paths::Combine(Paths::FileSystem(), "models");
        std::vector<std::string> modelFiles;
        std::vector<std::string> textureFiles;
        std::vector<std::string> animationFiles;
        std::vector<std::string> collisionFiles;
        std::vector<std::string> unknownFiles;

        for (const std::string& path : EnumerateFiles(modelPath, true))
        {
            const std::string pathLower = ManagedToLower(path);
            if (pathLower.find("_model.bin") != std::string::npos)
            {
                modelFiles.push_back(path);
            }
            else if (pathLower.find("_tex.bin") != std::string::npos)
            {
                textureFiles.push_back(path);
            }
            else if (pathLower.find("_anim.bin") != std::string::npos)
            {
                animationFiles.push_back(path);
            }
            else if (pathLower.find("_collision.bin") != std::string::npos)
            {
                collisionFiles.push_back(path);
            }
            else
            {
                unknownFiles.push_back(path);
            }
        }

        std::vector<std::string> lines;
        lines.push_back("model (" + std::to_string(modelFiles.size()) + "):");
        {
            std::vector<std::string> ordered = modelFiles;
            std::sort(ordered.begin(), ordered.end());
            lines.insert(lines.end(), ordered.begin(), ordered.end());
        }
        lines.emplace_back();
        lines.push_back("texture (" + std::to_string(textureFiles.size()) + "):");
        {
            std::vector<std::string> ordered = textureFiles;
            std::sort(ordered.begin(), ordered.end());
            lines.insert(lines.end(), ordered.begin(), ordered.end());
        }
        lines.emplace_back();
        lines.push_back("animation (" + std::to_string(animationFiles.size()) + "):");
        {
            std::vector<std::string> ordered = animationFiles;
            std::sort(ordered.begin(), ordered.end());
            lines.insert(lines.end(), ordered.begin(), ordered.end());
        }
        lines.emplace_back();
        lines.push_back("collision (" + std::to_string(collisionFiles.size()) + "):");
        {
            std::vector<std::string> ordered = collisionFiles;
            std::sort(ordered.begin(), ordered.end());
            lines.insert(lines.end(), ordered.begin(), ordered.end());
        }
        lines.emplace_back();
        lines.push_back("unknown (" + std::to_string(unknownFiles.size()) + "):");
        {
            std::vector<std::string> ordered = unknownFiles;
            std::sort(ordered.begin(), ordered.end());
            lines.insert(lines.end(), ordered.begin(), ordered.end());
        }
        FileWriteAllLines("models.txt", lines);
        lines.clear();

        const auto addMatch = [&lines](
            std::string model, const std::string& suffix,
            std::vector<std::string>& list)
        {
            model = ReplaceAll(ManagedToLower(std::move(model)), "_lod0", "");
            model = ReplaceAll(std::move(model), "lod1", "");
            std::string match1 = ReplaceAll(
                std::move(model), "_model.bin", "_" + suffix + ".bin");
            const std::string match2 = ReplaceAll(match1, "_mdl", "");

            std::int32_t index = -1;
            for (std::size_t i = 0; i < list.size(); ++i)
            {
                if (ManagedToLower(list[i]) == match1
                    || ManagedToLower(list[i]) == match2)
                {
                    index = static_cast<std::int32_t>(i);
                    break;
                }
            }
            if (index != -1)
            {
                lines.push_back(VectorAt(list, index));
                list.erase(list.begin() + index);
            }
        };

        for (const std::string& file : modelFiles)
        {
            lines.push_back(file);
            addMatch(file, "tex", textureFiles);
            addMatch(file, "anim", animationFiles);
            addMatch(file, "collision", collisionFiles);
            lines.emplace_back();
        }

        lines.push_back("unmatched texture:");
        for (const std::string& file : textureFiles)
        {
            lines.push_back(file);
        }
        lines.emplace_back();

        lines.push_back("unmatched animation:");
        for (const std::string& file : animationFiles)
        {
            lines.push_back(file);
        }
        lines.emplace_back();

        lines.push_back("unmatched collision:");
        for (const std::string& file : collisionFiles)
        {
            lines.push_back(file);
        }
        FileWriteAllLines("matches.txt", lines);
    }

    void Test::TestDlistBounds()
    {
        for (const std::shared_ptr<Model>& modelPtr : GetAllModels())
        {
            const Model& model = Require(modelPtr);
            const auto& nodeMatrixIds = Require(model.NodeMatrixIds);
            if (!nodeMatrixIds.empty())
            {
                continue;
            }

            const auto& displayLists = Require(model.DisplayLists);
            const auto& instructionLists = Require(model.RenderInstructionLists);
            for (std::int32_t i = 0;
                i < static_cast<std::int32_t>(displayLists.size());
                i = UncheckedAdd(i, 1))
            {
                std::vector<Vector4iValue> verts;
                const DisplayList dlist
                    = VectorAt(displayLists, static_cast<std::size_t>(i));
                const std::shared_ptr<const std::vector<std::shared_ptr<RenderInstruction>>>&
                    instructionListPtr
                    = VectorAt(instructionLists, static_cast<std::size_t>(i));
                const auto& list = Require(instructionListPtr);

                std::int32_t stackIndex = 0;
                std::int32_t vtxX = 0;
                std::int32_t vtxY = 0;
                std::int32_t vtxZ = 0;
                const auto update = [&]()
                {
                    verts.push_back(Vector4iValue{vtxX, vtxY, vtxZ, stackIndex});
                };

                for (const std::shared_ptr<RenderInstruction>& instructionPtr : list)
                {
                    const RenderInstruction& instruction = Require(instructionPtr);
                    const auto& arguments = Require(instruction.Arguments);
                    switch (instruction.Code)
                    {
                    case InstructionCode::MTX_RESTORE:
                        stackIndex = ManagedInt32(VectorAt(arguments, std::size_t{0}));
                        break;
                    case InstructionCode::VTX_16:
                    {
                        const std::uint32_t xy = VectorAt(arguments, std::size_t{0});
                        const std::int32_t x = SignExtend16(xy);
                        const std::int32_t y = SignExtend16(xy >> 16U);
                        const std::int32_t z
                            = SignExtend16(VectorAt(arguments, std::size_t{1}));
                        vtxX = x;
                        vtxY = y;
                        vtxZ = z;
                        update();
                        break;
                    }
                    case InstructionCode::VTX_10:
                    {
                        const std::uint32_t xyz = VectorAt(arguments, std::size_t{0});
                        const std::int32_t x = SignExtend10(xyz);
                        const std::int32_t y = SignExtend10(xyz >> 10U);
                        const std::int32_t z = SignExtend10(xyz >> 20U);
                        vtxX = UncheckedShiftLeft(x, 6);
                        vtxY = UncheckedShiftLeft(y, 6);
                        vtxZ = UncheckedShiftLeft(z, 6);
                        update();
                        break;
                    }
                    case InstructionCode::VTX_XY:
                    {
                        const std::uint32_t xy = VectorAt(arguments, std::size_t{0});
                        vtxX = SignExtend16(xy);
                        vtxY = SignExtend16(xy >> 16U);
                        update();
                        break;
                    }
                    case InstructionCode::VTX_XZ:
                    {
                        const std::uint32_t xz = VectorAt(arguments, std::size_t{0});
                        vtxX = SignExtend16(xz);
                        vtxZ = SignExtend16(xz >> 16U);
                        update();
                        break;
                    }
                    case InstructionCode::VTX_YZ:
                    {
                        const std::uint32_t yz = VectorAt(arguments, std::size_t{0});
                        vtxY = SignExtend16(yz);
                        vtxZ = SignExtend16(yz >> 16U);
                        update();
                        break;
                    }
                    case InstructionCode::VTX_DIFF:
                    {
                        const std::uint32_t xyz = VectorAt(arguments, std::size_t{0});
                        const std::int32_t x = SignExtend10(xyz);
                        const std::int32_t y = SignExtend10(xyz >> 10U);
                        const std::int32_t z = SignExtend10(xyz >> 20U);
                        vtxX = UncheckedAdd(vtxX, x);
                        vtxY = UncheckedAdd(vtxY, y);
                        vtxZ = UncheckedAdd(vtxZ, z);
                        update();
                        break;
                    }
                    default:
                        break;
                    }
                }

                const OpenTK::Mathematics::Vector3i dlistMin(
                    dlist.MinBounds.X.Value,
                    dlist.MinBounds.Y.Value,
                    dlist.MinBounds.Z.Value);
                const OpenTK::Mathematics::Vector3i dlistMax(
                    dlist.MaxBounds.X.Value,
                    dlist.MaxBounds.Y.Value,
                    dlist.MaxBounds.Z.Value);

                std::int32_t minX = std::numeric_limits<std::int32_t>::max();
                std::int32_t maxX = std::numeric_limits<std::int32_t>::min();
                std::int32_t minY = std::numeric_limits<std::int32_t>::max();
                std::int32_t maxY = std::numeric_limits<std::int32_t>::min();
                std::int32_t minZ = std::numeric_limits<std::int32_t>::max();
                std::int32_t maxZ = std::numeric_limits<std::int32_t>::min();
                for (const Vector4iValue& vert : verts)
                {
                    minX = std::min(minX, vert.X);
                    maxX = std::max(maxX, vert.X);
                    minY = std::min(minY, vert.Y);
                    maxY = std::max(maxY, vert.Y);
                    minZ = std::min(minZ, vert.Z);
                    maxZ = std::max(maxZ, vert.Z);
                }

                const std::int32_t scale = static_cast<std::int32_t>(model.Scale.X);
                if (nodeMatrixIds.empty() && model.Name != "Level MP5")
                {
                    minX = UncheckedMultiply(minX, scale);
                    maxX = UncheckedMultiply(maxX, scale);
                    minY = UncheckedMultiply(minY, scale);
                    maxY = UncheckedMultiply(maxY, scale);
                    minZ = UncheckedMultiply(minZ, scale);
                    maxZ = UncheckedMultiply(maxZ, scale);
                }

                MPH_TEST_DEBUG_ASSERT(model.Scale.X - static_cast<float>(scale) == 0.0F);

                if (minX != dlistMin.X
                    && ManagedAbs(UncheckedSubtract(minX, dlistMin.X)) != scale)
                {
                    DebuggerBreakFallback();
                }
                if (maxX != dlistMax.X
                    && ManagedAbs(UncheckedSubtract(maxX, dlistMax.X)) != scale)
                {
                    DebuggerBreakFallback();
                }
                if (minY != dlistMin.Y
                    && ManagedAbs(UncheckedSubtract(minY, dlistMin.Y)) != scale)
                {
                    DebuggerBreakFallback();
                }
                if (maxY != dlistMax.Y
                    && ManagedAbs(UncheckedSubtract(maxY, dlistMax.Y)) != scale)
                {
                    DebuggerBreakFallback();
                }
                if (minZ != dlistMin.Z
                    && ManagedAbs(UncheckedSubtract(minZ, dlistMin.Z)) != scale)
                {
                    DebuggerBreakFallback();
                }
                if (maxZ != dlistMax.Z
                    && ManagedAbs(UncheckedSubtract(maxZ, dlistMax.Z)) != scale)
                {
                    DebuggerBreakFallback();
                }
            }
        }
        Nop();
    }

    void Test::TestNodeBounds()
    {
        for (const std::shared_ptr<Model>& modelPtr : GetAllModels())
        {
            const Model& model = Require(modelPtr);
            if (model.Name == "Level MP5")
            {
                continue;
            }

            const bool uncapped = model.Name == "filter" || model.Name == "trail";
            const auto& nodes = Require(model.Nodes);
            const auto& rawNodes = Require(model.RawNodes);
            const auto& meshes = Require(model.Meshes);
            const auto& displayLists = Require(model.DisplayLists);

            for (std::int32_t i = 0;
                i < static_cast<std::int32_t>(nodes.size());
                i = UncheckedAdd(i, 1))
            {
                const std::shared_ptr<Node>& nodePtr
                    = VectorAt(nodes, static_cast<std::size_t>(i));
                const Node& node = Require(nodePtr);
                const RawNode rawNode
                    = VectorAt(rawNodes, static_cast<std::size_t>(i));

                std::int32_t minX = std::numeric_limits<std::int32_t>::max();
                std::int32_t minY = std::numeric_limits<std::int32_t>::max();
                std::int32_t minZ = std::numeric_limits<std::int32_t>::max();
                std::int32_t maxX = std::numeric_limits<std::int32_t>::min();
                std::int32_t maxY = std::numeric_limits<std::int32_t>::min();
                std::int32_t maxZ = std::numeric_limits<std::int32_t>::min();
                bool anyMesh = false;

                std::vector<std::int32_t> ids;
                if (node.MeshCount == 0)
                {
                    for (std::int32_t id : node.GetAllMeshIds(model.Nodes, true))
                    {
                        ids.push_back(id);
                    }
                }
                else
                {
                    for (std::int32_t id : node.GetMeshIds())
                    {
                        ids.push_back(id);
                    }
                }

                std::vector<std::int32_t> dlists;
                for (std::int32_t meshId : ids)
                {
                    anyMesh = true;
                    const std::shared_ptr<Mesh>& listMeshPtr = VectorAt(meshes, meshId);
                    dlists.push_back(Require(listMeshPtr).DlistId);

                    const std::shared_ptr<Mesh>& displayMeshPtr = VectorAt(meshes, meshId);
                    const DisplayList dlist
                        = VectorAt(displayLists, Require(displayMeshPtr).DlistId);
                    minX = std::min(minX, dlist.MinBounds.X.Value);
                    minY = std::min(minY, dlist.MinBounds.Y.Value);
                    minZ = std::min(minZ, dlist.MinBounds.Z.Value);
                    maxX = std::max(maxX, dlist.MaxBounds.X.Value);
                    maxY = std::max(maxY, dlist.MaxBounds.Y.Value);
                    maxZ = std::max(maxZ, dlist.MaxBounds.Z.Value);
                }

                const OpenTK::Mathematics::Vector3i nodeMin(
                    rawNode.MinBounds.X.Value,
                    rawNode.MinBounds.Y.Value,
                    rawNode.MinBounds.Z.Value);
                const OpenTK::Mathematics::Vector3i nodeMax(
                    rawNode.MaxBounds.X.Value,
                    rawNode.MaxBounds.Y.Value,
                    rawNode.MaxBounds.Z.Value);

                const float int16Max
                    = static_cast<float>(std::numeric_limits<std::int16_t>::max());
                const float int16Min
                    = static_cast<float>(std::numeric_limits<std::int16_t>::min());

                if (anyMesh)
                {
                    if (static_cast<float>(minX) > int16Max * model.Scale.X
                        || static_cast<float>(minX) < int16Min * model.Scale.X)
                    {
                        DebuggerBreakFallback();
                    }
                    if (static_cast<float>(minY) > int16Max * model.Scale.Y
                        || static_cast<float>(minY) < int16Min * model.Scale.Y)
                    {
                        DebuggerBreakFallback();
                    }
                    if (static_cast<float>(minZ) > int16Max * model.Scale.Z
                        || static_cast<float>(minZ) < int16Min * model.Scale.Z)
                    {
                        DebuggerBreakFallback();
                    }
                    if (static_cast<float>(maxX) > int16Max * model.Scale.X
                        || static_cast<float>(maxX) < int16Min * model.Scale.X)
                    {
                        DebuggerBreakFallback();
                    }
                    if (static_cast<float>(maxY) > int16Max * model.Scale.Y
                        || static_cast<float>(maxY) < int16Min * model.Scale.Y)
                    {
                        DebuggerBreakFallback();
                    }
                    if (static_cast<float>(maxZ) > int16Max * model.Scale.Z
                        || static_cast<float>(maxZ) < int16Min * model.Scale.Z)
                    {
                        DebuggerBreakFallback();
                    }

                    if (minX != nodeMin.X)
                    {
                        if (uncapped)
                        {
                            MPH_TEST_DEBUG_ASSERT(
                                static_cast<float>(minX)
                                    == static_cast<float>(nodeMin.X) / model.Scale.X);
                        }
                        else if (static_cast<float>(nodeMin.X) > int16Max * model.Scale.X)
                        {
                            MPH_TEST_DEBUG_ASSERT(
                                static_cast<float>(minX) == int16Max * model.Scale.X);
                        }
                        else if (static_cast<float>(nodeMin.X) < int16Min * model.Scale.X)
                        {
                            MPH_TEST_DEBUG_ASSERT(
                                static_cast<float>(minX) == int16Min * model.Scale.X);
                        }
                        else
                        {
                            DebuggerBreakFallback();
                        }
                    }
                    if (minY != nodeMin.Y)
                    {
                        if (uncapped)
                        {
                            MPH_TEST_DEBUG_ASSERT(
                                static_cast<float>(minY)
                                    == static_cast<float>(nodeMin.Y) / model.Scale.Y);
                        }
                        else if (static_cast<float>(nodeMin.Y) > int16Max * model.Scale.Y)
                        {
                            MPH_TEST_DEBUG_ASSERT(
                                static_cast<float>(minY) == int16Max * model.Scale.Y);
                        }
                        else if (static_cast<float>(nodeMin.Y) < int16Min * model.Scale.Y)
                        {
                            MPH_TEST_DEBUG_ASSERT(
                                static_cast<float>(minY) == int16Min * model.Scale.Y);
                        }
                        else
                        {
                            DebuggerBreakFallback();
                        }
                    }
                    if (minZ != nodeMin.Z)
                    {
                        if (uncapped)
                        {
                            MPH_TEST_DEBUG_ASSERT(
                                static_cast<float>(minZ)
                                    == static_cast<float>(nodeMin.Z) / model.Scale.Z);
                        }
                        else if (static_cast<float>(nodeMin.Z) > int16Max * model.Scale.Z)
                        {
                            MPH_TEST_DEBUG_ASSERT(
                                static_cast<float>(minZ) == int16Max * model.Scale.Z);
                        }
                        else if (static_cast<float>(nodeMin.Z) < int16Min * model.Scale.Z)
                        {
                            MPH_TEST_DEBUG_ASSERT(
                                static_cast<float>(minZ) == int16Min * model.Scale.Z);
                        }
                        else
                        {
                            DebuggerBreakFallback();
                        }
                    }
                    if (maxX != nodeMax.X)
                    {
                        if (uncapped)
                        {
                            MPH_TEST_DEBUG_ASSERT(
                                static_cast<float>(maxX)
                                    == static_cast<float>(nodeMax.X) / model.Scale.X);
                        }
                        else if (static_cast<float>(nodeMax.X) > int16Max * model.Scale.X)
                        {
                            MPH_TEST_DEBUG_ASSERT(
                                static_cast<float>(maxX) == int16Max * model.Scale.X);
                        }
                        else if (static_cast<float>(nodeMax.X) < int16Min * model.Scale.X)
                        {
                            MPH_TEST_DEBUG_ASSERT(
                                static_cast<float>(maxX) == int16Min * model.Scale.X);
                        }
                        else
                        {
                            DebuggerBreakFallback();
                        }
                    }
                    if (maxY != nodeMax.Y)
                    {
                        if (uncapped)
                        {
                            MPH_TEST_DEBUG_ASSERT(
                                static_cast<float>(maxY)
                                    == static_cast<float>(nodeMax.Y) / model.Scale.Y);
                        }
                        else if (static_cast<float>(nodeMax.Y) > int16Max * model.Scale.Y)
                        {
                            MPH_TEST_DEBUG_ASSERT(
                                static_cast<float>(maxY) == int16Max * model.Scale.Y);
                        }
                        else if (static_cast<float>(nodeMax.Y) < int16Min * model.Scale.Y)
                        {
                            MPH_TEST_DEBUG_ASSERT(
                                static_cast<float>(maxY) == int16Min * model.Scale.Y);
                        }
                        else
                        {
                            DebuggerBreakFallback();
                        }
                    }
                    if (maxZ != nodeMax.Z)
                    {
                        if (uncapped)
                        {
                            MPH_TEST_DEBUG_ASSERT(
                                static_cast<float>(maxZ)
                                    == static_cast<float>(nodeMax.Z) / model.Scale.Z);
                        }
                        else if (static_cast<float>(nodeMax.Z) > int16Max * model.Scale.Z)
                        {
                            MPH_TEST_DEBUG_ASSERT(
                                static_cast<float>(maxZ) == int16Max * model.Scale.Z);
                        }
                        else if (static_cast<float>(nodeMax.Z) < int16Min * model.Scale.Z)
                        {
                            MPH_TEST_DEBUG_ASSERT(
                                static_cast<float>(maxZ) == int16Min * model.Scale.Z);
                        }
                        else
                        {
                            DebuggerBreakFallback();
                        }
                    }
                }
                else
                {
                    MPH_TEST_DEBUG_ASSERT(
                        rawNode.MinBounds.X.Value == 0
                        && rawNode.MinBounds.Y.Value == 0
                        && rawNode.MinBounds.Z.Value == 0);
                    MPH_TEST_DEBUG_ASSERT(
                        rawNode.MaxBounds.X.Value == 0
                        && rawNode.MaxBounds.Y.Value == 0
                        && rawNode.MaxBounds.Z.Value == 0);
                }
            }
        }
        Nop();
    }

    void Test::Nop() noexcept
    {
    }
}

#undef MPH_TEST_DEBUG_ASSERT
