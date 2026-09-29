if(NOT DEFINED SOURCE_ROOT
    OR NOT DEFINED GOLDEN_CAPTURE_CPP
    OR NOT DEFINED GOLDEN_CAPTURE_HEADER
    OR NOT DEFINED GOLDEN_CAPTURE_VALIDATION
    OR NOT DEFINED OUTPUT_FILE)
    message(FATAL_ERROR
        "GoldenCapture provenance generation requires source/root/input/output paths")
endif()

foreach(_source IN ITEMS
    "${GOLDEN_CAPTURE_CPP}"
    "${GOLDEN_CAPTURE_HEADER}"
    "${GOLDEN_CAPTURE_VALIDATION}")
    if(NOT EXISTS "${_source}")
        message(FATAL_ERROR
            "GoldenCapture provenance input is missing: ${_source}")
    endif()
endforeach()

file(SHA256 "${GOLDEN_CAPTURE_CPP}" GOLDEN_CAPTURE_CPP_SHA256)
file(SHA256 "${GOLDEN_CAPTURE_HEADER}" GOLDEN_CAPTURE_HEADER_SHA256)
file(SHA256 "${GOLDEN_CAPTURE_VALIDATION}" GOLDEN_CAPTURE_VALIDATION_SHA256)
string(SHA256 GOLDEN_CAPTURE_HARNESS_SHA256
    "${GOLDEN_CAPTURE_CPP_SHA256}:${GOLDEN_CAPTURE_HEADER_SHA256}:${GOLDEN_CAPTURE_VALIDATION_SHA256}")

set(SOURCE_COMMIT "unavailable")
set(GOLDEN_CAPTURE_CPP_GIT_BLOB "unavailable")
set(GOLDEN_CAPTURE_GIT_STATE "unavailable")

find_program(GOLDEN_CAPTURE_GIT_EXECUTABLE NAMES git)
if(GOLDEN_CAPTURE_GIT_EXECUTABLE)
    execute_process(
        COMMAND "${GOLDEN_CAPTURE_GIT_EXECUTABLE}"
            -C "${SOURCE_ROOT}" rev-parse --verify HEAD
        RESULT_VARIABLE _commit_result
        OUTPUT_VARIABLE _commit_output
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(_commit_result EQUAL 0
        AND _commit_output MATCHES "^[0-9a-fA-F]{40}$")
        string(TOLOWER "${_commit_output}" SOURCE_COMMIT)

        execute_process(
            COMMAND "${GOLDEN_CAPTURE_GIT_EXECUTABLE}"
                hash-object "${GOLDEN_CAPTURE_CPP}"
            RESULT_VARIABLE _blob_result
            OUTPUT_VARIABLE _blob_output
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
        if(_blob_result EQUAL 0
            AND _blob_output MATCHES "^[0-9a-fA-F]{40}$")
            string(TOLOWER "${_blob_output}" GOLDEN_CAPTURE_CPP_GIT_BLOB)
        endif()

        execute_process(
            COMMAND "${GOLDEN_CAPTURE_GIT_EXECUTABLE}"
                -C "${SOURCE_ROOT}" diff --quiet HEAD --
                "src/MphRead.Native/Mods/Render/GoldenCapture.cpp"
                "src/MphRead.Native/Mods/Render/GoldenCapture.hpp"
                "src/MphRead.Native/Mods/Render/GoldenCaptureValidation.hpp"
            RESULT_VARIABLE _diff_result
            ERROR_QUIET)
        if(_diff_result EQUAL 0)
            set(GOLDEN_CAPTURE_GIT_STATE "clean")
        elseif(_diff_result EQUAL 1)
            set(GOLDEN_CAPTURE_GIT_STATE "dirty")
        else()
            set(GOLDEN_CAPTURE_GIT_STATE "unknown")
        endif()
    endif()
endif()

get_filename_component(_output_directory "${OUTPUT_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${_output_directory}")

set(_content "#pragma once\n\n")
string(APPEND _content "namespace MphRead::Mods::Render::GoldenCaptureProvenance\n{\n")
string(APPEND _content "    inline constexpr const char SourceCommit[] = \"${SOURCE_COMMIT}\";\n")
string(APPEND _content "    inline constexpr const char GitHarnessState[] = \"${GOLDEN_CAPTURE_GIT_STATE}\";\n")
string(APPEND _content "    inline constexpr const char GoldenCaptureCppGitBlob[] = \"${GOLDEN_CAPTURE_CPP_GIT_BLOB}\";\n")
string(APPEND _content "    inline constexpr const char GoldenCaptureCppSha256[] = \"${GOLDEN_CAPTURE_CPP_SHA256}\";\n")
string(APPEND _content "    inline constexpr const char HarnessSha256[] = \"${GOLDEN_CAPTURE_HARNESS_SHA256}\";\n")
string(APPEND _content "}\n")

set(_existing "")
if(EXISTS "${OUTPUT_FILE}")
    file(READ "${OUTPUT_FILE}" _existing)
endif()
if(NOT _existing STREQUAL _content)
    file(WRITE "${OUTPUT_FILE}" "${_content}")
endif()
