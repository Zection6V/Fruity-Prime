# Standalone MinGW build for Fruity Prime.
#
# This intentionally has no CMake or Visual Studio dependency.  The project
# still keeps native/CMakeLists.txt for compatibility, while this file is the
# reproducible Windows path when C:\mingw64 is the only toolchain installed.
# Its source lists intentionally point into src/MphRead.Native.

CXX = C:/mingw64/bin/g++.exe
AR = C:/mingw64/bin/ar.exe
BUILD ?= build-mingw-direct
SHELL := cmd.exe
CPP_ROOT := ../src/MphRead.Native

# The named counterparts for the managed MphRead source files are kept in a
# separate list so they are compiled as source units even while a few of them
# still delegate to a shared native backend.  This prevents a source file
# from being present only for Explorer parity and never reaching the build.
-include port-boundary-sources.mk

CXXFLAGS += -std=c++20 -Wall -Wextra -Wpedantic -O2 -MMD -MP \
	-I$(CPP_ROOT) \
	-IC:/mingw64/x86_64-w64-mingw32/include \
	-DWIN32_LEAN_AND_MEAN -DNOMINMAX
LDFLAGS += -static-libgcc -static-libstdc++
LDLIBS += -L C:/mingw64/x86_64-w64-mingw32/lib -lz \
	-lwindowscodecs -lole32 -luuid \
	-lws2_32 -lwinhttp -luser32 -lgdi32 -lcomdlg32 -lshell32 -lwinmm -lmsimg32 -lwindowscodecs -pthread

COMMON_SOURCES := \
    $(CPP_ROOT)/Features.cpp \
    $(CPP_ROOT)/GameState.cpp \
    $(CPP_ROOT)/Memory.cpp \
    $(CPP_ROOT)/MemoryArrays.cpp \
    $(CPP_ROOT)/MemoryClasses.cpp \
    $(CPP_ROOT)/Messaging.cpp \
    $(CPP_ROOT)/Read.cpp \
    $(CPP_ROOT)/Renderer.cpp \
    $(CPP_ROOT)/Scene.cpp \
    $(CPP_ROOT)/SceneSetup.cpp \
    $(CPP_ROOT)/Shaders.cpp \
    $(CPP_ROOT)/Entities/EntityBase.cpp \
    $(CPP_ROOT)/Entities/PlatformEntity.cpp \
    $(CPP_ROOT)/Entities/ObjectEntity.cpp \
    $(CPP_ROOT)/Entities/PlayerSpawnEntity.cpp \
    $(CPP_ROOT)/Entities/ItemSpawnEntity.cpp \
    $(CPP_ROOT)/Entities/DoorEntity.cpp \
    $(CPP_ROOT)/Entities/VolumeEntity.cpp \
    $(CPP_ROOT)/Entities/AreaVolumeEntity.cpp \
    $(CPP_ROOT)/Entities/TriggerVolumeEntity.cpp \
    $(CPP_ROOT)/Entities/JumpPadEntity.cpp \
    $(CPP_ROOT)/Entities/MorphCameraEntity.cpp \
    $(CPP_ROOT)/Entities/NodeDefenseEntity.cpp \
    $(CPP_ROOT)/Entities/FlagBaseEntity.cpp \
    $(CPP_ROOT)/Entities/LightSourceEntity.cpp \
    $(CPP_ROOT)/Entities/TeleporterEntity.cpp \
    $(CPP_ROOT)/Entities/OctolithFlagEntity.cpp \
    $(CPP_ROOT)/Entities/ArtifactEntity.cpp \
    $(CPP_ROOT)/Entities/ForceFieldEntity.cpp \
    $(CPP_ROOT)/Entities/PointModuleEntity.cpp \
    $(CPP_ROOT)/Entities/RoomEntity.cpp \
    $(CPP_ROOT)/Entities/room_node_refs.cpp \
    $(CPP_ROOT)/Entities/room_connectors.cpp \
    $(CPP_ROOT)/Entities/enemy_helpers.cpp \
    $(CPP_ROOT)/Entities/CamSeq/camera_sequence_entity.cpp \
	    $(CPP_ROOT)/Entities/CamSeq/CameraSequence.cpp \
	    $(CPP_ROOT)/Entities/CamSeq/CamSeqEntity.cpp \
	    $(CPP_ROOT)/Menu.cpp \
	    $(CPP_ROOT)/MenuModelMetadata.cpp \
	    $(CPP_ROOT)/Selection.cpp \
    $(CPP_ROOT)/Strings.cpp \
    $(CPP_ROOT)/Program.cpp \
    $(CPP_ROOT)/Test.cpp \
    $(CPP_ROOT)/Assets/archive.cpp \
    $(CPP_ROOT)/Assets/compression.cpp \
    $(CPP_ROOT)/Formats/camera_sequence.cpp \
    $(CPP_ROOT)/Formats/Entity.cpp \
    $(CPP_ROOT)/Formats/EntityEnemy.cpp \
    $(CPP_ROOT)/Formats/effects.cpp \
    $(CPP_ROOT)/Formats/effect_runtime.cpp \
    $(CPP_ROOT)/Formats/frontend.cpp \
    $(CPP_ROOT)/Assets/game_assets.cpp \
    $(CPP_ROOT)/Assets/model_catalog.cpp \
    $(CPP_ROOT)/Formats/model_instance.cpp \
    $(CPP_ROOT)/Formats/movie.cpp \
    $(CPP_ROOT)/Formats/paths.cpp \
    $(CPP_ROOT)/Assets/nds_rom.cpp \
    $(CPP_ROOT)/Export/Scripting.cpp \
    $(CPP_ROOT)/Metadata/entity_metadata.cpp \
    $(CPP_ROOT)/Metadata/metadata.cpp \
      $(CPP_ROOT)/Metadata/MetadataClasses.cpp \
      $(CPP_ROOT)/Metadata/MetadataFacade.cpp \
      $(CPP_ROOT)/Metadata/MetadataModels.generated.cpp \
      $(CPP_ROOT)/Metadata/MetadataValues.cpp \
      $(CPP_ROOT)/Metadata/Player.cpp \
      $(CPP_ROOT)/Metadata/Weapons.cpp \
      $(CPP_ROOT)/Metadata/Weapons.generated.cpp \
      $(CPP_ROOT)/Metadata/Weapons.generated.cpp \
      $(CPP_ROOT)/Metadata/Weapons.generated.cpp \
      $(CPP_ROOT)/Metadata/FrontendMeta.cpp \
      $(CPP_ROOT)/Metadata/FrontendMeta.generated.cpp \
      $(CPP_ROOT)/Metadata/Rooms.cpp \
    $(CPP_ROOT)/Metadata/Enemies.cpp \
    $(CPP_ROOT)/Mods/Input/GamepadDesktop.cpp \
    $(CPP_ROOT)/Mods/Input/GamepadInput.cpp \
    $(CPP_ROOT)/Mods/Input/GamepadState.cpp \
    $(CPP_ROOT)/Mods/Input/PadBindings.cpp \
    $(CPP_ROOT)/Mods/Input/GamepadProbe.cpp \
    $(CPP_ROOT)/Mods/Settings/settings_common.cpp \
    $(CPP_ROOT)/Mods/InputSettings.cpp \
    $(CPP_ROOT)/Mods/GameSettings.cpp \
    $(CPP_ROOT)/Mods/RenderOptions.cpp \
	$(CPP_ROOT)/Mods/Chat/ChatFont.cpp \
	$(CPP_ROOT)/Mods/Chat/PlayerEntityChatHud.cpp \
	$(CPP_ROOT)/Renderer/OpenGL/PlayerEntityChatHudRenderer.cpp \
	$(CPP_ROOT)/Mods/Credits.cpp \
	$(CPP_ROOT)/Mods/branding.cpp \
	$(CPP_ROOT)/Mods/Update/DesktopUpdate.cpp \
	$(CPP_ROOT)/Mods/Update/UpdateInstall.cpp \
	$(CPP_ROOT)/Mods/Launcher/native_launcher.cpp \
	$(CPP_ROOT)/Mods/Launcher/Gui/gui_model.cpp \
	$(CPP_ROOT)/Mods/Launcher/Gui/GuiTheme.cpp \
	$(CPP_ROOT)/Mods/Launcher/Gui/Win32HomeWindow.cpp \
    $(CPP_ROOT)/Mods/MapGen/mapgen.cpp \
    $(CPP_ROOT)/Mods/MapGen/image_decode.cpp \
    $(CPP_ROOT)/Entities/room_catalog.cpp \
    $(CPP_ROOT)/Entities/scene.cpp \
    $(CPP_ROOT)/Entities/RuntimeEntityBase.cpp \
    $(CPP_ROOT)/Entities/BeamProjectileEntity.cpp \
    $(CPP_ROOT)/Entities/ItemInstanceEntity.cpp \
    $(CPP_ROOT)/Entities/BeamEffectEntity.cpp \
    $(CPP_ROOT)/Entities/BombEntity.cpp \
    $(CPP_ROOT)/Entities/EnemyInstanceEntity.cpp \
    $(CPP_ROOT)/Entities/EnemySpawnEntity.cpp \
    $(CPP_ROOT)/Entities/EntityPool.cpp \
    $(CPP_ROOT)/Entities/Enemies/enemy_catalog.cpp \
    $(CPP_ROOT)/Entities/Enemies/enemy_instance.cpp \
    $(CPP_ROOT)/Entities/Enemies/enemy_combat.cpp \
    $(CPP_ROOT)/Entities/Enemies/00_WarWasp.cpp \
    $(CPP_ROOT)/Entities/Enemies/01_Zoomer.cpp \
    $(CPP_ROOT)/Entities/Enemies/02_Temroid.cpp \
    $(CPP_ROOT)/Entities/Enemies/03_Petrasyl1.cpp \
    $(CPP_ROOT)/Entities/Enemies/04_Petrasyl2.cpp \
    $(CPP_ROOT)/Entities/Enemies/05_Petrasyl3.cpp \
    $(CPP_ROOT)/Entities/Enemies/06_Petrasyl4.cpp \
    $(CPP_ROOT)/Entities/Enemies/10_BarbedWarWasp.cpp \
    $(CPP_ROOT)/Entities/Enemies/11_Shriekbat.cpp \
    $(CPP_ROOT)/Entities/Enemies/12_Geemer.cpp \
    $(CPP_ROOT)/Entities/Enemies/16_Blastcap.cpp \
    $(CPP_ROOT)/Entities/Enemies/18_AlimbicTurret.cpp \
    $(CPP_ROOT)/Entities/Enemies/19_Cretaphid.cpp \
    $(CPP_ROOT)/Entities/Enemies/20_CretaphidEye.cpp \
    $(CPP_ROOT)/Entities/Enemies/21_CretaphidCrystal.cpp \
    $(CPP_ROOT)/Entities/Enemies/23_PsychoBit.cpp \
    $(CPP_ROOT)/Entities/Enemies/24_Gorea1A.cpp \
    $(CPP_ROOT)/Entities/Enemies/25_GoreaHead.cpp \
    $(CPP_ROOT)/Entities/Enemies/26_GoreaArm.cpp \
    $(CPP_ROOT)/Entities/Enemies/27_GoreaLeg.cpp \
    $(CPP_ROOT)/Entities/Enemies/28_Gorea1B.cpp \
    $(CPP_ROOT)/Entities/Enemies/29_GoreaSealSphere1.cpp \
    $(CPP_ROOT)/Entities/Enemies/30_Trocra.cpp \
    $(CPP_ROOT)/Entities/Enemies/31_Gorea2.cpp \
    $(CPP_ROOT)/Entities/Enemies/32_GoreaSealSphere2.cpp \
    $(CPP_ROOT)/Entities/Enemies/33_GoreaMeteor.cpp \
    $(CPP_ROOT)/Entities/Enemies/35_Voldrum.cpp \
    $(CPP_ROOT)/Entities/Enemies/36_Voldrum.cpp \
    $(CPP_ROOT)/Entities/Enemies/37_Quadtroid.cpp \
    $(CPP_ROOT)/Entities/Enemies/38_CrashPillar.cpp \
    $(CPP_ROOT)/Entities/Enemies/39_FireSpawn.cpp \
    $(CPP_ROOT)/Entities/Enemies/40_EnemySpawner.cpp \
    $(CPP_ROOT)/Entities/Enemies/41_Slench.cpp \
    $(CPP_ROOT)/Entities/Enemies/42_SlenchShield.cpp \
    $(CPP_ROOT)/Entities/Enemies/43_SlenchNest.cpp \
    $(CPP_ROOT)/Entities/Enemies/44_SlenchSynapse.cpp \
    $(CPP_ROOT)/Entities/Enemies/45_SlenchTurret.cpp \
    $(CPP_ROOT)/Entities/Enemies/46_LesserIthrak.cpp \
    $(CPP_ROOT)/Entities/Enemies/47_GreaterIthrak.cpp \
    $(CPP_ROOT)/Entities/Enemies/49_ForceFieldLock.cpp \
    $(CPP_ROOT)/Entities/Enemies/50_HitZone.cpp \
    $(CPP_ROOT)/Entities/Enemies/51_CarnivorousPlant.cpp \
    $(CPP_ROOT)/Entities/Players/PlayerAi.cpp \
    $(CPP_ROOT)/Entities/Players/ai_funcs1.generated.cpp \
    $(CPP_ROOT)/Entities/Players/ai_funcs24.generated.cpp \
    $(CPP_ROOT)/Entities/Players/ai_funcs3.generated.cpp \
    $(CPP_ROOT)/Entities/Players/ai_helpers.generated.cpp \
    $(CPP_ROOT)/Entities/Players/player_profile.cpp \
    $(CPP_ROOT)/Entities/Players/PlayerEntity.cpp \
    $(CPP_ROOT)/Entities/Players/PlayerInput.cpp \
    $(CPP_ROOT)/Entities/Players/PlayerCollision.cpp \
    $(CPP_ROOT)/Entities/Players/DynamicLightEntity.cpp \
    $(CPP_ROOT)/Entities/Players/HalfturretEntity.cpp \
    $(CPP_ROOT)/Entities/Players/PlayerCamera.cpp \
    $(CPP_ROOT)/Entities/Players/PlayerDialog.cpp \
    $(CPP_ROOT)/Entities/Players/PlayerDraw.cpp \
    $(CPP_ROOT)/Entities/Players/PlayerHud.cpp \
    $(CPP_ROOT)/Entities/Players/player_hud_state.cpp \
    $(CPP_ROOT)/Entities/Players/player_statics.cpp \
    $(CPP_ROOT)/Entities/Players/hud_messages.cpp \
    $(CPP_ROOT)/Entities/Players/player_pause_map.cpp \
    $(CPP_ROOT)/Entities/Players/ai_globals.cpp \
    $(CPP_ROOT)/Entities/Players/ai_func_names.cpp \
    $(CPP_ROOT)/Entities/Players/PlayerPause.cpp \
    $(CPP_ROOT)/Entities/Players/PlayerProcess.cpp \
    $(CPP_ROOT)/Entities/Players/PlayerScan.cpp \
    $(CPP_ROOT)/Entities/Players/PlayerSound.cpp \
    $(CPP_ROOT)/Entities/gameplay.cpp \
    $(CPP_ROOT)/Entities/gameplay_combat.cpp \
    $(CPP_ROOT)/Entities/gameplay_enemies.cpp \
    $(CPP_ROOT)/Entities/gameplay_objectives.cpp \
    $(CPP_ROOT)/Entities/gameplay_items.cpp \
    $(CPP_ROOT)/Entities/gameplay_projectiles.cpp \
    $(CPP_ROOT)/Entities/gameplay_environment.cpp \
    $(CPP_ROOT)/Entities/gameplay_story.cpp \
    $(CPP_ROOT)/Entities/gameplay_snapshot.cpp \
    $(CPP_ROOT)/Entities/match_flow.cpp \
	$(CPP_ROOT)/Mods/Render/PreviewCamera.cpp \
	$(CPP_ROOT)/Mods/PauseMenu.cpp \
	$(CPP_ROOT)/Mods/ModEntry.cpp \
	$(CPP_ROOT)/Mods/ThumbnailMode.cpp \
	$(CPP_ROOT)/Mods/ThumbnailGenerator.cpp \
	$(CPP_ROOT)/Mods/ThumbnailLog.cpp \
	$(CPP_ROOT)/Mods/ThumbnailBatch.cpp \
	$(CPP_ROOT)/Mods/ThumbnailHost.cpp \
	$(CPP_ROOT)/Mods/Render/PlayerEntityProHud.cpp \
	$(CPP_ROOT)/Mods/Network/master_client.cpp \
    $(CPP_ROOT)/Mods/Network/master_server.cpp \
    $(CPP_ROOT)/Mods/Network/match_client.cpp \
	$(CPP_ROOT)/Mods/Network/net_client.cpp \
	$(CPP_ROOT)/Mods/Network/NetSession.cpp \
	$(CPP_ROOT)/Mods/Network/NetLag.cpp \
	$(CPP_ROOT)/Mods/Network/NetProbe.cpp \
	$(CPP_ROOT)/Mods/Network/NetHostSession.cpp \
	$(CPP_ROOT)/Mods/Network/NetHooks.cpp \
	$(CPP_ROOT)/Mods/Network/NetConnectCommand.cpp \
	$(CPP_ROOT)/Mods/Network/WeaponDps.cpp \
	$(CPP_ROOT)/Mods/Network/NetStatus.cpp \
	$(CPP_ROOT)/Mods/Network/NetMaster.cpp \
	$(CPP_ROOT)/Mods/Network/NetScoreboard.cpp \
	$(CPP_ROOT)/Mods/Network/NetSlotManager.cpp \
	$(CPP_ROOT)/Mods/Network/NetPlayerSetup.cpp \
	$(CPP_ROOT)/Mods/Network/NetDamage.cpp \
	$(CPP_ROOT)/Mods/Network/NetPlayerBridge.cpp \
	$(CPP_ROOT)/Mods/Network/NetMatchSync.cpp \
	$(CPP_ROOT)/Mods/Network/NetMatchEnd.cpp \
	$(CPP_ROOT)/Mods/Network/NetRoomChange.cpp \
	$(CPP_ROOT)/Mods/Network/NetLog.cpp \
	$(CPP_ROOT)/Mods/Network/NetDiagnostics.cpp \
	$(CPP_ROOT)/Mods/Network/NetTestScript.cpp \
	$(CPP_ROOT)/Mods/Network/NetLaunch.cpp \
	$(CPP_ROOT)/Mods/Network/PlayerEntityNetAim.cpp \
	$(CPP_ROOT)/Mods/Network/PlayerEntityNetHud.cpp \
	$(CPP_ROOT)/Utility/rng.cpp \
	$(CPP_ROOT)/Utility/parser.cpp \
	$(CPP_ROOT)/Utility/Compress.cpp \
	$(CPP_ROOT)/Utility/Output.cpp \
	$(CPP_ROOT)/Sound/sound.cpp \
    $(CPP_ROOT)/Sound/sdat.cpp \
    $(CPP_ROOT)/Sound/sound_resources.cpp \
    $(CPP_ROOT)/Formats/Sound.cpp \
    $(CPP_ROOT)/Sound/music.cpp \
    $(CPP_ROOT)/Sound/music_runtime.cpp \
    $(CPP_ROOT)/Sound/sseq_player.cpp \
    $(CPP_ROOT)/Sound/software_mixer.cpp \
	$(CPP_ROOT)/Sound/win32_audio_output.cpp \
    $(CPP_ROOT)/HUD/HudInfo.cpp

COMMON_SOURCES += $(PORT_BOUNDARY_SOURCES)
COMMON_OBJECTS := $(patsubst $(CPP_ROOT)/%.cpp,$(BUILD)/obj/%.o,$(COMMON_SOURCES))
LIBRARY := $(BUILD)/libfruityprime.a
LIBRARY_WIN := $(subst /,\,$(LIBRARY))
SERVER := $(BUILD)/FruityPrimeServer.exe
GAME := $(BUILD)/FruityPrime.exe

TEST_SOURCES := \
    $(CPP_ROOT)/Testing/ai_personality_tests.cpp \
    $(CPP_ROOT)/Testing/weapon_record_tests.cpp \
    $(CPP_ROOT)/Testing/camera_sequence_tests.cpp \
    $(CPP_ROOT)/Testing/camera_sequence_entity_tests.cpp \
	$(CPP_ROOT)/Testing/render_mods_tests.cpp \
	$(CPP_ROOT)/Testing/render_options_tests.cpp \
	$(CPP_ROOT)/Testing/renderer_tests.cpp \
	$(CPP_ROOT)/Testing/core_tests.cpp \
    $(CPP_ROOT)/Testing/protocol_tests.cpp \
    $(CPP_ROOT)/Testing/metadata_tests.cpp \
    $(CPP_ROOT)/Testing/enemy_catalog_tests.cpp \
    $(CPP_ROOT)/Testing/player_profile_tests.cpp \
    $(CPP_ROOT)/Testing/player_ai_tests.cpp \
    $(CPP_ROOT)/Testing/player_components_tests.cpp \
    $(CPP_ROOT)/Testing/export_tests.cpp \
    $(CPP_ROOT)/Testing/input_tests.cpp \
    $(CPP_ROOT)/Testing/settings_tests.cpp \
	$(CPP_ROOT)/Testing/gamepad_tests.cpp \
	$(CPP_ROOT)/Testing/mods_portable_tests.cpp \
	$(CPP_ROOT)/Testing/mod_entry_tests.cpp \
	$(CPP_ROOT)/Testing/launcher_tests.cpp \
	$(CPP_ROOT)/Testing/launcher_gui_tests.cpp \
    $(CPP_ROOT)/Testing/launcher_portable_tests.cpp \
	$(CPP_ROOT)/Testing/match_start_tests.cpp \
	$(CPP_ROOT)/Testing/text_launcher_tests.cpp \
    $(CPP_ROOT)/Testing/adventure_save_tests.cpp \
    $(CPP_ROOT)/Testing/format_tests.cpp \
    $(CPP_ROOT)/Testing/entity_class_tests.cpp \
    $(CPP_ROOT)/Testing/repack_entity_tests.cpp \
    $(CPP_ROOT)/Testing/repack_collision_tests.cpp \
    $(CPP_ROOT)/Testing/repack_model_tests.cpp \
    $(CPP_ROOT)/Testing/test_misc_tests.cpp \
    $(CPP_ROOT)/Testing/test_parse_tests.cpp \
    $(CPP_ROOT)/Testing/test_effects_tests.cpp \
    $(CPP_ROOT)/Testing/test_print_tests.cpp \
    $(CPP_ROOT)/Testing/test_overlay_tests.cpp \
    $(CPP_ROOT)/Testing/test_logic_tests.cpp \
    $(CPP_ROOT)/Testing/test_player_tests.cpp \
    $(CPP_ROOT)/Testing/analyzer_tests.cpp \
    $(CPP_ROOT)/Testing/test_weapons_tests.cpp \
    $(CPP_ROOT)/Testing/movie_tests.cpp \
    $(CPP_ROOT)/Testing/movie_playback_tests.cpp \
    $(CPP_ROOT)/Testing/effects_tests.cpp \
    $(CPP_ROOT)/Testing/effect_runtime_tests.cpp \
    $(CPP_ROOT)/Testing/node_data_tests.cpp \
    $(CPP_ROOT)/Testing/model_tests.cpp \
    $(CPP_ROOT)/Testing/model_catalog_tests.cpp \
    $(CPP_ROOT)/Testing/collision_tests.cpp \
    $(CPP_ROOT)/Testing/collision_runtime_tests.cpp \
    $(CPP_ROOT)/Testing/effect_func_tests.cpp \
    $(CPP_ROOT)/Testing/room_node_ref_tests.cpp \
    $(CPP_ROOT)/Testing/room_connector_tests.cpp \
    $(CPP_ROOT)/Testing/player_hud_state_tests.cpp \
    $(CPP_ROOT)/Testing/player_hud_message_tests.cpp \
    $(CPP_ROOT)/Testing/player_hud_locator_tests.cpp \
    $(CPP_ROOT)/Testing/enemy_helper_tests.cpp \
    $(CPP_ROOT)/Testing/player_statics_tests.cpp \
    $(CPP_ROOT)/Testing/hud_message_tests.cpp \
    $(CPP_ROOT)/Testing/pause_map_tests.cpp \
    $(CPP_ROOT)/Testing/player_actions_tests.cpp \
    $(CPP_ROOT)/Testing/player_hud_gate_tests.cpp \
    $(CPP_ROOT)/Testing/ai_globals_tests.cpp \
    $(CPP_ROOT)/Testing/sound_sample_tests.cpp \
    $(CPP_ROOT)/Testing/collision_volume_tests.cpp \
    $(CPP_ROOT)/Testing/player_controls_tests.cpp \
    $(CPP_ROOT)/Testing/entity_tests.cpp \
    $(CPP_ROOT)/Testing/runtime_entity_tests.cpp \
    $(CPP_ROOT)/Testing/static_entity_tests.cpp \
    $(CPP_ROOT)/Testing/scene_tests.cpp \
    $(CPP_ROOT)/Testing/gameplay_tests.cpp \
    $(CPP_ROOT)/Testing/match_flow_tests.cpp \
    $(CPP_ROOT)/Testing/room_catalog_tests.cpp \
    $(CPP_ROOT)/Testing/real_rom_tests.cpp \
    $(CPP_ROOT)/Testing/real_room_tests.cpp \
    $(CPP_ROOT)/Testing/network_tests.cpp \
    $(CPP_ROOT)/Testing/network_conditions_tests.cpp \
    $(CPP_ROOT)/Testing/net_feature_check_tests.cpp \
    $(CPP_ROOT)/Testing/weapon_dps_tests.cpp \
    $(CPP_ROOT)/Testing/chat_tests.cpp \
    $(CPP_ROOT)/Testing/demo_tests.cpp \
    $(CPP_ROOT)/Testing/hud_tests.cpp \
	$(CPP_ROOT)/Testing/strings_tests.cpp \
	$(CPP_ROOT)/Testing/sound_tests.cpp \
	$(CPP_ROOT)/Testing/sfx_mixer_tests.cpp \
	$(CPP_ROOT)/Testing/sfx_runtime_tests.cpp \
	$(CPP_ROOT)/Testing/sound_metadata_tests.cpp \
    $(CPP_ROOT)/Testing/mapgen_tests.cpp \
    $(CPP_ROOT)/Testing/mechanics_dump_tests.cpp \
    $(CPP_ROOT)/Testing/repack_access_tests.cpp \
    $(CPP_ROOT)/Testing/utility_tests.cpp \
	$(CPP_ROOT)/Testing/source_tree_contract_tests.cpp

TESTS := $(patsubst $(CPP_ROOT)/Testing/%.cpp,$(BUILD)/%.exe,$(TEST_SOURCES))
TEST_OBJECTS := $(patsubst $(CPP_ROOT)/Testing/%.cpp,$(BUILD)/tests/%.o,$(TEST_SOURCES))
TESTS_WIN := $(subst /,\,$(TESTS))
BUILD_WIN := $(subst /,\,$(BUILD))
PROGRAM_OBJECTS := $(BUILD)/obj/Utility/main.o $(BUILD)/obj/Platform/Windows/WinMain.o
DEPFILES := $(COMMON_OBJECTS:.o=.d) \
    $(TEST_OBJECTS:.o=.d) \
    $(PROGRAM_OBJECTS:.o=.d)

-include $(DEPFILES)

.PHONY: all server game tests test clean

all: server game tests

server: $(SERVER)

game: $(GAME)

tests: $(TESTS)

test: tests
	@set "FRUITY_PRIME_TEST_NDS=$(FRUITY_PRIME_TEST_NDS)" && for %%T in ($(TESTS_WIN)) do (%%T || exit /b 1)

$(LIBRARY): $(COMMON_OBJECTS) $(SOURCE_TREE_OBJECTS)
	@if exist "$(LIBRARY_WIN)" del /q "$(LIBRARY_WIN)"
	$(AR) rcs "$@" $(COMMON_OBJECTS) $(SOURCE_TREE_OBJECTS)

$(SERVER): $(BUILD)/obj/Utility/main.o $(LIBRARY)
	@if not exist "$(BUILD)" mkdir "$(BUILD)"
	$(CXX) $(LDFLAGS) -o "$@" "$<" "$(LIBRARY)" $(LDLIBS)

$(GAME): $(BUILD)/obj/Platform/Windows/WinMain.o $(LIBRARY)
	@if not exist "$(BUILD)" mkdir "$(BUILD)"
	$(CXX) $(LDFLAGS) -mwindows -o "$@" "$<" "$(LIBRARY)" \
		-lopengl32 $(LDLIBS)
	@rem The wordmark the launcher draws on its splash.  A build
	@rem taken out of the source tree has to carry it, or the front
	@rem screen falls back to the name in plain text.
	@if not exist "$(BUILD_WIN)\Assets" mkdir "$(BUILD_WIN)\Assets"
	@copy /y "..\src\MphRead\Assets\fruity-prime-logo.png" "$(BUILD_WIN)\Assets\" >nul
	@rem Inter, in the two weights the launcher draws with.  Loaded
	@rem privately at startup rather than installed; OFL.txt travels
	@rem with the files because the licence requires it.
	@if not exist "$(BUILD_WIN)\Assets\fonts" mkdir "$(BUILD_WIN)\Assets\fonts"
	@copy /y "..\assets\fonts\*.ttf" "$(BUILD_WIN)\Assets\fonts\" >nul
	@copy /y "..\assets\fonts\OFL.txt" "$(BUILD_WIN)\Assets\fonts\" >nul

$(BUILD)/obj/%.o: $(CPP_ROOT)/%.cpp
	@ if not exist "$(dir $@)" mkdir "$(dir $@)"
	$(CXX) $(CXXFLAGS) -c "$<" -o "$@"

$(BUILD)/tests/%.o: $(CPP_ROOT)/Testing/%.cpp
	@ if not exist "$(dir $@)" mkdir "$(dir $@)"
	$(CXX) $(CXXFLAGS) -c "$<" -o "$@"

$(BUILD)/%.exe: $(BUILD)/tests/%.o $(LIBRARY)
	@if not exist "$(BUILD)" mkdir "$(BUILD)"
	$(CXX) $(LDFLAGS) -o "$@" "$<" "$(LIBRARY)" $(LDLIBS)

clean:
	@echo Native direct-build outputs are under $(BUILD). Remove that exact
	@echo directory manually if a clean build is required.
