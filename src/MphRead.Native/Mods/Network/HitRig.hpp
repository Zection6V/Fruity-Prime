#pragma once

#include "../../Formats/Types.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace MphRead
{
    class Scene;
}

namespace MphRead::Entities
{
    class PlayerControls;
    class PlayerEntity;
}

namespace MphRead::Mods::Network
{
    // A scripted pair of clients that stresses hit registration: a shooter
    // and a moving target -- on jump pads, at long range, or a duel.
    class HitRig final
    {
    public:
        HitRig() = delete;

        enum class RigMode
        {
            Off,
            Jump,
            Sniper,
            Duel,
            Volley,
            Dialanche,
            All,
            // TEST WELLS / TEST WELLS STILL / TEST LANES: both players held in
            // place by glass, both shooting, every weapon in turn; Lanes also
            // strafes. What the hit location log (-hitlog) is measured with.
            Wells,
            Lanes
        };

        [[nodiscard]] static ::MphRead::BeamType VolleyWeapon() noexcept { return _volleyWeapon; }
        [[nodiscard]] static RigMode Mode() noexcept { return _mode; }
        [[nodiscard]] static bool Active() noexcept { return _mode != RigMode::Off; }
        [[nodiscard]] static bool Configure(const std::optional<std::string>& value);

        [[nodiscard]] static float AimDeltaX() noexcept { return _aimDeltaX; }
        [[nodiscard]] static float AimDeltaY() noexcept { return _aimDeltaY; }
        [[nodiscard]] static std::int64_t Triggers() noexcept { return _triggers; }
        [[nodiscard]] static std::int64_t FramesOnTarget() noexcept { return _framesOnTarget; }
        [[nodiscard]] static std::int64_t FramesAirborne() noexcept { return _framesAirborne; }
        [[nodiscard]] static double RangeSum() noexcept { return _rangeSum; }
        [[nodiscard]] static std::int64_t RangeSamples() noexcept { return _rangeSamples; }
        [[nodiscard]] static float WorstVerticalSpeed() noexcept { return _worstVerticalSpeed; }
        [[nodiscard]] static double VerticalSpeedSum() noexcept { return _verticalSpeedSum; }
        [[nodiscard]] static std::int64_t VerticalSpeedSamples() noexcept { return _verticalSpeedSamples; }

        static void Reset();
        static void SetScene(Scene* scene) noexcept { _scene = scene; }
        [[nodiscard]] static bool IsSniper();
        static void Drive(Entities::PlayerEntity& player);
        [[nodiscard]] static std::string Describe();

    private:
        static constexpr float CloseRange = 9.0F;
        static constexpr float LongRange = 34.0F;
        static constexpr float VolleyRange = 16.0F;
        static constexpr float HeadAimHeight = 0.95F;
        // Mid-torso: well clear of the 0.80 line where a headshot starts.
        static constexpr float ChestAimHeight = 0.30F;
        // TEST WELLS / TEST LANES: the cells' centres, even slots on the left.
        static constexpr float CellX = 6.0F;
        static constexpr float TurnRate = 6.0F;
        static constexpr float FiringCone = 2.5F;
        // -hitrig all: the shooter holds each weapon this long, then the next.
        static constexpr std::int32_t CycleFrames = 20 * 60;

        static void DriveRunner(Entities::PlayerEntity& player, Entities::PlayerControls& c, Entities::PlayerEntity* other);
        static void DriveDialanche(Entities::PlayerEntity& player, Entities::PlayerControls& c, Entities::PlayerEntity* other);
        static void DrivePadRider(Entities::PlayerEntity& player, Entities::PlayerControls& c, Entities::PlayerEntity* other);
        static void DriveWells(Entities::PlayerEntity& player, Entities::PlayerControls& c, Entities::PlayerEntity* other);
        [[nodiscard]] static ::MphRead::BeamType CycleWeapon() noexcept;
        static void DriveSniper(Entities::PlayerEntity& player, Entities::PlayerControls& c, Entities::PlayerEntity* other);
        static void HoldRange(Entities::PlayerEntity& player, Entities::PlayerControls& c, float range, float want);
        [[nodiscard]] static bool AimAt(Entities::PlayerEntity& player, Entities::PlayerEntity* target, float headHeight);
        [[nodiscard]] static Entities::PlayerEntity* Opponent(Entities::PlayerEntity& self);
        static void Square(Entities::PlayerControls& c, std::int32_t framesPerSide);
        static void ClearControls(Entities::PlayerControls& c);
        static void FinishControls(Entities::PlayerEntity& player, Entities::PlayerControls& c);

        inline static ::MphRead::BeamType _volleyWeapon = ::MphRead::BeamType::Missile;
        inline static RigMode _mode = RigMode::Off;
        inline static std::int32_t _frame = 0;
        inline static std::int32_t _stuckFrames = 0;
        inline static bool _stuckDirection = false;
        inline static OpenTK::Mathematics::Vector3 _lastPosition{};
        inline static float _aimDeltaX = 0;
        inline static float _aimDeltaY = 0;
        inline static std::int64_t _triggers = 0;
        inline static std::int64_t _framesOnTarget = 0;
        inline static std::int64_t _framesAirborne = 0;
        inline static double _rangeSum = 0;
        inline static std::int64_t _rangeSamples = 0;
        inline static float _worstVerticalSpeed = 0;
        inline static double _verticalSpeedSum = 0;
        inline static std::int64_t _verticalSpeedSamples = 0;
        inline static std::vector<bool> _wasDown{};
        inline static std::int64_t _padLaunches = 0;
        inline static std::int64_t _placements = 0;
        inline static float _highest = 0;
        inline static bool _wasAirborne = false;
        inline static Scene* _scene = nullptr;
    };

    // HitRig.RigMode.ToString().
    [[nodiscard]] std::string ToString(HitRig::RigMode value);
}
