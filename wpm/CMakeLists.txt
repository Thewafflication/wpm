# CMakeList.txt : CMake project for wpm, include source and define
# project specific logic here.
#

# Add source to this project's executable.
set(WPM_VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/version.h")

add_custom_target(wpm_version_header ALL
  COMMAND ${CMAKE_COMMAND}
    -DWPM_SOURCE_DIR=${CMAKE_SOURCE_DIR}
    -DWPM_VERSION_HEADER=${WPM_VERSION_HEADER}
    -P ${CMAKE_SOURCE_DIR}/cmake/GenerateVersion.cmake
  BYPRODUCTS ${WPM_VERSION_HEADER}
  COMMENT "Generating WPM version from Git"
  VERBATIM
)

add_executable (wpm
  "wpm.c"
  "archive.c"
  "archive.h"
  "init.c"
  "init.h"
  "helpers.c"
  "helpers.h"
)

add_dependencies(wpm wpm_version_header)

if(CMAKE_CONFIGURATION_TYPES)
  set(wpm_output_dir "${WPM_BIN_DIR}")
elseif(CMAKE_BUILD_TYPE)
  set(wpm_output_dir "${WPM_BIN_DIR}/${CMAKE_BUILD_TYPE}")
else()
  set(wpm_output_dir "${WPM_BIN_DIR}")
endif()

set_target_properties(wpm PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${wpm_output_dir}"
  PDB_OUTPUT_DIRECTORY "${wpm_output_dir}"
)

foreach(config Debug Release RelWithDebInfo MinSizeRel)
  string(TOUPPER "${config}" config_upper)
  set_target_properties(wpm PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${WPM_BIN_DIR}/${config}"
    PDB_OUTPUT_DIRECTORY_${config_upper} "${WPM_BIN_DIR}/${config}"
  )
endforeach()

target_include_directories(wpm PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET wpm PROPERTY C_STANDARD 11)
  set_property(TARGET wpm PROPERTY C_STANDARD_REQUIRED ON)
endif()

target_link_libraries(wpm PRIVATE miniz)

if(BUILD_TESTING)
  find_program(WPM_POWERSHELL_EXECUTABLE NAMES pwsh powershell)
  if(NOT WPM_POWERSHELL_EXECUTABLE)
    message(FATAL_ERROR "PowerShell is required to run the WPM smoke tests")
  endif()

  add_test(
    NAME wpm_smoke
    COMMAND ${WPM_POWERSHELL_EXECUTABLE}
      -NoProfile
      -ExecutionPolicy Bypass
      -File ${CMAKE_SOURCE_DIR}/tests/smoke.ps1
      -WpmExe $<TARGET_FILE:wpm>
  )

  add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS wpm
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running WPM tests"
  )
endif()
