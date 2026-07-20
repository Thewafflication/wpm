# Experimental 32-bit Windows XP toolchain using TinyCC.
#
# Set WPM_TCC_ROOT to use a TinyCC installation outside the repository.
# Otherwise the locally downloaded out/tools/tcc package is used.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86)

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

set(CMAKE_C_COMPILER "${WPM_TCC_ROOT}/tcc.exe" CACHE FILEPATH "TinyCC compiler")
get_filename_component(WPM_TCC_ARCHIVER
  "${CMAKE_CURRENT_LIST_DIR}/../tcc-ar.cmd"
  ABSOLUTE
)
set(CMAKE_AR "${WPM_TCC_ARCHIVER}" CACHE FILEPATH "TinyCC archiver wrapper")

# TinyCC exposes its archive writer through `tcc -ar`, rather than through a
# separate ar.exe program.
set(CMAKE_C_ARCHIVE_CREATE "<CMAKE_AR> \"<CMAKE_C_COMPILER>\" rc <TARGET> <OBJECTS>")
set(CMAKE_C_ARCHIVE_APPEND "<CMAKE_AR> \"<CMAKE_C_COMPILER>\" r <TARGET> <OBJECTS>")
set(CMAKE_C_ARCHIVE_FINISH "")

if(NOT EXISTS "${CMAKE_C_COMPILER}")
  message(FATAL_ERROR
    "TinyCC was not found at ${CMAKE_C_COMPILER}. "
    "Install the 32-bit Windows package there or set WPM_TCC_ROOT."
  )
endif()

# TinyCC's PE linker emits a console application by default. Keep the flag
# explicit so the intended executable type remains visible in verbose builds.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,-subsystem=console")
