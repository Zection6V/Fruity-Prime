#include "GoreaBattle.hpp"

#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Formats/Enums.hpp"
#include "../../GameState.hpp"
#include "../../Menu.hpp"
#include "ScenarioRunner.hpp"
#include "../../NativeRuntime/System/Managed.hpp"
#include "../../Renderer.hpp"

#include <cstdint>
#include <iostream>

namespace MphRead::Mods::Testing
{
    using ::MphRead::NativeRuntime::ManagedAt;

    void GoreaBattle::Run()
    {
        ScenarioRunner::RunInShell("gorea-battle", [](RenderWindow& window)
        {
            return GoreaBattle::Begin(window);
        });
    }

    bool GoreaBattle::Begin(RenderWindow& window)
    {
        // Slot zero disables story-save writes, so this practice fight never
        // changes one of the player's adventure slots.
        Menu::SaveSlot = 0;
        (void)window.BeginScene();
        GameState::StartNewSave();
        GameState::Mode(GameMode::SinglePlayer);
        Entities::PlayerEntity::SetMaxPlayers(4);

        window.AddPlayer(Hunter::Sylux);
        // Room 91 is Gorea_b1. Explicit layer zero selects the first Gorea
        // encounter even if the player's normal adventure has beaten him.
        window.AddRoom(91, GameMode::SinglePlayer, 1, BossFlags::None);

        const std::shared_ptr<StorySave>& save = GameState::StorySave;
        save->CheckpointRoomId = 91;
        save->CheckpointEntityId = -1;
        save->Health = save->HealthMax = 799;
        ManagedAt(save->Ammo, 0) = ManagedAt(save->AmmoMax, 0) = 4000;
        ManagedAt(save->Ammo, 1) = ManagedAt(save->AmmoMax, 1) = 950;
        ManagedAt(save->WeaponSlots, 0) = static_cast<std::int32_t>(BeamType::PowerBeam);
        ManagedAt(save->WeaponSlots, 1) = static_cast<std::int32_t>(BeamType::Missile);
        ManagedAt(save->WeaponSlots, 2) = static_cast<std::int32_t>(BeamType::None);
        save->Weapons = static_cast<std::uint16_t>(0x00FF); // all eight story weapons
        save->Areas = static_cast<std::uint16_t>(0x01FF); // include Oubliette
        save->FoundOctoliths = save->CurrentOctoliths = static_cast<std::uint16_t>(0x00FF);
        save->BossFlags = BossFlags::None;
        GameState::UpdateCleanSave(true);

        std::cout << "[gorea] starting Gorea_b1 as Sylux" << std::endl;
        window.LoadScene();
        return true;
    }
}
