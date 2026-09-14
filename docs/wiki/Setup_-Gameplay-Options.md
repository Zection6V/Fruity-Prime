## Adventure Mode Save Slot

By default, no save slot is selected. To use a save slot and record your single-player progress, enter the `Adventure Mode Settings` menu and choose a slot from 1 to 255. There are currently no options for copying or deleting saves, but you may do so manually by managing the `saveXXX.json` files in the `Savedata` folder.

There are separate settings for when MphRead will save your data to the slot, depending on whether you exit by closing the window, or by entering the ship hatch in-game. You can choose whether to save `always`, save after a yes/no `prompt` in the console window, or save `never`. It's recommended that the setting for save after exit should be `never`, or `prompt` for special use, because the game does not normally save data except by returning to the ship, so it's possible for unwanted changes to be persisted and for story progress to be blocked. The setting for saving from the ship should be `prompt` or `always`.

After choosing a slot, begin story mode by loading `UNIT2_LAND` (Celestial Gateway). To move to other planets once unlocked, return to the menu by entering the ship, and choose `UNIT1_LAND` (Alinos Gateway), `UNIT3_LAND` (VDO Gateway), or `UNIT4_LAND` (Arcterra Gateway).

## Adventure Mode Settings

When no save slot is selected, a list of story mode options is displayed instead. These settings are only used without a save slot, and allow you to specify various game state options that would normally be tracked in the save file.

## Features, Cheats and Bugfixes

The `Features` menu contains several options for altering gameplay from how it normally functions.

### Features

**No Repeat Encounters**:<br>After completing a random hunter/Guardian encounter, that room will no longer have any random encounters when reloaded, until you've reloaded the game by exiting or returning to the ship.

**Allow Invalid Teams**:<br>MPH will automatically end a multiplayer battle if it detects that there is only one player, or one team of players, in the match. When enabled, this setting prevents that behavior.

**Target Info On Top Screen**:<br>Display multiplayer target portrait and health bar on the main screen. This is shown on the bottom screen in-game

**Helmet Opacity**:<br>Make the helmet overlay graphics invisible, 50% opaque, or fully opaque. They are fully opaque in-game.

**Visor Opacity**:<br>Make the visor overlay graphics (lines) invisible, 50% opaque, or fully opaque. They are 50% opaque in-game.

**HUD Opacity**:<br>Make the HUD overlay graphics (health and ammo bars) invisible, 50% opaque, or fully opaque. They are fully opaque in-game.

**Reticle Opacity**:<br>Make the target reticle graphics invisible, 50% opaque, or fully opaque. They are fully opaque in-game.

**HUD Sway**:<br>Allows disabling the in-game effect where the helmet/HUD graphics shift while turning the camera.

**Target Info Sway**:<br>Allows disabling the effect where the top screen target info graphics shift while turning the camera.

**Delayed Idle Sway**:<br>The time until the idle first-person view begins swaying is increased by a factor of 4.

**No Idle Sway**:<br>The first-person view will not begin swaying no matter how long it is idle.

**No Map Centering**:<br>The map on the pause screen will not automatically center on the selected room when panning input stops.

**Maximum Room Detail**:<br>For hardware performance reasons, MPH will hide some room geometry in multiplayer mode when more than two players are in a match. When enabled, this setting preserves all room geometry even with more than two players.

**Maximum Player Detail**:<br>For hardware performance reasons, MPH will switch to a lower-quality player model when you are a certain distance away from another hunter. When enabled, this setting makes the higher-quality model always be used.

**Logarithmic Spatial Audio**:<br>Enables a different method for determining spatial audio volume by distance.

**Consistent Alarm Interval**:<br>Makes the multiplayer match timer alarm play at consistent half-second intervals.

**Full Boost Charge**:<br>The button-activated Boost Ball will always have full charge without the button needing to be held down, giving it the same strength as the touch-activated boost in-game.

**Update Adventure Mode For Other Hunters**:<br>When enabled, new entities will be added to some rooms in Adventure Mode to make them more possible to complete with hunters other than Samus, as well as a few other tweaks. See [[here|Adventure Mode Alterations for Other Hunters]] for the list of changes. Additionally, if the player's current hunter is encountered as an enemy hunter, the enemy will be changed to a different recolor if needed.

### Cheats

**Free Weapon Select**:<br>Allows equipping any weapon by pressing a dedicated key, regardless of whether that weapon has been collected or unlocked. The default keys are the digits 1 through 9.

**Unlimited Jumps**:<br>Allows unlimited jumping in midair.

**No Random Encounters**:<br>Prevents random hunter/Guardian encounters from occurring in Adventure Mode.

**All Doors Unlocked**:<br>Locked doors will be unlocked by default in Adventure Mode. May cause story progression issues.

**Retry From Current Room**:<br>When continuing from game over in Adventure Mode, you will start in the room where you died, even if the last activated checkpoint was in a different room.

**Skip Planet Intros**:<br>The player will spawn immediately in the starting area of the five planetary locations on a fresh Adventure Mode save (or no save), without the intro camera sequence (ship landing) playing.

**Start With All Upgrades**:<br>Health, missile ammo, and UA will start at their maximum values in Adventure Mode.

**Start With All Octoliths**:<br>All eight Octoliths will be collected at the start in Adventure Mode.

**Walk Through Walls**:<br>Allows passing through most walls and wall-like collision while in biped form. May cause movement issues.

**Always Fight Gorea 2**:<br>After defeating Gorea 1 in Adventure Mode, the Gorea 2 fight will always begin, even if the sequence of affinity weapon switches hasn't been completed.

**Quadruple Damage**:<br>Increases the damage of beam projectiles by a factor of 4. Currently applies to all beams, not just the player's.

### Bugfixes

**Smooth Camera Sequence Handoff**:<br>Avoids issues with "jumping" when switching between some alternate camera views in Adventure Mode.

**Better Camera Sequence Node Refs**:<br>Avoids issues with "tearing" of some alternate camera views in Adventure Mode.

**No Stray Respawn Text**:<br>Prevents incorrect respawn timer countdown text in multiplayer modes.

**Correct Bounty SFX**:<br>Corrects an in-game issue where the intended "Bounty rewarded" voice stream and score SFX will only play when the current player delivers an Octolith, and incorrect SFX will play when a teammate delivers one.

**Fix Double Enemy Death**:<br>Prevents Judicator ricochet projectiles from causing some enemy kills to register twice in Adventure Mode, which affects in-game stats.

**Fix Slench Roll Timer Underflow**:<br>Fixes an in-game bug where using bombs during Slench's rolling phase can cause its timer to underflow and make the phase last longer.
