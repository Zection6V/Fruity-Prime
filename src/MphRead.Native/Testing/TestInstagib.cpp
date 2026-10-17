// Instagib: the rule survives the wire, an older build's flags do not turn it
// on, every player has a colour of their own, and a rail fades the way it is
// meant to. No window and no game files: the rendering is checked by eye.

#include "../Mods/Network/Domain/MatchDefinition.hpp"
#include "../Mods/Network/Protocol/Packets/SessionPackets.hpp"
#include "../Mods/Render/InstagibRails.hpp"
#include "../Mods/Render/InstagibEffects.hpp"
#include "../Mods/Render/EffectTint.hpp"
#include "../Mods/Combat/InstagibRules.hpp"
#include "../Mods/Launcher/Portable/LaunchPlan.hpp"
#include "../Mods/Launcher/Portable/MatchStart.hpp"
#include "../Formats/Effects.hpp"
#include "../GameState.hpp"

#include <cmath>
#include <iostream>
#include <string_view>

namespace
{
    bool Check(bool condition, std::string_view name)
    {
        std::cout << (condition ? "PASS " : "FAIL ") << name << '\n';
        return condition;
    }

    float Distance(const OpenTK::Mathematics::Vector3& a, const OpenTK::Mathematics::Vector3& b)
    {
        const float x = a.X - b.X;
        const float y = a.Y - b.Y;
        const float z = a.Z - b.Z;
        return std::sqrt(x * x + y * y + z * z);
    }
}

int main()
{
    using namespace MphRead::Mods::Network;
    using MphRead::Mods::Render::InstagibRails;
    using MphRead::Mods::Render::InstagibEffects;
    using MphRead::Mods::Render::EffectTint;
    using MphRead::Mods::Combat::InstagibRules;
    using MphRead::GameState;
    using MphRead::GameMode;
    using MphRead::BeamType;
    using OpenTK::Mathematics::Vector3;
    bool ok = true;

    ok &= Check(InstagibRails::ShouldDraw(true, false)
        && !InstagibRails::ShouldDraw(false, false)
        && !InstagibRails::ShouldDraw(true, true),
        "rails draw only during active play, never during results");

    MatchDefinition match{};
    ok &= Check(!match.Instagib && (match.Rules() & SessionRules::Instagib) == SessionRules::None,
        "off by default, and off in the rule flags");
    match.Instagib = true;
    ok &= Check((match.Rules() & SessionRules::Instagib) == SessionRules::Instagib,
        "on in the rule flags when it is on");
    match.FriendlyFire = true;
    ok &= Check((match.Rules() & SessionRules::FriendlyFire) == SessionRules::FriendlyFire
        && (match.Rules() & SessionRules::Instagib) == SessionRules::Instagib,
        "it sits beside the other rules, not on top of one");

    MatchStatePacket state{};
    ok &= Check(!state.Instagib(), "a match state that does not say is not instagib");
    state.Flags = MatchStatePacket::FlagInstagib;
    ok &= Check(state.Instagib() && !state.FriendlyFire() && !state.AffinityWeapons(),
        "its bit is its own: the other rules do not move");
    state.Flags = static_cast<std::uint8_t>(MatchStatePacket::RuleFlags(1, true) | MatchStatePacket::FlagFriendlyFire);
    ok &= Check(!state.Instagib() && state.AffinityWeapons() && state.DamageLevel() == 1,
        "the damage level and the affinity rule never read as instagib");

    // Players are told apart by colour: no two of the first eight are close.
    float nearest = 1000.0F;
    for (int a = 0; a < InstagibRails::PaletteSize; ++a)
    {
        for (int b = a + 1; b < InstagibRails::PaletteSize; ++b)
        {
            nearest = std::fmin(nearest,
                Distance(InstagibRails::ColorForSlot(a), InstagibRails::ColorForSlot(b)));
        }
    }
    ok &= Check(nearest > 0.45F, "eight players, eight colours that are not alike");
    ok &= Check(Distance(InstagibRails::ColorForSlot(0), InstagibRails::ColorForSlot(InstagibRails::PaletteSize)) < 1.0e-6F,
        "a ninth slot wraps round rather than reading off the table");
    ok &= Check(Distance(InstagibRails::ColorForSlot(-1), InstagibRails::ColorForSlot(0)) < 1.0e-6F,
        "no slot at all is the first colour");

    const InstagibRails::Look fired = InstagibRails::LookAt(0.0F);
    const InstagibRails::Look middle = InstagibRails::LookAt(0.5F);
    const InstagibRails::Look gone = InstagibRails::LookAt(1.0F);
    ok &= Check(fired.Core > middle.Core && middle.Core > gone.Core && gone.Core < 0.001F,
        "the core fades from full to nothing");
    ok &= Check(fired.Glow > middle.Glow && gone.Glow < 0.001F && fired.Glow < fired.Core,
        "the glow fades too, and is never brighter than the core");
    ok &= Check(fired.Spiral > middle.Spiral && gone.Spiral < 0.1F, "the spiral fades");
    ok &= Check(gone.SpiralRadius > fired.SpiralRadius, "the spiral opens up as it goes");
    ok &= Check(InstagibRails::LookAt(-3.0F).Core <= 1.0001F && InstagibRails::LookAt(7.0F).Core < 0.001F,
        "an age outside the life is clamped");

    // The same rule gates damage, pickup visibility/interaction and bot AI.
    GameState::Mode(GameMode::Battle);
    GameState::Instagib(false);
    std::int32_t damage = 45, headshot = 200, splash = 17;
    InstagibRules::ApplyDamage(BeamType::Imperialist, damage, headshot, splash);
    ok &= Check(damage == 45 && headshot == 200 && splash == 17
        && InstagibRules::PickupsAllowed()
        && InstagibRules::AiWeapon(BeamType::PowerBeam) == BeamType::PowerBeam
        && !InstagibRules::DirectDamage(BeamType::Imperialist),
        "normal matches retain damage, pickups and AI weapon choice");
    GameState::Instagib(true);
    InstagibRules::ApplyDamage(BeamType::Imperialist, damage, headshot, splash);
    ok &= Check(damage == 9999 && headshot == 9999 && splash == 0
        && !InstagibRules::PickupsAllowed()
        && InstagibRules::AiWeapon(BeamType::PowerBeam) == BeamType::Imperialist
        && InstagibRules::DirectDamage(BeamType::Imperialist) == damage,
        "instagib applies lethal rails, no pickups and the bot firing weapon together");
    damage = 45; headshot = 200; splash = 17;
    InstagibRules::ApplyDamage(BeamType::PowerBeam, damage, headshot, splash);
    ok &= Check(damage == 45 && headshot == 200 && splash == 17,
        "non-Imperialist weapons keep their damage");
    ok &= Check(!InstagibRules::SuppressPickupTarget(33)
        && InstagibRules::SuppressPickupTarget(34)
        && InstagibRules::SuppressPickupTarget(73)
        && !InstagibRules::SuppressPickupTarget(74),
        "only the AI pickup/spawn reference range is suppressed");
    GameState::Mode(GameMode::SinglePlayer);
    InstagibRules::ApplyDamage(BeamType::Imperialist, damage, headshot, splash);
    ok &= Check(!InstagibRules::Active() && InstagibRules::PickupsAllowed()
        && !InstagibRules::SuppressPickupTarget(34)
        && damage == 45 && headshot == 200 && splash == 17,
        "story ignores a previously enabled multiplayer rule");

    // Both desktop and Android use this transition; a plain offline match
    // must clear the rule left by a previous online/Instagib match.
    using namespace MphRead::Mods::Launcher;
    LaunchPlan::Init offline{};
    offline.Kind = LaunchKind::Offline;
    MatchStart::ApplyLocalRules(LaunchPlan(offline));
    ok &= Check(!GameState::Instagib(), "plain offline launch clears the previous rule");
    offline.Instagib = true;
    MatchStart::ApplyLocalRules(LaunchPlan(offline));
    ok &= Check(GameState::Instagib(), "offline launch carries its requested rule");
    offline.Kind = LaunchKind::Online;
    offline.Instagib = false;
    MatchStart::ApplyLocalRules(LaunchPlan(offline));
    ok &= Check(GameState::Instagib(), "online launch leaves rule selection to server sync");

    // Pool data can be absent, entry-owned or element-owned. The effective
    // tint is also what child effects inherit, including muzzle flash debris.
    MphRead::Effects::EffectParticle particle;
    const Vector3 authored(0.2F, 0.8F, 0.4F);
    ok &= Check(Distance(EffectTint::ParticleColor(particle, authored), authored) < 1.0e-6F,
        "an unowned particle keeps its authored colour");
    particle.Owner = std::make_shared<MphRead::Effects::EffectElementEntry>();
    particle.Owner->EffectEntry = std::make_shared<MphRead::Effects::EffectEntry>();
    ok &= Check(!EffectTint::ForElement(*particle.Owner), "new pool entries carry no tint");
    particle.Owner->EffectEntry->ModTint = Vector3(1, 0, 0);
    ok &= Check(Distance(EffectTint::ParticleColor(particle, authored), Vector3(0.8F, 0, 0)) < 1.0e-6F
        && EffectTint::ForElement(*particle.Owner)->X == 1.0F,
        "entry tint preserves brightness and propagates to child effects");
    particle.Owner->ModTint = Vector3(0, 1, 0);
    ok &= Check(Distance(EffectTint::ParticleColor(particle, authored), Vector3(0, 0.8F, 0)) < 1.0e-6F,
        "element tint takes precedence over entry tint");
    auto muzzle = particle.Owner->EffectEntry;
    GameState::Mode(GameMode::Battle);
    InstagibEffects::TintMuzzle(muzzle, BeamType::Imperialist, 1);
    ok &= Check(muzzle->ModTint && Distance(*muzzle->ModTint, InstagibRails::ColorForSlot(1)) < 1.0e-6F,
        "Imperialist muzzle uses its shooter slot colour");
    GameState::Instagib(false);
    InstagibEffects::TintMuzzle(muzzle, BeamType::Imperialist, 1);
    ok &= Check(!muzzle->ModTint && !InstagibEffects::Tint(BeamType::Imperialist, nullptr),
        "normal muzzle and ownerless shots do not inherit the previous tint");

    std::cout << (ok ? "ALL PASS" : "FAILED") << '\n';
    return ok ? 0 : 1;
}
