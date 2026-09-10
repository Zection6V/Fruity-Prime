#pragma once

#include "enemy_module.hpp"
#include "Entities/gameplay.hpp"

#include <cstdint>

namespace fruityprime::gameplay {

// Native counterpart of Enemy35Entity.
//
// A Voldrum rolls.  It picks a point inside the cylinder the spawner
// authored, turns towards it over ten frames, speeds up until it is
// halfway there and slows down after, and hops when it arrives -- the hop
// is how it clears whatever stands between it and the next point, not
// decoration.  When a player walks into its cylinder it stops, faces
// them, waits forty frames, and charges.
//
// The class is declared in a header rather than an anonymous namespace
// because Enemy36Entity derives from it: the second Voldrum is written as
// this one plus a gun.
class Enemy35Entity {
public:
    Enemy35Entity(const EnemyScene& scene, EnemyState& agent,
                  net::PlayerState* main) noexcept;
    virtual ~Enemy35Entity() = default;

    Enemy35Entity(const Enemy35Entity&) = delete;
    Enemy35Entity& operator=(const Enemy35Entity&) = delete;

    // Enemy35Entity.EnemyProcess.
    virtual void EnemyProcess();

    // Enemy35Entity.EnemyInitialize, which is Setup, which the second
    // Voldrum overrides outright rather than extending.
    virtual void Setup();

    // The scene's frame count, which UpdateRollSfx reads: the roll is
    // eased on even frames only.
    void set_frame_count(std::uint64_t frames) noexcept {
        frame_count_ = frames;
    }
    // EnemySpawnEntity.Data.SpawnerHealth, which decides whether a
    // Voldrum roams from where it was put or walks to the middle first.
    void set_spawner_health(std::uint16_t health) noexcept {
        spawner_health_ = health;
    }

protected:
    // Enemy35Entity.UpdateRollSfx.
    void UpdateRollSfx(float newAmount, bool grounded);

    // Enemy35Entity.HandleCollision.  The virtual one names the two
    // states that are allowed to count air time rather than bounce; the
    // second Voldrum names different ones.
    virtual bool HandleCollision();
    bool HandleCollision(int stateA, int stateB);

    // Enemy35Entity.PickRoamTarget, UpdateMoveTarget and UpdateSpeed.
    void PickRoamTarget();
    void UpdateMoveTarget(net::Vec3 targetPoint);
    void UpdateSpeed();

    // EnemyInstanceEntity.HandleBlockingCollision and SeekTargetFacing,
    // reached through the scene.  Both are the enemy instance's, not this
    // class's, which is why they are named for what they do rather than
    // for the seam.
    bool Blocking(bool update_speed);
    [[nodiscard]] bool SeekTargetFacing();

    // EnemyInstanceEntity.ContactDamagePlayer and the overlap test it
    // reads.
    [[nodiscard]] bool Touching() const noexcept;
    void ContactDamagePlayer(std::uint32_t damage, bool knockback);

    // The tail both Setups share.
    void StartRoaming();

    const EnemyScene& scene_;
    EnemyState& agent_;
    net::PlayerState* main_;
    std::uint64_t frame_count_ = 0;
    std::uint16_t spawner_health_ = 1;

private:
    void State0();
    void State1();
    void State2();
    void State3();
    void State4();
    void State5();
    void State6();

    [[nodiscard]] bool Behavior00();
    [[nodiscard]] bool Behavior01();
    [[nodiscard]] bool Behavior02();
    [[nodiscard]] bool Behavior03();
    [[nodiscard]] bool Behavior04();
    [[nodiscard]] bool Behavior05();
    [[nodiscard]] bool Behavior06();

    [[nodiscard]] bool CallSubroutine();
    void CallStateProcess();
};

} // namespace fruityprime::gameplay


namespace fruityprime::enemy::module_35_voldrum_2 {

inline constexpr ModuleDescriptor kModule{
    35, "35_Voldrum.cs", "Enemy35Entity",
    PortStatus::ImplementedController, "update_voldrum2"};

// Enemy35Entity.EnemyInitialize.
void EnemyInitialize(gameplay::EnemyState& agent,
                     std::uint16_t spawner_health) noexcept;

} // namespace fruityprime::enemy::module_35_voldrum_2

