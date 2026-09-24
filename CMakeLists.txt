cmake_minimum_required(VERSION 3.25)

project(FruityPrimeNative LANGUAGES CXX)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/FruityNativeConfig.cmake")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)

# std::stop_token, std::stop_source and std::jthread stand in for the C#
# CancellationToken and the decoder/network worker threads. libc++ before 20
# (the NDK's and Apple's) ships them only behind -fexperimental-library; GCC's
# libstdc++ and MSVC's STL have them outright. Ask for the flag only where the
# plain compile fails, and only if it then succeeds.
include(CheckCXXSourceCompiles)
set(_fruity_stop_token_probe [=[
#include <stop_token>
#include <thread>
int main()
{
    std::stop_source source;
    std::jthread worker([](std::stop_token token) { (void)token.stop_requested(); });
    return source.stop_requested() ? 1 : 0;
}
]=])
set(CMAKE_REQUIRED_FLAGS "${CMAKE_CXX20_STANDARD_COMPILE_OPTION}")
check_cxx_source_compiles("${_fruity_stop_token_probe}" FRUITY_STOP_TOKEN_BUILTIN)
set(FRUITY_STOP_TOKEN_OPTIONS "")
if(NOT FRUITY_STOP_TOKEN_BUILTIN)
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_CXX20_STANDARD_COMPILE_OPTION} -fexperimental-library")
    set(CMAKE_REQUIRED_LINK_OPTIONS "-fexperimental-library")
    check_cxx_source_compiles("${_fruity_stop_token_probe}" FRUITY_STOP_TOKEN_EXPERIMENTAL)
    unset(CMAKE_REQUIRED_LINK_OPTIONS)
    if(FRUITY_STOP_TOKEN_EXPERIMENTAL)
        set(FRUITY_STOP_TOKEN_OPTIONS "-fexperimental-library")
    else()
        message(FATAL_ERROR
            "This standard library has no std::stop_token/std::jthread, even with "
            "-fexperimental-library.")
    endif()
endif()
unset(CMAKE_REQUIRED_FLAGS)

function(fruity_collect_owner owner_root out_sources out_headers)
    file(GLOB_RECURSE _sources CONFIGURE_DEPENDS "${owner_root}/*.cpp")
    file(GLOB_RECURSE _headers CONFIGURE_DEPENDS "${owner_root}/*.hpp" "${owner_root}/*.h")
    list(SORT _sources)
    list(SORT _headers)

    if(NOT _sources)
        message(FATAL_ERROR "No C++ sources were found under ${owner_root}")
    endif()

    set(${out_sources} "${_sources}" PARENT_SCOPE)
    set(${out_headers} "${_headers}" PARENT_SCOPE)
endfunction()

function(fruity_report_owner label sources_var headers_var)
    list(LENGTH ${sources_var} _source_count)
    list(LENGTH ${headers_var} _header_count)
    message(STATUS "${label}: ${_source_count} C++ sources, ${_header_count} headers")
endfunction()

set(FRUITY_NCSFPLAY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src/NcsfPlay.Native")
set(FRUITY_MPHREAD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src/MphRead.Native")
set(FRUITY_ANDROID_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src/MphRead.Native.Android")

fruity_collect_owner("${FRUITY_NCSFPLAY_ROOT}" NCSFPLAY_NATIVE_SOURCES NCSFPLAY_NATIVE_HEADERS)
fruity_collect_owner("${FRUITY_MPHREAD_ROOT}" MPHREAD_NATIVE_SOURCES MPHREAD_NATIVE_HEADERS)

# Android has its own activity, views, input bridge and EGL/GLES renderer under
# MphRead.Native.Android. The desktop Avalonia/GLFW/Freetype owners must not be
# pulled into the Android common archive merely because the source list is a
# recursive glob.
if(ANDROID)
    list(FILTER MPHREAD_NATIVE_SOURCES EXCLUDE REGEX "/NativeRuntime/Avalonia/.*\\.cpp$")
    list(FILTER MPHREAD_NATIVE_SOURCES EXCLUDE REGEX "/NativeRuntime/Gui/.*\\.cpp$")
    list(FILTER MPHREAD_NATIVE_SOURCES EXCLUDE REGEX "/NativeRuntime/OpenTK/RendererPlatform\\.cpp$")
    list(FILTER MPHREAD_NATIVE_SOURCES EXCLUDE REGEX "/NativeRuntime/OpenTK/GL\\.cpp$")
    list(FILTER MPHREAD_NATIVE_SOURCES EXCLUDE REGEX "/NativeRuntime/SoundFlow/MiniAudio\\.cpp$")
endif()

fruity_report_owner("NcsfPlay.Native" NCSFPLAY_NATIVE_SOURCES NCSFPLAY_NATIVE_HEADERS)
fruity_report_owner("MphRead.Native" MPHREAD_NATIVE_SOURCES MPHREAD_NATIVE_HEADERS)

add_library(fruity_ncsfplay_native STATIC)
add_library(FruityPrime::NcsfPlay.Native ALIAS fruity_ncsfplay_native)
target_sources(fruity_ncsfplay_native PRIVATE ${NCSFPLAY_NATIVE_SOURCES} ${NCSFPLAY_NATIVE_HEADERS})
target_include_directories(fruity_ncsfplay_native PUBLIC "${FRUITY_NCSFPLAY_ROOT}")
target_compile_features(fruity_ncsfplay_native PUBLIC cxx_std_20)
if(FRUITY_STOP_TOKEN_OPTIONS)
    target_compile_options(fruity_ncsfplay_native PUBLIC ${FRUITY_STOP_TOKEN_OPTIONS})
    target_link_options(fruity_ncsfplay_native PUBLIC ${FRUITY_STOP_TOKEN_OPTIONS})
endif()
target_link_libraries(fruity_ncsfplay_native PUBLIC Threads::Threads ZLIB::ZLIB)
set_target_properties(fruity_ncsfplay_native PROPERTIES OUTPUT_NAME "NcsfPlay.Native")

# TagList's invariant collation: Windows and Apple ask the system, Android asks
# the JVM, and everywhere else it is ICU's -- as it is for .NET on Linux.
if(NOT WIN32 AND NOT APPLE AND NOT ANDROID)
    find_package(ICU REQUIRED COMPONENTS uc i18n)
    target_link_libraries(fruity_ncsfplay_native PUBLIC ICU::uc ICU::i18n)
endif()

add_library(fruity_mphread_native STATIC)
add_library(FruityPrime::MphRead.Native ALIAS fruity_mphread_native)
target_sources(fruity_mphread_native PRIVATE ${MPHREAD_NATIVE_SOURCES} ${MPHREAD_NATIVE_HEADERS})
target_include_directories(fruity_mphread_native PUBLIC "${FRUITY_MPHREAD_ROOT}")
target_compile_features(fruity_mphread_native PUBLIC cxx_std_20)
if(FRUITY_STOP_TOKEN_OPTIONS)
    target_compile_options(fruity_mphread_native PUBLIC ${FRUITY_STOP_TOKEN_OPTIONS})
    target_link_options(fruity_mphread_native PUBLIC ${FRUITY_STOP_TOKEN_OPTIONS})
endif()
target_link_libraries(fruity_mphread_native PUBLIC fruity_ncsfplay_native Threads::Threads ZLIB::ZLIB)
# The C# JIT never fuses a * b + c into one fused multiply-add, so neither may
# the port: a fused result rounds once where the C# rounds twice, and every
# transform, collision test and inverse would drift in the last bit. GCC in
# ISO mode and MSVC already keep them apart; Clang (macOS, the Android NDK)
# contracts by default, and on arm64, where FMA is always there, it does.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(fruity_mphread_native PUBLIC -ffp-contract=off)
endif()
set_target_properties(fruity_mphread_native PROPERTIES OUTPUT_NAME "MphRead.Native")

# The windowed launcher. The name is upstream's, from the toolkit the C# build
# uses; here it selects NativeRuntime/Avalonia, which is the same screens on
# NativeRuntime/Gui's own toolkit.
if(NOT ANDROID)
    target_compile_definitions(fruity_mphread_native PUBLIC MPHREAD_AVALONIA)
endif()

if(WIN32)
    target_compile_definitions(fruity_mphread_native PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    # The launcher's file picker is the platform's own common dialog.
    target_link_libraries(fruity_mphread_native PUBLIC ws2_32 bcrypt comdlg32)
endif()

# The renderer's GL entry points and the sound mixer's OpenAL ones, which the
# C# build gets from OpenTK's own native dependencies.
if(NOT ANDROID)
    find_package(OpenGL REQUIRED)
    target_link_libraries(fruity_mphread_native PUBLIC OpenGL::GL)

    find_package(glfw3 CONFIG REQUIRED)
    target_link_libraries(fruity_mphread_native PUBLIC glfw)

    # The launcher's text, which Avalonia rasterises on the C# side.
    find_package(Freetype REQUIRED)
    target_link_libraries(fruity_mphread_native PUBLIC Freetype::Freetype)

    find_library(FRUITY_OPENAL_LIBRARY NAMES openal OpenAL OpenAL32)
    if(FRUITY_OPENAL_LIBRARY)
        target_link_libraries(fruity_mphread_native PUBLIC "${FRUITY_OPENAL_LIBRARY}")
    else()
        message(FATAL_ERROR
            "OpenAL was not found. The sound mixer calls it directly, as OpenTK does "
            "in the C# build.")
    endif()
endif()

# The game binary. Its only source is the entry-point shim; everything else is
# the library above.
if(NOT ANDROID)
    add_executable(fruity_prime "${CMAKE_CURRENT_SOURCE_DIR}/src/FruityPrime/Main.cpp")
    target_link_libraries(fruity_prime PRIVATE fruity_mphread_native)
    set_target_properties(fruity_prime PROPERTIES OUTPUT_NAME "FruityPrime")

    # The launcher's window icon and front-screen logo. The C# build embeds
    # them as Avalonia resources; here they are the files beside the binary
    # that an avares: URI resolves to.
    add_custom_command(TARGET fruity_prime POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${FRUITY_MPHREAD_ROOT}/Assets/fruity-prime-logo.png"
            "${FRUITY_MPHREAD_ROOT}/Assets/fruity-prime-mark.png"
            "$<TARGET_FILE_DIR:fruity_prime>"
        VERBATIM)

    if(WIN32)
        # The executable's own icon, the csproj's ApplicationIcon.
        enable_language(RC)
        set(FRUITY_APP_ICON "${FRUITY_MPHREAD_ROOT}/Assets/fruity-prime.ico")
        file(TO_CMAKE_PATH "${FRUITY_APP_ICON}" FRUITY_APP_ICON)
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/FruityPrime.rc"
            "1 ICON \"${FRUITY_APP_ICON}\"\n")
        target_sources(fruity_prime PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/FruityPrime.rc")
    endif()

    if(MINGW)
        # The compiler's own runtime goes into the binary. Nobody running the
        # game has the toolchain installed, and these three DLLs are the ones
        # they would otherwise be missing.
        target_link_options(fruity_prime PRIVATE
            -static-libgcc -static-libstdc++
            -Wl,-Bstatic,--whole-archive -lwinpthread
            -Wl,--no-whole-archive,-Bdynamic)

        # Everything else the binary loads -- GLFW, OpenAL, zlib, curl,
        # libarchive and whatever those need in turn -- is copied next to it,
        # so the build output runs as it stands.
        find_program(FRUITY_OBJDUMP NAMES objdump)
        get_filename_component(FRUITY_TOOLCHAIN_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
        if(FRUITY_OBJDUMP)
            add_custom_command(TARGET fruity_prime POST_BUILD
                COMMAND "${CMAKE_COMMAND}"
                    "-DBINARY=$<TARGET_FILE:fruity_prime>"
                    "-DSEARCH_DIR=${FRUITY_TOOLCHAIN_BIN}"
                    "-DOBJDUMP=${FRUITY_OBJDUMP}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/build/copy-runtime-dlls.cmake"
                VERBATIM)
        endif()
    endif()
endif()

if(CMAKE_DL_LIBS)
    target_link_libraries(fruity_mphread_native PUBLIC "${CMAKE_DL_LIBS}")
endif()

if(ANDROID)
    set(FRUITY_NATIVE_DEPENDENCY_INCLUDE_DIR "" CACHE PATH
        "Header root containing curl/curl.h, archive.h, and archive_entry.h for the Android static compile target")

    foreach(_required_header IN ITEMS curl/curl.h archive.h archive_entry.h)
        if(NOT EXISTS "${FRUITY_NATIVE_DEPENDENCY_INCLUDE_DIR}/${_required_header}")
            message(FATAL_ERROR
                "Android native compilation requires ${_required_header} under "
                "FRUITY_NATIVE_DEPENDENCY_INCLUDE_DIR. The repository has no Android "
                "runtime owner for libcurl/libarchive, so CI supplies headers only and "
                "does not pretend those runtime libraries are packaged.")
        endif()
    endforeach()
    target_include_directories(fruity_mphread_native PRIVATE "${FRUITY_NATIVE_DEPENDENCY_INCLUDE_DIR}")

    list(FIND FRUITY_ANDROID_ABIS "${ANDROID_ABI}" _android_abi_index)
    if(_android_abi_index EQUAL -1)
        message(FATAL_ERROR
            "ANDROID_ABI '${ANDROID_ABI}' is not in FRUITY_ANDROID_ABIS: ${FRUITY_ANDROID_ABIS}")
    endif()

    if(ANDROID_PLATFORM MATCHES "^android-([0-9]+)$")
        if(CMAKE_MATCH_1 LESS FRUITY_ANDROID_MIN_API)
            message(FATAL_ERROR
                "ANDROID_PLATFORM ${ANDROID_PLATFORM} is below minimum API ${FRUITY_ANDROID_MIN_API}")
        endif()
    endif()

    find_library(FRUITY_ANDROID_LOG_LIBRARY log REQUIRED)
    find_library(FRUITY_ANDROID_PLATFORM_LIBRARY android REQUIRED)
    find_library(FRUITY_ANDROID_EGL_LIBRARY EGL REQUIRED)
    find_library(FRUITY_ANDROID_GLESV3_LIBRARY GLESv3 REQUIRED)

    target_link_libraries(fruity_mphread_native PUBLIC "${FRUITY_ANDROID_GLESV3_LIBRARY}")

    fruity_collect_owner("${FRUITY_ANDROID_ROOT}" MPHREAD_ANDROID_SOURCES MPHREAD_ANDROID_HEADERS)
    fruity_report_owner("MphRead.Native.Android" MPHREAD_ANDROID_SOURCES MPHREAD_ANDROID_HEADERS)

    add_library(fruity_mphread_native_android STATIC)
    add_library(FruityPrime::MphRead.Native.Android ALIAS fruity_mphread_native_android)
    target_sources(fruity_mphread_native_android PRIVATE ${MPHREAD_ANDROID_SOURCES} ${MPHREAD_ANDROID_HEADERS})
    target_include_directories(fruity_mphread_native_android PUBLIC
        "${FRUITY_ANDROID_ROOT}"
        "${FRUITY_MPHREAD_ROOT}"
        "${FRUITY_NCSFPLAY_ROOT}")
    target_compile_features(fruity_mphread_native_android PUBLIC cxx_std_20)
    target_link_libraries(fruity_mphread_native_android PUBLIC
        fruity_mphread_native
        fruity_ncsfplay_native
        "${FRUITY_ANDROID_PLATFORM_LIBRARY}"
        "${FRUITY_ANDROID_LOG_LIBRARY}"
        "${FRUITY_ANDROID_EGL_LIBRARY}"
        "${FRUITY_ANDROID_GLESV3_LIBRARY}")
    set_target_properties(fruity_mphread_native_android PROPERTIES OUTPUT_NAME "MphRead.Native.Android")
else()
    find_package(CURL REQUIRED)
    find_package(LibArchive REQUIRED)
    target_link_libraries(fruity_mphread_native PUBLIC CURL::libcurl LibArchive::LibArchive)
endif()
