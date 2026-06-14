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

target_include_directories(wpm PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET wpm PROPERTY C_STANDARD 11)
  set_property(TARGET wpm PROPERTY C_STANDARD_REQUIRED ON)
endif()

# TODO: Add tests and install targets if needed.
target_link_libraries(wpm PRIVATE miniz)
