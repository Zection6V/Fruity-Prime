#include "Mods/Render/MotionCarry.hpp"
#include "Formats/Types.hpp"

#include <cmath>
#include <iostream>

using MphRead::Mods::Render::MotionCarry;
using OpenTK::Mathematics::Matrix4;
using OpenTK::Mathematics::Vector3;
using OpenTK::Mathematics::Vector4;

namespace
{
    bool Check(bool condition, const char* message)
    {
        if (!condition) std::cerr << "FAIL: " << message << '\n';
        return condition;
    }

    // A pose turned about Y by `degrees`, scaled, at `at`: row-vector, like the engine's.
    Matrix4 Pose(float degrees, Vector3 at, float scale = 1.0F)
    {
        const float r = degrees * 3.14159265358979323846F / 180.0F;
        const float c = std::cos(r), s = std::sin(r);
        return Matrix4(Vector4(c * scale, 0, -s * scale, 0), Vector4(0, scale, 0, 0),
            Vector4(s * scale, 0, c * scale, 0), Vector4(at.X, at.Y, at.Z, 1));
    }

    float Largest(const Matrix4& a, const Matrix4& b)
    {
        const float* x = &a.M11;
        const float* y = &b.M11;
        float worst = 0;
        for (int i = 0; i < 16; ++i) worst = std::fmax(worst, std::fabs(x[i] - y[i]));
        return worst;
    }
}

int main()
{
    bool passed = true;

    // A steady motion: one step moved by v and turned by 10 degrees. Half a
    // step on, the pose is the one the motion has there.
    {
        const Matrix4 previous = Pose(20, Vector3(1, 2, 3));
        const Matrix4 current = Pose(30, Vector3(1.3F, 2, 2.8F));
        const auto carry = MotionCarry::Between(previous, current, 0.5F);
        passed &= Check(carry.has_value(), "a steady motion is carried");
        if (carry)
        {
            const Matrix4 drawn = current * *carry;
            passed &= Check(Largest(drawn, Pose(35, Vector3(1.45F, 2, 2.7F))) < 1e-4F,
                "half a step on, the pose is where the motion takes it");
        }
    }
    // A scaled pose keeps its scale, and turns as the step turned it.
    {
        const Matrix4 previous = Pose(-40, Vector3(0, 0, 0), 2.0F);
        const Matrix4 current = Pose(-36, Vector3(0.1F, 0, 0), 2.0F);
        const auto carry = MotionCarry::Between(previous, current, 0.25F);
        passed &= Check(carry && Largest(current * *carry, Pose(-35, Vector3(0.125F, 0, 0), 2.0F)) < 1e-4F,
            "a scaled pose is carried, scale kept");
    }
    // A change of scale is not a turn: only the position is carried.
    {
        const Matrix4 previous = Pose(0, Vector3(0, 0, 0), 1.0F);
        const Matrix4 current = Pose(0, Vector3(1, 0, 0), 1.5F);
        const auto carry = MotionCarry::Between(previous, current, 0.5F);
        passed &= Check(carry && Largest(current * *carry, Pose(0, Vector3(1.5F, 0, 0), 1.5F)) < 1e-4F,
            "a rescaled pose only moves");
    }
    // Jumps and stillness are not carried; nor is a picture at the step.
    passed &= Check(!MotionCarry::Between(Pose(0, Vector3(0, 0, 0)), Pose(0, Vector3(5, 0, 0)), 0.5F),
        "a jump further than a step's motion is not carried");
    passed &= Check(!MotionCarry::Between(Pose(10, Vector3(1, 1, 1)), Pose(10, Vector3(1, 1, 1)), 0.5F),
        "a still pose is not carried");
    passed &= Check(!MotionCarry::Between(Pose(0, Vector3(0, 0, 0)), Pose(0, Vector3(1, 0, 0)), 0.0F),
        "the step's own instant is the step's pose");
    // A turn too large for a step keeps the turn and carries the position.
    {
        const auto carry = MotionCarry::Between(Pose(0, Vector3(0, 0, 0)), Pose(170, Vector3(1, 0, 0)), 0.5F);
        passed &= Check(carry && Largest(Pose(170, Vector3(1, 0, 0)) * *carry, Pose(170, Vector3(1.5F, 0, 0))) < 1e-4F,
            "a turn no step makes is not carried");
    }

    // Toward a pose the game says comes next: part of the way there, never past.
    {
        const Matrix4 current = Pose(30, Vector3(1, 0, 0));
        const Matrix4 next = Pose(40, Vector3(1.2F, 0, 0.4F));
        const auto carry = MotionCarry::Toward(current, next, 0.25F);
        passed &= Check(carry && Largest(current * *carry, Pose(32.5F, Vector3(1.05F, 0, 0.1F))) < 1e-4F,
            "a quarter of the way toward the next pose");
        passed &= Check(!MotionCarry::Toward(current, current, 0.5F), "toward the same pose is not a carry");
    }

    // Drawn items: a transform, a matrix stack, and a billboard that only moves.
    {
        const auto carry = MotionCarry::Between(Pose(0, Vector3(0, 0, 0)), Pose(10, Vector3(1, 0, 0)), 1.0F);
        MphRead::RenderItem item;
        item.Transform = Pose(10, Vector3(1, 0, 0));
        MotionCarry::Apply(item, *carry);
        passed &= Check(Largest(item.Transform, Pose(20, Vector3(2, 0, 0))) < 1e-4F, "an item's transform is carried");

        MphRead::RenderItem billboard;
        billboard.BillboardMode = MphRead::BillboardMode::Sphere;
        billboard.Transform = Pose(10, Vector3(1, 0, 0));
        MotionCarry::Apply(billboard, *carry);
        passed &= Check(Largest(billboard.Transform, Pose(10, Vector3(2, 0, 0))) < 1e-4F,
            "a billboard moves without turning");

        MphRead::RenderItem skinned;
        skinned.MatrixStackCount = 1;
        const Matrix4 bone = Pose(10, Vector3(1, 0, 0));
        const float* values = &bone.M11;
        for (std::size_t i = 0; i < 16U; ++i) (*skinned.MatrixStack)[i] = values[i];
        MotionCarry::Apply(skinned, *carry);
        Matrix4 moved{};
        float* out = &moved.M11;
        for (std::size_t i = 0; i < 16U; ++i) out[i] = (*skinned.MatrixStack)[i];
        passed &= Check(Largest(moved, Pose(20, Vector3(2, 0, 0))) < 1e-4F, "an item's matrix stack is carried");
    }

    if (!passed) return 1;
    std::cout << "Motion carry checks passed.\n";
    return 0;
}
