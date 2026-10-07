#include "../Entities/Players/DialancheNativeCollision.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <type_traits>

using MphRead::Entities::DialancheNativeCollision;
using OpenTK::Mathematics::Vector3;

namespace
{
    void Check(bool ok, const char* name)
    {
        if (!ok) { std::cerr << "FAIL " << name << '\n'; std::exit(1); }
    }
    bool Equal(DialancheNativeCollision::Pose pose, Vector3 left, Vector3 right)
    {
        return OpenTK::Mathematics::Equal(pose.Left, left) && OpenTK::Mathematics::Equal(pose.Right, right);
    }
}

int main()
{
    static_assert(std::is_trivially_copyable_v<DialancheNativeCollision>);
    static_assert(sizeof(DialancheNativeCollision) <= 104);
    DialancheNativeCollision history;
    const Vector3 initial(1, 2, 3), left(4, 5, 6), right(7, 8, 9), next(10, 11, 12);
    history.Reset(initial);
    Check(Equal(history.PoseForHit(0), initial, initial), "reset before sample");
    Check(Equal(history.PoseForHit(100), initial, initial), "reset at arbitrary tick");
    history.Record(10, left, right);
    Check(Equal(history.PoseForHit(10), initial, initial), "same tick hidden");
    Check(Equal(history.PoseForHit(11), left, right), "next tick visible");
    const auto before = history.PoseForHit(11);
    history.Record(11, next, next);
    Check(Equal(history.PoseForHit(11), before.Left, before.Right), "consumer order invariant");
    Check(Equal(history.PoseForHit(12), next, next), "following tick");
    history.Record(11, right, left);
    Check(Equal(history.PoseForHit(11), left, right), "duplicate record preserves previous");
    Check(Equal(history.PoseForHit(12), right, left), "duplicate updates current sample");
    history.Record(9, next, next);
    Check(Equal(history.PoseForHit(12), right, left), "stale record ignored");
    history.Reset(next);
    Check(Equal(history.PoseForHit(12), next, next), "new attack discards stale samples");
    for (std::uint64_t frame = 0; frame < 100; ++frame)
    {
        Check(DialancheNativeCollision::IsNativeCollisionStep(frame) == (frame != 0 && frame % 2 == 0), "scene phase");
        Check(DialancheNativeCollision::NativeTick(frame) == frame / 2, "native tick");
    }
    Check(!DialancheNativeCollision::IsNativeCollisionStep(std::numeric_limits<std::uint64_t>::max()), "large odd frame");
    std::cout << "PASS Dialanche native phase and two-generation pose history\n";
}
