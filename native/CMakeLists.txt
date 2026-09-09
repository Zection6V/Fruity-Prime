cmake_minimum_required(VERSION 3.20)

project(FruityPrimeNative
    VERSION 0.1.0
    DESCRIPTION "Native C++ migration of Fruity Prime"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Match the standalone MinGW release link: the delivered Windows binaries
# should run beside the system DLLs without requiring a MinGW installation.
if(MINGW)
    add_link_options(-static-libgcc -static-libstdc++)
endif()

set(FRUITY_PRIME_CPP_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/MphRead.Native")
# Every header lives beside the code that owns it, so the source root is the
# only include directory and a header is reached by the directory it is in:
# #include "Formats/Types.hpp".  Two headers with one basename therefore
# cannot shadow each other.
set(FRUITY_PRIME_CPP_INCLUDE_DIR
    "${FRUITY_PRIME_CPP_ROOT}")
include("${CMAKE_CURRENT_SOURCE_DIR}/port-boundary-sources.cmake")

# Keep format and export implementations in the libraries that own their
# public APIs.  The direct MinGW archive still compiles every source unit in
# one archive, but the CMake build deliberately splits consumers so tests can
# link the smallest useful target.  Leaving these files in the generic core
# boundary made the format tests depend on unrelated runtime targets and
# caused duplicate or order-sensitive static-library resolution.
set(FRUITY_PRIME_FORMAT_BOUNDARY_SOURCES
    "${FRUITY_PRIME_CPP_ROOT}/Formats/AiPersonality.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Formats/Collision.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Formats/CollisionDetection.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Formats/EntityClass.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Formats/FhSound.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Formats/Model.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Formats/NodeData.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Formats/RawFormats.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Utility/Archive.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Utility/RepackEntity.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Utility/RepackCollision.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Utility/RepackModel.cpp")
set(FRUITY_PRIME_EXPORT_BOUNDARY_SOURCES
    "${FRUITY_PRIME_CPP_ROOT}/Export/Images.cpp")
set(FRUITY_PRIME_MAPGEN_BOUNDARY_SOURCES
    "${FRUITY_PRIME_CPP_ROOT}/Mods/MapGen/BuiltMap.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/MapGen/CustomRooms.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/MapGen/MapBuilder.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/MapGen/MapBundle.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/MapGen/MapCollisionPacker.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/MapGen/MapDefinition.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/MapGen/MapNodePacker.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/MapGen/MapPacker.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/MapGen/MapTextureBake.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/MapGen/MapTexturePack.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/MapGen/Q3Bsp.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/MapGen/Q3Convert.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/MapGen/Q3Import.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/MapGen/RawStructs.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/MapGen/RepackAccess.cpp")
set(FRUITY_PRIME_MAP_REPORT_BOUNDARY_SOURCES
    "${FRUITY_PRIME_CPP_ROOT}/Mods/MapGen/MapReport.cpp")
set(FRUITY_PRIME_NET_BOUNDARY_SOURCES
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/DedicatedServer.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/NetCheckClient.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/NetFeatureCheck.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/NetProtocol.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/NetTransport.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/MapRotation.cpp")
set(FRUITY_PRIME_DEMO_BOUNDARY_SOURCES
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/DemoInfo.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/DemoLibrary.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/DemoPlayback.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/DemoRecorder.cpp")
set(FRUITY_PRIME_SOUND_BOUNDARY_SOURCES
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Sound/SfxMixer.cpp")
list(REMOVE_ITEM FRUITY_PRIME_SOURCE_BOUNDARY_SOURCES
    ${FRUITY_PRIME_FORMAT_BOUNDARY_SOURCES}
    ${FRUITY_PRIME_EXPORT_BOUNDARY_SOURCES}
    ${FRUITY_PRIME_MAPGEN_BOUNDARY_SOURCES}
    ${FRUITY_PRIME_MAP_REPORT_BOUNDARY_SOURCES}
    ${FRUITY_PRIME_NET_BOUNDARY_SOURCES}
    ${FRUITY_PRIME_DEMO_BOUNDARY_SOURCES}
    ${FRUITY_PRIME_SOUND_BOUNDARY_SOURCES})

# The Windows build is intentionally self-contained.  When CMake itself is
# launched from an MSYS installation, find_package(ZLIB) can otherwise pick
# MSYS's import library even though the compiler is the user's standalone
# MinGW at C:\mingw64.  Prefer the compiler's sibling static zlib when it is
# available so source-tree builds and CTest do not depend on zlib1.dll being
# on PATH.
if(MINGW)
    get_filename_component(FRUITY_PRIME_MINGW_BIN
        "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(FRUITY_PRIME_MINGW_ROOT
        "${FRUITY_PRIME_MINGW_BIN}" DIRECTORY)
    set(FRUITY_PRIME_MINGW_ZLIB_INCLUDE
        "${FRUITY_PRIME_MINGW_ROOT}/x86_64-w64-mingw32/include")
    set(FRUITY_PRIME_MINGW_ZLIB_LIBRARY
        "${FRUITY_PRIME_MINGW_ROOT}/x86_64-w64-mingw32/lib/libz.a")
    if(EXISTS "${FRUITY_PRIME_MINGW_ZLIB_INCLUDE}/zlib.h"
       AND EXISTS "${FRUITY_PRIME_MINGW_ZLIB_LIBRARY}")
        set(ZLIB_USE_STATIC_LIBS ON)
        set(ZLIB_INCLUDE_DIR "${FRUITY_PRIME_MINGW_ZLIB_INCLUDE}"
            CACHE PATH "MinGW zlib include directory" FORCE)
        set(ZLIB_LIBRARY "${FRUITY_PRIME_MINGW_ZLIB_LIBRARY}"
            CACHE FILEPATH "MinGW zlib static library" FORCE)
        set(ZLIB_LIBRARY_RELEASE "${FRUITY_PRIME_MINGW_ZLIB_LIBRARY}"
            CACHE FILEPATH "MinGW zlib release library" FORCE)
        set(ZLIB_LIBRARY_DEBUG "${FRUITY_PRIME_MINGW_ZLIB_LIBRARY}"
            CACHE FILEPATH "MinGW zlib debug library" FORCE)
    endif()
endif()

find_package(ZLIB REQUIRED)

add_library(fruity_prime_read STATIC
    "${FRUITY_PRIME_CPP_ROOT}/Read.cpp")
target_include_directories(fruity_prime_read
    PUBLIC
        ${FRUITY_PRIME_CPP_INCLUDE_DIR})
if(MSVC)
    target_compile_options(fruity_prime_read PRIVATE /W4 /permissive-)
else()
    target_compile_options(fruity_prime_read PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Effects and the gameplay/renderer libraries share the deterministic RNG.
# Keep it in its own leaf target so static-library consumers do not depend on
# the source-file order that happens to make the standalone archive link.
add_library(fruity_prime_rng STATIC
    "${FRUITY_PRIME_CPP_ROOT}/Utility/rng.cpp")
target_include_directories(fruity_prime_rng
    PUBLIC
        ${FRUITY_PRIME_CPP_INCLUDE_DIR})
if(MSVC)
    target_compile_options(fruity_prime_rng PRIVATE /W4 /permissive-)
else()
    target_compile_options(fruity_prime_rng PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(fruity_prime_game_state STATIC
    "${FRUITY_PRIME_CPP_ROOT}/GameState.cpp")
target_include_directories(fruity_prime_game_state
    PUBLIC
        ${FRUITY_PRIME_CPP_INCLUDE_DIR})
if(MSVC)
    target_compile_options(fruity_prime_game_state PRIVATE /W4 /permissive-)
else()
    target_compile_options(fruity_prime_game_state PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(fruity_prime_core STATIC
    "${FRUITY_PRIME_CPP_ROOT}/Features.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Memory.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/MemoryArrays.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/MemoryClasses.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Messaging.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Renderer.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Scene.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/SceneSetup.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Shaders.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/EntityBase.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/PlatformEntity.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/ObjectEntity.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/PlayerSpawnEntity.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/ItemSpawnEntity.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/DoorEntity.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/VolumeEntity.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/AreaVolumeEntity.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/TriggerVolumeEntity.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/JumpPadEntity.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/MorphCameraEntity.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/NodeDefenseEntity.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/FlagBaseEntity.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/LightSourceEntity.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/TeleporterEntity.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/OctolithFlagEntity.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/ArtifactEntity.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/ForceFieldEntity.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/PointModuleEntity.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/RoomEntity.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/CamSeq/camera_sequence_entity.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/CamSeq/CameraSequence.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/CamSeq/CamSeqEntity.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Render/PlayerEntityIconBounds.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Render/PreviewCamera.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/PauseMenu.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/ModEntry.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/ThumbnailMode.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/ThumbnailGenerator.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/ThumbnailLog.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/ThumbnailBatch.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/ThumbnailHost.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Render/PlayerEntityProHud.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Credits.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Update/DesktopUpdate.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Update/UpdateInstall.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Menu.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/MenuModelMetadata.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Selection.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Strings.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Program.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Test.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Utility/parser.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Utility/Output.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/branding.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/NetStatus.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/NetMaster.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/NetScoreboard.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/NetSlotManager.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/NetPlayerSetup.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/NetDamage.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/NetPlayerBridge.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/NetMatchSync.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/NetMatchEnd.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/NetRoomChange.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/NetLog.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/NetDiagnostics.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/NetTestScript.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/PlayerEntityNetAim.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/NetHostSession.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/NetHooks.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/NetConnectCommand.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/WeaponDps.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/PlayerEntityNetHud.cpp")
target_include_directories(fruity_prime_core
    PUBLIC
        ${FRUITY_PRIME_CPP_INCLUDE_DIR}
        ${FRUITY_PRIME_CPP_ROOT})
target_link_libraries(fruity_prime_core PUBLIC
    fruity_prime_read fruity_prime_rng fruity_prime_game_state ZLIB::ZLIB)
if(WIN32)
    target_link_libraries(fruity_prime_core PUBLIC winhttp)
elseif(UNIX)
    target_link_libraries(fruity_prime_core PUBLIC ${CMAKE_DL_LIBS})
endif()
target_sources(fruity_prime_core PRIVATE
    ${FRUITY_PRIME_SOURCE_BOUNDARY_SOURCES})
if(MSVC)
    target_compile_options(fruity_prime_core PRIVATE /W4 /permissive-)
else()
target_compile_options(fruity_prime_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(fruity_prime_formats STATIC
    "${FRUITY_PRIME_CPP_ROOT}/Formats/camera_sequence.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Assets/archive.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Assets/compression.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Utility/Compress.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Formats/Entity.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Formats/EntityEnemy.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Formats/effects.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Formats/effect_runtime.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Formats/frontend.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Assets/game_assets.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Assets/model_catalog.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Formats/model_instance.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Formats/movie.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Formats/paths.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Assets/nds_rom.cpp"
    ${FRUITY_PRIME_FORMAT_BOUNDARY_SOURCES}
)

target_include_directories(fruity_prime_formats
    PUBLIC
        ${FRUITY_PRIME_CPP_INCLUDE_DIR}
)
target_link_libraries(fruity_prime_formats PUBLIC
    fruity_prime_read fruity_prime_rng ZLIB::ZLIB)

if(MSVC)
    target_compile_options(fruity_prime_formats PRIVATE /W4 /permissive-)
else()
    target_compile_options(fruity_prime_formats PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(fruity_prime_metadata STATIC
    "${FRUITY_PRIME_CPP_ROOT}/Metadata/metadata.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Metadata/MetadataClasses.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Metadata/MetadataModels.generated.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Metadata/MetadataValues.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Metadata/Player.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Metadata/Weapons.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Metadata/FrontendMeta.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Metadata/FrontendMeta.generated.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Metadata/Rooms.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Metadata/Enemies.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Metadata/entity_metadata.cpp")
target_include_directories(fruity_prime_metadata
    PUBLIC
        ${FRUITY_PRIME_CPP_INCLUDE_DIR}
)
if(MSVC)
    target_compile_options(fruity_prime_metadata PRIVATE /W4 /permissive-)
else()
    target_compile_options(fruity_prime_metadata PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(fruity_prime_sound STATIC
    "${FRUITY_PRIME_CPP_ROOT}/Sound/sound.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Sound/sdat.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Sound/sound_resources.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Formats/Sound.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Sound/music.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Sound/music_runtime.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Sound/sseq_player.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Sound/software_mixer.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Sound/win32_audio_output.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Sound/Sfx.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Metadata/SoundMeta.cpp"
    ${FRUITY_PRIME_SOUND_BOUNDARY_SOURCES}
    )
target_include_directories(fruity_prime_sound
    PUBLIC
        ${FRUITY_PRIME_CPP_INCLUDE_DIR}
        ${FRUITY_PRIME_CPP_ROOT})
target_link_libraries(fruity_prime_sound PUBLIC fruity_prime_formats)
if(WIN32)
    target_link_libraries(fruity_prime_sound PUBLIC winmm)
endif()
if(MSVC)
    target_compile_options(fruity_prime_sound PRIVATE /W4 /permissive-)
else()
    target_compile_options(fruity_prime_sound PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(fruity_prime_export STATIC
    "${FRUITY_PRIME_CPP_ROOT}/Export/Collada.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Export/Scripting.cpp"
    ${FRUITY_PRIME_EXPORT_BOUNDARY_SOURCES})
target_include_directories(fruity_prime_export
    PUBLIC
        ${FRUITY_PRIME_CPP_INCLUDE_DIR}
)
target_link_libraries(fruity_prime_export PUBLIC fruity_prime_formats)
target_link_libraries(fruity_prime_core PUBLIC fruity_prime_export)
if(MSVC)
    target_compile_options(fruity_prime_export PRIVATE /W4 /permissive-)
else()
    target_compile_options(fruity_prime_export PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(fruity_prime_input STATIC
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Input/GamepadInput.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Input/GamepadState.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Input/PadBindings.cpp")
target_include_directories(fruity_prime_input
    PUBLIC
    ${FRUITY_PRIME_CPP_INCLUDE_DIR}
)
target_link_libraries(fruity_prime_core PUBLIC fruity_prime_input)
if(MSVC)
    target_compile_options(fruity_prime_input PRIVATE /W4 /permissive-)
else()
    target_compile_options(fruity_prime_input PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(fruity_prime_settings STATIC
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Settings/settings_common.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/InputSettings.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/GameSettings.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/RenderOptions.cpp")
target_include_directories(fruity_prime_settings
    PUBLIC
        ${FRUITY_PRIME_CPP_INCLUDE_DIR}
)
if(MSVC)
    target_compile_options(fruity_prime_settings PRIVATE /W4 /permissive-)
else()
    target_compile_options(fruity_prime_settings PRIVATE -Wall -Wextra -Wpedantic)
endif()

# SettingsView.cpp is part of the mirrored source boundary compiled into the
# core archive. Keep its storage/render-option implementations transitively
# available to every core consumer, including the launcher GUI tests; the
# direct archive build already links the complete native source set.
target_link_libraries(fruity_prime_core PUBLIC fruity_prime_settings)

add_library(fruity_prime_gamepad STATIC
    # Probe uses the device implementation below; keep it first so a MinGW
    # archive scan can resolve the implementation in the same library.
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Input/GamepadProbe.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Input/GamepadDesktop.cpp")
target_include_directories(fruity_prime_gamepad
    PUBLIC
        ${FRUITY_PRIME_CPP_INCLUDE_DIR}
)
target_link_libraries(fruity_prime_gamepad PUBLIC fruity_prime_input)
if(MSVC)
    target_compile_options(fruity_prime_gamepad PRIVATE /W4 /permissive-)
else()
    target_compile_options(fruity_prime_gamepad PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(fruity_prime_launcher STATIC
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Launcher/native_launcher.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Launcher/Gui/gui_model.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Launcher/Gui/GuiTheme.cpp"
    )
target_include_directories(fruity_prime_launcher
    PUBLIC
        ${FRUITY_PRIME_CPP_INCLUDE_DIR}
)
target_link_libraries(fruity_prime_launcher PUBLIC fruity_prime_core
    fruity_prime_gameplay)
if(MSVC)
    target_compile_options(fruity_prime_launcher PRIVATE /W4 /permissive-)
else()
    target_compile_options(fruity_prime_launcher PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(WIN32)
    target_link_libraries(fruity_prime_launcher PUBLIC comdlg32)
endif()

add_library(fruity_prime_mapgen STATIC
    "${FRUITY_PRIME_CPP_ROOT}/Mods/MapGen/mapgen.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/MapGen/image_decode.cpp"
    # CustomRooms resolves the same launch-relative paths as the console
    # utility. Keep this leaf implementation available without pulling the
    # full core/runtime graph into MapGen tests.
    "${FRUITY_PRIME_CPP_ROOT}/Utility/Console.cpp"
    ${FRUITY_PRIME_MAPGEN_BOUNDARY_SOURCES})
target_include_directories(fruity_prime_mapgen
    PUBLIC
        ${FRUITY_PRIME_CPP_INCLUDE_DIR}
        ${FRUITY_PRIME_CPP_ROOT})
target_link_libraries(fruity_prime_mapgen PUBLIC
    fruity_prime_formats ZLIB::ZLIB)
if(WIN32)
    target_link_libraries(fruity_prime_mapgen PUBLIC windowscodecs ole32 uuid)
endif()
if(MSVC)
    target_compile_options(fruity_prime_mapgen PRIVATE /W4 /permissive-)
else()
    target_compile_options(fruity_prime_mapgen PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(fruity_prime_scene STATIC
    "${FRUITY_PRIME_CPP_ROOT}/Entities/room_catalog.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/scene.cpp"
)
target_include_directories(fruity_prime_scene
    PUBLIC
        ${FRUITY_PRIME_CPP_INCLUDE_DIR}
)
target_link_libraries(fruity_prime_scene PUBLIC fruity_prime_formats)
target_link_libraries(fruity_prime_scene PUBLIC fruity_prime_mapgen)
if(MSVC)
    target_compile_options(fruity_prime_scene PRIVATE /W4 /permissive-)
else()
    target_compile_options(fruity_prime_scene PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(fruity_prime_gameplay STATIC
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/enemy_catalog.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/enemy_instance.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/enemy_combat.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/00_WarWasp.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/01_Zoomer.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/02_Temroid.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/03_Petrasyl1.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/04_Petrasyl2.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/05_Petrasyl3.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/06_Petrasyl4.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/10_BarbedWarWasp.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/11_Shriekbat.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/12_Geemer.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/16_Blastcap.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/18_AlimbicTurret.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/19_Cretaphid.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/20_CretaphidEye.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/21_CretaphidCrystal.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/23_PsychoBit.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/24_Gorea1A.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/25_GoreaHead.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/26_GoreaArm.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/27_GoreaLeg.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/28_Gorea1B.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/29_GoreaSealSphere1.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/30_Trocra.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/31_Gorea2.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/32_GoreaSealSphere2.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/33_GoreaMeteor.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/35_Voldrum.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/36_Voldrum.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/37_Quadtroid.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/38_CrashPillar.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/39_FireSpawn.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/40_EnemySpawner.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/41_Slench.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/42_SlenchShield.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/43_SlenchNest.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/44_SlenchSynapse.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/45_SlenchTurret.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/46_LesserIthrak.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/47_GreaterIthrak.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/49_ForceFieldLock.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/50_HitZone.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Enemies/51_CarnivorousPlant.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Players/PlayerAi.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Players/player_profile.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Players/PlayerEntity.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Players/PlayerInput.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Players/PlayerCollision.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Players/DynamicLightEntity.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Players/HalfturretEntity.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Players/PlayerCamera.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Players/PlayerDialog.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Players/PlayerDraw.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Players/PlayerHud.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Players/PlayerPause.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Players/PlayerProcess.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Players/PlayerScan.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/Players/PlayerSound.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/RuntimeEntityBase.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/BeamProjectileEntity.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/ItemInstanceEntity.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/BeamEffectEntity.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/BombEntity.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/EnemyInstanceEntity.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/EnemySpawnEntity.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/EntityPool.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/gameplay.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/gameplay_combat.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/gameplay_enemies.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/gameplay_objectives.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/gameplay_items.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/gameplay_projectiles.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/gameplay_environment.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/gameplay_story.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/gameplay_snapshot.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Entities/match_flow.cpp"
)
target_include_directories(fruity_prime_gameplay
    PUBLIC
        ${FRUITY_PRIME_CPP_INCLUDE_DIR}
)
target_link_libraries(fruity_prime_gameplay PUBLIC fruity_prime_scene fruity_prime_net)
target_link_libraries(fruity_prime_gameplay PUBLIC
    fruity_prime_metadata fruity_prime_game_state)
if(MSVC)
    target_compile_options(fruity_prime_gameplay PRIVATE /W4 /permissive-)
else()
    target_compile_options(fruity_prime_gameplay PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(fruity_prime_map_report STATIC
    ${FRUITY_PRIME_MAP_REPORT_BOUNDARY_SOURCES})
target_include_directories(fruity_prime_map_report
    PUBLIC
        ${FRUITY_PRIME_CPP_INCLUDE_DIR}
        ${FRUITY_PRIME_CPP_ROOT})
target_link_libraries(fruity_prime_map_report PUBLIC
    fruity_prime_scene fruity_prime_mapgen)
if(MSVC)
    target_compile_options(fruity_prime_map_report PRIVATE /W4 /permissive-)
else()
    target_compile_options(fruity_prime_map_report PRIVATE -Wall -Wextra -Wpedantic)
endif()

# The root-level Scene facade is a consumer of the decoded room and gameplay
# libraries. Keep that dependency explicit for CMake users; the direct
# MinGW archive build resolves the same objects in one native library.
target_link_libraries(fruity_prime_core PUBLIC fruity_prime_gameplay)

add_library(fruity_prime_net STATIC
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/master_client.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/master_server.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/match_client.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/net_client.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/NetSession.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/NetLag.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/NetProbe.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/NetLaunch.cpp"
    ${FRUITY_PRIME_NET_BOUNDARY_SOURCES}
)

target_include_directories(fruity_prime_net
    PUBLIC
        ${FRUITY_PRIME_CPP_INCLUDE_DIR}
)

target_link_libraries(fruity_prime_net PUBLIC fruity_prime_formats)

if(MSVC)
    target_compile_options(fruity_prime_net PRIVATE /W4 /permissive-)
else()
    target_compile_options(fruity_prime_net PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    target_compile_definitions(fruity_prime_net PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_link_libraries(fruity_prime_net PUBLIC ws2_32)
elseif(UNIX)
    target_link_libraries(fruity_prime_net PUBLIC pthread)
endif()

add_library(fruity_prime_chat STATIC
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Chat/ChatBox.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Chat/ChatFont.cpp"
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Chat/PlayerEntityChatHud.cpp"
)
target_include_directories(fruity_prime_chat
    PUBLIC
        ${FRUITY_PRIME_CPP_INCLUDE_DIR}
)
target_link_libraries(fruity_prime_chat PUBLIC fruity_prime_net)
if(MSVC)
    target_compile_options(fruity_prime_chat PRIVATE /W4 /permissive-)
else()
    target_compile_options(fruity_prime_chat PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(fruity_prime_demo STATIC
    "${FRUITY_PRIME_CPP_ROOT}/Mods/Network/DemoFile.cpp"
    ${FRUITY_PRIME_DEMO_BOUNDARY_SOURCES}
)
target_include_directories(fruity_prime_demo
    PUBLIC
        ${FRUITY_PRIME_CPP_INCLUDE_DIR}
)
target_link_libraries(fruity_prime_demo PUBLIC fruity_prime_net)
if(MSVC)
    target_compile_options(fruity_prime_demo PRIVATE /W4 /permissive-)
else()
    target_compile_options(fruity_prime_demo PRIVATE -Wall -Wextra -Wpedantic)
endif()

# DemoPickerView is part of the launcher GUI boundary and uses the same
# recording descriptions as the portable demo library.
target_link_libraries(fruity_prime_launcher PUBLIC fruity_prime_demo)

add_executable(FruityPrimeServer "${FRUITY_PRIME_CPP_ROOT}/Utility/main.cpp")
target_link_libraries(FruityPrimeServer PRIVATE fruity_prime_net fruity_prime_scene
    fruity_prime_gameplay fruity_prime_demo fruity_prime_export fruity_prime_sound
    fruity_prime_mapgen fruity_prime_map_report fruity_prime_gamepad
    fruity_prime_core)

if(WIN32)
    add_executable(FruityPrime WIN32
        "${FRUITY_PRIME_CPP_ROOT}/HUD/game_main.cpp"
        "${FRUITY_PRIME_CPP_ROOT}/HUD/HudInfo.cpp")
    target_include_directories(FruityPrime PRIVATE
        "${FRUITY_PRIME_CPP_ROOT}")
    target_link_libraries(FruityPrime PRIVATE fruity_prime_gameplay fruity_prime_input
        fruity_prime_gamepad fruity_prime_launcher fruity_prime_settings
        fruity_prime_chat fruity_prime_demo
        fruity_prime_export fruity_prime_sound fruity_prime_core
        opengl32 gdi32 user32)
    target_link_libraries(FruityPrime PRIVATE shell32)
endif()

# Fruity Prime keeps the historical package names: Windows has a distinct
# console server binary, while Linux and ARM64 publish the single `FruityPrime`
# executable. The native game target is currently a Windows OpenGL/session
# preview while the cross-platform renderer and full launcher are still
# managed.
if(WIN32)
    set_target_properties(FruityPrimeServer PROPERTIES OUTPUT_NAME FruityPrimeServer)
else()
    set_target_properties(FruityPrimeServer PROPERTIES OUTPUT_NAME FruityPrime)
endif()

if(MSVC)
    target_compile_options(FruityPrimeServer PRIVATE /W4 /permissive-)
else()
    target_compile_options(FruityPrimeServer PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    if(MSVC)
        target_compile_options(FruityPrime PRIVATE /W4 /permissive-)
    else()
        target_compile_options(FruityPrime PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    # Release builds define NDEBUG globally, but these native tests use
    # assert-based behavioral checks. Keep assertions enabled for every test
    # target so a green Release CTest run remains meaningful.
    if(MSVC)
        add_compile_options(/U NDEBUG)
    else()
        add_compile_options(-UNDEBUG)
    endif()

    set(FRUITY_PRIME_TEST_NDS "" CACHE FILEPATH
        "Optional real NDS ROM used by the native integration test")

    add_executable(FruityPrimeNativeProtocolTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/protocol_tests.cpp")
    target_link_libraries(FruityPrimeNativeProtocolTests PRIVATE fruity_prime_net)
    add_test(NAME FruityPrimeNativeProtocolTests COMMAND FruityPrimeNativeProtocolTests)

    add_executable(FruityPrimeNativeAiPersonalityTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/ai_personality_tests.cpp")
    target_link_libraries(FruityPrimeNativeAiPersonalityTests PRIVATE
        fruity_prime_formats)
    add_test(NAME FruityPrimeNativeAiPersonalityTests
        COMMAND FruityPrimeNativeAiPersonalityTests)
    if(FRUITY_PRIME_TEST_NDS)
        set_tests_properties(FruityPrimeNativeAiPersonalityTests PROPERTIES
            ENVIRONMENT "FRUITY_PRIME_TEST_NDS=${FRUITY_PRIME_TEST_NDS}")
    endif()

    add_executable(FruityPrimeNativeCameraSequenceTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/camera_sequence_tests.cpp")
    target_link_libraries(FruityPrimeNativeCameraSequenceTests PRIVATE
        fruity_prime_formats)
    add_test(NAME FruityPrimeNativeCameraSequenceTests
        COMMAND FruityPrimeNativeCameraSequenceTests)
    if(FRUITY_PRIME_TEST_NDS)
        set_tests_properties(FruityPrimeNativeCameraSequenceTests PROPERTIES
            ENVIRONMENT "FRUITY_PRIME_TEST_NDS=${FRUITY_PRIME_TEST_NDS}")
    endif()

    add_executable(FruityPrimeNativeCameraSequenceEntityTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/camera_sequence_entity_tests.cpp")
    target_link_libraries(FruityPrimeNativeCameraSequenceEntityTests PRIVATE
        fruity_prime_core)
    add_test(NAME FruityPrimeNativeCameraSequenceEntityTests
        COMMAND FruityPrimeNativeCameraSequenceEntityTests)

    add_executable(FruityPrimeNativeRenderModsTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/render_mods_tests.cpp")
    target_link_libraries(FruityPrimeNativeRenderModsTests PRIVATE
        fruity_prime_core)
    add_test(NAME FruityPrimeNativeRenderModsTests
        COMMAND FruityPrimeNativeRenderModsTests)

    add_executable(FruityPrimeNativeRenderOptionsTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/render_options_tests.cpp")
    target_link_libraries(FruityPrimeNativeRenderOptionsTests PRIVATE
        fruity_prime_core fruity_prime_settings)
    add_test(NAME FruityPrimeNativeRenderOptionsTests
        COMMAND FruityPrimeNativeRenderOptionsTests)

    add_executable(FruityPrimeNativeRendererTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/renderer_tests.cpp")
    target_link_libraries(FruityPrimeNativeRendererTests PRIVATE
        fruity_prime_core)
    add_test(NAME FruityPrimeNativeRendererTests
        COMMAND FruityPrimeNativeRendererTests)

    add_executable(FruityPrimeNativeCoreTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/core_tests.cpp")
    target_link_libraries(FruityPrimeNativeCoreTests PRIVATE fruity_prime_core)
    add_test(NAME FruityPrimeNativeCoreTests COMMAND FruityPrimeNativeCoreTests)

    add_executable(FruityPrimeNativeMetadataTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/metadata_tests.cpp")
    target_link_libraries(FruityPrimeNativeMetadataTests PRIVATE fruity_prime_metadata)
    add_test(NAME FruityPrimeNativeMetadataTests COMMAND FruityPrimeNativeMetadataTests)

    add_executable(FruityPrimeNativeEnemyCatalogTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/enemy_catalog_tests.cpp")
    target_link_libraries(FruityPrimeNativeEnemyCatalogTests PRIVATE
        fruity_prime_gameplay)
    add_test(NAME FruityPrimeNativeEnemyCatalogTests
        COMMAND FruityPrimeNativeEnemyCatalogTests)

    add_executable(FruityPrimeNativePlayerProfileTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/player_profile_tests.cpp")
    target_link_libraries(FruityPrimeNativePlayerProfileTests PRIVATE
        fruity_prime_gameplay)
    add_test(NAME FruityPrimeNativePlayerProfileTests
        COMMAND FruityPrimeNativePlayerProfileTests)

    add_executable(FruityPrimeNativePlayerAiTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/player_ai_tests.cpp")
    target_link_libraries(FruityPrimeNativePlayerAiTests PRIVATE
        fruity_prime_gameplay)
    add_test(NAME FruityPrimeNativePlayerAiTests
        COMMAND FruityPrimeNativePlayerAiTests)
    if(FRUITY_PRIME_TEST_NDS)
        set_tests_properties(FruityPrimeNativePlayerAiTests PROPERTIES
            ENVIRONMENT "FRUITY_PRIME_TEST_NDS=${FRUITY_PRIME_TEST_NDS}")
    endif()

    add_executable(FruityPrimeNativePlayerComponentsTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/player_components_tests.cpp")
    target_link_libraries(FruityPrimeNativePlayerComponentsTests PRIVATE
        fruity_prime_gameplay fruity_prime_core)
    add_test(NAME FruityPrimeNativePlayerComponentsTests
        COMMAND FruityPrimeNativePlayerComponentsTests)

    add_executable(FruityPrimeNativeExportTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/export_tests.cpp")
    target_link_libraries(FruityPrimeNativeExportTests PRIVATE
        fruity_prime_export fruity_prime_metadata)
    add_test(NAME FruityPrimeNativeExportTests COMMAND FruityPrimeNativeExportTests)

    add_executable(FruityPrimeNativeInputTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/input_tests.cpp")
    target_link_libraries(FruityPrimeNativeInputTests PRIVATE fruity_prime_input)
    add_test(NAME FruityPrimeNativeInputTests COMMAND FruityPrimeNativeInputTests)

    add_executable(FruityPrimeNativeSettingsTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/settings_tests.cpp")
    target_link_libraries(FruityPrimeNativeSettingsTests PRIVATE fruity_prime_settings)
    add_test(NAME FruityPrimeNativeSettingsTests COMMAND FruityPrimeNativeSettingsTests)

    add_executable(FruityPrimeNativeGamepadTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/gamepad_tests.cpp")
    target_link_libraries(FruityPrimeNativeGamepadTests PRIVATE fruity_prime_gamepad)
    add_test(NAME FruityPrimeNativeGamepadTests COMMAND FruityPrimeNativeGamepadTests)

    add_executable(FruityPrimeNativeModsPortableTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/mods_portable_tests.cpp")
    target_link_libraries(FruityPrimeNativeModsPortableTests PRIVATE fruity_prime_core)
    add_test(NAME FruityPrimeNativeModsPortableTests
        COMMAND FruityPrimeNativeModsPortableTests)

    add_executable(FruityPrimeNativeModEntryTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/mod_entry_tests.cpp")
    target_link_libraries(FruityPrimeNativeModEntryTests PRIVATE fruity_prime_core)
    add_test(NAME FruityPrimeNativeModEntryTests
        COMMAND FruityPrimeNativeModEntryTests)

    add_executable(FruityPrimeNativeUtilityTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/utility_tests.cpp")
    target_link_libraries(FruityPrimeNativeUtilityTests PRIVATE fruity_prime_core)
    add_test(NAME FruityPrimeNativeUtilityTests COMMAND FruityPrimeNativeUtilityTests)

    add_executable(FruityPrimeNativeLauncherTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/launcher_tests.cpp")
    target_link_libraries(FruityPrimeNativeLauncherTests PRIVATE fruity_prime_launcher)
    add_test(NAME FruityPrimeNativeLauncherTests COMMAND FruityPrimeNativeLauncherTests)

    add_executable(FruityPrimeNativeLauncherGuiTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/launcher_gui_tests.cpp")
    target_link_libraries(FruityPrimeNativeLauncherGuiTests PRIVATE
        fruity_prime_launcher)
    add_test(NAME FruityPrimeNativeLauncherGuiTests
        COMMAND FruityPrimeNativeLauncherGuiTests)

    add_executable(FruityPrimeNativeLauncherPortableTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/launcher_portable_tests.cpp")
    target_link_libraries(FruityPrimeNativeLauncherPortableTests PRIVATE
        fruity_prime_launcher fruity_prime_formats fruity_prime_metadata)
    add_test(NAME FruityPrimeNativeLauncherPortableTests
        COMMAND FruityPrimeNativeLauncherPortableTests)

    add_executable(FruityPrimeNativeMatchStartTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/match_start_tests.cpp")
    target_link_libraries(FruityPrimeNativeMatchStartTests PRIVATE
        fruity_prime_launcher fruity_prime_gameplay)
    add_test(NAME FruityPrimeNativeMatchStartTests
        COMMAND FruityPrimeNativeMatchStartTests)
    if(FRUITY_PRIME_TEST_NDS)
        set_tests_properties(FruityPrimeNativeMatchStartTests PROPERTIES
            ENVIRONMENT "FRUITY_PRIME_TEST_NDS=${FRUITY_PRIME_TEST_NDS}")
    endif()

    add_executable(FruityPrimeNativeTextLauncherTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/text_launcher_tests.cpp")
    target_link_libraries(FruityPrimeNativeTextLauncherTests PRIVATE
        fruity_prime_launcher fruity_prime_formats fruity_prime_metadata)
    add_test(NAME FruityPrimeNativeTextLauncherTests
        COMMAND FruityPrimeNativeTextLauncherTests)

    add_executable(FruityPrimeNativeAdventureSaveTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/adventure_save_tests.cpp")
    target_link_libraries(FruityPrimeNativeAdventureSaveTests PRIVATE
        fruity_prime_launcher fruity_prime_core)
    add_test(NAME FruityPrimeNativeAdventureSaveTests
        COMMAND FruityPrimeNativeAdventureSaveTests)

    add_executable(FruityPrimeNativeFormatTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/format_tests.cpp")
    target_link_libraries(FruityPrimeNativeFormatTests PRIVATE fruity_prime_formats)
    add_test(NAME FruityPrimeNativeFormatTests COMMAND FruityPrimeNativeFormatTests)

    add_executable(FruityPrimeNativeEntityClassTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/entity_class_tests.cpp")
    target_link_libraries(FruityPrimeNativeEntityClassTests PRIVATE
        fruity_prime_formats)
    add_test(NAME FruityPrimeNativeEntityClassTests
        COMMAND FruityPrimeNativeEntityClassTests)

    add_executable(FruityPrimeNativeRepackEntityTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/repack_entity_tests.cpp")
    target_link_libraries(FruityPrimeNativeRepackEntityTests PRIVATE
        fruity_prime_formats)
    add_test(NAME FruityPrimeNativeRepackEntityTests
        COMMAND FruityPrimeNativeRepackEntityTests)

    add_executable(FruityPrimeNativeRepackCollisionTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/repack_collision_tests.cpp")
    target_link_libraries(FruityPrimeNativeRepackCollisionTests PRIVATE
        fruity_prime_formats)
    add_test(NAME FruityPrimeNativeRepackCollisionTests
        COMMAND FruityPrimeNativeRepackCollisionTests)

    add_executable(FruityPrimeNativeRepackModelTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/repack_model_tests.cpp")
    target_link_libraries(FruityPrimeNativeRepackModelTests PRIVATE
        fruity_prime_formats)
    add_test(NAME FruityPrimeNativeRepackModelTests
        COMMAND FruityPrimeNativeRepackModelTests)

    add_executable(FruityPrimeNativeTestMiscTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/test_misc_tests.cpp"
        "${FRUITY_PRIME_CPP_ROOT}/Testing/TestMisc.cpp")
    target_link_libraries(FruityPrimeNativeTestMiscTests PRIVATE
        fruity_prime_scene)
    add_test(NAME FruityPrimeNativeTestMiscTests
        COMMAND FruityPrimeNativeTestMiscTests)

    add_executable(FruityPrimeNativeTestParseTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/test_parse_tests.cpp"
        "${FRUITY_PRIME_CPP_ROOT}/Testing/TestParse.cpp")
    target_link_libraries(FruityPrimeNativeTestParseTests PRIVATE
        fruity_prime_formats)
    add_test(NAME FruityPrimeNativeTestParseTests
        COMMAND FruityPrimeNativeTestParseTests)

    add_executable(FruityPrimeNativeTestEffectsTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/test_effects_tests.cpp"
        "${FRUITY_PRIME_CPP_ROOT}/Testing/TestEffects.cpp")
    target_link_libraries(FruityPrimeNativeTestEffectsTests PRIVATE
        fruity_prime_formats)
    add_test(NAME FruityPrimeNativeTestEffectsTests
        COMMAND FruityPrimeNativeTestEffectsTests)

    add_executable(FruityPrimeNativeTestPrintTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/test_print_tests.cpp"
        "${FRUITY_PRIME_CPP_ROOT}/Testing/TestPrint.cpp")
    target_link_libraries(FruityPrimeNativeTestPrintTests PRIVATE
        fruity_prime_formats)
    add_test(NAME FruityPrimeNativeTestPrintTests
        COMMAND FruityPrimeNativeTestPrintTests)

    add_executable(FruityPrimeNativeTestOverlayTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/test_overlay_tests.cpp"
        "${FRUITY_PRIME_CPP_ROOT}/Testing/TestOverlay.cpp")
    target_link_libraries(FruityPrimeNativeTestOverlayTests PRIVATE
        fruity_prime_formats)
    add_test(NAME FruityPrimeNativeTestOverlayTests
        COMMAND FruityPrimeNativeTestOverlayTests)

    add_executable(FruityPrimeNativeTestLogicTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/test_logic_tests.cpp"
        "${FRUITY_PRIME_CPP_ROOT}/Testing/TestLogic.cpp")
    target_link_libraries(FruityPrimeNativeTestLogicTests PRIVATE
        fruity_prime_formats)
    add_test(NAME FruityPrimeNativeTestLogicTests
        COMMAND FruityPrimeNativeTestLogicTests)

    add_executable(FruityPrimeNativeTestPlayerTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/test_player_tests.cpp"
        "${FRUITY_PRIME_CPP_ROOT}/Testing/TestPlayer.cpp")
    target_link_libraries(FruityPrimeNativeTestPlayerTests PRIVATE
        fruity_prime_formats)
    add_test(NAME FruityPrimeNativeTestPlayerTests
        COMMAND FruityPrimeNativeTestPlayerTests)

    add_executable(FruityPrimeNativeAnalyzerTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/analyzer_tests.cpp")
    target_link_libraries(FruityPrimeNativeAnalyzerTests PRIVATE
        fruity_prime_core)
    add_test(NAME FruityPrimeNativeAnalyzerTests
        COMMAND FruityPrimeNativeAnalyzerTests)

    add_executable(FruityPrimeNativeTestWeaponsTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/test_weapons_tests.cpp"
        "${FRUITY_PRIME_CPP_ROOT}/Testing/TestWeapons.cpp")
    target_link_libraries(FruityPrimeNativeTestWeaponsTests PRIVATE
        fruity_prime_formats)
    add_test(NAME FruityPrimeNativeTestWeaponsTests
        COMMAND FruityPrimeNativeTestWeaponsTests)

    add_executable(FruityPrimeNativeMovieTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/movie_tests.cpp")
    target_link_libraries(FruityPrimeNativeMovieTests PRIVATE fruity_prime_export)
    add_test(NAME FruityPrimeNativeMovieTests COMMAND FruityPrimeNativeMovieTests)
    if(FRUITY_PRIME_TEST_NDS)
        set_tests_properties(FruityPrimeNativeMovieTests PROPERTIES
            ENVIRONMENT "FRUITY_PRIME_TEST_NDS=${FRUITY_PRIME_TEST_NDS}")
    endif()

    add_executable(FruityPrimeNativeEffectsTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/effects_tests.cpp")
    target_link_libraries(FruityPrimeNativeEffectsTests PRIVATE
        fruity_prime_formats)
    add_test(NAME FruityPrimeNativeEffectsTests
        COMMAND FruityPrimeNativeEffectsTests)

    add_executable(FruityPrimeNativeEffectRuntimeTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/effect_runtime_tests.cpp")
    target_link_libraries(FruityPrimeNativeEffectRuntimeTests PRIVATE
        fruity_prime_formats)
    add_test(NAME FruityPrimeNativeEffectRuntimeTests
        COMMAND FruityPrimeNativeEffectRuntimeTests)

    add_executable(FruityPrimeNativeNodeDataTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/node_data_tests.cpp")
    target_link_libraries(FruityPrimeNativeNodeDataTests PRIVATE fruity_prime_formats)
    add_test(NAME FruityPrimeNativeNodeDataTests COMMAND FruityPrimeNativeNodeDataTests)
    if(FRUITY_PRIME_TEST_NDS)
        set_tests_properties(FruityPrimeNativeNodeDataTests PROPERTIES
            ENVIRONMENT "FRUITY_PRIME_TEST_NDS=${FRUITY_PRIME_TEST_NDS}")
    endif()

    add_executable(FruityPrimeNativeModelTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/model_tests.cpp")
    target_link_libraries(FruityPrimeNativeModelTests PRIVATE fruity_prime_formats)
    add_test(NAME FruityPrimeNativeModelTests COMMAND FruityPrimeNativeModelTests)

    add_executable(FruityPrimeNativeModelCatalogTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/model_catalog_tests.cpp")
    target_link_libraries(FruityPrimeNativeModelCatalogTests PRIVATE
        fruity_prime_formats)
    add_test(NAME FruityPrimeNativeModelCatalogTests
        COMMAND FruityPrimeNativeModelCatalogTests)

    add_executable(FruityPrimeNativeCollisionTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/collision_tests.cpp")
    target_link_libraries(FruityPrimeNativeCollisionTests PRIVATE fruity_prime_formats)
    add_test(NAME FruityPrimeNativeCollisionTests COMMAND FruityPrimeNativeCollisionTests)

    add_executable(FruityPrimeNativeEntityTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/entity_tests.cpp")
    target_link_libraries(FruityPrimeNativeEntityTests PRIVATE fruity_prime_formats)
    add_test(NAME FruityPrimeNativeEntityTests COMMAND FruityPrimeNativeEntityTests)

    add_executable(FruityPrimeNativeRuntimeEntityTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/runtime_entity_tests.cpp")
    target_link_libraries(FruityPrimeNativeRuntimeEntityTests PRIVATE
        fruity_prime_gameplay)
    add_test(NAME FruityPrimeNativeRuntimeEntityTests
        COMMAND FruityPrimeNativeRuntimeEntityTests)

    add_executable(FruityPrimeNativeStaticEntityTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/static_entity_tests.cpp")
    target_link_libraries(FruityPrimeNativeStaticEntityTests PRIVATE
        fruity_prime_core)
    add_test(NAME FruityPrimeNativeStaticEntityTests
        COMMAND FruityPrimeNativeStaticEntityTests)

    add_executable(FruityPrimeNativeSceneTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/scene_tests.cpp")
    target_link_libraries(FruityPrimeNativeSceneTests PRIVATE
        fruity_prime_scene fruity_prime_gameplay fruity_prime_core)
    add_test(NAME FruityPrimeNativeSceneTests COMMAND FruityPrimeNativeSceneTests)

    add_executable(FruityPrimeNativeGameplayTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/gameplay_tests.cpp")
    target_link_libraries(FruityPrimeNativeGameplayTests PRIVATE
        fruity_prime_gameplay fruity_prime_core)
    add_test(NAME FruityPrimeNativeGameplayTests COMMAND FruityPrimeNativeGameplayTests)

    add_executable(FruityPrimeNativeMatchFlowTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/match_flow_tests.cpp")
    target_link_libraries(FruityPrimeNativeMatchFlowTests PRIVATE fruity_prime_gameplay)
    add_test(NAME FruityPrimeNativeMatchFlowTests COMMAND FruityPrimeNativeMatchFlowTests)

    add_executable(FruityPrimeNativeRoomCatalogTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/room_catalog_tests.cpp"
        "${FRUITY_PRIME_CPP_ROOT}/Mods/ThumbnailGenerator.cpp")
    target_link_libraries(FruityPrimeNativeRoomCatalogTests PRIVATE fruity_prime_scene)
    add_test(NAME FruityPrimeNativeRoomCatalogTests COMMAND FruityPrimeNativeRoomCatalogTests)

    add_executable(FruityPrimeNativeRealRomTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/real_rom_tests.cpp")
    target_link_libraries(FruityPrimeNativeRealRomTests PRIVATE fruity_prime_formats)
    add_test(NAME FruityPrimeNativeRealRomTests COMMAND FruityPrimeNativeRealRomTests)
    add_executable(FruityPrimeNativeRealRoomTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/real_room_tests.cpp")
    target_link_libraries(FruityPrimeNativeRealRoomTests PRIVATE
        fruity_prime_scene fruity_prime_core)
    add_test(NAME FruityPrimeNativeRealRoomTests COMMAND FruityPrimeNativeRealRoomTests)
    if(FRUITY_PRIME_TEST_NDS)
        set_tests_properties(FruityPrimeNativeSceneTests FruityPrimeNativeGameplayTests
            FruityPrimeNativeRealRomTests FruityPrimeNativeRealRoomTests PROPERTIES
            ENVIRONMENT "FRUITY_PRIME_TEST_NDS=${FRUITY_PRIME_TEST_NDS}")
        set_tests_properties(FruityPrimeNativeExportTests PROPERTIES
            ENVIRONMENT "FRUITY_PRIME_TEST_NDS=${FRUITY_PRIME_TEST_NDS}")
    endif()

    add_executable(FruityPrimeNativeNetworkTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/network_tests.cpp")
    target_link_libraries(FruityPrimeNativeNetworkTests PRIVATE
        fruity_prime_net fruity_prime_core)
    add_test(NAME FruityPrimeNativeNetworkTests COMMAND FruityPrimeNativeNetworkTests)

    add_executable(FruityPrimeNativeNetworkConditionsTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/network_conditions_tests.cpp")
    target_link_libraries(FruityPrimeNativeNetworkConditionsTests PRIVATE fruity_prime_net)
    add_test(NAME FruityPrimeNativeNetworkConditionsTests
        COMMAND FruityPrimeNativeNetworkConditionsTests)

    add_executable(FruityPrimeNativeNetFeatureCheckTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/net_feature_check_tests.cpp")
    target_link_libraries(FruityPrimeNativeNetFeatureCheckTests PRIVATE
        fruity_prime_net)
    add_test(NAME FruityPrimeNativeNetFeatureCheckTests
        COMMAND FruityPrimeNativeNetFeatureCheckTests)

    add_executable(FruityPrimeNativeWeaponDpsTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/weapon_dps_tests.cpp")
    target_link_libraries(FruityPrimeNativeWeaponDpsTests PRIVATE
        fruity_prime_gameplay fruity_prime_core)
    add_test(NAME FruityPrimeNativeWeaponDpsTests
        COMMAND FruityPrimeNativeWeaponDpsTests)
    if(FRUITY_PRIME_TEST_NDS)
        set_tests_properties(FruityPrimeNativeWeaponDpsTests PROPERTIES
            ENVIRONMENT "FRUITY_PRIME_TEST_NDS=${FRUITY_PRIME_TEST_NDS}")
    endif()

    add_executable(FruityPrimeNativeChatTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/chat_tests.cpp")
    target_link_libraries(FruityPrimeNativeChatTests PRIVATE fruity_prime_chat)
    add_test(NAME FruityPrimeNativeChatTests COMMAND FruityPrimeNativeChatTests)

    add_executable(FruityPrimeNativeDemoTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/demo_tests.cpp")
    target_link_libraries(FruityPrimeNativeDemoTests PRIVATE fruity_prime_demo)
    add_test(NAME FruityPrimeNativeDemoTests COMMAND FruityPrimeNativeDemoTests)

    add_executable(FruityPrimeNativeHudTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/hud_tests.cpp"
        "${FRUITY_PRIME_CPP_ROOT}/HUD/HudInfo.cpp"
        "${FRUITY_PRIME_CPP_ROOT}/Mods/Render/PlayerEntityProHud.cpp")
    target_include_directories(FruityPrimeNativeHudTests PRIVATE
        "${FRUITY_PRIME_CPP_ROOT}")
    target_link_libraries(FruityPrimeNativeHudTests PRIVATE
        fruity_prime_formats fruity_prime_gameplay fruity_prime_metadata)
    add_test(NAME FruityPrimeNativeHudTests COMMAND FruityPrimeNativeHudTests)
    if(FRUITY_PRIME_TEST_NDS)
        set_tests_properties(FruityPrimeNativeHudTests PROPERTIES
        ENVIRONMENT "FRUITY_PRIME_TEST_NDS=${FRUITY_PRIME_TEST_NDS}")
    endif()

    add_executable(FruityPrimeNativeStringsTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/strings_tests.cpp")
    target_link_libraries(FruityPrimeNativeStringsTests PRIVATE
        fruity_prime_core fruity_prime_formats)
    add_test(NAME FruityPrimeNativeStringsTests
        COMMAND FruityPrimeNativeStringsTests)
    if(FRUITY_PRIME_TEST_NDS)
        set_tests_properties(FruityPrimeNativeStringsTests PROPERTIES
            ENVIRONMENT "FRUITY_PRIME_TEST_NDS=${FRUITY_PRIME_TEST_NDS}")
    endif()

    add_executable(FruityPrimeNativeSoundTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/sound_tests.cpp")
    target_link_libraries(FruityPrimeNativeSoundTests PRIVATE
        fruity_prime_sound fruity_prime_formats)
    add_test(NAME FruityPrimeNativeSoundTests COMMAND FruityPrimeNativeSoundTests)
    if(FRUITY_PRIME_TEST_NDS)
        set_tests_properties(FruityPrimeNativeSoundTests PROPERTIES
            ENVIRONMENT "FRUITY_PRIME_TEST_NDS=${FRUITY_PRIME_TEST_NDS}")
    endif()

    add_executable(FruityPrimeNativeSfxMixerTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/sfx_mixer_tests.cpp")
    target_link_libraries(FruityPrimeNativeSfxMixerTests PRIVATE
        fruity_prime_sound fruity_prime_core)
    add_test(NAME FruityPrimeNativeSfxMixerTests
        COMMAND FruityPrimeNativeSfxMixerTests)

    add_executable(FruityPrimeNativeSfxRuntimeTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/sfx_runtime_tests.cpp")
    target_link_libraries(FruityPrimeNativeSfxRuntimeTests PRIVATE
        fruity_prime_sound)
    add_test(NAME FruityPrimeNativeSfxRuntimeTests
        COMMAND FruityPrimeNativeSfxRuntimeTests)
    if(FRUITY_PRIME_TEST_NDS)
        set_tests_properties(FruityPrimeNativeSfxRuntimeTests PROPERTIES
            ENVIRONMENT "FRUITY_PRIME_TEST_NDS=${FRUITY_PRIME_TEST_NDS}")
    endif()

    add_executable(FruityPrimeNativeSoundMetadataTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/sound_metadata_tests.cpp")
    target_link_libraries(FruityPrimeNativeSoundMetadataTests PRIVATE
        fruity_prime_sound)
    add_test(NAME FruityPrimeNativeSoundMetadataTests
        COMMAND FruityPrimeNativeSoundMetadataTests)
    if(FRUITY_PRIME_TEST_NDS)
        set_tests_properties(FruityPrimeNativeSoundMetadataTests PROPERTIES
            ENVIRONMENT "FRUITY_PRIME_TEST_NDS=${FRUITY_PRIME_TEST_NDS}")
    endif()

    add_executable(FruityPrimeNativeMapGenTests
        "${FRUITY_PRIME_CPP_ROOT}/Testing/mapgen_tests.cpp")
    target_link_libraries(FruityPrimeNativeMapGenTests PRIVATE
        fruity_prime_mapgen fruity_prime_map_report fruity_prime_formats)
    add_test(NAME FruityPrimeNativeMapGenTests
        COMMAND FruityPrimeNativeMapGenTests)

    add_executable(FruityPrimeNativeSourceTreeContract
        "${FRUITY_PRIME_CPP_ROOT}/Testing/source_tree_contract_tests.cpp")
    target_include_directories(FruityPrimeNativeSourceTreeContract PRIVATE
        "${FRUITY_PRIME_CPP_ROOT}")
    target_link_libraries(FruityPrimeNativeSourceTreeContract PRIVATE
        fruity_prime_core fruity_prime_metadata fruity_prime_sound)
    add_test(NAME FruityPrimeNativeSourceTreeContract
        COMMAND FruityPrimeNativeSourceTreeContract)

    # CTest must exercise the assertions in the native contract tests even
    # when the production build type is Release. Without this, a malformed
    # packet, ROM record, or deterministic simulation result can appear green
    # simply because assert() was compiled out.
    set(native_test_targets
        FruityPrimeNativeAiPersonalityTests
        FruityPrimeNativeCameraSequenceTests
        FruityPrimeNativeCameraSequenceEntityTests
        FruityPrimeNativeRenderModsTests
        FruityPrimeNativeRenderOptionsTests
        FruityPrimeNativeRendererTests
        FruityPrimeNativeCoreTests
        FruityPrimeNativeProtocolTests
        FruityPrimeNativeMetadataTests
        FruityPrimeNativeExportTests
        FruityPrimeNativeEnemyCatalogTests
        FruityPrimeNativePlayerProfileTests
        FruityPrimeNativePlayerAiTests
        FruityPrimeNativePlayerComponentsTests
        FruityPrimeNativeInputTests
        FruityPrimeNativeSettingsTests
        FruityPrimeNativeGamepadTests
        FruityPrimeNativeModsPortableTests
        FruityPrimeNativeUtilityTests
        FruityPrimeNativeLauncherTests
        FruityPrimeNativeLauncherGuiTests
        FruityPrimeNativeLauncherPortableTests
        FruityPrimeNativeMatchStartTests
        FruityPrimeNativeTextLauncherTests
        FruityPrimeNativeAdventureSaveTests
        FruityPrimeNativeFormatTests
        FruityPrimeNativeEntityClassTests
        FruityPrimeNativeRepackEntityTests
        FruityPrimeNativeRepackCollisionTests
        FruityPrimeNativeRepackModelTests
        FruityPrimeNativeTestMiscTests
        FruityPrimeNativeTestParseTests
        FruityPrimeNativeTestEffectsTests
        FruityPrimeNativeTestPrintTests
        FruityPrimeNativeTestOverlayTests
        FruityPrimeNativeTestLogicTests
        FruityPrimeNativeTestPlayerTests
        FruityPrimeNativeAnalyzerTests
        FruityPrimeNativeTestWeaponsTests
        FruityPrimeNativeMovieTests
        FruityPrimeNativeEffectsTests
        FruityPrimeNativeNodeDataTests
        FruityPrimeNativeModelTests
        FruityPrimeNativeModelCatalogTests
        FruityPrimeNativeEffectRuntimeTests
        FruityPrimeNativeCollisionTests
        FruityPrimeNativeEntityTests
        FruityPrimeNativeRuntimeEntityTests
        FruityPrimeNativeStaticEntityTests
        FruityPrimeNativeSceneTests
        FruityPrimeNativeGameplayTests
        FruityPrimeNativeMatchFlowTests
        FruityPrimeNativeRoomCatalogTests
        FruityPrimeNativeRealRomTests
        FruityPrimeNativeRealRoomTests
        FruityPrimeNativeNetworkTests
        FruityPrimeNativeNetworkConditionsTests
        FruityPrimeNativeWeaponDpsTests
        FruityPrimeNativeChatTests
        FruityPrimeNativeDemoTests
        FruityPrimeNativeHudTests
        FruityPrimeNativeStringsTests
        FruityPrimeNativeSoundTests
        FruityPrimeNativeSfxMixerTests
        FruityPrimeNativeSfxRuntimeTests
        FruityPrimeNativeSoundMetadataTests
        FruityPrimeNativeMapGenTests
        FruityPrimeNativeSourceTreeContract
    )
    if(MSVC)
        foreach(native_test IN LISTS native_test_targets)
            target_compile_options(${native_test} PRIVATE /U NDEBUG)
        endforeach()
    else()
        foreach(native_test IN LISTS native_test_targets)
            target_compile_options(${native_test} PRIVATE -UNDEBUG)
        endforeach()
    endif()
endif()
