#pragma once

#include "enemy_module.hpp"
#include "Entities/gameplay.hpp"

#include <cstdint>

namespace fruityprime::gameplay {

// Native counterpart of Enemy46Entity.
//
// An Ithrak hangs from the ceiling and does nothing at all until a player
// comes inside the volume it watches.  Then it warns, drops, and walks --
// and from there it is a creature of distances.  Between three and a half
// and five units it stops and screams; between one and a half and two it
// gathers and lunges; in contact it bites; and if you get inside a unit
// and a half it hops backwards rather than letting you stand on it.
//
// Almost every state waits on an animation ending rather than on a timer,
// which is why the animation cursor is state rather than decoration: the
// pauses between an Ithrak's moves are the animations themselves.
//
// The class is in a header because Enemy47Entity derives from it.
class Enemy46Entity {
public:
    Enemy46Entity(const EnemyScene& scene, EnemyState& agent,
                  net::PlayerState* main) noexcept;
    virtual ~Enemy46Entity() = default;

    Enemy46Entity(const Enemy46Entity&) = delete;
    Enemy46Entity& operator=(const Enemy46Entity&) = delete;

    // Enemy46Entity.EnemyInitialize, which for the Greater Ithrak reads a
    // different record and a different effectiveness.
    virtual void EnemyInitialize();

    // Enemy46Entity.EnemyProcess.
    void EnemyProcess();

    // Enemy46Entity.EnemyTakeDamage: an Ithrak's hit zone dies with it.
    [[nodiscard]] bool EnemyTakeDamage();

    void set_frame_count(std::uint64_t frames) noexcept {
        frame_count_ = frames;
    }

protected:
    // Enemy46Entity.Setup: the half both Ithraks share, with the four
    // volumes and the effectiveness handed in because they are the only
    // things that differ.
    void Setup(std::uint32_t effectiveness);

    // Enemy46Entity.CallSubroutine, virtual because the two read
    // different subroutine tables.
    virtual bool CallSubroutine();

    // Enemy46Entity.UpdateMouthMaterial.  A Greater Ithrak's mouth is
    // darker, which is the only thing you can see about it at a glance.
    virtual void UpdateMouthMaterial();

    // Enemy46Entity.SetNodeAnim, SpawnHitZone, PickMoveTarget,
    // HandleCollision, StartRecoil and AnimEnded.
    void SetNodeAnim(std::uint8_t id, bool no_loop = false);
    void SpawnHitZone() const;
    void PickMoveTarget(const scene::EntityVolume& volume);
    [[nodiscard]] bool HandleCollision(net::Vec3 testPos);
    void StartRecoil();
    [[nodiscard]] bool AnimEnded() const noexcept;

    [[nodiscard]] bool Behavior00();
    [[nodiscard]] bool Behavior01();
    [[nodiscard]] bool Behavior02();
    [[nodiscard]] bool Behavior03();
    [[nodiscard]] bool Behavior04();
    [[nodiscard]] bool Behavior05();
    [[nodiscard]] bool Behavior06();
    [[nodiscard]] bool Behavior07();
    [[nodiscard]] bool Behavior08();
    [[nodiscard]] bool Behavior09();
    [[nodiscard]] bool Behavior10();
    [[nodiscard]] bool Behavior11();
    [[nodiscard]] bool Behavior12();
    [[nodiscard]] bool Behavior13();
    [[nodiscard]] bool Behavior14();
    [[nodiscard]] bool Behavior15();
    [[nodiscard]] bool Behavior16();
    [[nodiscard]] bool Behavior17();
    [[nodiscard]] bool Behavior18();
    [[nodiscard]] bool Behavior19();
    [[nodiscard]] bool Behavior20();
    [[nodiscard]] bool Behavior21();
    [[nodiscard]] bool Behavior22();
    [[nodiscard]] bool Behavior23();
    [[nodiscard]] bool Behavior24();
    [[nodiscard]] bool Behavior25();

    [[nodiscard]] bool RunBehavior(std::uint8_t index);

    const EnemyScene& scene_;
    EnemyState& agent_;
    net::PlayerState* main_;
    std::uint64_t frame_count_ = 0;

private:
    void State00();
    void State01();
    void State02();
    void State03();
    void State04();
    void State05();
    void State06();
    void State07();
    void State08();
    void State09();
    void State10();
    void State11();
    void State12();
    void State13();
    void State14();
    void State15();
    void State16();
    void State17();
    void State18();
    void State19();

    void CallStateProcess();
    void AdvanceAnimation();
    void StopHorizontal();
    [[nodiscard]] bool SeekTargetFacing();
    void ContactDamagePlayer(std::uint32_t damage, bool knockback) const;
    [[nodiscard]] net::Vec3 FlatToPlayer() const;
    void BeginTurn(net::Vec3 target);
};

} // namespace fruityprime::gameplay

namespace fruityprime::enemy::module_46_lesser_ithrak {

inline constexpr ModuleDescriptor kModule{
    46, "46_LesserIthrak.cs", "Enemy46Entity",
    PortStatus::ImplementedController, "update_lesser_ithrak"};

// Enemy46Entity.EnemyInitialize.
void EnemyInitialize(gameplay::EnemyState& agent) noexcept;

} // namespace fruityprime::enemy::module_46_lesser_ithrak

