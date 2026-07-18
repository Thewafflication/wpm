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
        COMMAND git describe --tags --long --abbrev=7 HEAD
        WORKING_DIRECTORY "${WPM_SOURCE_DIR}"
        RESULT_VARIABLE describe_result
        OUTPUT_VARIABLE describe_version
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    if(describe_result EQUAL 0 AND describe_version MATCHES "^(.+)-([0-9]+)-g([0-9A-Fa-f]+)$")
        set(version "${CMAKE_MATCH_1}-dev+${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
    else()
        execute_process(
            COMMAND git rev-list --count HEAD
            WORKING_DIRECTORY "${WPM_SOURCE_DIR}"
            RESULT_VARIABLE count_result
            OUTPUT_VARIABLE commit_count
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        execute_process(
            COMMAND git rev-parse --short=7 HEAD
            WORKING_DIRECTORY "${WPM_SOURCE_DIR}"
            RESULT_VARIABLE commit_result
            OUTPUT_VARIABLE commit_version
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(NOT count_result EQUAL 0 OR NOT commit_result EQUAL 0)
            set(version "unknown")
        else()
            set(version "0.0.0-dev+${commit_count}.${commit_version}")
        endif()
    endif()

    if(NOT version STREQUAL "unknown")
        execute_process(
            COMMAND git status --porcelain --untracked-files=no
            WORKING_DIRECTORY "${WPM_SOURCE_DIR}"
            OUTPUT_VARIABLE dirty_files
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )

        if(NOT dirty_files STREQUAL "")
            string(APPEND version ".dirty")
        endif()
    endif()
endif()

execute_process(
    COMMAND git describe --tags --exact-match
    WORKING_DIRECTORY "${WPM_SOURCE_DIR}/third_party/miniz"
    RESULT_VARIABLE miniz_tag_result
    OUTPUT_VARIABLE miniz_version
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(NOT miniz_tag_result EQUAL 0)
    set(miniz_version "unknown")
endif()

execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY "${WPM_SOURCE_DIR}/third_party/miniz"
    RESULT_VARIABLE miniz_commit_result
    OUTPUT_VARIABLE miniz_commit
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(NOT miniz_commit_result EQUAL 0)
    set(miniz_commit "unknown")
endif()

execute_process(
    COMMAND git status --porcelain --untracked-files=no
    WORKING_DIRECTORY "${WPM_SOURCE_DIR}/third_party/miniz"
    OUTPUT_VARIABLE miniz_dirty_files
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(miniz_dirty_files STREQUAL "")
    set(miniz_dirty 0)
else()
    set(miniz_dirty 1)
endif()

execute_process(
    COMMAND git describe --tags --exact-match
    WORKING_DIRECTORY "${WPM_SOURCE_DIR}/third_party/libsodium"
    RESULT_VARIABLE sodium_tag_result
    OUTPUT_VARIABLE sodium_version
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(NOT sodium_tag_result EQUAL 0)
    set(sodium_version "unknown")
endif()

execute_process(
	COMMAND git rev-parse --short HEAD
	WORKING_DIRECTORY "${WPM_SOURCE_DIR}/third_party/libsodium"
        RESULT_VARIABLE sodium_commit_result
        OUTPUT_VARIABLE sodium_commit
        OUTPUT_STRIP_TRAILING_WHITESPACE
	ERROR_QUIET
)

if(NOT sodium_commit_result EQUAL 0)
	set(sodium_commit "unknown")
endif()

execute_process(
	COMMAND git status --porcelain --untracked-files=no
        WORKING_DIRECTORY "${WPM_SOURCE_DIR}/third_party/libsodium"
        OUTPUT_VARIABLE sodium_dirty_files
        OUTPUT_STRIP_TRAILING_WHITESPACE
	ERROR_QUIET
)

if(sodium_dirty_files STREQUAL "")
        set(sodium_dirty 0)
else()
	set(sodium_dirty 1)
endif()

set(version_header "#ifndef WPM_VERSION_H\n#define WPM_VERSION_H\n\n
#define WPM_VERSION \"${version}\"\n
#define WPM_MINIZ_VERSION \"${miniz_version}\"\n
#define WPM_MINIZ_COMMIT \"${miniz_commit}\"\n
#define WPM_MINIZ_DIRTY ${miniz_dirty}\n
#define WPM_SODIUM_VERSION \"${sodium_version}\"\n
#define WPM_SODIUM_COMMIT \"${sodium_commit}\"\n
#define WPM_SODIUM_DIRTY ${sodium_dirty}\n
\n#endif\n")

if(EXISTS "${WPM_VERSION_HEADER}")
    file(READ "${WPM_VERSION_HEADER}" existing_version_header)
else()
    set(existing_version_header "")
endif()

if(NOT existing_version_header STREQUAL version_header)
    file(WRITE "${WPM_VERSION_HEADER}" "${version_header}")
endif()
