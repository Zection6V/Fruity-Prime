#pragma once

#include "../../Formats/Enums.hpp"
#include "../../Metadata/SoundMeta.hpp"
#include "../../Sound/Sfx.hpp"

#include <array>
#include <cstdint>

#define MPHREAD_PLAYER_SOUND_MEMBERS                                                        \
private:                                                                                    \
    void PlayHunterSfx(::MphRead::HunterSfx sfx);                                           \
    [[nodiscard]] std::int32_t PlayMissileSfx(::MphRead::HunterSfx sfx);                    \
    float _damageSfxTimer = 0.0F;                                                           \
    void PlayRandomDamageSfx();                                                             \
    void PlayBeamEmptySfx(::MphRead::BeamType beam);                                        \
    void PlayBeamShotSfx(::MphRead::BeamType beam, bool charged, bool continuous,           \
        bool homing, float amountA);                                                        \
    [[nodiscard]] std::int32_t GetBeamChargeSfx(::MphRead::BeamType beam);                  \
    void PlayBeamChargeSfx(::MphRead::BeamType beam);                                       \
    void StopBeamChargeSfx(::MphRead::BeamType beam);                                       \
public:                                                                                     \
    void StopContinuousBeamSfx(::MphRead::BeamType beam);                                   \
private:                                                                                    \
    std::int32_t _healthSfxHandle = -1;                                                     \
    void UpdateHealthSfx(std::int32_t health);                                              \
    void UpdateWalkingSfx();                                                                \
    [[nodiscard]] std::int32_t GetAltMovementSfx();                                         \
    void UpdateAltMovementSfx();                                                            \
    void UpdateSlidingSfx(float newAmount);                                                 \
    void UpdateMovementSfxAmount(float newAmount);                                          \
    void StopTerrainSfx(::MphRead::Terrain prevTerrain);                                    \
    void StopAltFormSfx();                                                                  \
    void PlayLandingSfx();                                                                  \
    void UpdateBurningSfx(bool burning);                                                    \
    inline static const std::array<::MphRead::SfxId, 3> _dblDamageIds{                      \
        ::MphRead::SfxId::DBL_DAMAGE_A, ::MphRead::SfxId::DBL_DAMAGE_B,                    \
        ::MphRead::SfxId::DBL_DAMAGE_C};                                                    \
public:                                                                                     \
    bool _dblDamageSfxMuted = false;                                                        \
private:                                                                                    \
    std::int32_t _dblDamageSfxHandle = -1;                                                 \
    ::MphRead::SfxId _dblDamageSfxId = ::MphRead::SfxId::None;                             \
    void UpdateDoubleDamageSfx(std::int32_t index, bool play);                              \
    inline static const std::array<::MphRead::SfxId, 3> _cloakSfxIds{                      \
        ::MphRead::SfxId::CLOAK_A, ::MphRead::SfxId::CLOAK_B,                             \
        ::MphRead::SfxId::CLOAK_C};                                                        \
    bool _cloakSfxMuted = false;                                                            \
    std::int32_t _cloakSfxHandle = -1;                                                     \
    ::MphRead::SfxId _cloakSfxId = ::MphRead::SfxId::None;                                \
    void UpdateCloakSfx(std::int32_t index, bool play);                                    \
    bool _flagCarrySfxOn = false;                                                           \
    bool _flagCarrySfxMuted = false;                                                        \
    std::int32_t _flagCarrySfxHandle = -1;                                                 \
public:                                                                                     \
    void StartFlagCarrySfx();                                                               \
    void StopFlagCarrySfx();                                                                \
private:                                                                                    \
    ::MphRead::Sound::SoundSource _timedSfxSource{};                                        \
    float _sfxStopTimer = 0.0F;                                                            \
public:                                                                                     \
    float ForceFieldSfxTimer = 0.0F;                                                       \
    float DoorUnlockSfxTimer = 0.0F;                                                       \
    float DoorChimeSfxTimer = 0.0F;                                                        \
private:                                                                                    \
    std::array<bool, 3> _scanSfxOn{};                                                      \
    std::array<std::int32_t, 3> _scanSfxHandles{-1, -1, -1};                              \
    inline static const std::array<::MphRead::SfxId, 3> _scanSfxIds{                       \
        ::MphRead::SfxId::SCAN_VISOR_ON, ::MphRead::SfxId::SCAN_STATUS_BAR,               \
        ::MphRead::SfxId::SCAN_VISOR_LOOP};                                                \
    void UpdateScanSfx(std::int32_t index, bool enable);                                   \
public:                                                                                     \
    void StopAllSfx();                                                                      \
    void PlayTimedSfx(::MphRead::SfxId id);                                                \
    void StopTimedSfx(::MphRead::SfxId id);                                                \
    void StopTimedSfx();                                                                    \
    void RestartTimedSfx(bool force = false);                                              \
    void StopLongSfx();                                                                     \
    void RestartLongSfx(bool force = false);                                               \
private:                                                                                    \
    float _scrollSfxTimer = 0.0F;                                                          \
public:                                                                                     \
    void UpdateTimedSounds();                                                               \
private:                                                                                    \
    [[nodiscard]] ::MphRead::MusicId CheckMusicUpdate(                                     \
        ::MphRead::MusicId currentMusicId, std::int32_t param1, std::int32_t param2);
