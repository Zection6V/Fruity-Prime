#pragma once

#include "../Formats/Formats.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace MphRead::Utility
{
    enum class RepackFilter : std::int32_t
    {
        All = 0,
        SinglePlayer = 1,
        Multiplayer = 2
    };

    class BinaryWriter final
    {
    public:
        [[nodiscard]] std::size_t Position() const noexcept;
        void Position(std::size_t value);
        [[nodiscard]] std::vector<std::uint8_t> ToArray() const;

        void Write(std::uint8_t value);
        void Write(std::int8_t value);
        void Write(std::uint16_t value);
        void Write(std::int16_t value);
        void Write(std::uint32_t value);
        void Write(std::int32_t value);

        void WriteString(const std::string& value, std::size_t length);
        void WriteFloat(float value);
        void WriteVector3(OpenTK::Mathematics::Vector3 value);
        void WriteVector4(OpenTK::Mathematics::Vector4 value);
        void WriteColorRgb(ColorRgb value);
        void WriteByte(bool value);
        void WriteInt(bool value);

    private:
        [[nodiscard]] const std::vector<std::uint8_t>& Bytes() const noexcept;

        std::vector<std::uint8_t> _bytes{};
        std::size_t _position = 0;

        void WriteRaw(std::uint32_t value, std::size_t count);
    };

    class Repack final
    {
    public:
        [[nodiscard]] static std::vector<std::uint8_t> RepackMphEntities(const std::string& room);
        [[nodiscard]] static std::vector<std::uint8_t> RepackFhEntities(
            const std::string& room, RepackFilter filter = RepackFilter::All);
        [[nodiscard]] static std::vector<std::uint8_t> RepackHook(
            const std::string& path, bool firstHunt);
        [[nodiscard]] static std::vector<std::uint8_t> TestEntityEdit();
        static void TestEntities();

        static void CompareRooms(
            const std::string& room1,
            const std::string& room2,
            const std::string& game1 = "amhe1",
            const std::string& game2 = "amhe1");
        static void PrintLayers(std::uint16_t mask);

        static void WriteVolume(BinaryWriter& writer, const CollisionVolume& volume);
        static void WriteFhVolume(BinaryWriter& writer, const CollisionVolume& volume);

        Repack() = delete;
        Repack(const Repack&) = delete;
        Repack& operator=(const Repack&) = delete;
    };
}
