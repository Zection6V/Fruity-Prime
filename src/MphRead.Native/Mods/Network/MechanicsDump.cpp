#include "Mods/Network/mechanics_dump.hpp"

#include "Entities/match_flow.hpp"
#include "Metadata/metadata.hpp"

#include "../../Entities/Players/player_profile.hpp"

#include <array>
#include <cstdint>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>

namespace fruityprime::mechanics {
namespace {

constexpr std::uint32_t FlagCanCharge = 0x00000200U;
constexpr std::uint32_t FlagRepeatFire = 0x00000400U;
constexpr std::uint32_t FlagCanZoom = 0x00000800U;
constexpr std::uint32_t FlagRicochetUncharged = 0x00001000U;
constexpr std::uint32_t FlagSelfDamageUncharged = 0x00008000U;
constexpr std::uint32_t FlagAoeUncharged = 0x00080000U;
constexpr std::uint32_t FlagAoeCharged = 0x00100000U;
constexpr std::uint32_t FlagContinuous = 0x00200000U;

[[nodiscard]] bool has_flag(std::uint32_t flags,
                            std::uint32_t flag) noexcept {
    return (flags & flag) != 0;
}

[[nodiscard]] std::string_view beam_name(formats::BeamType beam) noexcept {
    switch (beam) {
    case formats::BeamType::PowerBeam: return "PowerBeam";
    case formats::BeamType::VoltDriver: return "VoltDriver";
    case formats::BeamType::Missile: return "Missile";
    case formats::BeamType::Battlehammer: return "Battlehammer";
    case formats::BeamType::Imperialist: return "Imperialist";
    case formats::BeamType::Judicator: return "Judicator";
    case formats::BeamType::Magmaul: return "Magmaul";
    case formats::BeamType::ShockCoil: return "ShockCoil";
    case formats::BeamType::OmegaCannon: return "OmegaCannon";
    default: return "Unknown";
    }
}

[[nodiscard]] std::string_view affliction_name(std::uint8_t value) noexcept {
    switch (value) {
    case 1: return "Freeze";
    case 2: return "Disrupt";
    case 4: return "Burn";
    default: return "None";
    }
}

[[nodiscard]] std::string weapon_notes(std::uint32_t flags) {
    static constexpr std::array<std::pair<std::uint32_t, std::string_view>, 7>
        notes{{
            {FlagCanZoom, "can zoom"},
            {FlagContinuous, "continuous"},
            {FlagRepeatFire, "repeat fire"},
            {FlagCanCharge, "chargeable"},
            {FlagRicochetUncharged, "ricochets"},
            {FlagAoeUncharged | FlagAoeCharged, "area of effect"},
            {FlagSelfDamageUncharged, "can hurt the shooter"}
        }};
    std::string result;
    for (const auto& [flag, note] : notes) {
        if (!has_flag(flags, flag)) {
            continue;
        }
        if (!result.empty()) {
            result += ", ";
        }
        result += note;
    }
    return result.empty() ? "-" : result;
}

[[nodiscard]] std::string afflictions(
    const metadata::MultiplayerWeaponInfo& weapon) {
    std::string result;
    for (std::size_t i = 0; i < weapon.afflictions.size(); ++i) {
        if (weapon.afflictions[i] == 0) {
            continue;
        }
        if (!result.empty()) {
            result += ", ";
        }
        if (i != 0) {
            result += "charged: ";
        }
        result += affliction_name(weapon.afflictions[i]);
    }
    return result.empty() ? "-" : result;
}

[[nodiscard]] std::string fixed_text(std::int32_t value,
                                     int precision = 4) {
    std::ostringstream result;
    result << std::setprecision(precision) << std::defaultfloat
           << players::Profile::fixed_to_float(value);
    return result.str();
}

void weapon_table(std::ostringstream& text) {
    text << "## Weapons (multiplayer table)\n\n"
         << "Damage is per shot before any multiplier. \"Charged\" is a full "
            "charge; \"min\" is the smallest charge that counts as charged.\n\n"
         << "| Beam | dmg | min-chg | charged | headshot | hs charged | splash | "
            "ammo | cost | cooldown | afflictions | notes |\n"
         << "|---|---|---|---|---|---|---|---|---|---|---|---|\n";
    const auto& weapons = metadata::multiplayer_weapons();
    for (std::size_t i = 0; i < weapons.size(); ++i) {
        if (i == metadata::WeaponCount) {
            text << "\nAffinity versions -- what a hunter gets when it carries its "
                    "own weapon. Note the extra damage and the afflictions the "
                    "plain versions do not have:\n\n"
                 << "| Beam | dmg | min-chg | charged | headshot | hs charged | "
                    "splash | ammo | cost | cooldown | afflictions | notes |\n"
                 << "|---|---|---|---|---|---|---|---|---|---|---|---|\n";
        }
        const auto& weapon = weapons[i];
        text << "| " << beam_name(weapon.beam)
             << " | " << weapon.uncharged_damage
             << " | " << weapon.min_charge_damage
             << " | " << weapon.charged_damage
             << " | " << weapon.headshot_damage
             << " | " << weapon.charged_headshot_damage
             << " | " << weapon.splash_damage << "/"
             << weapon.charged_splash_damage
             << " | " << (weapon.ammo_type == 0 ? "UA" : "missile")
             << " | " << weapon.ammo_cost << "/" << weapon.charge_cost
             << " | " << static_cast<int>(weapon.shot_cooldown)
             << " | " << afflictions(weapon)
             << " | " << weapon_notes(weapon.flags) << " |\n";
    }
    text << "\nAffinity weapon per hunter (the one whose enhanced version it uses):\n\n";
    for (std::size_t i = 0; i < metadata::PlayableHunterCount; ++i) {
        const auto native_slot = metadata::hunters()[i].affinity_weapon;
        const auto beam = metadata::beam_type_from_native_weapon_slot(native_slot);
        text << "- " << metadata::hunters()[i].name << ": "
             << beam_name(static_cast<formats::BeamType>(beam)) << "\n";
    }
    text << "\n";
}

void damage_rules(std::ostringstream& text) {
    text << "## Damage multipliers, in the order the code applies them\n\n"
         << "| Rule | Effect | Where |\n"
         << "|---|---|---|\n"
         << "| Beam effectiveness vs the target | x0 / x0.5 / x1 / x2 | "
            "`PlayerEntity.TakeDamage`, from `BeamEffectiveness[beam]`. Players "
            "are set to Normal for every beam in `Spawn()`; **a player that never "
            "spawned has x0 for everything and cannot be hurt at all** |\n"
         << "| Double damage pickup | x2, and *not* applied to Shock Coil | "
            "`BeamProjectileEntity` |\n"
         << "| Prime Hunter | x1.5 | `BeamProjectileEntity` |\n"
         << "| **Imperialist without zoom** | **/2** | `BeamProjectileEntity`: "
            "`if (weapon.Beam == Imperialist && !equip.Zoomed) damage /= 2` |\n"
         << "| Quadruple Damage cheat | x4 | `BeamProjectileEntity`, from "
            "settings.json. Disabled automatically while connected to a server |\n"
         << "| Match damage level | x0.75 low / x1 medium / x1.25 high | "
            "`PlayerEntity.TakeDamage` |\n"
         << "| Headshot | uses the weapon's headshot damage instead | "
            "`BeamProjectileEntity` |\n"
         << "| Friendly fire off, same team | x0 | `PlayerEntity.TakeDamage` |\n"
         << "| Weavel halfturret alive | damage is split between body and turret | "
            "`PlayerEntity.TakeDamage` |\n"
         << "| Affinity weapon | the hunter uses entry `beam + 9`, which is a "
            "different set of numbers entirely -- more damage and, for several "
            "weapons, an affliction the plain version does not inflict | "
            "`PlayerEntity.TryEquipWeapon` |\n\n"
         << "Invulnerability windows: a hit sets a damage-invulnerability timer "
            "(per hunter, `Values.DamageInvuln`), and spawning sets a "
            "spawn-invulnerability timer. Both reject further damage until they "
            "run out.\n\n";
}

void hunters(std::ostringstream& text) {
    text << "## Hunters\n\n"
         << "| Hunter | energy tank | MP max health | MP ammo cap | alt form | "
            "bombs | boost | alt attack |\n"
         << "|---|---|---|---|---|---|---|---|\n";
    constexpr std::array<std::string_view, metadata::PlayableHunterCount> alt{{
        "Morph Ball", "Stinglarva", "Triskelion", "Lockjaw", "Vhoscythe",
        "Dialanche", "Halfturret"}};
    constexpr std::array<std::string_view, metadata::PlayableHunterCount> attacks{{
        "bombs", "bombs", "cloak and lunge", "bombs", "spin attack", "slam",
        "leaves a turret that shoots on its own"}};
    for (std::size_t i = 0; i < metadata::PlayableHunterCount; ++i) {
        const auto& hunter = metadata::hunters()[i];
        const auto& profile = players::profiles()[i];
        const bool bombs = i == 0 || i == 1 || i == 3;
        text << "| " << hunter.name
             << " | " << profile.energy_tank
             << " | " << (2 * profile.energy_tank - 1)
             << " | " << profile.multiplayer_ammo_cap
             << " | " << alt[i]
             << " | " << (bombs ? "yes" : "no")
             << " | " << (i == 0 ? "yes" : "no")
             << " | " << attacks[i] << " |\n";
    }
    text << "\nHealth on spawn is `EnergyTank - 1`; the maximum in multiplayer is "
            "`2 * EnergyTank - 1`, so a full pickup run doubles a hunter's "
            "effective health.\n\n";
}

void movement(std::ostringstream& text) {
    text << "## Movement, per hunter\n\n"
         << "Speeds are units per frame as the engine stores them (20.12 fixed "
            "point converted to float on load).\n\n"
         << "| Hunter | walk cap | strafe cap | jump | biped gravity | alt gravity "
            "(air/ground) | boost cap | boost charge (min-max) | alt radius |\n"
         << "|---|---|---|---|---|---|---|---|---|\n";
    for (std::size_t i = 0; i < metadata::PlayableHunterCount; ++i) {
        const auto& profile = players::profiles()[i];
        text << "| " << metadata::hunters()[i].name
             << " | " << fixed_text(profile.walk_speed_cap, 3)
             << " | " << fixed_text(profile.strafe_speed_cap, 3)
             << " | " << fixed_text(profile.jump_speed, 3)
             << " | " << fixed_text(profile.biped_gravity)
             << " | " << fixed_text(profile.alt_air_gravity) << "/"
             << fixed_text(profile.alt_ground_gravity)
             << " | " << fixed_text(profile.boost_speed_cap, 3)
             << " | " << profile.boost_charge_min << "-"
             << profile.boost_charge_max
             << " | " << fixed_text(profile.alt_collision_radius, 3)
             << " |\n";
    }
    text << "\n"
         << "- Morphing plays an animation and the form only changes when it ends "
            "(`EnterAltForm` sets Morphing; `ProcessPlayer` applies `UpdateForm` "
            "on `AnimFlags.Ended`). Unmorphing applies the change immediately.\n"
         << "- Alt form swaps the collision volume (`PlayerVolumes[hunter, 2]`), "
            "and the position is shifted by the difference between the two volume "
            "centres.\n"
         << "- Boost is a charge-then-release: hold to build from `BoostChargeMin` "
            "to `BoostChargeMax`, release to convert it into speed between "
            "`BoostSpeedMin` and `BoostSpeedMax`.\n\n";
}

void states(std::ostringstream& text) {
    text << "## States a player can be put into\n\n"
         << "| State | Cause | Duration | Effect |\n"
         << "|---|---|---|---|\n"
         << "| Frozen | Judicator (affinity version only) | 75 frames doubled; 15 "
            "doubled if refrozen within 60 doubled | cannot act; the ice layer "
            "draws over the hunter; animation frames stop advancing |\n"
         << "| Disrupted | Volt Driver (affinity) | 60 doubled | HUD distortion, "
            "aim disrupted |\n"
         << "| Burning | Magmaul (affinity) | 150 doubled | damage over time, "
            "attributed to whoever set the fire |\n"
         << "| Double damage | pickup | timer | x2 outgoing damage, Shock Coil "
            "excepted; a visible effect is attached to the gun |\n"
         << "| Cloaked | pickup | timer | alpha drops towards invisible |\n"
         << "| Deathalt | pickup | timer | forced alt form, health drains, heavy "
            "damage output |\n"
         << "| Halfturret | Weavel morphs | until unmorph or death | the turret is a "
            "separate entity that shoots on its own; damage is split between it "
            "and the body, and unmorphing gives its remaining health back to "
            "Weavel |\n"
         << "| Damage invulnerable | any hit | `Values.DamageInvuln` doubled | "
            "further hits are rejected outright |\n"
         << "| Spawn invulnerable | spawning | timer | hits are rejected unless the "
            "damage carries `IgnoreInvuln` or `Death` |\n\n";
}

void spawn_rules(std::ostringstream& text) {
    text << "## Spawning and respawning\n\n"
         << "`PlayerProcess.GetRespawnPoint` picks a point by these rules, in order:\n\n"
         << "1. Consider at most 25 spawn points. Skip any that is inactive, still "
            "on cooldown, or (on the very first frame) flagged by availability.\n"
         << "2. In Capture, skip points belonging to the other team.\n"
         << "3. A point is *valid* only if every living player is at least 10 units "
            "away. Among the valid ones, the choice rotates with the frame counter.\n"
         << "4. If none is valid, take the one furthest from any living player.\n"
         << "5. If even that is unavailable -- every point on cooldown, which happens "
            "with more players than the map was drawn for -- take any active point. "
            "Without this last step the player simply does not spawn and waits at the "
            "origin.\n\n"
         << "The chosen point goes on a cooldown of 2 frames doubled, so a crowd cannot "
            "all land on the same one.\n\n"
         << "A dead player waits on `_respawnTimer`; it may spawn early by holding fire. "
            "Health on spawn is `EnergyTank - 1`. **`Spawn()` is also what fills in "
            "`BeamEffectiveness`** -- a player that reaches the map without it takes "
            "zero damage from every beam in the game.\n\n";
}

void modes(std::ostringstream& text) {
    text << "## Match modes\n\n"
         << "| Mode | Scored on | Second column on the scoreboard |\n"
         << "|---|---|---|\n"
         << "| Battle / Battle Teams | points (a kill is +1, dying is -1) | deaths |\n"
         << "| Survival / Survival Teams | time alive | deaths; running out of lives puts "
            "a player out of the game |\n"
         << "| Capture | octoliths taken | kills |\n"
         << "| Bounty / Bounty Teams | octoliths delivered | kills |\n"
         << "| Nodes / Nodes Teams | points from held nodes | kills |\n"
         << "| Defender / Defender Teams | time holding the node | kills |\n"
         << "| Prime Hunter | time spent as the prime hunter | kills; the prime hunter deals "
            "x1.5 damage and is shown to everyone |\n\n"
         << "A match ends on the point goal or the time limit, whichever comes first. "
            "Team play merges the per-player tallies into two team tallies, and with "
            "friendly fire off a shot at a team-mate does nothing at all.\n\n"
         << "Native mode defaults are sourced from `match::defaults_for_mode`: ";
    bool first = true;
    for (std::uint8_t id = 3; id <= 14; ++id) {
        const auto* mode = metadata::game_mode_info(id);
        if (mode == nullptr) {
            continue;
        }
        const auto defaults = match::defaults_for_mode(id);
        if (!first) {
            text << "; ";
        }
        first = false;
        text << mode->name << "=" << defaults.point_goal << " points/"
             << static_cast<int>(defaults.time_limit_seconds) << "s";
    }
    text << ".\n\n";
}

void world(std::ostringstream& text) {
    text << "## World interactions\n\n"
         << "| Thing | Behaviour |\n"
         << "|---|---|\n"
         << "| Jump pad | launches whatever touches it along a fixed vector; this is "
            "the one place a player legitimately covers a lot of ground in a few "
            "frames |\n"
         << "| Teleporter | moves the player to the linked pad, optionally forcing alt "
            "form on arrival |\n"
         << "| Door / force field | opens on contact or on a weapon of the right colour; "
            "locked doors ignore everything else |\n"
         << "| Kill height | a room-wide floor: below it the player dies. This is what most "
            "\"random deaths\" in a fast match actually are |\n"
         << "| Lava and hazard volumes | set the player on fire while standing in them, "
            "except for Spire, who is immune |\n"
         << "| Morph camera | a volume that forces the camera behind a morphed player, and "
            "blocks unmorphing while inside |\n"
         << "| Item spawner | respawns its item on a timer once taken |\n\n";
}

void items(std::ostringstream& text) {
    text << "## Pickups\n\n"
         << "| Item | Effect |\n"
         << "|---|---|\n";
    for (const auto& item : metadata::items()) {
        if (item.id < 0) {
            continue;
        }
        text << "| " << item.name << " | ";
        if (item.weapon >= 0) {
            text << "grants " << metadata::weapon_info(
                static_cast<std::uint8_t>(item.weapon)).display_name
                 << " and its ammo";
        } else if (item.name.find("HEALTH") != std::string_view::npos) {
            text << "restores health";
        } else if (item.name.find("AMMO") != std::string_view::npos
                   || item.name.find("UA ") != std::string_view::npos
                   || item.name.find("MISSILE ") != std::string_view::npos) {
            text << "restores ammunition";
        } else if (item.name == "DOUBLE DAMAGE") {
            text << "x2 damage for a time, Shock Coil excepted";
        } else if (item.name == "CLOAK") {
            text << "invisibility for a time";
        } else if (item.name == "DEATHALT") {
            text << "forced alt form, drains health, heavy damage";
        } else {
            text << "world pickup";
        }
        text << " |\n";
    }
    text << "\nA player killed in multiplayer drops ammo of the type its killer's weapon "
            "uses.\n\n";
}

void bots(std::ostringstream& text) {
    text << "## Bots\n\n"
         << "A bot is an ordinary player whose `Controls` are written by "
            "`PlayerAi.ProcessInput` instead of by a keyboard. That is the whole of "
            "the difference, and it is why a networked player can reuse the same "
            "surface: relayed input is simply a third writer of the same buttons.\n\n"
         << "| Piece | What it does |\n"
         << "|---|---|\n"
         << "| `PlayerEntity.IsBot` | marks the slot as AI-driven. `Scene.AddPlayer` sets it "
            "on every player after the first, which is right for a local match and "
            "wrong for a networked one -- the AI would overwrite relayed input, so a "
            "networked session clears it on every slot |\n"
         << "| `BotLevel` (0-2) | difficulty; clamped and used to index reaction and "
            "accuracy tables |\n"
         << "| `AiPersonality` | per-hunter behaviour trees loaded from the ROM's own data, "
            "one set per hunter and encounter. `AiPersonalityData1` nodes hold "
            "conditions and the function ids to run |\n"
         << "| `AiData.Process()` | run once per frame per bot from `Scene.UpdateScene`, "
            "but only while the bot is alive |\n"
         << "| `UpdateExecutionPath` / `Execute` | walks the tree and dispatches `Func24Id` "
            "to the behaviour functions -- move, aim, fire, morph, use the alt "
            "attack, pick a weapon |\n"
         << "| Weapon choice | prefers the hunter's affinity weapon "
            "(`Weapons.AffinityWeapons[hunter]`) and zooms when it holds a weapon "
            "that can |\n"
         << "| `AiFlags3` | the spawn/despawn handshake: one bit asks for the spawn effect "
            "and sound, another marks the bot as despawned |\n\n"
         << "For testing, `NetTestScript` replaces the AI entirely: it writes the same "
            "`Controls`, but to a fixed script rather than a behaviour tree, so two "
            "machines can be asked to do the same thing at the same moment and "
            "compared.\n\n";
}

void networking(std::ostringstream& text) {
    text << "## How multiplayer works here\n\n"
         << "This is not the DS Wi-Fi protocol and cannot talk to real hardware or an "
            "emulator. It connects MphRead instances to each other.\n\n"
         << "| Piece | Rule |\n"
         << "|---|---|\n"
         << "| Server | a relay with no game files: it assigns slots, keeps the match clock "
            "and the map rotation, and forwards packets. It never simulates |\n"
         << "| Authority | the first client to connect. It resolves damage, deaths and "
            "scores for everybody |\n"
         << "| Position | owned by the player it belongs to. Each client publishes its own "
            "position in every intent and everyone else follows it, including the "
            "authority. Two simulations of one player fighting over a position is "
            "what produced rubber-banding |\n"
         << "| Input | relayed to every client, not only the authority. Input is what makes "
            "a player fire, morph, lay a bomb or swing an alt attack; clients that "
            "received only positions drew opponents gliding in silence |\n"
         << "| One-frame presses | carried as an 8-frame history of rising edges, because a "
            "press exists in exactly one packet and UDP loses packets. Edges are taken "
            "only from that history: deriving them from the button level as well "
            "applied each press twice, which for a toggle means never |\n"
         << "| Snapshot | the authority's view of every player: health, score, form, weapon, "
            "zoom, and a damage record. Sent every frame |\n"
         << "| Damage | resolved only by the authority; every other client throws away "
            "locally-resolved hits. The authority stamps each hit with a counter, and "
            "victims replay the *difference* in that counter, so several hits between "
            "two snapshots are all accounted for |\n"
         << "| Score | carried in the snapshot. Counting locally worked only for whoever had "
            "been present since the first kill |\n"
         << "| Remote smoothing | remote players ease toward their reported position (35% of "
            "the gap per frame, 60% when it is wide) and only jump past 15 units, so a "
            "lost burst glides instead of popping |\n"
         << "| Slots | `PlayerEntity.SlotCapacity` (8). Every slot-indexed array is sized "
            "from it |\n"
         << "| Map rotation | the server owns it; clients poll the match state and load the "
            "new room, rebuilding every player slot and resetting the scores |\n"
         << "| Chat | T opens a line, the server stamps it with the sender's real slot and "
            "name and relays it to everybody else, and the sender echoes its own. Rate "
            "limited at the relay: three back to back, then one every two seconds. "
            "Additive on the wire, so an older server simply drops it |\n\n";
}

} // namespace

void print(std::ostream& output) {
    std::ostringstream text;
    text << "# Metroid Prime Hunters — multiplayer mechanics\n\n"
         << "Generated by `FruityPrime -mechanics` from the native game's own "
            "tables.\n\n";
    weapon_table(text);
    damage_rules(text);
    hunters(text);
    movement(text);
    states(text);
    spawn_rules(text);
    modes(text);
    world(text);
    items(text);
    bots(text);
    networking(text);
    output << text.str();
}

} // namespace fruityprime::mechanics
