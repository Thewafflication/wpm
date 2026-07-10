cmake_minimum_required(VERSION 3.8)

project(wpm_third_party C)

include(GenerateExportHeader)

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

set_target_properties(miniz PROPERTIES OUTPUT_NAME miniz)
wpm_third_party_set_archive_output(miniz)

set(LIBSODIUM_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libsodium")
set(LIBSODIUM_MSVC_PROJECT
  "${LIBSODIUM_SOURCE_DIR}/builds/msvc/vs2022/libsodium/libsodium.vcxproj"
)

if(NOT EXISTS "${LIBSODIUM_MSVC_PROJECT}")
  message(FATAL_ERROR "Unable to find libsodium MSVC project: ${LIBSODIUM_MSVC_PROJECT}")
endif()

file(STRINGS "${LIBSODIUM_MSVC_PROJECT}" libsodium_vcxproj_lines
  REGEX "<ClCompile Include=\".*\\.c\""
)

set(libsodium_sources)
get_filename_component(LIBSODIUM_MSVC_PROJECT_DIR
  "${LIBSODIUM_MSVC_PROJECT}"
  DIRECTORY
)

foreach(libsodium_vcxproj_line IN LISTS libsodium_vcxproj_lines)
  string(REGEX REPLACE "^.*<ClCompile Include=\"([^\"]+\\.c)\".*$" "\\1"
    libsodium_source_rel
    "${libsodium_vcxproj_line}"
  )
  get_filename_component(libsodium_source_abs
    "${libsodium_source_rel}"
    ABSOLUTE
    BASE_DIR "${LIBSODIUM_MSVC_PROJECT_DIR}"
  )
  file(TO_CMAKE_PATH "${libsodium_source_abs}" libsodium_source_abs)
  list(APPEND libsodium_sources "${libsodium_source_abs}")
endforeach()

list(REMOVE_DUPLICATES libsodium_sources)

if(NOT libsodium_sources)
  message(FATAL_ERROR "Unable to discover libsodium sources from ${LIBSODIUM_MSVC_PROJECT}")
endif()

if(NOT TARGET libsodium)
  set(LIBSODIUM_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/libsodium/include")
  file(MAKE_DIRECTORY "${LIBSODIUM_GENERATED_INCLUDE_DIR}/sodium")
  configure_file(
    "${LIBSODIUM_SOURCE_DIR}/builds/msvc/version.h"
    "${LIBSODIUM_GENERATED_INCLUDE_DIR}/sodium/version.h"
    COPYONLY
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
    inline=__inline
    NATIVE_LITTLE_ENDIAN
    SODIUM_STATIC
    _CRT_SECURE_NO_WARNINGS
    _LIB
  )

  if(MSVC)
    target_compile_options(libsodium PRIVATE
      /W3
      /wd4146
      /wd4244
      $<$<VERSION_GREATER_EQUAL:${MSVC_VERSION},1930>:/UndefIntOverflow->
    )
  endif()
endif()

set_target_properties(libsodium PROPERTIES OUTPUT_NAME libsodium)
wpm_third_party_set_archive_output(libsodium)

add_custom_target(wpm-third-party
  DEPENDS
    miniz
    libsodium
)
