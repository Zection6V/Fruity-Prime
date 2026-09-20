#pragma once

#include "../Formats/Enums.hpp"
#include "../Formats/Types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <memory>
#include <new>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace MphRead::ReadDetail
{
    template <typename T>
    [[nodiscard]] T MarshalRead(std::span<const std::uint8_t> bytes);
}

namespace MphRead::Testing
{
    class TestPlayer final
    {
    private:
        template <typename T, std::size_t N>
        [[nodiscard]] static T ReadScalar(
            const std::array<std::uint8_t, N>& raw, std::size_t offset) noexcept
        {
            T value{};
            std::memcpy(static_cast<void*>(std::addressof(value)),
                raw.data() + offset, sizeof(T));
            return value;
        }

        template <typename T>
        static T& AssignReadonly(T& self, const T& other) noexcept
        {
            if (std::addressof(self) != std::addressof(other))
            {
                self.~T();
                ::new (static_cast<void*>(std::addressof(self))) T(other);
            }
            return self;
        }

    public:
        template <typename T>
        class ReadOnlyList final
        {
        public:
            class const_iterator final
            {
            public:
                using value_type = T;
                using difference_type = std::ptrdiff_t;
                using iterator_category = std::input_iterator_tag;

                [[nodiscard]] T operator*() const
                {
                    return _items->at(_index);
                }

                const_iterator& operator++() noexcept
                {
                    ++_index;
                    return *this;
                }

                friend bool operator==(const const_iterator& left, const const_iterator& right) noexcept
                {
                    return left._items == right._items && left._index == right._index;
                }

                friend bool operator!=(const const_iterator& left, const const_iterator& right) noexcept
                {
                    return !(left == right);
                }

            private:
                const_iterator(std::shared_ptr<const std::vector<T>> items, std::size_t index) noexcept
                    : _items(std::move(items)), _index(index)
                {
                }

                std::shared_ptr<const std::vector<T>> _items;
                std::size_t _index = 0;

                friend class ReadOnlyList;
            };

            ReadOnlyList(const ReadOnlyList&) = delete;
            ReadOnlyList(ReadOnlyList&&) = delete;
            ReadOnlyList& operator=(const ReadOnlyList&) = delete;
            ReadOnlyList& operator=(ReadOnlyList&&) = delete;

            [[nodiscard]] std::int32_t Count() const noexcept
            {
                return static_cast<std::int32_t>(_items->size());
            }

            [[nodiscard]] T operator[](std::int32_t index) const
            {
                if (index < 0 || static_cast<std::size_t>(index) >= _items->size())
                {
                    throw std::out_of_range(
                        "Index was out of range. Must be non-negative and less than the size "
                        "of the collection. (Parameter 'index')");
                }
                return (*_items)[static_cast<std::size_t>(index)];
            }

            [[nodiscard]] const_iterator begin() const noexcept
            {
                return const_iterator(_items, 0);
            }

            [[nodiscard]] const_iterator end() const noexcept
            {
                return const_iterator(_items, _items->size());
            }

        private:
            explicit ReadOnlyList(std::shared_ptr<const std::vector<T>> items) noexcept
                : _items(std::move(items))
            {
            }

            std::shared_ptr<const std::vector<T>> _items;

            friend class TestPlayer;
        };

        struct PlayerControls;

        struct ButtonControl
        {
            const ButtonFlags Button{};
            const PressFlags Flags{};

            ButtonControl() noexcept = default;
            ButtonControl(const ButtonControl&) noexcept = default;

            ButtonControl& operator=(const ButtonControl& other) noexcept
            {
                return TestPlayer::AssignReadonly(*this, other);
            }

        private:
            constexpr ButtonControl(ButtonFlags button, PressFlags flags) noexcept
                : Button(button), Flags(flags)
            {
            }

            template <std::size_t N>
            [[nodiscard]] static ButtonControl FromMarshaledBytes(
                const std::array<std::uint8_t, N>& raw, std::size_t offset) noexcept
            {
                return ButtonControl(
                    TestPlayer::ReadScalar<ButtonFlags>(raw, offset),
                    TestPlayer::ReadScalar<PressFlags>(raw, offset + sizeof(ButtonFlags)));
            }

            friend struct PlayerControls;
        };

        struct PlayerControls
        {
            const std::uint32_t Flags{};
            const ButtonControl Left{};
            const ButtonControl Right{};
            const ButtonControl Up{};
            const ButtonControl Down{};
            const ButtonControl Field14{};
            const ButtonControl Field18{};
            const ButtonControl Field1C{};
            const ButtonControl Field20{};
            const ButtonControl Field24{};
            const ButtonControl Field28{};
            const ButtonControl Field2C{};
            const ButtonControl Field30{};
            const ButtonControl Shoot{};
            const ButtonControl Jump{}; // repeated touch is checked separately
            const ButtonControl Morph{};
            const ButtonControl Field40{};
            const ButtonControl Field44{};
            const ButtonControl Bomb{};
            const ButtonControl Unused4C{};
            const ButtonControl BoostCharge{};
            const ButtonControl Unused54{};
            const ButtonControl Unused58{};
            const ButtonControl Unused5C{};
            const ButtonControl Unused60{};
            const ButtonControl NoxusAltAttack{};
            const ButtonControl Unused68{}; // unused alt attack
            const ButtonControl SpireAltAttack{};
            const ButtonControl TraceAltAttack{};
            const ButtonControl Unused74{}; // unused alt attack
            const ButtonControl WeavelAltAttack{};
            const ButtonControl Zoom{};
            const ButtonControl Respawn{};
            const ButtonControl Unused84{};
            const std::int32_t Field88{};
            const std::int32_t Field8C{};
            const std::int32_t Field90{};
            const std::int32_t Field94{};
            const std::int32_t Field98{};

            PlayerControls() noexcept = default;
            PlayerControls(const PlayerControls&) noexcept = default;

            PlayerControls& operator=(const PlayerControls& other) noexcept
            {
                return TestPlayer::AssignReadonly(*this, other);
            }

        private:
            explicit PlayerControls(const std::array<std::uint8_t, 0x9C>& raw) noexcept
              : Flags(TestPlayer::ReadScalar<std::uint32_t>(raw, 0x0))
              , Left(ButtonControl::FromMarshaledBytes(raw, 0x4))
              , Right(ButtonControl::FromMarshaledBytes(raw, 0x8))
              , Up(ButtonControl::FromMarshaledBytes(raw, 0xC))
              , Down(ButtonControl::FromMarshaledBytes(raw, 0x10))
              , Field14(ButtonControl::FromMarshaledBytes(raw, 0x14))
              , Field18(ButtonControl::FromMarshaledBytes(raw, 0x18))
              , Field1C(ButtonControl::FromMarshaledBytes(raw, 0x1C))
              , Field20(ButtonControl::FromMarshaledBytes(raw, 0x20))
              , Field24(ButtonControl::FromMarshaledBytes(raw, 0x24))
              , Field28(ButtonControl::FromMarshaledBytes(raw, 0x28))
              , Field2C(ButtonControl::FromMarshaledBytes(raw, 0x2C))
              , Field30(ButtonControl::FromMarshaledBytes(raw, 0x30))
              , Shoot(ButtonControl::FromMarshaledBytes(raw, 0x34))
              , Jump(ButtonControl::FromMarshaledBytes(raw, 0x38))
              , Morph(ButtonControl::FromMarshaledBytes(raw, 0x3C))
              , Field40(ButtonControl::FromMarshaledBytes(raw, 0x40))
              , Field44(ButtonControl::FromMarshaledBytes(raw, 0x44))
              , Bomb(ButtonControl::FromMarshaledBytes(raw, 0x48))
              , Unused4C(ButtonControl::FromMarshaledBytes(raw, 0x4C))
              , BoostCharge(ButtonControl::FromMarshaledBytes(raw, 0x50))
              , Unused54(ButtonControl::FromMarshaledBytes(raw, 0x54))
              , Unused58(ButtonControl::FromMarshaledBytes(raw, 0x58))
              , Unused5C(ButtonControl::FromMarshaledBytes(raw, 0x5C))
              , Unused60(ButtonControl::FromMarshaledBytes(raw, 0x60))
              , NoxusAltAttack(ButtonControl::FromMarshaledBytes(raw, 0x64))
              , Unused68(ButtonControl::FromMarshaledBytes(raw, 0x68))
              , SpireAltAttack(ButtonControl::FromMarshaledBytes(raw, 0x6C))
              , TraceAltAttack(ButtonControl::FromMarshaledBytes(raw, 0x70))
              , Unused74(ButtonControl::FromMarshaledBytes(raw, 0x74))
              , WeavelAltAttack(ButtonControl::FromMarshaledBytes(raw, 0x78))
              , Zoom(ButtonControl::FromMarshaledBytes(raw, 0x7C))
              , Respawn(ButtonControl::FromMarshaledBytes(raw, 0x80))
              , Unused84(ButtonControl::FromMarshaledBytes(raw, 0x84))
              , Field88(TestPlayer::ReadScalar<std::int32_t>(raw, 0x88))
              , Field8C(TestPlayer::ReadScalar<std::int32_t>(raw, 0x8C))
              , Field90(TestPlayer::ReadScalar<std::int32_t>(raw, 0x90))
              , Field94(TestPlayer::ReadScalar<std::int32_t>(raw, 0x94))
              , Field98(TestPlayer::ReadScalar<std::int32_t>(raw, 0x98))
            {
            }

            [[nodiscard]] static PlayerControls FromMarshaledBytes(
                const std::array<std::uint8_t, 0x9C>& raw) noexcept
            {
                return PlayerControls(raw);
            }

            template <typename T>
            friend T MphRead::ReadDetail::MarshalRead(std::span<const std::uint8_t> bytes);
        };

        struct PlayerValues
        {
            const Fixed BipedTractionLr{};
            const Fixed BipedTractionFb{};
            const Fixed WalkHSpeedCap{};
            const Fixed StrafeHSpeedCap{};
            const Fixed MinAltHSpeed{}; // boost is considered over if speed drops below this -- also minimum(?) for alt movement SFX
            const Fixed BoostHSpeedCap{};
            const Fixed BipedGravity{}; // gravity to apply to biped form in the air or on slippery terrain
            const Fixed AltGravityAir{}; // gravity to apply to alt form in the air
            const Fixed AltGravityGround{}; // gravity to apply to alt form on the ground
            const Fixed JumpSpeed{}; // 1433 (0.35) is used if the player is prime hunter
            const Fixed WalkSpeedFactor{};
            const Fixed AltGroundSpeedFactor{};
            const Fixed StrafeSpeedFactor{};
            const Fixed AirSpeedFactor{};
            const Fixed StandSpeedFactor{}; // grounded, biped, "moving" flag bit not set
            const Fixed Field3C{}; // alt touch movement factor when grounded gravity
            const Fixed AltCollisionRadius{};
            const Fixed AltCollisionY{};
            const std::uint16_t BoostChargeMin{};
            const std::uint16_t BoostChargeMax{};
            const Fixed BoostSpeedMin{};
            const Fixed BoostSpeedMax{};
            const Fixed AltSpeedCapHInc{}; // if player's h speed is greater than the cap, reduce it by this amount
            const Fixed Field58{}; // h speed in "gravity" alt form at which aim direction is updated
            const Fixed Field5C{}; // h speed in "gravity" alt form at which movement direction (for lost octolith position?) is updated
            const Fixed WalkViewBob{};
            const std::uint32_t Field64{}; // aim factor or something
            const std::uint16_t Field68{}; // gun idle time -- but also FieldFC
            const std::uint16_t Padding6A{};
            const Fixed RegularFov{}; // 39
            const std::uint32_t Field70{}; // camera-related
            const std::uint32_t Field74{}; // aim y offset or something
            const std::uint32_t Field78{}; // camera-related (x?)
            const std::uint32_t Field7C{}; // camera-related (y?)
            const std::uint32_t Field80{}; // camera-related (z?)
            const std::uint32_t Field84{}; // camera-related
            const std::uint32_t Field88{}; // camera-related
            const std::uint32_t Field8C{}; // camera-related
            const std::uint32_t Field90{}; // camera-related
            const Fixed MinCollisionHeight{}; // cylinder bottom
            const Fixed MaxCollisionHeight{}; // cylinder top
            const Fixed BipedCollisionRadius{};
            const Fixed FieldA0{};
            const Fixed FieldA4{};
            const Fixed FieldA8{};
            const std::uint16_t DamageInvuln{};
            const std::uint16_t DamageFlashDuration{};
            const Fixed FieldB0{}; // gun draw offset/factor - gun eff_vec_2
            const Fixed FieldB4{}; // gun draw offset/factor - player vec_1
            const Fixed FieldB8{}; // gun draw offset/factor - player vec_2
            const Fixed SmokeZOffset{};
            const std::uint32_t BombCooldown{};
            const Fixed BombSelfRadius{};
            const Fixed BombSelfRadiusSquared{}; // set in load_some_hunter_stuff
            const Fixed BombRadius{};
            const Fixed BombRadiusSquared{}; // set in load_some_hunter_stuff
            const Fixed BombJumpSpeed{};
            const std::uint32_t BombRefillTime{};
            const std::uint16_t BombDamage{};
            const std::uint16_t BombEnemyDamage{};
            const std::uint16_t FieldE0{};
            const std::uint16_t SpawnInvuln{};
            const std::uint16_t FieldE4{}; // some touch duration threshold
            const std::uint16_t PaddingE6{};
            const std::uint32_t FieldE8{};
            const std::uint32_t FieldEC{};
            const std::uint32_t ViewSwayStartTime{};
            const std::uint32_t SwayTimeIncrement{};
            const std::uint32_t SwayLimit{};
            const std::uint32_t FieldFC{}; // gun idle time -- but also Field68
            const std::uint16_t MpAmmoCap{};
            const std::uint8_t AmmoRecharge{}; // FH leftover, non-functional due to empty weapons being automatically unequipped
            const std::uint8_t Padding103{};
            const std::uint16_t EnergyStart{};
            const std::uint16_t Field106{};
            const std::uint8_t AltGroundNoGravity{}; // don't apply gravity to alt form on ground unless terrain is slippery
            const std::uint8_t Padding109{};
            const std::uint16_t Padding10A{};
            const Fixed FallDamageSpeed{}; // absolute value
            const std::uint32_t FallDamageMax{}; // 80% of this is the actual max
            const std::uint32_t Field114{}; // some kind of camera bob value for movement
            const std::uint32_t Field118{};
            const Fixed JumpPadSlideFactor{};
            const std::uint32_t Field120{};
            const std::uint32_t Field124{};
            const std::uint32_t Field128{};
            const Fixed AltSpinSpeed{}; // used for noxus
            const std::uint32_t Field130{};
            const std::uint32_t Field134{};
            const std::uint32_t Field138{};
            const std::uint32_t Field13C{};
            const Fixed Field140{}; // 140/144/148 are related to spin/wobble when noxus alt form lands
            const Fixed Field144{};
            const Fixed Field148{};
            const std::uint32_t Field14C{}; // speed loss value
            const std::uint16_t Field150{}; // speed counter value
            const std::uint16_t NoxusAltAttackStartup{}; // SFX played at halfway time, anim frame based on elapsed percentage
            const std::uint32_t Field154{};
            const std::uint32_t Field158{};
            const Fixed LungeHSpeed{}; // lunge = trace/weavel
            const Fixed LungeVSpeed{};
            const std::uint16_t AltAttackDamage{}; // includes boost
            const std::uint16_t AltAttackCooldown{}; // includes boost

            PlayerValues() noexcept = default;
            PlayerValues(const PlayerValues&) noexcept = default;

            PlayerValues& operator=(const PlayerValues& other) noexcept
            {
                return TestPlayer::AssignReadonly(*this, other);
            }

        private:
            explicit PlayerValues(const std::array<std::uint8_t, 0x168>& raw) noexcept
              : BipedTractionLr(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x0)))
              , BipedTractionFb(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x4)))
              , WalkHSpeedCap(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x8)))
              , StrafeHSpeedCap(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0xC)))
              , MinAltHSpeed(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x10)))
              , BoostHSpeedCap(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x14)))
              , BipedGravity(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x18)))
              , AltGravityAir(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x1C)))
              , AltGravityGround(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x20)))
              , JumpSpeed(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x24)))
              , WalkSpeedFactor(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x28)))
              , AltGroundSpeedFactor(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x2C)))
              , StrafeSpeedFactor(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x30)))
              , AirSpeedFactor(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x34)))
              , StandSpeedFactor(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x38)))
              , Field3C(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x3C)))
              , AltCollisionRadius(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x40)))
              , AltCollisionY(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x44)))
              , BoostChargeMin(TestPlayer::ReadScalar<std::uint16_t>(raw, 0x48))
              , BoostChargeMax(TestPlayer::ReadScalar<std::uint16_t>(raw, 0x4A))
              , BoostSpeedMin(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x4C)))
              , BoostSpeedMax(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x50)))
              , AltSpeedCapHInc(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x54)))
              , Field58(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x58)))
              , Field5C(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x5C)))
              , WalkViewBob(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x60)))
              , Field64(TestPlayer::ReadScalar<std::uint32_t>(raw, 0x64))
              , Field68(TestPlayer::ReadScalar<std::uint16_t>(raw, 0x68))
              , Padding6A(TestPlayer::ReadScalar<std::uint16_t>(raw, 0x6A))
              , RegularFov(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x6C)))
              , Field70(TestPlayer::ReadScalar<std::uint32_t>(raw, 0x70))
              , Field74(TestPlayer::ReadScalar<std::uint32_t>(raw, 0x74))
              , Field78(TestPlayer::ReadScalar<std::uint32_t>(raw, 0x78))
              , Field7C(TestPlayer::ReadScalar<std::uint32_t>(raw, 0x7C))
              , Field80(TestPlayer::ReadScalar<std::uint32_t>(raw, 0x80))
              , Field84(TestPlayer::ReadScalar<std::uint32_t>(raw, 0x84))
              , Field88(TestPlayer::ReadScalar<std::uint32_t>(raw, 0x88))
              , Field8C(TestPlayer::ReadScalar<std::uint32_t>(raw, 0x8C))
              , Field90(TestPlayer::ReadScalar<std::uint32_t>(raw, 0x90))
              , MinCollisionHeight(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x94)))
              , MaxCollisionHeight(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x98)))
              , BipedCollisionRadius(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x9C)))
              , FieldA0(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0xA0)))
              , FieldA4(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0xA4)))
              , FieldA8(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0xA8)))
              , DamageInvuln(TestPlayer::ReadScalar<std::uint16_t>(raw, 0xAC))
              , DamageFlashDuration(TestPlayer::ReadScalar<std::uint16_t>(raw, 0xAE))
              , FieldB0(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0xB0)))
              , FieldB4(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0xB4)))
              , FieldB8(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0xB8)))
              , SmokeZOffset(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0xBC)))
              , BombCooldown(TestPlayer::ReadScalar<std::uint32_t>(raw, 0xC0))
              , BombSelfRadius(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0xC4)))
              , BombSelfRadiusSquared(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0xC8)))
              , BombRadius(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0xCC)))
              , BombRadiusSquared(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0xD0)))
              , BombJumpSpeed(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0xD4)))
              , BombRefillTime(TestPlayer::ReadScalar<std::uint32_t>(raw, 0xD8))
              , BombDamage(TestPlayer::ReadScalar<std::uint16_t>(raw, 0xDC))
              , BombEnemyDamage(TestPlayer::ReadScalar<std::uint16_t>(raw, 0xDE))
              , FieldE0(TestPlayer::ReadScalar<std::uint16_t>(raw, 0xE0))
              , SpawnInvuln(TestPlayer::ReadScalar<std::uint16_t>(raw, 0xE2))
              , FieldE4(TestPlayer::ReadScalar<std::uint16_t>(raw, 0xE4))
              , PaddingE6(TestPlayer::ReadScalar<std::uint16_t>(raw, 0xE6))
              , FieldE8(TestPlayer::ReadScalar<std::uint32_t>(raw, 0xE8))
              , FieldEC(TestPlayer::ReadScalar<std::uint32_t>(raw, 0xEC))
              , ViewSwayStartTime(TestPlayer::ReadScalar<std::uint32_t>(raw, 0xF0))
              , SwayTimeIncrement(TestPlayer::ReadScalar<std::uint32_t>(raw, 0xF4))
              , SwayLimit(TestPlayer::ReadScalar<std::uint32_t>(raw, 0xF8))
              , FieldFC(TestPlayer::ReadScalar<std::uint32_t>(raw, 0xFC))
              , MpAmmoCap(TestPlayer::ReadScalar<std::uint16_t>(raw, 0x100))
              , AmmoRecharge(TestPlayer::ReadScalar<std::uint8_t>(raw, 0x102))
              , Padding103(TestPlayer::ReadScalar<std::uint8_t>(raw, 0x103))
              , EnergyStart(TestPlayer::ReadScalar<std::uint16_t>(raw, 0x104))
              , Field106(TestPlayer::ReadScalar<std::uint16_t>(raw, 0x106))
              , AltGroundNoGravity(TestPlayer::ReadScalar<std::uint8_t>(raw, 0x108))
              , Padding109(TestPlayer::ReadScalar<std::uint8_t>(raw, 0x109))
              , Padding10A(TestPlayer::ReadScalar<std::uint16_t>(raw, 0x10A))
              , FallDamageSpeed(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x10C)))
              , FallDamageMax(TestPlayer::ReadScalar<std::uint32_t>(raw, 0x110))
              , Field114(TestPlayer::ReadScalar<std::uint32_t>(raw, 0x114))
              , Field118(TestPlayer::ReadScalar<std::uint32_t>(raw, 0x118))
              , JumpPadSlideFactor(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x11C)))
              , Field120(TestPlayer::ReadScalar<std::uint32_t>(raw, 0x120))
              , Field124(TestPlayer::ReadScalar<std::uint32_t>(raw, 0x124))
              , Field128(TestPlayer::ReadScalar<std::uint32_t>(raw, 0x128))
              , AltSpinSpeed(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x12C)))
              , Field130(TestPlayer::ReadScalar<std::uint32_t>(raw, 0x130))
              , Field134(TestPlayer::ReadScalar<std::uint32_t>(raw, 0x134))
              , Field138(TestPlayer::ReadScalar<std::uint32_t>(raw, 0x138))
              , Field13C(TestPlayer::ReadScalar<std::uint32_t>(raw, 0x13C))
              , Field140(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x140)))
              , Field144(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x144)))
              , Field148(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x148)))
              , Field14C(TestPlayer::ReadScalar<std::uint32_t>(raw, 0x14C))
              , Field150(TestPlayer::ReadScalar<std::uint16_t>(raw, 0x150))
              , NoxusAltAttackStartup(TestPlayer::ReadScalar<std::uint16_t>(raw, 0x152))
              , Field154(TestPlayer::ReadScalar<std::uint32_t>(raw, 0x154))
              , Field158(TestPlayer::ReadScalar<std::uint32_t>(raw, 0x158))
              , LungeHSpeed(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x15C)))
              , LungeVSpeed(Fixed(TestPlayer::ReadScalar<std::int32_t>(raw, 0x160)))
              , AltAttackDamage(TestPlayer::ReadScalar<std::uint16_t>(raw, 0x164))
              , AltAttackCooldown(TestPlayer::ReadScalar<std::uint16_t>(raw, 0x166))
            {
            }

            [[nodiscard]] static PlayerValues FromMarshaledBytes(
                const std::array<std::uint8_t, 0x168>& raw) noexcept
            {
                return PlayerValues(raw);
            }

            template <typename T>
            friend T MphRead::ReadDetail::MarshalRead(std::span<const std::uint8_t> bytes);
        };

        [[nodiscard]] static std::shared_ptr<const ReadOnlyList<PlayerControls>> GetPlayerControls();
        [[nodiscard]] static std::shared_ptr<const ReadOnlyList<PlayerValues>> GetPlayerValues();

    private:
        TestPlayer() = delete;
        TestPlayer(const TestPlayer&) = delete;
        TestPlayer& operator=(const TestPlayer&) = delete;
        TestPlayer(TestPlayer&&) = delete;
        TestPlayer& operator=(TestPlayer&&) = delete;
    };
}
