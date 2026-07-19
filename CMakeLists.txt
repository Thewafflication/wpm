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

# Keep the 32-bit build usable on the oldest platform and processor it targets.
# Pentium III provides SSE, but not SSE2.  Current MSVC x86 builds otherwise
# default to a newer instruction-set baseline, and current linkers otherwise
# stamp executables with a post-XP subsystem version.
if(WIN32 AND wpm_arch STREQUAL "x86")
  add_definitions(
    -DWINVER=0x0501
    -D_WIN32_WINNT=0x0501
    -DNTDDI_VERSION=0x05010000
  )

endif()

if(MSVC AND wpm_arch STREQUAL "x86")
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
