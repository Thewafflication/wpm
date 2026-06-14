# CMakeList.txt : CMake project for wpm, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (wpm "wpm.c" "init.c" "init.h" "helpers.h" "helpers.c")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET wpm PROPERTY C_STANDARD 11)
  set_property(TARGET wpm PROPERTY C_STANDARD_REQUIRED ON)
endif()

# TODO: Add tests and install targets if needed.
target_link_libraries(wpm PRIVATE miniz)