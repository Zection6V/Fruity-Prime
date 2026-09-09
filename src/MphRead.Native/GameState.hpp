#pragma once

#include "Messaging.hpp"
#include "Mods/Network/net_protocol.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace fruityprime::game {

constexpr std::size_t SlotCapacity = 8;

enum class MatchState : std::uint8_t {
    InProgress,
    GameOver,
    Ending,
    Disconnected
};

enum class TransitionState : std::uint8_t {
    None,
    Start,
    Process,
    End,
    // Names used by the older native scene loader.  They intentionally share
    // the managed GameState values above so either call site can be migrated
    // without translating the state on the boundary.
    Loading = Start,
    Entering = Process,
    Leaving = End
};

enum class EscapeState : std::uint8_t {
    None,
    Event,
    Escape,
    Ending,
    // Compatibility names for the first native scene implementation.
    Starting = Event,
    Active = Escape
};

enum class Mode : std::uint8_t {
    None = 0,
    Story = 2,
    SinglePlayer = 2,
    Battle = 3,
    BattleTeams = 4,
    Survival = 5,
    SurvivalTeams = 6,
    Capture = 7,
    Bounty = 8,
    BountyTeams = 9,
    Nodes = 10,
    NodesTeams = 11,
    Defender = 12,
    DefenderTeams = 13,
    PrimeHunter = 14,
    Unknown15 = 15
};

enum class BossFlags : std::uint32_t {
    None = 0x00000,
    Unit1B1Kill = 0x00001,
    Unit1B1Done = 0x00002,
    Unit1B2Kill = 0x00004,
    Unit1B2Done = 0x00008,
    Unit2B1Kill = 0x00010,
    Unit2B1Done = 0x00020,
    Unit2B2Kill = 0x00040,
    Unit2B2Done = 0x00080,
    Unit3B1Kill = 0x00100,
    Unit3B1Done = 0x00200,
    Unit3B2Kill = 0x00400,
    Unit3B2Done = 0x00800,
    Unit4B1Kill = 0x01000,
    Unit4B1Done = 0x02000,
    Unit4B2Kill = 0x04000,
    Unit4B2Done = 0x08000,
    Gorea1Kill = 0x10000,
    All = 0x55555
};

enum class ModeStateKind : std::uint8_t {
    Adventure,
    Battle,
    Survival,
    Capture,
    Bounty,
    Defender,
    Nodes,
    PrimeHunter
};

enum class AreaState : std::uint8_t {
    None,
    Escape,
    Clear
};

class StorySave {
public:
    static constexpr std::size_t RoomCount = 66;
    static constexpr std::size_t RoomStateBytes = 60;
    static constexpr std::size_t LogbookBytes = 68;
    static constexpr std::size_t ScanCount = LogbookBytes * 8;

    struct SaveStats {
        std::uint32_t hunter_kills = 0;
        std::uint32_t deaths = 0;
        std::uint32_t enemy_hunter_deaths = 0;
        std::uint32_t enemy_kills = 0;
    };

    StorySave() noexcept;

    std::array<std::array<std::uint8_t, RoomStateBytes>, RoomCount> room_state{};
    std::array<std::uint8_t, 9> visited_rooms{};
    std::array<std::int32_t, 9> visited_connectors{};
    std::array<std::uint8_t, 4> trigger_state{};
    std::array<std::uint8_t, LogbookBytes> logbook{};
    std::array<std::array<std::uint8_t, 8>, 8> enemy_encounters{};
    std::array<char, ScanCount> scan_categories{};
    std::int32_t scan_count = 0;
    std::int32_t equipment_count = 0;
    std::int32_t checkpoint_entity_id = -1;
    std::int32_t checkpoint_room_id = -1;
    std::int32_t health = 99;
    std::int32_t health_max = 99;
    std::array<std::int32_t, 2> ammo{400, 0};
    std::array<std::int32_t, 2> ammo_max{400, 50};
    // These fields are serialized with the cartridge BeamType ordinals from
    // Formats.Enums.cs: Power=0, Volt=1, Missile=2.  Gameplay converts them
    // to its native weapon-table slots at the session boundary.
    std::array<std::int32_t, 3> weapon_slots{0, 2, -1};
    std::uint16_t weapons = 0x0005;
    std::uint32_t artifacts = 0;
    std::uint16_t found_octoliths = 0;
    std::uint16_t current_octoliths = 0;
    std::uint32_t lost_octoliths = 0xffffffffU;
    std::uint16_t areas = 0x000c;
    std::uint32_t boss_flags = 0;
    std::array<std::uint8_t, 4> area_hunters{};
    std::uint8_t defeated_hunters = 0;
    SaveStats stats{};

    [[nodiscard]] std::int32_t init_room_state(
        std::int32_t room_id, std::int32_t entity_id, bool active,
        std::int32_t active_state = 3,
        std::int32_t inactive_state = 1) noexcept;
    [[nodiscard]] std::int32_t room_state_value(
        std::int32_t room_id, std::int32_t entity_id) const noexcept;
    void set_room_state(std::int32_t room_id, std::int32_t entity_id,
                        std::int32_t state) noexcept;
    [[nodiscard]] bool visited_room(std::int32_t room_id) const noexcept;
    void set_visited_room(std::int32_t room_id) noexcept;
    [[nodiscard]] bool visited_connector(std::int32_t connector_id,
                                         std::int32_t area_id) const noexcept;
    void set_visited_connector(std::int32_t connector_id,
                               std::int32_t area_id) noexcept;
    [[nodiscard]] bool found_octolith(std::int32_t area_id) const noexcept;
    [[nodiscard]] std::int32_t found_octolith_count() const noexcept;
    void update_found_octolith(std::int32_t area_id) noexcept;
    [[nodiscard]] bool found_artifact(std::int32_t artifact_id,
                                      std::int32_t model_id) const noexcept;
    [[nodiscard]] std::int32_t found_artifact_count(
        std::int32_t model_id) const noexcept;
    void update_found_artifact(std::int32_t artifact_id,
                               std::int32_t model_id) noexcept;
    [[nodiscard]] std::int32_t enemy_octolith_drop(
        std::int32_t hunter) const noexcept;
    void set_scan_category(std::int32_t scan_id, char category) noexcept;
    void update_logbook(std::int32_t scan_id) noexcept;
    [[nodiscard]] bool logbook_found(std::int32_t scan_id) const noexcept;
    [[nodiscard]] std::int32_t logbook_count(
        bool unlocked_only, std::span<const char> categories = {}) const noexcept;
    [[nodiscard]] std::int32_t max_scan_count() const noexcept;
    [[nodiscard]] std::int32_t completion_percentage() const noexcept;
    void copy_to(StorySave& other) const noexcept { other = *this; }
};

// Process-wide state that the managed GameState.cs exposes to scene, menu,
// HUD, and message code. The native implementation keeps ownership explicit
// and leaves large cartridge save structures in the asset/save layer.
struct State {
    static constexpr std::size_t TriggerBytes = 256;

    State() noexcept;

    bool single_player = true;
    bool first_hunt = false;
    bool paused = false;
    bool pause_prevented = false;
    bool menu_pause = false;
    bool dialog_pause = false;
    // GameState.cs applies dialog pause requests at the next explicit
    // ApplyPause call.  Keep the requests separate from the public state so
    // movie/dialog code observes the same ordering as the managed runtime.
    bool pausing_dialog = false;
    bool unpausing_dialog = false;
    bool quit_requested = false;
    bool loading = false;
    Mode mode = Mode::Story;
    ModeStateKind mode_state = ModeStateKind::Adventure;
    MatchState match_state = MatchState::InProgress;
    TransitionState transition_state = TransitionState::None;
    EscapeState escape_state = EscapeState::None;
    float escape_timer = -1.0F;
    bool escape_paused = false;
    std::array<std::int32_t, SlotCapacity> encounter_state{};
    std::array<bool, StorySave::RoomCount> completed_random_encounter_rooms{};
    std::int32_t transition_room_id = -1;
    bool transition_alt_form = false;
    std::uint64_t frame_count = 0;
    std::uint32_t room_id = 0;
    std::string room_name;
    std::uint8_t language = 0;
    std::uint8_t local_slot = 0;
    std::uint8_t player_count = 1;
    std::uint8_t active_players = 0;
    std::array<std::string, SlotCapacity> nicknames{};
    std::array<std::int32_t, SlotCapacity> stars{};
    std::array<std::int32_t, SlotCapacity> team_standings{};
    std::array<std::int32_t, SlotCapacity> result_slots{};
    std::int32_t prime_hunter = -1;
    float match_time = -1.0F;
    bool match_clock_enabled = false;
    std::uint16_t point_goal = 0;
    float time_goal = 0.0F;
    bool teams = false;
    bool friendly_fire = false;
    std::int32_t damage_level = 1;
    bool octolith_reset = false;
    bool radar_players = false;
    bool affinity_weapons = false;
    bool force_end_game = false;
    std::array<std::int32_t, SlotCapacity> points{};
    std::array<std::int32_t, SlotCapacity> team_points{};
    std::array<std::int32_t, SlotCapacity> kills{};
    std::array<std::int32_t, SlotCapacity> team_kills{};
    std::array<std::int32_t, SlotCapacity> deaths{};
    std::array<std::int32_t, SlotCapacity> team_deaths{};
    std::array<float, SlotCapacity> player_time{};
    std::array<float, SlotCapacity> team_time{};
    std::array<std::int32_t, SlotCapacity> standings{};
    std::array<std::int32_t, SlotCapacity> beam_damage_max{};
    std::array<std::int32_t, SlotCapacity> beam_damage_dealt{};
    std::array<std::int32_t, SlotCapacity> damage_count{};
    std::array<std::int32_t, SlotCapacity> alt_damage_count{};
    std::array<std::int32_t, SlotCapacity> kill_streak{};
    std::array<std::int32_t, SlotCapacity> suicides{};
    std::array<std::int32_t, SlotCapacity> friendly_kills{};
    std::array<std::int32_t, SlotCapacity> headshot_kills{};
    std::array<std::array<std::int32_t, 9>, SlotCapacity> beam_kills{};
    std::array<std::int32_t, SlotCapacity> octolith_scores{};
    std::array<std::int32_t, SlotCapacity> octolith_drops{};
    std::array<std::int32_t, SlotCapacity> octolith_stops{};
    std::array<std::int32_t, SlotCapacity> nodes_captured{};
    std::array<std::int32_t, SlotCapacity> nodes_lost{};
    std::array<std::int32_t, SlotCapacity> kills_as_prime{};
    std::array<std::int32_t, SlotCapacity> primes_killed{};
    std::int32_t queued_octolith_message_id = -1;
    bool queued_oubliette_unlock_message = false;
    std::array<std::uint8_t, TriggerBytes> trigger_state{};
    // TeamIndex is part of the player entity in the managed runtime and is
    // not carried by the public GameState arrays.  Retain it at this native
    // boundary so result ordering can be rebuilt from a wire snapshot.
    std::array<std::uint8_t, SlotCapacity> player_teams{};
    StorySave story_save;

    void reset() noexcept;
    void begin_room(std::string_view name, std::uint32_t id,
                    bool multiplayer, Mode game_mode);
    void tick() noexcept;
    void pause_menu() noexcept;
    void unpause_menu() noexcept;
    void pause_dialog() noexcept;
    void unpause_dialog() noexcept;
    void apply_pause() noexcept;
    void reset_match_progress() noexcept;
    // Copy the authoritative fixed-step player counters into the same
    // process-wide arrays consumed by the managed GameState/PlayerHud
    // boundary. Renderers and result screens do not need to know about
    // gameplay::Session.
    void sync_players(std::span<const net::PlayerState> players) noexcept;
    // Rebuild ResultSlots, Standings, and TeamStandings using the current
    // mode's comparison rules. A server or replay frontend can call this
    // after applying a final snapshot.
    void update_results() noexcept;
    // Execute the scene-independent part of GameState.ProcessFrame. The
    // caller supplies the already simulated player snapshot; render/audio
    // transitions remain frontend-owned.
    void process_match_frame(float seconds,
                             std::span<const net::PlayerState> players,
                             bool score_authority = true) noexcept;
    // Dispatch the process-wide portion of Scene.DispatchMessage.  Entity
    // delivery stays in Scene/World, but story checkpoints, escape updates,
    // trigger bits, and room-transition requests belong to GameState just as
    // they do in the managed runtime.
    void process_message(const messaging::MessageInfo& info) noexcept;
    // Advance story-only timers which are not part of a multiplayer snapshot.
    // Audio, dialogs, and the actual player death remain frontend-owned.
    void process_story_frame(float seconds) noexcept;
    // Reset the story escape sequence when the player returns to the ship or
    // when a room reload starts.  Music/event-sound cleanup is intentionally
    // left to the audio frontend, matching the existing State boundary.
    void reset_escape_state() noexcept;
    void update_escape_state(std::int32_t frames,
                             std::int32_t state_id) noexcept;
    // Match setup mirrors the managed GameState.Setup rule table.  Scene and
    // camera initialization remain scene-owned, but all mode-independent
    // public state is prepared here for the native game loop.
    void setup() noexcept;
    void update_time(float seconds) noexcept;
    void start_new_save() noexcept;
    void complete_random_encounter(std::int32_t room_id) noexcept;
    void update_boss_flags(std::int32_t area_id) noexcept;
    // Apply the story-save mutations performed when the player confirms the
    // ship-hatch return: clear the active escape trigger and turn every boss
    // *Kill marker into its corresponding *Done marker.
    void enter_ship() noexcept;
    [[nodiscard]] AreaState area_state(
        std::int32_t area_id, const StorySave* save = nullptr) const noexcept;
    void set_trigger(std::size_t index) noexcept;
    void clear_trigger(std::size_t index) noexcept;
    [[nodiscard]] bool trigger_set(std::size_t index) const noexcept;
    [[nodiscard]] bool in_room_transition() const noexcept {
        return transition_state != TransitionState::None;
    }
    [[nodiscard]] bool multiplayer() const noexcept { return !single_player; }
    [[nodiscard]] bool is_octolith_mode() const noexcept;
    [[nodiscard]] static bool is_team_mode(Mode value) noexcept;
};

} // namespace fruityprime::game

namespace MphReadNative {
using GameState = ::fruityprime::game::State;
namespace Game = ::fruityprime::game;
}
