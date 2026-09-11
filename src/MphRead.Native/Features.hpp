#pragma once

#include <string>
#include <unordered_map>

namespace MphRead
{
    class Bugfixes final
    {
    public:
        Bugfixes() = delete;

        static bool SmoothCamSeqHandoff;
        static bool BetterCamSeqNodeRef;
        static bool NoStrayRespawnText;
        static bool CorrectBountySfx;
        static bool NoDoubleEnemyDeath;
        static bool NoSlenchRollTimerUnderflow;

        static void Load(const std::unordered_map<std::string, std::string>& values);
        static std::unordered_map<std::string, std::string> Commit();
    };

    class Features final
    {
    public:
        Features() = delete;

        static bool NoRepeatEncounters;
        static bool AllowInvalidTeams;
        static bool TopScreenTargetInfo;
        static bool HudSway;
        static bool TargetInfoSway;
        static bool DelayedIdleSway;
        static bool NoIdleSway;
        static bool NoMapCentering;
        static bool MaxRoomDetail;
        static bool MaxPlayerDetail;
        static bool LogSpatialAudio;
        static bool HalfSecondAlarm;
        static bool FullBoostCharge;
        static bool BoostOpensDoors;
        static bool AlternateHunters1P;
        static bool HalfDamageUnscoped;

        static bool ProHud;
        static constexpr float ProHudWeaponListScale = 1.7F;

        static float HelmetOpacity();
        static void SetHelmetOpacity(float value);
        static float VisorOpacity();
        static void SetVisorOpacity(float value);

        static float HudOpacity;
        static float ReticleOpacity;

        static bool FixedCrosshair();
        static void SetFixedCrosshair(bool value);
        static bool CustomCrosshair();
        static void SetCustomCrosshair(bool value);
        static bool ModernHud();
        static void SetModernHud(bool value);
        static float WeaponListScale();
        static void SetWeaponListScale(float value);
        static bool FixedWeapon();
        static void SetFixedWeapon(bool value);

        static bool ProHudFixedWeapon;

        static void Load(const std::unordered_map<std::string, std::string>& values);
        static std::unordered_map<std::string, std::string> Commit();

    private:
        static float helmetOpacity_;
        static float visorOpacity_;
        static bool fixedCrosshair_;
        static bool customCrosshair_;
        static bool modernHud_;
        static float weaponListScale_;
        static bool fixedWeapon_;
    };

    class Cheats final
    {
    public:
        Cheats() = delete;

        static bool FreeWeaponSelect;
        static bool UnlimitedJumps;
        static bool NoRandomEncounters;
        static bool UnlockAllDoors;
        static bool ContinueFromCurrentRoom;
        static bool SkipPlanetIntros;
        static bool StartWithAllUpgrades;
        static bool StartWithAllOctoliths;
        static bool WalkThroughWalls;
        static bool AlwaysFightGorea2;
        static bool QuadrupleDamage;

        static void Load(const std::unordered_map<std::string, std::string>& values);
        static std::unordered_map<std::string, std::string> Commit();
    };
}
