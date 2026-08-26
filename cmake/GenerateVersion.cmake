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

if(DEFINED WPM_MINIZIP_NG_VERSION_OVERRIDE AND NOT WPM_MINIZIP_NG_VERSION_OVERRIDE STREQUAL "" AND
   DEFINED WPM_MINIZIP_NG_COMMIT_OVERRIDE AND NOT WPM_MINIZIP_NG_COMMIT_OVERRIDE STREQUAL "")
    set(minizip_ng_version "${WPM_MINIZIP_NG_VERSION_OVERRIDE}")
    set(minizip_ng_commit "${WPM_MINIZIP_NG_COMMIT_OVERRIDE}")
    set(minizip_ng_dirty 0)
else()
execute_process(
    COMMAND git describe --tags --exact-match
    WORKING_DIRECTORY "${WPM_SOURCE_DIR}/third_party/minizip-ng"
    RESULT_VARIABLE minizip_ng_tag_result
    OUTPUT_VARIABLE minizip_ng_version
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(NOT minizip_ng_tag_result EQUAL 0)
    file(STRINGS "${WPM_SOURCE_DIR}/third_party/minizip-ng/mz.h" minizip_ng_version_line
        REGEX "^#define MZ_VERSION[ \\t]+")
    if(minizip_ng_version_line MATCHES "MZ_VERSION[ \\t]+\\(\"([^\"]+)\"\\)")
        set(minizip_ng_version "${CMAKE_MATCH_1}")
    else()
        set(minizip_ng_version "unknown")
    endif()
endif()

execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY "${WPM_SOURCE_DIR}/third_party/minizip-ng"
    RESULT_VARIABLE minizip_ng_commit_result
    OUTPUT_VARIABLE minizip_ng_commit
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(NOT minizip_ng_commit_result EQUAL 0)
    execute_process(
        COMMAND git rev-parse HEAD:third_party/minizip-ng
        WORKING_DIRECTORY "${WPM_SOURCE_DIR}"
        RESULT_VARIABLE minizip_ng_gitlink_result
        OUTPUT_VARIABLE minizip_ng_gitlink_commit
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(minizip_ng_gitlink_result EQUAL 0)
        string(SUBSTRING "${minizip_ng_gitlink_commit}" 0 7 minizip_ng_commit)
    else()
        set(minizip_ng_commit "unknown")
    endif()
endif()

execute_process(
    COMMAND git status --porcelain --untracked-files=no
    WORKING_DIRECTORY "${WPM_SOURCE_DIR}/third_party/minizip-ng"
    OUTPUT_VARIABLE minizip_ng_dirty_files
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(minizip_ng_dirty_files STREQUAL "")
    set(minizip_ng_dirty 0)
else()
    set(minizip_ng_dirty 1)
endif()
endif()

if(DEFINED WPM_ZLIB_NG_VERSION_OVERRIDE AND NOT WPM_ZLIB_NG_VERSION_OVERRIDE STREQUAL "" AND
   DEFINED WPM_ZLIB_NG_COMMIT_OVERRIDE AND NOT WPM_ZLIB_NG_COMMIT_OVERRIDE STREQUAL "")
    set(zlib_ng_version "${WPM_ZLIB_NG_VERSION_OVERRIDE}")
    set(zlib_ng_commit "${WPM_ZLIB_NG_COMMIT_OVERRIDE}")
    set(zlib_ng_dirty 0)
else()
execute_process(
    COMMAND git describe --tags --exact-match
    WORKING_DIRECTORY "${WPM_SOURCE_DIR}/third_party/zlib-ng"
    RESULT_VARIABLE zlib_ng_tag_result
    OUTPUT_VARIABLE zlib_ng_version
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(NOT zlib_ng_tag_result EQUAL 0)
    file(STRINGS "${WPM_SOURCE_DIR}/third_party/zlib-ng/zlib-ng.h.in" zlib_ng_version_line
        REGEX "^#define ZLIBNG_VERSION")
    if(zlib_ng_version_line MATCHES "ZLIBNG_VERSION[ \t]+\"([^\"]+)\"")
        set(zlib_ng_version "${CMAKE_MATCH_1}")
    else()
        set(zlib_ng_version "unknown")
    endif()
endif()

execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY "${WPM_SOURCE_DIR}/third_party/zlib-ng"
    RESULT_VARIABLE zlib_ng_commit_result
    OUTPUT_VARIABLE zlib_ng_commit
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(NOT zlib_ng_commit_result EQUAL 0)
    execute_process(
        COMMAND git rev-parse HEAD:third_party/zlib-ng
        WORKING_DIRECTORY "${WPM_SOURCE_DIR}"
        RESULT_VARIABLE zlib_ng_gitlink_result
        OUTPUT_VARIABLE zlib_ng_gitlink_commit
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(zlib_ng_gitlink_result EQUAL 0)
        string(SUBSTRING "${zlib_ng_gitlink_commit}" 0 7 zlib_ng_commit)
    else()
        set(zlib_ng_commit "unknown")
    endif()
endif()

execute_process(
    COMMAND git status --porcelain --untracked-files=no
    WORKING_DIRECTORY "${WPM_SOURCE_DIR}/third_party/zlib-ng"
    OUTPUT_VARIABLE zlib_ng_dirty_files
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(zlib_ng_dirty_files STREQUAL "")
    set(zlib_ng_dirty 0)
else()
    set(zlib_ng_dirty 1)
endif()
endif()

if(DEFINED WPM_LIBSODIUM_VERSION_OVERRIDE AND NOT WPM_LIBSODIUM_VERSION_OVERRIDE STREQUAL "" AND
   DEFINED WPM_LIBSODIUM_COMMIT_OVERRIDE AND NOT WPM_LIBSODIUM_COMMIT_OVERRIDE STREQUAL "")
    set(sodium_version "${WPM_LIBSODIUM_VERSION_OVERRIDE}")
    set(sodium_commit "${WPM_LIBSODIUM_COMMIT_OVERRIDE}")
    set(sodium_dirty 0)
else()
execute_process(
    COMMAND git describe --tags --exact-match
    WORKING_DIRECTORY "${WPM_SOURCE_DIR}/third_party/libsodium"
    RESULT_VARIABLE sodium_tag_result
    OUTPUT_VARIABLE sodium_version
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(NOT sodium_tag_result EQUAL 0)
    file(STRINGS
        "${WPM_SOURCE_DIR}/third_party/libsodium/src/libsodium/include/sodium/version.h"
        sodium_version_line
        REGEX "^#define SODIUM_VERSION_STRING")
    if(sodium_version_line MATCHES "SODIUM_VERSION_STRING[ \t]+\"([^\"]+)\"")
        set(sodium_version "${CMAKE_MATCH_1}")
    else()
        set(sodium_version "unknown")
    endif()
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
    execute_process(
        COMMAND git rev-parse HEAD:third_party/libsodium
        WORKING_DIRECTORY "${WPM_SOURCE_DIR}"
        RESULT_VARIABLE sodium_gitlink_result
        OUTPUT_VARIABLE sodium_gitlink_commit
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(sodium_gitlink_result EQUAL 0)
        string(SUBSTRING "${sodium_gitlink_commit}" 0 7 sodium_commit)
    else()
        set(sodium_commit "unknown")
    endif()
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
endif()

set(version_header "#ifndef WPM_VERSION_H\n#define WPM_VERSION_H\n\n
#define WPM_VERSION \"${version}\"\n
#define WPM_MINIZIP_NG_VERSION \"${minizip_ng_version}\"\n
#define WPM_MINIZIP_NG_COMMIT \"${minizip_ng_commit}\"\n
#define WPM_MINIZIP_NG_DIRTY ${minizip_ng_dirty}\n
#define WPM_ZLIB_NG_VERSION \"${zlib_ng_version}\"\n
#define WPM_ZLIB_NG_COMMIT \"${zlib_ng_commit}\"\n
#define WPM_ZLIB_NG_DIRTY ${zlib_ng_dirty}\n
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

# Windows VERSIONINFO requires four numeric 16-bit components. Keep the full
# Git-derived version in the string fields while using its SemVer core (and,
# for development builds, the commit count) for the numeric file version.
set(version_major 0)
set(version_minor 0)
set(version_patch 0)
set(version_build 0)
if(version MATCHES "^v?([0-9]+)\\.([0-9]+)\\.([0-9]+)")
    set(version_major "${CMAKE_MATCH_1}")
    set(version_minor "${CMAKE_MATCH_2}")
    set(version_patch "${CMAKE_MATCH_3}")
endif()
if(version MATCHES "-dev\\+([0-9]+)")
    set(version_build "${CMAKE_MATCH_1}")
endif()

foreach(component version_major version_minor version_patch version_build)
    if(${component} GREATER 65535)
        set(${component} 65535)
    endif()
endforeach()

if(NOT DEFINED WPM_ICON_FILE OR NOT EXISTS "${WPM_ICON_FILE}")
    message(FATAL_ERROR "WPM icon file was not found: ${WPM_ICON_FILE}")
endif()
file(TO_CMAKE_PATH "${WPM_ICON_FILE}" version_resource_icon)
file(SHA256 "${WPM_ICON_FILE}" version_resource_icon_sha256)

string(CONCAT version_resource
    "#include <winver.h>\n\n"
    "// Icon SHA-256: ${version_resource_icon_sha256}\n"
    "101 ICON \"${version_resource_icon}\"\n\n"
    "VS_VERSION_INFO VERSIONINFO\n"
    " FILEVERSION ${version_major},${version_minor},${version_patch},${version_build}\n"
    " PRODUCTVERSION ${version_major},${version_minor},${version_patch},${version_build}\n"
    " FILEFLAGSMASK VS_FFI_FILEFLAGSMASK\n"
    " FILEFLAGS 0x0L\n"
    " FILEOS VOS_NT_WINDOWS32\n"
    " FILETYPE VFT_APP\n"
    " FILESUBTYPE VFT2_UNKNOWN\n"
    "BEGIN\n"
    "  BLOCK \"StringFileInfo\"\n"
    "  BEGIN\n"
    "    BLOCK \"040904B0\"\n"
    "    BEGIN\n"
    "      VALUE \"Comments\", \"https://github.com/Thewafflication/wpm\"\n"
    "      VALUE \"FileDescription\", \"Waughtal Package Manager\"\n"
    "      VALUE \"FileVersion\", \"${version}\"\n"
    "      VALUE \"InternalName\", \"wpm\"\n"
    "      VALUE \"OriginalFilename\", \"wpm.exe\"\n"
    "      VALUE \"ProductName\", \"WPM\"\n"
    "      VALUE \"ProductVersion\", \"${version}\"\n"
    "    END\n"
    "  END\n"
    "  BLOCK \"VarFileInfo\"\n"
    "  BEGIN\n"
    "    VALUE \"Translation\", 0x0409, 1200\n"
    "  END\n"
    "END\n"
)

if(EXISTS "${WPM_VERSION_RESOURCE}")
    file(READ "${WPM_VERSION_RESOURCE}" existing_version_resource)
else()
    set(existing_version_resource "")
endif()

if(NOT existing_version_resource STREQUAL version_resource)
    file(WRITE "${WPM_VERSION_RESOURCE}" "${version_resource}")
endif()
