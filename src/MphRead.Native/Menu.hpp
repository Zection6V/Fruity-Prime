#pragma once

#include "Formats/Enums.hpp"

#include <cstdint>
#include <string>

namespace MphRead
{
    enum class SaveWhen : std::int32_t;

    enum class SoundCapability : std::int32_t
    {
        None = 0,
        Unsupported = 1,
        Supported = 2
    };

    class MenuSettings
    {
    public:
        std::string RoomKey = "MP3 PROVING GROUND";
        std::string Mode = "auto-select";
        std::string Player1 = "Samus 0";
        std::string Player2 = "none 0";
        std::string Player3 = "none 0";
        std::string Player4 = "none 0";
        std::string Models = "none";
        std::string MphVersion = "AMHE1";
        std::string FhVersion = "AMFE0";
        std::string Language = "English";
        std::string SfxVolume = "0.35";
        std::string MusicVolume = "0.50";
        std::string ResolutionScale = "100";
        std::string Lighting = "on";
        std::string Fog = "on";
        std::string TextureFiltering = "off";
        std::string ShowFps = "off";
        std::string FrameRateCap = "display";
        std::string CelShading = "off";
        std::string CelBands = "8";
        std::string CelEdge = "50";
        std::string PointGoal = "7";
        std::string TimeLimit = "7:00";
        std::string TimeGoal = "1:30";
        std::string AutoReset = "on";
        std::string TeamPlay = "off";
        std::string HunterRadar = "off";
        std::string DamageLevel = "medium";
        std::string FriendlyFire = "off";
        std::string AffinityWeapons = "off";
        std::string ShadowFreeze = "on";
        std::string SaveSlot = "none";
        std::string SaveFromExit = "never";
        std::string SaveFromShip = "prompt";
        std::string Planets = "CA";
        std::string Alinos1State = "none";
        std::string Alinos2State = "none";
        std::string Ca1State = "none";
        std::string Ca2State = "none";
        std::string Vdo1State = "none";
        std::string Vdo2State = "none";
        std::string Arcterra1State = "none";
        std::string Arcterra2State = "none";
        std::string CheckpointId = "none";
        std::string HealthMax = "99";
        std::string MissileMax = "50";
        std::string UaMax = "400";
        std::string Weapons = "PB, MS";
        std::string Octoliths = "none";
    };

    class Menu final
    {
    public:
        Menu() = delete;
        Menu(const Menu&) = delete;
        Menu& operator=(const Menu&) = delete;

        static void ShowMenuPrompts();

        static std::uint8_t SaveSlot;
        static std::int32_t PreviousSaveSlot;
        static SaveWhen SaveFromExit;
        static SaveWhen SaveFromShip;
        static SaveWhen NeededSave;

        static void ApplyMultiplayerSettings();
        static void ApplyAdventureSettings();

    private:
        enum class MusicType : std::int32_t
        {
            Music = 0,
            Seq = 1,
            Stream = 2
        };

        static void PrintSoundInfo(SoundCapability soundCapability);
        static bool ShowSettingsPrompts();
        static bool ShowFeaturePrompts();
        static bool ShowSoundTest(SoundCapability soundCapability);
        static void ResetFeatures();
        static void UpdateSaveInfo();
        static bool ShowLogbook(class StorySave& save);
        static bool ShowStoryModePrompts();
    };
}
