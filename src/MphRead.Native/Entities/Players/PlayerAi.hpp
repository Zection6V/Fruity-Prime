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
    Type32 = 32, Type33 = 33, Type34 = 34, Type35 = 35, Type36 = 36,
    Type37 = 37, Type38 = 38, Type39 = 39,
    // Not zero: a behaviour asking for nothing is a distinct value from
    // one asking for Type0, and the find pass compares against this.
    None = 40,
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
// The DS's own buttons, which is what the managed AiButtons holds.  They
// were semantic names here -- Jump, Morph, Shoot -- and that lost the thing
// that makes them interesting: what a button does depends on the form the
// player is in.  L is jump on foot and the alt-form attack as a ball; R is
// fire on foot and boost as a ball.  A bot pressing "L" is not pressing
// "jump", and ProcessInput is where the difference is resolved.
enum class AiButtonId : std::uint8_t {
    Up,
    Down,
    Left,
    Right,
    A,
    B,
    X,
    Y,
    L,
    R,
    Start,
    Select,
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

enum class AiTouchButtonId : std::uint8_t {
    Morph,
    Unmorph,
    PowerBeam,
    Missile,
    VoltDriver,
    Battlehammer,
    Imperialist,
    Judicator,
    Magmaul,
    ShockCoil,
    OmegaCannon,
    Count
};

struct AiTouchButtons {
    std::array<AiButton,
               static_cast<std::size_t>(AiTouchButtonId::Count)> values{};

    void clear() noexcept {
        for (auto& value : values) {
            value.clear();
        }
    }

    [[nodiscard]] AiButton& operator[](AiTouchButtonId id) noexcept {
        return values[static_cast<std::size_t>(id)];
    }

    [[nodiscard]] const AiButton& operator[](
        AiTouchButtonId id) const noexcept {
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
    std::uint8_t field4 = 0;
    std::uint8_t field5 = 0;
    std::uint8_t field6 = 0;
    std::uint8_t field7 = 0;
    std::uint8_t field8 = 0;
    std::uint8_t field9 = 0;
    std::uint8_t fielda = 0;
    std::uint8_t fieldb = 0;
    std::uint8_t fieldc = 0;
    std::uint8_t fieldd = 0;
    std::uint8_t fielde = 0;
    std::uint8_t fieldf = 0;
    std::uint8_t field10 = 0;
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
        field4 = 0;
        field5 = 0;
        field6 = 0;
        field7 = 0;
        field8 = 0;
        field9 = 0;
        fielda = 0;
        fieldb = 0;
        fieldc = 0;
        fieldd = 0;
        fielde = 0;
        fieldf = 0;
        field10 = 0;
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

// What one pass of PlayerAi.ProcessInput produced.  Two of the four
// channels the cartridge writes have no bit on the wire: aim is a rotation
// in degrees per frame -- the same channel a gamepad's right stick uses --
// and the d-pad's on-foot half is the four aim keybinds rather than a
// movement direction.
struct AiInputResult {
    gameplay::Input input;
    float aim_degrees_x = 0.0F;
    float aim_degrees_y = 0.0F;
    bool aim_up = false;
    bool aim_down = false;
    bool aim_left = false;
    bool aim_right = false;
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
    // PlayerAi.ProcessInput: turn what the behaviours pressed into an
    // input, and age every button's held and released counter.  `alt_form`
    // decides what six of the twelve buttons mean.
    [[nodiscard]] AiInputResult process_input(
        bool alt_form = false) noexcept;

    // The controls a behaviour presses.  The managed class reaches its own
    // fields directly; these are the same two tables under a name a caller
    // outside the behaviours can use.
    [[nodiscard]] AiButton& button(AiButtonId id) noexcept {
        return buttons_[id];
    }
    [[nodiscard]] AiButton& touch_button(AiTouchButtonId id) noexcept {
        return touch_buttons_[id];
    }
    [[nodiscard]] const AiTouchButtons& touch_buttons() const noexcept {
        return touch_buttons_;
    }
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

    // PlayerAi.ExecuteFuncs2 and ExecuteFuncs4: the behaviour a node runs
    // while the bot sits in it, and the one it runs on entering.  Both take
    // the context because the behaviours write to it.
    void execute_funcs2(const gameplay::Session& session,
                        std::uint8_t bot_slot, AiContext& context) noexcept;
    void execute_funcs4(const gameplay::Session& session,
                        std::uint8_t bot_slot, AiContext& context) noexcept;

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
    void dispatch_funcs2(const gameplay::Session& session,
                         std::uint8_t bot_slot, AiContext& context,
                         std::uint8_t behavior) noexcept;
    void dispatch_funcs4(const gameplay::Session& session,
                         std::uint8_t bot_slot, AiContext& context,
                         std::uint8_t behavior) noexcept;
    // ExecuteFuncs3's mapping.  `handled` is false for an identifier whose
    // predicate is not ported, which is a different answer from 0.
    [[nodiscard]] int dispatch_funcs3(const gameplay::Session& session,
                                      std::uint8_t bot_slot,
                                      const AiContext& context,
                                      const ai::Parameters& parameters,
                                      int func_id,
                                      bool& handled) noexcept;

    // ---- generated behaviour declarations: begin --------------
    // ExecuteFuncs1's behaviours.
    void func1_2148D50(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2148DE8(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2148DF8(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2148E54(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2148E64(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2148E74(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2148E88(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2148E98(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2148EA8(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2148EB8(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2148ECC(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2148EDC(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2148F10(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149034(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149088(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149094(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21490AC(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21490C4(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21490DC(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21490F4(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_214910C(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149124(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_214913C(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149154(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_214916C(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149184(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_214919C(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21491B4(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21491CC(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21491E4(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21491FC(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_214920C(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_214921C(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_214922C(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_214923C(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_214924C(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_214925C(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_214926C(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_214927C(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_214928C(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_214929C(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21492AC(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21492BC(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21492CC(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21492DC(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21492EC(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_214932C(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149360(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21493A0(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21493D4(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149414(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149448(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149488(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21494BC(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21494FC(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149530(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149570(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21495A4(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_21497F0(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149824(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149A64(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149A98(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149AA4(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149AB0(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149ABC(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149AC8(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149AD8(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149B98(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149BA8(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149BC0(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149BD8(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149BF0(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149C08(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149C20(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149C38(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149C50(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149C68(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149C80(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149C98(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_2149D3C(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_214A098(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_214A39C(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_SetInvulnerable(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func1_UnlockEchoHallForceField(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    // ExecuteFuncs2's behaviours.
    void func2_213D96C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            AiContext& context) noexcept;
    void func2_213D9B8(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            AiContext& context) noexcept;
    void func2_213DA88(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            AiContext& context) noexcept;
    void func2_213DDCC(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            AiContext& context) noexcept;
    void func2_213E148(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            AiContext& context) noexcept;
    void func2_213E1CC(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            AiContext& context) noexcept;
    void func2_213E274(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            AiContext& context) noexcept;
    void func2_213E31C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            AiContext& context) noexcept;
    void func2_213E3C4(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            AiContext& context) noexcept;
    void func2_213E684(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            AiContext& context) noexcept;
    void func2_213E904(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            AiContext& context) noexcept;
    void func2_213E934(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            AiContext& context) noexcept;
    void func2_213E984(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            AiContext& context) noexcept;
    void func2_213E9C8(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            AiContext& context) noexcept;
    void func2_213EA10(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            AiContext& context) noexcept;
    void func2_213EA48(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            AiContext& context) noexcept;
    // ExecuteFuncs3's predicates.
    [[nodiscard]] int func3_213A650(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213A660(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213A688(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213A698(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213A714(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213A72C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213A798(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213A804(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213A828(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213A844(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213A868(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213A884(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213A8A8(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213A8DC(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213A900(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213A91C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213A938(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213A94C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213A9B8(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213AA20(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213AA64(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213AAF0(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213AB24(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213AB58(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213AB8C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213ABC0(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213AC04(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213AC38(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213AC54(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213AC70(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213AC8C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213ACA8(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213ACCC(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213ACE8(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213AD64(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213AD88(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213ADA0(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213ADC4(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213ADF8(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213AE14(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213AE30(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213AE48(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213AE60(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213AE78(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213AE90(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213AEA8(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213AEC0(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213AED8(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213AEF0(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213AF08(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213AF20(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213AF38(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213AF50(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213AF68(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213AF80(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213AFA0(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213AFC0(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213AFE0(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B000(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B020(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B040(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B058(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B070(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B088(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B0A0(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B0B8(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B0D0(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B0E8(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B100(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B118(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B130(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B148(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B160(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B178(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B190(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B1A8(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B1C0(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B1D8(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B1F0(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B260(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B284(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B328(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B34C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B37C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B3A0(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B3F0(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B45C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B4A0(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B4E4(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B528(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B5DC(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B690(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B77C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B7A0(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B88C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B8B0(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B978(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213B99C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213BA04(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213BA28(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213BA44(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213BA68(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213BAD0(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213BAF4(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213BB7C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213BBA0(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213BBE8(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213BC0C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213BC4C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213BC70(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213BC8C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213BCB0(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213BCC4(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213BCE8(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213BD7C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213BDF4(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213BE10(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213BE48(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213BEA0(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213BEBC(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213BED8(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213BFD8(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213BFFC(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213C054(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213C078(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213C0D0(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213C310(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213C334(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213C470(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213C48C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213C52C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213C600(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213C64C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213C698(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213C75C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213C764(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213C88C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213C89C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213C9C4(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213C9D4(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213CA00(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213CA2C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213CA58(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213CA70(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213CA84(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213CAA8(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213CADC(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213CB8C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213CBB0(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213CBC0(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213CBE4(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213CC94(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213CCB0(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213CCBC(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213CCD8(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213CCF4(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213CD18(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213CD34(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213CD58(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213CD74(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213CDA4(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213CDB8(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213CEE8(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213CF0C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213CF7C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213CF94(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213CFA4(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213CFC0(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213CFDC(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213CFF4(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D010(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D028(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D044(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D05C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D078(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D0A8(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D0C4(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D0F4(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D128(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D15C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D178(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D218(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D234(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D2A4(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D2C0(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D36C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D388(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D418(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D43C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D49C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D4C0(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D514(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D530(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D540(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D564(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D608(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D624(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D6AC(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D6D0(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D710(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D734(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D758(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D77C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D7A0(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D7B8(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D7D0(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D7E8(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D7F0(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D800(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D814(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D83C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    [[nodiscard]] int func3_213D87C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            const AiContext& context,
            const ai::Parameters& parameters) noexcept;
    // ExecuteFuncs4's behaviours.
    void func4_2145E40(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            AiContext& context) noexcept;
    void func4_2145E54(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            AiContext& context) noexcept;
    void func4_2145EB0(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            AiContext& context) noexcept;
    void func4_2145F00(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            AiContext& context) noexcept;
    void func4_2145F28(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            AiContext& context) noexcept;
    void func4_2145F50(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            AiContext& context) noexcept;
    void func4_2145F78(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            AiContext& context) noexcept;
    void func4_214612C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            AiContext& context) noexcept;
    void func4_21461EC(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            AiContext& context) noexcept;
    void func4_2146284(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            AiContext& context) noexcept;
    void func4_21462AC(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            AiContext& context) noexcept;
    void func4_21462DC(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            AiContext& context) noexcept;
    void func4_SetDespawned(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            AiContext& context) noexcept;
    // the helpers those call.
    void checkUnmorph(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void findQueuedEntityRef(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func2135320(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func2135380(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func2135480(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func21354B0(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func21354E0(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func2135510(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func21355D8(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func213FD94(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func214003C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            AiContext& context) noexcept;
    void func2140094(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            AiContext& context) noexcept;
    void func2140D5C(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func214182C(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func2141EA8(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func214201C(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func2142A80(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func2142D38(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func2142DCC(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func2142FC0(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func21430B4(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func21431B4(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func21433E4(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func2143470(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func2143578(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func2143658(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func21436D8(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func214380C(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func2143A40(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func21447E8(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func2144964(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func21449DC(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func2144AE4(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func2144B88(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func2145BA0(
            const gameplay::Session& session,
            std::uint8_t bot_slot) noexcept;
    void func214715C(
            const gameplay::Session& session,
            std::uint8_t bot_slot,
            AiContext& context) noexcept;
    // ---- generated behaviour declarations: end ----------------
    void add_aggro(std::uint8_t a2, std::uint8_t a3, std::uint8_t a4,
                   std::uint8_t player1, std::uint8_t player2,
                   std::uint32_t value, std::uint32_t expiration,
                   std::uint32_t priority, std::uint8_t mode) noexcept;
    [[nodiscard]] std::uint32_t aggro_priority(
        std::uint8_t a2, std::uint8_t a3, std::uint8_t a4,
        std::uint8_t player1, std::uint8_t player2) const noexcept;
    [[nodiscard]] int evaluate_func3(
        const gameplay::Session& session, std::uint8_t bot_slot,
        const AiContext& context, int func_id,
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
    AiTouchButtons touch_buttons_;
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
