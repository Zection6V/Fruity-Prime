cmake_minimum_required(VERSION 3.25)

project(FruityPrimeNative LANGUAGES CXX)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/FruityNativeConfig.cmake")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)

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

fruity_report_owner("NcsfPlay.Native" NCSFPLAY_NATIVE_SOURCES NCSFPLAY_NATIVE_HEADERS)
fruity_report_owner("MphRead.Native" MPHREAD_NATIVE_SOURCES MPHREAD_NATIVE_HEADERS)

add_library(fruity_ncsfplay_native STATIC)
add_library(FruityPrime::NcsfPlay.Native ALIAS fruity_ncsfplay_native)
target_sources(fruity_ncsfplay_native PRIVATE ${NCSFPLAY_NATIVE_SOURCES} ${NCSFPLAY_NATIVE_HEADERS})
target_include_directories(fruity_ncsfplay_native PUBLIC "${FRUITY_NCSFPLAY_ROOT}")
target_compile_features(fruity_ncsfplay_native PUBLIC cxx_std_20)
target_link_libraries(fruity_ncsfplay_native PUBLIC Threads::Threads ZLIB::ZLIB)
set_target_properties(fruity_ncsfplay_native PROPERTIES OUTPUT_NAME "NcsfPlay.Native")

add_library(fruity_mphread_native STATIC)
add_library(FruityPrime::MphRead.Native ALIAS fruity_mphread_native)
target_sources(fruity_mphread_native PRIVATE ${MPHREAD_NATIVE_SOURCES} ${MPHREAD_NATIVE_HEADERS})
target_include_directories(fruity_mphread_native PUBLIC "${FRUITY_MPHREAD_ROOT}")
target_compile_features(fruity_mphread_native PUBLIC cxx_std_20)
target_link_libraries(fruity_mphread_native PUBLIC fruity_ncsfplay_native Threads::Threads)
set_target_properties(fruity_mphread_native PROPERTIES OUTPUT_NAME "MphRead.Native")

if(WIN32)
    target_compile_definitions(fruity_mphread_native PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(fruity_mphread_native PUBLIC ws2_32 bcrypt)
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
