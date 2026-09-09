#pragma once

#include "Entities/Players/DynamicLightEntity.hpp"
#include "Entities/Players/PlayerAi.hpp"
#include "Entities/Players/PlayerCamera.hpp"
#include "Entities/Players/player_controls.hpp"
#include "Entities/Players/player_profile.hpp"
#include "Entities/Players/player_state.hpp"
#include "Entities/Players/player_statics.hpp"
#include "Entities/Players/PlayerScan.hpp"
#include "Entities/Players/PlayerSound.hpp"
#include "Entities/gameplay.hpp"
#include "GameState.hpp"
#include "Metadata/player_values.hpp"
#include "Strings.hpp"
#include "Mods/Chat/PlayerEntityChatHud.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace fruityprime::runtime {
class HalfturretEntity;
}

namespace fruityprime::players {

// The owning type shared by every PlayerEntity.cs partial. The native
// deterministic Session remains the simulation engine; this class restores
// the managed per-slot identity, process-wide player table and public player
// API over the same live state instead of leaving unrelated adapter objects.
class PlayerEntity final : public DynamicLightEntityBase {
public:
    ~PlayerEntity();
    static constexpr int SlotCapacity = 8;
    static constexpr std::uint16_t RespawnTime = 90 * 2;
    static inline constexpr auto& ScanIds = PlayerScan::ScanIds;

    static void Construct(gameplay::Session& session,
                          const game::State* game_state = nullptr);
    static void Reset() noexcept;
    [[nodiscard]] static PlayerEntity* Create(metadata::Hunter hunter,
                                              int recolor);
    static void SessionPlayerAdded(gameplay::Session& session,
                                   std::uint8_t slot,
                                   std::uint8_t hunter) noexcept;
    static void SessionPlayerRemoved(gameplay::Session& session,
                                     std::uint8_t slot) noexcept;
    static void SessionWeaponChanged(gameplay::Session& session,
                                     std::uint8_t slot,
                                     std::uint8_t previous_weapon,
                                     std::uint8_t current_weapon) noexcept;
    static void SessionMovementChanged(gameplay::Session& session,
                                       std::uint8_t slot,
                                       net::Vec3 previous_position,
                                       net::Vec3 previous_speed) noexcept;
    static void SessionPlayerTick(gameplay::Session& session,
                                  std::uint8_t slot) noexcept;
    static void SessionShotFired(gameplay::Session& session,
                                 std::uint8_t slot) noexcept;

    [[nodiscard]] static int MainPlayerIndex() noexcept;
    static void MainPlayerIndex(int value) noexcept;
    [[nodiscard]] static int PlayerCount() noexcept;
    static void PlayerCount(int value) noexcept;
    [[nodiscard]] static int MaxPlayers() noexcept;
    static void MaxPlayers(int value) noexcept;
    [[nodiscard]] static int PlayersCreated() noexcept;
    [[nodiscard]] static PlayerEntity* Main() noexcept;
    [[nodiscard]] static std::span<PlayerEntity* const> Players() noexcept;

    [[nodiscard]] int SlotIndex() const noexcept { return slot_index_; }
    [[nodiscard]] metadata::Hunter Hunter() const noexcept { return hunter_; }
    [[nodiscard]] int Recolor() const noexcept { return recolor_; }
    [[nodiscard]] bool IsBot() const noexcept { return is_bot_; }
    void IsBot(bool value) noexcept { is_bot_ = value; }
    [[nodiscard]] formats::LoadFlags LoadFlags() const noexcept {
        return load_flags_;
    }
    void LoadFlags(formats::LoadFlags value) noexcept { load_flags_ = value; }
    [[nodiscard]] bool IsMainPlayer() const noexcept;
    [[nodiscard]] bool IsPrimeHunter() const noexcept;

    [[nodiscard]] net::PlayerState& State();
    [[nodiscard]] const net::PlayerState& State() const;
    [[nodiscard]] gameplay::InventoryState& Inventory();
    [[nodiscard]] const gameplay::InventoryState& Inventory() const;
    [[nodiscard]] PlayerRuntimeState& RuntimeState() noexcept {
        return runtime_state_;
    }
    [[nodiscard]] const entities::PlayerValues& Values() const noexcept;
    [[nodiscard]] PlayerControls& Controls() noexcept { return controls_; }
    [[nodiscard]] const PlayerControls& Controls() const noexcept {
        return controls_;
    }
    [[nodiscard]] PlayerAiData& AiData() noexcept { return ai_data_; }
    [[nodiscard]] const PlayerAiData& AiData() const noexcept {
        return ai_data_;
    }
    [[nodiscard]] int BotLevel() const noexcept { return bot_level_; }
    void BotLevel(int value) noexcept;
    [[nodiscard]] PlayerCamera& Camera() noexcept { return camera_; }
    [[nodiscard]] const PlayerCamera& Camera() const noexcept {
        return camera_;
    }
    [[nodiscard]] PlayerScan& ScanState() noexcept { return scan_state_; }
    [[nodiscard]] const PlayerScan& ScanState() const noexcept {
        return scan_state_;
    }
    [[nodiscard]] PlayerSoundState& SoundState() noexcept {
        return sound_state_;
    }
    [[nodiscard]] const PlayerSoundState& SoundState() const noexcept {
        return sound_state_;
    }
    [[nodiscard]] fruityprime::chat::player_entity_chat_hud::State&
        ChatHudState() noexcept {
        return chat_hud_;
    }
    [[nodiscard]] const fruityprime::chat::player_entity_chat_hud::State&
        ChatHudState() const noexcept {
        return chat_hud_;
    }
    [[nodiscard]] runtime::HalfturretEntity& Halfturret() noexcept;
    [[nodiscard]] const runtime::HalfturretEntity& Halfturret() const noexcept;
    void CreateHalfturret();
    [[nodiscard]] bool HasHalfturret() const noexcept;

    [[nodiscard]] int Health() const;
    void Health(int value);
    [[nodiscard]] int HealthMax() const;
    [[nodiscard]] AvailableArray& AvailableWeapons();
    [[nodiscard]] formats::BeamType CurrentWeapon() const;
    [[nodiscard]] formats::BeamType PreviousWeapon() const noexcept {
        return runtime_state_.PreviousWeapon;
    }
    void PreviousWeapon(formats::BeamType value) noexcept {
        runtime_state_.PreviousWeapon = value;
    }
    [[nodiscard]] formats::BeamType WeaponSelection() const noexcept {
        return runtime_state_.WeaponSelection;
    }
    void WeaponSelection(formats::BeamType value) noexcept {
        runtime_state_.WeaponSelection = value;
    }
    [[nodiscard]] formats::GunAnimation GunAnimation() const noexcept {
        return runtime_state_.GunAnimation;
    }
    void GunAnimation(formats::GunAnimation value) noexcept {
        runtime_state_.GunAnimation = value;
    }
    [[nodiscard]] formats::Team Team() const;
    void Team(formats::Team value);
    [[nodiscard]] int TeamIndex() const;
    void TeamIndex(int value);
    [[nodiscard]] formats::PlayerFlags1 Flags1() const noexcept;
    void Flags1(formats::PlayerFlags1 value) noexcept {
        flags1_ = value;
        runtime_state_.Flags1 = value;
    }
    [[nodiscard]] formats::PlayerFlags2 Flags2() const noexcept;
    void Flags2(formats::PlayerFlags2 value) noexcept {
        runtime_state_.Flags2 = value;
    }
    [[nodiscard]] bool SwipeBoostRequested() const noexcept {
        return runtime_state_.SwipeBoostRequested;
    }
    void SwipeBoostRequested(bool value) noexcept {
        runtime_state_.SwipeBoostRequested = value;
    }
    [[nodiscard]] float SwipeBoostX() const noexcept {
        return runtime_state_.SwipeBoostX;
    }
    void SwipeBoostX(float value) noexcept { runtime_state_.SwipeBoostX = value; }
    [[nodiscard]] float SwipeBoostY() const noexcept {
        return runtime_state_.SwipeBoostY;
    }
    void SwipeBoostY(float value) noexcept { runtime_state_.SwipeBoostY = value; }
    [[nodiscard]] std::uint8_t SyluxBombCount() const noexcept {
        return runtime_state_.SyluxBombCount;
    }
    void SyluxBombCount(std::uint8_t value) noexcept {
        runtime_state_.SyluxBombCount = value;
    }
    [[nodiscard]] bool IgnoreItemPickups() const noexcept {
        return runtime_state_.IgnoreItemPickups;
    }
    void IgnoreItemPickups(bool value) noexcept {
        runtime_state_.IgnoreItemPickups = value;
    }
    [[nodiscard]] bool ReloadInit() const noexcept {
        return runtime_state_.ReloadInit;
    }
    void ReloadInit(bool value) noexcept { runtime_state_.ReloadInit = value; }
    [[nodiscard]] bool IsAltForm() const noexcept;
    [[nodiscard]] bool IsMorphing() const noexcept;
    [[nodiscard]] bool IsUnmorphing() const noexcept;
    [[nodiscard]] net::Vec3 Position() const;
    [[nodiscard]] net::Vec3 FacingVector() const;
    [[nodiscard]] net::Vec3 UpVector() const noexcept { return up_vector_; }
    [[nodiscard]] net::Vec3 Speed() const;
    void Speed(net::Vec3 value);
    [[nodiscard]] net::Vec3 Acceleration() const noexcept;
    void Acceleration(net::Vec3 value) noexcept;
    [[nodiscard]] net::Vec3 PrevSpeed() const noexcept;
    void PrevSpeed(net::Vec3 value) noexcept;
    [[nodiscard]] net::Vec3 PrevPosition() const noexcept;
    void PrevPosition(net::Vec3 value) noexcept;
    [[nodiscard]] net::Vec3 IdlePosition() const noexcept;
    [[nodiscard]] std::uint16_t TimeSinceShot() const noexcept;
    void TimeSinceShot(std::uint16_t value) noexcept;
    [[nodiscard]] std::uint16_t RespawnTimer() const noexcept;
    void RespawnTimer(std::uint16_t value) noexcept;
    [[nodiscard]] float DeathCountdown() const noexcept {
        return runtime_state_.DeathCountdown;
    }
    [[nodiscard]] bool DoubleDamage() const noexcept;
    [[nodiscard]] std::uint16_t ShockCoilTimer() const noexcept {
        return runtime_state_.ShockCoilTimer;
    }
    [[nodiscard]] float CurAlpha() const noexcept {
        return runtime_state_.CurAlpha;
    }
    [[nodiscard]] const formats::CollisionVolume& Volume() const noexcept {
        return runtime_state_.Volume;
    }
    [[nodiscard]] float Field70() const noexcept {
        return runtime_state_.Field70;
    }
    [[nodiscard]] float Field74() const noexcept {
        return runtime_state_.Field74;
    }
    [[nodiscard]] bool Field6D0() const noexcept {
        return runtime_state_.Field6D0;
    }
    [[nodiscard]] std::optional<net::Vec3> ForcedSpawnPos() const noexcept {
        return forced_spawn_pos_;
    }
    void ForcedSpawnPos(std::optional<net::Vec3> value) noexcept {
        forced_spawn_pos_ = value;
    }

    void Spawn(net::Vec3 position, net::Vec3 facing, net::Vec3 up,
               bool respawn);
    void Teleport(net::Vec3 position, net::Vec3 facing);
    void Reposition(net::Vec3 position, net::Vec3 facing);
    void Reposition(net::Vec3 offset);
    void BlockFormSwitch();
    void SetCombatVisor() noexcept;
    void ResetCombatVisor() noexcept;
    void StartFlagCarrySfx() noexcept;
    void StopFlagCarrySfx() noexcept;
    void StopAllSfx() noexcept;
    void StopTimedSfx() noexcept;
    void RestartTimedSfx(bool force = false) noexcept;
    void StopLongSfx() noexcept;
    void RestartLongSfx(bool force = false) noexcept;
    [[nodiscard]] PlayerSoundState::TimedSoundUpdate UpdateTimedSounds(
        float frame_seconds, bool camera_blocks_input = false) noexcept;
    void GainHealth(std::uint32_t health);
    void GainHealth(std::int32_t health);
    void ExitAltForm();
    void OnHalfturretDied();
    [[nodiscard]] bool GetTargetable() const;
    [[nodiscard]] int GetScanId(bool alternate = false) const noexcept;
    [[nodiscard]] bool ScanVisible() const;
    void UpdateZoom(bool zoom);
    void TakeDamage(std::int32_t damage, formats::DamageFlags flags,
                    const net::Vec3* direction = nullptr,
                    PlayerEntity* source = nullptr);
    void TakeDamage(std::uint32_t damage, formats::DamageFlags flags,
                    const net::Vec3* direction = nullptr,
                    PlayerEntity* source = nullptr);
    void SaveStatus(game::StorySave& save, bool fade_active) const;
    void ResetReferences() noexcept;
    void ModForgetInputDeltas() noexcept;

    static void LoadWeaponNames() noexcept;
    static void WeaponNameTable(std::vector<strings::TableEntry> entries);
    [[nodiscard]] static const std::array<std::string, 9>& WeaponNames()
        noexcept;
    [[nodiscard]] static const std::array<std::string, 8>& HunterNames()
        noexcept;
    [[nodiscard]] static const std::array<std::string, 8>& AltAttackNames()
        noexcept;
    static void GeneratePlayerVolumes() noexcept;
    [[nodiscard]] static const formats::CollisionVolume& PlayerVolume(
        std::size_t hunter, PlayerStatics::Volume which) noexcept;

private:
    // Private nested PlayerInput from PlayerInput.cs. Native window state is
    // frontend-owned, so snapshots are opaque while its observable state and
    // nullable-snapshot delta behavior remain identical.
    struct PlayerInput {
        const void* PrevKeyboardState = nullptr;
        const void* KeyboardState = nullptr;
        const void* PrevMouseState = nullptr;
        const void* MouseState = nullptr;
        float PrevMouseX = 0.0F;
        float PrevMouseY = 0.0F;
        float MouseX = 0.0F;
        float MouseY = 0.0F;
        float ClickX = -1.0F;
        float ClickY = -1.0F;
        bool HasInput = false;

        [[nodiscard]] float MouseDeltaX() const noexcept {
            return MouseState != nullptr && PrevMouseState != nullptr
                ? MouseX - PrevMouseX : 0.0F;
        }
        [[nodiscard]] float MouseDeltaY() const noexcept {
            return MouseState != nullptr && PrevMouseState != nullptr
                ? MouseY - PrevMouseY : 0.0F;
        }
    };

    explicit PlayerEntity(int slot_index, gameplay::Session& session);
    void assign(metadata::Hunter hunter, int recolor) noexcept;
    [[nodiscard]] bool has_live_state() const noexcept;

    gameplay::Session* session_ = nullptr;
    int slot_index_ = 0;
    metadata::Hunter hunter_ = metadata::Hunter::Samus;
    int recolor_ = 0;
    bool is_bot_ = false;
    formats::LoadFlags load_flags_ = formats::LoadFlags::None;
    formats::PlayerFlags1 flags1_{};
    PlayerControls controls_ = PlayerControls::GetDefault();
    PlayerInput input_{};
    PlayerAiData ai_data_{};
    int bot_level_ = 0;
    PlayerCamera camera_{};
    PlayerScan scan_state_{};
    PlayerSoundState sound_state_{};
    fruityprime::chat::player_entity_chat_hud::State chat_hud_{};
    net::Vec3 up_vector_{0.0F, 1.0F, 0.0F};
    std::optional<net::Vec3> forced_spawn_pos_{};
    std::unique_ptr<runtime::HalfturretEntity> halfturret_;
    AvailableArray available_weapons_;
    PlayerRuntimeState runtime_state_;

    inline static gameplay::Session* bound_session_ = nullptr;
    inline static const game::State* bound_game_state_ = nullptr;
    inline static int main_player_index_ = 0;
    inline static int player_count_ = 0;
    inline static int max_players_ = 4;
    inline static int players_created_ = 0;
    inline static std::array<std::unique_ptr<PlayerEntity>, SlotCapacity>
        storage_{};
    inline static std::array<PlayerEntity*, SlotCapacity> players_{};
    inline static std::vector<strings::TableEntry> weapon_name_table_{};
    inline static std::array<std::string, 9> weapon_names_{};
    inline static std::array<std::string, 8> hunter_names_{};
    inline static std::array<std::string, 8> alt_attack_names_{};
    inline static PlayerSoundState::MuteState sfx_mute_state_{};
    inline static PlayerStatics statics_{};
};

} // namespace fruityprime::players

namespace MphReadNative::Entities {
using PlayerEntity = ::fruityprime::players::PlayerEntity;
}
