#include "CameraSequence.hpp"

#include "../../Features.hpp"
#include "../../MemoryArrays.hpp"
#include "../../Messaging.hpp"
#include "../../Read.hpp"
#include "../../Scene.hpp"
#include "../../Formats/Culling.hpp"
#include "../../Formats/Formats.hpp"
#include "../../Formats/RawFormats.hpp"
#include "../EntityBase.hpp"
#include "../Players/PlayerEntity.hpp"

#include <algorithm>
#include <any>
#include <cassert>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace
{
    using ::OpenTK::Mathematics::Matrix4x3;
    using ::OpenTK::Mathematics::Vector3;
    using ::OpenTK::Mathematics::Vector4;

    template <typename TEnum>
    [[nodiscard]] constexpr bool TestFlag(TEnum value, TEnum flag) noexcept
    {
        using Underlying = std::underlying_type_t<TEnum>;
        return (static_cast<Underlying>(value) & static_cast<Underlying>(flag))
            == static_cast<Underlying>(flag);
    }

    template <typename TEnum>
    [[nodiscard]] constexpr TEnum SetFlag(TEnum value, TEnum flag) noexcept
    {
        using Underlying = std::underlying_type_t<TEnum>;
        return static_cast<TEnum>(
            static_cast<Underlying>(value) | static_cast<Underlying>(flag));
    }

    template <typename TEnum>
    [[nodiscard]] constexpr TEnum ClearFlag(TEnum value, TEnum flag) noexcept
    {
        using Underlying = std::underlying_type_t<TEnum>;
        return static_cast<TEnum>(
            static_cast<Underlying>(value) & ~static_cast<Underlying>(flag));
    }

    template <typename T>
    [[nodiscard]] T& RequireReference(T* value)
    {
        if (value == nullptr)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

    [[nodiscard]] MphRead::MessageObject BoxInt32(std::int32_t value)
    {
        return std::make_shared<const std::any>(value);
    }

    [[nodiscard]] constexpr Vector3 Add(Vector3 left, Vector3 right) noexcept
    {
        return Vector3(left.X + right.X, left.Y + right.Y, left.Z + right.Z);
    }

    [[nodiscard]] constexpr Vector3 Subtract(Vector3 left, Vector3 right) noexcept
    {
        return Vector3(left.X - right.X, left.Y - right.Y, left.Z - right.Z);
    }

    [[nodiscard]] constexpr Vector3 Multiply(Vector3 value, float scalar) noexcept
    {
        return Vector3(value.X * scalar, value.Y * scalar, value.Z * scalar);
    }

    [[nodiscard]] constexpr Vector3 Divide(Vector3 value, float scalar) noexcept
    {
        return Vector3(value.X / scalar, value.Y / scalar, value.Z / scalar);
    }

    [[nodiscard]] constexpr Vector3 Vec4MultMtx4x3(
        Vector4 vector, const Matrix4x3& matrix) noexcept
    {
        return Vector3(
            vector.X * matrix.M11 + vector.Y * matrix.M21
                + vector.Z * matrix.M31 + vector.W * matrix.M41,
            vector.X * matrix.M12 + vector.Y * matrix.M22
                + vector.Z * matrix.M32 + vector.W * matrix.M42,
            vector.X * matrix.M13 + vector.Y * matrix.M23
                + vector.Z * matrix.M33 + vector.W * matrix.M43);
    }

    [[nodiscard]] constexpr float Dot(Vector4 left, Vector4 right) noexcept
    {
        return left.X * right.X + left.Y * right.Y
            + left.Z * right.Z + left.W * right.W;
    }

    [[nodiscard]] constexpr float DegreesToRadians(float degrees) noexcept
    {
        return degrees * 0.01745329251994329576923690768489F;
    }

    [[nodiscard]] std::string ReplaceAll(
        std::string value, std::string_view oldValue, std::string_view newValue)
    {
        if (oldValue.empty())
        {
            return value;
        }
        std::size_t start = 0;
        while ((start = value.find(oldValue, start)) != std::string::npos)
        {
            value.replace(start, oldValue.size(), newValue);
            start += newValue.size();
        }
        return value;
    }

    [[nodiscard]] bool IsCockpitLoop(std::int32_t id) noexcept
    {
        return id == 102 || id == 103 || id == 104
            || id == 105 || id == 106 || id == 168;
    }

    [[nodiscard]] bool IsLandingIntro(std::int32_t id) noexcept
    {
        return id == 0 || id == 1 || id == 2 || id == 3 || id == 167;
    }

    [[nodiscard]] bool IsLandingIntroWithoutFade(std::int32_t id) noexcept
    {
        return id == 0 || id == 3 || id == 167;
    }

    class IOException : public std::runtime_error
    {
    public:
        explicit IOException(std::string message)
            : std::runtime_error(std::move(message))
        {
        }
    };

    class FileNotFoundException final : public IOException
    {
    public:
        explicit FileNotFoundException(const std::string& path)
            : IOException("Could not find file '" + path + "'.")
        {
        }
    };

    class DirectoryNotFoundException final : public IOException
    {
    public:
        explicit DirectoryNotFoundException(const std::string& path)
            : IOException("Could not find a part of the path '" + path + "'.")
        {
        }
    };

    class UnauthorizedAccessException final : public std::runtime_error
    {
    public:
        explicit UnauthorizedAccessException(const std::string& path)
            : std::runtime_error("Access to the path '" + path + "' is denied.")
        {
        }
    };

    [[noreturn]] void ThrowIOException(const std::string& path)
    {
        throw IOException("I/O error occurred while reading file '" + path + "'.");
    }

    [[noreturn]] void ThrowFileTooLong(const std::string& path)
    {
        throw IOException(
            "The file '" + path + "' is too long. This operation is limited to files "
            "less than 2 gigabytes in size.");
    }

#ifdef _WIN32
    class FileHandle final
    {
    public:
        explicit FileHandle(HANDLE value) noexcept
            : _value(value)
        {
        }

        FileHandle(const FileHandle&) = delete;
        FileHandle& operator=(const FileHandle&) = delete;

        ~FileHandle()
        {
            if (_value != INVALID_HANDLE_VALUE)
            {
                CloseHandle(_value);
            }
        }

        [[nodiscard]] HANDLE Get() const noexcept
        {
            return _value;
        }

    private:
        HANDLE _value;
    };

    [[nodiscard]] std::wstring ToWidePath(const std::string& path)
    {
        if (path.empty())
        {
            return {};
        }
        if (path.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        {
            ThrowIOException(path);
        }
        const int length = static_cast<int>(path.size());
        const int count = MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, path.data(), length, nullptr, 0);
        if (count <= 0)
        {
            ThrowIOException(path);
        }
        std::wstring result(static_cast<std::size_t>(count), L'\0');
        if (MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, path.data(), length,
                result.data(), count) != count)
        {
            ThrowIOException(path);
        }
        return result;
    }

    [[noreturn]] void ThrowOpenError(const std::string& path, DWORD error)
    {
        if (error == ERROR_FILE_NOT_FOUND)
        {
            throw FileNotFoundException(path);
        }
        if (error == ERROR_PATH_NOT_FOUND || error == ERROR_INVALID_DRIVE)
        {
            throw DirectoryNotFoundException(path);
        }
        if (error == ERROR_ACCESS_DENIED)
        {
            throw UnauthorizedAccessException(path);
        }
        ThrowIOException(path);
    }

    [[nodiscard]] std::vector<std::uint8_t> ReadAllBytes(const std::string& path)
    {
        const std::wstring widePath = ToWidePath(path);
        FileHandle file(CreateFileW(
            widePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, nullptr));
        if (file.Get() == INVALID_HANDLE_VALUE)
        {
            ThrowOpenError(path, GetLastError());
        }

        LARGE_INTEGER length{};
        if (!GetFileSizeEx(file.Get(), &length) || length.QuadPart < 0)
        {
            ThrowIOException(path);
        }
        constexpr std::uint64_t maxManagedByteArrayLength = 0x7FFFFFC7ULL;
        if (static_cast<std::uint64_t>(length.QuadPart) > maxManagedByteArrayLength)
        {
            ThrowFileTooLong(path);
        }

        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length.QuadPart));
        std::size_t offset = 0;
        while (offset < bytes.size())
        {
            const DWORD requested = static_cast<DWORD>(std::min<std::size_t>(
                bytes.size() - offset,
                static_cast<std::size_t>(std::numeric_limits<DWORD>::max())));
            DWORD readCount = 0;
            if (!ReadFile(
                    file.Get(), bytes.data() + offset, requested, &readCount, nullptr))
            {
                ThrowIOException(path);
            }
            if (readCount == 0)
            {
                bytes.resize(offset);
                break;
            }
            offset += readCount;
        }
        return bytes;
    }
#else
    class FileDescriptor final
    {
    public:
        explicit FileDescriptor(int value) noexcept
            : _value(value)
        {
        }

        FileDescriptor(const FileDescriptor&) = delete;
        FileDescriptor& operator=(const FileDescriptor&) = delete;

        ~FileDescriptor()
        {
            if (_value >= 0)
            {
                close(_value);
            }
        }

        [[nodiscard]] int Get() const noexcept
        {
            return _value;
        }

    private:
        int _value;
    };

    [[nodiscard]] bool ParentDirectoryIsMissing(const std::string& path)
    {
        const std::size_t slash = path.find_last_of('/');
        if (slash == std::string::npos)
        {
            return false;
        }
        const std::string parent = slash == 0 ? "/" : path.substr(0, slash);
        struct stat status{};
        return stat(parent.c_str(), &status) != 0 && errno == ENOENT;
    }

    [[noreturn]] void ThrowOpenError(const std::string& path, int error)
    {
        if (error == EACCES || error == EPERM || error == EISDIR)
        {
            throw UnauthorizedAccessException(path);
        }
        if (error == ENOTDIR)
        {
            throw DirectoryNotFoundException(path);
        }
        if (error == ENOENT)
        {
            if (ParentDirectoryIsMissing(path))
            {
                throw DirectoryNotFoundException(path);
            }
            throw FileNotFoundException(path);
        }
        ThrowIOException(path);
    }

    [[nodiscard]] std::vector<std::uint8_t> ReadAllBytes(const std::string& path)
    {
        FileDescriptor file(open(path.c_str(), O_RDONLY));
        if (file.Get() < 0)
        {
            ThrowOpenError(path, errno);
        }

        struct stat status{};
        if (fstat(file.Get(), &status) != 0)
        {
            ThrowIOException(path);
        }
        if (S_ISDIR(status.st_mode))
        {
            throw UnauthorizedAccessException(path);
        }
        if (status.st_size < 0)
        {
            ThrowIOException(path);
        }
        constexpr std::uint64_t maxManagedByteArrayLength = 0x7FFFFFC7ULL;
        if (static_cast<std::uint64_t>(status.st_size) > maxManagedByteArrayLength)
        {
            ThrowFileTooLong(path);
        }

        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(status.st_size));
        std::size_t offset = 0;
        while (offset < bytes.size())
        {
            const ssize_t readCount = read(
                file.Get(), bytes.data() + offset, bytes.size() - offset);
            if (readCount < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                ThrowIOException(path);
            }
            if (readCount == 0)
            {
                bytes.resize(offset);
                break;
            }
            offset += static_cast<std::size_t>(readCount);
        }
        return bytes;
    }
#endif

    template <typename T>
    [[nodiscard]] std::shared_ptr<MphRead::Entities::EntityBase> ToEntityShared(
        const std::shared_ptr<T>& value)
    {
        return std::static_pointer_cast<MphRead::Entities::EntityBase>(value);
    }

    template <typename T>
    [[nodiscard]] std::shared_ptr<MphRead::Entities::EntityBase> ToEntityShared(T* value)
    {
        if (value == nullptr)
        {
            return nullptr;
        }
        return std::shared_ptr<MphRead::Entities::EntityBase>(
            static_cast<MphRead::Entities::EntityBase*>(value),
            [](MphRead::Entities::EntityBase*) { });
    }
}

namespace MphRead::Formats::CameraSequenceDetail
{
    [[noreturn]] void ThrowArrayIndexOutOfRange()
    {
        throw Memory::Detail::IndexOutOfRangeException();
    }

    [[noreturn]] void ThrowListIndexOutOfRange()
    {
        throw Memory::Detail::ArgumentOutOfRangeException();
    }
}

namespace MphRead::Formats
{
    std::shared_ptr<CameraSequence> CameraSequence::_current{};
    std::shared_ptr<CameraSequence> CameraSequence::_intro{};

    const CameraSequenceReadOnlyArray<std::int32_t, 199> CameraSequence::MusicData(
        std::array<std::int32_t, 199>{
        28, 27, 29, 30, 0, 0, 0, 0, 0, 0, 0, 1|0x4000, 0, 0, 0, 30|0x4000, 0, 0, 3|0x4000, 0, 5|0x4000, 0, 38|0x8000, 0, 0, 0, 0, 0, 16|0x400|0x4000, 14|0x400|0x4000, 0, 49|0x4000, 48|0x4000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 53|0x8000, 9|0x4000, 37|0x4000, 13|0x4000, 0, 0, 41|0x800|0x4000, 0, 19|0x1000|0x4000, 51|0x1000|0x4000, 57|0x1000|0x4000, 44|0x2000|0x4000, 0, 10|0x4000, 5|0x4000, 0, 0, 0, 0, 0, 0, 0, 0, 16|0x4000, 60|0x4000, 50|0x1000|0x4000, 53|0x1000|0x4000, 50|0x1000|0x4000, 53|0x1000|0x4000, 0, 53|0x1000|0x4000, 0, 0, 0, 0, 0, 0, 52|0x1000|0x4000, 0, 0|0x1000|0x4000, 0, 0, 39|0x4000, 0|0x1000|0x4000, 0, 0, 0, 0, 0, 0, 0, 1|0x4000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 53|0x1000|0x4000, 0, 0, 0, 0, 0, 14|0x4000, 0, 0, 0, 0, 0, 0, 28|0x4000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 59, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    });

    const CameraSequenceReadOnlyArray<std::int32_t, 199> CameraSequence::SfxData(
        std::array<std::int32_t, 199>{
        92|0x8000, 17|0x8000, 94|0x8000, 93|0x8000, 0, 0, 19, 0, 0, 19, 0, 0, 19, 0, 19, 0, 19, 19, 0, 0, 0, 18|0x8000, 20|0x8000, 0, 0, 19, 0, 0, 0, 19, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 70, 0, 19|0x2000, 0, 69, 66|0x4000, 0, 0, 21, 0, 95|0x8000, 0, 0, 98, 80, 90, 0, 0, 0, 0, 0, 0, 0, 0, 19, 0, 19, 19, 0, 19, 72, 71, 0, 79, 73, 0, 19, 0, 0, 0, 0, 0, 19, 0, 22, 0, 0, 0, 19, 0, 0, 0, 0, 0, 0, 0, 0, 0, 19, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 19, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 65|0x4000, 84, 0, 0, 0, 97, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 19, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 19, 0, 0, 0, 99|0x8000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    });

    const CameraSequenceReadOnlyArray<std::string_view, 199> CameraSequence::Filenames(
        std::array<std::string_view, 199>{
        "unit1_land_intro.bin",
        "unit2_land_intro.bin",
        "unit3_land_intro.bin",
        "unit4_land_intro.bin",
        "unit4_c1_platform_intro.bin",
        "unit2_co_scan_intro.bin",
        "unit2_co_scan_outro.bin",
        "unit2_co_bit_intro.bin",
        "unit2_c4_teleporter_intro.bin",
        "unit2_co_bit_outro.bin",
        "unit2_co_helm_flyby.bin",
        "unit2_rm1_artifact_intro.bin",
        "unit2_rm1_artifact_outro.bin",
        "unit2_c4_artifact_intro.bin",
        "unit2_c4_artifact_outro.bin",
        "unit2_rm2_kanden_intro.bin",
        "unit2_rm3_artifact_intro.bin",
        "unit2_rm3_artifact_outro.bin",
        "unit2_rm3_kanden_intro.bin",
        "unit4_co_morphballmaze.bin",
        "unit2_b1_octolith_intro.bin",
        "unit2_co_guardian_intro.bin",
        "unit2_rm3_kanden_outro.bin",
        "unit4_rm1_morphballjumps1.bin",
        "unit4_land_guardian_intro.bin",
        "unit4_rm3_scandoor_unlock.bin",
        "unit1_c4_dropmaze_left.bin",
        "unit4_co_morphballmaze_enter.bin",
        "unit4_rm5_arcticspawn_intro.bin",
        "unit4_rm5_arcticspawn_outro.bin",
        "unit1_c4_dropmaze_right.bin",
        "unit4_rm3_hunters_intro.bin",
        "unit4_rm3_hunters_outro.bin",
        "unit4_rm2_switch_intro.bin",
        "unit4_rm2_guardian_intro.bin",
        "unit1_c5_pistonmaze_1.bin",
        "unit1_c5_pistonmaze_2.bin",
        "unit1_c5_pistonmaze_3.bin",
        "unit1_c5_pistonmaze_4.bin",
        "unit4_rm2_guardian_outro.bin",
        "unit3_c2_morphballmaze.bin",
        "unit4_rm5_powerdown.bin",
        "unit4_rm1_morphballjumps2.bin",
        "unit4_rm2_elevator_intro.bin",
        "unit4_rm1_morphballjumps3.bin",
        "unit4_rm5_pillarcrash.bin",
        "unit4_rm1_wasp_intro.bin",
        "unit1_RM1_spire_intro_layer0.bin",
        "unit1_RM1_spire_intro_layer3.bin",
        "unit1_RM1_spire_outro.bin",
        "unit1_RM6_spire_intro_layer3.bin",
        "unit1_c1_shipflyby.bin",
        "unit4_rm3_trace_intro.bin",
        "unit3_rm1_forcefield_unlock.bin",
        "unit3_rm1_ship_battle_end.bin",
        "unit3_rm2_evac_intro.bin",
        "unit3_rm2_evac_fail.bin",
        "unit4_rm1_puzzle_activate.bin",
        "unit4_rm1_artifact_intro.bin",
        "unit1_c0_weavel_intro.bin",
        "bigeye_octolith_intro.bin",
        "unit2_rm4_panel_open_1.bin",
        "unit2_rm4_panel_open_2.bin",
        "unit2_rm4_panel_open_3.bin",
        "unit2_rm4_cntlroom_open.bin",
        "unit2_rm4_teleporter_active.bin",
        "unit2_rm6_teleporter_active.bin",
        "unit1_rm2_rm3door_open.bin",
        "unit1_rm2_c3door_open.bin",
        "unit1_rm3_lavademon_intro.bin",
        "unit1_rm3_magmaul_intro.bin",
        "unit3_rm3_race1.bin",
        "unit3_rm3_race1_fail.bin",
        "unit3_rm3_race2.bin",
        "unit3_rm3_race2_fail.bin",
        "unit3_rm3_incubator_malfunction_intro.bin",
        "unit3_rm3_incubator_malfunction_outro.bin",
        "unit3_rm3_door_unlock.bin",
        "unit1_rm3_forcefield_unlock.bin",
        "unit4_rm4_sniperspot_intro.bin",
        "unit4_rm5_artifact_key_intro.bin",
        "unit4_rm5_artifact_intro.bin",
        "unit3_rm2_door_unlock.bin",
        "unit3_rm2_evac_end.bin",
        "unit1_rm3_forcefield_unlock.bin",
        "unit1_c0_weavel_outro.bin",
        "unit3_rm1_sylux_preship.bin",
        "unit1_rm1_mover_activate_layer3.bin",
        "unit3_rm1_sylux_intro.bin",
        "unit4_rm3_trace_outro.bin",
        "unit4_rm5_sniper_intro.bin",
        "unit3_rm1_artifact_intro.bin",
        "unit1_rm6_spire_escape.bin",
        "unit1_crystalroom_octolith.bin",
        "unit4_co_morphballmaze_exit.bin",
        "unit4_rm3_key_intro.bin",
        "unit2_rm1_door_lock.bin",
        "unit2_rm1_key_intro.bin",
        "unit3_rm4_morphball.bin",
        "unit1_c0_morphball_door_unlock.bin",
        "unit1_rm6_forcefield_lock.bin",
        "unit1_rm6_forcefield_unlock.bin",
        "unit1_land_cockpit.bin",
        "unit2_land_cockpit.bin",
        "unit3_land_cockpit.bin",
        "unit4_land_cockpit.bin",
        "unit1_land_cockpit.bin",
        "unit1_rm1_artifact_intro.bin",
        "unit2_rm5_artifact_intro.bin",
        "unit2_c7_forcefield_lock.bin",
        "unit2_c7_forcefield_unlock.bin",
        "unit2_c7_artifact_intro.bin",
        "unit2_rm8_artifact_intro.bin",
        "unit4_co_door_unlock.bin",
        "unit1_land_cockpit_land.bin",
        "unit1_land_cockpit_takeoff.bin",
        "unit2_land_cockpit_land.bin",
        "unit2_land_cockpit_takeoff.bin",
        "unit3_land_cockpit_land.bin",
        "unit3_land_cockpit_takeoff.bin",
        "unit4_land_cockpit_land.bin",
        "unit4_land_cockpit_takeoff.bin",
        "unit1_land_cockpit_land.bin",
        "unit1_land_cockpit_takeoff.bin",
        "unit1_rm2_mover1_activate.bin",
        "unit1_rm2_mover2_activate.bin",
        "unit1_rm2_mover3_activate.bin",
        "unit3_rm3_race_artifact_intro.bin",
        "unit1_c5_artifact_intro.bin",
        "unit1_rm3_key_intro.bin",
        "unit1_rm3_artifact_intro.bin",
        "unit1_c3_artifact_intro.bin",
        "unit4_rm1_forcefield_unlock.bin",
        "unit4_rm1_wasp_outro.bin",
        "unit4_rm4_artifact_intro.bin",
        "unit4_rm4_artifact_outro.bin",
        "unit3_rm2_artifact_intro.bin",
        "unit3_rm1_ship_intro.bin",
        "bigeye1_intro.bin",
        "unit4_rm2_key_intro.bin",
        "unit2_rm1_bit_intro.bin",
        "unit1_rm1_spire_escape.bin",
        "bigeye_morphball.bin",
        "unit3_rm3_key_outro.bin",
        "unit3_rm4_key_intro.bin",
        "unit3_rm4_key_outro.bin",
        "unit1_c0_key_intro.bin",
        "unit1_rm6_key_intro.bin",
        "unit1_rm6_morphball.bin",
        "unit3_rm1_bottomfloorkey_intro.bin",
        "unit4_rm4_key_intro.bin",
        "unit4_rm4_guardian_outro.bin",
        "unit4_rm5_forcefield_outro.bin",
        "unit4_rm5_quadtroid_outro.bin",
        "unit4_rm2_key_outro.bin",
        "unit3_rm4_guardian_intro.bin",
        "unit3_rm4_morphballdoor_unlock.bin",
        "unit2_rm5_key_intro.bin",
        "unit3_rm4_item_intro.bin",
        "unit2_c7_key_intro.bin",
        "unit2_rm4_forcefield_unlock_1.bin",
        "unit2_rm4_forcefield_unlock_2.bin",
        "unit2_rm4_forcefield_unlock_3.bin",
        "unit3_c2_battlehammer_intro.bin",
        "unit4_rm3_morphball_cam.bin",
        "unit4_rm1_door_open.bin",
        "gorea_b2_gun_intro.bin",
        "gorea_land_intro.bin",
        "gorea_land_cockpit.bin",
        "gorea_land_cockpit_land.bin",
        "gorea_land_cockpit_takeoff.bin",
        "unit4_rm1_puzzle_intro.bin",
        "mp00_intro.bin",
        "mp01_intro.bin",
        "mp02_intro.bin",
        "mp03_intro.bin",
        "mp04_intro.bin",
        "mp05_intro.bin",
        "mp06_intro.bin",
        "mp07_intro.bin",
        "mp08_intro.bin",
        "mp09_intro.bin",
        "mp10_intro.bin",
        "mp11_intro.bin",
        "mp12_intro.bin",
        "mp13_intro.bin",
        "mp14_intro.bin",
        "mp15_intro.bin",
        "mp16_intro.bin",
        "mp17_intro.bin",
        "mp18_intro.bin",
        "mp19_intro.bin",
        "mp20_intro.bin",
        "mp21_intro.bin",
        "mp22_intro.bin",
        "mp23_intro.bin",
        "mp24_intro.bin",
        "mp25_intro.bin",
        "mp26_intro.bin"
    }, CameraSequenceDetail::IndexSemantics::List);

    CameraSequence::CameraSequence(
        std::int32_t id,
        std::string name,
        Scene* scene,
        CameraSequenceHeader header,
        const std::vector<RawCameraSequenceKeyframe>& keyframes)
        : _sequenceId(id),
          _name(ReplaceAll(std::move(name), ".bin", "")),
          _version(header.Version),
          _initialCamInfo(std::make_unique<Entities::CameraInfo>()),
          _scene(scene)
    {
        _keyframes.reserve(keyframes.size());
        for (const RawCameraSequenceKeyframe& keyframe : keyframes)
        {
            _keyframes.push_back(std::make_shared<CameraSequenceKeyframe>(keyframe));
        }
        if (id > 171 || IsCockpitLoop(id))
        {
            _flags = SetFlag(_flags, CamSeqFlags::Loop);
        }
    }

    CameraSequence::~CameraSequence() = default;

    std::int32_t CameraSequence::SequenceId() const noexcept
    {
        return _sequenceId;
    }

    const std::string& CameraSequence::Name() const noexcept
    {
        return _name;
    }

    std::uint8_t CameraSequence::Version() const noexcept
    {
        return _version;
    }

    const std::vector<std::shared_ptr<CameraSequenceKeyframe>>&
        CameraSequence::Keyframes() const noexcept
    {
        return _keyframes;
    }

    CamSeqFlags CameraSequence::Flags() const noexcept
    {
        return _flags;
    }

    void CameraSequence::Flags(CamSeqFlags value) noexcept
    {
        _flags = value;
    }

    bool CameraSequence::BlockInput() const noexcept
    {
        return TestFlag(_flags, CamSeqFlags::BlockInput);
    }

    bool CameraSequence::ForceAlt() const noexcept
    {
        return TestFlag(_flags, CamSeqFlags::ForceAlt);
    }

    bool CameraSequence::ForceBiped() const noexcept
    {
        return TestFlag(_flags, CamSeqFlags::ForceBiped);
    }

    std::uint16_t CameraSequence::TransitionTimer() const noexcept
    {
        return _transitionTimer;
    }

    void CameraSequence::TransitionTimer(std::uint16_t value) noexcept
    {
        _transitionTimer = value;
    }

    std::uint16_t CameraSequence::TransitionTime() const noexcept
    {
        return _transitionTime;
    }

    Entities::CameraInfo& CameraSequence::InitialCamInfo() noexcept
    {
        return *_initialCamInfo;
    }

    const Entities::CameraInfo& CameraSequence::InitialCamInfo() const noexcept
    {
        return *_initialCamInfo;
    }

    Entities::CameraInfo* CameraSequence::CamInfoRef() const noexcept
    {
        return _camInfoRef;
    }

    void CameraSequence::CamInfoRef(Entities::CameraInfo* value) noexcept
    {
        _camInfoRef = value;
    }

    CameraSequence* CameraSequence::Current() noexcept
    {
        return _current.get();
    }

    void CameraSequence::Current(CameraSequence* value) noexcept
    {
        _current = value == nullptr ? nullptr : value->shared_from_this();
    }

    CameraSequence* CameraSequence::Intro() noexcept
    {
        return _intro.get();
    }

    void CameraSequence::Intro(CameraSequence* value) noexcept
    {
        _intro = value == nullptr ? nullptr : value->shared_from_this();
    }

    bool CameraSequence::IsIntro() const noexcept
    {
        return _sequenceId >= 172 && _sequenceId <= 198;
    }

    void CameraSequence::Initialize()
    {
        Scene& scene = RequireReference(_scene);
        for (const std::shared_ptr<CameraSequenceKeyframe>& keyframeRef : _keyframes)
        {
            CameraSequenceKeyframe& keyframe = RequireReference(keyframeRef.get());
            keyframe.PositionEntity = GetKeyframeRef(keyframe.PosEntityType, keyframe.PosEntityId);
            keyframe.TargetEntity = GetKeyframeRef(keyframe.TargetEntityType, keyframe.TargetEntityId);
            keyframe.MessageTarget = GetKeyframeRef(keyframe.MessageTargetType, keyframe.MessageTargetId);
            keyframe.NodeRef = scene.GetNodeRefByName(keyframe.NodeName);
            if (keyframe.NodeRef == Culling::NodeRef::None)
            {
                keyframe.NodeRef = scene.GetNodeRefByName("rmMain");
            }
            assert(scene.Room() == nullptr || keyframe.NodeRef != Culling::NodeRef::None);
        }
    }

    void CameraSequence::Process()
    {
        if (TestFlag(_flags, CamSeqFlags::Complete) || _keyframes.empty())
        {
            return;
        }

        assert(_camInfoRef != nullptr);
        Entities::CameraInfo& camInfo = RequireReference(_camInfoRef);
        camInfo.Shake = 0.0F;
        camInfo.PrevPosition = camInfo.Position;

        CameraSequenceKeyframe& curFrame
            = RequireReference(_keyframes.at(static_cast<std::size_t>(_keyframeIndex)).get());
        const float frameLength = curFrame.HoldTime + curFrame.MoveTime;
        const float fadeOutStart = frameLength - curFrame.FadeOutTime;

        CalculateFrameValues();

        Scene& scene = RequireReference(_scene);
        if (_keyframeElapsed < 1.0F / 60.0F)
        {
            camInfo.PrevPosition = camInfo.Position;
            if (curFrame.PositionEntity)
            {
                Culling::NodeRef nodeRef = curFrame.PositionEntity->NodeRef;
                if (nodeRef != Culling::NodeRef::None)
                {
                    Vector3 prevPos;
                    curFrame.PositionEntity->GetPosition(prevPos);
                    camInfo.NodeRef = scene.UpdateNodeRef(
                        camInfo.NodeRef, prevPos, camInfo.Position);
                }
                else
                {
                    camInfo.NodeRef = curFrame.NodeRef;
                }
            }
            else
            {
                camInfo.NodeRef = curFrame.NodeRef;
            }

            const Message message = static_cast<Message>(curFrame.MessageId);
            if (message != Message::None)
            {
                scene.SendMessage(
                    message,
                    nullptr,
                    curFrame.MessageTarget.get(),
                    BoxInt32(static_cast<std::int32_t>(curFrame.MessageParam)),
                    BoxInt32(0));
            }
        }

        FadeType fadeType = FadeType::None;
        float fadeTime = 0.0F;
        if (curFrame.FadeInType != FadeType::None
            && _keyframeElapsed <= 2.0F / 30.0F)
        {
            fadeType = curFrame.FadeInType;
            fadeTime = curFrame.FadeInTime;
        }
        else if (curFrame.FadeOutType != FadeType::None
            && _keyframeElapsed >= fadeOutStart
            && _keyframeElapsed <= fadeOutStart + 2.0F / 30.0F)
        {
            fadeType = curFrame.FadeOutType;
            fadeTime = curFrame.FadeOutTime;
        }
        else if (_keyframeIndex == 0
            && _keyframeElapsed == 0.0F
            && IsLandingIntroWithoutFade(_sequenceId))
        {
            fadeType = FadeType::FadeInWhite;
            fadeTime = 5.0F / 30.0F;
        }

        if (fadeType != FadeType::None)
        {
            const bool overwrite = _keyframeIndex == 0 && IsLandingIntro(_sequenceId);
            scene.SetFade(fadeType, fadeTime, overwrite);
        }

        _keyframeElapsed += scene.FrameTime();
        if (_keyframeElapsed >= frameLength)
        {
            _keyframeElapsed -= frameLength;
            if (_keyframeElapsed >= 1.0F / 60.0F)
            {
                _keyframeElapsed = 1.0F / 60.0F - 1.0F / 4096.0F;
            }
            ++_keyframeIndex;
            if (_keyframeIndex >= static_cast<std::int32_t>(_keyframes.size()))
            {
                if (TestFlag(_flags, CamSeqFlags::Loop))
                {
                    Restart();
                }
                else
                {
                    _flags = SetFlag(_flags, CamSeqFlags::CanEnd);
                    _flags = SetFlag(_flags, CamSeqFlags::Complete);
                    _keyframeElapsed = frameLength;
                }
            }
        }

        if (_transitionTimer < _transitionTime)
        {
            ++_transitionTimer;
        }

        camInfo.Update();
        camInfo.NodeRef = scene.UpdateNodeRef(
            camInfo.NodeRef, camInfo.PrevPosition, camInfo.Position);

        Entities::PlayerEntity& player
            = RequireReference(Entities::PlayerEntity::Main());
        if ((ForceAlt() && player.IsAltForm())
            || (ForceBiped() && !player.IsAltForm()))
        {
            player.BlockFormSwitch();
        }
    }

    void CameraSequence::SetUp(
        Entities::CameraInfo& camInfo, std::uint16_t transitionTime)
    {
        _camInfoRef = &camInfo;
        Entities::CameraInfo& initial = *_initialCamInfo;

        initial.Position = camInfo.Position;
        initial.PrevPosition = camInfo.PrevPosition;
        initial.Target = camInfo.Target;
        initial.UpVector = camInfo.UpVector;
        initial.TrueUp = camInfo.TrueUp;
        initial.Facing = camInfo.Facing;
        initial.Fov = camInfo.Fov;
        initial.Shake = camInfo.Shake;
        initial.ViewMatrix = camInfo.ViewMatrix;
        initial.Field48 = camInfo.Field48;
        initial.Field4C = camInfo.Field4C;
        initial.Field50 = camInfo.Field50;
        initial.Field54 = camInfo.Field54;
        initial.NodeRef = camInfo.NodeRef;

        _flags = ClearFlag(_flags, CamSeqFlags::Complete);
        _flags = ClearFlag(_flags, CamSeqFlags::CanEnd);
        _flags = ClearFlag(_flags, CamSeqFlags::Loop);
        _keyframeElapsed = 0.0F;
        _transitionTimer = 0;
        _transitionTime = transitionTime;
        _keyframeIndex = 0;

        if (!_keyframes.empty())
        {
            CameraSequenceKeyframe& firstFrame
                = RequireReference(_keyframes[0].get());
            camInfo.NodeRef = firstFrame.NodeRef;
            CalculateFrameValues();
            if (firstFrame.PositionEntity)
            {
                Culling::NodeRef nodeRef = firstFrame.PositionEntity->NodeRef;
                if (nodeRef != Culling::NodeRef::None)
                {
                    Vector3 prevPos;
                    firstFrame.PositionEntity->GetPosition(prevPos);
                    camInfo.NodeRef = RequireReference(_scene).UpdateNodeRef(
                        nodeRef, prevPos, camInfo.Position);
                }
            }
        }

        _current = shared_from_this();
        Entities::PlayerEntity& player
            = RequireReference(Entities::PlayerEntity::Main());
        if (_sequenceId > 3)
        {
            player.CloseDialogs();
            player.ResetCombatVisor();
        }
        player.HudEndDisrupted();
    }

    void CameraSequence::Restart(
        std::uint16_t transitionTimer, std::uint16_t transitionTime)
    {
        _flags = ClearFlag(_flags, CamSeqFlags::Complete);
        _flags = ClearFlag(_flags, CamSeqFlags::CanEnd);
        _keyframeElapsed = 0.0F;
        _transitionTimer = transitionTimer;
        _transitionTime = transitionTime;
        _keyframeIndex = 0;

        assert(!_keyframes.empty());
        assert(_camInfoRef != nullptr);

        CameraSequenceKeyframe& firstFrame
            = RequireReference(_keyframes.at(0).get());
        Entities::CameraInfo& camInfo = RequireReference(_camInfoRef);
        camInfo.NodeRef = firstFrame.NodeRef;
        CalculateFrameValues();

        if (firstFrame.PositionEntity
            && (!Bugfixes::BetterCamSeqNodeRef || _sequenceId == 98))
        {
            Culling::NodeRef nodeRef = firstFrame.PositionEntity->NodeRef;
            if (nodeRef != Culling::NodeRef::None)
            {
                Vector3 prevPos;
                firstFrame.PositionEntity->GetPosition(prevPos);
                camInfo.NodeRef = RequireReference(_scene).UpdateNodeRef(
                    nodeRef, prevPos, camInfo.Position);
            }
        }
    }

    void CameraSequence::End()
    {
        _flags = SetFlag(_flags, CamSeqFlags::CanEnd);
        _flags = SetFlag(_flags, CamSeqFlags::Complete);
        _keyframeElapsed = 0.0F;
        _transitionTimer = 0;
        _transitionTime = 0;
        _keyframeIndex = 0;

        if (_current.get() == this)
        {
            if (_camInfoRef != nullptr)
            {
                Entities::CameraInfo& camInfo = *_camInfoRef;
                const Entities::CameraInfo& initial = *_initialCamInfo;
                camInfo.Position = initial.Position;
                camInfo.PrevPosition = initial.PrevPosition;
                camInfo.Target = initial.Target;
                camInfo.UpVector = initial.UpVector;
                camInfo.TrueUp = initial.TrueUp;
                camInfo.Facing = initial.Facing;
                camInfo.Fov = initial.Fov;
                camInfo.Shake = initial.Shake;
                camInfo.ViewMatrix = initial.ViewMatrix;
                camInfo.Field48 = initial.Field48;
                camInfo.Field4C = initial.Field4C;
                camInfo.Field50 = initial.Field50;
                camInfo.Field54 = initial.Field54;
                camInfo.NodeRef = initial.NodeRef;
                _camInfoRef = nullptr;
            }
            _current = nullptr;
        }
    }

    void CameraSequence::CalculateFrameValues()
    {
        CameraSequenceKeyframe& curFrame
            = RequireReference(_keyframes.at(static_cast<std::size_t>(_keyframeIndex)).get());

        float movePercent = 0.0F;
        const float moveElapsed = _keyframeElapsed - curFrame.HoldTime;
        if (moveElapsed >= 0.0F && curFrame.MoveTime > 0.0F)
        {
            movePercent = moveElapsed / curFrame.MoveTime;
        }

        const Vector4 moveVec = GetVec4(movePercent);
        const Vector4 factorVec(
            0.0F,
            (curFrame.PrevFrameInfluence & 1) == 0 ? 1.0F / 3.0F : 0.0F,
            (curFrame.AfterFrameInfluence & 1) == 0 ? 2.0F / 3.0F : 1.0F,
            1.0F);
        const float factorDot = Dot(factorVec, moveVec);

        CameraSequenceKeyframe* nextFrame = nullptr;
        if (_keyframeIndex + 1 < static_cast<std::int32_t>(_keyframes.size()))
        {
            nextFrame = _keyframes[static_cast<std::size_t>(_keyframeIndex + 1)].get();
        }

        Vector3 finalPosition;
        Vector3 finalToTarget;
        float finalRoll;
        float finalFov;

        if (nextFrame != nullptr)
        {
            if (((curFrame.PrevFrameInfluence | curFrame.AfterFrameInfluence) & 2) != 0
                && curFrame.MoveTime > 0.0F)
            {
                Vector3 curPos = curFrame.Position;
                Vector3 curTarget = curFrame.ToTarget;
                Vector3 nextPos = nextFrame->Position;
                Vector3 nextTarget = nextFrame->ToTarget;
                AddEntityPosition(curFrame, curPos, curTarget);
                AddEntityPosition(*nextFrame, nextPos, nextTarget);

                const Vector3 curToNextPos
                    = Divide(Subtract(nextPos, curPos), curFrame.MoveTime);
                const Vector3 curToNextTarget
                    = Divide(Subtract(nextTarget, curTarget), curFrame.MoveTime);

                Vector3 prevPos;
                Vector3 prevTarget;
                CameraSequenceKeyframe* prevFrame = nullptr;
                if (_keyframeIndex - 1 >= 0)
                {
                    prevFrame = _keyframes[static_cast<std::size_t>(_keyframeIndex - 1)].get();
                }
                if (_keyframeIndex > 0
                    && prevFrame != nullptr
                    && (curFrame.PrevFrameInfluence & 2) != 0
                    && prevFrame->MoveTime > 0.0F)
                {
                    const float easing
                        = 1.0F / 6.0F * curFrame.MoveTime * curFrame.Easing;
                    prevPos = prevFrame->Position;
                    prevTarget = prevFrame->ToTarget;
                    AddEntityPosition(*prevFrame, prevPos, prevTarget);

                    const Vector3 prevToCurPos
                        = Divide(Subtract(curPos, prevPos), prevFrame->MoveTime);
                    const Vector3 prevToNextPos = Add(curToNextPos, prevToCurPos);
                    prevPos = Add(Multiply(prevToNextPos, easing), curPos);

                    const Vector3 prevToCurTarget
                        = Divide(Subtract(curTarget, prevTarget), prevFrame->MoveTime);
                    const Vector3 prevToNextTarget
                        = Add(curToNextTarget, prevToCurTarget);
                    prevTarget = Add(Multiply(prevToNextTarget, easing), curTarget);
                }
                else
                {
                    const float easing
                        = 1.0F / 3.0F * curFrame.MoveTime * curFrame.Easing;
                    prevPos = Add(Multiply(curToNextPos, easing), curPos);
                    prevTarget = Add(Multiply(curToNextTarget, easing), curTarget);
                }

                Vector3 afterPos;
                Vector3 afterTarget;
                CameraSequenceKeyframe* afterFrame = nullptr;
                if (_keyframeIndex + 2 < static_cast<std::int32_t>(_keyframes.size()))
                {
                    afterFrame
                        = _keyframes[static_cast<std::size_t>(_keyframeIndex + 2)].get();
                }
                if (afterFrame != nullptr
                    && (curFrame.AfterFrameInfluence & 2) != 0
                    && nextFrame->MoveTime > 0.0F)
                {
                    const float easing
                        = -1.0F / 6.0F * curFrame.MoveTime * nextFrame->Easing;
                    afterPos = afterFrame->Position;
                    afterTarget = afterFrame->ToTarget;
                    AddEntityPosition(*afterFrame, afterPos, afterTarget);

                    const Vector3 nextToAfterPos
                        = Divide(Subtract(afterPos, nextPos), nextFrame->MoveTime);
                    const Vector3 currentToAfterPos = Add(curToNextPos, nextToAfterPos);
                    afterPos = Add(Multiply(currentToAfterPos, easing), nextPos);

                    const Vector3 nextToAfterTarget
                        = Divide(Subtract(afterTarget, nextTarget), nextFrame->MoveTime);
                    const Vector3 currentToAfterTarget
                        = Add(curToNextTarget, nextToAfterTarget);
                    afterTarget = Add(
                        Multiply(currentToAfterTarget, easing), nextTarget);
                }
                else
                {
                    const float easing
                        = -1.0F / 3.0F * curFrame.MoveTime * curFrame.Easing;
                    afterPos = Add(Multiply(curToNextPos, easing), nextPos);
                    afterTarget = Add(Multiply(curToNextTarget, easing), nextTarget);
                }

                const Vector4 dotVec = GetVec4(factorDot);
                const Matrix4x3 posMtx(curPos, prevPos, afterPos, nextPos);
                const Matrix4x3 targetMtx(
                    curTarget, prevTarget, afterTarget, nextTarget);
                finalPosition = Vec4MultMtx4x3(dotVec, posMtx);
                finalToTarget = Vec4MultMtx4x3(dotVec, targetMtx);
                finalRoll = curFrame.Roll * (1.0F - factorDot)
                    + nextFrame->Roll * factorDot;
                finalFov = curFrame.Fov * (1.0F - factorDot)
                    + nextFrame->Fov * factorDot;
            }
            else
            {
                Vector3 curPos = curFrame.Position;
                Vector3 curTarget = curFrame.ToTarget;
                Vector3 nextPos = nextFrame->Position;
                Vector3 nextTarget = nextFrame->ToTarget;
                AddEntityPosition(curFrame, curPos, curTarget);
                AddEntityPosition(*nextFrame, nextPos, nextTarget);

                finalPosition = Vector3(
                    curPos.X * (1.0F - factorDot) + nextPos.X * factorDot,
                    curPos.Y * (1.0F - factorDot) + nextPos.Y * factorDot,
                    curPos.Z * (1.0F - factorDot) + nextPos.Z * factorDot);
                finalToTarget = Vector3(
                    curTarget.X * (1.0F - factorDot) + nextTarget.X * factorDot,
                    curTarget.Y * (1.0F - factorDot) + nextTarget.Y * factorDot,
                    curTarget.Z * (1.0F - factorDot) + nextTarget.Z * factorDot);
                finalRoll = curFrame.Roll * (1.0F - factorDot)
                    + nextFrame->Roll * factorDot;
                finalFov = curFrame.Fov * (1.0F - factorDot)
                    + nextFrame->Fov * factorDot;
            }
        }
        else
        {
            Vector3 curPos = curFrame.Position;
            Vector3 curTarget = curFrame.ToTarget;
            AddEntityPosition(curFrame, curPos, curTarget);
            finalPosition = curPos;
            finalToTarget = curTarget;
            finalRoll = curFrame.Roll;
            finalFov = curFrame.Fov;
        }

        assert(_camInfoRef != nullptr);
        Entities::CameraInfo& camInfo = RequireReference(_camInfoRef);
        finalFov *= 2.0F;

        if (_transitionTimer >= _transitionTime)
        {
            camInfo.Fov = finalFov;
            camInfo.Position = finalPosition;
            camInfo.Target = Add(camInfo.Position, finalToTarget);
        }
        else
        {
            assert(_transitionTime != 0);
            const float pct
                = static_cast<float>(_transitionTimer)
                / static_cast<float>(_transitionTime);
            camInfo.Fov += (finalFov - camInfo.Fov) * pct;
            camInfo.Position = Add(
                camInfo.Position,
                Multiply(Subtract(finalPosition, camInfo.Position), pct));
            finalToTarget = Add(
                camInfo.Facing,
                Multiply(Subtract(finalToTarget, camInfo.Facing), pct));
            camInfo.Target = Add(camInfo.Position, finalToTarget);
        }

        Vector3 upVector(0.0F, 1.0F, 0.0F);
        if (std::fabs(finalRoll) >= 1.0F / 4096.0F)
        {
            finalRoll = DegreesToRadians(finalRoll + 90.0F);
            camInfo.Facing = Subtract(camInfo.Target, camInfo.Position);
            const Vector3 cross
                = Vector3::Cross(upVector, camInfo.Facing).Normalized();
            upVector = Vector3::Cross(camInfo.Facing, cross).Normalized();
            const float cos = std::cos(finalRoll);
            const float sin = std::sin(finalRoll);
            upVector = Vector3(cross.X * cos, upVector.Y * sin, cross.Z * cos);
        }

        if (_transitionTimer >= _transitionTime)
        {
            camInfo.UpVector = upVector;
        }
        else
        {
            assert(_transitionTime != 0);
            const float pct
                = static_cast<float>(_transitionTimer)
                / static_cast<float>(_transitionTime);
            camInfo.UpVector = Add(
                camInfo.UpVector,
                Multiply(Subtract(upVector, camInfo.UpVector), pct));
            camInfo.UpVector = camInfo.UpVector.Normalized();
        }
    }

    void CameraSequence::AddEntityPosition(
        CameraSequenceKeyframe& keyframe,
        Vector3& position,
        Vector3& toTarget)
    {
        if (keyframe.PositionEntity)
        {
            Vector3 entPos;
            Vector3 entUp;
            Vector3 entFacing;
            keyframe.PositionEntity->GetVectors(entPos, entUp, entFacing);
            if (keyframe.UseEntityTransform)
            {
                const Vector3 entRight
                    = Vector3::Cross(entUp, entFacing).Normalized();
                entUp = Vector3::Cross(entFacing, entRight).Normalized();
                position = Add(
                    entPos,
                    Vector3(
                        entRight.X * position.X
                            + entUp.X * position.Y
                            + entFacing.X * position.Z,
                        entRight.Y * position.X
                            + entUp.Y * position.Y
                            + entFacing.Y * position.Z,
                        entRight.Z * position.X
                            + entUp.Z * position.Y
                            + entFacing.Z * position.Z));
            }
            else
            {
                position = Add(position, entPos);
            }
        }

        if (keyframe.TargetEntity)
        {
            Vector3 entPos;
            keyframe.TargetEntity->GetPosition(entPos);
            const Vector3 between = Subtract(entPos, position).Normalized();
            const Vector3 cross1
                = Vector3::Cross(Vector3(0.0F, 1.0F, 0.0F), between).Normalized();
            const Vector3 cross2 = Vector3::Cross(between, cross1);
            toTarget = Vector3(
                toTarget.Z * between.X
                    + toTarget.X * cross1.X
                    + toTarget.Y * cross2.X,
                toTarget.Z * between.Y
                    + toTarget.X * cross1.Y
                    + toTarget.Y * cross2.Y,
                toTarget.Z * between.Z
                    + toTarget.X * cross1.Z
                    + toTarget.Y * cross2.Z);
        }
    }

    Vector4 CameraSequence::GetVec4(float percent) const noexcept
    {
        const float pctSqr = percent * percent;
        const float inverse = 1.0F - percent;
        const float invSqr = inverse * inverse;
        return Vector4(
            invSqr * inverse,
            3.0F * percent * invSqr,
            3.0F * pctSqr * inverse,
            pctSqr * percent);
    }

    std::shared_ptr<CameraSequence> CameraSequence::Load(
        std::int32_t id, Scene* scene)
    {
        assert(id >= 0 && id < 199);
        return Load(std::string(Filenames[id]), scene, id);
    }

    std::shared_ptr<CameraSequence> CameraSequence::Load(
        std::string name, Scene* scene, std::int32_t id)
    {
        const std::string path = Paths::Combine(
            Paths::FileSystem(), "cameraEditor", name);
        const std::vector<std::uint8_t> storage = ReadAllBytes(path);
        const std::span<const std::uint8_t> bytes(storage.data(), storage.size());

        const CameraSequenceHeader header
            = Read::ReadStruct<CameraSequenceHeader>(bytes);
        assert(header.Padding3 == 0);
        assert(header.Padding4 == 0);

        const std::shared_ptr<const std::vector<RawCameraSequenceKeyframe>> keyframes
            = Read::DoOffsets<RawCameraSequenceKeyframe>(
                bytes, Sizes::CameraSequenceHeader, header.Count);

        return std::shared_ptr<CameraSequence>(
            new CameraSequence(id, std::move(name), scene, header, *keyframes));
    }

    std::shared_ptr<Entities::EntityBase> CameraSequence::GetKeyframeRef(
        std::int16_t type, std::int16_t id)
    {
        if (type == static_cast<std::int16_t>(EntityType::Player))
        {
            if (id < Entities::PlayerEntity::PlayerCount())
            {
                auto&& players = Entities::PlayerEntity::Players();
                if (id < 0)
                {
                    CameraSequenceDetail::ThrowArrayIndexOutOfRange();
                }
                return ToEntityShared(players[static_cast<std::size_t>(id)]);
            }
            return nullptr;
        }

        if (type != -1 && id != -1)
        {
            std::shared_ptr<Entities::EntityBase> entity;
            if (RequireReference(_scene).TryGetEntity(id, entity))
            {
                return entity;
            }
        }
        return nullptr;
    }
}
