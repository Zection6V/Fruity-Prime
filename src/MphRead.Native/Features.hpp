#pragma once

#include <string>
#include <unordered_map>

namespace MphRead
{
    class Bugfixes final
    {
    public:
        Bugfixes() = delete;

        [[nodiscard]] static bool SmoothCamSeqHandoff() noexcept;
        static void SmoothCamSeqHandoff(bool value) noexcept;
        [[nodiscard]] static bool BetterCamSeqNodeRef() noexcept;
        static void BetterCamSeqNodeRef(bool value) noexcept;
        [[nodiscard]] static bool NoStrayRespawnText() noexcept;
        static void NoStrayRespawnText(bool value) noexcept;
        [[nodiscard]] static bool CorrectBountySfx() noexcept;
        static void CorrectBountySfx(bool value) noexcept;
        [[nodiscard]] static bool NoDoubleEnemyDeath() noexcept;
        static void NoDoubleEnemyDeath(bool value) noexcept;
        [[nodiscard]] static bool NoSlenchRollTimerUnderflow() noexcept;
        static void NoSlenchRollTimerUnderflow(bool value) noexcept;

        static void Load(const std::unordered_map<std::string, std::string>& values);
        [[nodiscard]] static std::unordered_map<std::string, std::string> Commit();

    private:
        static bool _smoothCamSeqHandoff;
        static bool _betterCamSeqNodeRef;
        static bool _noStrayRespawnText;
        static bool _correctBountySfx;
        static bool _noDoubleEnemyDeath;
        static bool _noSlenchRollTimerUnderflow;
    };

    class Features final
    {
    public:
        Features() = delete;

        [[nodiscard]] static bool NoRepeatEncounters() noexcept;
        static void NoRepeatEncounters(bool value) noexcept;
        [[nodiscard]] static bool AllowInvalidTeams() noexcept;
        static void AllowInvalidTeams(bool value) noexcept;
        [[nodiscard]] static bool TopScreenTargetInfo() noexcept;
        static void TopScreenTargetInfo(bool value) noexcept;
        [[nodiscard]] static bool HudSway() noexcept;
        static void HudSway(bool value) noexcept;
        [[nodiscard]] static bool TargetInfoSway() noexcept;
        static void TargetInfoSway(bool value) noexcept;
        [[nodiscard]] static bool DelayedIdleSway() noexcept;
        static void DelayedIdleSway(bool value) noexcept;
        [[nodiscard]] static bool NoIdleSway() noexcept;
        static void NoIdleSway(bool value) noexcept;
        [[nodiscard]] static bool NoMapCentering() noexcept;
        static void NoMapCentering(bool value) noexcept;
        [[nodiscard]] static bool MaxRoomDetail() noexcept;
        static void MaxRoomDetail(bool value) noexcept;
        [[nodiscard]] static bool MaxPlayerDetail() noexcept;
        static void MaxPlayerDetail(bool value) noexcept;
        [[nodiscard]] static bool LogSpatialAudio() noexcept;
        static void LogSpatialAudio(bool value) noexcept;
        [[nodiscard]] static bool HalfSecondAlarm() noexcept;
        static void HalfSecondAlarm(bool value) noexcept;
        [[nodiscard]] static bool FullBoostCharge() noexcept;
        static void FullBoostCharge(bool value) noexcept;
        [[nodiscard]] static bool BoostOpensDoors() noexcept;
        static void BoostOpensDoors(bool value) noexcept;
        [[nodiscard]] static bool AlternateHunters1P() noexcept;
        static void AlternateHunters1P(bool value) noexcept;
        [[nodiscard]] static bool HalfDamageUnscoped() noexcept;
        static void HalfDamageUnscoped(bool value) noexcept;

        [[nodiscard]] static bool ProHud() noexcept;
        static void ProHud(bool value) noexcept;

        static constexpr float ProHudWeaponListScale = 1.7F;

        [[nodiscard]] static float HelmetOpacity() noexcept;
        static void HelmetOpacity(float value) noexcept;
        [[nodiscard]] static float VisorOpacity() noexcept;
        static void VisorOpacity(float value) noexcept;

        [[nodiscard]] static float HudOpacity() noexcept;
        static void HudOpacity(float value) noexcept;
        [[nodiscard]] static float ReticleOpacity() noexcept;
        static void ReticleOpacity(float value) noexcept;

        [[nodiscard]] static bool FixedCrosshair() noexcept;
        static void FixedCrosshair(bool value) noexcept;
        [[nodiscard]] static bool CustomCrosshair() noexcept;
        static void CustomCrosshair(bool value) noexcept;
        [[nodiscard]] static bool ModernHud() noexcept;
        static void ModernHud(bool value) noexcept;
        [[nodiscard]] static float WeaponListScale() noexcept;
        static void WeaponListScale(float value) noexcept;
        [[nodiscard]] static bool FixedWeapon() noexcept;
        static void FixedWeapon(bool value) noexcept;

        [[nodiscard]] static bool ProHudFixedWeapon() noexcept;
        static void ProHudFixedWeapon(bool value) noexcept;

        static void Load(const std::unordered_map<std::string, std::string>& values);
        [[nodiscard]] static std::unordered_map<std::string, std::string> Commit();

    private:
        static bool _noRepeatEncounters;
        static bool _allowInvalidTeams;
        static bool _topScreenTargetInfo;
        static bool _hudSway;
        static bool _targetInfoSway;
        static bool _delayedIdleSway;
        static bool _noIdleSway;
        static bool _noMapCentering;
        static bool _maxRoomDetail;
        static bool _maxPlayerDetail;
        static bool _logSpatialAudio;
        static bool _halfSecondAlarm;
        static bool _fullBoostCharge;
        static bool _boostOpensDoors;
        static bool _alternateHunters1P;
        static bool _halfDamageUnscoped;
        static bool _proHud;
        static float _helmetOpacity;
        static float _visorOpacity;
        static float _hudOpacity;
        static float _reticleOpacity;
        static bool _fixedCrosshair;
        static bool _customCrosshair;
        static bool _modernHud;
        static float _weaponListScale;
        static bool _fixedWeapon;
        static bool _proHudFixedWeapon;
    };

    class Cheats final
    {
    public:
        Cheats() = delete;

        [[nodiscard]] static bool FreeWeaponSelect() noexcept;
        static void FreeWeaponSelect(bool value) noexcept;
        [[nodiscard]] static bool UnlimitedJumps() noexcept;
        static void UnlimitedJumps(bool value) noexcept;
        [[nodiscard]] static bool NoRandomEncounters() noexcept;
        static void NoRandomEncounters(bool value) noexcept;
        [[nodiscard]] static bool UnlockAllDoors() noexcept;
        static void UnlockAllDoors(bool value) noexcept;
        [[nodiscard]] static bool ContinueFromCurrentRoom() noexcept;
        static void ContinueFromCurrentRoom(bool value) noexcept;
        [[nodiscard]] static bool SkipPlanetIntros() noexcept;
        static void SkipPlanetIntros(bool value) noexcept;
        [[nodiscard]] static bool StartWithAllUpgrades() noexcept;
        static void StartWithAllUpgrades(bool value) noexcept;
        [[nodiscard]] static bool StartWithAllOctoliths() noexcept;
        static void StartWithAllOctoliths(bool value) noexcept;
        [[nodiscard]] static bool WalkThroughWalls() noexcept;
        static void WalkThroughWalls(bool value) noexcept;
        [[nodiscard]] static bool AlwaysFightGorea2() noexcept;
        static void AlwaysFightGorea2(bool value) noexcept;
        [[nodiscard]] static bool QuadrupleDamage() noexcept;
        static void QuadrupleDamage(bool value) noexcept;

        static void Load(const std::unordered_map<std::string, std::string>& values);
        [[nodiscard]] static std::unordered_map<std::string, std::string> Commit();

    private:
        static bool _freeWeaponSelect;
        static bool _unlimitedJumps;
        static bool _noRandomEncounters;
        static bool _unlockAllDoors;
        static bool _continueFromCurrentRoom;
        static bool _skipPlanetIntros;
        static bool _startWithAllUpgrades;
        static bool _startWithAllOctoliths;
        static bool _walkThroughWalls;
        static bool _alwaysFightGorea2;
        static bool _quadrupleDamage;
    };
}
