cmake_minimum_required(VERSION 3.10)

project(wpm_third_party C)

include(GenerateExportHeader)

if(WIN32 AND NOT CMAKE_C_COMPILER_ID STREQUAL "TinyCC")
  message(FATAL_ERROR "WPM third-party Windows builds require TinyCC")
endif()

function(wpm_third_party_set_archive_output target_name)
  if(CMAKE_CONFIGURATION_TYPES)
    set(wpm_third_party_default_output_dir "${WPM_THIRD_PARTY_BIN_DIR}")
  elseif(CMAKE_BUILD_TYPE)
    set(wpm_third_party_default_output_dir "${WPM_THIRD_PARTY_BIN_DIR}/${CMAKE_BUILD_TYPE}")
  else()
    set(wpm_third_party_default_output_dir "${WPM_THIRD_PARTY_BIN_DIR}")
  endif()

  set_target_properties(${target_name} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${wpm_third_party_default_output_dir}"
    LIBRARY_OUTPUT_DIRECTORY "${wpm_third_party_default_output_dir}"
    RUNTIME_OUTPUT_DIRECTORY "${wpm_third_party_default_output_dir}"
  )

  foreach(config Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${config}" config_upper)
    set_target_properties(${target_name} PROPERTIES
      ARCHIVE_OUTPUT_DIRECTORY_${config_upper} "${WPM_THIRD_PARTY_BIN_DIR}/${config}"
      LIBRARY_OUTPUT_DIRECTORY_${config_upper} "${WPM_THIRD_PARTY_BIN_DIR}/${config}"
      RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${WPM_THIRD_PARTY_BIN_DIR}/${config}"
    )
  endforeach()
endfunction()

if(DEFINED WPM_THIRD_PARTY_ARCH)
  set(wpm_third_party_arch "${WPM_THIRD_PARTY_ARCH}")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^[Aa][Rr][Mm]64|^[Aa][Aa][Rr][Cc][Hh]64")
  set(wpm_third_party_arch "arm64")
elseif(CMAKE_GENERATOR_PLATFORM MATCHES "^[Ww][Ii][Nn]32|^[Xx]86$")
  set(wpm_third_party_arch "x86")
elseif(CMAKE_GENERATOR_PLATFORM MATCHES "^[Aa][Rr][Mm]64")
  set(wpm_third_party_arch "arm64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(wpm_third_party_arch "x64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
  set(wpm_third_party_arch "x86")
else()
  set(wpm_third_party_arch "${CMAKE_SYSTEM_PROCESSOR}")
endif()

set(WPM_THIRD_PARTY_BIN_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/bin/${wpm_third_party_arch}"
  CACHE PATH "Output directory for third-party static libraries"
)

set(MINIZ_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/miniz")

if(NOT TARGET miniz)
  add_library(miniz STATIC
    "${MINIZ_SOURCE_DIR}/miniz.c"
    "${MINIZ_SOURCE_DIR}/miniz_zip.c"
    "${MINIZ_SOURCE_DIR}/miniz_tinfl.c"
    "${MINIZ_SOURCE_DIR}/miniz_tdef.c"
  )

  generate_export_header(miniz)

  target_include_directories(miniz PUBLIC
    "${MINIZ_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}"
  )
endif()

set_target_properties(miniz PROPERTIES
  OUTPUT_NAME miniz
  C_STANDARD 99
  C_STANDARD_REQUIRED ON
  C_EXTENSIONS OFF
)
if(WIN32)
  # WCRT uses a 64-bit time_t on every architecture. Load that definition
  # before TinyCC's WinAPI headers and suppress their legacy x86 definition.
  target_compile_options(miniz PRIVATE -include time.h)
  # Miniz uses POSIX file-status names. WCRT intentionally exposes the
  # explicit-width Windows interfaces instead.
  target_compile_definitions(miniz PRIVATE
    _TIME_T_DEFINED
    stat=_stat64
    utimbuf=__utimbuf64
    utime=_utime64
  )
endif()
wpm_third_party_set_archive_output(miniz)

set(LIBSODIUM_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libsodium")
file(GLOB_RECURSE libsodium_sources
  "${LIBSODIUM_SOURCE_DIR}/src/libsodium/*.c"
)

if(WIN32)
  # The internal random implementation is not selected by the Windows build,
  # and assumes a Unix unistd.h on this compiler.
  list(FILTER libsodium_sources EXCLUDE REGEX
    "/randombytes/internal/randombytes_internal_random\\.c$"
  )
endif()

if(NOT libsodium_sources)
  message(FATAL_ERROR "Unable to discover libsodium sources beneath ${LIBSODIUM_SOURCE_DIR}")
endif()

if(NOT TARGET libsodium)
  set(LIBSODIUM_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/libsodium/include")
  file(MAKE_DIRECTORY "${LIBSODIUM_GENERATED_INCLUDE_DIR}/sodium")
  if(WIN32)
    # libsodium includes this header for Windows, but the APIs used by this
    # translation unit are already declared by TinyCC's windows.h.
    file(WRITE "${LIBSODIUM_GENERATED_INCLUDE_DIR}/wincrypt.h" "#pragma once\n")
    # This header is included unconditionally by libsodium's cross-platform
    # RNG source, but none of its declarations are used by the Windows path.
    file(WRITE "${LIBSODIUM_GENERATED_INCLUDE_DIR}/fcntl.h" "#pragma once\n")
  endif()
  file(STRINGS "${LIBSODIUM_SOURCE_DIR}/configure.ac" libsodium_version_line
    REGEX "^AC_INIT\\(\\[libsodium\\],\\[[0-9]+\\.[0-9]+\\.[0-9]+\\]"
  )
  file(STRINGS "${LIBSODIUM_SOURCE_DIR}/configure.ac" libsodium_major_line
    REGEX "^SODIUM_LIBRARY_VERSION_MAJOR=[0-9]+$"
  )
  file(STRINGS "${LIBSODIUM_SOURCE_DIR}/configure.ac" libsodium_minor_line
    REGEX "^SODIUM_LIBRARY_VERSION_MINOR=[0-9]+$"
  )
  if(NOT libsodium_version_line OR NOT libsodium_major_line OR NOT libsodium_minor_line)
    message(FATAL_ERROR "Unable to read the libsodium version from configure.ac")
  endif()
  string(REGEX REPLACE
    "^AC_INIT\\(\\[libsodium\\],\\[([^]]+)\\].*$" "\\1"
    VERSION "${libsodium_version_line}"
  )
  string(REGEX REPLACE "^.*=([0-9]+)$" "\\1"
    SODIUM_LIBRARY_VERSION_MAJOR "${libsodium_major_line}"
  )
  string(REGEX REPLACE "^.*=([0-9]+)$" "\\1"
    SODIUM_LIBRARY_VERSION_MINOR "${libsodium_minor_line}"
  )
  set(SODIUM_LIBRARY_MINIMAL_DEF "")
  configure_file(
    "${LIBSODIUM_SOURCE_DIR}/src/libsodium/include/sodium/version.h.in"
    "${LIBSODIUM_GENERATED_INCLUDE_DIR}/sodium/version.h"
    @ONLY
  )
  configure_file(
    "${LIBSODIUM_SOURCE_DIR}/src/libsodium/include/sodium/export.h"
    "${LIBSODIUM_GENERATED_INCLUDE_DIR}/sodium/export.h"
    COPYONLY
  )

  add_library(libsodium STATIC ${libsodium_sources})

  target_include_directories(libsodium PUBLIC
    "${LIBSODIUM_GENERATED_INCLUDE_DIR}"
    "${LIBSODIUM_GENERATED_INCLUDE_DIR}/sodium"
    "${LIBSODIUM_SOURCE_DIR}/src/libsodium/include"
    "${LIBSODIUM_SOURCE_DIR}/src/libsodium/include/sodium"
  )

  target_compile_definitions(libsodium PRIVATE
    NATIVE_LITTLE_ENDIAN
    SODIUM_STATIC
    _CRT_SECURE_NO_WARNINGS
    _LIB
  )
  if(WIN32)
    # Libsodium reports these POSIX conditions even on its Windows paths;
    # they are outside the ISO C errno set exposed by WCRT.
    target_compile_options(libsodium PRIVATE -include time.h)
    target_compile_definitions(libsodium PRIVATE
      _TIME_T_DEFINED
      EFBIG=27
      ENXIO=6
    )
  endif()

endif()

set_target_properties(libsodium PROPERTIES
  OUTPUT_NAME libsodium
  C_STANDARD 99
  C_STANDARD_REQUIRED ON
  C_EXTENSIONS OFF
)
wpm_third_party_set_archive_output(libsodium)

add_custom_target(wpm-third-party
  DEPENDS
    miniz
    libsodium
)
