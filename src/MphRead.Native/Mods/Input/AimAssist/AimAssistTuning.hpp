#pragma once

#include <cstdint>

namespace MphRead::Mods::Input::AimAssist
{
    class AimAssistTuning final
    {
    public:
        AimAssistTuning() = delete;

        static constexpr float AcquireCone = 7;
        static constexpr float ReleaseCone = 9;
        static constexpr float InnerCone = 2.4F;
        static constexpr float MinimumFriction = .62F;
        static constexpr float HorizontalRotation = .24F;
        static constexpr float VerticalRotation = .15F;
        static constexpr float ChallengerRatio = 1.30F;
        static constexpr float HeadDelay = .120F;
        static constexpr float MaxHeadBlend = .80F;
    };

    enum class AimAssistWeaponClass : std::int32_t { Standard, Tracking, Precision, Projectile, Splash };

    struct AimAssistWeaponProfile
    {
        float Cone = 0;
        float ReleaseCone = 0;
        float Inner = 0;
        float Rotation = 0;
        float MaxSpeed = 0;
        bool Head = false;

        [[nodiscard]] static AimAssistWeaponProfile For(AimAssistWeaponClass weapon, bool scoped) noexcept
        {
            if (scoped)
            {
                return {3.5F, 4.75F, 1.25F, .5F, 8,
                    weapon == AimAssistWeaponClass::Standard || weapon == AimAssistWeaponClass::Precision};
            }
            switch (weapon)
            {
            case AimAssistWeaponClass::Tracking: return {7, 9, 2.4F, 1, 24, false};
            case AimAssistWeaponClass::Precision: return {5, 7, 1.8F, .6F, 12, true};
            case AimAssistWeaponClass::Splash: return {7, 9, 2.4F, .45F, 12, false};
            case AimAssistWeaponClass::Projectile: return {7, 9, 2.4F, .6F, 16, false};
            default: return {7, 9, 2.4F, .8F, 20, true};
            }
        }

        friend bool operator==(const AimAssistWeaponProfile&, const AimAssistWeaponProfile&) = default;
    };
}
