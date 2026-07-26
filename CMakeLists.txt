# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.10)

# Let CMake select the MSVC runtime per target/configuration when supported.
if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project (wpm C)

include(CTest)

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

# Keep the explicitly selected 32-bit XP build usable on the oldest platform
# and processor it targets.
# Pentium III provides SSE, but not SSE2.  Current MSVC x86 builds otherwise
# default to a newer instruction-set baseline, and current linkers otherwise
# stamp executables with a post-XP subsystem version.
option(WPM_WINDOWS_XP_COMPAT "Enable Windows XP compatibility workarounds" OFF)
option(WPM_USE_WCRT "Link TinyCC builds against WCRT instead of MSVCRT" OFF)
if(WPM_WINDOWS_XP_COMPAT AND NOT (WIN32 AND wpm_arch STREQUAL "x86"))
  message(FATAL_ERROR "WPM_WINDOWS_XP_COMPAT is supported only by Windows x86 builds")
endif()

if(WPM_USE_WCRT)
  if(NOT CMAKE_C_COMPILER_ID STREQUAL "TinyCC")
    message(FATAL_ERROR "WPM_USE_WCRT requires TinyCC")
  endif()

  if(DEFINED WPM_WCRT_ROOT AND NOT "${WPM_WCRT_ROOT}" STREQUAL "")
    file(TO_CMAKE_PATH "${WPM_WCRT_ROOT}" WPM_WCRT_ROOT)
  elseif(DEFINED ENV{WPM_WCRT_ROOT} AND NOT "$ENV{WPM_WCRT_ROOT}" STREQUAL "")
    file(TO_CMAKE_PATH "$ENV{WPM_WCRT_ROOT}" WPM_WCRT_ROOT)
  else()
    file(TO_CMAKE_PATH "$ENV{ProgramFiles}/WCRT" WPM_WCRT_INSTALL_ROOT)
    file(GLOB WPM_WCRT_INSTALLATIONS LIST_DIRECTORIES TRUE "${WPM_WCRT_INSTALL_ROOT}/*")
    list(SORT WPM_WCRT_INSTALLATIONS COMPARE NATURAL ORDER DESCENDING)
    foreach(WPM_WCRT_INSTALLATION IN LISTS WPM_WCRT_INSTALLATIONS)
      if(EXISTS "${WPM_WCRT_INSTALLATION}/bin/wcrt.dll" AND
         EXISTS "${WPM_WCRT_INSTALLATION}/include/stdio.h" AND
         EXISTS "${WPM_WCRT_INSTALLATION}/lib/wcrt.def" AND
         EXISTS "${WPM_WCRT_INSTALLATION}/lib/libwcrt.a")
        set(WPM_WCRT_ROOT "${WPM_WCRT_INSTALLATION}")
        break()
      endif()
    endforeach()
  endif()

  if(NOT WPM_WCRT_ROOT)
    message(FATAL_ERROR "WCRT was not found. Install it or set WPM_WCRT_ROOT.")
  endif()

  message(STATUS "Using WCRT from ${WPM_WCRT_ROOT}")
  set(WPM_WCRT_GENERATED_INCLUDE_DIR "${CMAKE_BINARY_DIR}/wcrt-include")
  file(MAKE_DIRECTORY "${WPM_WCRT_GENERATED_INCLUDE_DIR}")
  file(READ "${WPM_WCRT_ROOT}/include/stdio.h" WPM_WCRT_STDIO_HEADER)
  string(REPLACE "extern FILE __wcrt_" "__declspec(dllimport) extern FILE __wcrt_"
    WPM_WCRT_STDIO_HEADER "${WPM_WCRT_STDIO_HEADER}")
  file(WRITE "${WPM_WCRT_GENERATED_INCLUDE_DIR}/stdio.h" "${WPM_WCRT_STDIO_HEADER}")
  file(READ "${WPM_WCRT_ROOT}/include/errno.h" WPM_WCRT_ERRNO_HEADER)
  string(REPLACE "extern int wcrt_errno" "__declspec(dllimport) extern int wcrt_errno"
    WPM_WCRT_ERRNO_HEADER "${WPM_WCRT_ERRNO_HEADER}")
  file(WRITE "${WPM_WCRT_GENERATED_INCLUDE_DIR}/errno.h" "${WPM_WCRT_ERRNO_HEADER}")
  include_directories(BEFORE "${WPM_WCRT_ROOT}/include")
  include_directories(BEFORE "${WPM_WCRT_GENERATED_INCLUDE_DIR}")
  add_compile_options(-include "${CMAKE_SOURCE_DIR}/wpm/tcc_compat/wcrt_compat.h")
endif()

if(WPM_WINDOWS_XP_COMPAT)
  add_definitions(
    -DWINVER=0x0501
    -D_WIN32_WINNT=0x0501
    -DNTDDI_VERSION=0x05010000
  )

endif()

if(MSVC AND WPM_WINDOWS_XP_COMPAT)
  # Avoid requiring a separately installed VC/UCRT redistributable on XP.
  # This initializes MSVC_RUNTIME_LIBRARY on every subsequently created target,
  # including the bundled static libraries.
  if(POLICY CMP0091)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  endif()

  add_compile_options(/arch:SSE)
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
