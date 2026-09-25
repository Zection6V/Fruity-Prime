#include "Parser.hpp"

#include "../Formats/Enums.hpp"
#include "../NativeRuntime/System/IO.hpp"
#include "../NativeRuntime/System/Console.hpp"
#include "NativeRuntime/System/Globalization.hpp"

#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <typeinfo>
#include <utility>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#if !defined(_WIN32) && (defined(__unix__) || defined(__APPLE__))
#include <unistd.h>
#endif

using ::MphRead::NativeRuntime::ConsoleWrite;
using ::MphRead::NativeRuntime::ConsoleWriteLine;

namespace
{

    [[nodiscard]] std::string EnumToString(const std::type_info& enumType, std::int32_t value)
    {
        if (enumType == typeid(MphRead::PolygonMode))
        {
            const std::uint32_t raw = std::bit_cast<std::uint32_t>(value);
            switch (raw)
            {
            case static_cast<std::uint32_t>(MphRead::PolygonMode::Modulate):
                return "Modulate";
            case static_cast<std::uint32_t>(MphRead::PolygonMode::Decal):
                return "Decal";
            case static_cast<std::uint32_t>(MphRead::PolygonMode::Toon):
                return "Toon";
            case static_cast<std::uint32_t>(MphRead::PolygonMode::Shadow):
                return "Shadow";
            default:
                return ::MphRead::NativeRuntime::ToString(raw);
            }
        }
        return ::MphRead::NativeRuntime::ToString(value);
    }

}

namespace MphRead::Utility
{
    Parser::Thing::Thing(std::int32_t bit, std::string name, ThingType type)
        : Name(std::move(name)), Type(type), EnumType(nullptr), BitFrom(bit), BitTo(bit)
    {
    }

    Parser::Thing::Thing(std::int32_t bit, std::string name, const std::type_info& enumType)
        : Name(std::move(name)), Type(ThingType::Enum), EnumType(&enumType), BitFrom(bit), BitTo(bit)
    {
    }

    Parser::Thing::Thing(
        std::int32_t bitFrom, std::int32_t bitTo, std::string name, ThingType type)
        : Name(std::move(name)), Type(type), EnumType(nullptr), BitFrom(bitFrom), BitTo(bitTo)
    {
    }

    Parser::Thing::Thing(
        std::int32_t bitFrom, std::int32_t bitTo, std::string name, const std::type_info& enumType)
        : Name(std::move(name)), Type(ThingType::Enum), EnumType(&enumType), BitFrom(bitFrom), BitTo(bitTo)
    {
    }

    std::string Parser::Thing::Get(std::int32_t value) const
    {
        const std::int32_t width = ManagedAdd(ManagedSubtract(BitTo, BitFrom), 1);
        const std::int32_t mask = ManagedNot(ManagedLeftShift(-1, width));
        value = ManagedAnd(ManagedArithmeticRightShift(value, BitFrom), mask);

        std::string output;
        if (Type == ThingType::Boolean)
        {
            output = value == 0 ? "No" : "Yes";
        }
        else if (Type == ThingType::Enum)
        {
            if (EnumType == nullptr)
            {
                throw std::invalid_argument("enumType");
            }
            output = EnumToString(*EnumType, value);
        }
        else
        {
            output = ::MphRead::NativeRuntime::ToString(value);
        }
        return Name + ": " + output;
    }

    const std::unordered_map<std::string, std::vector<Parser::Thing>> Parser::_things = {
        {
            "POLYGON_ATTR",
            {
                Thing(0, "Light 1", ThingType::Boolean),
                Thing(1, "Light 2", ThingType::Boolean),
                Thing(2, "Light 3", ThingType::Boolean),
                Thing(3, "Light 4", ThingType::Boolean),
                Thing(4, 5, "Polygon mode", typeid(PolygonMode)),
                Thing(6, "Back face", ThingType::Boolean),
                Thing(7, "Front face", ThingType::Boolean),
                Thing(11, "Set new depth", ThingType::Boolean),
                Thing(12, "Render far", ThingType::Boolean),
                Thing(13, "Render 1-dot", ThingType::Boolean),
                Thing(14, "Equal depth test", ThingType::Boolean),
                Thing(15, "Enable fog", ThingType::Boolean),
                Thing(16, 20, "Alpha", ThingType::General),
                Thing(24, 29, "Polygon ID", ThingType::General)
            }
        }
    };

    void Parser::ParseFloat(std::uint64_t value)
    {
        const double d = std::bit_cast<double>(value);
        ConsoleWriteLine(::MphRead::NativeRuntime::ToString(d) + " (" + ::MphRead::NativeRuntime::ToString(d / 4096.0F) + ")");
    }

    void Parser::MainLoop()
    {
        while (true)
        {
            ::MphRead::NativeRuntime::ConsoleClear();
            ConsoleWriteLine(std::string("1: POLYGON_ATTR") + std::string(::MphRead::NativeRuntime::EnvironmentNewLine()) + "x: quit");
            const std::string input = ::MphRead::NativeRuntime::ConsoleReadLine().value_or(std::string());
            if (input == "x" || input == "X")
            {
                break;
            }

            const std::string* type = nullptr;
            static const std::string polygonAttr = "POLYGON_ATTR";
            if (input == "1")
            {
                type = &polygonAttr;
            }
            if (type != nullptr)
            {
                bool hasOutput = false;
                std::string output;
                while (true)
                {
                    ::MphRead::NativeRuntime::ConsoleClear();
                    if (hasOutput)
                    {
                        ConsoleWriteLine(output);
                        ConsoleWriteLine();
                        output.clear();
                        hasOutput = false;
                    }
                    ConsoleWrite("Value: ");
                    const std::string value = ::MphRead::NativeRuntime::ConsoleReadLine().value_or(std::string());
                    if (value == "x" || value == "X")
                    {
                        break;
                    }

                    std::int32_t result = 0;
                    if (value.size() <= 8 && ::MphRead::NativeRuntime::Int32TryParseHexNumber(value, result))
                    {
                        output = ::MphRead::NativeRuntime::ToString(result, "B32");
                        const auto& things = _things.at(*type);
                        for (const Thing& thing : things)
                        {
                            output += ::MphRead::NativeRuntime::EnvironmentNewLine();
                            output += thing.Get(result);
                        }
                        hasOutput = true;
                    }
                }
            }
        }
    }
}
