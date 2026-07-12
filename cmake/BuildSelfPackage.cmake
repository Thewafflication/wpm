cmake_minimum_required(VERSION 3.10)

foreach(required_variable
    WPM_EXECUTABLE
    WPM_PACKAGE_STAGING_DIR
    WPM_PACKAGE_OUTPUT_DIR
    WPM_PACKAGE_ARCH
    WPM_SETUP_SCRIPT
    WPM_REMOVE_SCRIPT
    WPM_REPORT_DIR
    WPM_DOCS_DIR
    WPM_README_FILE
    WPM_LICENSE_FILE
    WPM_THIRD_PARTY_NOTICES_FILE
)
  if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
    message(FATAL_ERROR "${required_variable} is required to build the WPM package.")
  endif()
endforeach()

execute_process(
  COMMAND "${WPM_EXECUTABLE}" --version
  RESULT_VARIABLE wpm_version_result
  OUTPUT_VARIABLE wpm_version_output
  ERROR_VARIABLE wpm_version_error
)
if(NOT wpm_version_result EQUAL 0)
  message(FATAL_ERROR "Could not determine the WPM package version: ${wpm_version_error}")
endif()

string(REGEX MATCH "Version ([A-Za-z0-9._-]+)" wpm_version_match "${wpm_version_output}")
if(NOT wpm_version_match)
  message(FATAL_ERROR "Could not parse a safe WPM package version from: ${wpm_version_output}")
endif()
set(wpm_version "${CMAKE_MATCH_1}")

file(GLOB wpm_report_files
  "${WPM_REPORT_DIR}/tc-*-report.tex"
  "${WPM_REPORT_DIR}/tc-*-execution-evidence.tex"
)
if(NOT wpm_report_files)
  message(FATAL_ERROR "No generated WPM test reports were found in: ${WPM_REPORT_DIR}")
endif()

file(REMOVE_RECURSE "${WPM_PACKAGE_STAGING_DIR}")
file(MAKE_DIRECTORY
  "${WPM_PACKAGE_STAGING_DIR}"
  "${WPM_PACKAGE_STAGING_DIR}/.wpm"
  "${WPM_PACKAGE_STAGING_DIR}/docs"
  "${WPM_PACKAGE_STAGING_DIR}/test-reports"
)

configure_file("${WPM_EXECUTABLE}" "${WPM_PACKAGE_STAGING_DIR}/wpm.exe" COPYONLY)
configure_file("${WPM_SETUP_SCRIPT}" "${WPM_PACKAGE_STAGING_DIR}/setup.cmd" COPYONLY)
configure_file("${WPM_REMOVE_SCRIPT}" "${WPM_PACKAGE_STAGING_DIR}/remove.cmd" COPYONLY)
configure_file("${WPM_README_FILE}" "${WPM_PACKAGE_STAGING_DIR}/README.md" COPYONLY)
configure_file("${WPM_LICENSE_FILE}" "${WPM_PACKAGE_STAGING_DIR}/LICENSE.txt" COPYONLY)
configure_file("${WPM_THIRD_PARTY_NOTICES_FILE}" "${WPM_PACKAGE_STAGING_DIR}/THIRD_PARTY_NOTICES.md" COPYONLY)
file(COPY "${WPM_DOCS_DIR}/" DESTINATION "${WPM_PACKAGE_STAGING_DIR}/docs" FILES_MATCHING PATTERN "*.md")
foreach(wpm_report_file IN LISTS wpm_report_files)
  get_filename_component(wpm_report_name "${wpm_report_file}" NAME)
  configure_file("${wpm_report_file}" "${WPM_PACKAGE_STAGING_DIR}/test-reports/${wpm_report_name}" COPYONLY)
endforeach()

file(WRITE "${WPM_PACKAGE_STAGING_DIR}/.wpm/package.txt"
  "name=wpm\n"
  "version=${wpm_version}\n"
  "arch=${WPM_PACKAGE_ARCH}\n"
  "debug=true\n"
  "description=Waughtal Package Manager\n"
  "license=GPL-3.0-or-later\n"
)
file(WRITE "${WPM_PACKAGE_STAGING_DIR}/.wpm/wpmignore.txt" ".wpm/\n")
file(WRITE "${WPM_PACKAGE_STAGING_DIR}/.wpm/install.cmd"
  "@echo off\n"
  "call \"%~dp0..\\setup.cmd\" \"%~dp0..\\wpm.exe\"\n"
  "exit /b %errorlevel%\n"
)
file(WRITE "${WPM_PACKAGE_STAGING_DIR}/.wpm/remove.cmd"
  "@echo off\n"
  "call \"%~dp0..\\remove.cmd\"\n"
  "exit /b %errorlevel%\n"
)

file(MAKE_DIRECTORY "${WPM_PACKAGE_OUTPUT_DIR}")
execute_process(
  COMMAND "${WPM_EXECUTABLE}" build "${WPM_PACKAGE_STAGING_DIR}" "${WPM_PACKAGE_OUTPUT_DIR}"
  RESULT_VARIABLE wpm_build_result
  OUTPUT_VARIABLE wpm_build_output
  ERROR_VARIABLE wpm_build_error
)
if(NOT wpm_build_result EQUAL 0)
  message(FATAL_ERROR "WPM self-package build failed:\n${wpm_build_output}${wpm_build_error}")
endif()

message(STATUS "Built WPM package: ${WPM_PACKAGE_OUTPUT_DIR}/wpm-${wpm_version}-${WPM_PACKAGE_ARCH}-debug.zip")
