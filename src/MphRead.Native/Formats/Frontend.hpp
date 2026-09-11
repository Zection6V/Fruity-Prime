#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace MphRead::Formats
{
    // size: 20
    struct FrontendHeader
    {
        // C# [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)] char[]
        char Type[4]; // MARM
        std::uint16_t Field4;
        std::uint8_t Field6;
        std::uint8_t Field7;
        std::uint32_t Offfset1; // null-terminated list of MenuStruct1*
        std::uint32_t Offfset2; // ntl of MenuStruct2*
        std::uint32_t Offfset3;
    };

    // size: 32
    struct MenuStruct1
    {
        // 021E87F8 - offset 4 in use
        std::uint32_t Field0;
        std::uint32_t Offset1; // unused in metroidhunters.bin
        std::uint32_t Offset2; // ntl of MenuStruct1A*
        std::uint32_t Offset3; // ntl of MenuStruct1A1*
        std::uint32_t Offset4; // ntl of MenuStruct1B*
        std::uint16_t Field14; // index into this struct's list of MenuStruct1A*
        std::uint16_t Field16;
        std::uint32_t Field18;
        std::uint8_t Index; // position in the list of MenuStruct1
        std::uint8_t Field1D;
        std::uint8_t Field1E;
        std::uint8_t Flags;
    };

    // size: 60
    struct MenuStruct1A
    {
        std::uint32_t Offset1; // unused in metroidhunters.bin
        std::uint32_t Offset2; // ntl of MenuStruct1A1*
        std::uint32_t Offset3; // ntl of MenuStruct1A5*
        std::uint32_t Offset4; // ntl of MenuStruct1A2*
        std::uint32_t Field10; // runtime single pointer to a MenuStruct1A2
        std::uint32_t Field14;
        std::uint32_t Field18;
        std::uint32_t Field1C; // (?) runtime single pointer, maybe?
        std::uint32_t Field20;
        std::uint32_t Field24;
        std::uint32_t Field28;
        std::uint32_t Offset5; // MenuStruct1A6 -- only set for one MenuStruct1A in metroidhunters.bin (22140BC)
        std::uint32_t Offset6; // pointer to list of ints, can be external
        std::uint8_t Field34;
        std::uint8_t Field35;
        std::uint16_t Field36;
        std::uint8_t Flags;
        std::uint8_t Field39;
        std::uint16_t Field3A;
    };

    // size: 4
    struct MenuStruct1A5
    {
        std::uint8_t Field0;
        std::uint8_t Field1;
        std::uint16_t Field2; // index into the list of MenuStruct1A* on the parent of MenuStruct1 of the parent MenuStruct1A
    };

    // size: 44
    struct MenuStruct1A6
    {
        std::int32_t Field0;
        std::int32_t Field4;
        std::int32_t Field8;
        std::int32_t FieldC;
        std::int32_t Field10;
        std::int32_t Field14;
        std::int32_t Field18;
        std::int32_t Field1C;
        std::int32_t Field20;
        std::int32_t Field24;
        std::int32_t Field28;
    };

    // size: 24
    struct MenuStruct1A1
    {
        std::uint16_t Field0;
        std::uint8_t Field2;
        std::uint8_t Flags;
        std::uint32_t Field4;
        std::uint32_t Field8;
        std::uint32_t FieldC;
        std::uint32_t Offset1; // ntl of pointers to int pairs -- passed to call_pair_func_ptr
        std::uint16_t Field14;
        std::uint8_t Field16;
        std::uint8_t Field17;
    };

    // size: 8
    struct MenuStruct1A2
    {
        std::uint8_t Field0; // if 1, Offset1 is converted to MenuStruct1A3*
        std::uint8_t Field1;
        std::uint16_t Field2;
        std::uint32_t Offset1; // else, this is a ushort index into the MenuStruct2* list (if not 0xFFFF) and another ushort value (flags?)
    };

    // C# source comment says size: 4, but the two uint fields marshal to 8 bytes.
    struct MenuStruct1A3
    {
        std::uint32_t Field0;
        std::uint32_t Offset1; // MenuStruct1A4*
    };

    // size: 24
    struct MenuStruct1A4
    {
        std::int32_t Field0;
        std::int32_t Field4;
        std::int32_t Field8;
        std::int32_t FieldC;
        std::int32_t Field10;
        std::uint8_t Flags;
        std::uint8_t Field15;
        std::uint16_t Field16;
    };

    // size: 32
    struct MenuStruct1B
    {
        std::uint32_t Field0;
        MenuStruct1A1 Struct1A1;
        std::uint8_t Flags;
        std::uint8_t Field1D;
        std::uint16_t Field1E;
    };

    // size: 12
    struct MenuStruct2
    {
        std::uint32_t Field0;
        std::uint32_t Field4; // CModel*
        std::uint32_t Offset1; // ntl of pairs of MenuStruct2A* -- first for model, second for animation
    };

    // size: 12
    struct MenuStruct2A
    {
        std::uint8_t Field0;
        std::uint8_t FilenameLength; // includes terminator and 0xBB padding to multiple of 4 bytes
        std::uint16_t Field2;
        std::uint32_t Field4;
        std::uint32_t FilenameOffset; // char* (becomes pointer to model/animation in download play version)
    };

    class Frontend final
    {
    public:
        Frontend() = delete;

        static void Parse();

    private:
        static void Nop();
    };

    static_assert(std::is_standard_layout_v<FrontendHeader> && std::is_trivially_copyable_v<FrontendHeader>);
    static_assert(sizeof(FrontendHeader) == 20);
    static_assert(offsetof(FrontendHeader, Type) == 0);
    static_assert(offsetof(FrontendHeader, Field4) == 4);
    static_assert(offsetof(FrontendHeader, Field6) == 6);
    static_assert(offsetof(FrontendHeader, Field7) == 7);
    static_assert(offsetof(FrontendHeader, Offfset1) == 8);
    static_assert(offsetof(FrontendHeader, Offfset2) == 12);
    static_assert(offsetof(FrontendHeader, Offfset3) == 16);

    static_assert(std::is_standard_layout_v<MenuStruct1> && std::is_trivially_copyable_v<MenuStruct1>);
    static_assert(sizeof(MenuStruct1) == 32);
    static_assert(offsetof(MenuStruct1, Field0) == 0);
    static_assert(offsetof(MenuStruct1, Offset1) == 4);
    static_assert(offsetof(MenuStruct1, Offset2) == 8);
    static_assert(offsetof(MenuStruct1, Offset3) == 12);
    static_assert(offsetof(MenuStruct1, Offset4) == 16);
    static_assert(offsetof(MenuStruct1, Field14) == 20);
    static_assert(offsetof(MenuStruct1, Field16) == 22);
    static_assert(offsetof(MenuStruct1, Field18) == 24);
    static_assert(offsetof(MenuStruct1, Index) == 28);
    static_assert(offsetof(MenuStruct1, Field1D) == 29);
    static_assert(offsetof(MenuStruct1, Field1E) == 30);
    static_assert(offsetof(MenuStruct1, Flags) == 31);

    static_assert(std::is_standard_layout_v<MenuStruct1A> && std::is_trivially_copyable_v<MenuStruct1A>);
    static_assert(sizeof(MenuStruct1A) == 60);
    static_assert(offsetof(MenuStruct1A, Offset1) == 0);
    static_assert(offsetof(MenuStruct1A, Offset2) == 4);
    static_assert(offsetof(MenuStruct1A, Offset3) == 8);
    static_assert(offsetof(MenuStruct1A, Offset4) == 12);
    static_assert(offsetof(MenuStruct1A, Field10) == 16);
    static_assert(offsetof(MenuStruct1A, Field14) == 20);
    static_assert(offsetof(MenuStruct1A, Field18) == 24);
    static_assert(offsetof(MenuStruct1A, Field1C) == 28);
    static_assert(offsetof(MenuStruct1A, Field20) == 32);
    static_assert(offsetof(MenuStruct1A, Field24) == 36);
    static_assert(offsetof(MenuStruct1A, Field28) == 40);
    static_assert(offsetof(MenuStruct1A, Offset5) == 44);
    static_assert(offsetof(MenuStruct1A, Offset6) == 48);
    static_assert(offsetof(MenuStruct1A, Field34) == 52);
    static_assert(offsetof(MenuStruct1A, Field35) == 53);
    static_assert(offsetof(MenuStruct1A, Field36) == 54);
    static_assert(offsetof(MenuStruct1A, Flags) == 56);
    static_assert(offsetof(MenuStruct1A, Field39) == 57);
    static_assert(offsetof(MenuStruct1A, Field3A) == 58);

    static_assert(std::is_standard_layout_v<MenuStruct1A5> && std::is_trivially_copyable_v<MenuStruct1A5>);
    static_assert(sizeof(MenuStruct1A5) == 4);
    static_assert(offsetof(MenuStruct1A5, Field0) == 0);
    static_assert(offsetof(MenuStruct1A5, Field1) == 1);
    static_assert(offsetof(MenuStruct1A5, Field2) == 2);

    static_assert(std::is_standard_layout_v<MenuStruct1A6> && std::is_trivially_copyable_v<MenuStruct1A6>);
    static_assert(sizeof(MenuStruct1A6) == 44);
    static_assert(offsetof(MenuStruct1A6, Field0) == 0);
    static_assert(offsetof(MenuStruct1A6, Field4) == 4);
    static_assert(offsetof(MenuStruct1A6, Field8) == 8);
    static_assert(offsetof(MenuStruct1A6, FieldC) == 12);
    static_assert(offsetof(MenuStruct1A6, Field10) == 16);
    static_assert(offsetof(MenuStruct1A6, Field14) == 20);
    static_assert(offsetof(MenuStruct1A6, Field18) == 24);
    static_assert(offsetof(MenuStruct1A6, Field1C) == 28);
    static_assert(offsetof(MenuStruct1A6, Field20) == 32);
    static_assert(offsetof(MenuStruct1A6, Field24) == 36);
    static_assert(offsetof(MenuStruct1A6, Field28) == 40);

    static_assert(std::is_standard_layout_v<MenuStruct1A1> && std::is_trivially_copyable_v<MenuStruct1A1>);
    static_assert(sizeof(MenuStruct1A1) == 24);
    static_assert(offsetof(MenuStruct1A1, Field0) == 0);
    static_assert(offsetof(MenuStruct1A1, Field2) == 2);
    static_assert(offsetof(MenuStruct1A1, Flags) == 3);
    static_assert(offsetof(MenuStruct1A1, Field4) == 4);
    static_assert(offsetof(MenuStruct1A1, Field8) == 8);
    static_assert(offsetof(MenuStruct1A1, FieldC) == 12);
    static_assert(offsetof(MenuStruct1A1, Offset1) == 16);
    static_assert(offsetof(MenuStruct1A1, Field14) == 20);
    static_assert(offsetof(MenuStruct1A1, Field16) == 22);
    static_assert(offsetof(MenuStruct1A1, Field17) == 23);

    static_assert(std::is_standard_layout_v<MenuStruct1A2> && std::is_trivially_copyable_v<MenuStruct1A2>);
    static_assert(sizeof(MenuStruct1A2) == 8);
    static_assert(offsetof(MenuStruct1A2, Field0) == 0);
    static_assert(offsetof(MenuStruct1A2, Field1) == 1);
    static_assert(offsetof(MenuStruct1A2, Field2) == 2);
    static_assert(offsetof(MenuStruct1A2, Offset1) == 4);

    static_assert(std::is_standard_layout_v<MenuStruct1A3> && std::is_trivially_copyable_v<MenuStruct1A3>);
    static_assert(sizeof(MenuStruct1A3) == 8);
    static_assert(offsetof(MenuStruct1A3, Field0) == 0);
    static_assert(offsetof(MenuStruct1A3, Offset1) == 4);

    static_assert(std::is_standard_layout_v<MenuStruct1A4> && std::is_trivially_copyable_v<MenuStruct1A4>);
    static_assert(sizeof(MenuStruct1A4) == 24);
    static_assert(offsetof(MenuStruct1A4, Field0) == 0);
    static_assert(offsetof(MenuStruct1A4, Field4) == 4);
    static_assert(offsetof(MenuStruct1A4, Field8) == 8);
    static_assert(offsetof(MenuStruct1A4, FieldC) == 12);
    static_assert(offsetof(MenuStruct1A4, Field10) == 16);
    static_assert(offsetof(MenuStruct1A4, Flags) == 20);
    static_assert(offsetof(MenuStruct1A4, Field15) == 21);
    static_assert(offsetof(MenuStruct1A4, Field16) == 22);

    static_assert(std::is_standard_layout_v<MenuStruct1B> && std::is_trivially_copyable_v<MenuStruct1B>);
    static_assert(sizeof(MenuStruct1B) == 32);
    static_assert(offsetof(MenuStruct1B, Field0) == 0);
    static_assert(offsetof(MenuStruct1B, Struct1A1) == 4);
    static_assert(offsetof(MenuStruct1B, Flags) == 28);
    static_assert(offsetof(MenuStruct1B, Field1D) == 29);
    static_assert(offsetof(MenuStruct1B, Field1E) == 30);

    static_assert(std::is_standard_layout_v<MenuStruct2> && std::is_trivially_copyable_v<MenuStruct2>);
    static_assert(sizeof(MenuStruct2) == 12);
    static_assert(offsetof(MenuStruct2, Field0) == 0);
    static_assert(offsetof(MenuStruct2, Field4) == 4);
    static_assert(offsetof(MenuStruct2, Offset1) == 8);

    static_assert(std::is_standard_layout_v<MenuStruct2A> && std::is_trivially_copyable_v<MenuStruct2A>);
    static_assert(sizeof(MenuStruct2A) == 12);
    static_assert(offsetof(MenuStruct2A, Field0) == 0);
    static_assert(offsetof(MenuStruct2A, FilenameLength) == 1);
    static_assert(offsetof(MenuStruct2A, Field2) == 2);
    static_assert(offsetof(MenuStruct2A, Field4) == 4);
    static_assert(offsetof(MenuStruct2A, FilenameOffset) == 8);
}
