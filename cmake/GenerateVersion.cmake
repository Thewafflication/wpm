execute_process(
    COMMAND git describe --tags --exact-match
    WORKING_DIRECTORY "${WPM_SOURCE_DIR}"
    RESULT_VARIABLE tag_result
    OUTPUT_VARIABLE version
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(NOT tag_result EQUAL 0)
    execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY "${WPM_SOURCE_DIR}"
        RESULT_VARIABLE commit_result
        OUTPUT_VARIABLE version
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    if(NOT commit_result EQUAL 0)
        set(version "unknown")
    else()
        execute_process(
            COMMAND git status --porcelain --untracked-files=no
            WORKING_DIRECTORY "${WPM_SOURCE_DIR}"
            OUTPUT_VARIABLE dirty_files
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )

        if(NOT dirty_files STREQUAL "")
            string(APPEND version "-dirty")
        endif()
    endif()
endif()

set(version_header "#ifndef WPM_VERSION_H\n#define WPM_VERSION_H\n\n#define WPM_VERSION \"${version}\"\n\n#endif\n")

if(EXISTS "${WPM_VERSION_HEADER}")
    file(READ "${WPM_VERSION_HEADER}" existing_version_header)
else()
    set(existing_version_header "")
endif()

if(NOT existing_version_header STREQUAL version_header)
    file(WRITE "${WPM_VERSION_HEADER}" "${version_header}")
endif()
