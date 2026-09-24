# Copy the DLLs a freshly built binary needs into its own directory, so it runs
# without the toolchain's bin directory on PATH.
#
# Only the compiler's own runtime is linked statically; everything else here is
# a third-party library the program loads at run time, so it ships beside the
# executable instead. The closure is walked with objdump, because a DLL of its
# own may need more.
#
# Invoked as:
#   cmake -DBINARY=<exe> -DSEARCH_DIR=<toolchain bin> -DOBJDUMP=<objdump>
#         -P copy-runtime-dlls.cmake

if(NOT EXISTS "${BINARY}")
    message(FATAL_ERROR "copy-runtime-dlls: ${BINARY} does not exist.")
endif()

get_filename_component(_output_dir "${BINARY}" DIRECTORY)

set(_pending "${BINARY}")
set(_seen "")

while(_pending)
    list(POP_FRONT _pending _current)

    execute_process(
        COMMAND "${OBJDUMP}" -p "${_current}"
        OUTPUT_VARIABLE _dump
        ERROR_QUIET
        RESULT_VARIABLE _status)
    if(NOT _status EQUAL 0)
        continue()
    endif()

    string(REGEX MATCHALL "DLL Name: [^\n\r]+" _lines "${_dump}")
    foreach(_line IN LISTS _lines)
        string(REPLACE "DLL Name: " "" _name "${_line}")
        string(STRIP "${_name}" _name)

        list(FIND _seen "${_name}" _index)
        if(NOT _index EQUAL -1)
            continue()
        endif()
        list(APPEND _seen "${_name}")

        # A DLL that is not in the toolchain's directory is the system's own.
        if(NOT EXISTS "${SEARCH_DIR}/${_name}")
            continue()
        endif()

        configure_file("${SEARCH_DIR}/${_name}" "${_output_dir}/${_name}" COPYONLY)
        list(APPEND _pending "${SEARCH_DIR}/${_name}")
    endforeach()
endwhile()
