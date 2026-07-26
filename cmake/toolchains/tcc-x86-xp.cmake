# Windows toolchain using the architecture-specific compiler in TinyCC's
# multi-architecture Windows package. The x86 preset additionally enables the
# project's Windows XP compatibility layer.
#
# Set WPM_TCC_ROOT to use a TinyCC installation outside the repository.
# Otherwise the locally downloaded out/tools/tcc package is used.
set(CMAKE_SYSTEM_NAME Windows)

if(NOT DEFINED WPM_ARCH)
  set(WPM_ARCH x86)
endif()

if(WPM_ARCH STREQUAL "x86")
  set(WPM_TCC_DRIVER i386-win32-tcc.exe)
  set(CMAKE_SYSTEM_PROCESSOR x86)
elseif(WPM_ARCH STREQUAL "x64")
  set(WPM_TCC_DRIVER x86_64-win32-tcc.exe)
  set(CMAKE_SYSTEM_PROCESSOR AMD64)
elseif(WPM_ARCH STREQUAL "arm64")
  set(WPM_TCC_DRIVER arm64-win32-tcc.exe)
  set(CMAKE_SYSTEM_PROCESSOR ARM64)
else()
  message(FATAL_ERROR "Unsupported TinyCC Windows architecture: ${WPM_ARCH}")
endif()

if(DEFINED ENV{WPM_TCC_ROOT} AND NOT "$ENV{WPM_TCC_ROOT}" STREQUAL "")
  file(TO_CMAKE_PATH "$ENV{WPM_TCC_ROOT}" WPM_TCC_ROOT)
else()
  file(TO_CMAKE_PATH "$ENV{ProgramFiles}/TinyCC" WPM_TCC_INSTALL_ROOT)
  file(GLOB WPM_TCC_INSTALLATIONS LIST_DIRECTORIES TRUE
    "${WPM_TCC_INSTALL_ROOT}/*"
  )
  list(SORT WPM_TCC_INSTALLATIONS COMPARE NATURAL ORDER DESCENDING)
  foreach(WPM_TCC_INSTALLATION IN LISTS WPM_TCC_INSTALLATIONS)
    if(EXISTS "${WPM_TCC_INSTALLATION}/tcc.exe")
      set(WPM_TCC_ROOT "${WPM_TCC_INSTALLATION}")
      break()
    endif()
  endforeach()

  if(NOT WPM_TCC_ROOT)
    get_filename_component(WPM_TCC_ROOT
      "${CMAKE_CURRENT_LIST_DIR}/../../out/tools/tcc"
      ABSOLUTE
    )
  endif()
endif()

set(CMAKE_C_COMPILER "${WPM_TCC_ROOT}/${WPM_TCC_DRIVER}" CACHE FILEPATH "TinyCC compiler")
get_filename_component(WPM_TCC_ARCHIVER_SCRIPT
  "${CMAKE_CURRENT_LIST_DIR}/../TinyCCArchiver.ps1"
  ABSOLUTE
)
find_program(WPM_TCC_ARCHIVER_POWERSHELL_EXECUTABLE powershell.exe REQUIRED)
set(CMAKE_AR "${WPM_TCC_ARCHIVER_POWERSHELL_EXECUTABLE}" CACHE FILEPATH "PowerShell used by the TinyCC archiver")
string(REPLACE "'" "''" WPM_TCC_COMPILER_POWERSHELL_LITERAL "${CMAKE_C_COMPILER}")
execute_process(
  COMMAND "${WPM_TCC_ARCHIVER_POWERSHELL_EXECUTABLE}" -NoProfile -Command
    "[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes('${WPM_TCC_COMPILER_POWERSHELL_LITERAL}'))"
  OUTPUT_VARIABLE WPM_TCC_COMPILER_BASE64
  OUTPUT_STRIP_TRAILING_WHITESPACE
  COMMAND_ERROR_IS_FATAL ANY
)

# TinyCC exposes its archive writer through `tcc -ar`, rather than through a
# separate ar.exe program. Encode the compiler path so nested cmd.exe parsing
# cannot split a TinyCC installation below Program Files.
set(CMAKE_C_ARCHIVE_CREATE "<CMAKE_AR> -NoProfile -ExecutionPolicy Bypass -File \"${WPM_TCC_ARCHIVER_SCRIPT}\" ${WPM_TCC_COMPILER_BASE64} rc <TARGET> <OBJECTS>")
set(CMAKE_C_ARCHIVE_APPEND "<CMAKE_AR> -NoProfile -ExecutionPolicy Bypass -File \"${WPM_TCC_ARCHIVER_SCRIPT}\" ${WPM_TCC_COMPILER_BASE64} r <TARGET> <OBJECTS>")
set(CMAKE_C_ARCHIVE_FINISH "")

if(NOT EXISTS "${CMAKE_C_COMPILER}")
  message(FATAL_ERROR
    "TinyCC was not found at ${CMAKE_C_COMPILER}. "
    "Install the multi-architecture Windows package there or set WPM_TCC_ROOT."
  )
endif()

# TinyCC's PE linker emits a console application by default. Keep the flag
# explicit so the intended executable type remains visible in verbose builds.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,-subsystem=console")
