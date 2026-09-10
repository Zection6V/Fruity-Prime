#pragma once

// Native counterpart of the EnemyNNSubroutines tables in Metadata/Enemies.cs.
//
// Each enemy is a small state machine: a state is an ordered list of
// behaviours, and the first behaviour whose predicate passes moves the enemy
// to that entry's next state.  The predicates are the per-enemy Behavior
// methods, which the native controllers implement; what is transliterated
// here is the graph -- the transition target and which behaviour to run --
// because that is the part that would otherwise be retyped by hand.
//
// One table spells a state's array with an inferred length rather than an
// explicit one; `count` is taken from what the initialiser actually holds, so
// the two spellings produce the same state.

#include <span>
#include <functional>
#include <algorithm>
#include <array>
#include <cstdint>

namespace fruityprime::metadata {

// EnemyBehavior<T>: the next state, and the index of the Behavior method to
// evaluate (Behavior07 is index 7).
struct EnemyBehaviorEntry {
    std::uint8_t next_state = 0;
    std::uint8_t behavior = 0;
};

// EnemySubroutine<T>: one state's behaviour list.  `count` is how many of
// `behaviors` are used.
struct EnemySubroutine {
    std::array<EnemyBehaviorEntry, 8> behaviors{};
    std::uint8_t count = 0;
};

// EnemyInstanceEntity.CallSubroutine, as a free function.
//
// The first behaviour in this state's list whose predicate passes names
// the next state -- first, not best, and not last, so the order in the
// table is load-bearing and nothing past the one that passed is asked.
// A state with an empty list, or one where none pass, leaves the enemy
// where it is rather than falling through to the first entry.
//
// `next_state` is the managed _state2: what the enemy will be doing on
// the next frame, not this one.
[[nodiscard]] inline bool call_subroutine(
    std::span<const EnemySubroutine> subroutines, std::uint8_t sub_id,
    std::uint8_t& next_state,
    const std::function<bool(std::uint8_t)>& behavior) noexcept {
    if (!behavior || sub_id >= subroutines.size()) {
        return false;
    }
    const auto& subroutine = subroutines[sub_id];
    const std::size_t count = std::min<std::size_t>(
        subroutine.count, subroutine.behaviors.size());
    for (std::size_t index = 0; index < count; ++index) {
        const auto& entry = subroutine.behaviors[index];
        if (!behavior(entry.behavior)) {
            continue;
        }
        next_state = entry.next_state;
        return true;
    }
    return false;
}

// Metadata.Enemy00Subroutines -- 7 states
inline constexpr std::array<EnemySubroutine, 7> Enemy00Subroutines{{
    // state 0
    {{{{0, 2}, {1, 3}}}, 2},
    // state 1
    {{{{1, 2}, {2, 6}, {6, 7}, {6, 8}}}, 4},
    // state 2
    {{{{3, 9}, {6, 7}, {6, 10}, {6, 8}}}, 4},
    // state 3
    {{{{4, 0}}}, 1},
    // state 4
    {{{{5, 4}, {5, 5}}}, 2},
    // state 5
    {{{{1, 1}}}, 1},
    // state 6
    {{{{0, 2}, {1, 3}}}, 2}
}};

// Metadata.Enemy02Subroutines -- 11 states
inline constexpr std::array<EnemySubroutine, 11> Enemy02Subroutines{{
    // state 0
    {{{{2, 4}, {1, 5}}}, 2},
    // state 1
    {{{{7, 9}, {7, 10}, {10, 11}, {2, 12}}}, 4},
    // state 2
    {{{{3, 0}}}, 1},
    // state 3
    {{{{4, 0}}}, 1},
    // state 4
    {{{{5, 0}}}, 1},
    // state 5
    {{{{1, 2}, {8, 3}}}, 2},
    // state 6
    {{{{9, 0}}}, 1},
    // state 7
    {{{{1, 4}, {1, 5}, {0, 6}}}, 3},
    // state 8
    {{{{6, 7}, {6, 2}, {6, 8}}}, 3},
    // state 9
    {{{{7, 0}}}, 1},
    // state 10
    {{{{7, 1}}}, 1}
}};

// Metadata.Enemy03Subroutines -- 3 states
inline constexpr std::array<EnemySubroutine, 3> Enemy03Subroutines{{
    // state 0
    {{{{1, 2}}}, 1},
    // state 1
    {{{{2, 1}}}, 1},
    // state 2
    {{{{0, 0}}}, 1}
}};

// Metadata.Enemy04Subroutines -- 2 states
inline constexpr std::array<EnemySubroutine, 2> Enemy04Subroutines{{
    // state 0
    {{{{1, 1}}}, 1},
    // state 1
    {{{{1, 0}}}, 1}
}};

// Metadata.Enemy05Subroutines -- 2 states
inline constexpr std::array<EnemySubroutine, 2> Enemy05Subroutines{{
    // state 0
    {{{{1, 1}}}, 1},
    // state 1
    {{{{1, 0}}}, 1}
}};

// Metadata.Enemy06Subroutines -- 5 states
inline constexpr std::array<EnemySubroutine, 5> Enemy06Subroutines{{
    // state 0
    {{{{1, 2}, {2, 0}}}, 2},
    // state 1
    {{{{2, 0}}}, 1},
    // state 2
    {{{{3, 3}, {4, 1}}}, 2},
    // state 3
    {{{{4, 1}}}, 1},
    // state 4
    {{{{1, 2}, {2, 0}}}, 2}
}};

// Metadata.Enemy10Subroutines -- 6 states
inline constexpr std::array<EnemySubroutine, 6> Enemy10Subroutines{{
    // state 0
    {{{{0, 1}, {1, 3}}}, 2},
    // state 1
    {{{{1, 1}, {2, 6}, {4, 7}, {4, 8}, {4, 9}}}, 5},
    // state 2
    {{{{3, 0}}}, 1},
    // state 3
    {{{{4, 4}, {2, 5}}}, 2},
    // state 4
    {{{{0, 1}}}, 1},
    // state 5
    {{{{1, 2}}}, 1}
}};

// Metadata.Enemy11Subroutines -- 5 states
inline constexpr std::array<EnemySubroutine, 5> Enemy11Subroutines{{
    // state 0
    {{{{1, 4}}}, 1},
    // state 1
    {{{{2, 3}}}, 1},
    // state 2
    {{{{3, 2}}}, 1},
    // state 3
    {{{{4, 1}}}, 1},
    // state 4
    {{{{0, 0}}}, 1}
}};

// Metadata.Enemy16Subroutines -- 4 states
inline constexpr std::array<EnemySubroutine, 4> Enemy16Subroutines{{
    // state 0
    {{{{1, 6}, {2, 5}, {3, 3}}}, 3},
    // state 1
    {{{{0, 4}, {2, 5}, {3, 3}}}, 3},
    // state 2
    {{{{0, 2}, {3, 3}}}, 2},
    // state 3
    {{{{3, 0}, {0, 1}}}, 2}
}};

// Metadata.Enemy18Subroutines -- 5 states
inline constexpr std::array<EnemySubroutine, 5> Enemy18Subroutines{{
    // state 0
    {{{{1, 0}}}, 1},
    // state 1
    {{{{2, 4}, {4, 2}}}, 2},
    // state 2
    {{{{3, 5}, {4, 2}}}, 2},
    // state 3
    {{{{4, 2}, {2, 3}}}, 2},
    // state 4
    {{{{0, 1}}}, 1}
}};

// Metadata.Enemy19Subroutines -- 27 states
inline constexpr std::array<EnemySubroutine, 27> Enemy19Subroutines{{
    // state 0
    {{{{1, 5}}}, 1},
    // state 1
    {{{{4, 2}}}, 1},
    // state 2
    {{{{3, 7}, {4, 2}}}, 2},
    // state 3
    {{{{1, 8}, {4, 2}}}, 2},
    // state 4
    {{{{5, 4}}}, 1},
    // state 5
    {{{{7, 9}, {9, 10}, {6, 12}}}, 3},
    // state 6
    {{{{7, 9}, {9, 10}, {5, 11}}}, 3},
    // state 7
    {{{{8, 3}}}, 1},
    // state 8
    {{{{10, 0}}}, 1},
    // state 9
    {{{{1, 6}}}, 1},
    // state 10
    {{{{13, 2}}}, 1},
    // state 11
    {{{{13, 2}}}, 1},
    // state 12
    {{{{13, 2}}}, 1},
    // state 13
    {{{{14, 4}}}, 1},
    // state 14
    {{{{16, 9}, {18, 10}, {15, 12}}}, 3},
    // state 15
    {{{{16, 9}, {18, 10}, {14, 12}}}, 3},
    // state 16
    {{{{17, 3}}}, 1},
    // state 17
    {{{{19, 0}}}, 1},
    // state 18
    {{{{10, 6}}}, 1},
    // state 19
    {{{{22, 2}}}, 1},
    // state 20
    {{{{22, 2}}}, 1},
    // state 21
    {{{{22, 2}}}, 1},
    // state 22
    {{{{23, 4}}}, 1},
    // state 23
    {{{{23, 9}, {25, 10}, {24, 12}}}, 3},
    // state 24
    {{{{23, 9}, {25, 10}, {23, 12}}}, 3},
    // state 25
    {{{{19, 6}}}, 1},
    // state 26
    {{{{23, 1}}}, 1}
}};

// Metadata.Enemy23Subroutines -- 11 states
inline constexpr std::array<EnemySubroutine, 11> Enemy23Subroutines{{
    // state 0
    {{{{1, 1}, {2, 7}}}, 2},
    // state 1
    {{{{0, 5}}}, 1},
    // state 2
    {{{{3, 8}, {1, 9}, {8, 10}}}, 3},
    // state 3
    {{{{5, 11}, {1, 9}, {8, 10}, {8, 12}}}, 4},
    // state 4
    {{{{6, 6}}}, 1},
    // state 5
    {{{{4, 0}}}, 1},
    // state 6
    {{{{2, 3}}}, 1},
    // state 7
    {{{{1, 1}}}, 1},
    // state 8
    {{{{7, 4}}}, 1},
    // state 9
    {{{{10, 4}}}, 1},
    // state 10
    {{{{1, 2}}}, 1}
}};

// Metadata.Enemy24Subroutines -- 15 states
inline constexpr std::array<EnemySubroutine, 15> Enemy24Subroutines{{
    // state 0
    {{{{1, 0}}}, 1},
    // state 1
    {{{{3, 11}, {2, 18}, {9, 4}, {8, 9}, {12, 10}, {4, 19}, {10, 20}, {7, 21}}}, 8},
    // state 2
    {{{{9, 4}, {3, 11}, {8, 9}, {12, 10}, {4, 22}, {10, 20}, {7, 21}, {4, 7}}}, 8},
    // state 3
    {{{{14, 7}, {14, 8}, {8, 9}, {12, 10}}}, 4},
    // state 4
    {{{{9, 4}, {3, 11}, {8, 9}, {12, 10}, {14, 12}}}, 5},
    // state 5
    {{{{9, 4}, {3, 13}, {8, 14}, {12, 10}, {1, 15}, {14, 16}, {11, 17}}}, 7},
    // state 6
    {{{{9, 4}, {5, 5}}}, 2},
    // state 7
    {{{{9, 4}, {1, 0}}}, 2},
    // state 8
    {{{{14, 3}}}, 1},
    // state 9
    {{{{13, 6}, {9, 4}, {1, 0}}}, 3},
    // state 10
    {{{{9, 4}, {1, 0}}}, 2},
    // state 11
    {{{{14, 3}}}, 1},
    // state 12
    {{{{2, 0}}}, 1},
    // state 13
    {{{{0, 1}}}, 1},
    // state 14
    {{{{14, 2}}}, 1}
}};

// Metadata.Enemy28Subroutines -- 14 states
inline constexpr std::array<EnemySubroutine, 14> Enemy28Subroutines{{
    // state 0
    {{{}}, 0},
    // state 1
    {{{{2, 1}}}, 1},
    // state 2
    {{{{13, 2}, {12, 3}, {3, 13}, {4, 14}}}, 4},
    // state 3
    {{{{13, 2}, {12, 3}, {4, 14}, {2, 15}}}, 4},
    // state 4
    {{{{13, 2}, {12, 3}, {5, 4}}}, 3},
    // state 5
    {{{{13, 2}, {12, 3}, {11, 6}, {6, 7}}}, 4},
    // state 6
    {{{{13, 2}, {12, 3}, {10, 8}, {7, 9}}}, 4},
    // state 7
    {{{{13, 2}, {12, 3}, {10, 8}, {8, 10}}}, 4},
    // state 8
    {{{{13, 2}, {12, 3}, {10, 8}, {9, 11}}}, 4},
    // state 9
    {{{{13, 2}, {12, 3}, {10, 8}, {11, 12}}}, 4},
    // state 10
    {{{{11, 0}}}, 1},
    // state 11
    {{{{12, 2}, {12, 3}, {2, 5}}}, 3},
    // state 12
    {{{{0, 0}}}, 1},
    // state 13
    {{{{2, 0}}}, 1}
}};

// Metadata.Enemy31Subroutines -- 19 states
inline constexpr std::array<EnemySubroutine, 19> Enemy31Subroutines{{
    // state 0
    {{{}}, 0},
    // state 1
    {{{{17, 2}, {14, 4}, {15, 5}, {16, 6}, {2, 12}, {9, 13}, {6, 14}}}, 7},
    // state 2
    {{{{17, 2}, {14, 4}, {15, 5}, {16, 6}, {3, 11}, {9, 13}, {6, 14}, {7, 17}}}, 8},
    // state 3
    {{{{17, 2}, {14, 4}, {15, 5}, {16, 6}, {9, 13}, {6, 14}, {4, 7}}}, 7},
    // state 4
    {{{{17, 2}, {14, 4}, {15, 5}, {16, 6}, {9, 13}, {7, 9}, {5, 10}}}, 7},
    // state 5
    {{{{1, 0}}}, 1},
    // state 6
    {{{{17, 2}}}, 1},
    // state 7
    {{{{17, 2}, {18, 3}}}, 2},
    // state 8
    {{{{17, 2}, {14, 4}, {15, 5}, {16, 6}, {11, 15}, {6, 14}, {1, 16}}}, 7},
    // state 9
    {{{}}, 0},
    // state 10
    {{{{17, 2}, {14, 4}, {15, 5}, {16, 6}, {11, 11}, {8, 8}}}, 6},
    // state 11
    {{{{17, 2}, {14, 4}, {15, 5}, {16, 6}, {12, 7}, {8, 8}}}, 6},
    // state 12
    {{{{17, 2}, {14, 4}, {15, 5}, {16, 6}, {7, 9}, {13, 10}}}, 6},
    // state 13
    {{{{8, 0}}}, 1},
    // state 14
    {{{{17, 2}}}, 1},
    // state 15
    {{{{17, 2}, {18, 0}}}, 2},
    // state 16
    {{{}}, 0},
    // state 17
    {{{{17, 0}}}, 1},
    // state 18
    {{{{18, 1}}}, 1}
}};

// Metadata.Enemy33Subroutines -- 4 states
inline constexpr std::array<EnemySubroutine, 4> Enemy33Subroutines{{
    // state 0
    {{{{3, 2}, {2, 3}}}, 2},
    // state 1
    {{{{0, 1}}}, 1},
    // state 2
    {{{{3, 0}}}, 1},
    // state 3
    {{{{0, 1}}}, 1}
}};

// Metadata.Enemy35Subroutines -- 7 states
inline constexpr std::array<EnemySubroutine, 7> Enemy35Subroutines{{
    // state 0
    {{{{1, 2}, {2, 6}}}, 2},
    // state 1
    {{{{0, 0}}}, 1},
    // state 2
    {{{{3, 3}}}, 1},
    // state 3
    {{{{4, 4}, {1, 5}}}, 2},
    // state 4
    {{{{5, 1}}}, 1},
    // state 5
    {{{{6, 0}}}, 1},
    // state 6
    {{{{1, 2}}}, 1}
}};

// Metadata.Enemy36Subroutines -- 6 states
inline constexpr std::array<EnemySubroutine, 6> Enemy36Subroutines{{
    // state 0
    {{{{1, 3}, {2, 5}, {1, 4}}}, 3},
    // state 1
    {{{{0, 0}}}, 1},
    // state 2
    {{{{3, 1}}}, 1},
    // state 3
    {{{{4, 2}}}, 1},
    // state 4
    {{{{5, 0}}}, 1},
    // state 5
    {{{{1, 3}, {1, 4}}}, 2}
}};

// Metadata.Enemy38Subroutines -- 17 states
inline constexpr std::array<EnemySubroutine, 17> Enemy38Subroutines{{
    // state 0
    {{{{1, 5}}}, 1},
    // state 1
    {{{{2, 1}}}, 1},
    // state 2
    {{{{4, 13}}}, 1},
    // state 3
    {{{{5, 14}, {6, 15}, {4, 16}}}, 3},
    // state 4
    {{{{3, 3}}}, 1},
    // state 5
    {{{{12, 11}}}, 1},
    // state 6
    {{{{7, 10}}}, 1},
    // state 7
    {{{{8, 9}}}, 1},
    // state 8
    {{{{9, 8}}}, 1},
    // state 9
    {{{{10, 0}}}, 1},
    // state 10
    {{{{11, 6}}}, 1},
    // state 11
    {{{{12, 12}}}, 1},
    // state 12
    {{{{15, 17}, {13, 16}, {14, 18}}}, 3},
    // state 13
    {{{{12, 3}}}, 1},
    // state 14
    {{{{3, 7}}}, 1},
    // state 15
    {{{{16, 4}}}, 1},
    // state 16
    {{{{0, 2}}}, 1}
}};

// Metadata.Enemy39Subroutines -- 6 states
inline constexpr std::array<EnemySubroutine, 6> Enemy39Subroutines{{
    // state 0
    {{{{1, 0}}}, 1},
    // state 1
    {{{{2, 4}}}, 1},
    // state 2
    {{{{3, 3}}}, 1},
    // state 3
    {{{{4, 1}}}, 1},
    // state 4
    {{{{5, 5}, {5, 6}}}, 2},
    // state 5
    {{{{0, 2}}}, 1}
}};

// Metadata.Enemy45Subroutines -- 4 states
inline constexpr std::array<EnemySubroutine, 4> Enemy45Subroutines{{
    // state 0
    {{{{1, 0}}}, 1},
    // state 1
    {{{{2, 4}, {0, 2}}}, 2},
    // state 2
    {{{{0, 2}, {1, 3}}}, 2},
    // state 3
    {{{{0, 1}}}, 1}
}};

// Metadata.Enemy46Subroutines -- 20 states
inline constexpr std::array<EnemySubroutine, 20> Enemy46Subroutines{{
    // state 0
    {{{{1, 2}}}, 1},
    // state 1
    {{{{0, 14}, {2, 15}}}, 2},
    // state 2
    {{{{4, 5}}}, 1},
    // state 3
    {{{{19, 16}, {8, 18}, {5, 24}, {12, 25}}}, 4},
    // state 4
    {{{{3, 1}}}, 1},
    // state 5
    {{{{19, 16}, {6, 23}, {8, 18}}}, 3},
    // state 6
    {{{{19, 16}, {7, 17}, {8, 18}}}, 3},
    // state 7
    {{{{9, 5}, {8, 11}}}, 2},
    // state 8
    {{{{19, 16}, {3, 21}, {19, 22}}}, 3},
    // state 9
    {{{{10, 9}}}, 1},
    // state 10
    {{{{11, 6}}}, 1},
    // state 11
    {{{{3, 3}}}, 1},
    // state 12
    {{{{13, 0}}}, 1},
    // state 13
    {{{{14, 4}}}, 1},
    // state 14
    {{{{15, 19}, {11, 13}, {16, 20}}}, 3},
    // state 15
    {{{{18, 10}}}, 1},
    // state 16
    {{{{17, 8}}}, 1},
    // state 17
    {{{{15, 0}}}, 1},
    // state 18
    {{{{14, 12}, {11, 13}}}, 2},
    // state 19
    {{{{3, 7}}}, 1}
}};

// Metadata.Enemy47Subroutines -- 20 states
inline constexpr std::array<EnemySubroutine, 20> Enemy47Subroutines{{
    // state 0
    {{{{1, 2}}}, 1},
    // state 1
    {{{{0, 14}, {2, 15}}}, 2},
    // state 2
    {{{{4, 5}}}, 1},
    // state 3
    {{{{19, 16}, {8, 18}, {5, 24}, {12, 25}}}, 4},
    // state 4
    {{{{3, 1}}}, 1},
    // state 5
    {{{{19, 16}, {6, 23}, {8, 18}}}, 3},
    // state 6
    {{{{19, 16}, {7, 17}, {8, 18}}}, 3},
    // state 7
    {{{{9, 5}, {8, 11}}}, 2},
    // state 8
    {{{{19, 16}, {3, 21}, {19, 22}}}, 3},
    // state 9
    {{{{10, 9}}}, 1},
    // state 10
    {{{{11, 6}}}, 1},
    // state 11
    {{{{3, 3}}}, 1},
    // state 12
    {{{{13, 0}}}, 1},
    // state 13
    {{{{14, 4}}}, 1},
    // state 14
    {{{{15, 19}, {11, 13}, {16, 20}}}, 3},
    // state 15
    {{{{18, 10}}}, 1},
    // state 16
    {{{{17, 8}}}, 1},
    // state 17
    {{{{15, 0}}}, 1},
    // state 18
    {{{{14, 12}, {11, 13}}}, 2},
    // state 19
    {{{{3, 7}}}, 1}
}};

} // namespace fruityprime::metadata
