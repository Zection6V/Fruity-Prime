#include "../Entities/Players/HalfturretEntity.hpp"
#include "../Formats/NodeData.hpp"
#include "../Metadata/Metadata.hpp"
#include "../Mods/Gameplay/NativeGameplayClock.hpp"

#include <array>
#include <cstdio>
#include <limits>
#include <stdexcept>

namespace
{
    int checks = 0;
    void Expect(bool ok, const char* reason)
    {
        if (!ok) throw std::runtime_error(reason);
        ++checks;
    }

    void FireRateAndClock()
    {
        using Rate = MphRead::Mods::Combat::HalfturretFireRate;
        using Clock = MphRead::Mods::Gameplay::NativeGameplayClock;
        Expect(Rate::Normal == 6144 && Rate::Floor == 2867, "EU1.1 raw constants");
        const std::array<std::uint32_t, 5> damage{0, 1, 12, 36, 100};
        const std::array<std::int32_t, 5> factor{6144, 6083, 5412, 3948, 2867};
        const std::array<std::uint32_t, 5> threshold{15, 15, 13, 10, 7};
        for (std::size_t i = 0; i < damage.size(); ++i)
        {
            Expect(Rate::AfterDamage(Rate::Normal, damage[i]) == factor[i], "damage raw arithmetic");
            Expect(Rate::Threshold(10, factor[i]) == threshold[i], "fixed rounded shot threshold");
        }
        Expect(Rate::AfterDamage(Rate::Normal, std::numeric_limits<std::uint32_t>::max())
            == Rate::Floor, "huge damage cannot wrap");
        Expect(Rate::AfterDamage(Rate::Floor, 1) == Rate::Floor, "floor remains clamped");
        auto raw = Rate::Floor;
        for (int i = 0; i < 53; ++i) raw = Rate::Recover(raw);
        Expect(raw == 6100, "53 native recovery ticks");
        Expect(Rate::Recover(raw) == Rate::Normal, "54th native tick clamps to normal");
        Expect(Rate::Recover(6206) == Rate::Normal, "above normal double truncation");
        Expect(Rate::Recover(Rate::Normal) == Rate::Normal, "normal remains stable");
        Expect(Rate::Threshold(1, Rate::Normal) == 7, "minimum shot threshold");
        Expect(!Clock::IsNativeTick(0), "initialization is not a gameplay tick");
        int ticks = 0;
        for (std::uint64_t frame = 1; frame <= 120; ++frame)
        {
            Expect(Clock::IsNativeTick(frame) == (frame % 2 == 0), "common scene phase");
            ticks += Clock::IsNativeTick(frame);
        }
        Expect(ticks == 60, "60 Hz scene has 30 Hz native cadence");
    }
}

namespace MphRead::Entities
{
    // Exercise the real reused object's reset and damage hooks without game assets.
    class WeavelAltFormParityTest final
    {
    public:
        static void Run()
        {
            using Rate = Mods::Combat::HalfturretFireRate;
            auto turret = std::make_shared<HalfturretEntity>(nullptr, nullptr);
            const auto equip = turret->EquipInfo();
            // Current is selected by room loading; asset-free tests bind the MP table directly.
            const auto weapon = Weapons::WeaponsMP->at(3);
            equip->Weapon = weapon;
            const auto defaultDamage = equip->UnchargedDamage();
            const auto defaultHeadshot = equip->HeadshotDamage();
            const auto defaultSplash = equip->SplashDamage();
            const auto defaultMinSplash = equip->MinChargeSplashDamage();
            const auto defaultChargedSplash = equip->ChargedSplashDamage();
            turret->OnTakeDamage(turret, 36);
            Expect(turret->CooldownFactorRaw() == 3948 && turret->NativeShotThreshold() == 10,
                "production damage hook and threshold");
            Expect(turret->_target == turret && turret->_targetTimer == 30, "native retaliation timer");
            turret->_closestNode = std::make_shared<Formats::NodeData3>(Vector3Fx{});
            turret->_health = 50;
            turret->_timeSinceDamage = 2;
            turret->_timeSinceFrozen = 61;
            turret->OnFrozen();
            Expect(turret->_freezeTimer == 75, "long freeze uses native timer");
            turret->_timeSinceFrozen = 0;
            turret->_freezeTimer = 0;
            turret->OnFrozen();
            Expect(turret->_freezeTimer == 15, "short freeze uses native timer");
            turret->_burnTimer = 150;
            turret->_ySpeed = -0.7F;
            turret->_grounded = true;
            turret->_aimVector = {1, 2, 3};
            turret->_cooldownTimer = 65;
            equip->UnchargedDamage(3);
            equip->HeadshotDamage(3);
            equip->SplashDamage(3);
            equip->MinChargeSplashDamage(3);
            equip->ChargedSplashDamage(3);
            equip->DmgDirTypes = {0, 0};
            equip->ChargeLevel = 20;
            equip->SmokeLevel = 7;
            equip->InfiniteAmmo = true;
            equip->GetAmmo = [] { return 999; };
            equip->SetAmmo = [](int) {};
            turret->ResetForSpawn();
            Expect(turret->_target == nullptr && turret->_closestNode == nullptr,
                "reset clears previous life references");
            Expect(turret->_health == 0 && turret->_timeSinceDamage == 65535
                && turret->_timeSinceFrozen == 0 && turret->_freezeTimer == 0
                && turret->_burnTimer == 0 && turret->_burnEffect == nullptr, "reset clears damage and afflictions");
            Expect(turret->_ySpeed == 0 && !turret->_grounded
                && OpenTK::Mathematics::IsZero(turret->_aimVector), "reset clears physical state");
            Expect(turret->_targetTimer == 0 && turret->_cooldownTimer == 0
                && turret->CooldownFactorRaw() == Rate::Normal, "reset clears native combat state");
            Expect(equip == turret->EquipInfo() && equip->Weapon == nullptr && equip->Beams == nullptr
                && equip->ChargeLevel == 0 && equip->SmokeLevel == 0 && !equip->InfiniteAmmo
                && !equip->GetAmmo && !equip->SetAmmo, "fresh EquipInfo without replacing reused object");
            Expect(equip->DmgDirTypes == std::array<std::uint8_t, 2>{255, 255}, "reset damage direction overrides");
            equip->Weapon = weapon; // same binding Initialize performs after reset
            Expect(equip->UnchargedDamage() == defaultDamage && equip->HeadshotDamage() == defaultHeadshot
                && equip->SplashDamage() == defaultSplash && equip->MinChargeSplashDamage() == defaultMinSplash
                && equip->ChargedSplashDamage() == defaultChargedSplash, "Story damage overrides do not survive reset");
            Expect(weapon->UnchargedDamage == defaultDamage, "shared Battlehammer metadata remains unchanged");
            Expect(turret->NativeShotThreshold() == 15, "new life starts at normal shot threshold");
            turret->ResetForSpawn();
            Expect(turret->CooldownFactorRaw() == Rate::Normal && turret->Health() == 0,
                "repeated reset is stable");
        }
    };
}

int main()
{
    try
    {
        FireRateAndClock();
        MphRead::Entities::WeavelAltFormParityTest::Run();
        std::printf("WeavelAltFormParity PASS %d checks (fire rate, cadence, reused turret reset)\n", checks);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "WeavelAltFormParity FAIL: %s\n", error.what());
        return 1;
    }
}
