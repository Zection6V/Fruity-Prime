#include "TestLogic.hpp"

#include "../MemoryArrays.hpp"
#include "../MemoryClasses.hpp"

#include <bit>
#include <cassert>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace
{
    class TestLogicDivideByZeroException final : public std::runtime_error
    {
    public:
        TestLogicDivideByZeroException()
            : std::runtime_error("Attempted to divide by zero.")
        {
        }
    };

    class TestLogicOverflowException final : public std::overflow_error
    {
    public:
        TestLogicOverflowException()
            : std::overflow_error("Arithmetic operation resulted in an overflow.")
        {
        }
    };

    [[nodiscard]] std::int32_t UncheckedAdd(
        std::int32_t left, std::int32_t right) noexcept
    {
        const std::uint32_t result = std::bit_cast<std::uint32_t>(left)
            + std::bit_cast<std::uint32_t>(right);
        return std::bit_cast<std::int32_t>(result);
    }

    [[nodiscard]] std::int32_t UncheckedSubtract(
        std::int32_t left, std::int32_t right) noexcept
    {
        const std::uint32_t result = std::bit_cast<std::uint32_t>(left)
            - std::bit_cast<std::uint32_t>(right);
        return std::bit_cast<std::int32_t>(result);
    }

    [[nodiscard]] std::int32_t UncheckedNegate(std::int32_t value) noexcept
    {
        const std::uint32_t result = 0U - std::bit_cast<std::uint32_t>(value);
        return std::bit_cast<std::int32_t>(result);
    }

    [[nodiscard]] std::int32_t UncheckedMultiply(
        std::int32_t left, std::int32_t right) noexcept
    {
        const std::uint32_t result = std::bit_cast<std::uint32_t>(left)
            * std::bit_cast<std::uint32_t>(right);
        return std::bit_cast<std::int32_t>(result);
    }

    [[nodiscard]] std::int32_t UncheckedFromUInt32(std::uint32_t value) noexcept
    {
        return std::bit_cast<std::int32_t>(value);
    }

    [[nodiscard]] std::int32_t ManagedDivide(
        std::int32_t left, std::int32_t right)
    {
        if (right == 0)
        {
            throw TestLogicDivideByZeroException();
        }
        if (left == std::numeric_limits<std::int32_t>::min() && right == -1)
        {
            throw TestLogicOverflowException();
        }
        return left / right;
    }

    template <typename T>
    [[nodiscard]] T& Require(const std::shared_ptr<T>& value)
    {
        if (!value)
        {
            throw System::NullReferenceException();
        }
        return *value;
    }

    [[nodiscard]] OpenTK::Mathematics::Matrix4x3 CreateScale(float scale) noexcept
    {
        return OpenTK::Mathematics::Matrix4x3(
            OpenTK::Mathematics::Vector3(scale, 0.0F, 0.0F),
            OpenTK::Mathematics::Vector3(0.0F, scale, 0.0F),
            OpenTK::Mathematics::Vector3(0.0F, 0.0F, scale),
            OpenTK::Mathematics::Vector3());
    }

    [[nodiscard]] OpenTK::Mathematics::Matrix4x3 CreateTranslation(
        OpenTK::Mathematics::Vector3 position) noexcept
    {
        return OpenTK::Mathematics::Matrix4x3(
            OpenTK::Mathematics::Vector3(1.0F, 0.0F, 0.0F),
            OpenTK::Mathematics::Vector3(0.0F, 1.0F, 0.0F),
            OpenTK::Mathematics::Vector3(0.0F, 0.0F, 1.0F),
            position);
    }

    [[nodiscard]] bool HunterGreaterThan(MphRead::Hunter left, MphRead::Hunter right) noexcept
    {
        return static_cast<std::uint8_t>(left) > static_cast<std::uint8_t>(right);
    }
}

namespace MphRead::Testing
{
    const std::shared_ptr<TestLogic::MModel> TestLogic::_mdl200D960{};
    const std::shared_ptr<TestLogic::MModel> TestLogic::_mdl200D938{};
    const std::shared_ptr<TestLogic::MModel> TestLogic::_mdl200E490{};
    const OpenTK::Mathematics::Matrix4x3 TestLogic::_mtx20D955C{};
    const OpenTK::Mathematics::Matrix4x3 TestLogic::_viewMatrix{};
    OpenTK::Mathematics::Matrix4x3 TestLogic::_currentTextureMatrix{};
    const std::int32_t TestLogic::_mem20E97B0 = 0;
    const std::int32_t TestLogic::_mem20DA5D0 = 0;
    std::int32_t TestLogic::_mem20E3EA0 = 0;
    const std::int32_t TestLogic::_gameState = 2;

    void TestLogic::TestSaveBlocks()
    {
        std::vector<std::shared_ptr<SaveBlock>> blocks;
        for (std::int32_t i = 0; i < 3; i = UncheckedAdd(i, 1))
        {
            auto block = std::make_shared<SaveBlock>();
            block->Field0(0);
            block->Field4(128);
            block->Field8(4276);
            block->FieldC(-23);
            blocks.push_back(std::move(block));
        }
        {
            auto block = std::make_shared<SaveBlock>();
            block->Field0(0);
            block->Field4(128);
            block->Field8(64);
            block->FieldC(2);
            blocks.push_back(std::move(block));
        }
        {
            auto block = std::make_shared<SaveBlock>();
            block->Field0(0);
            block->Field4(128);
            block->Field8(164);
            block->FieldC(-1);
            blocks.push_back(std::move(block));
        }
        {
            auto block = std::make_shared<SaveBlock>();
            block->Field0(0);
            block->Field4(128);
            block->Field8(488);
            block->FieldC(9);
            blocks.push_back(std::move(block));
        }
        {
            auto block = std::make_shared<SaveBlock>();
            block->Field0(0);
            block->Field4(128);
            block->Field8(8400);
            block->FieldC(0);
            blocks.push_back(std::move(block));
        }

        const std::int32_t count = static_cast<std::int32_t>(blocks.size());
        const std::int32_t dword20EEFF4 = UncheckedAdd(
            UncheckedMultiply(4, UncheckedSubtract(count, 1)), 36);
        const std::int32_t dword20EEFC0 = 0x100;
        const std::int32_t dword20EEFBC = 0x40000;

        std::vector<std::shared_ptr<SaveBufBlock>> bufBlocks;
        for (std::int32_t i = 0; i < count; i = UncheckedAdd(i, 1))
        {
            bufBlocks.push_back(std::make_shared<SaveBufBlock>());
        }

        std::int32_t size = 0;
        for (std::int32_t i = 0; i < count; i = UncheckedAdd(i, 1))
        {
            const std::shared_ptr<SaveBlock> block = blocks[static_cast<std::size_t>(i)];
            if (block->FieldC() != 0)
            {
                const std::int32_t v11 = UncheckedMultiply(
                    ManagedDivide(
                        UncheckedSubtract(
                            UncheckedAdd(
                                UncheckedAdd(block->Field8(), 8),
                                dword20EEFC0),
                            1),
                        dword20EEFC0),
                    dword20EEFC0);
                if (block->Field0() == 0 && block->FieldC() < 0)
                {
                    block->FieldC(ManagedDivide(
                        ManagedDivide(
                            UncheckedMultiply(
                                dword20EEFBC,
                                UncheckedNegate(block->FieldC())),
                            100),
                        v11));
                }
                const std::shared_ptr<SaveBufBlock> newBuf =
                    bufBlocks[static_cast<std::size_t>(i)];
                newBuf->Field0(block->Field0());
                newBuf->Field4(block->Field8());
                newBuf->Field8(v11);
                newBuf->FieldC(0);
                newBuf->Field14(block->FieldC());
                newBuf->Field10(UncheckedMultiply(block->FieldC(), v11));
                size = UncheckedAdd(size, newBuf->Field10());
                if (size > dword20EEFBC)
                {
                    assert(false);
                }
            }
        }

        std::int32_t v19 = ManagedDivide(
            UncheckedMultiply(
                dword20EEFC0,
                UncheckedSubtract(
                    UncheckedAdd(dword20EEFF4, dword20EEFC0),
                    1)),
            dword20EEFC0);
        size = UncheckedAdd(size, v19);

        for (std::int32_t i = 0; i < count; i = UncheckedAdd(i, 1))
        {
            const std::shared_ptr<SaveBlock> block = blocks[static_cast<std::size_t>(i)];
            if (block->FieldC() == 0)
            {
                const std::int32_t v11 = UncheckedMultiply(
                    ManagedDivide(
                        UncheckedSubtract(
                            UncheckedAdd(
                                UncheckedAdd(block->Field8(), 8),
                                dword20EEFC0),
                            1),
                        dword20EEFC0),
                    dword20EEFC0);
                const std::shared_ptr<SaveBufBlock> newBuf =
                    bufBlocks[static_cast<std::size_t>(i)];
                newBuf->Field0(block->Field0());
                newBuf->Field4(block->Field8());
                newBuf->Field8(v11);
                newBuf->FieldC(0);
                newBuf->Field14(ManagedDivide(
                    UncheckedSubtract(dword20EEFBC, size),
                    v11));
                newBuf->Field10(UncheckedMultiply(newBuf->Field14(), v11));
                size = UncheckedAdd(size, newBuf->Field10());
                if (size > dword20EEFBC)
                {
                    assert(false);
                }
            }
        }

        for (const std::shared_ptr<SaveBufBlock>& bufBlock : bufBlocks)
        {
            bufBlock->FieldC(v19);
            v19 = UncheckedAdd(v19, bufBlock->Field10());
        }
    }

    std::shared_ptr<TestLogic::CompletionValues> TestLogic::GetCompletionValues(
        std::shared_ptr<Memory::StorySaveData> save)
    {
        std::int32_t octolithCount = 0;
        for (std::int32_t i = 0; i < 8; i = UncheckedAdd(i, 1))
        {
            const std::uint8_t foundOctos = Require(save).FoundOctos();
            if ((static_cast<std::int32_t>(foundOctos) & (1 << i)) != 0)
            {
                octolithCount = UncheckedAdd(octolithCount, 1);
            }
        }

        auto values = std::make_shared<CompletionValues>();
        values->Completion(GetCompletionPercentage(save));
        values->Octolith(ManagedDivide(UncheckedMultiply(100, octolithCount), 8));
        values->EnergyTanks(ManagedDivide(
            static_cast<std::int32_t>(Require(save).EnergyCap()), 100));

        std::shared_ptr<Memory::UInt16Array> ammoCaps = Require(save).AmmoCaps();
        const std::int32_t uaCap = static_cast<std::int32_t>(Require(ammoCaps).Item(0));
        values->UaExpansions(ManagedDivide(UncheckedSubtract(uaCap, 400), 300));

        ammoCaps = Require(save).AmmoCaps();
        const std::int32_t missileCap = static_cast<std::int32_t>(Require(ammoCaps).Item(1));
        values->MissileExpansions(ManagedDivide(UncheckedSubtract(missileCap, 50), 100));
        return values;
    }

    std::int32_t TestLogic::GetCompletionPercentage(
        std::shared_ptr<Memory::StorySaveData> save)
    {
        if (Require(save).MaxScanCount() == 0)
        {
            return 0;
        }

        std::int32_t counts = 0;
        counts = UncheckedAdd(counts, Require(save).ScanCount());

        for (std::int32_t i = 1; i < 8; i = UncheckedAdd(i, 1))
        {
            const std::uint16_t weapons = Require(save).Weapons();
            if (i != 2 && (static_cast<std::int32_t>(weapons) & (1 << i)) != 0)
            {
                counts = UncheckedAdd(counts, 1);
            }
        }

        for (std::int32_t i = 0; i < 8; i = UncheckedAdd(i, 1))
        {
            const std::uint8_t foundOctos = Require(save).FoundOctos();
            if ((static_cast<std::int32_t>(foundOctos) & (1 << i)) != 0)
            {
                counts = UncheckedAdd(counts, 1);
            }
        }

        for (std::int32_t i = 0; i < 24; i = UncheckedAdd(i, 1))
        {
            const std::uint32_t artifacts = Require(save).Artifacts();
            if ((artifacts & (std::uint32_t{1} << i)) != 0)
            {
                counts = UncheckedAdd(counts, 1);
            }
        }

        counts = UncheckedAdd(
            counts,
            ManagedDivide(static_cast<std::int32_t>(Require(save).EnergyCap()), 100));

        std::shared_ptr<Memory::UInt16Array> ammoCaps = Require(save).AmmoCaps();
        const std::int32_t uaCap = static_cast<std::int32_t>(Require(ammoCaps).Item(0));
        counts = UncheckedAdd(
            counts,
            ManagedDivide(UncheckedSubtract(uaCap, 400), 300));

        ammoCaps = Require(save).AmmoCaps();
        const std::int32_t missileCap = static_cast<std::int32_t>(Require(ammoCaps).Item(1));
        counts = UncheckedAdd(
            counts,
            ManagedDivide(UncheckedSubtract(missileCap, 50), 100));

        const std::int32_t denominator = UncheckedAdd(
            UncheckedFromUInt32(Require(save).MaxScanCount()), 66);
        return ManagedDivide(UncheckedMultiply(100, counts), denominator);
    }

    void TestLogic::TestLogic1()
    {
        MphRead::Hunter hunter = MphRead::Hunter::Samus;
        std::int32_t flags = 0;
        std::int32_t v45 = 0;

        if (hunter == MphRead::Hunter::Noxus)
        {
        }
        else if (HunterGreaterThan(hunter, MphRead::Hunter::Samus)
            && hunter != MphRead::Hunter::Spire)
        {
            if (hunter == MphRead::Hunter::Kanden)
            {
            }
            else
            {
            }
        }
        else
        {
            if (HunterGreaterThan(hunter, MphRead::Hunter::Samus)
                || (flags & 0x80) > 0)
            {
                v45 = 1;
            }
            else
            {
                v45 = 2;
            }
            if (v45 > 0)
            {
                if (hunter == MphRead::Hunter::Samus)
                {
                }
                if (hunter == MphRead::Hunter::Spire)
                {
                }
            }
        }
    }

    bool TestLogic::IsVisibleMaybe(std::shared_ptr<CPlayer> player) noexcept
    {
        return player != nullptr;
    }

    void TestLogic::Memory1FF8000()
    {
    }

    void TestLogic::CModelDraw(
        std::shared_ptr<CModel> model,
        OpenTK::Mathematics::Matrix4x3 someMatrix)
    {
        CModel& value = Require(model);
        std::shared_ptr<MModel> innerModel = value.Model();
        const std::int16_t someFlag = value.SomeFlag();
        DrawAnimatedModel(std::move(innerModel), someMatrix, static_cast<std::uint8_t>(someFlag));
    }

    void TestLogic::DrawAnimatedModel(
        std::shared_ptr<MModel> model,
        OpenTK::Mathematics::Matrix4x3 texMatrix,
        std::uint8_t flags)
    {
        MModel& value = Require(model);
        OpenTK::Mathematics::Matrix4x3 currentTextureMatrix;
        if ((value.Flags() & 1) > 0)
        {
            if (value.Scale() == 1.0F)
            {
                currentTextureMatrix = Matrix::Concat43(texMatrix, _viewMatrix);
            }
            else
            {
                const OpenTK::Mathematics::Matrix4x3 scaleMatrix = CreateScale(value.Scale());
                currentTextureMatrix = Matrix::Concat43(scaleMatrix, texMatrix);
                currentTextureMatrix = Matrix::Concat43(currentTextureMatrix, _viewMatrix);
            }
        }
        else
        {
            if (value.Scale() == 1.0F)
            {
                currentTextureMatrix = texMatrix;
            }
            else
            {
                const OpenTK::Mathematics::Matrix4x3 scaleMatrix = CreateScale(value.Scale());
                currentTextureMatrix = Matrix::Concat43(scaleMatrix, texMatrix);
            }
        }

        _currentTextureMatrix = currentTextureMatrix;
        _mem20E3EA0 = 0;
        if (value.NodeAnimation() != 0)
        {
            if ((flags & 1) > 0)
            {
                Memory1FF8000();
            }
            else
            {
                Memory1FF8000();
                _mem20E3EA0 = std::numeric_limits<std::int32_t>::min();
            }
        }

        if (_mem20E3EA0 >= 0)
        {
        }
        else
        {
        }
    }

    void TestLogic::CModelInitializeAnimationData(std::shared_ptr<CModel> model)
    {
        CModel& value = Require(model);
        std::shared_ptr<MModel> innerModel = value.Model();
        std::shared_ptr<CNodeAnimation> animation = value.NodeAnimation();
        const std::uintptr_t nodeAnimation = Require(animation).NodeAnimation();
        CNodeAnimationSetData(std::move(innerModel), nodeAnimation);
    }

    void TestLogic::CNodeAnimationSetData(
        std::shared_ptr<MModel> model,
        std::uintptr_t nodeAnimation)
    {
        Require(model).NodeAnimation(nodeAnimation);
    }

    void TestLogic::TestLogic2(std::shared_ptr<CPlayer> player, std::int32_t playerId)
    {
        CPlayer& value = Require(player);
        if (!TypeExtensions::TestFlag(value.MoreFlags(), MoreFlags::HideModel))
        {
            if (value.Hunter() == MphRead::Hunter::Spire
                && TypeExtensions::TestFlag(value.MoreFlags(), MoreFlags::AltFormAttack))
            {
                CModelInitializeAnimationData(value.Model());
            }

            if (playerId == 0 || IsVisibleMaybe(player))
            {
                const bool v10 = playerId != 0
                    || value.Field4D6() != 0
                    || value.Field550() < value.Field46C()
                    || _mem20DA5D0 != 0;

                if (TypeExtensions::TestFlag(value.SomeFlags(), SomeFlags::AltForm))
                {
                    if (value.Hunter() == MphRead::Hunter::Kanden)
                    {
                        {
                            const std::shared_ptr<CModel> playerModel = value.Model();
                            const std::shared_ptr<MModel> innerModel = Require(playerModel).Model();
                            CNodeAnimationSetData(innerModel, 0);
                        }
                        CModelDraw(value.Model(), _mtx20D955C);
                        {
                            const std::shared_ptr<CModel> playerModelForModel = value.Model();
                            const std::shared_ptr<MModel> innerModel = Require(playerModelForModel).Model();
                            const std::shared_ptr<CModel> playerModelForAnimation = value.Model();
                            const std::shared_ptr<CNodeAnimation> animation =
                                Require(playerModelForAnimation).NodeAnimation();
                            const std::uintptr_t nodeAnimation = Require(animation).NodeAnimation();
                            CNodeAnimationSetData(innerModel, nodeAnimation);
                        }
                    }
                    else if (value.Hunter() == MphRead::Hunter::Spire)
                    {
                        if (TypeExtensions::TestFlag(value.MoreFlags(), MoreFlags::AltFormAttack))
                        {
                            CModelInitializeAnimationData(value.Model());
                            {
                                const std::shared_ptr<CModel> playerModel = value.Model();
                                const std::shared_ptr<MModel> innerModel = Require(playerModel).Model();
                                CNodeAnimationSetData(innerModel, 0);
                            }
                            const OpenTK::Mathematics::Matrix4x3 matrix =
                                CreateTranslation(value.Position());
                            const std::shared_ptr<CModel> playerModel = value.Model();
                            const std::int16_t someFlag = Require(playerModel).SomeFlag();
                            DrawAnimatedModel(
                                _mdl200D960,
                                matrix,
                                static_cast<std::uint8_t>(someFlag));
                        }
                        else
                        {
                            CModelDraw(value.Model(), value.SomeMatrix());
                        }
                    }
                    else
                    {
                        CModelDraw(value.Model(), value.SomeMatrix());
                    }

                    if (value.Field4BB() != 0)
                    {
                        const std::int32_t v55 = 1;
                        const OpenTK::Mathematics::Matrix4x3 scaleMatrix =
                            CreateScale(static_cast<float>(v55));
                        const OpenTK::Mathematics::Matrix4x3 matrix =
                            Matrix::Concat43(scaleMatrix, value.SomeMatrix());
                        CModelDraw(value.Field1A4(), matrix);
                    }
                }
                else if (v10)
                {
                    if (value.Health() > 0)
                    {
                        const OpenTK::Mathematics::Matrix4x3 matrix{};
                        DrawAnimatedModel(_mdl200D938, matrix, 0);
                        if (value.Field4BB() != 0)
                        {
                            CModelDraw(value.Field1A4(), matrix);
                        }
                    }
                }
                else
                {
                    if (value.Field358() != 0 || value.Field6D0() != 0)
                    {
                    }
                    else
                    {
                        const OpenTK::Mathematics::Vector3 field64 = value.Field64();
                        const OpenTK::Mathematics::Vector3 fieldB4 = value.FieldB4();
                        const OpenTK::Mathematics::Matrix3 transform =
                            Matrix::GetTransform3(field64, fieldB4);
                        const OpenTK::Mathematics::Matrix4x3 matrix(
                            transform.Row0(),
                            transform.Row1(),
                            transform.Row2(),
                            OpenTK::Mathematics::Vector3());
                        CModelDraw(value.Gun(), matrix);
                        if (TypeExtensions::TestFlag(
                                value.SomeFlags(), SomeFlags::DrawGunSmoke))
                        {
                            CModelDraw(value.GunSmoke(), matrix);
                        }
                    }
                }
            }

            if (_gameState == 2
                && playerId == 0
                && _mem20E97B0 != 0
                && value.FieldE2() <= 0x77)
            {
                const OpenTK::Mathematics::Matrix4x3 matrix{};
                DrawAnimatedModel(_mdl200E490, matrix, 0);
            }
        }
    }
}
