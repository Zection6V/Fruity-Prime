cmake_minimum_required(VERSION 3.20)

set(FRUITY_PRIME_NATIVE_BACKEND
    "${CMAKE_CURRENT_LIST_DIR}/../../native")

if(NOT EXISTS "${FRUITY_PRIME_NATIVE_BACKEND}/CMakeLists.txt")
    message(FATAL_ERROR
        "The native C++ backend was not found: ${FRUITY_PRIME_NATIVE_BACKEND}")
endif()

include(CTest)

# Keep the currently verified implementation in one place while exposing the
# C++ source tree at the same level as the managed MphRead project.  Category
# targets let modules move here without changing consumers or creating two
# copies of the implementation.
add_subdirectory(
    "${FRUITY_PRIME_NATIVE_BACKEND}"
    "${CMAKE_BINARY_DIR}/fruity-prime-native-backend")

add_library(FruityPrimeCppAssets INTERFACE)
target_link_libraries(FruityPrimeCppAssets INTERFACE fruity_prime_formats)

add_library(FruityPrimeCppCore INTERFACE)
target_link_libraries(FruityPrimeCppCore INTERFACE fruity_prime_core)

add_library(FruityPrimeCppEntities INTERFACE)
target_link_libraries(FruityPrimeCppEntities INTERFACE fruity_prime_gameplay)

add_library(FruityPrimeCppExport INTERFACE)
target_link_libraries(FruityPrimeCppExport INTERFACE fruity_prime_export)

add_library(FruityPrimeCppFormats INTERFACE)
target_link_libraries(FruityPrimeCppFormats INTERFACE fruity_prime_formats)

add_library(FruityPrimeCppHud INTERFACE)
add_library(FruityPrimeCppHudImpl STATIC HUD/HudInfo.cpp)
target_include_directories(FruityPrimeCppHudImpl PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}")
target_link_libraries(FruityPrimeCppHudImpl PUBLIC
    fruity_prime_gameplay fruity_prime_metadata)
target_link_libraries(FruityPrimeCppHud INTERFACE FruityPrimeCppHudImpl)
if(MSVC)
    target_compile_options(FruityPrimeCppHudImpl PRIVATE /W4 /permissive-)
else()
    target_compile_options(FruityPrimeCppHudImpl PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(FruityPrimeCppMetadata INTERFACE)
target_link_libraries(FruityPrimeCppMetadata INTERFACE fruity_prime_metadata)

add_library(FruityPrimeCppMods INTERFACE)
target_link_libraries(FruityPrimeCppMods INTERFACE
    fruity_prime_chat
    fruity_prime_demo
    fruity_prime_gamepad
    fruity_prime_input
    fruity_prime_launcher
    fruity_prime_mapgen
    fruity_prime_net
    fruity_prime_settings)

add_library(FruityPrimeCppSound INTERFACE)
add_library(FruityPrimeCppSoundImpl STATIC
    Sound/sound.cpp Sound/sdat.cpp Sound/sound_resources.cpp
    Formats/Sound.cpp
    Sound/music.cpp Sound/music_runtime.cpp
    Sound/sseq_player.cpp
    Sound/software_mixer.cpp
    Sound/win32_audio_output.cpp Sound/Sfx.cpp
    Metadata/SoundMeta.cpp
    Mods/Sound/SfxMixer.cpp)
target_include_directories(FruityPrimeCppSoundImpl PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}")
target_link_libraries(FruityPrimeCppSoundImpl PUBLIC fruity_prime_formats)
if(WIN32)
    target_link_libraries(FruityPrimeCppSoundImpl PUBLIC winmm)
endif()
target_link_libraries(FruityPrimeCppSound INTERFACE FruityPrimeCppSoundImpl)
if(MSVC)
    target_compile_options(FruityPrimeCppSoundImpl PRIVATE /W4 /permissive-)
else()
    target_compile_options(FruityPrimeCppSoundImpl PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(FruityPrimeCppTesting INTERFACE)

add_library(FruityPrimeCppUtility INTERFACE)
target_link_libraries(FruityPrimeCppUtility INTERFACE
    FruityPrimeCppCore
    fruity_prime_formats
    fruity_prime_scene)

add_library(FruityPrimeCpp INTERFACE)
target_include_directories(FruityPrimeCpp INTERFACE
    "${CMAKE_CURRENT_LIST_DIR}")
target_link_libraries(FruityPrimeCpp INTERFACE
    FruityPrimeCppCore
    FruityPrimeCppAssets
    FruityPrimeCppEntities
    FruityPrimeCppExport
    FruityPrimeCppFormats
    FruityPrimeCppHud
    FruityPrimeCppMetadata
    FruityPrimeCppMods
    FruityPrimeCppSound
    FruityPrimeCppTesting
    FruityPrimeCppUtility)

set(FRUITY_PRIME_CPP_RUNTIME_TARGETS FruityPrimeServer)
if(WIN32)
    list(APPEND FRUITY_PRIME_CPP_RUNTIME_TARGETS FruityPrime)
endif()
add_custom_target(FruityPrimeNativeSourceTree ALL
    DEPENDS ${FRUITY_PRIME_CPP_RUNTIME_TARGETS})

if(BUILD_TESTING AND NOT TARGET FruityPrimeNativeSourceTreeContract)
    add_executable(FruityPrimeNativeSourceTreeContract
        Testing/source_tree_contract_tests.cpp)
    target_link_libraries(FruityPrimeNativeSourceTreeContract PRIVATE
        FruityPrimeCpp)
    if(MSVC)
        target_compile_options(FruityPrimeNativeSourceTreeContract PRIVATE /W4 /permissive- /U NDEBUG)
    else()
        target_compile_options(FruityPrimeNativeSourceTreeContract PRIVATE
            -Wall -Wextra -Wpedantic -UNDEBUG)
    endif()
    if(MINGW)
        target_link_options(FruityPrimeNativeSourceTreeContract PRIVATE
            -static-libgcc -static-libstdc++)
    endif()
    add_test(NAME FruityPrimeNativeSourceTreeContract
        COMMAND FruityPrimeNativeSourceTreeContract)
endif()
