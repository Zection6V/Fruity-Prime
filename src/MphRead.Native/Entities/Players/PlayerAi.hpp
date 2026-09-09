#pragma once

#include <string>
#include "Formats/ai_personality.hpp"
#include "Entities/gameplay.hpp"
#include "Entities/Players/ai_globals.hpp"
#include "Entities/Players/PlayerProcess.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace fruityprime::players {

// These values are the fields exposed by PlayerAiData in PlayerAi.cs. They
// remain masks so personality conditions preserve the cartridge bit meaning.
enum class AiFlags2 : std::uint32_t {
    None = 0,
    Bit0 = 1u << 0,
    Bit1 = 1u << 1,
    TargetPlayer = 1u << 2,
    TargetHalfturret = 1u << 3,
    TargetItem = 1u << 4,
    TargetDefense = 1u << 5,
    TargetDoor = 1u << 6,
    Bit7 = 1u << 7,
    Bit8 = 1u << 8,
    Bit9 = 1u << 9,
    Bit10 = 1u << 10,
    Bit11 = 1u << 11,
    Bit12 = 1u << 12,
    Bit13 = 1u << 13,
    AiStart = 1u << 14,
    Bit15 = 1u << 15,
    Bit16 = 1u << 16,
    Bit17 = 1u << 17,
    Bit18 = 1u << 18,
    Bit19 = 1u << 19,
    Bit20 = 1u << 20,
    Bit21 = 1u << 21,
    Unused22 = 1u << 22,
    Unused23 = 1u << 23,
    Unused24 = 1u << 24,
    Unused25 = 1u << 25,
    Unused26 = 1u << 26,
    Unused27 = 1u << 27,
    Unused28 = 1u << 28,
    Unused29 = 1u << 29,
    Unused30 = 1u << 30,
    Unused31 = 1u << 31
};

enum class AiFlags3 : std::uint32_t {
    None = 0,
    NoInput = 1u << 0,
    Bit1 = 1u << 1,
    Bit2 = 1u << 2,
    Despawned = 1u << 3,
    Invulnerable = 1u << 4,
    Bit5 = 1u << 5
};

enum class AiFlags4 : std::uint8_t {
    None = 0,
    Bit0 = 1u << 0,
    Bit1 = 1u << 1,
    Bit2 = 1u << 2,
    Bit3 = 1u << 3
};

[[nodiscard]] constexpr bool has_flag(AiFlags2 value,
                                       AiFlags2 flag) noexcept {
    return (static_cast<std::uint32_t>(value)
            & static_cast<std::uint32_t>(flag)) != 0;
}

[[nodiscard]] constexpr bool has_flag(AiFlags3 value,
                                       AiFlags3 flag) noexcept {
    return (static_cast<std::uint32_t>(value)
            & static_cast<std::uint32_t>(flag)) != 0;
}

[[nodiscard]] constexpr bool has_flag(AiFlags4 value,
                                       AiFlags4 flag) noexcept {
    return (static_cast<std::uint8_t>(value)
            & static_cast<std::uint8_t>(flag)) != 0;
}

constexpr AiFlags2 operator|(AiFlags2 left, AiFlags2 right) noexcept {
    return static_cast<AiFlags2>(static_cast<std::uint32_t>(left)
                                 | static_cast<std::uint32_t>(right));
}

constexpr AiFlags2 operator~(AiFlags2 value) noexcept {
    return static_cast<AiFlags2>(~static_cast<std::uint32_t>(value));
}

constexpr AiFlags2& operator|=(AiFlags2& left, AiFlags2 right) noexcept {
    left = left | right;
    return left;
}

constexpr AiFlags2& operator&=(AiFlags2& left, AiFlags2 right) noexcept {
    left = static_cast<AiFlags2>(static_cast<std::uint32_t>(left)
                                 & static_cast<std::uint32_t>(right));
    return left;
}

constexpr AiFlags3 operator|(AiFlags3 left, AiFlags3 right) noexcept {
    return static_cast<AiFlags3>(static_cast<std::uint32_t>(left)
                                 | static_cast<std::uint32_t>(right));
}

constexpr AiFlags3 operator~(AiFlags3 value) noexcept {
    return static_cast<AiFlags3>(~static_cast<std::uint32_t>(value));
}

constexpr AiFlags3& operator|=(AiFlags3& left, AiFlags3 right) noexcept {
    left = left | right;
    return left;
}

constexpr AiFlags3& operator&=(AiFlags3& left, AiFlags3 right) noexcept {
    left = static_cast<AiFlags3>(static_cast<std::uint32_t>(left)
                                 & static_cast<std::uint32_t>(right));
    return left;
}

constexpr AiFlags4 operator|(AiFlags4 left, AiFlags4 right) noexcept {
    return static_cast<AiFlags4>(static_cast<std::uint8_t>(left)
                                 | static_cast<std::uint8_t>(right));
}

constexpr AiFlags4 operator~(AiFlags4 value) noexcept {
    return static_cast<AiFlags4>(static_cast<std::uint8_t>(
        ~static_cast<std::uint8_t>(value)));
}

constexpr AiFlags4& operator|=(AiFlags4& left, AiFlags4 right) noexcept {
    left = left | right;
    return left;
}

constexpr AiFlags4& operator&=(AiFlags4& left, AiFlags4 right) noexcept {
    left = static_cast<AiFlags4>(static_cast<std::uint8_t>(left)
                                 & static_cast<std::uint8_t>(right));
    return left;
}


// PlayerAi.AiQueuedEnt and AiEntRefType.  Both are plain TypeN = N runs in
// the managed source; the numbers are what the personality data holds, so
// they are kept rather than given names the cartridge never had.
enum class AiQueuedEnt : std::uint8_t {
    Type0 = 0, Type1 = 1, Type2 = 2, Type3 = 3, Type4 = 4, Type5 = 5,
    Type6 = 6, Type7 = 7, Type8 = 8, Type9 = 9, Type10 = 10, Type11 = 11,
    Type12 = 12, Type13 = 13, Type14 = 14, Type15 = 15, Type16 = 16,
    Type17 = 17, Type18 = 18, Type19 = 19, Type20 = 20, Type21 = 21,
    Type22 = 22, Type23 = 23, Type24 = 24, Type25 = 25, Type26 = 26,
    Type27 = 27, Type28 = 28, Type29 = 29, Type30 = 30, Type31 = 31,
};

enum class AiEntRefType : std::uint8_t {
    Type0 = 0, Type1 = 1, Type2 = 2, Type3 = 3, Type4 = 4, Type5 = 5,
    Type6 = 6, Type7 = 7, Type8 = 8, Type9 = 9, Type10 = 10, Type11 = 11,
    Type12 = 12, Type13 = 13, Type14 = 14, Type15 = 15, Type16 = 16,
    Type17 = 17, Type18 = 18, Type19 = 19, Type20 = 20, Type21 = 21,
    Type22 = 22, Type23 = 23, Type24 = 24, Type25 = 25, Type26 = 26,
    Type27 = 27, Type28 = 28, Type29 = 29, Type30 = 30, Type31 = 31,
    Type32 = 32, Type33 = 33, Type34 = 34, Type35 = 35, Type36 = 36,
    Type37 = 37, Type38 = 38, Type39 = 39, Type40 = 40, Type41 = 41,
    Type42 = 42, Type43 = 43, Type44 = 44, Type45 = 45, Type46 = 46,
    Type47 = 47, Type48 = 48, Type49 = 49, Type50 = 50, Type51 = 51,
    Type52 = 52, Type53 = 53, Type54 = 54, Type55 = 55, Type56 = 56,
    Type57 = 57, Type58 = 58, Type59 = 59, Type60 = 60, Type61 = 61,
    Type62 = 62, Type63 = 63, Type64 = 64, Type65 = 65, Type66 = 66,
    Type67 = 67, Type68 = 68, Type69 = 69, Type70 = 70, Type71 = 71,
    Type72 = 72, Type73 = 73, Type74 = 74, Type75 = 75, Type76 = 76,
    Type77 = 77,
};

struct AiButton {
    bool is_down = false;
    std::uint32_t frames_down = 0;
    std::uint32_t frames_up = 0;

    void clear() noexcept {
        is_down = false;
        frames_down = 0;
        frames_up = 0;
    }
};

// The managed class uses twelve controls. The native input boundary is
// already post-keybind, so these names identify the resulting intents.
enum class AiButtonId : std::uint8_t {
    Up,
    Down,
    Left,
    Right,
    Jump,
    Morph,
    Shoot,
    AltAttack,
    Boost,
    Zoom,
    NextWeapon,
    PrevWeapon,
    Count
};

struct AiButtons {
    std::array<AiButton,
               static_cast<std::size_t>(AiButtonId::Count)> values{};

    void clear() noexcept {
        for (auto& value : values) {
            value.clear();
        }
    }

    [[nodiscard]] AiButton& operator[](AiButtonId id) noexcept {
        return values[static_cast<std::size_t>(id)];
    }

    [[nodiscard]] const AiButton& operator[](AiButtonId id) const noexcept {
        return values[static_cast<std::size_t>(id)];
    }
};

struct AiAggroEntry {
    std::uint8_t var_a2 = 0;
    std::uint8_t var_a9 = 0;
    std::uint8_t var_a3 = 0;
    std::uint8_t var_a4 = 0;
    std::uint8_t var_a10 = 0;
    std::uint16_t var_a7 = 0;
    std::uint16_t staleness = 0;
    std::uint16_t expiration = 0;
    std::uint8_t player1 = 0xff;
    std::uint8_t player2 = 0xff;

    void clear() noexcept { *this = {}; }
    [[nodiscard]] bool matches(std::uint8_t a2, std::uint8_t a3,
                               std::uint8_t a4, std::uint8_t first,
                               std::uint8_t second) const noexcept;
};

// One context exists for every active depth of the C# execution tree. The
// byte fields are retained even where a native controller does not yet use a
// particular Func1/2/3/4 ID.
struct AiContext {
    std::int32_t func24_id = 0;
    std::array<std::uint8_t, 13> fields{};
    std::int32_t field14 = 0;
    std::int32_t field18 = 0;
    std::int32_t field1c = 0;
    std::int32_t field20 = 0;
    std::int32_t field24 = 0;
    bool field28 = false;
    std::int32_t field2c = 0;
    bool field30 = false;
    net::Vec3 field34;
    std::int32_t field40 = 0;
    std::int32_t field44 = 0;
    const ai::Data1* data1 = nullptr;
    std::uint32_t call_count = 0;
    std::uint8_t depth = 0;
    std::array<std::int32_t, 21> weights{};

    void clear() noexcept {
        func24_id = 0;
        fields.fill(0);
        field14 = 0;
        field18 = 0;
        field1c = 0;
        field20 = 0;
        field24 = 0;
        field28 = false;
        field2c = 0;
        field30 = false;
        field34 = {};
        field40 = 0;
        field44 = 0;
        data1 = nullptr;
        call_count = 0;
        depth = 0;
        weights.fill(0);
    }
};

struct BotDecision {
    gameplay::Input input;
    std::uint8_t target_slot = 0xff;
    float target_distance = 0.0F;
    std::uint32_t aggro_score = 0;
    std::int32_t personality_func24_id = 0;
    std::uint32_t ai_flags2 = 0;
    std::uint32_t ai_flags3 = 0;
    std::uint8_t ai_flags4 = 0;
};

// Native state holder for PlayerEntity.PlayerAiData. Session deliberately
// does not own this object yet: each bot/replay caller can keep one instance.
class PlayerAiData final {
public:
    static constexpr std::size_t MaxAggroEntries = 25;
    static constexpr std::size_t MaxContextDepth = 20;

    PlayerAiData() noexcept { reset(); }

    void reset() noexcept;
    void initialize(const ai::Personality* personality) noexcept;
    void set_bot_level(int level) noexcept;

    // PlayerAi.InitializeAtLoad: a bot that is being created with the room.
    // The random-delay values are rolled here and not again, which is why a
    // bot that respawns keeps the reflexes it was loaded with.
    void initialize_at_load(const ai::Personality* personality,
                            std::uint8_t hunter, int bot_level,
                            bool single_player, bool in_encounter) noexcept;

    // PlayerAi.InitializeAtSpawn: the same, minus the delay roll.  Respawning
    // must not re-roll it, or a bot's reaction time would change every death.
    void initialize_at_spawn(const ai::Personality* personality) noexcept;

    // PlayerAi.InitializeSub: the two random-delay counters, in doubled
    // frames.  A story bot mid-encounter reacts instantly; everyone else waits
    // a hunter- and level-dependent number of frames.
    void initialize_sub(std::uint8_t hunter, int bot_level, bool single_player,
                        bool in_encounter) noexcept;
    [[nodiscard]] std::uint32_t aim_delay() const noexcept {
        return field102c_;
    }
    [[nodiscard]] std::uint32_t decision_delay() const noexcept {
        return field1030_;
    }

    // PlayerAi.ClearInput: every button released, every aim axis zeroed, and
    // the touch counters reset.  A button left down across a respawn would
    // keep firing.
    void clear_input() noexcept;

    // PlayerAi.GetOuptut -- the managed spelling is kept.  Renders the bot's
    // current execution path and the weights that chose it, which is the only
    // way to see why a bot is doing what it is doing.
    [[nodiscard]] std::string get_ouptut() const;
    void process_input(const gameplay::Input& input) noexcept;
    void process(const gameplay::Session& session,
                 std::uint8_t bot_slot) noexcept;
    void notify_damage(std::uint8_t source_slot,
                       std::uint32_t damage) noexcept;
    void update_visibility(const gameplay::Session& session) noexcept;

    [[nodiscard]] AiFlags2 flags2() const noexcept { return flags2_; }
    [[nodiscard]] AiFlags3 flags3() const noexcept { return flags3_; }
    [[nodiscard]] AiFlags4 flags4() const noexcept { return flags4_; }
    [[nodiscard]] std::uint8_t target_slot() const noexcept {
        return target_slot_;
    }
    [[nodiscard]] std::uint32_t aggro_score() const noexcept {
        return aggro_score_;
    }
    [[nodiscard]] std::int32_t personality_func24_id() const noexcept {
        return execution_tree_[0].func24_id;
    }
    [[nodiscard]] std::size_t aggro_count() const noexcept {
        return player_aggro_count_;
    }
    [[nodiscard]] const std::array<AiAggroEntry, MaxAggroEntries>& aggro()
        const noexcept {
        return player_aggro_;
    }
    [[nodiscard]] const AiButtons& buttons() const noexcept {
        return buttons_;
    }
    [[nodiscard]] const std::array<AiContext, MaxContextDepth>&
    execution_tree() const noexcept {
        return execution_tree_;
    }
    [[nodiscard]] std::size_t unknown_func3_calls() const noexcept {
        return unknown_func3_calls_;
    }
    [[nodiscard]] const std::array<std::array<bool, net::NetConfig::SlotCapacity>,
                                   net::NetConfig::SlotCapacity>&
    visibility() const noexcept {
        return visibility_;
    }

private:
    void update_aggro(const gameplay::Session& session,
                      std::uint8_t bot_slot) noexcept;
    void update_aggro_expiration() noexcept;

    // PlayerAi.ExecuteVectorFunc and the eight vector functions it selects
    // between.  The game packs all three parameters into one value, which
    // would matter if it came from metadata; it never does, so they are
    // spelled out.
    //
    // `aim` is the bot's own aim vectors (PlayerProcess.UpdateAimVecs), which
    // three of the eight read.
    [[nodiscard]] net::Vec3 execute_vector_func(
        const gameplay::Session& session, std::uint8_t bot_slot,
        const PlayerProcess::AimVectors& aim, net::Vec3 camera_facing,
        int index, bool clear_y, bool normalize) const noexcept;

    [[nodiscard]] net::Vec3 func_213A470(const gameplay::Session& session,
                                          std::uint8_t bot_slot)
        const noexcept;
    [[nodiscard]] net::Vec3 func_213A458(const gameplay::Session& session,
                                          std::uint8_t bot_slot)
        const noexcept;
    [[nodiscard]] net::Vec3 func_213A3DC(
        const gameplay::Session& session,
        const PlayerProcess::AimVectors& aim) const noexcept;
    [[nodiscard]] net::Vec3 func_213A3C0(
        const PlayerProcess::AimVectors& aim) const noexcept;
    [[nodiscard]] net::Vec3 func_213A3A8(const gameplay::Session& session)
        const noexcept;
    [[nodiscard]] net::Vec3 func_213A37C(const gameplay::Session& session,
                                          std::uint8_t bot_slot)
        const noexcept;
    [[nodiscard]] net::Vec3 func_213A35C(net::Vec3 camera_facing)
        const noexcept;
    [[nodiscard]] net::Vec3 func_213A31C(const gameplay::Session& session,
                                          std::uint8_t bot_slot)
        const noexcept;

    // PlayerAi.CheckBeam and CheckCharge: whether the bot has a beam at all,
    // and whether it is worth charging.  GetBeamType maps the bot's own
    // weapon numbering onto the cartridge's.
    [[nodiscard]] static std::uint8_t get_beam_type(int weapon) noexcept;
    [[nodiscard]] bool check_beam(const gameplay::Session& session,
                                   std::uint8_t bot_slot,
                                   std::uint8_t beam) const noexcept;
    [[nodiscard]] bool check_charge(std::uint8_t beam) const noexcept;

    // PlayerAi.IsPlayerVisible: whether the first player can see the second.
    [[nodiscard]] bool is_player_visible(std::uint8_t viewer,
                                          std::uint8_t other) const noexcept;


    // ---- PlayerAi's Func1_* behaviours ---------------------------------
    // Generated declarations; the bodies are in ai_funcs1.generated.cpp.
    // ExecuteFuncs1 dispatches to these by the identifier in the personality
    // data, through the table in ai_dispatch.generated.hpp.
    void execute_funcs1(const gameplay::Session& session,
                        std::uint8_t bot_slot, int func_id) noexcept;

    // PlayerAi.FindEntityRef: look up one of the seventy-eight entity
    // references a behaviour can ask for.  The reference table is not
    // populated by this head yet, so this records what was asked for.
    void find_entity_ref(AiEntRefType type) noexcept;

    // PlayerAi.UpdateTargetItem: point the bot at an item, or at nothing.
    // An item that is despawning is not worth walking to, which is why the
    // flag follows the timer rather than the reference.
    void update_target_item(const gameplay::Session& session,
                            std::int32_t item_index) noexcept;

    // PlayerAi's local SetEntity: remember an item spawn, but only one that
    // actually has an item on it.
    void set_item_spawn(std::int32_t spawn_index) noexcept;

    [[nodiscard]] const AiEntityRefs& entity_refs() const noexcept {
        return entity_refs_;
    }

    // Behaviour index to behaviour, generated alongside the bodies.
    void dispatch_funcs1(const gameplay::Session& session,
                         std::uint8_t bot_slot,
                         std::uint8_t behavior) noexcept;

    void func1_214A39C(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_214A098(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149D3C(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149C98(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149C80(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149C68(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149C50(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149C38(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149C20(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149C08(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149BF0(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149BD8(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149BC0(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149BA8(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149B98(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149AD8(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149AC8(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149ABC(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149AB0(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149AA4(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149A98(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149A64(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149824(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21497F0(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149570(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21494FC(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149488(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149414(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21493A0(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_214932C(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21495A4(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149530(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21494BC(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149448(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21493D4(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149360(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21492EC(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21492DC(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21492CC(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21492BC(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21492AC(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_214929C(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_214928C(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_214927C(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_214926C(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_214925C(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_214924C(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_214923C(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_214922C(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_214921C(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_214920C(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21491FC(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21491E4(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21491CC(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21491B4(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_214919C(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149184(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_214916C(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149154(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_214913C(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149124(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_214910C(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21490F4(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21490DC(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21490C4(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21490AC(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149094(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149088(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149034(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2148F10(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2148EDC(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2148ECC(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2148EB8(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2148EA8(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2148E98(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2148E88(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2148E74(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2148E64(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2148E54(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2148DF8(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2148DE8(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2148D50(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_UnlockEchoHallForceField(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_SetInvulnerable(const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;

    void add_aggro(std::uint8_t a2, std::uint8_t a3, std::uint8_t a4,
                   std::uint8_t player1, std::uint8_t player2,
                   std::uint32_t value, std::uint32_t expiration,
                   std::uint32_t priority, std::uint8_t mode) noexcept;
    [[nodiscard]] std::uint32_t aggro_priority(
        std::uint8_t a2, std::uint8_t a3, std::uint8_t a4,
        std::uint8_t player1, std::uint8_t player2) const noexcept;
    [[nodiscard]] int evaluate_func3(const gameplay::Session& session,
                                      std::uint8_t bot_slot,
                                      const AiContext& context,
                                      int func_id,
                                      const ai::Parameters& parameters) noexcept;
    void update_execution_path(const gameplay::Session* session,
                               std::uint8_t bot_slot,
                               const ai::Data1* node,
                               std::size_t depth) noexcept;
    void execute(const gameplay::Session& session,
                 std::uint8_t bot_slot, std::size_t depth) noexcept;
    [[nodiscard]] int update_path_weights(
        const gameplay::Session& session, std::uint8_t bot_slot,
        AiContext& context) noexcept;

    const ai::Personality* personality_ = nullptr;
    AiFlags2 flags2_ = AiFlags2::None;
    AiFlags3 flags3_ = AiFlags3::None;
    AiFlags4 flags4_ = AiFlags4::None;
    AiButtons buttons_;
    std::array<AiAggroEntry, MaxAggroEntries> player_aggro_{};
    std::size_t player_aggro_count_ = 0;
    std::array<AiContext, MaxContextDepth> execution_tree_{};
    std::array<std::array<bool, net::NetConfig::SlotCapacity>,
               net::NetConfig::SlotCapacity> visibility_{};
    std::array<std::uint32_t, net::NetConfig::SlotCapacity> slot_hits_{};
    std::array<std::uint32_t, net::NetConfig::SlotCapacity> slot_damage_{};
    std::uint32_t damage_from_halfturret_ = 0;
    std::uint32_t field118_ = 0;
    std::uint8_t target_slot_ = 0xff;
    std::uint32_t aggro_score_ = 0;
    // The entity lookup a behaviour has asked for, and the last reference
    // type it wanted.  PlayerAi sets these and the find pass consumes them.
    AiQueuedEnt queued_find_entity_action_ = AiQueuedEnt::Type0;
    AiEntRefType last_entity_ref_ = AiEntRefType::Type0;
    // PlayerAi._entityRefs, _itemC8 and _itemSpawnC4.
    AiEntityRefs entity_refs_;
    std::int32_t target_item_ = -1;
    std::int32_t item_spawn_ = -1;
    // ---- the rest of PlayerAiData's state ------------------------------
    // The managed class declares sixty-one fields; these are the ones the
    // behaviours touch that were not here before.  Entity references are the
    // referred-to entity's index in the session's own list, or -1 for none,
    // which is the null the managed code branches on.
    bool force_disable_ = false;
    // _node3C, _node40, _node44, _node48: path nodes the bot is working
    // between.  Node navigation is not ported, so these stay unset.
    std::int32_t node3c_ = -1;
    std::int32_t node40_ = -1;
    std::int32_t node44_ = -1;
    std::int32_t node48_ = -1;
    std::uint16_t field78_ = 0;
    std::int32_t target_halfturret_ = -1;
    std::int32_t octolith_flag_cc_ = -1;
    std::int32_t flag_base_d0_ = -1;
    std::int32_t octolith_flag_d4_ = -1;
    std::int32_t flag_base_d8_ = -1;
    std::int32_t octolith_flag_dc_ = -1;
    std::int32_t flag_base_e0_ = -1;
    std::int32_t target_defense_ = -1;
    std::int32_t target_door_ = -1;
    std::int32_t node_data_set_index_ = 0;
    std::uint8_t node_data_sel_off_ = 0;
    std::uint8_t node_data_sel_on_ = 0;
    std::uint32_t field102e_ = 0;
    std::uint32_t field1034_ = 0;
    std::int32_t field116_ = 0;
    std::int32_t field1020_ = 0;
    std::int32_t field1032_ = 0;
    net::Vec3 field_a0_{};
    net::Vec3 field_ac_{};
    net::Vec3 field_b8_{};
    net::Vec3 field1038_{};
    net::Vec3 field1048_{};
    net::Vec3 field1054_{};
    std::int32_t field30_ = 0;
    net::Vec3 field90_{};
    float field9c_ = 0.0F;
    std::int32_t shot_delay_ = 0;
    std::uint16_t touch_aim_x_ = 0;
    std::uint16_t touch_aim_y_ = 0;
    bool has_touch_ = false;
    std::uint16_t frames_with_touch_ = 0;
    std::uint16_t frames_without_touch_ = 0;
    float button_aim_x_ = 0.0F;
    float button_aim_y_ = 0.0F;
    std::uint8_t bot_level_ = 0;
    std::uint8_t weapon1_ = 0;
    std::uint8_t weapon2_ = 0;
    std::int32_t find_weapon_index_ = 0;
    std::uint64_t frame_count_ = 0;
    std::size_t unknown_func3_calls_ = 0;
    // The two random-delay counters InitializeSub rolls, in doubled
    // frames: how long the bot takes to aim, and how long it holds a
    // decision before reconsidering.
    std::uint32_t field102c_ = 0;
    std::uint32_t field1030_ = 0;
};

class BotAi final {
public:
    [[nodiscard]] static BotDecision decide(
        const gameplay::Session& session, std::uint8_t bot_slot,
        int level = 1) noexcept;
    [[nodiscard]] static BotDecision decide(
        const gameplay::Session& session, std::uint8_t bot_slot,
        PlayerAiData& state, int level = 1) noexcept;
};

} // namespace fruityprime::players
