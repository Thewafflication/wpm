# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project (wpm C)

# Miniz library
add_library(miniz
    third_party/miniz/miniz.c
)

target_include_directories(miniz PUBLIC
    third_party/miniz
)

target_compile_definitions(miniz PRIVATE
    MINIZ_EXPORT=
)

# Include sub-projects.
add_subdirectory ("wpm")