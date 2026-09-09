#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace fruityprime::frontend {

struct Header {
    std::array<std::uint8_t, 4> type{};
    std::uint16_t field4 = 0;
    std::uint8_t field6 = 0;
    std::uint8_t field7 = 0;
    std::uint32_t offset1 = 0;
    std::uint32_t offset2 = 0;
    std::uint32_t offset3 = 0;
};

struct MenuStruct1 {
    std::uint32_t field0 = 0;
    std::uint32_t offset1 = 0;
    std::uint32_t offset2 = 0;
    std::uint32_t offset3 = 0;
    std::uint32_t offset4 = 0;
    std::uint16_t field14 = 0;
    std::uint16_t field16 = 0;
    std::uint32_t field18 = 0;
    std::uint8_t index = 0;
    std::uint8_t field1d = 0;
    std::uint8_t field1e = 0;
    std::uint8_t flags = 0;
};

struct MenuStruct1A {
    std::uint32_t offset1 = 0;
    std::uint32_t offset2 = 0;
    std::uint32_t offset3 = 0;
    std::uint32_t offset4 = 0;
    std::uint32_t field10 = 0;
    std::uint32_t field14 = 0;
    std::uint32_t field18 = 0;
    std::uint32_t field1c = 0;
    std::uint32_t field20 = 0;
    std::uint32_t field24 = 0;
    std::uint32_t field28 = 0;
    std::uint32_t offset5 = 0;
    std::uint32_t offset6 = 0;
    std::uint8_t field34 = 0;
    std::uint8_t field35 = 0;
    std::uint16_t field36 = 0;
    std::uint8_t flags = 0;
    std::uint8_t field39 = 0;
    std::uint16_t field3a = 0;
};

struct MenuStruct1A5 {
    std::uint8_t field0 = 0;
    std::uint8_t field1 = 0;
    std::uint16_t field2 = 0;
};

struct MenuStruct1A6 {
    std::array<std::int32_t, 11> fields{};
};

struct MenuStruct1A1 {
    std::uint16_t field0 = 0;
    std::uint8_t field2 = 0;
    std::uint8_t flags = 0;
    std::uint32_t field4 = 0;
    std::uint32_t field8 = 0;
    std::uint32_t fieldc = 0;
    std::uint32_t offset1 = 0;
    std::uint16_t field14 = 0;
    std::uint8_t field16 = 0;
    std::uint8_t field17 = 0;
};

struct MenuStruct1A2 {
    std::uint8_t field0 = 0;
    std::uint8_t field1 = 0;
    std::uint16_t field2 = 0;
    std::uint32_t offset1 = 0;
};

struct MenuStruct1A3 {
    std::uint32_t field0 = 0;
    std::uint32_t offset1 = 0;
};

struct MenuStruct1A4 {
    std::array<std::int32_t, 5> fields{};
    std::uint8_t flags = 0;
    std::uint8_t field15 = 0;
    std::uint16_t field16 = 0;
};

struct MenuStruct1B {
    std::uint32_t field0 = 0;
    MenuStruct1A1 struct1a1;
    std::uint8_t flags = 0;
    std::uint8_t field1d = 0;
    std::uint16_t field1e = 0;
};

struct MenuStruct2 {
    std::uint32_t field0 = 0;
    std::uint32_t field4 = 0;
    std::uint32_t offset1 = 0;
};

struct MenuStruct2A {
    std::uint8_t field0 = 0;
    std::uint8_t filename_length = 0;
    std::uint16_t field2 = 0;
    std::uint32_t field4 = 0;
    std::uint32_t filename_offset = 0;
};

struct Menu1Entry {
    MenuStruct1 value;
    std::vector<MenuStruct1A> children;
};

class File {
public:
    [[nodiscard]] static File parse(std::span<const std::uint8_t> bytes);
    [[nodiscard]] static File read_file(const std::filesystem::path& path);

    [[nodiscard]] const Header& header() const noexcept { return header_; }
    [[nodiscard]] bool is_marm() const noexcept {
        return header_.type == std::array<std::uint8_t, 4>{'M', 'A', 'R', 'M'};
    }
    [[nodiscard]] const std::vector<Menu1Entry>& menu1() const noexcept {
        return menu1_;
    }
    [[nodiscard]] const std::vector<MenuStruct2>& menu2() const noexcept {
        return menu2_;
    }

private:
    Header header_;
    std::vector<Menu1Entry> menu1_;
    std::vector<MenuStruct2> menu2_;
};

static_assert(sizeof(Header) == 20);
static_assert(sizeof(MenuStruct1) == 32);
static_assert(sizeof(MenuStruct1A) == 60);
static_assert(sizeof(MenuStruct1A5) == 4);
static_assert(sizeof(MenuStruct1A6) == 44);
static_assert(sizeof(MenuStruct1A1) == 24);
static_assert(sizeof(MenuStruct1A2) == 8);
static_assert(sizeof(MenuStruct1A3) == 8);
static_assert(sizeof(MenuStruct1A4) == 24);
static_assert(sizeof(MenuStruct1B) == 32);
static_assert(sizeof(MenuStruct2) == 12);
static_assert(sizeof(MenuStruct2A) == 12);

} // namespace fruityprime::frontend

namespace MphReadNative {
namespace Frontend = ::fruityprime::frontend;
}
