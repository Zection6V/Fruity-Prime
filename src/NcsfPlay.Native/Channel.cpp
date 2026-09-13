#include "Channel.hpp"

#include "NC/SBNKInstrument.hpp"
#include "NC/SWAR.hpp"
#include "NC/SWAV.hpp"
#include "Player.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace
{
    [[noreturn]] void ThrowNullReference()
    {
        throw std::runtime_error("NullReferenceException");
    }

    [[nodiscard]] constexpr std::int32_t WrapAdd32(std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr std::int32_t WrapSub32(std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) - static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr std::int32_t WrapMul32(std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) * static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr std::int32_t WrapNegate32(std::int32_t value) noexcept
    {
        return std::bit_cast<std::int32_t>(0U - static_cast<std::uint32_t>(value));
    }

    [[nodiscard]] constexpr std::int8_t WrapSByte(std::int32_t value) noexcept
    {
        return std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(value));
    }

    [[nodiscard]] constexpr std::int32_t WrapInt32(std::int64_t value) noexcept
    {
        return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
    }

    [[nodiscard]] constexpr std::int32_t ArithmeticShiftRight32(std::int32_t value, unsigned count) noexcept
    {
        if (count == 0U)
            return value;
        const std::uint32_t bits = static_cast<std::uint32_t>(value);
        const std::uint32_t shifted = bits >> count;
        if (value >= 0)
            return static_cast<std::int32_t>(shifted);
        return std::bit_cast<std::int32_t>(shifted | (~0U << (32U - count)));
    }

    [[nodiscard]] constexpr std::int64_t ArithmeticShiftRight64(std::int64_t value, unsigned count) noexcept
    {
        if (count == 0U)
            return value;
        const std::uint64_t bits = static_cast<std::uint64_t>(value);
        const std::uint64_t shifted = bits >> count;
        if (value >= 0)
            return static_cast<std::int64_t>(shifted);
        return std::bit_cast<std::int64_t>(shifted | (~0ULL << (64U - count)));
    }

    [[nodiscard]] constexpr std::int64_t WrapShiftLeft64(std::int64_t value, unsigned count) noexcept
    {
        return std::bit_cast<std::int64_t>(static_cast<std::uint64_t>(value) << count);
    }
}

namespace NCSFCommon
{
    const std::array<std::int8_t, 33> LFO::SinTable =
    {
        0, 6, 12, 19, 25, 31, 37, 43, 49, 54, 60, 65, 71, 76, 81, 85, 90, 94,
        98, 102, 106, 109, 112, 115, 117, 120, 122, 123, 125, 126, 126, 127, 127
    };

    const std::array<std::uint8_t, 724> Channel::GetVolumeTable =
    {
        0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
        0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
        0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
        0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
        0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
        0x05, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
        0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x08, 0x08, 0x08, 0x08,
        0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
        0x09, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
        0x0b, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0e,
        0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x10, 0x10, 0x10, 0x10, 0x10,
        0x10, 0x11, 0x11, 0x11, 0x11, 0x11, 0x12, 0x12, 0x12, 0x12, 0x12, 0x13, 0x13, 0x13, 0x13, 0x14,
        0x14, 0x14, 0x14, 0x14, 0x15, 0x15, 0x15, 0x15, 0x16, 0x16, 0x16, 0x16, 0x17, 0x17, 0x17, 0x18,
        0x18, 0x18, 0x18, 0x19, 0x19, 0x19, 0x19, 0x1a, 0x1a, 0x1a, 0x1b, 0x1b, 0x1b, 0x1c, 0x1c, 0x1c,
        0x1d, 0x1d, 0x1d, 0x1e, 0x1e, 0x1e, 0x1f, 0x1f, 0x1f, 0x20, 0x20, 0x20, 0x21, 0x21, 0x22, 0x22,
        0x22, 0x23, 0x23, 0x24, 0x24, 0x24, 0x25, 0x25, 0x26, 0x26, 0x27, 0x27, 0x27, 0x28, 0x28, 0x29,
        0x29, 0x2a, 0x2a, 0x2b, 0x2b, 0x2c, 0x2c, 0x2d, 0x2d, 0x2e, 0x2e, 0x2f, 0x2f, 0x30, 0x31, 0x31,
        0x32, 0x32, 0x33, 0x33, 0x34, 0x35, 0x35, 0x36, 0x36, 0x37, 0x38, 0x38, 0x39, 0x3a, 0x3a, 0x3b,
        0x3c, 0x3c, 0x3d, 0x3e, 0x3f, 0x3f, 0x40, 0x41, 0x42, 0x42, 0x43, 0x44, 0x45, 0x45, 0x46, 0x47,
        0x48, 0x49, 0x4a, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x52, 0x53, 0x54, 0x55,
        0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x67,
        0x68, 0x69, 0x6a, 0x6b, 0x6d, 0x6e, 0x6f, 0x71, 0x72, 0x73, 0x75, 0x76, 0x77, 0x79, 0x7a, 0x7b,
        0x7d, 0x7e, 0x7f, 0x20, 0x21, 0x21, 0x21, 0x22, 0x22, 0x23, 0x23, 0x23, 0x24, 0x24, 0x25, 0x25,
        0x26, 0x26, 0x26, 0x27, 0x27, 0x28, 0x28, 0x29, 0x29, 0x2a, 0x2a, 0x2b, 0x2b, 0x2c, 0x2c, 0x2d,
        0x2d, 0x2e, 0x2e, 0x2f, 0x2f, 0x30, 0x30, 0x31, 0x31, 0x32, 0x33, 0x33, 0x34, 0x34, 0x35, 0x36,
        0x36, 0x37, 0x37, 0x38, 0x39, 0x39, 0x3a, 0x3b, 0x3b, 0x3c, 0x3d, 0x3e, 0x3e, 0x3f, 0x40, 0x40,
        0x41, 0x42, 0x43, 0x43, 0x44, 0x45, 0x46, 0x47, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4d,
        0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d,
        0x5e, 0x5f, 0x60, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6f, 0x70,
        0x71, 0x73, 0x74, 0x75, 0x77, 0x78, 0x79, 0x7b, 0x7c, 0x7e, 0x7e, 0x40, 0x41, 0x42, 0x43, 0x43,
        0x44, 0x45, 0x46, 0x47, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51,
        0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x61,
        0x62, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6b, 0x6c, 0x6d, 0x6e, 0x70, 0x71, 0x72, 0x74, 0x75,
        0x76, 0x78, 0x79, 0x7b, 0x7c, 0x7d, 0x7e, 0x40, 0x41, 0x42, 0x42, 0x43, 0x44, 0x45, 0x46, 0x46,
        0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
        0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62, 0x63, 0x65, 0x66,
        0x67, 0x68, 0x69, 0x6a, 0x6c, 0x6d, 0x6e, 0x6f, 0x71, 0x72, 0x73, 0x75, 0x76, 0x77, 0x79, 0x7a,
        0x7c, 0x7d, 0x7e, 0x7f
    };

    const std::array<std::uint16_t, 768> Channel::GetPitchTable =
    {
        0x0000, 0x003b, 0x0076, 0x00b2, 0x00ed, 0x0128, 0x0164, 0x019f,
        0x01db, 0x0217, 0x0252, 0x028e, 0x02ca, 0x0305, 0x0341, 0x037d,
        0x03b9, 0x03f5, 0x0431, 0x046e, 0x04aa, 0x04e6, 0x0522, 0x055f,
        0x059b, 0x05d8, 0x0614, 0x0651, 0x068d, 0x06ca, 0x0707, 0x0743,
        0x0780, 0x07bd, 0x07fa, 0x0837, 0x0874, 0x08b1, 0x08ef, 0x092c,
        0x0969, 0x09a7, 0x09e4, 0x0a21, 0x0a5f, 0x0a9c, 0x0ada, 0x0b18,
        0x0b56, 0x0b93, 0x0bd1, 0x0c0f, 0x0c4d, 0x0c8b, 0x0cc9, 0x0d07,
        0x0d45, 0x0d84, 0x0dc2, 0x0e00, 0x0e3f, 0x0e7d, 0x0ebc, 0x0efa,
        0x0f39, 0x0f78, 0x0fb6, 0x0ff5, 0x1034, 0x1073, 0x10b2, 0x10f1,
        0x1130, 0x116f, 0x11ae, 0x11ee, 0x122d, 0x126c, 0x12ac, 0x12eb,
        0x132b, 0x136b, 0x13aa, 0x13ea, 0x142a, 0x146a, 0x14a9, 0x14e9,
        0x1529, 0x1569, 0x15aa, 0x15ea, 0x162a, 0x166a, 0x16ab, 0x16eb,
        0x172c, 0x176c, 0x17ad, 0x17ed, 0x182e, 0x186f, 0x18b0, 0x18f0,
        0x1931, 0x1972, 0x19b3, 0x19f5, 0x1a36, 0x1a77, 0x1ab8, 0x1afa,
        0x1b3b, 0x1b7d, 0x1bbe, 0x1c00, 0x1c41, 0x1c83, 0x1cc5, 0x1d07,
        0x1d48, 0x1d8a, 0x1dcc, 0x1e0e, 0x1e51, 0x1e93, 0x1ed5, 0x1f17,
        0x1f5a, 0x1f9c, 0x1fdf, 0x2021, 0x2064, 0x20a6, 0x20e9, 0x212c,
        0x216f, 0x21b2, 0x21f5, 0x2238, 0x227b, 0x22be, 0x2301, 0x2344,
        0x2388, 0x23cb, 0x240e, 0x2452, 0x2496, 0x24d9, 0x251d, 0x2561,
        0x25a4, 0x25e8, 0x262c, 0x2670, 0x26b4, 0x26f8, 0x273d, 0x2781,
        0x27c5, 0x280a, 0x284e, 0x2892, 0x28d7, 0x291c, 0x2960, 0x29a5,
        0x29ea, 0x2a2f, 0x2a74, 0x2ab9, 0x2afe, 0x2b43, 0x2b88, 0x2bcd,
        0x2c13, 0x2c58, 0x2c9d, 0x2ce3, 0x2d28, 0x2d6e, 0x2db4, 0x2df9,
        0x2e3f, 0x2e85, 0x2ecb, 0x2f11, 0x2f57, 0x2f9d, 0x2fe3, 0x302a,
        0x3070, 0x30b6, 0x30fd, 0x3143, 0x318a, 0x31d0, 0x3217, 0x325e,
        0x32a5, 0x32ec, 0x3332, 0x3379, 0x33c1, 0x3408, 0x344f, 0x3496,
        0x34dd, 0x3525, 0x356c, 0x35b4, 0x35fb, 0x3643, 0x368b, 0x36d3,
        0x371a, 0x3762, 0x37aa, 0x37f2, 0x383a, 0x3883, 0x38cb, 0x3913,
        0x395c, 0x39a4, 0x39ed, 0x3a35, 0x3a7e, 0x3ac6, 0x3b0f, 0x3b58,
        0x3ba1, 0x3bea, 0x3c33, 0x3c7c, 0x3cc5, 0x3d0e, 0x3d58, 0x3da1,
        0x3dea, 0x3e34, 0x3e7d, 0x3ec7, 0x3f11, 0x3f5a, 0x3fa4, 0x3fee,
        0x4038, 0x4082, 0x40cc, 0x4116, 0x4161, 0x41ab, 0x41f5, 0x4240,
        0x428a, 0x42d5, 0x431f, 0x436a, 0x43b5, 0x4400, 0x444b, 0x4495,
        0x44e1, 0x452c, 0x4577, 0x45c2, 0x460d, 0x4659, 0x46a4, 0x46f0,
        0x473b, 0x4787, 0x47d3, 0x481e, 0x486a, 0x48b6, 0x4902, 0x494e,
        0x499a, 0x49e6, 0x4a33, 0x4a7f, 0x4acb, 0x4b18, 0x4b64, 0x4bb1,
        0x4bfe, 0x4c4a, 0x4c97, 0x4ce4, 0x4d31, 0x4d7e, 0x4dcb, 0x4e18,
        0x4e66, 0x4eb3, 0x4f00, 0x4f4e, 0x4f9b, 0x4fe9, 0x5036, 0x5084,
        0x50d2, 0x5120, 0x516e, 0x51bc, 0x520a, 0x5258, 0x52a6, 0x52f4,
        0x5343, 0x5391, 0x53e0, 0x542e, 0x547d, 0x54cc, 0x551a, 0x5569,
        0x55b8, 0x5607, 0x5656, 0x56a5, 0x56f4, 0x5744, 0x5793, 0x57e2,
        0x5832, 0x5882, 0x58d1, 0x5921, 0x5971, 0x59c1, 0x5a10, 0x5a60,
        0x5ab0, 0x5b01, 0x5b51, 0x5ba1, 0x5bf1, 0x5c42, 0x5c92, 0x5ce3,
        0x5d34, 0x5d84, 0x5dd5, 0x5e26, 0x5e77, 0x5ec8, 0x5f19, 0x5f6a,
        0x5fbb, 0x600d, 0x605e, 0x60b0, 0x6101, 0x6153, 0x61a4, 0x61f6,
        0x6248, 0x629a, 0x62ec, 0x633e, 0x6390, 0x63e2, 0x6434, 0x6487,
        0x64d9, 0x652c, 0x657e, 0x65d1, 0x6624, 0x6676, 0x66c9, 0x671c,
        0x676f, 0x67c2, 0x6815, 0x6869, 0x68bc, 0x690f, 0x6963, 0x69b6,
        0x6a0a, 0x6a5e, 0x6ab1, 0x6b05, 0x6b59, 0x6bad, 0x6c01, 0x6c55,
        0x6caa, 0x6cfe, 0x6d52, 0x6da7, 0x6dfb, 0x6e50, 0x6ea4, 0x6ef9,
        0x6f4e, 0x6fa3, 0x6ff8, 0x704d, 0x70a2, 0x70f7, 0x714d, 0x71a2,
        0x71f7, 0x724d, 0x72a2, 0x72f8, 0x734e, 0x73a4, 0x73fa, 0x7450,
        0x74a6, 0x74fc, 0x7552, 0x75a8, 0x75ff, 0x7655, 0x76ac, 0x7702,
        0x7759, 0x77b0, 0x7807, 0x785e, 0x78b4, 0x790c, 0x7963, 0x79ba,
        0x7a11, 0x7a69, 0x7ac0, 0x7b18, 0x7b6f, 0x7bc7, 0x7c1f, 0x7c77,
        0x7ccf, 0x7d27, 0x7d7f, 0x7dd7, 0x7e2f, 0x7e88, 0x7ee0, 0x7f38,
        0x7f91, 0x7fea, 0x8042, 0x809b, 0x80f4, 0x814d, 0x81a6, 0x81ff,
        0x8259, 0x82b2, 0x830b, 0x8365, 0x83be, 0x8418, 0x8472, 0x84cb,
        0x8525, 0x857f, 0x85d9, 0x8633, 0x868e, 0x86e8, 0x8742, 0x879d,
        0x87f7, 0x8852, 0x88ac, 0x8907, 0x8962, 0x89bd, 0x8a18, 0x8a73,
        0x8ace, 0x8b2a, 0x8b85, 0x8be0, 0x8c3c, 0x8c97, 0x8cf3, 0x8d4f,
        0x8dab, 0x8e07, 0x8e63, 0x8ebf, 0x8f1b, 0x8f77, 0x8fd4, 0x9030,
        0x908c, 0x90e9, 0x9146, 0x91a2, 0x91ff, 0x925c, 0x92b9, 0x9316,
        0x9373, 0x93d1, 0x942e, 0x948c, 0x94e9, 0x9547, 0x95a4, 0x9602,
        0x9660, 0x96be, 0x971c, 0x977a, 0x97d8, 0x9836, 0x9895, 0x98f3,
        0x9952, 0x99b0, 0x9a0f, 0x9a6e, 0x9acd, 0x9b2c, 0x9b8b, 0x9bea,
        0x9c49, 0x9ca8, 0x9d08, 0x9d67, 0x9dc7, 0x9e26, 0x9e86, 0x9ee6,
        0x9f46, 0x9fa6, 0xa006, 0xa066, 0xa0c6, 0xa127, 0xa187, 0xa1e8,
        0xa248, 0xa2a9, 0xa30a, 0xa36b, 0xa3cc, 0xa42d, 0xa48e, 0xa4ef,
        0xa550, 0xa5b2, 0xa613, 0xa675, 0xa6d6, 0xa738, 0xa79a, 0xa7fc,
        0xa85e, 0xa8c0, 0xa922, 0xa984, 0xa9e7, 0xaa49, 0xaaac, 0xab0e,
        0xab71, 0xabd4, 0xac37, 0xac9a, 0xacfd, 0xad60, 0xadc3, 0xae27,
        0xae8a, 0xaeed, 0xaf51, 0xafb5, 0xb019, 0xb07c, 0xb0e0, 0xb145,
        0xb1a9, 0xb20d, 0xb271, 0xb2d6, 0xb33a, 0xb39f, 0xb403, 0xb468,
        0xb4cd, 0xb532, 0xb597, 0xb5fc, 0xb662, 0xb6c7, 0xb72c, 0xb792,
        0xb7f7, 0xb85d, 0xb8c3, 0xb929, 0xb98f, 0xb9f5, 0xba5b, 0xbac1,
        0xbb28, 0xbb8e, 0xbbf5, 0xbc5b, 0xbcc2, 0xbd29, 0xbd90, 0xbdf7,
        0xbe5e, 0xbec5, 0xbf2c, 0xbf94, 0xbffb, 0xc063, 0xc0ca, 0xc132,
        0xc19a, 0xc202, 0xc26a, 0xc2d2, 0xc33a, 0xc3a2, 0xc40b, 0xc473,
        0xc4dc, 0xc544, 0xc5ad, 0xc616, 0xc67f, 0xc6e8, 0xc751, 0xc7bb,
        0xc824, 0xc88d, 0xc8f7, 0xc960, 0xc9ca, 0xca34, 0xca9e, 0xcb08,
        0xcb72, 0xcbdc, 0xcc47, 0xccb1, 0xcd1b, 0xcd86, 0xcdf1, 0xce5b,
        0xcec6, 0xcf31, 0xcf9c, 0xd008, 0xd073, 0xd0de, 0xd14a, 0xd1b5,
        0xd221, 0xd28d, 0xd2f8, 0xd364, 0xd3d0, 0xd43d, 0xd4a9, 0xd515,
        0xd582, 0xd5ee, 0xd65b, 0xd6c7, 0xd734, 0xd7a1, 0xd80e, 0xd87b,
        0xd8e9, 0xd956, 0xd9c3, 0xda31, 0xda9e, 0xdb0c, 0xdb7a, 0xdbe8,
        0xdc56, 0xdcc4, 0xdd32, 0xdda0, 0xde0f, 0xde7d, 0xdeec, 0xdf5b,
        0xdfc9, 0xe038, 0xe0a7, 0xe116, 0xe186, 0xe1f5, 0xe264, 0xe2d4,
        0xe343, 0xe3b3, 0xe423, 0xe493, 0xe503, 0xe573, 0xe5e3, 0xe654,
        0xe6c4, 0xe735, 0xe7a5, 0xe816, 0xe887, 0xe8f8, 0xe969, 0xe9da,
        0xea4b, 0xeabc, 0xeb2e, 0xeb9f, 0xec11, 0xec83, 0xecf5, 0xed66,
        0xedd9, 0xee4b, 0xeebd, 0xef2f, 0xefa2, 0xf014, 0xf087, 0xf0fa,
        0xf16d, 0xf1e0, 0xf253, 0xf2c6, 0xf339, 0xf3ad, 0xf420, 0xf494,
        0xf507, 0xf57b, 0xf5ef, 0xf663, 0xf6d7, 0xf74c, 0xf7c0, 0xf834,
        0xf8a9, 0xf91e, 0xf992, 0xfa07, 0xfa7c, 0xfaf1, 0xfb66, 0xfbdc,
        0xfc51, 0xfcc7, 0xfd3c, 0xfdb2, 0xfe28, 0xfe9e, 0xff14, 0xff8a
    };

    const std::array<std::int16_t, 128> Channel::convertSustainLookupTable =
    {
        -32768, -722, -721, -651, -601, -562, -530, -503,
        -480, -460, -442, -425, -410, -396, -383, -371,
        -360, -349, -339, -330, -321, -313, -305, -297,
        -289, -282, -276, -269, -263, -257, -251, -245,
        -239, -234, -229, -224, -219, -214, -210, -205,
        -201, -196, -192, -188, -184, -180, -176, -173,
        -169, -165, -162, -158, -155, -152, -149, -145,
        -142, -139, -136, -133, -130, -127, -125, -122,
        -119, -116, -114, -111, -109, -106, -103, -101,
        -99, -96, -94, -91, -89, -87, -85, -82,
        -80, -78, -76, -74, -72, -70, -68, -66,
        -64, -62, -60, -58, -56, -54, -52, -50,
        -49, -47, -45, -43, -42, -40, -38, -36,
        -35, -33, -31, -30, -28, -27, -25, -23,
        -22, -20, -19, -17, -16, -14, -13, -11,
        -10, -8, -7, -6, -4, -3, -1, 0
    };

    const std::array<std::uint8_t, 19> Channel::AttackCoefficientTable =
    {
        0, 1, 5, 14, 26, 38, 51, 63, 73, 84,
        92, 100, 109, 116, 123, 127, 132, 137, 143
    };

    const std::array<std::uint8_t, 4> Channel::SampleDataShiftTable = {0, 1, 2, 4};

    const std::array<std::array<float, 8>, 8> Channel::WaveDutyTable =
    {{
        {{-1.0F, -1.0F, -1.0F, -1.0F, -1.0F, -1.0F, -1.0F,  1.0F}},
        {{-1.0F, -1.0F, -1.0F, -1.0F, -1.0F, -1.0F,  1.0F,  1.0F}},
        {{-1.0F, -1.0F, -1.0F, -1.0F, -1.0F,  1.0F,  1.0F,  1.0F}},
        {{-1.0F, -1.0F, -1.0F, -1.0F,  1.0F,  1.0F,  1.0F,  1.0F}},
        {{-1.0F, -1.0F, -1.0F,  1.0F,  1.0F,  1.0F,  1.0F,  1.0F}},
        {{-1.0F, -1.0F,  1.0F,  1.0F,  1.0F,  1.0F,  1.0F,  1.0F}},
        {{-1.0F,  1.0F,  1.0F,  1.0F,  1.0F,  1.0F,  1.0F,  1.0F}},
        {{-1.0F, -1.0F, -1.0F, -1.0F, -1.0F, -1.0F, -1.0F, -1.0F}}
    }};

    std::uint8_t NDSSoundRegister::VolumeMultiplier() const noexcept { return _volumeMultiplier; }
    void NDSSoundRegister::VolumeMultiplier(std::uint8_t value) noexcept { _volumeMultiplier = value; }
    std::uint8_t NDSSoundRegister::VolumeDivisor() const noexcept { return _volumeDivisor; }
    void NDSSoundRegister::VolumeDivisor(std::uint8_t value) noexcept { _volumeDivisor = value; }
    std::uint8_t NDSSoundRegister::Panning() const noexcept { return _panning; }
    void NDSSoundRegister::Panning(std::uint8_t value) noexcept { _panning = value; }
    std::uint8_t NDSSoundRegister::WaveDuty() const noexcept { return _waveDuty; }
    void NDSSoundRegister::WaveDuty(std::uint8_t value) noexcept { _waveDuty = value; }
    std::uint8_t NDSSoundRegister::RepeatMode() const noexcept { return _repeatMode; }
    void NDSSoundRegister::RepeatMode(std::uint8_t value) noexcept { _repeatMode = value; }
    std::uint8_t NDSSoundRegister::Format() const noexcept { return _format; }
    void NDSSoundRegister::Format(std::uint8_t value) noexcept { _format = value; }
    bool NDSSoundRegister::Enable() const noexcept { return _enable; }
    void NDSSoundRegister::Enable(bool value) noexcept { _enable = value; }
    const std::shared_ptr<NC::SWAV>& NDSSoundRegister::Source() const noexcept { return _source; }
    void NDSSoundRegister::Source(std::shared_ptr<NC::SWAV> value) noexcept { _source = std::move(value); }
    std::uint16_t NDSSoundRegister::Timer() const noexcept { return _timer; }
    void NDSSoundRegister::Timer(std::uint16_t value) noexcept { _timer = value; }
    std::uint16_t NDSSoundRegister::PSGX() const noexcept { return _psgx; }
    void NDSSoundRegister::PSGX(std::uint16_t value) noexcept { _psgx = value; }
    float NDSSoundRegister::PSGLast() const noexcept { return _psgLast; }
    void NDSSoundRegister::PSGLast(float value) noexcept { _psgLast = value; }
    std::uint32_t NDSSoundRegister::PSGLastCount() const noexcept { return _psgLastCount; }
    void NDSSoundRegister::PSGLastCount(std::uint32_t value) noexcept { _psgLastCount = value; }
    double NDSSoundRegister::SamplePosition() const noexcept { return _samplePosition; }
    void NDSSoundRegister::SamplePosition(double value) noexcept { _samplePosition = value; }
    double NDSSoundRegister::SampleIncrease() const noexcept { return _sampleIncrease; }
    void NDSSoundRegister::SampleIncrease(double value) noexcept { _sampleIncrease = value; }
    std::uint32_t NDSSoundRegister::LoopStart() const noexcept { return _loopStart; }
    void NDSSoundRegister::LoopStart(std::uint32_t value) noexcept { _loopStart = value; }
    std::uint32_t NDSSoundRegister::Length() const noexcept { return _length; }
    void NDSSoundRegister::Length(std::uint32_t value) noexcept { _length = value; }
    std::uint32_t NDSSoundRegister::TotalLength() const noexcept { return _totalLength; }
    void NDSSoundRegister::TotalLength(std::uint32_t value) noexcept { _totalLength = value; }

    void NDSSoundRegister::ClearControlRegister() noexcept
    {
        _volumeMultiplier = _volumeDivisor = _panning = _waveDuty = _repeatMode = _format = 0;
        _enable = false;
    }

    LFOTarget LFOParam::Target() const noexcept { return _target; }
    void LFOParam::Target(LFOTarget value) noexcept { _target = value; }
    std::uint8_t LFOParam::Speed() const noexcept { return _speed; }
    void LFOParam::Speed(std::uint8_t value) noexcept { _speed = value; }
    std::uint8_t LFOParam::Depth() const noexcept { return _depth; }
    void LFOParam::Depth(std::uint8_t value) noexcept { _depth = value; }
    std::uint8_t LFOParam::Range() const noexcept { return _range; }
    void LFOParam::Range(std::uint8_t value) noexcept { _range = value; }
    std::uint16_t LFOParam::Delay() const noexcept { return _delay; }
    void LFOParam::Delay(std::uint16_t value) noexcept { _delay = value; }

    void LFOParam::CopyTo(LFOParam* other) const
    {
        if (other == nullptr)
            ThrowNullReference();
        other->Target(_target);
        other->Speed(_speed);
        other->Depth(_depth);
        other->Range(_range);
        other->Delay(_delay);
    }

    LFO::LFO()
        : _param(std::make_shared<LFOParam>())
    {
    }

    const std::shared_ptr<LFOParam>& LFO::Param() const noexcept { return _param; }
    void LFO::Param(std::shared_ptr<LFOParam> value) noexcept { _param = std::move(value); }
    std::uint16_t LFO::DelayCounter() const noexcept { return _delayCounter; }
    void LFO::DelayCounter(std::uint16_t value) noexcept { _delayCounter = value; }
    std::uint16_t LFO::Counter() const noexcept { return _counter; }
    void LFO::Counter(std::uint16_t value) noexcept { _counter = value; }

    void LFO::Start() noexcept
    {
        _counter = _delayCounter = 0;
    }

    void LFO::Update()
    {
        if (!_param)
            ThrowNullReference();

        if (_delayCounter < _param->Delay())
            ++_delayCounter;
        else
        {
            std::uint32_t tmp = _counter;
            tmp += static_cast<std::uint32_t>(static_cast<std::int32_t>(_param->Speed()) << 6);
            tmp >>= 8;
            while (tmp >= 0x80U)
                tmp -= 0x80U;
            _counter = static_cast<std::uint16_t>(
                _counter + static_cast<std::uint16_t>(
                    static_cast<std::int32_t>(_param->Speed()) << 6));
            _counter = static_cast<std::uint16_t>(_counter & 0x00FFU);
            _counter = static_cast<std::uint16_t>(
                _counter | static_cast<std::uint16_t>(tmp << 8));
        }
    }

    std::int8_t LFO::SinIndex(std::int32_t x)
    {
        if (x < 0x20)
            return SinTable.at(static_cast<std::size_t>(x));
        if (x < 0x40)
            return SinTable.at(static_cast<std::size_t>(0x40 - x));
        if (x < 0x60)
            return static_cast<std::int8_t>(-SinTable.at(static_cast<std::size_t>(x - 0x40)));
        return static_cast<std::int8_t>(
            -SinTable.at(static_cast<std::size_t>(0x20 - (x - 0x60))));
    }

    std::int32_t LFO::GetValue() const
    {
        if (!_param)
            ThrowNullReference();
        if (_param->Depth() == 0 || _delayCounter < _param->Delay())
            return 0;

        return static_cast<std::int32_t>(
            SinIndex(static_cast<std::int32_t>(
                static_cast<std::uint32_t>(_counter) >> 8))) *
            static_cast<std::int32_t>(_param->Depth()) *
            static_cast<std::int32_t>(_param->Range());
    }

    std::uint8_t Channel::Id() const noexcept { return _id; }
    void Channel::Id(std::uint8_t value) noexcept { _id = value; }
    ChannelState Channel::EnvelopeStatus() const noexcept { return _envelopeStatus; }
    void Channel::EnvelopeStatus(ChannelState value) noexcept { _envelopeStatus = value; }
    ChannelFlag Channel::Flags() const noexcept { return _flags; }
    void Channel::Flags(ChannelFlag value) noexcept { _flags = value; }
    ChannelSyncFlag Channel::SyncFlags() const noexcept { return _syncFlags; }
    void Channel::SyncFlags(ChannelSyncFlag value) noexcept { _syncFlags = value; }
    std::uint8_t Channel::PanRange() const noexcept { return _panRange; }
    void Channel::PanRange(std::uint8_t value) noexcept { _panRange = value; }
    std::uint8_t Channel::MidiKey() const noexcept { return _midiKey; }
    void Channel::MidiKey(std::uint8_t value) noexcept { _midiKey = value; }
    std::uint8_t Channel::Velocity() const noexcept { return _velocity; }
    void Channel::Velocity(std::uint8_t value) noexcept { _velocity = value; }
    std::int8_t Channel::UserPan() const noexcept { return _userPan; }
    void Channel::UserPan(std::int8_t value) noexcept { _userPan = value; }
    std::int16_t Channel::UserDecay() const noexcept { return _userDecay; }
    void Channel::UserDecay(std::int16_t value) noexcept { _userDecay = value; }
    std::int16_t Channel::UserPitch() const noexcept { return _userPitch; }
    void Channel::UserPitch(std::int16_t value) noexcept { _userPitch = value; }
    std::int32_t Channel::SweepCounter() const noexcept { return _sweepCounter; }
    void Channel::SweepCounter(std::int32_t value) noexcept { _sweepCounter = value; }
    std::int32_t Channel::SweepLength() const noexcept { return _sweepLength; }
    void Channel::SweepLength(std::int32_t value) noexcept { _sweepLength = value; }
    std::uint8_t Channel::Priority() const noexcept { return _priority; }
    void Channel::Priority(std::uint8_t value) noexcept { _priority = value; }
    NCSFCommon::LFO& Channel::LFO() noexcept { return _lfo; }
    const NCSFCommon::LFO& Channel::LFO() const noexcept { return _lfo; }
    std::int16_t Channel::SweepPitch() const noexcept { return _sweepPitch; }
    void Channel::SweepPitch(std::int16_t value) noexcept { _sweepPitch = value; }
    std::int32_t Channel::Length() const noexcept { return _length; }
    void Channel::Length(std::int32_t value) noexcept { _length = value; }
    const std::function<void(Channel*, bool)>& Channel::Callback() const noexcept { return _callback; }
    void Channel::Callback(std::function<void(Channel*, bool)> value) noexcept { _callback = std::move(value); }
    NCSFCommon::Player* Channel::Player() const noexcept { return _player; }
    void Channel::Player(NCSFCommon::Player* value) noexcept { _player = value; }
    NDSSoundRegister& Channel::Register() noexcept { return _register; }
    const NDSSoundRegister& Channel::Register() const noexcept { return _register; }

    void Channel::Init(std::int32_t id)
    {
        _id = static_cast<std::uint8_t>(id);
        _syncFlags = static_cast<ChannelSyncFlag>(0);
        _register.ClearControlRegister();
        _flags |= ChannelFlag::Active;
    }

    void Channel::Update()
    {
        if (static_cast<std::uint8_t>(_syncFlags) != 0)
        {
            if ((_syncFlags & ChannelSyncFlag::Stop) == ChannelSyncFlag::Stop)
                _register.Enable(false);

            if ((_syncFlags & ChannelSyncFlag::Start) == ChannelSyncFlag::Start)
            {
                _register.ClearControlRegister();
                _register.Panning(_pan);
                _register.VolumeMultiplier(static_cast<std::uint8_t>(_volume & 0x00FFU));
                _register.VolumeDivisor(static_cast<std::uint8_t>(_volume >> 8));

                switch (_type)
                {
                case ChannelType::PCM:
                    if (!_waveData)
                        ThrowNullReference();
                    _register.Format(static_cast<std::uint8_t>(_waveData->WaveType() & 3U));
                    _register.RepeatMode(static_cast<std::uint8_t>(_waveData->Loop() != 0 ? 1 : 2));
                    _register.LoopStart(_waveData->LoopOffset());
                    _register.Length(_waveData->LoopLength());
                    _register.TotalLength(_register.LoopStart() + _register.Length());
                    _register.Source(_waveData);
                    break;
                case ChannelType::PSG:
                    _register.Format(3);
                    _register.WaveDuty(static_cast<std::uint8_t>(_dutyCycle));
                    break;
                case ChannelType::Noise:
                    _register.Format(3);
                    break;
                }

                _register.Timer(static_cast<std::uint16_t>(0x10000U - _timer));

                _register.Enable(true);
                _syncFlags = static_cast<ChannelSyncFlag>(0);
            }
            else
            {
                if ((_syncFlags & ChannelSyncFlag::Timer) == ChannelSyncFlag::Timer)
                    _register.Timer(static_cast<std::uint16_t>(0x10000U - _timer));
                if ((_syncFlags & ChannelSyncFlag::Volume) == ChannelSyncFlag::Volume)
                {
                    _register.VolumeMultiplier(static_cast<std::uint8_t>(_volume & 0x00FFU));
                    _register.VolumeDivisor(static_cast<std::uint8_t>(_volume >> 8));
                }
                if ((_syncFlags & ChannelSyncFlag::Pan) == ChannelSyncFlag::Pan)
                    _register.Panning(_pan);
            }
        }
    }

    std::uint16_t Channel::CalculateChannelVolume(std::int32_t value)
    {
        value = std::clamp(value, SoundVolumeDBMin, 0);

        std::int32_t divisor;
        if (value < -240)
            divisor = 3;
        else if (value < -120)
            divisor = 2;
        else if (value < -60)
            divisor = 1;
        else
            divisor = 0;

        return static_cast<std::uint16_t>(
            GetVolumeTable.at(static_cast<std::size_t>(value - SoundVolumeDBMin)) |
            static_cast<std::uint16_t>(divisor << 8));
    }

    std::uint16_t Channel::CalculateTimer(std::int32_t timer, std::int32_t pitch)
    {
        std::int32_t octave = 0;
        std::int32_t pitchNormalized = WrapNegate32(pitch);

        while (pitchNormalized < 0)
        {
            --octave;
            pitchNormalized += 768;
        }

        while (pitchNormalized >= 768)
        {
            ++octave;
            pitchNormalized -= 768;
        }

        std::uint64_t result = GetPitchTable.at(static_cast<std::size_t>(pitchNormalized));

        result += 0x10000ULL;
        result *= static_cast<std::uint64_t>(static_cast<std::int64_t>(timer));

        const std::int32_t shift = octave - 16;

        if (shift <= 0)
        {
            const std::uint32_t shiftCount =
                static_cast<std::uint32_t>(WrapNegate32(shift)) & 0x3FU;
            result >>= shiftCount;
        }
        else if (shift < 32)
        {
            if ((result & (~0ULL << (32 - shift))) != 0)
                return 0xFFFFU;
            result <<= shift;
        }
        else
            return 0xFFFFU;

        result = std::clamp<std::uint64_t>(result, 0x10ULL, 0xFFFFULL);
        return static_cast<std::uint16_t>(result);
    }

    std::int16_t Channel::ConvertSustain(std::int32_t sustain)
    {
        if ((sustain & 0x80) != 0)
            sustain = 0x7F;
        return convertSustainLookupTable.at(static_cast<std::size_t>(sustain));
    }

    void Channel::Main()
    {
        if (IsActive())
        {
            if ((_flags & ChannelFlag::Start) == ChannelFlag::Start)
            {
                _syncFlags |= ChannelSyncFlag::Start;
                _flags &= ~ChannelFlag::Start;
                _register.Enable(false);
            }
            else if (!_register.Enable())
            {
                Kill();
                return;
            }

            std::int32_t vol = ConvertSustain(_velocity);
            std::int32_t pitch =
                (static_cast<std::int32_t>(_midiKey) - static_cast<std::int32_t>(_rootMidiKey)) * 0x40;

            vol = WrapAdd32(vol, UpdateEnvelope());
            pitch = WrapAdd32(pitch, UpdateSweep());

            vol = WrapAdd32(vol, _userDecay);
            pitch = WrapAdd32(pitch, _userPitch);

            const std::int32_t lfo = UpdateLFO();

            std::int32_t pan = 0;

            if (!_lfo.Param())
                ThrowNullReference();

            switch (_lfo.Param()->Target())
            {
            case LFOTarget::Volume:
                if (vol > -0x8000)
                    vol = WrapAdd32(vol, lfo);
                break;
            case LFOTarget::Pan:
                pan = WrapAdd32(pan, lfo);
                break;
            case LFOTarget::Pitch:
                pitch = WrapAdd32(pitch, lfo);
                break;
            }

            pan = WrapAdd32(pan, _initialPan);
            if (_panRange != 127)
                pan = ArithmeticShiftRight32(WrapAdd32(WrapMul32(pan, _panRange), 0x40), 7);
            pan = WrapAdd32(pan, _userPan);

            if (_envelopeStatus == ChannelState::Release && vol <= -723)
            {
                _syncFlags = ChannelSyncFlag::Stop;
                Kill();
            }
            else
            {
                vol = CalculateChannelVolume(vol);
                std::uint16_t newTimer = CalculateTimer(_waveTimer, pitch);

                if (_type == ChannelType::PSG)
                    newTimer = static_cast<std::uint16_t>(newTimer & 0xFFFCU);

                pan = WrapAdd32(pan, 0x40);
                pan = std::clamp(pan, 0, 127);

                if (vol != _volume)
                {
                    _volume = static_cast<std::uint16_t>(vol);
                    _syncFlags |= ChannelSyncFlag::Volume;
                }
                if (newTimer != _timer)
                {
                    _timer = newTimer;
                    if (_player == nullptr)
                        ThrowNullReference();
                    _register.SampleIncrease(
                        static_cast<double>(Player::ARM7Clock) /
                        (static_cast<double>(_player->SampleRate()) * 2.0) /
                        static_cast<double>(newTimer));
                    _syncFlags |= ChannelSyncFlag::Timer;
                }
                if (pan != _pan)
                {
                    _pan = static_cast<std::uint8_t>(pan);
                    _syncFlags |= ChannelSyncFlag::Pan;
                }
            }
        }
    }

    bool Channel::StartPCM(std::shared_ptr<NC::SWAV> wave, std::int32_t length)
    {
        _type = ChannelType::PCM;
        if (!wave)
            ThrowNullReference();
        _register.SamplePosition(wave->WaveType() == 2 ? -11.0 : -3.0);
        _waveData = std::move(wave);
        _waveTimer = _waveData->Time();
        Start(length);
        return true;
    }

    bool Channel::StartPSG(std::int32_t duty, std::int32_t length)
    {
        if (_id < 8 || _id > 13)
            return false;

        _type = ChannelType::PSG;
        _register.SamplePosition(-1.0);
        _dutyCycle = duty;
        _waveTimer = 8006;
        Start(length);
        return true;
    }

    bool Channel::StartNoise(std::int32_t length)
    {
        if (_id < 14 || _id > 15)
            return false;

        _type = ChannelType::Noise;
        _register.SamplePosition(-1.0);
        _register.PSGX(0x7FFF);
        _waveTimer = 8006;
        Start(length);
        return true;
    }

    std::int32_t Channel::UpdateEnvelope()
    {
        switch (_envelopeStatus)
        {
        case ChannelState::Attack:
        {
            const std::int32_t negated = WrapNegate32(_envelopeAttenuation);
            const std::int32_t product = WrapMul32(negated, _envelopeAttack);
            _envelopeAttenuation = WrapNegate32(ArithmeticShiftRight32(product, 8));
            if (_envelopeAttenuation == 0)
                _envelopeStatus = ChannelState::Decay;
            break;
        }
        case ChannelState::Decay:
        {
            const std::int32_t sustain = static_cast<std::int32_t>(ConvertSustain(_envelopeSustain)) * 128;
            _envelopeAttenuation = WrapSub32(_envelopeAttenuation, _envelopeDecay);
            if (_envelopeAttenuation <= sustain)
            {
                _envelopeAttenuation = sustain;
                _envelopeStatus = ChannelState::Sustain;
            }
            break;
        }
        case ChannelState::Release:
            _envelopeAttenuation = WrapSub32(_envelopeAttenuation, _envelopeRelease);
            break;
        case ChannelState::Sustain:
            break;
        }

        return ArithmeticShiftRight32(_envelopeAttenuation, 7);
    }

    void Channel::SetAttack(std::int32_t attack)
    {
        if (attack < 109)
            _envelopeAttack = static_cast<std::uint8_t>(WrapSub32(255, attack));
        else
            _envelopeAttack = AttackCoefficientTable.at(
                static_cast<std::size_t>(127 - attack));
    }

    std::uint16_t Channel::CalculateDecayCoefficient(std::int32_t vol)
    {
        if ((vol & 0x80) != 0)
            vol = 0;

        if (vol == 127)
            return 0xFFFFU;
        if (vol == 126)
            return 0x3C00U;
        if (vol < 50)
            return static_cast<std::uint16_t>(WrapAdd32(WrapMul32(vol, 2), 1));
        return static_cast<std::uint16_t>(0x1E00 / (126 - vol));
    }

    void Channel::SetDecay(std::int32_t decay)
    {
        _envelopeDecay = CalculateDecayCoefficient(decay);
    }

    void Channel::SetSustain(std::int32_t sustain) noexcept
    {
        _envelopeSustain = static_cast<std::uint8_t>(sustain);
    }

    void Channel::SetRelease(std::int32_t release)
    {
        _envelopeRelease = CalculateDecayCoefficient(release);
    }

    void Channel::Release() noexcept
    {
        _envelopeStatus = ChannelState::Release;
    }

    bool Channel::IsActive() const noexcept
    {
        return (_flags & ChannelFlag::Active) == ChannelFlag::Active;
    }

    void Channel::Free() noexcept
    {
        _callback = {};
    }

    void Channel::Setup(std::function<void(Channel*, bool)> callback, std::int32_t priority)
    {
        _callback = std::move(callback);
        _length = _sweepLength = _sweepCounter = 0;
        _priority = static_cast<std::uint8_t>(priority);
        _volume = 127;
        _flags &= ~ChannelFlag::Start;
        _flags |= ChannelFlag::AutoSweep;
        _midiKey = _rootMidiKey = 60;
        _velocity = _panRange = 127;
        _initialPan = _userPan = 0;
        _userDecay = _userPitch = _sweepPitch = 0;

        SetAttack(127);
        SetSustain(127);
        SetDecay(127);
        SetRelease(127);
        _lfo.Param(std::make_shared<LFOParam>());
    }

    void Channel::Start(std::int32_t length)
    {
        _envelopeAttenuation = -92544;
        _envelopeStatus = ChannelState::Attack;
        _length = length;
        _lfo.Start();
        _flags |= ChannelFlag::Start | ChannelFlag::Active;
    }

    std::int32_t Channel::VolumeCompare(const Channel* other) const
    {
        std::int32_t volA = _volume & 0x00FF;
        if (other == nullptr)
            ThrowNullReference();
        std::int32_t volB = other->_volume & 0x00FF;

        volA <<= 4;
        volB <<= 4;

        volA >>= SampleDataShiftTable.at(static_cast<std::size_t>(_volume >> 8));
        volB >>= SampleDataShiftTable.at(static_cast<std::size_t>(other->_volume >> 8));

        return volA != volB ? (volA < volB ? 1 : -1) : 0;
    }

    std::int32_t Channel::UpdateSweep()
    {
        std::int64_t result = 0;

        if (_sweepPitch != 0 && _sweepCounter < _sweepLength)
        {
            if (_sweepLength == 0)
                throw std::runtime_error("DivideByZeroException");

            const std::int32_t sweepRemaining = WrapSub32(_sweepLength, _sweepCounter);
            result = static_cast<std::int64_t>(_sweepPitch) *
                static_cast<std::int64_t>(sweepRemaining) /
                _sweepLength;

            if ((_flags & ChannelFlag::AutoSweep) == ChannelFlag::AutoSweep)
                ++_sweepCounter;
        }

        return WrapInt32(result);
    }

    std::int32_t Channel::UpdateLFO()
    {
        std::int64_t result = _lfo.GetValue();

        if (result != 0)
        {
            if (!_lfo.Param())
                ThrowNullReference();

            switch (_lfo.Param()->Target())
            {
            case LFOTarget::Volume:
                result *= 60;
                break;
            case LFOTarget::Pitch:
            case LFOTarget::Pan:
                result = WrapShiftLeft64(result, 6);
                break;
            }
            result = ArithmeticShiftRight64(result, 14);
        }

        _lfo.Update();

        return static_cast<std::int32_t>(result);
    }

    bool Channel::NoteOn(
        std::int32_t midiKey,
        std::int32_t velocity,
        std::int32_t length,
        const NC::SBNKInstrument* instData)
    {
        if (instData == nullptr)
            ThrowNullReference();

        std::uint8_t release = instData->ReleaseRate();

        if (release == 0xFF)
        {
            length = -1;
            release = 0;
        }

        bool success;
        switch (instData->Record())
        {
        case 1:
        {
            if (_player == nullptr)
                ThrowNullReference();
            const auto swars = _player->SWARs();
            const std::size_t swarIndex = instData->SWAR();
            if (swarIndex >= swars.size())
                throw std::out_of_range("IndexOutOfRangeException");
            const auto& swar = swars[swarIndex];
            if (!swar)
                ThrowNullReference();
            success = StartPCM(swar->SWAVs().At(instData->SWAV()), length);
            break;
        }
        case 2:
            success = StartPSG(instData->SWAV(), length);
            break;
        case 3:
            success = StartNoise(length);
            break;
        default:
            success = false;
            break;
        }

        if (success)
        {
            _midiKey = static_cast<std::uint8_t>(midiKey);
            _rootMidiKey = instData->NoteNumber();
            _velocity = static_cast<std::uint8_t>(velocity);
            SetAttack(instData->AttackRate());
            SetSustain(instData->SustainLevel());
            SetDecay(instData->DecayRate());
            SetRelease(release);
            _initialPan = WrapSByte(
                static_cast<std::int32_t>(instData->Pan()) - 0x40);
            return true;
        }

        return false;
    }

    void Channel::Kill()
    {
        if (!_callback)
            _priority = 0;
        else
            _callback(this, true);
        _volume = 0;
        _flags &= ~ChannelFlag::Active;
    }
}
