# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.14)

project (wpm C)

include(CTest)

set(WPM_ZLIB_NG_BUFFER_KIB "1024" CACHE STRING
  "zlib-ng archive streaming buffer size in KiB (32 through 32768)"
)
if(NOT WPM_ZLIB_NG_BUFFER_KIB MATCHES "^[0-9]+$" OR
   WPM_ZLIB_NG_BUFFER_KIB LESS 32 OR WPM_ZLIB_NG_BUFFER_KIB GREATER 32768)
  message(FATAL_ERROR "WPM_ZLIB_NG_BUFFER_KIB must be an integer from 32 through 32768")
endif()
math(EXPR WPM_ZLIB_NG_BUFFER_BYTES "${WPM_ZLIB_NG_BUFFER_KIB} * 1024")

set(WPM_ZLIB_NG_COMPRESSION_LEVEL "2" CACHE STRING
  "zlib-ng package compression level (1 through 9)"
)
if(NOT WPM_ZLIB_NG_COMPRESSION_LEVEL MATCHES "^[0-9]+$" OR
   WPM_ZLIB_NG_COMPRESSION_LEVEL LESS 1 OR WPM_ZLIB_NG_COMPRESSION_LEVEL GREATER 9)
  message(FATAL_ERROR "WPM_ZLIB_NG_COMPRESSION_LEVEL must be an integer from 1 through 9")
endif()

if(DEFINED WPM_ARCH)
  set(wpm_arch "${WPM_ARCH}")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^[Aa][Rr][Mm]64|^[Aa][Aa][Rr][Cc][Hh]64")
  set(wpm_arch "arm64")
elseif(CMAKE_GENERATOR_PLATFORM MATCHES "^[Ww][Ii][Nn]32|^[Xx]86$")
  set(wpm_arch "x86")
elseif(CMAKE_GENERATOR_PLATFORM MATCHES "^[Aa][Rr][Mm]64")
  set(wpm_arch "arm64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(wpm_arch "x64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
  set(wpm_arch "x86")
else()
  set(wpm_arch "${CMAKE_SYSTEM_PROCESSOR}")
endif()

if(WIN32 AND NOT CMAKE_C_COMPILER_ID STREQUAL "TinyCC")
  message(FATAL_ERROR "WPM Windows builds require TinyCC")
endif()

if(WIN32)
  if(DEFINED WPM_WCRT_ROOT AND NOT "${WPM_WCRT_ROOT}" STREQUAL "")
    file(TO_CMAKE_PATH "${WPM_WCRT_ROOT}" WPM_WCRT_ROOT)
  elseif(DEFINED ENV{WPM_WCRT_ROOT} AND NOT "$ENV{WPM_WCRT_ROOT}" STREQUAL "")
    file(TO_CMAKE_PATH "$ENV{WPM_WCRT_ROOT}" WPM_WCRT_ROOT)
  else()
    file(TO_CMAKE_PATH "$ENV{ProgramFiles}/WCRT" WPM_WCRT_INSTALL_ROOT)
    file(GLOB WPM_WCRT_INSTALLATIONS LIST_DIRECTORIES TRUE "${WPM_WCRT_INSTALL_ROOT}/*")
    list(SORT WPM_WCRT_INSTALLATIONS COMPARE NATURAL ORDER DESCENDING)
    foreach(WPM_WCRT_INSTALLATION IN LISTS WPM_WCRT_INSTALLATIONS)
      if(EXISTS "${WPM_WCRT_INSTALLATION}/${wpm_arch}/bin/wcrt.dll" AND
         EXISTS "${WPM_WCRT_INSTALLATION}/include/dirent.h" AND
         EXISTS "${WPM_WCRT_INSTALLATION}/include/stdio.h" AND
         EXISTS "${WPM_WCRT_INSTALLATION}/include/wcrt/thread_pool.h" AND
         EXISTS "${WPM_WCRT_INSTALLATION}/${wpm_arch}/lib/libwcrt.a" AND
         EXISTS "${WPM_WCRT_INSTALLATION}/${wpm_arch}/lib/wcrt-startup-console.o")
        set(WPM_WCRT_ROOT "${WPM_WCRT_INSTALLATION}")
        break()
      endif()
    endforeach()
  endif()

  if(NOT WPM_WCRT_ROOT)
    message(FATAL_ERROR "WCRT 1.2.5 or newer was not found. Install it or set WPM_WCRT_ROOT.")
  endif()

  set(WPM_WCRT_TARGET_ROOT "${WPM_WCRT_ROOT}/${wpm_arch}")
  # Accept an explicitly selected legacy, single-architecture SDK as well.
  if(EXISTS "${WPM_WCRT_ROOT}/bin/wcrt.dll")
    set(WPM_WCRT_TARGET_ROOT "${WPM_WCRT_ROOT}")
  endif()

  foreach(WPM_WCRT_REQUIRED_FILE
      "include/wcrt/thread_pool.h"
      "include/dirent.h"
      "include/stdio.h"
      "include/errno.h"
      "include/string.h"
      "include/sys/types.h"
      "include/sys/stat.h"
      "include/utime.h")
    if(NOT EXISTS "${WPM_WCRT_ROOT}/${WPM_WCRT_REQUIRED_FILE}")
      message(FATAL_ERROR
        "The WCRT SDK at ${WPM_WCRT_ROOT} is missing ${WPM_WCRT_REQUIRED_FILE}. "
        "Install WCRT 1.2.5 or newer.")
    endif()
  endforeach()
  foreach(WPM_WCRT_REQUIRED_FILE
      "bin/wcrt.dll"
      "lib/libwcrt.a"
      "lib/wcrt-startup-console.o")
    if(NOT EXISTS "${WPM_WCRT_TARGET_ROOT}/${WPM_WCRT_REQUIRED_FILE}")
      message(FATAL_ERROR
        "The WCRT SDK at ${WPM_WCRT_ROOT} has no ${wpm_arch} target file "
        "${WPM_WCRT_REQUIRED_FILE}. Install WCRT 1.2.5 or newer.")
    endif()
  endforeach()

  message(STATUS "Using WCRT from ${WPM_WCRT_ROOT}")
  get_filename_component(WPM_WCRT_VERSION "${WPM_WCRT_ROOT}" NAME)
  if(NOT WPM_WCRT_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+")
    set(WPM_WCRT_VERSION "unknown")
  endif()
  if(EXISTS "${WPM_WCRT_ROOT}/.wpm/package.txt")
    file(STRINGS "${WPM_WCRT_ROOT}/.wpm/package.txt" WPM_WCRT_PACKAGE_VERSION_LINE
      REGEX "^version=")
    if(WPM_WCRT_PACKAGE_VERSION_LINE)
      list(GET WPM_WCRT_PACKAGE_VERSION_LINE 0 WPM_WCRT_PACKAGE_VERSION_LINE)
      string(REGEX REPLACE "^version=" "" WPM_WCRT_VERSION
        "${WPM_WCRT_PACKAGE_VERSION_LINE}")
    endif()
  endif()
  if(NOT WPM_WCRT_VERSION STREQUAL "unknown" AND
      WPM_WCRT_VERSION VERSION_LESS "1.2.5")
    message(FATAL_ERROR
      "WCRT ${WPM_WCRT_VERSION} is too old; WPM requires WCRT 1.2.5 or newer.")
  endif()
  message(STATUS "WCRT version: ${WPM_WCRT_VERSION}")
  include_directories(BEFORE "${WPM_WCRT_ROOT}/include")
endif()

set(WPM_BIN_DIR
  "${CMAKE_SOURCE_DIR}/bin/${wpm_arch}"
  CACHE PATH "Output directory for final WPM build artifacts"
)

find_package(Git QUIET)
if(GIT_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} status --porcelain
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE WPM_GIT_STATUS
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )

  if(WPM_GIT_STATUS)
    message(WARNING "Git working tree is dirty; build outputs may not be reproducible.")
  endif()
endif()

# Include sub-projects.
add_subdirectory ("third_party")
add_subdirectory ("wpm")
