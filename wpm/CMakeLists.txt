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
    message(FATAL_ERROR "PowerShell is required to run the WPM tests")
  endif()

  add_test(
    NAME TC-0001
    COMMAND ${WPM_POWERSHELL_EXECUTABLE}
      -NoProfile
      -ExecutionPolicy Bypass
      -File ${CMAKE_SOURCE_DIR}/tests/tc-0001-usage-version-check.ps1
      -WpmExe $<TARGET_FILE:wpm>
  )

  add_test(
    NAME wpm_smoke
    COMMAND ${WPM_POWERSHELL_EXECUTABLE}
      -NoProfile
      -ExecutionPolicy Bypass
      -File ${CMAKE_SOURCE_DIR}/tests/smoke.ps1
      -WpmExe $<TARGET_FILE:wpm>
  )

  add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --build-config $<CONFIG> --output-on-failure
    DEPENDS wpm
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running WPM tests"
  )

  option(WPM_BUILD_TEST_REPORTS "Generate local LaTeX test reports from automated test execution" OFF)
  if(WPM_BUILD_TEST_REPORTS)
    set(WPM_TEST_REPORT_DIR "${CMAKE_BINARY_DIR}/test-reports")
    set(WPM_TC0001_EVIDENCE_TEX "${WPM_TEST_REPORT_DIR}/tc-0001-execution-evidence.tex")
    set(WPM_TC0001_REPORT_TEX "${WPM_TEST_REPORT_DIR}/tc-0001-usage-version-check-report.tex")

    add_custom_command(
      OUTPUT "${WPM_TC0001_EVIDENCE_TEX}"
      COMMAND ${WPM_POWERSHELL_EXECUTABLE}
        -NoProfile
        -ExecutionPolicy Bypass
        -File ${CMAKE_SOURCE_DIR}/tests/tc-0001-usage-version-check.ps1
        -WpmExe $<TARGET_FILE:wpm>
        -EvidenceTex "${WPM_TC0001_EVIDENCE_TEX}"
      DEPENDS wpm ${CMAKE_SOURCE_DIR}/tests/tc-0001-usage-version-check.ps1
      COMMENT "Running TC-0001 and collecting LaTeX execution evidence"
      VERBATIM
    )

    add_custom_command(
      OUTPUT "${WPM_TC0001_REPORT_TEX}"
      COMMAND ${WPM_POWERSHELL_EXECUTABLE}
        -NoProfile
        -ExecutionPolicy Bypass
        -File ${CMAKE_SOURCE_DIR}/tests/generate-test-report.ps1
        -TestCaseTex ${CMAKE_SOURCE_DIR}/docs/tc-0001-usage-version-check.tex
        -EvidenceTex "${WPM_TC0001_EVIDENCE_TEX}"
        -OutputTex "${WPM_TC0001_REPORT_TEX}"
      DEPENDS
        "${WPM_TC0001_EVIDENCE_TEX}"
        ${CMAKE_SOURCE_DIR}/tests/generate-test-report.ps1
        ${CMAKE_SOURCE_DIR}/docs/tc-0001-usage-version-check.tex
        ${CMAKE_SOURCE_DIR}/docs/iso_iec_ieee29119-3_2021.tex
      COMMENT "Generating TC-0001 LaTeX test report"
      VERBATIM
    )

    add_custom_target(test-reports DEPENDS "${WPM_TC0001_REPORT_TEX}")

    find_program(WPM_PDFLATEX_EXECUTABLE
      NAMES pdflatex pdflatex.exe
      HINTS
        "$ENV{LOCALAPPDATA}/Programs/MiKTeX/miktex/bin/x64"
        "$ENV{LOCALAPPDATA}/Programs/MiKTeX/miktex/bin"
        "$ENV{APPDATA}/MiKTeX/miktex/bin/x64"
        "$ENV{APPDATA}/MiKTeX/miktex/bin"
    )
    if(WPM_PDFLATEX_EXECUTABLE)
      set(WPM_TC0001_REPORT_PDF "${WPM_TEST_REPORT_DIR}/tc-0001-usage-version-check-report.pdf")
      add_custom_command(
        OUTPUT "${WPM_TC0001_REPORT_PDF}"
        COMMAND ${WPM_POWERSHELL_EXECUTABLE}
          -NoProfile
          -ExecutionPolicy Bypass
          -File ${CMAKE_SOURCE_DIR}/tests/generate-test-report.ps1
          -TestCaseTex ${CMAKE_SOURCE_DIR}/docs/tc-0001-usage-version-check.tex
          -EvidenceTex "${WPM_TC0001_EVIDENCE_TEX}"
          -OutputTex "${WPM_TC0001_REPORT_TEX}"
          -PdfLatex "${WPM_PDFLATEX_EXECUTABLE}"
        DEPENDS
          "${WPM_TC0001_EVIDENCE_TEX}"
          ${CMAKE_SOURCE_DIR}/tests/generate-test-report.ps1
          ${CMAKE_SOURCE_DIR}/docs/tc-0001-usage-version-check.tex
          ${CMAKE_SOURCE_DIR}/docs/iso_iec_ieee29119-3_2021.tex
        COMMENT "Generating TC-0001 PDF test report"
        VERBATIM
      )
      add_custom_target(test-report-pdfs DEPENDS "${WPM_TC0001_REPORT_PDF}")
    else()
      message(STATUS "pdflatex was not found; WPM_BUILD_TEST_REPORTS will generate LaTeX .tex reports only.")
    endif()
  endif()
endif()
