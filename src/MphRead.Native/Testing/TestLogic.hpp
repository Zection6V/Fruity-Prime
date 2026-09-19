#pragma once

#include "../Formats/Types.hpp"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>

namespace MphRead::Memory
{
    class StorySaveData;
}

namespace MphRead::Testing
{
    class TestLogicDivideByZeroException final : public std::runtime_error
    {
    public:
        TestLogicDivideByZeroException();
    };

    class TestLogicOverflowException final : public std::overflow_error
    {
    public:
        TestLogicOverflowException();
    };

    class TestLogic final
    {
    private:
        class SaveBlock
        {
        public:
            SaveBlock() = default;
            SaveBlock(const SaveBlock&) = delete;
            SaveBlock& operator=(const SaveBlock&) = delete;
            SaveBlock(SaveBlock&&) = delete;
            SaveBlock& operator=(SaveBlock&&) = delete;

            [[nodiscard]] std::int32_t Field0() const noexcept { return _field0; }
            void Field0(std::int32_t value) noexcept { _field0 = value; }
            [[nodiscard]] std::int32_t Field4() const noexcept { return _field4; }
            void Field4(std::int32_t value) noexcept { _field4 = value; }
            [[nodiscard]] std::int32_t Field8() const noexcept { return _field8; }
            void Field8(std::int32_t value) noexcept { _field8 = value; }
            [[nodiscard]] std::int32_t FieldC() const noexcept { return _fieldC; }
            void FieldC(std::int32_t value) noexcept { _fieldC = value; }

        private:
            std::int32_t _field0 = 0;
            std::int32_t _field4 = 0;
            std::int32_t _field8 = 0;
            std::int32_t _fieldC = 0;
        };

        class SaveBufBlock
        {
        public:
            SaveBufBlock() = default;
            SaveBufBlock(const SaveBufBlock&) = delete;
            SaveBufBlock& operator=(const SaveBufBlock&) = delete;
            SaveBufBlock(SaveBufBlock&&) = delete;
            SaveBufBlock& operator=(SaveBufBlock&&) = delete;

            [[nodiscard]] std::int32_t Field0() const noexcept { return _field0; }
            void Field0(std::int32_t value) noexcept { _field0 = value; }
            [[nodiscard]] std::int32_t Field4() const noexcept { return _field4; }
            void Field4(std::int32_t value) noexcept { _field4 = value; }
            [[nodiscard]] std::int32_t Field8() const noexcept { return _field8; }
            void Field8(std::int32_t value) noexcept { _field8 = value; }
            [[nodiscard]] std::int32_t FieldC() const noexcept { return _fieldC; }
            void FieldC(std::int32_t value) noexcept { _fieldC = value; }
            [[nodiscard]] std::int32_t Field10() const noexcept { return _field10; }
            void Field10(std::int32_t value) noexcept { _field10 = value; }
            [[nodiscard]] std::int32_t Field14() const noexcept { return _field14; }
            void Field14(std::int32_t value) noexcept { _field14 = value; }
            [[nodiscard]] std::int32_t Field18() const noexcept { return _field18; }
            void Field18(std::int32_t value) noexcept { _field18 = value; }
            [[nodiscard]] std::int32_t Field1C() const noexcept { return _field1C; }
            void Field1C(std::int32_t value) noexcept { _field1C = value; }

        private:
            std::int32_t _field0 = 0;
            std::int32_t _field4 = 0;
            std::int32_t _field8 = 0;
            std::int32_t _fieldC = 0;
            std::int32_t _field10 = 0;
            std::int32_t _field14 = 0;
            std::int32_t _field18 = 0;
            std::int32_t _field1C = 0;
        };

    public:
        class CompletionValues
        {
        public:
            CompletionValues() = default;
            CompletionValues(const CompletionValues&) = delete;
            CompletionValues& operator=(const CompletionValues&) = delete;
            CompletionValues(CompletionValues&&) = delete;
            CompletionValues& operator=(CompletionValues&&) = delete;

            [[nodiscard]] std::int32_t Completion() const noexcept { return _completion; }
            void Completion(std::int32_t value) noexcept { _completion = value; }
            [[nodiscard]] std::int32_t Octolith() const noexcept { return _octolith; }
            void Octolith(std::int32_t value) noexcept { _octolith = value; }
            [[nodiscard]] std::int32_t EnergyTanks() const noexcept { return _energyTanks; }
            void EnergyTanks(std::int32_t value) noexcept { _energyTanks = value; }
            [[nodiscard]] std::int32_t UaExpansions() const noexcept { return _uaExpansions; }
            void UaExpansions(std::int32_t value) noexcept { _uaExpansions = value; }
            [[nodiscard]] std::int32_t MissileExpansions() const noexcept { return _missileExpansions; }
            void MissileExpansions(std::int32_t value) noexcept { _missileExpansions = value; }

        private:
            std::int32_t _completion = 0;
            std::int32_t _octolith = 0;
            std::int32_t _energyTanks = 0;
            std::int32_t _uaExpansions = 0;
            std::int32_t _missileExpansions = 0;
        };

        enum class SomeFlags : std::uint32_t
        {
            None = 0x0,
            SurfaceCollision = 0x10,
            PlatformCollision = 0x80,
            UsedJump = 0x100,
            AltForm = 0x200,
            DrawAltForm = 0x400,
            BlockAiming = 0x1000000,
            WeaponMenu = 0x2000000,
            DrawGunSmoke = 0x80000000U
        };

        enum class MoreFlags : std::uint32_t
        {
            None = 0x0,
            FullCharge = 0x1,
            HideModel = 0x2,
            WeaponFiring = 0x4,
            AltFormAttack = 0x8
        };

        class CModel;
        class CNodeAnimation;
        class MModel;

        class CPlayer
        {
        public:
            CPlayer() = default;
            CPlayer(const CPlayer&) = delete;
            CPlayer& operator=(const CPlayer&) = delete;
            CPlayer(CPlayer&&) = delete;
            CPlayer& operator=(CPlayer&&) = delete;

            [[nodiscard]] OpenTK::Mathematics::Vector3 Position() const noexcept { return _position; }
            void Position(OpenTK::Mathematics::Vector3 value) noexcept { _position = value; }
            [[nodiscard]] MphRead::Hunter Hunter() const noexcept { return _hunter; }
            void Hunter(MphRead::Hunter value) noexcept { _hunter = value; }
            [[nodiscard]] auto SomeFlags() const noexcept -> TestLogic::SomeFlags { return _someFlags; }
            void SomeFlags(TestLogic::SomeFlags value) noexcept { _someFlags = value; }
            [[nodiscard]] auto MoreFlags() const noexcept -> TestLogic::MoreFlags { return _moreFlags; }
            void MoreFlags(TestLogic::MoreFlags value) noexcept { _moreFlags = value; }
            [[nodiscard]] std::shared_ptr<CModel> Model() const noexcept { return _model; }
            void Model(std::shared_ptr<CModel> value) noexcept { _model = std::move(value); }
            [[nodiscard]] std::shared_ptr<CModel> Gun() const noexcept { return _gun; }
            void Gun(std::shared_ptr<CModel> value) noexcept { _gun = std::move(value); }
            [[nodiscard]] std::shared_ptr<CModel> GunSmoke() const noexcept { return _gunSmoke; }
            void GunSmoke(std::shared_ptr<CModel> value) noexcept { _gunSmoke = std::move(value); }
            [[nodiscard]] OpenTK::Mathematics::Matrix4x3 SomeMatrix() const noexcept { return _someMatrix; }
            void SomeMatrix(OpenTK::Mathematics::Matrix4x3 value) noexcept { _someMatrix = value; }
            [[nodiscard]] std::uint8_t Field4BB() const noexcept { return _field4BB; }
            void Field4BB(std::uint8_t value) noexcept { _field4BB = value; }
            [[nodiscard]] std::shared_ptr<CModel> Field1A4() const noexcept { return _field1A4; }
            void Field1A4(std::shared_ptr<CModel> value) noexcept { _field1A4 = std::move(value); }
            [[nodiscard]] std::uint32_t Health() const noexcept { return _health; }
            void Health(std::uint32_t value) noexcept { _health = value; }
            [[nodiscard]] std::int32_t Field358() const noexcept { return _field358; }
            void Field358(std::int32_t value) noexcept { _field358 = value; }
            [[nodiscard]] std::int32_t Field6D0() const noexcept { return _field6D0; }
            void Field6D0(std::int32_t value) noexcept { _field6D0 = value; }
            [[nodiscard]] OpenTK::Mathematics::Vector3 Field64() const noexcept { return _field64; }
            void Field64(OpenTK::Mathematics::Vector3 value) noexcept { _field64 = value; }
            [[nodiscard]] OpenTK::Mathematics::Vector3 FieldB4() const noexcept { return _fieldB4; }
            void FieldB4(OpenTK::Mathematics::Vector3 value) noexcept { _fieldB4 = value; }
            [[nodiscard]] std::int16_t FieldE2() const noexcept { return _fieldE2; }
            void FieldE2(std::int16_t value) noexcept { _fieldE2 = value; }
            [[nodiscard]] std::uint8_t Field4D6() const noexcept { return _field4D6; }
            void Field4D6(std::uint8_t value) noexcept { _field4D6 = value; }
            [[nodiscard]] std::int32_t Field550() const noexcept { return _field550; }
            void Field550(std::int32_t value) noexcept { _field550 = value; }
            [[nodiscard]] std::int32_t Field46C() const noexcept { return _field46C; }
            void Field46C(std::int32_t value) noexcept { _field46C = value; }

        private:
            OpenTK::Mathematics::Vector3 _position{};
            MphRead::Hunter _hunter = MphRead::Hunter::Samus;
            TestLogic::SomeFlags _someFlags = TestLogic::SomeFlags::None;
            TestLogic::MoreFlags _moreFlags = TestLogic::MoreFlags::None;
            std::shared_ptr<CModel> _model{};
            std::shared_ptr<CModel> _gun{};
            std::shared_ptr<CModel> _gunSmoke{};
            OpenTK::Mathematics::Matrix4x3 _someMatrix{};
            std::uint8_t _field4BB = 0;
            std::shared_ptr<CModel> _field1A4{};
            std::uint32_t _health = 0;
            std::int32_t _field358 = 0;
            std::int32_t _field6D0 = 0;
            OpenTK::Mathematics::Vector3 _field64{};
            OpenTK::Mathematics::Vector3 _fieldB4{};
            std::int16_t _fieldE2 = 0;
            std::uint8_t _field4D6 = 0;
            std::int32_t _field550 = 0;
            std::int32_t _field46C = 0;
        };

        class CModel
        {
        public:
            CModel() = default;
            CModel(const CModel&) = delete;
            CModel& operator=(const CModel&) = delete;
            CModel(CModel&&) = delete;
            CModel& operator=(CModel&&) = delete;

            [[nodiscard]] std::shared_ptr<MModel> Model() const noexcept { return _model; }
            void Model(std::shared_ptr<MModel> value) noexcept { _model = std::move(value); }
            [[nodiscard]] std::int16_t SomeFlag() const noexcept { return _someFlag; }
            void SomeFlag(std::int16_t value) noexcept { _someFlag = value; }
            [[nodiscard]] std::shared_ptr<CNodeAnimation> NodeAnimation() const noexcept { return _nodeAnimation; }
            void NodeAnimation(std::shared_ptr<CNodeAnimation> value) noexcept { _nodeAnimation = std::move(value); }

        private:
            std::shared_ptr<MModel> _model{};
            std::int16_t _someFlag = 0;
            std::shared_ptr<CNodeAnimation> _nodeAnimation{};
        };

        class CNodeAnimation
        {
        public:
            CNodeAnimation() = default;
            CNodeAnimation(const CNodeAnimation&) = delete;
            CNodeAnimation& operator=(const CNodeAnimation&) = delete;
            CNodeAnimation(CNodeAnimation&&) = delete;
            CNodeAnimation& operator=(CNodeAnimation&&) = delete;

            [[nodiscard]] std::uintptr_t NodeAnimation() const noexcept { return _nodeAnimation; }
            void NodeAnimation(std::uintptr_t value) noexcept { _nodeAnimation = value; }

        private:
            std::uintptr_t _nodeAnimation = 0;
        };

        class MModel
        {
        public:
            MModel() = default;
            MModel(const MModel&) = delete;
            MModel& operator=(const MModel&) = delete;
            MModel(MModel&&) = delete;
            MModel& operator=(MModel&&) = delete;

            [[nodiscard]] std::uintptr_t NodeAnimation() const noexcept { return _nodeAnimation; }
            void NodeAnimation(std::uintptr_t value) noexcept { _nodeAnimation = value; }
            [[nodiscard]] std::uint8_t Flags() const noexcept { return _flags; }
            void Flags(std::uint8_t value) noexcept { _flags = value; }
            [[nodiscard]] float Scale() const noexcept { return _scale; }
            void Scale(float value) noexcept { _scale = value; }

        private:
            std::uintptr_t _nodeAnimation = 0;
            std::uint8_t _flags = 0;
            float _scale = 0.0F;
        };

        static void TestSaveBlocks();
        [[nodiscard]] static std::shared_ptr<CompletionValues> GetCompletionValues(
            std::shared_ptr<Memory::StorySaveData> save);
        [[nodiscard]] static std::int32_t GetCompletionPercentage(
            std::shared_ptr<Memory::StorySaveData> save);
        static void TestLogic1();
        static void TestLogic2(std::shared_ptr<CPlayer> player, std::int32_t playerId);

    private:
        TestLogic() = delete;
        TestLogic(const TestLogic&) = delete;
        TestLogic& operator=(const TestLogic&) = delete;
        TestLogic(TestLogic&&) = delete;
        TestLogic& operator=(TestLogic&&) = delete;

        [[nodiscard]] static bool IsVisibleMaybe(std::shared_ptr<CPlayer> player) noexcept;
        static void Memory1FF8000();
        static void CModelDraw(
            std::shared_ptr<CModel> model,
            OpenTK::Mathematics::Matrix4x3 someMatrix);
        static void DrawAnimatedModel(
            std::shared_ptr<MModel> model,
            OpenTK::Mathematics::Matrix4x3 texMatrix,
            std::uint8_t flags);
        static void CModelInitializeAnimationData(std::shared_ptr<CModel> model);
        static void CNodeAnimationSetData(
            std::shared_ptr<MModel> model,
            std::uintptr_t nodeAnimation);

        static const std::shared_ptr<MModel> _mdl200D960;
        static const std::shared_ptr<MModel> _mdl200D938;
        static const std::shared_ptr<MModel> _mdl200E490;
        static const OpenTK::Mathematics::Matrix4x3 _mtx20D955C;
        static const OpenTK::Mathematics::Matrix4x3 _viewMatrix;
        static OpenTK::Mathematics::Matrix4x3 _currentTextureMatrix;
        static const std::int32_t _mem20E97B0;
        static const std::int32_t _mem20DA5D0;
        static std::int32_t _mem20E3EA0;
        static const std::int32_t _gameState;
    };
}
