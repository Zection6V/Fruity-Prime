// EnemyInstanceEntity's state machine, which every enemy in the cartridge
// is built on.
//
// A state is an ordered list of behaviours.  The first whose predicate
// passes names the next state -- first, not best, and not last, which is
// why the order in the table is load-bearing.  The change is decided now
// and taken on the next frame, so a behaviour that sets the next state
// does not see it until the frame after.  Both are easy to get subtly
// wrong in a way that still produces an enemy that moves.
#include "Entities/runtime_entities.hpp"
#include "Entities/Enemies/30_Trocra.hpp"
#include "Entities/gameplay.hpp"

#include <cstdio>
#include <vector>

int main() {
    using fruityprime::runtime::EnemyInstanceEntity;
    namespace metadata = fruityprime::metadata;

    int failures = 0;
    const auto check = [&failures](bool condition, const char* what) {
        if (!condition) {
            std::printf("native enemy state machine: %s\n", what);
            ++failures;
        }
    };

    // Crash Pillar's own graph.  State 3 is the interesting one: three
    // behaviours, going to states 5, 6 and 4.
    const auto& table = metadata::Enemy38Subroutines;
    const std::span<const metadata::EnemySubroutine> subroutines{table};

    EnemyInstanceEntity enemy(1, 38, {0.0F, 0.0F, 0.0F});
    enemy.set_states(3, 3);
    check(enemy.state_a() == 3 && enemy.sub_id() == 3,
          "setting the state did not follow through to the subroutine");

    // Nothing passes: the enemy stays where it is rather than falling
    // through to the first entry.
    check(!enemy.CallSubroutine(subroutines,
                                [](std::uint8_t) { return false; }),
          "a state with no passing behaviour reported a transition");
    check(enemy.state_b() == 3, "a failed subroutine moved the enemy");

    // The first passing behaviour wins even when a later one would too.
    std::vector<std::uint8_t> asked;
    check(enemy.CallSubroutine(subroutines, [&asked](std::uint8_t index) {
              asked.push_back(index);
              return index == 15 || index == 16;
          }),
          "a passing behaviour was not noticed");
    check(enemy.state_b() == 6,
          "the second behaviour did not win over the third");
    check(asked.size() == 2 && asked[0] == 14 && asked[1] == 15,
          "the behaviours were not asked in table order, or asked past the "
          "one that passed");

    // Decided, but not taken until the frame advances.
    check(enemy.state_a() == 3, "the transition was taken immediately");
    enemy.AdvanceState();
    check(enemy.state_a() == 6 && enemy.sub_id() == 6,
          "the frame advance did not take the decided transition");

    // A single-entry state, and one whose predicate refuses.
    enemy.set_states(0, 0);
    check(enemy.CallSubroutine(subroutines,
                               [](std::uint8_t index) { return index == 5; }),
          "state zero's only behaviour did not fire");
    check(enemy.state_b() == 1, "state zero went somewhere unexpected");

    // A state index past the table, and a predicate that was never given.
    enemy.set_states(200, 200);
    check(!enemy.CallSubroutine(subroutines,
                                [](std::uint8_t) { return true; }),
          "a state outside the table was dispatched anyway");
    enemy.set_states(0, 0);
    check(!enemy.CallSubroutine(subroutines, {}),
          "a missing predicate was called");

    // CallStateProcess runs the current state's own method, not the next.
    enemy.set_states(4, 9);
    int ran = -1;
    enemy.CallStateProcess([&ran](std::uint8_t index) {
        ran = static_cast<int>(index);
    });
    check(ran == 4, "the state process ran the wrong state");

    if (failures == 0) {
        std::printf("native enemy state machine tests passed\n");
    }
    return failures == 0 ? 0 : 1;
}
