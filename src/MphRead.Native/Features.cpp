#include "Features.hpp"
#include "NativeRuntime/System/Charconv.hpp"

#include <iterator>
#include <span>

#include "Mods/Render/Crosshair.hpp"
#include "NativeRuntime/System/Globalization.hpp"

#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <system_error>

using ::MphRead::NativeRuntime::BooleanTryParse;
using ::MphRead::NativeRuntime::IsNumberWhiteSpace;
using ::MphRead::NativeRuntime::StringEqualsOrdinalIgnoreCase;
using ::MphRead::NativeRuntime::StringTrimView;

namespace MphRead
{
    namespace
    {
        std::string BoolLower(bool value)
        {
            return value ? "true" : "false";
        }

        std::string BoolDefault(bool value)
        {
            return value ? "True" : "False";
        }

        std::string CrosshairStyleToString(Mods::Render::CrosshairStyle value)
        {
            return Mods::Render::ToString(value);
        }

        std::string CrosshairSizeToString(Mods::Render::CrosshairSize value)
        {
            return Mods::Render::ToString(value);
        }
    }

    bool Bugfixes::_smoothCamSeqHandoff = false;
    bool Bugfixes::_betterCamSeqNodeRef = true;
    bool Bugfixes::_noStrayRespawnText = false;
    bool Bugfixes::_correctBountySfx = true;
    bool Bugfixes::_noDoubleEnemyDeath = true;
    bool Bugfixes::_noSlenchRollTimerUnderflow = true;

    bool Bugfixes::SmoothCamSeqHandoff() noexcept
    {
        return _smoothCamSeqHandoff;
    }

    void Bugfixes::SmoothCamSeqHandoff(bool value) noexcept
    {
        _smoothCamSeqHandoff = value;
    }

    bool Bugfixes::BetterCamSeqNodeRef() noexcept
    {
        return _betterCamSeqNodeRef;
    }

    void Bugfixes::BetterCamSeqNodeRef(bool value) noexcept
    {
        _betterCamSeqNodeRef = value;
    }

    bool Bugfixes::NoStrayRespawnText() noexcept
    {
        return _noStrayRespawnText;
    }

    void Bugfixes::NoStrayRespawnText(bool value) noexcept
    {
        _noStrayRespawnText = value;
    }

    bool Bugfixes::CorrectBountySfx() noexcept
    {
        return _correctBountySfx;
    }

    void Bugfixes::CorrectBountySfx(bool value) noexcept
    {
        _correctBountySfx = value;
    }

    bool Bugfixes::NoDoubleEnemyDeath() noexcept
    {
        return _noDoubleEnemyDeath;
    }

    void Bugfixes::NoDoubleEnemyDeath(bool value) noexcept
    {
        _noDoubleEnemyDeath = value;
    }

    bool Bugfixes::NoSlenchRollTimerUnderflow() noexcept
    {
        return _noSlenchRollTimerUnderflow;
    }

    void Bugfixes::NoSlenchRollTimerUnderflow(bool value) noexcept
    {
        _noSlenchRollTimerUnderflow = value;
    }

    void Bugfixes::Load(const std::unordered_map<std::string, std::string>& values)
    {
        bool parsed = false;
        if (const auto it = values.find("SmoothCamSeqHandoff");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            SmoothCamSeqHandoff(parsed);
        }
        if (const auto it = values.find("BetterCamSeqNodeRef");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            BetterCamSeqNodeRef(parsed);
        }
        if (const auto it = values.find("NoStrayRespawnText");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            NoStrayRespawnText(parsed);
        }
        if (const auto it = values.find("CorrectBountySfx");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            CorrectBountySfx(parsed);
        }
        if (const auto it = values.find("NoDoubleEnemyDeath");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            NoDoubleEnemyDeath(parsed);
        }
        if (const auto it = values.find("NoSlenchRollTimerUnderflow");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            NoSlenchRollTimerUnderflow(parsed);
        }
    }

    std::unordered_map<std::string, std::string> Bugfixes::Commit()
    {
        return {
            {"SmoothCamSeqHandoff", BoolLower(SmoothCamSeqHandoff())},
            {"BetterCamSeqNodeRef", BoolLower(BetterCamSeqNodeRef())},
            {"NoStrayRespawnText", BoolLower(NoStrayRespawnText())},
            {"CorrectBountySfx", BoolLower(CorrectBountySfx())},
            {"NoDoubleEnemyDeath", BoolLower(NoDoubleEnemyDeath())},
            {"NoSlenchRollTimerUnderflow", BoolLower(NoSlenchRollTimerUnderflow())}
        };
    }

    bool Features::_noRepeatEncounters = false;
    bool Features::_allowInvalidTeams = true;
    bool Features::_topScreenTargetInfo = true;
    bool Features::_hudSway = true;
    bool Features::_targetInfoSway = false;
    bool Features::_delayedIdleSway = true;
    bool Features::_noIdleSway = false;
    bool Features::_noMapCentering = false;
    bool Features::_maxRoomDetail = false;
    bool Features::_maxPlayerDetail = true;
    bool Features::_logSpatialAudio = false;
    bool Features::_halfSecondAlarm = false;
    bool Features::_fullBoostCharge = false;
    bool Features::_boostOpensDoors = false;
    bool Features::_alternateHunters1P = true;
    bool Features::_halfDamageUnscoped = false;
    bool Features::_proHud = false;
    float Features::_helmetOpacity = 1.0F;
    float Features::_visorOpacity = 0.5F;
    float Features::_hudOpacity = 1.0F;
    float Features::_reticleOpacity = 1.0F;
    bool Features::_fixedCrosshair = false;
    bool Features::_customCrosshair = false;
    bool Features::_modernHud = false;
    float Features::_weaponListScale = 1.0F;
    bool Features::_fixedWeapon = false;
    bool Features::_proHudFixedWeapon = true;

    bool Features::NoRepeatEncounters() noexcept
    {
        return _noRepeatEncounters;
    }

    void Features::NoRepeatEncounters(bool value) noexcept
    {
        _noRepeatEncounters = value;
    }

    bool Features::AllowInvalidTeams() noexcept
    {
        return _allowInvalidTeams;
    }

    void Features::AllowInvalidTeams(bool value) noexcept
    {
        _allowInvalidTeams = value;
    }

    bool Features::TopScreenTargetInfo() noexcept
    {
        return _topScreenTargetInfo;
    }

    void Features::TopScreenTargetInfo(bool value) noexcept
    {
        _topScreenTargetInfo = value;
    }

    bool Features::HudSway() noexcept
    {
        return _hudSway;
    }

    void Features::HudSway(bool value) noexcept
    {
        _hudSway = value;
    }

    bool Features::TargetInfoSway() noexcept
    {
        return _targetInfoSway;
    }

    void Features::TargetInfoSway(bool value) noexcept
    {
        _targetInfoSway = value;
    }

    bool Features::DelayedIdleSway() noexcept
    {
        return _delayedIdleSway;
    }

    void Features::DelayedIdleSway(bool value) noexcept
    {
        _delayedIdleSway = value;
    }

    bool Features::NoIdleSway() noexcept
    {
        return _noIdleSway;
    }

    void Features::NoIdleSway(bool value) noexcept
    {
        _noIdleSway = value;
    }

    bool Features::NoMapCentering() noexcept
    {
        return _noMapCentering;
    }

    void Features::NoMapCentering(bool value) noexcept
    {
        _noMapCentering = value;
    }

    bool Features::MaxRoomDetail() noexcept
    {
        return _maxRoomDetail;
    }

    void Features::MaxRoomDetail(bool value) noexcept
    {
        _maxRoomDetail = value;
    }

    bool Features::MaxPlayerDetail() noexcept
    {
        return _maxPlayerDetail;
    }

    void Features::MaxPlayerDetail(bool value) noexcept
    {
        _maxPlayerDetail = value;
    }

    bool Features::LogSpatialAudio() noexcept
    {
        return _logSpatialAudio;
    }

    void Features::LogSpatialAudio(bool value) noexcept
    {
        _logSpatialAudio = value;
    }

    bool Features::HalfSecondAlarm() noexcept
    {
        return _halfSecondAlarm;
    }

    void Features::HalfSecondAlarm(bool value) noexcept
    {
        _halfSecondAlarm = value;
    }

    bool Features::FullBoostCharge() noexcept
    {
        return _fullBoostCharge;
    }

    void Features::FullBoostCharge(bool value) noexcept
    {
        _fullBoostCharge = value;
    }

    bool Features::BoostOpensDoors() noexcept
    {
        return _boostOpensDoors;
    }

    void Features::BoostOpensDoors(bool value) noexcept
    {
        _boostOpensDoors = value;
    }

    bool Features::AlternateHunters1P() noexcept
    {
        return _alternateHunters1P;
    }

    void Features::AlternateHunters1P(bool value) noexcept
    {
        _alternateHunters1P = value;
    }

    bool Features::HalfDamageUnscoped() noexcept
    {
        return _halfDamageUnscoped;
    }

    void Features::HalfDamageUnscoped(bool value) noexcept
    {
        _halfDamageUnscoped = value;
    }

    bool Features::ProHud() noexcept
    {
        return _proHud;
    }

    void Features::ProHud(bool value) noexcept
    {
        _proHud = value;
    }

    float Features::HelmetOpacity() noexcept
    {
        return ProHud() ? 0.0F : _helmetOpacity;
    }

    void Features::HelmetOpacity(float value) noexcept
    {
        _helmetOpacity = value;
    }

    float Features::VisorOpacity() noexcept
    {
        return ProHud() ? 0.0F : _visorOpacity;
    }

    void Features::VisorOpacity(float value) noexcept
    {
        _visorOpacity = value;
    }

    float Features::HudOpacity() noexcept
    {
        return _hudOpacity;
    }

    void Features::HudOpacity(float value) noexcept
    {
        _hudOpacity = value;
    }

    float Features::ReticleOpacity() noexcept
    {
        return _reticleOpacity;
    }

    void Features::ReticleOpacity(float value) noexcept
    {
        _reticleOpacity = value;
    }

    bool Features::FixedCrosshair() noexcept
    {
        return ProHud() || _fixedCrosshair;
    }

    void Features::FixedCrosshair(bool value) noexcept
    {
        _fixedCrosshair = value;
    }

    bool Features::CustomCrosshair() noexcept
    {
        return ProHud() || _customCrosshair;
    }

    void Features::CustomCrosshair(bool value) noexcept
    {
        _customCrosshair = value;
    }

    bool Features::ModernHud() noexcept
    {
        return ProHud() || _modernHud;
    }

    void Features::ModernHud(bool value) noexcept
    {
        _modernHud = value;
    }

    float Features::WeaponListScale() noexcept
    {
        return ProHud() ? ProHudWeaponListScale : _weaponListScale;
    }

    void Features::WeaponListScale(float value) noexcept
    {
        _weaponListScale = value;
    }

    bool Features::FixedWeapon() noexcept
    {
        return ProHud() ? ProHudFixedWeapon() : _fixedWeapon;
    }

    void Features::FixedWeapon(bool value) noexcept
    {
        _fixedWeapon = value;
    }

    bool Features::ProHudFixedWeapon() noexcept
    {
        return _proHudFixedWeapon;
    }

    void Features::ProHudFixedWeapon(bool value) noexcept
    {
        _proHudFixedWeapon = value;
    }

    void Features::Load(const std::unordered_map<std::string, std::string>& values)
    {
        if (const auto it = values.find("ReticleOpacity"); it != values.end())
        {
            float parsed = 0.0F;
            if (::MphRead::NativeRuntime::SingleTryParseInvariant(it->second, parsed))
            {
                ReticleOpacity(parsed);
            }
        }

        bool parsed = false;
        if (const auto it = values.find("ProHud");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            ProHud(parsed);
        }
        if (const auto it = values.find("ProHudFixedWeapon");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            ProHudFixedWeapon(parsed);
        }
        if (const auto it = values.find("CrosshairStyle"); it != values.end())
        {
            Mods::Render::Crosshair::Style = Mods::Render::Crosshair::ParseStyle(
                std::optional<std::string_view>{std::string_view(it->second)},
                Mods::Render::Crosshair::Style);
        }
        if (const auto it = values.find("CrosshairSize"); it != values.end())
        {
            Mods::Render::Crosshair::Size = Mods::Render::Crosshair::ParseSize(
                std::optional<std::string_view>{std::string_view(it->second)},
                Mods::Render::Crosshair::Size);
        }
    }

    std::unordered_map<std::string, std::string> Features::Commit()
    {
        return {
            {"ReticleOpacity", ::MphRead::NativeRuntime::ToStringInvariant(ReticleOpacity())},
            {"ProHud", BoolLower(ProHud())},
            {"ProHudFixedWeapon", BoolLower(ProHudFixedWeapon())},
            {"CrosshairStyle", CrosshairStyleToString(Mods::Render::Crosshair::Style)},
            {"CrosshairSize", CrosshairSizeToString(Mods::Render::Crosshair::Size)}
        };
    }

    bool Cheats::_freeWeaponSelect = false;
    bool Cheats::_unlimitedJumps = false;
    bool Cheats::_noRandomEncounters = false;
    bool Cheats::_unlockAllDoors = false;
    bool Cheats::_continueFromCurrentRoom = false;
    bool Cheats::_skipPlanetIntros = false;
    bool Cheats::_startWithAllUpgrades = false;
    bool Cheats::_startWithAllOctoliths = false;
    bool Cheats::_walkThroughWalls = false;
    bool Cheats::_alwaysFightGorea2 = false;
    bool Cheats::_quadrupleDamage = false;

    bool Cheats::FreeWeaponSelect() noexcept
    {
        return _freeWeaponSelect;
    }

    void Cheats::FreeWeaponSelect(bool value) noexcept
    {
        _freeWeaponSelect = value;
    }

    bool Cheats::UnlimitedJumps() noexcept
    {
        return _unlimitedJumps;
    }

    void Cheats::UnlimitedJumps(bool value) noexcept
    {
        _unlimitedJumps = value;
    }

    bool Cheats::NoRandomEncounters() noexcept
    {
        return _noRandomEncounters;
    }

    void Cheats::NoRandomEncounters(bool value) noexcept
    {
        _noRandomEncounters = value;
    }

    bool Cheats::UnlockAllDoors() noexcept
    {
        return _unlockAllDoors;
    }

    void Cheats::UnlockAllDoors(bool value) noexcept
    {
        _unlockAllDoors = value;
    }

    bool Cheats::ContinueFromCurrentRoom() noexcept
    {
        return _continueFromCurrentRoom;
    }

    void Cheats::ContinueFromCurrentRoom(bool value) noexcept
    {
        _continueFromCurrentRoom = value;
    }

    bool Cheats::SkipPlanetIntros() noexcept
    {
        return _skipPlanetIntros;
    }

    void Cheats::SkipPlanetIntros(bool value) noexcept
    {
        _skipPlanetIntros = value;
    }

    bool Cheats::StartWithAllUpgrades() noexcept
    {
        return _startWithAllUpgrades;
    }

    void Cheats::StartWithAllUpgrades(bool value) noexcept
    {
        _startWithAllUpgrades = value;
    }

    bool Cheats::StartWithAllOctoliths() noexcept
    {
        return _startWithAllOctoliths;
    }

    void Cheats::StartWithAllOctoliths(bool value) noexcept
    {
        _startWithAllOctoliths = value;
    }

    bool Cheats::WalkThroughWalls() noexcept
    {
        return _walkThroughWalls;
    }

    void Cheats::WalkThroughWalls(bool value) noexcept
    {
        _walkThroughWalls = value;
    }

    bool Cheats::AlwaysFightGorea2() noexcept
    {
        return _alwaysFightGorea2;
    }

    void Cheats::AlwaysFightGorea2(bool value) noexcept
    {
        _alwaysFightGorea2 = value;
    }

    bool Cheats::QuadrupleDamage() noexcept
    {
        return _quadrupleDamage;
    }

    void Cheats::QuadrupleDamage(bool value) noexcept
    {
        _quadrupleDamage = value;
    }

    void Cheats::Load(const std::unordered_map<std::string, std::string>& values)
    {
        bool parsed = false;
        if (const auto it = values.find("FreeWeaponSelect");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            FreeWeaponSelect(parsed);
        }
        if (const auto it = values.find("UnlimitedJumps");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            UnlimitedJumps(parsed);
        }
        if (const auto it = values.find("NoRandomEncounters");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            NoRandomEncounters(parsed);
        }
        if (const auto it = values.find("UnlockAllDoors");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            UnlockAllDoors(parsed);
        }
        if (const auto it = values.find("ContinueFromCurrentRoom");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            ContinueFromCurrentRoom(parsed);
        }
        if (const auto it = values.find("SkipPlanetIntros");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            SkipPlanetIntros(parsed);
        }
        if (const auto it = values.find("StartWithAllUpgrades");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            StartWithAllUpgrades(parsed);
        }
        if (const auto it = values.find("StartWithAllOctoliths");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            StartWithAllOctoliths(parsed);
        }
        if (const auto it = values.find("WalkThroughWalls");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            WalkThroughWalls(parsed);
        }
        if (const auto it = values.find("AlwaysFightGorea2");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            AlwaysFightGorea2(parsed);
        }
        if (const auto it = values.find("QuadrupleDamage");
            it != values.end() && BooleanTryParse(it->second, parsed))
        {
            QuadrupleDamage(parsed);
        }
    }

    std::unordered_map<std::string, std::string> Cheats::Commit()
    {
        return {
            {"FreeWeaponSelect", BoolLower(FreeWeaponSelect())},
            {"UnlimitedJumps", BoolLower(UnlimitedJumps())},
            {"NoRandomEncounters", BoolDefault(NoRandomEncounters())},
            {"UnlockAllDoors", BoolDefault(UnlockAllDoors())},
            {"ContinueFromCurrentRoom", BoolDefault(ContinueFromCurrentRoom())},
            {"SkipPlanetIntros", BoolDefault(SkipPlanetIntros())},
            {"StartWithAllUpgrades", BoolDefault(StartWithAllUpgrades())},
            {"StartWithAllOctoliths", BoolLower(StartWithAllOctoliths())},
            {"WalkThroughWalls", BoolLower(WalkThroughWalls())},
            {"AlwaysFightGorea2", BoolLower(AlwaysFightGorea2())},
            {"QuadrupleDamage", BoolLower(QuadrupleDamage())}
        };
    }
}

namespace MphRead
{
    std::span<const Cheats::BooleanProperty> Cheats::BooleanProperties() noexcept
    {
        // Declaration order, which is the order reflection reports.
        static constexpr BooleanProperty properties[] = {
            {"FreeWeaponSelect", &Cheats::FreeWeaponSelect,
                [](bool value) noexcept { Cheats::FreeWeaponSelect(value); }},
            {"UnlimitedJumps", &Cheats::UnlimitedJumps,
                [](bool value) noexcept { Cheats::UnlimitedJumps(value); }},
            {"NoRandomEncounters", &Cheats::NoRandomEncounters,
                [](bool value) noexcept { Cheats::NoRandomEncounters(value); }},
            {"UnlockAllDoors", &Cheats::UnlockAllDoors,
                [](bool value) noexcept { Cheats::UnlockAllDoors(value); }},
            {"ContinueFromCurrentRoom", &Cheats::ContinueFromCurrentRoom,
                [](bool value) noexcept { Cheats::ContinueFromCurrentRoom(value); }},
            {"SkipPlanetIntros", &Cheats::SkipPlanetIntros,
                [](bool value) noexcept { Cheats::SkipPlanetIntros(value); }},
            {"StartWithAllUpgrades", &Cheats::StartWithAllUpgrades,
                [](bool value) noexcept { Cheats::StartWithAllUpgrades(value); }},
            {"StartWithAllOctoliths", &Cheats::StartWithAllOctoliths,
                [](bool value) noexcept { Cheats::StartWithAllOctoliths(value); }},
            {"WalkThroughWalls", &Cheats::WalkThroughWalls,
                [](bool value) noexcept { Cheats::WalkThroughWalls(value); }},
            {"AlwaysFightGorea2", &Cheats::AlwaysFightGorea2,
                [](bool value) noexcept { Cheats::AlwaysFightGorea2(value); }},
            {"QuadrupleDamage", &Cheats::QuadrupleDamage,
                [](bool value) noexcept { Cheats::QuadrupleDamage(value); }},
        };
        return std::span<const BooleanProperty>(properties, std::size(properties));
    }
}
